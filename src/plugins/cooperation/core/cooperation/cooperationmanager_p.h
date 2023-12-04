﻿// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COOPERATIONMANAGER_P_H
#define COOPERATIONMANAGER_P_H

#include "info/deviceinfo.h"
#include "cooperationtaskdialog.h"

#include "ipc/bridge.h"

#ifdef linux
#include <QDBusInterface>
#endif

namespace cooperation_core {

class CooperationManager;
class CooperationManagerPrivate : public QObject
{
    Q_OBJECT
public:
    explicit CooperationManagerPrivate(CooperationManager *qq);

    void backendShareEvent(req_type_t type, const DeviceInfoPointer devInfo = nullptr, bool accepted = false);
    CooperationTaskDialog *taskDialog();
    uint notifyMessage(uint replacesId, const QString &body, const QStringList &actions, int expireTimeout);

public Q_SLOTS:
    void onActionTriggered(uint replacesId, const QString &action);

public:
    CooperationManager *q;
#ifdef linux
    QDBusInterface *notifyIfc { nullptr };
#endif
    CooperationTaskDialog *ctDialog { nullptr };
    uint recvReplacesId { 0 };
    bool isRecvMode { true };
    bool isReplied { false };

    // 作为接收方时，发送方的ip
    QString senderDeviceIp;
    // 作为发送方时，为接收方设备信息；作为接收方时，为发送方设备信息
    DeviceInfoPointer targetDeviceInfo { nullptr };
};

}   // namespace cooperation_core

#endif   // COOPERATIONMANAGER_P_H
