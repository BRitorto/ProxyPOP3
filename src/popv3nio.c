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
#include "logger.h"
#include "errorslib.h"
#include "Multiplexor.h"

#define BUFFER_SIZE 2048

typedef enum popv3State {
    CONNECTING,
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

    /** cantidad de referencias a este objeto. si es uno se debe destruir */
    unsigned references;
    /** siguiente en el pool */
    struct popv3 * next;
} popv3;

static void popv3Read(MultiplexorKey key);

static void popv3Write(MultiplexorKey key);

static void popv3Block(MultiplexorKey key);

static void popv3Close(MultiplexorKey key);

//static popv3 newPopv3(int clientFd, int originFd, size_t bufferSize, popv3State initialState);

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

#define ATTACHMENT(key) ( (struct popv3 *)(key)->data)

static void popv3Read(MultiplexorKey key) {
    logInfo("Starting to copy");
    copyInit(COPY, key);
    copyReadAndQueue(key);
    logInfo("Finishing copy");
}

static void popv3Write(MultiplexorKey key) {
    copyInit(COPY, key);
    copyWrite(key);
}

static void popv3Block(MultiplexorKey key) {

}

static void popv3Close(MultiplexorKey key) {

}

/**
 * Pool de `struct popv3', para ser reusados.
 *
 * Como tenemos un unico hilo que emite eventos no necesitamos barreras de
 * contención.
 */
static const unsigned  max_pool  = 50; // tamaño máximo
static unsigned        pool_size = 0;  // tamaño actual
static struct popv3  * pool      = 0;  // pool propiamente dicho

static popv3 * newPopv3(int clientFd, int originFd, size_t bufferSize) {
   
    struct popv3 * ret;
    if(pool == NULL) {
        ret = malloc(sizeof(*ret));
    } else {
        ret       = pool;
        pool      = pool->next;
        ret->next = 0;
    }
    if(ret == NULL) {
        goto finally;
    }
    memset(ret, 0x00, sizeof(*ret));

    ret->state = COPY;
    ret->clientFd = clientFd;
    ret->originFd = originFd;
    ret->readBuffer = createBuffer(BUFFER_SIZE);
    ret->writeBuffer = createBuffer(BUFFER_SIZE);
    
    ret->references = 1;
finally:
    return ret;
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
    logInfo("Accepting new client from: ");
    
    ////connecting to server
    originServerAddr originAddr = *((originServerAddr *) key->data);
    int originFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(originFd == -1) {
        exit(1);
    }
    if(connect(originFd, (struct sockaddr *) &originAddr.ipv4, sizeof(originAddr.ipv4)) == -1) {
        logFatal("imposible conectar con el servidor origen");
        exit(1); //ACA DEBERIAMOS IR A UN ESTADO
    }
    /////

    popv3 * p = newPopv3(client, originFd, BUFFER_SIZE);

    /// borrar
    pool_size++;
    int i = max_pool;
    i++;
    ///

    if(MUX_SUCCESS != registerFd(key->mux, client, &popv3Handler, READ, p)) {
        goto fail;
    }
    if(MUX_SUCCESS != registerFd(key->mux, originFd, &popv3Handler, READ, p)) { //READ QUIERO SI ME DA MENSAJE DE +OK DEVOLVERLO
        goto fail;
    }
    logInfo("Connection established, client fd: %d, origin fd:%d", client, originFd);

    return ;
fail:
    if(client != -1) {
        close(client);
    }
}

static void copyInit(const unsigned state, MultiplexorKey key) {
    struct popv3 * p = ATTACHMENT(key);

    copy * d        = &(p->clientCopy);
    d->fd           = &(p->clientFd);
    d->readBuffer   = p->readBuffer;
    d->writeBuffer  = p->writeBuffer;
    d->duplex       = READ | WRITE;
    d->other        = &(p->originCopy);

    d               = &(p->originCopy);
    d->fd           = &(p->originFd);
    d->readBuffer   = p->writeBuffer;
    d->writeBuffer  = p->readBuffer;
    d->duplex       = READ | WRITE;
    d->other        = &(p->clientCopy);
}

static fdInterest copyComputeInterests(MultiplexorADT mux, copy * d) {
    fdInterest ret = NO_INTEREST;
    if ((d->duplex & READ)  && canWrite(d->readBuffer)) {
        ret |= READ;
    }
    if ((d->duplex & WRITE) && canRead(d->writeBuffer)) {
        ret |= WRITE;
    }
    if(MUX_SUCCESS != setInterest(mux, *d->fd, ret)) {
        abort();
    }
    return ret;
}

static copy * copyPtr(MultiplexorKey key) {
    copy * d = &(ATTACHMENT(key)->clientCopy);

    if(*d->fd == key->fd) {
        // ok
    } else {
        d = d->other;
    }
    return  d;
}

static unsigned copyReadAndQueue(MultiplexorKey key) {
    copy * d = copyPtr(key);
    checkAreEquals(*d->fd, key->fd, "Copy destination and source have the same fd");

    logInfo("Copping from %s", (*d->fd == ATTACHMENT(key)->clientFd)? "client to server" : "server to client");

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
    logInfo("Total copied: %lu bytes", n);

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
        if(*d->other->fd != -1) {                       //shutdeteo tanto socket cliente como servidor
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

