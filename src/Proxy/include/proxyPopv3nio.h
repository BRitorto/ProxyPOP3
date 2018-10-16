#ifndef PROXY_POPV3_NIO_H
#define PROXY_POPV3_NIO_H

#include "multiplexor.h"

typedef union originServerAddr {
    char fqdn[0xFF];
    struct sockaddr_in  ipv4;
    struct sockaddr_in6 ipv6;
} originServerAddr;

void proxyPopv3PassiveAccept(MultiplexorKey key);

#endif

