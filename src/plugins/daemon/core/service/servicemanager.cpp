﻿// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "servicemanager.h"
#include "rpc/remoteservice.h"
#include "ipc/proto/chan.h"
#include "ipc/proto/comstruct.h"
#include "ipc/backendservice.h"
#include "common/constant.h"
#include "comshare.h"
#include "co/cout.h"
#include "session.h"
#include "discoveryjob.h"

#include "utils/config.h"
#include "utils/cert.h"

#include <functional>
#include <QSettings>

co::chan<IncomeData> _income_chan(10, 300);
co::chan<OutData> _outgo_chan(10, 10);

ServiceManager::ServiceManager(QObject *parent) : QObject(parent)
{
    flag::set_value("rpc_log", "false");
    _rpcServiceBinder = new RemoteServiceBinder(this);

    _transjob_sends.clear();
    _transjob_recvs.clear();
    _transjob_break.clear();

    _sessions.clear();
    _this_destruct = false;

    // init and start backend IPC
    localIPCStart();

    // init the pin code: no setting then refresh as random
    DaemonConfig::instance()->initPin();

    // init the host uuid. no setting then gen a random
    fastring hostid = DaemonConfig::instance()->getUUID();
    if (hostid.empty()) {
        hostid = Util::genUUID();
        DaemonConfig::instance()->setUUID(hostid.c_str());
    }

    // temp disable discovery service.
    asyncDiscovery();
}

ServiceManager::~ServiceManager()
{
    _this_destruct = true;
    if (_rpcServiceBinder) {
        _rpcServiceBinder->deleteLater();
    }
    if (_backendIpcService) {
        _backendIpcService->deleteLater();
    }

    _sessions.reset();
}

void ServiceManager::startRemoteServer()
{
    if (_rpcServiceBinder) {
        fastring key = Cert::instance()->writeKey();
        fastring crt = Cert::instance()->writeCrt();
        _rpcServiceBinder->startRpcListen(key.c_str(), crt.c_str());
        Cert::instance()->removeFile(key);
        Cert::instance()->removeFile(crt);
    }
    go([this]() {
        while(!_this_destruct) {
            IncomeData indata;
            _income_chan >> indata;
            if (!_income_chan.done()) {
                // timeout, next read
                continue;
            }
//            LOG << "ServiceManager get chan value: " << indata.type << " json:" << indata.json;
            co::Json json_obj = json::parse(indata.json);
            if (json_obj.is_null()) {
                ELOG << "parse error from: " << indata.json;
                continue;
            }
            switch (indata.type) {
            case IN_LOGIN_CONFIRM:
            {
                //TODO: notify user confirm login
                break;
            }
            case IN_LOGIN_RESULT:
            {
                UserLoginResult result;
                result.from_json(json_obj);
                QString appname(result.appname.c_str());
                handleLoginResult(result.result, appname);
                break;
            }
            case IN_TRANSJOB:
            {
                go(std::bind(&ServiceManager::handleRemoteRequestJob, this, json_obj));
                break;
            }
            case FS_DATA:
            {
                // must update the binrary data into struct object.
                handleFSData(json_obj, indata.buf);
                break;
            }
            case FS_INFO:
            {
                handleFSInfo(json_obj);
                break;
            }
            case TRANS_CANCEL:
            {
                handleCancelJob(json_obj);
                break;
            }
            case FS_REPORT:
            {
                handleTransReport(json_obj);
                break;
            }
            default:
                break;
            }
        }
    });
}

void ServiceManager::localIPCStart()
{
    if (_backendIpcService) return;

    _backendIpcService = new BackendService(this);

    connect(_rpcServiceBinder, &RemoteServiceBinder::loginResult, this, &ServiceManager::handleLoginResult, Qt::QueuedConnection);

    go([this]() {
        while(!_this_destruct) {
            BridgeJsonData bridge;
            _backendIpcService->bridgeChan()->operator>>(bridge); //300ms超时
            if (!_backendIpcService->bridgeChan()->done()) {
                // timeout, next read
                continue;
            }

//            LOG << "ServiceManager get bridge json: " << bridge.type << " json:" << bridge.json;
            co::Json json_obj = json::parse(bridge.json);
            if (json_obj.is_null()) {
                ELOG << "parse error from: " << bridge.json;
                continue;
            }
            switch (bridge.type) {
            case PING:
            {
                //check session or gen new one
                ipc::PingBackParam param;
                param.from_json(json_obj);

                fastring session = "";
                const char *appname = param.who.c_str();
                fastring my_ver(BACKEND_PROTO_VERSION);
                if (my_ver.compare(param.version) != 0) {
                    DLOG << param.version << " =version not match= " << my_ver;
                } else {
                    QString name = QString(appname);
                    Session *s = sessionByName(name);
                    if (s) {
                        session = s->getSession().toStdString();
                    } else {
                        // gen new one
                        session = co::randstr(appname, 8); // 长度为8的16进制字符串
                        QString sesid(session.c_str());
                        saveSession(name, sesid, param.cb_port);
                    }
                }

                BridgeJsonData res;
                res.type = PING;
                res.json = session;

                _backendIpcService->bridgeResult()->operator<<(res);
                break;
            }
            case MISC_MSG:
            {
                MiscJsonCall call;
                call.from_json(json_obj);
                sendMiscMessage(call.app, call.json);
                break;
            }
            case BACK_TRY_CONNECT:
            {
                ipc::ConnectParam param;
                param.from_json(json_obj);

                QString session(param.session.c_str());
                QString ip(param.host.c_str());
                QString pass(param.password.c_str());
                notifyConnect(session, ip, pass);
                break;
            }
            case BACK_TRY_TRANS_FILES:
            {
                ipc::TransFilesParam param;
                param.from_json(json_obj);

                QString session = QString(param.session.c_str());
                QString savedir = QString(param.savedir.c_str());
                QStringList paths;
                for (uint32 i = 0; i < param.paths.size(); i++) {
                    paths << param.paths[i].c_str();
                }

                qInfo() << "paths: " << paths;
                newTransSendJob(session, param.id, paths, param.sub, savedir);
                break;
            }
            case BACK_RESUME_JOB:
            case BACK_CANCEL_JOB:
            {
                bool ok = doJobAction(bridge.type, json_obj);
                co::Json resjson = {
                    { "result", ok },
                    { "msg", json_obj.str() }
                };

                BridgeJsonData res;
                res.type = bridge.type;
                res.json = resjson.str();

                _backendIpcService->bridgeResult()->operator<<(res);
                break;
            }
            case BACK_GET_DISCOVERY:
            {
                break;
            }
            case BACK_GET_PEER:
            {
                break;
            }
            case BACK_FS_CREATE:
            {
                break;
            }
            case BACK_FS_DELETE:
            {
                break;
            }
            case BACK_FS_RENAME:
            {
                break;
            }
            case BACK_FS_PULL:
            {
                break;
            }
            default:
                break;
            }
        }
    });


    // start ipc services
    ipc::BackendImpl *backendimp = new ipc::BackendImpl();
    backendimp->setInterface(_backendIpcService);
    rpc::Server().add_service(backendimp)
                 .start("0.0.0.0", UNI_IPC_BACKEND_PORT, "/backend", "", "");
}

bool ServiceManager::handleRemoteRequestJob(co::Json &info)
{
    FSJob fsjob;
    fsjob.from_json(info);
    int32 jobId = fsjob.job_id;
    fastring savedir = fsjob.save;
    if (savedir.empty() && fsjob.write) {
        // 如果未指定保存相对路径，则默认保存到$home/hostname
        savedir = DaemonConfig::instance()->getStorageDir();
    }

    TransferJob *job = new TransferJob();
    job->initRpc(_connected_target, UNI_RPC_PORT_BASE);
    job->initJob(fsjob.who, jobId, fsjob.path, fsjob.sub, savedir, fsjob.write);
    connect(job, &TransferJob::notifyFileTransStatus, this, &ServiceManager::handleFileTransStatus, Qt::QueuedConnection);
    connect(job, &TransferJob::notifyJobResult, this, &ServiceManager::handleJobTransStatus, Qt::QueuedConnection);

    g_m.lock();
    if (fsjob.write) {
        DLOG << "(" << jobId <<")write job save to: " << savedir;
        _transjob_recvs.insert(jobId, job);
    } else {
        DLOG << "(" << jobId <<")read job save to: " << savedir;
        _transjob_sends.insert(jobId, job);
    }
    g_m.unlock();
    go([this, job]() {
        // start job one by one
        co::mutex_guard g(g_m);
        // DLOG << ".........start job: sched: " << co::sched_id() << " co: " << co::coroutine_id();
        job->start();
    });

    return true;
}

bool ServiceManager::handleFSData(co::Json &info, fastring buf)
{
    FSDataBlock datablock;
    datablock.from_json(info);
    datablock.data = buf;
    int32 jobId = datablock.job_id;

    TransferJob *job = _transjob_recvs.value(jobId);
    if (nullptr != job) {
        job->pushQueque(datablock);
    } else {
        return false;
    }

    return true;
}

bool ServiceManager::handleFSInfo(co::Json &info)
{
    FileInfo finfo;
    finfo.from_json(info);
    int32 jobId = finfo.job_id;

    TransferJob *job = _transjob_recvs.value(jobId);
    if (nullptr != job) {
        job->insertFileInfo(finfo);
    } else {
        return false;
    }

    return true;
}

// cancel the receive job from remote
bool ServiceManager::handleCancelJob(co::Json &info)
{
    FSJobCancel obj;
    obj.from_json(info);
    int32 jobId = obj.job_id;

    TransferJob *job = _transjob_recvs.value(jobId);
    if (nullptr != job) {
        //disconnect(job, &TransferJob::notifyFileTransStatus, this, &ServiceManager::handleFileTransStatus);
        job->stop();
        _transjob_recvs.remove(jobId);
        QString name(job->getAppName().c_str());
        Session *s = sessionByName(name);
        if (s && s->valid()) {
            DLOG << "notify cancel success:" << s->getName().toStdString();
            return true;
        }
    } else {
        return false;
    }

    return true;
}

bool ServiceManager::handleTransReport(co::Json &info)
{
    FSReport obj;
    obj.from_json(info);
    int32 jobId = obj.job_id;
    fastring path = obj.path;

    switch (obj.result) {
    case IO_ERROR:
    {
        TransferJob *job = _transjob_sends.value(jobId);
        if (nullptr != job) {
            // move the job into breaks record map.
            _transjob_break.insert(jobId, _transjob_sends.take(jobId));
        }
        break;
    }
    case OK:
        break;
    case FINIASH:
    {
        TransferJob *job = _transjob_recvs.value(jobId);
        if (nullptr != job) {
            //disconnect(job, &TransferJob::notifyFileTransStatus, this, &ServiceManager::handleFileTransStatus);
            job->waitFinish();
            _transjob_recvs.remove(jobId);
            QString name(job->getAppName().c_str());
            Session *s = sessionByName(name);
            if (s && s->valid()) {
                DLOG << "notify job finish success:" << s->getName().toStdString();
                return true;
            }
        } else {
            return false;
        }
        break;
    }
    default:
        DLOG << "unkown report: " << obj.result;
        break;
    }

    return true;
}

Session* ServiceManager::sessionById(QString &id)
{
    // find the session by id
    for (size_t i = 0; i < _sessions.size(); ++i) {
        Session *s = _sessions[i];
        if (s->getSession().compare(id) == 0) {
            return s;
        }
    }
    return nullptr;
}

Session* ServiceManager::sessionByName(QString &name)
{
    // find the session by name
    for (size_t i = 0; i < _sessions.size(); ++i) {
        Session *s = _sessions[i];
        if (s->getName().compare(name) == 0) {
            return s;
        }
    }
    return nullptr;
}

fastring ServiceManager::genPeerInfo()
{
    fastring nick = DaemonConfig::instance()->getNickName();
    int mode = DaemonConfig::instance()->getMode();
    co::Json info = {
        { "proto_version", UNI_RPC_PROTO },
        { "uuid", DaemonConfig::instance()->getUUID() },
        { "nickname", nick },
        { "username", Util::getUsername() },
        { "hostname", Util::getHostname() },
        { "ipv4", Util::getFirstIp() },
        { "port", UNI_RPC_PORT_BASE },
        { "os_type", Util::getOSType() },
        { "mode_type", mode },
    };

    return info.str();
}

void ServiceManager::asyncDiscovery()
{
    connect(DiscoveryJob::instance(), &DiscoveryJob::sigNodeChanged, this, &ServiceManager::handleNodeChanged, Qt::QueuedConnection);
    go ([]() {
        DiscoveryJob::instance()->discovererRun();
    });
    go ([this]() {
        fastring peerinfo = genPeerInfo();
        DiscoveryJob::instance()->announcerRun(peerinfo);
    });
}

void ServiceManager::saveSession(QString who, QString session, int cbport)
{
    Session *s = new Session(who, session, cbport);
    _sessions.push_back(s);
}

void ServiceManager::newTransSendJob(QString session, int32 jobId, QStringList paths, bool sub, QString savedir)
{
    Session *s = sessionById(session);
    if (!s || !s->valid()) {
        DLOG << "this session is invalid." << session.toStdString();
        return;
    }

    int32 id = jobId;
    fastring who = s->getName().toStdString();
    fastring savepath = savedir.toStdString();

    co::Json pathjson;
    for (QString path : paths) {
        fastring filepath = path.toStdString();
        pathjson.push_back(filepath);
    }
    //FSJob
    co::Json jobjson = {
        { "who", who },
        { "job_id", id },
        { "path", pathjson.str() },
        { "save", savepath },
        { "hidden", false },
        { "sub", sub },
        { "write", false }
    };

    go(std::bind(&ServiceManager::handleRemoteRequestJob, this, jobjson));
    s->addJob(id); // record this job into session
}

void ServiceManager::notifyConnect(QString session, QString ip, QString password)
{
    Session *s = sessionByName(session);
    if (!s || !s->valid()) {
        DLOG << "this session is invalid." << session.toStdString();
        return;
    }
    //TODO: check ipv4 format
    if (ip.isEmpty()) {
        return;
    }
    fastring name = Util::getHostname();

    if (_rpcServiceBinder) {
        fastring target_ip = ip.toStdString();
        fastring user = session.toStdString();
        fastring pincode = password.toStdString();
        _connected_target = target_ip;

        go([this, target_ip, user, pincode]() {
            _rpcServiceBinder->createExecutor(target_ip.c_str(), UNI_RPC_PORT_BASE);
            _rpcServiceBinder->doLogin(user.c_str(), pincode.c_str());
        });
    }
}

bool ServiceManager::doJobAction(uint32_t action, co::Json &jsonobj)
{
    ipc::TransJobParam param;
    param.from_json(jsonobj);

    QString session(param.session.c_str());
    int jobid = param.job_id;
    bool remote = param.is_remote;
    Session *s = sessionById(session);
    if (!s || s->hasJob(jobid) < 0) {
        DLOG << "not find session by id:" << session.toStdString();
        return false;
    }

    if (BACK_CANCEL_JOB == action) {
        if (remote) {
            //receive job cancel and notify remote send cancel
            TransferJob *job = _transjob_recvs.value(jobid);
            if (nullptr != job) {
                job->cancel();
                _transjob_recvs.remove(jobid);
                return true;
            } else {
                return false;
            }
        } else {
            //send job cancel.
            TransferJob *job = _transjob_sends.value(jobid);
            if (nullptr != job) {
                job->cancel();
                _transjob_sends.remove(jobid);
                return true;
            } else {
                return false;
            }
        }
    } else if (BACK_RESUME_JOB == action) {

    }
    return true;
}

void ServiceManager::sendMiscMessage(fastring &appname, fastring &message)
{
    go([this, appname, message]() {
        QString res = _rpcServiceBinder->doMisc(appname.c_str(), message.c_str());
        JsonMessage rpc_res;
        if (!res.isEmpty() && rpc_res.ParseFromString(res.toStdString())) {
            // handle result.
            fastring app = rpc_res.app();
            fastring json = rpc_res.json();
            forwardJsonMisc(app, json);
        } else {
            DLOG << "misc message return by async.";
        }
    });
}

void ServiceManager::forwardJsonMisc(fastring &appname, fastring &message)
{
    QString app(appname.c_str());
    Session *s = sessionByName(app);
    if (s && s->valid()) {
        go ([s, message]() {
            co::pool_guard<rpc::Client> c(s->clientPool());
            co::Json req, res;
            //cbMiscMessage {any json format}
            if (req.parse_from(message)) {
                req.add_member("api", "Frontend.cbMiscMessage");
                c->call(req, res);
            } else {
                ELOG << "forwardJsonMisc NOT correct json:"<< message;
            }
        });
    }
}

void ServiceManager::handleLoginResult(bool result, QString session)
{
    qInfo() << session << " LoginResult: " << result;
    fastring session_name(session.toStdString());

    // find the session by session id
    Session *s = sessionByName(session);
    if (s && s->valid()) {
        go ([s, result, session_name]() {
            co::pool_guard<rpc::Client> c(s->clientPool());
            co::Json req, res;
            //cbConnect {GenericResult}
            req = {
                { "id", 0 },
                { "result", result ? 1 : 0 },
                { "msg", session_name },
            };
            req.add_member("api", "Frontend.cbConnect");
            c->call(req, res);
        });
    } else {
        DLOG << "Donot find login seesion: " << session_name;
    }
}

void ServiceManager::handleFileTransStatus(QString appname, int status, QString fileinfo)
{
    Session *s = sessionByName(appname);
    if (s && s->valid()) {
        //DLOG << "notify file trans status to:" << s->getName().toStdString();
        go ([s, status, fileinfo]() {
            co::Json infojson;
            infojson.parse_from(fileinfo.toStdString());
            FileInfo filejob;
            filejob.from_json(infojson);

            co::pool_guard<rpc::Client> c(s->clientPool());
            co::Json req, res;
            //notifyFileStatus {FileStatus}
            req = {
                { "job_id", filejob.job_id },
                { "file_id", filejob.file_id },
                { "name", filejob.name },
                { "status", status },
                { "total", filejob.total_size },
                { "current", filejob.current_size },
                { "second", filejob.time_spended },
            };

            req.add_member("api", "Frontend.notifyFileStatus");
            c->call(req, res);
        });
    }
}

void ServiceManager::handleJobTransStatus(QString appname, int jobid, int status, QString savedir)
{
    Session *s = sessionByName(appname);
    if (s && s->valid()) {
        //DLOG << "notify file trans status to:" << s->getName().toStdString();
        go ([s, jobid, status, savedir]() {
            co::pool_guard<rpc::Client> c(s->clientPool());
            co::Json req, res;
            //cbTransStatus {GenericResult}
            req = {
                { "id", jobid },
                { "result", status },
                { "msg", savedir.toStdString() },
            };

            req.add_member("api", "Frontend.cbTransStatus");
            c->call(req, res);
        });
    }
}

void ServiceManager::handleNodeChanged(bool found, QString info)
{
//    qInfo() << info << " node found: " << found << " session.size=" << _sessions.size();

    // notify to all frontend sessions
    go ([this, found, info]() {
        for (size_t i = 0; i < _sessions.size(); ++i) {
            Session *s = _sessions[i];
            if (s->alive()) {
                // fastring session_id(s->getSession().toStdString());
                fastring nodeinfo(info.toStdString());
                co::pool_guard<rpc::Client> c(s->clientPool());
                co::Json req, res;
                //cbPeerInfo {GenericResult}
                req = {
                    { "id", 0 },
                    { "result", found ? 1 : 0 },
                    { "msg", nodeinfo },
                };

                req.add_member("api", "Frontend.cbPeerInfo");
                c->call(req, res);
            } else {
                // the frontend is offline
                _sessions.remove(i);
            }
        }
    });
}
