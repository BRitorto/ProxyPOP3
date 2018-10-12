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
#include "multiplexor.h"
#include "stateMachine.h"

#define BUFFER_SIZE 2048

#define N(x) (sizeof(x)/sizeof((x)[0]))

typedef enum proxyPopv3State {
    HELLO_READ,
    CHECK_PIPELINIG,
    HELLO_WRITE,
    COPY,
    DONE,
    ERROR,
} proxyPopv3State;

typedef struct checkPipelining {
    /** buffer utilizado para I/O */
    bufferADT           readBuffer;
    bufferADT           writeBuffer;
    //pipeliningParser    parser;
} checkPipelining;

typedef struct copyStruct {
    int *           fd;
    bufferADT       readBuffer;
    bufferADT       writeBuffer;
    fdInterest      duplex;
    struct copyStruct *   other;
} copyStruct;

typedef struct proxyPopv3 {
    int originFd;
    int clientFd;
    bufferADT readBuffer;
    bufferADT writeBuffer;

    /** estados para el client_fd */
    union {
        copyStruct          copy;
    } client;
    /** estados para el origin_fd */
    union {
        checkPipelining     checkPipelining;
        copyStruct          copy;
    } origin;

    /** maquinas de estados */
    struct stateMachineCDT stm;

    /** cantidad de referencias a este objeto. si es uno se debe destruir */
    unsigned references;
    /** siguiente en el pool */
    struct proxyPopv3 * next;
} proxyPopv3;

#define ATTACHMENT(key) ( (struct proxyPopv3 *)(key)->data)

static void proxyPopv3Read(MultiplexorKey key);

static void proxyPopv3Write(MultiplexorKey key);

static void proxyPopv3Block(MultiplexorKey key);

static void proxyPopv3Close(MultiplexorKey key);

static void proxyPopv3Done(MultiplexorKey key);


static proxyPopv3 * newProxyPopv3(int clientFd, int originFd, size_t bufferSize);

static void deleteProxyPopv3(proxyPopv3 * p);

static void copyInit(const unsigned state, MultiplexorKey key);

static fdInterest copyComputeInterests(MultiplexorADT mux, copyStruct * d);

static copyStruct * copyPtr(MultiplexorKey key);

static unsigned copyReadAndQueue(MultiplexorKey key);

static unsigned copyWrite(MultiplexorKey key);

static const struct stateDefinition * proxyPopv3DescribeStates(void);


static const eventHandler proxyPopv3Handler = {
    .read   = proxyPopv3Read,
    .write  = proxyPopv3Write,
    .block  = proxyPopv3Block,
    .close  = proxyPopv3Close,
};

static void proxyPopv3Read(MultiplexorKey key) {
    stateMachine stm = &ATTACHMENT(key)->stm;
    const proxyPopv3State state = stateMachineHandlerRead(stm, key);

    logDebug("State: %d", state);
    if(ERROR == state || DONE == state) {
        proxyPopv3Done(key);
    }
}

static void proxyPopv3Write(MultiplexorKey key) {
    stateMachine stm = &ATTACHMENT(key)->stm;
    const proxyPopv3State state = stateMachineHandlerWrite(stm, key);


    logDebug("State: %d", state);
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
    logDebug("Connection close, unregistering file descriptors from multiplexor");
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
    
    ret->stm    .initial   = HELLO_READ;
    ret->stm    .maxState = ERROR;
    ret->stm    .states    = proxyPopv3DescribeStates();
    stateMachineInit(&ret->stm);

    ret->references = 1;
finally:
    return ret;
}

static void realDeleteProxyPopv3(proxyPopv3 * proxy) {
    deleteBuffer(proxy->readBuffer);
    deleteBuffer(proxy->writeBuffer);
    free(proxy);
}

static void deleteProxyPopv3(proxyPopv3 * proxy) {
    if(proxy != NULL) {
        if(proxy->references == 1) {
            if(poolSize < maxPool) {
                proxy->next = pool;
                pool        = proxy;
                poolSize++;
            } else {
                realDeleteProxyPopv3(proxy);
            }
        } else {
            proxy->references -= 1;
        }
    } 
}

static void errorConnectingOriginHandler(void * data) {
    const int clientFd = *((int *) data);
    const char * errorMsg = "-ERR Unable to reach the server.\r\n";

    if(clientFd != -1) {
        //escribiendo sin saber si podemos escribir puede BLOQUEAR
        send(clientFd, errorMsg, strlen(errorMsg), MSG_NOSIGNAL);
        close(clientFd);
    }
}

static int connectToOrigin(originServerAddr * originAddr, int clientFd) {
    int originFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    checkFailWithFinally(originFd, errorConnectingOriginHandler, &clientFd, "Origin fd = -1");
    
    int connectResp = connect(originFd, (struct sockaddr *) &originAddr->ipv4, sizeof(originAddr->ipv4));
    checkFailWithFinally(connectResp, errorConnectingOriginHandler, &clientFd, "Unable to reach the origin server");
    return originFd;
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
    logInfo("Accepting new client.");
    
    int originFd = connectToOrigin(key->data, clientFd);
    proxy = newProxyPopv3(clientFd, originFd, BUFFER_SIZE);
    logDebug("Proxy done.");

    if(MUX_SUCCESS != registerFd(key->mux, clientFd, &proxyPopv3Handler, NO_INTEREST, proxy)) {
        goto fail;
    }
    if(MUX_SUCCESS != registerFd(key->mux, originFd, &proxyPopv3Handler, READ, proxy)) {
        goto fail;
    }
    logInfo("Connection established, client fd: %d, origin fd:%d.", clientFd, originFd);

    return ;
fail:
    if(clientFd != -1) {
        close(clientFd);
    }
    deleteProxyPopv3(proxy);
}

static void helloReadInit(const unsigned state, MultiplexorKey key) {
    proxyPopv3 * proxy      = ATTACHMENT(key);
    checkPipelining * check = &proxy->origin.checkPipelining;

    check->readBuffer   = proxy->readBuffer;
    check->writeBuffer  = proxy->writeBuffer;
}

/** lee todos los bytes del mensaje de tipo `hello' y inicia su proceso */
static unsigned helloRead(MultiplexorKey key) {
    checkPipelining * check = &ATTACHMENT(key)->origin.checkPipelining;
    unsigned  ret      = HELLO_READ;
     uint8_t *writePtr;
      size_t  count;
     ssize_t  n;
    logDebug("Server send something");

    writePtr = getWritePtr(check->readBuffer, &count);
    n = recv(key->fd, writePtr, count, 0);
    logDebug("Server send something");
    if(n > 0) {
        updateWritePtr(check->readBuffer, n);
        if(n >= 3) {
            char * helloOrigin = (char *) getReadPtr(check->readBuffer, &count);
            if(strncmp(helloOrigin, "+OK", 3) == 0) {
                if(MUX_SUCCESS == setInterestKey(key, WRITE)) {
                    ret = CHECK_PIPELINIG;
                } else {
                    ret = ERROR;
                }
            }
        }
    } else {
        ret = ERROR;
    }

    return ret;
}
/*
static void checkPipeliningInit(const unsigned state, MultiplexorKey key) {
    checkPipelining * check = &ATTACHMENT(key)->origin.checkPipelining;

    check->readBuffer                      = &(ATTACHMENT(key)->readBuffer);
    check->writeBuffer                     = &(ATTACHMENT(key)->writeBuffer);
    check->parser.data                     = &d->method;
    check->parser.on_authentication_method = on_hello_method, hello_parser_init(
            &d->parser);
}

static void checkPipeliningClose(const unsigned state, MultiplexorKey key) {
    checkPipelining check = &ATTACHMENT(key)->origin.checkPipelining;

    hello_parser_close(&d->parser);
}*/

static unsigned checkPipeliningWrite(MultiplexorKey key) {
    
    unsigned  ret     = CHECK_PIPELINIG;
      size_t  count = 6; //tamaño de buffer
     ssize_t  n;

    const char * capaMsg = "CAPA\r\n";
    n = send(key->fd, capaMsg, count, MSG_NOSIGNAL);
    if(n == -1) {
        ret = ERROR;
    } else if(MUX_SUCCESS == setInterestKey(key, READ)) {
        ret = CHECK_PIPELINIG;
    } else {
        ret = ERROR;
    }
    return ret;
}

static unsigned checkPipeliningRead(MultiplexorKey key) {
     
    logDebug("check pipelining read");
    //checkPipelining * check = &ATTACHMENT(key)->origin.checkPipelining;
    unsigned  ret      = CHECK_PIPELINIG;
    uint8_t *writePtr = calloc(256,1);
    //size_t  count;
    ssize_t  n;

    //writePtr = getWritePtr(check->readBuffer, &count);
    n = recv(key->fd, writePtr, 256, 0);
    if(n > 0) {
        /*updateWritePtr(check->readBuffer, n);

        char * source = (char *) getReadPtr(check->readBuffer, &count);
        char * dest = malloc(n+1);
        memcpy(dest, source, n);
        *(dest+n) = 0;

        updateReadPtr(check->readBuffer, n);*/
        logWarn("Capa: %s", writePtr);

        if(MUX_SUCCESS == setInterestKey(key, NO_INTEREST) &&
            MUX_SUCCESS == setInterest(key->mux, ATTACHMENT(key)->clientFd, WRITE)) {
            ret = HELLO_WRITE;
        } else {
            ret = ERROR;
        }
    } else {
        ret = ERROR;
    }

    return ret;
}

/** escribe todos los bytes de la respuesta al mensaje `hello' */
static unsigned
helloWrite(MultiplexorKey key) {
    //const char * helloMsg = "+OK Como va viejo?\r\n";
    checkPipelining * check = &ATTACHMENT(key)->origin.checkPipelining;
    unsigned  ret     = HELLO_WRITE;
      size_t  count;// = strlen(helloMsg); //tamaño del buffer
     ssize_t  n;
     uint8_t * readPtr = getReadPtr(check->readBuffer, &count);
    n = send(key->fd, readPtr, count, MSG_NOSIGNAL);
    if(n == -1) {
        ret = ERROR;
    } else if(MUX_SUCCESS == setInterestKey(key, READ)) {
        ret = COPY;
    } else {
        ret = ERROR;
    }
    return ret;

    return ret;
}

static void copyInit(const unsigned state, MultiplexorKey key) {
    struct proxyPopv3 * proxy = ATTACHMENT(key);

    copyStruct * copy  = &(proxy->client.copy);
    copy->fd           = &(proxy->clientFd);
    copy->readBuffer   = proxy->readBuffer;
    copy->writeBuffer  = proxy->writeBuffer;
    copy->duplex       = READ | WRITE;
    copy->other        = &(proxy->origin.copy);

    copy               = &(proxy->origin.copy);
    copy->fd           = &(proxy->originFd);
    copy->readBuffer   = proxy->writeBuffer;
    copy->writeBuffer  = proxy->readBuffer;
    copy->duplex       = READ | WRITE;
    copy->other        = &(proxy->client.copy);
}

static fdInterest copyComputeInterests(MultiplexorADT mux, copyStruct * d) {
    fdInterest ret = NO_INTEREST;
    if ((d->duplex & READ)  && canWrite(d->readBuffer))
        ret |= READ;
    if ((d->duplex & WRITE) && canRead(d->writeBuffer)) {
        ret |= WRITE;
    }
    logDebug("Computed interest: %d.", ret);
    if(MUX_SUCCESS != setInterest(mux, *d->fd, ret))
        fail("Problem trying to set interest: %d,to multiplexor in copy, fd: %d.", ret, *d->fd);
    
    return ret;
}

static copyStruct * copyPtr(MultiplexorKey key) {
    copyStruct * copy = &(ATTACHMENT(key)->client.copy);

    if(*copy->fd != key->fd)
        copy = copy->other;
    
    return  copy;
}

static unsigned copyReadAndQueue(MultiplexorKey key) {
    copyStruct * copy = copyPtr(key);
    checkAreEquals(*copy->fd, key->fd, "Copy destination and source have the same file descriptor.");

    size_t size;
    ssize_t n;
    bufferADT buffer = copy->readBuffer;
    unsigned ret = COPY;

    uint8_t *ptr = getWritePtr(buffer, &size);
    n = recv(key->fd, ptr, size, 0);
    if(n <= 0) {
        logDebug("Someone close the connection.");
        shutdown(*copy->fd, SHUT_RD);
        copy->duplex &= ~READ;
        if(*copy->other->fd != -1) {
            shutdown(*copy->other->fd, SHUT_WR);
            copy->other->duplex &= ~WRITE;
        }
    } else {
        updateWritePtr(buffer, n);
    }

    logDebug("Duplex READ interest: %d.", copy->duplex);
    
    logMetric("Coppied from %s, total copied: %lu bytes.", (*copy->fd == ATTACHMENT(key)->clientFd)? "client to server" : "server to client", n);

    copyComputeInterests(key->mux, copy);
    copyComputeInterests(key->mux, copy->other);
    if(copy->duplex == NO_INTEREST) {
        ret = DONE;
    }
    return ret;
}

static unsigned copyWrite(MultiplexorKey key) {
    copyStruct * copy = copyPtr(key);
    checkAreEquals(*copy->fd, key->fd, "Copy destination and source have the same file descriptor.");

    size_t size;
    ssize_t n;
    bufferADT buffer = copy->writeBuffer;
    unsigned ret = COPY;

    uint8_t *ptr = getReadPtr(buffer, &size);
    n = send(key->fd, ptr, size, MSG_NOSIGNAL);
    if(n == -1) {
        shutdown(*copy->fd, SHUT_WR);
        copy->duplex &= ~WRITE;
        if(*copy->other->fd != -1) {                       //shutdeteo tanto socket cliente como servidor
            shutdown(*copy->other->fd, SHUT_RD);
            copy->other->duplex &= ~READ;
        }
    } else {
        updateReadPtr(buffer, n);
    }
    logDebug("Duplex WRITE interest: %d.", copy->duplex);

    copyComputeInterests(key->mux, copy);
    copyComputeInterests(key->mux, copy->other);
    if(copy->duplex == NO_INTEREST) {
        ret = DONE;
    }
    return ret;
}

/* definición de handlers para cada estado */
static const struct stateDefinition clientStatbl[] = {
    {
        .state            = HELLO_READ,
        .onArrival        = helloReadInit,
        .onReadReady      = helloRead,
    }, {
        .state            = CHECK_PIPELINIG,
        .onReadReady      = checkPipeliningRead,
        .onWriteReady     = checkPipeliningWrite,
    }, {
        .state            = HELLO_WRITE,
        .onWriteReady     = helloWrite,
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

