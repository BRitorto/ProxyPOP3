/**
 * proxyPopv3nio.c  - controla el flujo de un proxy POPv3 (sockets no bloqueantes)
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

#include "proxyPopv3nio.h"
#include "buffer.h"
#include "logger.h"
#include "errorslib.h"
#include "Multiplexor.h"
#include "stateMachine.h"

#define BUFFER_SIZE 2048

#define N(x) (sizeof(x)/sizeof((x)[0]))

typedef enum proxyPopv3State {
    CLEAN_TRANSACTION,
    TRANSFORM_TRANSACTION,
    COPY,
    DONE,
    ERROR,
} proxyPopv3State;

typedef struct copy {
    int *           fd;
    bufferADT       readBuffer;
    bufferADT       writeBuffer;
    fdInterest      duplex;
    struct copy *   other;
} copy;

typedef struct proxyPopv3 {
    int originFd;
    int clientFd;
    bufferADT readBuffer;
    bufferADT writeBuffer;

    copy clientCopy;
    copy originCopy;

    /** maquinas de estados */
    struct stateMachineCDT stm;

    /** cantidad de referencias a este objeto. si es uno se debe destruir */
    unsigned references;
    /** siguiente en el pool */
    struct proxyPopv3 * next;
} proxyPopv3;

static void proxyPopv3Read(MultiplexorKey key);

static void proxyPopv3Write(MultiplexorKey key);

static void proxyPopv3Block(MultiplexorKey key);

static void proxyPopv3Close(MultiplexorKey key);

static void proxyPopv3Done(MultiplexorKey key);


static proxyPopv3 * newProxyPopv3(int clientFd, int originFd, size_t bufferSize);

static void deleteProxyPopv3(proxyPopv3 * p);

static void copyInit(const unsigned state, MultiplexorKey key);

static fdInterest copyComputeInterests(MultiplexorADT mux, copy * d);

static copy * copyPtr(MultiplexorKey key);

static unsigned copyReadAndQueue(MultiplexorKey key);

static unsigned copyWrite(MultiplexorKey key);

static const struct stateDefinition * proxyPopv3DescribeStates(void);


static const eventHandler proxyPopv3Handler = {
    .read   = proxyPopv3Read,
    .write  = proxyPopv3Write,
    .block  = proxyPopv3Block,
    .close  = proxyPopv3Close,
};

#define ATTACHMENT(key) ( (struct proxyPopv3 *)(key)->data)

static void proxyPopv3Read(MultiplexorKey key) {
    logInfo("Starting to copy");
    stateMachine stm = &ATTACHMENT(key)->stm;
    const proxyPopv3State state = stateMachineHandlerRead(stm, key);

    logDebug("state: %d", state);
    if(ERROR == state || DONE == state) {
        proxyPopv3Done(key);
    }
}

static void proxyPopv3Write(MultiplexorKey key) {
    stateMachine stm = &ATTACHMENT(key)->stm;
    const proxyPopv3State state = stateMachineHandlerWrite(stm, key);

    logInfo("Finishing copy");

    logDebug("state: %d", state);
    if(ERROR == state || DONE == state) {
        proxyPopv3Done(key);
    }
}

static void proxyPopv3Block(MultiplexorKey key) {

}

static void proxyPopv3Close(MultiplexorKey key) {
    deleteProxyPopv3(ATTACHMENT(key));
}

static void proxyPopv3Done(MultiplexorKey key) {
    logDebug("Estoy por desregistrar los fds en done");
    const int fds[] = {
        ATTACHMENT(key)->clientFd,
        ATTACHMENT(key)->originFd,
    };
    for(unsigned i = 0; i < N(fds); i++) {
        if(fds[i] != -1) {
            if(MUX_SUCCESS != unregisterFd(key->mux, fds[i])) {
                fail("Problem trying to unregister a fd: %d.", fds[i]);
            }
            close(fds[i]);
        }
    }
}

/**
 * Pool de `struct proxyPopv3', para ser reusados.
 *
 * Como tenemos un unico hilo que emite eventos no necesitamos barreras de
 * contención.
 */
static const unsigned  maxPool  = 50; // tamaño máximo
static unsigned        poolSize = 0;  // tamaño actual
static struct proxyPopv3  * pool      = 0;  // pool propiamente dicho

static proxyPopv3 * newProxyPopv3(int clientFd, int originFd, size_t bufferSize) {
   
    struct proxyPopv3 * ret;
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

    ret->clientFd = clientFd;
    ret->originFd = originFd;
    ret->readBuffer = createBuffer(bufferSize);
    ret->writeBuffer = createBuffer(bufferSize);
    
    ret->stm    .initial   = COPY;
    ret->stm    .maxState = ERROR;
    ret->stm    .states    = proxyPopv3DescribeStates();
    stateMachineInit(&ret->stm);

    ret->references = 1;
finally:
    return ret;
}

static void deleteProxyPopv3(proxyPopv3 * p) {
    if(p != NULL) {
        if(p->references == 1) {
            if(poolSize < maxPool) {
                p->next = pool;
                pool    = p;
                poolSize++;
            } else {
                free(p);
            }
        } else {
            p->references -= 1;
        }
    } 
}

void proxyPopv3PassiveAccept(MultiplexorKey key) {

    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len = sizeof(client_addr);
    proxyPopv3 *                  proxy           = NULL;

    const int clientFd = accept(key->fd, (struct sockaddr*) &client_addr, &client_addr_len);
    if(clientFd == -1) {
        goto fail;
    }
    if(fdSetNIO(clientFd) == -1) {
        goto fail;
    }
    logInfo("Accepting new client from: ");
    
    ////connecting to server
    originServerAddr originAddr = *((originServerAddr *) key->data);
    int originFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(originFd == -1) {
        logFatal("origin fd = -1");
        exit(1);
    }
    if(connect(originFd, (struct sockaddr *) &originAddr.ipv4, sizeof(originAddr.ipv4)) == -1) {
        logFatal("imposible conectar con el servidor origen");
        exit(1); //ACA DEBERIAMOS IR A UN ESTADO
    }
    /////

    proxy = newProxyPopv3(clientFd, originFd, BUFFER_SIZE);
    logDebug("Proxy hecho");
    if(MUX_SUCCESS != registerFd(key->mux, clientFd, &proxyPopv3Handler, READ, proxy)) {
        goto fail;
    }
    if(MUX_SUCCESS != registerFd(key->mux, originFd, &proxyPopv3Handler, READ, proxy)) { //READ QUIERO SI ME DA MENSAJE DE +OK DEVOLVERLO
        goto fail;
    }
    logInfo("Connection established, client fd: %d, origin fd:%d", clientFd, originFd);

    return ;
fail:
    if(clientFd != -1) {
        close(clientFd);
    }
    deleteProxyPopv3(proxy);
}

static void copyInit(const unsigned state, MultiplexorKey key) {
    struct proxyPopv3 * p = ATTACHMENT(key);

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
    if ((d->duplex & READ)  && canWrite(d->readBuffer))
        ret |= READ;
    if ((d->duplex & WRITE) && canRead(d->writeBuffer)) {
        ret |= WRITE;
    }
    logDebug("Interes computado: %d", ret);
    if(MUX_SUCCESS != setInterest(mux, *d->fd, ret))
        fail("Problem trying to set interest: %d,to multiplexor in copy, fd: %d.", ret, *d->fd);
    
    return ret;
}

static copy * copyPtr(MultiplexorKey key) {
    copy * d = &(ATTACHMENT(key)->clientCopy);

    if(*d->fd != key->fd)
        d = d->other;
    
    return  d;
}

static unsigned copyReadAndQueue(MultiplexorKey key) {
    copy * d = copyPtr(key);
    checkAreEquals(*d->fd, key->fd, "Copy destination and source have the same fd");

    size_t size;
    ssize_t n;
    bufferADT buffer = d->readBuffer;
    unsigned ret = COPY;

    uint8_t *ptr = getWritePtr(buffer, &size);
    n = recv(key->fd, ptr, size, 0);
    if(n <= 0) {
        logDebug("ALGUIEN CERRO LA CONEXION");
        shutdown(*d->fd, SHUT_RD);
        d->duplex &= ~READ;
        if(*d->other->fd != -1) {
            shutdown(*d->other->fd, SHUT_WR);
            d->other->duplex &= ~WRITE;
        }
    } else {
        updateWritePtr(buffer, n);
    }

    logDebug("duplex READ interest: %d", d->duplex);
    
    logMetric("Coppied from %s, total copied: %lu bytes", (*d->fd == ATTACHMENT(key)->clientFd)? "client to server" : "server to client", n);

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
    logDebug("duplex WRITE interest: %d", d->duplex);

    copyComputeInterests(key->mux, d);
    copyComputeInterests(key->mux, d->other);
    if(d->duplex == NO_INTEREST) {
        ret = DONE;
    }
    return ret;
}

/* definición de handlers para cada estado */
static const struct stateDefinition clientStatbl[] = {
    {
        .state            = CLEAN_TRANSACTION,
    }, {
        .state            = TRANSFORM_TRANSACTION,
    },{
        .state            = COPY,
        .onArrival        = copyInit,
        .onReadReady      = copyReadAndQueue,
        .onWriteReady     = copyWrite,
    }, {
        .state            = DONE,

    },{
        .state            = ERROR,
    }
};

static const struct stateDefinition * proxyPopv3DescribeStates(void) {
    return clientStatbl;
}

