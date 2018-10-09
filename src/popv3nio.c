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
#include <sys/socket.h>
#include <arpa/inet.h>

#include "popv3nio.h"
#include "buffer.h"
#include "Multiplexor.h"

typedef enum popv3State {
    AUTHORIZATION,
    TRANSACTION,
    UPDATE,
    COPY,
    DONE,
    ERROR,
} popv3State;

typedef struct copy {
    int *           fd;
    bufferADT       readBuffer;
    bufferADT       writeBuffer;
    fdInterest      duplex;
    struct copy *   other;
} copy;

typedef struct popv3 {
    popv3State state;
    copy copyStruct;
} popv3;

static void popv3Read(MultiplexorKey key);

static void popv3Write(MultiplexorKey key);

static void popv3Block(MultiplexorKey key);

static void popv3Close(MultiplexorKey key);

static const eventHandler popv3Handler = {
    .read   = popv3Read,
    .write  = popv3Write,
    .block  = popv3Block,
    .close  = popv3Close,
};

static void popv3Read   (MultiplexorKey key) {
    /* echo server
    int fd = key->fd;
    char buffer[256];
    memset(buffer, 0, 256);
    recv(fd, buffer, 256, 0);
    printf("%s",buffer);
    send(fd, buffer, 256, MSG_NOSIGNAL);*/


}

static void popv3Write(MultiplexorKey key) {

}

static void popv3Block(MultiplexorKey key) {

}

static void popv3Close(MultiplexorKey key) {

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



    //copy c ={
      //  .fd = *key->data;
       // .
    //}


    if(MUX_SUCCESS != registerFd(key->mux, client, &popv3Handler, READ, key->data)) {
        goto fail;
    }
    return ;
fail:
    if(client != -1) {
        close(client);
    }
}

/**
 * Computa los intereses en base a la disponiblidad de los buffer.
 * La variable duplex nos permite saber si alguna vía ya fue cerrada.
 * Arrancá OP_READ | OP_WRITE.

static fdInterest copyComputeInterests(MultiplexorADT mux, copy * d) {
    fdInterest ret = NO_INTEREST;
    if ((d->duplex & READ)  && canWrite(d->readBuffer)) {
        ret |= READ;
    }
    if ((d->duplex & WRITE) && canRead (d->writeBuffer)) {
        ret |= WRITE;
    }
    if(MUX_SUCCESS != setInterest(mux, *d->fd, ret)) {
        abort();
    }
    return ret;
}


static unsigned copyReadAndQueue(MultiplexorKey key) {
    copy * d = key->data;

    assert(*d->fd == key->fd);

    size_t size;
    ssize_t n;
    bufferADT buffer = d->readBuffer;
    unsigned ret = COPY;

    uint8_t *ptr = getWritePtr(buffer, &size);
    n = recv(key->fd, ptr, size, 0);
    if(n <= 0) {
        shutdown(*d->fd, SHUT_RD);
        d->duplex &= ~READ;
        if(*d->other->fd != -1) {
            shutdown(*d->other->fd, SHUT_WR);
            d->other->duplex &= ~WRITE;
        }
    } else {
        updateWritePtr(buffer, n);
    }
    copyComputeInterests(key->mux, d);
    copyComputeInterests(key->mux, d->other);
    if(d->duplex == NO_INTEREST) {
        ret = DONE;
    }
    return ret;
}

static unsigned copyWrite(MultiplexorKey key) {
    copy * d = key->data;
    assert(*d->fd == key->fd);
    size_t size;
    ssize_t n;
    bufferADT buffer = d->writeBuffer;
    unsigned ret = COPY;

    uint8_t *ptr = getReadPtr(buffer, &size);
    n = send(key->fd, ptr, size, MSG_NOSIGNAL);
    if(n == -1) {
        shutdown(*d->fd, SHUT_WR);
        d->duplex &= ~WRITE;
        if(*d->other->fd != -1) {
            shutdown(*d->other->fd, SHUT_RD);
            d->other->duplex &= ~READ;
        }
    } else {
        updateReadPtr(buffer, n);
    }
    copyComputeInterests(key->mux, d);
    copyComputeInterests(key->mux, d->other);
    if(d->duplex == NO_INTEREST) {
        ret = DONE;
    }
    return ret;
} */

