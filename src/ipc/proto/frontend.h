// Autogenerated.
// DO NOT EDIT. All changes will be undone.
#pragma once

#include "co/rpc.h"

namespace ipc {

class Frontend : public rpc::Service {
  public:
    typedef std::function<void(co::Json&, co::Json&)> Fun;

    Frontend() {
        using std::placeholders::_1;
        using std::placeholders::_2;
        _methods["Frontend.ping"] = std::bind(&Frontend::ping, this, _1, _2);
        _methods["Frontend.cbPeerInfo"] = std::bind(&Frontend::cbPeerInfo, this, _1, _2);
        _methods["Frontend.cbConnect"] = std::bind(&Frontend::cbConnect, this, _1, _2);
        _methods["Frontend.cbMiscMessage"] = std::bind(&Frontend::cbMiscMessage, this, _1, _2);
        _methods["Frontend.cbTransStatus"] = std::bind(&Frontend::cbTransStatus, this, _1, _2);
        _methods["Frontend.cbFsPull"] = std::bind(&Frontend::cbFsPull, this, _1, _2);
        _methods["Frontend.cbFsAction"] = std::bind(&Frontend::cbFsAction, this, _1, _2);
        _methods["Frontend.notifyFileStatus"] = std::bind(&Frontend::notifyFileStatus, this, _1, _2);
    }

    virtual ~Frontend() {}

    virtual const char* name() const {
        return "Frontend";
    }

    virtual const co::map<const char*, Fun>& methods() const {
        return _methods;
    }

    virtual void ping(co::Json& req, co::Json& res) = 0;

    virtual void cbPeerInfo(co::Json& req, co::Json& res) = 0;

    virtual void cbConnect(co::Json& req, co::Json& res) = 0;

    virtual void cbMiscMessage(co::Json& req, co::Json& res) = 0;

    virtual void cbTransStatus(co::Json& req, co::Json& res) = 0;

    virtual void cbFsPull(co::Json& req, co::Json& res) = 0;

    virtual void cbFsAction(co::Json& req, co::Json& res) = 0;

    virtual void notifyFileStatus(co::Json& req, co::Json& res) = 0;

  private:
    co::map<const char*, Fun> _methods;
};

struct PingFrontParam {
    fastring session;
    fastring version;

    void from_json(const co::Json& _x_) {
        session = _x_.get("session").as_c_str();
        version = _x_.get("version").as_c_str();
    }

    co::Json as_json() const {
        co::Json _x_;
        _x_.add_member("session", session);
        _x_.add_member("version", version);
        return _x_;
    }
};

struct FileEntry {
    uint32 type;
    fastring name;
    bool hidden;
    uint64 size;
    uint64 modified_time;

    void from_json(const co::Json& _x_) {
        type = (uint32)_x_.get("type").as_int64();
        name = _x_.get("name").as_c_str();
        hidden = _x_.get("hidden").as_bool();
        size = (uint64)_x_.get("size").as_int64();
        modified_time = (uint64)_x_.get("modified_time").as_int64();
    }

    co::Json as_json() const {
        co::Json _x_;
        _x_.add_member("type", type);
        _x_.add_member("name", name);
        _x_.add_member("hidden", hidden);
        _x_.add_member("size", size);
        _x_.add_member("modified_time", modified_time);
        return _x_;
    }
};

struct FileDirectory {
    int32 id;
    fastring path;
    co::vector<FileEntry> entries;

    void from_json(const co::Json& _x_) {
        id = (int32)_x_.get("id").as_int64();
        path = _x_.get("path").as_c_str();
        do {
            auto& _unamed_v1 = _x_.get("entries");
            for (uint32 i = 0; i < _unamed_v1.array_size(); ++i) {
                FileEntry _unamed_v2;
                _unamed_v2.from_json(_unamed_v1[i]);
                entries.emplace_back(std::move(_unamed_v2));
            }
        } while (0);
    }

    co::Json as_json() const {
        co::Json _x_;
        _x_.add_member("id", id);
        _x_.add_member("path", path);
        do {
            co::Json _unamed_v1;
            for (size_t i = 0; i < entries.size(); ++i) {
                _unamed_v1.push_back(entries[i].as_json());
            }
            _x_.add_member("entries", _unamed_v1);
        } while (0);
        return _x_;
    }
};

struct GenericResult {
    int32 id;
    int32 result;
    fastring msg;

    void from_json(const co::Json& _x_) {
        id = (int32)_x_.get("id").as_int64();
        result = (int32)_x_.get("result").as_int64();
        msg = _x_.get("msg").as_c_str();
    }

    co::Json as_json() const {
        co::Json _x_;
        _x_.add_member("id", id);
        _x_.add_member("result", result);
        _x_.add_member("msg", msg);
        return _x_;
    }
};

struct FileStatus {
    int32 job_id;
    int32 file_id;
    fastring name;
    int32 status;
    int64 total;
    int64 current;
    int32 second;

    void from_json(const co::Json& _x_) {
        job_id = (int32)_x_.get("job_id").as_int64();
        file_id = (int32)_x_.get("file_id").as_int64();
        name = _x_.get("name").as_c_str();
        status = (int32)_x_.get("status").as_int64();
        total = (int64)_x_.get("total").as_int64();
        current = (int64)_x_.get("current").as_int64();
        second = (int32)_x_.get("second").as_int64();
    }

    co::Json as_json() const {
        co::Json _x_;
        _x_.add_member("job_id", job_id);
        _x_.add_member("file_id", file_id);
        _x_.add_member("name", name);
        _x_.add_member("status", status);
        _x_.add_member("total", total);
        _x_.add_member("current", current);
        _x_.add_member("second", second);
        return _x_;
    }
};

} // ipc
