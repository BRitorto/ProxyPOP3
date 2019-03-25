#ifndef NET_UTILS_H
#define NET_UTILS_H

#include <stdlib.h>

typedef enum addressType {
    ADDR_IPV4   = 0x01,
    ADDR_IPV6   = 0x02,
    ADDR_DOMAIN = 0x03,
} addressType;

typedef union address {
    char                    fqdn[0xFF];
    struct sockaddr_storage addrStorage;
} address;

typedef struct addressData {
    addressType             type;
    address                 addr;
    /** Port in network byte order */
    in_port_t               port;

    socklen_t               addrLength;
    int                     domain;
} addressData;

void setAddress(addressData * address, const char * strIP);


void sockaddrToString(char * buffer, const size_t bufferSize, const struct sockaddr * address);

#endif

