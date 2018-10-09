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

#define BUFFER_SIZE 2048

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
    int originFd;
    int clientFd;
    bufferADT readBuffer;
    bufferADT writeBuffer;
    copy clientCopy;
    copy originCopy;
} popv3;

static void popv3Read(MultiplexorKey key);

static void popv3Write(MultiplexorKey key);

static void popv3Block(MultiplexorKey key);

static void popv3Close(MultiplexorKey key);

static popv3 newPopv3(int clientFd, int originFd, size_t bufferSize, popv3State initialState);

static void copyInit(const unsigned state, MultiplexorKey key);

static fdInterest copyComputeInterests(MultiplexorADT mux, copy * d);

static copy * copyPtr(MultiplexorKey key);

static unsigned copyReadAndQueue(MultiplexorKey key);

static unsigned copyWrite(MultiplexorKey key);


static const eventHandler popv3Handler = {
    .read   = popv3Read,
    .write  = popv3Write,
    .block  = popv3Block,
    .close  = popv3Close,
};

static void popv3Read(MultiplexorKey key) {
    /* echo server
    int fd = key->fd;
    char buffer[256];
    memset(buffer, 0, 256);
    recv(fd, buffer, 256, 0);
    printf("%s",buffer);
    send(fd, buffer, 256, MSG_NOSIGNAL);*/
    copyInit(COPY, key);
    copyReadAndQueue(key);

}

static void popv3Write(MultiplexorKey key) {
    copyInit(COPY, key);
    copyWrite(key);
}

static void popv3Block(MultiplexorKey key) {

}

static void popv3Close(MultiplexorKey key) {

}


static popv3 newPopv3(int clientFd, int originFd, size_t bufferSize, popv3State initialState) {
    struct popv3 p = {
        .state = initialState,
        .originFd = originFd,
        .clientFd = clientFd,
        .readBuffer = createBuffer(bufferSize),
        .writeBuffer = createBuffer(bufferSize),
    }; 
    return p;
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

    popv3 p = newPopv3(client, *((int *)key->data), BUFFER_SIZE, COPY);


    if(MUX_SUCCESS != registerFd(key->mux, client, &popv3Handler, READ, &p)) {
        goto fail;
    }
    return ;
fail:
    if(client != -1) {
        close(client);
    }
}


static void copyInit(const unsigned state, MultiplexorKey key) {
    popv3 * p = (struct popv3 *)(key->data);
    copy * d        = &p->clientCopy;

    d->fd           = &p->clientFd;
    d->readBuffer   = p->readBuffer;
    d->writeBuffer  = p->writeBuffer;
    d->duplex       = READ | WRITE;
    d->other        = &p->originCopy;

    d               = &p->originCopy;
    d->fd           = &p->originFd;
    d->readBuffer   = p->writeBuffer;
    d->writeBuffer  = p->readBuffer;
    d->duplex       = READ | WRITE;
    d->other        = &p->clientCopy;
}

/**
 * Computa los intereses en base a la disponiblidad de los buffer.
 * La variable duplex nos permite saber si alguna vía ya fue cerrada.
 * Arrancá READ | WRITE.
 */
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

/** elige la estructura de copia correcta de cada fd (origin o client) */
static copy * copyPtr(MultiplexorKey key) {
    popv3 * p = (struct popv3 *)(key->data);
    copy * d = &p->clientCopy;
    if(*d->fd != key->fd) {
        d = d->other;
    }
    return  d;
}

static unsigned copyReadAndQueue(MultiplexorKey key) {
    copy * d = copyPtr(key);

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
    copy * d = copyPtr(key);
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
}

