// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ZRPC_NETADDRESS_H
#define ZRPC_NETADDRESS_H

#include <unistd.h>
#include <memory>
#include "co/fastring.h"
#include "co/fastream.h"

namespace zrpc {

// typedef  struct net_address_t {
//     char ip[128] = {0};
//     uint16 port;
//     bool use_ssl;
//     char ssl_key[4096] = {0};
//     char ssl_crt[4096] = {0};
// } NetAddress;

class NetAddress {

public:
    typedef std::shared_ptr<NetAddress> ptr;

    NetAddress(const char *ip, uint16 port, char *key, char *crt);
    NetAddress(const char *ip, uint16 port, bool ssl);

    fastring toString() const {
        fastream ss;
        ss << m_ip << ":" << m_port << ":" << m_ssl_key << ":" << m_ssl_crt;
        return ss.str();
    }

    char* getIP() { return m_ip; }

    int getPort() { return m_port; }

    bool isSSL() { return m_ssl; }

    char* getKey() { return m_ssl_key; }

    char* getCrt() { return m_ssl_crt; }

private:
    char m_ip[128] = {0};
    uint16 m_port;
    bool m_ssl;
    char m_ssl_key[4096] = {0};
    char m_ssl_crt[4096] = {0};
};

} // namespace zrpc

#endif
