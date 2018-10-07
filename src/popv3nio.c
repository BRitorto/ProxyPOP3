/**
 * popv3nio.c  - controla el flujo de un proxy POPv3 (sockets no bloqueantes)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include <arpa/inet.h>
#include "popv3nio.h"

static void popv3Read   (MultiplexorKey key);

static void popv3Write  (MultiplexorKey key);

static void popv3Block  (MultiplexorKey key);

static void popv3Close  (MultiplexorKey key);

static const eventHandler popv3Handler = {
    .read   = popv3Read,
    .write  = popv3Write,
    .block  = popv3Block,
    .close  = popv3Close,
};

static void popv3Read   (MultiplexorKey key) {
    int fd = key->fd;
    char buffer[256];
    memset(buffer, 0, 256);
    recv(fd, buffer, 256, 0);
    printf("%s",buffer);
    send(fd, buffer, 256, MSG_NOSIGNAL);
}

static void popv3Write  (MultiplexorKey key) {

}

static void popv3Block  (MultiplexorKey key) {

}

static void popv3Close  (MultiplexorKey key) {

}

void popv3PassiveAccept(MultiplexorKey key) {

    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len = sizeof(client_addr);

    const int client = accept(key->fd, (struct sockaddr*) &client_addr, &client_addr_len);
    if(client == -1) {
        goto fail;
    }
    if(fdSetNIO(client) == -1) {
        goto fail;
    }
    /*state = socks5_new(client);
    if(state == NULL) {
        // sin un estado, nos es imposible manejaro.
        // tal vez deberiamos apagar accept() hasta que detectemos
        // que se liberó alguna conexión.
        goto fail;
    }
    memcpy(&state->client_addr, &client_addr, client_addr_len);
    state->client_addr_len = client_addr_len;
    */
    if(MUX_SUCCESS != registerFd(key->mux, client, &popv3Handler, READ, NULL)) {
        goto fail;
    }
    return ;
fail:
    if(client != -1) {
        close(client);
    }
}

