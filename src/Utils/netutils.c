#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "netutils.h"


/**
 * Inicializa una varibale del tipo addressData a través de un string.
 *
 *
 * @param address puntero al addressData a inicializar.
 * @param strIp string de inicialización.
 */
void setAddress(addressData * address, const char * strIP) {
    
    memset(&(address->addr.addrStorage), 0, sizeof(address->addr.addrStorage));
    
    address->type   = ADDR_IPV4;
    address->domain = AF_INET;
    address->addrLength = sizeof(struct sockaddr_in);
    struct sockaddr_in originIpv4; 
    memset(&(originIpv4), 0, sizeof(originIpv4));
    originIpv4.sin_family = AF_INET;
    int result = 0;
    if((result = inet_pton(AF_INET, strIP, &originIpv4.sin_addr.s_addr)) <= 0) {
        address->type   = ADDR_IPV6;
        address->domain = AF_INET6;
        address->addrLength = sizeof(struct sockaddr_in6);
        struct sockaddr_in6 originIpv6; 
        memset(&(originIpv6), 0, sizeof(originIpv6));
        originIpv6.sin6_family = AF_INET6;
        if((result = inet_pton(AF_INET6, strIP, &originIpv6.sin6_addr.s6_addr)) <= 0){
            memset(&(address->addr.addrStorage), 0, sizeof(address->addr.addrStorage));
            address->type   = ADDR_DOMAIN;
            memcpy(address->addr.fqdn, strIP, strlen(strIP));
            return;
        }
        originIpv6.sin6_port = htons(address->port); 
        memcpy(&address->addr.addrStorage, &originIpv6, address->addrLength);    
        return;
    }    
    originIpv4.sin_port = htons(address->port); 
    memcpy(&address->addr.addrStorage, &originIpv4, address->addrLength);
    return;
}

/**
 * Convierte en un string para lectura humana de una dirección IP.
 */
void sockaddrToString(char * buffer, const size_t bufferSize, const struct sockaddr * address) {
    if(address == NULL) {
        strncpy(buffer, "NULL", bufferSize);
        return;
    }
    in_port_t port;
    void * p = 0x00;
    bool handled = false;

    switch(address->sa_family) {
        case AF_INET:
            p    = &((struct sockaddr_in *) address)->sin_addr;
            port =  ((struct sockaddr_in *) address)->sin_port;
            handled = true;
            break;
        case AF_INET6:
            p    = &((struct sockaddr_in6 *) address)->sin6_addr;
            port =  ((struct sockaddr_in6 *) address)->sin6_port;
            handled = true;
            break;
    }
    if(handled) {
        if (inet_ntop(address->sa_family, p,  buffer, bufferSize) == 0) {
            strncpy(buffer, "Unknown IP Address", bufferSize);
            buffer[bufferSize - 1] = 0;
        }
    } else {
        strncpy(buffer, "Unknown", bufferSize);
    }

    strncat(buffer, ":", bufferSize);
    buffer[bufferSize - 1] = 0;
    const size_t length = strlen(buffer);

    if(handled) {
        snprintf(buffer + length, bufferSize - length, "%d", ntohs(port));
    }
    buffer[bufferSize - 1] = 0;

    return;
}
