// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sortfilterworker.h"
#include "utils/historymanager.h"

using TransHistoryInfo = QMap<QString, QString>;
Q_GLOBAL_STATIC(TransHistoryInfo, transHistory)

using namespace cooperation_core;

SortFilterWorker::SortFilterWorker(QObject *parent)
    : QObject(parent)
{
    onTransHistoryUpdated();
    connect(HistoryManager::instance(), &HistoryManager::transHistoryUpdated, this, &SortFilterWorker::onTransHistoryUpdated, Qt::QueuedConnection);
}

void SortFilterWorker::stop()
{
    isStoped = true;
}

void SortFilterWorker::onTransHistoryUpdated()
{
    *transHistory = HistoryManager::instance()->getTransHistory();
}

void SortFilterWorker::addDevice(const QList<DeviceInfoPointer> &infoList)
{
    if (isStoped)
        return;

    for (auto info : infoList) {
        if (contains(allDeviceList, info)) {
            updateDevice(info);
            continue;
        }

        int index = 0;
        switch (info->connectStatus()) {
        case DeviceInfo::Connected:
            // 连接中的设备放第一个
            index = 0;
            break;
        case DeviceInfo::Connectable: {
            index = findLast(DeviceInfo::Connectable, info);
            if (index != -1)
                break;

            index = findFirst(DeviceInfo::Offline);
            if (index != -1)
                break;

            index = allDeviceList.size();
        } break;
        case DeviceInfo::Offline:
            index = allDeviceList.size();
            break;
        }

        if (isStoped)
            return;

        allDeviceList.insert(index, info);
        visibleDeviceList.insert(index, info);
        Q_EMIT sortFilterResult(index, info);
    }
}

void SortFilterWorker::removeDevice(const QString &ip)
{
    for (int i = 0; i < visibleDeviceList.size(); ++i) {
        if (visibleDeviceList[i]->ipAddress() != ip)
            continue;

        allDeviceList.removeOne(visibleDeviceList[i]);
        visibleDeviceList.removeAt(i);
        Q_EMIT deviceRemoved(i);
        break;
    }
}

void SortFilterWorker::filterDevice(const QString &filter)
{
    visibleDeviceList.clear();
    int index = -1;
    for (const auto &dev : allDeviceList) {
        if (dev->deviceName().contains(filter, Qt::CaseInsensitive)
            || dev->ipAddress().contains(filter, Qt::CaseInsensitive)) {
            ++index;
            visibleDeviceList.append(dev);
            Q_EMIT sortFilterResult(index, dev);
        }
    }

    Q_EMIT filterFinished();
}

void SortFilterWorker::clear()
{
    allDeviceList.clear();
}

int SortFilterWorker::findFirst(DeviceInfo::ConnectStatus state)
{
    int index = -1;
    auto iter = std::find_if(allDeviceList.cbegin(), allDeviceList.cend(),
                             [&](const DeviceInfoPointer info) {
                                 if (isStoped)
                                     return true;
                                 index++;
                                 return info->connectStatus() == state;
                             });

    if (iter == allDeviceList.cend())
        return -1;

    return index;
}

int SortFilterWorker::findLast(DeviceInfo::ConnectStatus state, const DeviceInfoPointer info)
{
    bool isRecord = transHistory->contains(info->ipAddress());
    int startPos = -1;
    int endPos = -1;

    for (int i = allDeviceList.size() - 1; i >= 0; --i) {
        if (allDeviceList[i]->connectStatus() == state) {
            startPos = (startPos == -1 ? i : startPos);
            endPos = i;

            if (!isRecord)
                return startPos + 1;

            if (transHistory->contains(allDeviceList[i]->ipAddress()))
                return endPos + 1;
        }
    }

    return qMin(startPos, endPos);
}

void SortFilterWorker::updateDevice(const DeviceInfoPointer info)
{
    // 更新
    int index = indexOf(allDeviceList, info);
    if (allDeviceList[index]->discoveryMode() == DeviceInfo::DiscoveryMode::NotAllow)
        return removeDevice(allDeviceList[index]->ipAddress());

    if (allDeviceList[index]->deviceName() != info->deviceName()
        || allDeviceList[index]->connectStatus() != info->connectStatus()) {
        allDeviceList.replace(index, info);
    }

    if (!contains(visibleDeviceList, info))
        return;

    index = indexOf(visibleDeviceList, info);
    visibleDeviceList.replace(index, info);
    Q_EMIT deviceUpdated(index, info);
}

bool SortFilterWorker::contains(const QList<DeviceInfoPointer> &list, const DeviceInfoPointer info)
{
    auto iter = std::find_if(list.begin(), list.end(),
                             [&info](const DeviceInfoPointer it) {
                                 return it->ipAddress() == info->ipAddress();
                             });

    return iter != list.end();
}

int SortFilterWorker::indexOf(const QList<DeviceInfoPointer> &list, const DeviceInfoPointer info)
{
    int index = -1;
    auto iter = std::find_if(list.begin(), list.end(),
                             [&](const DeviceInfoPointer it) {
                                 index++;
                                 return it->ipAddress() == info->ipAddress();
                             });

    if (iter == list.end())
        return -1;

    return index;
}
