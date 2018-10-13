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
#include <signal.h>

#include "proxyPopv3nio.h"
#include "buffer.h"
#include "logger.h"
#include "errorslib.h"
#include "multiplexor.h"
#include "stateMachine.h"
#include "helloParser.h"
#include "capaParser.h"

#define BUFFER_SIZE 2048

#define N(x) (sizeof(x)/sizeof((x)[0]))

typedef enum proxyPopv3State {
    HELLO_READ,
    CHECK_CAPABILITIES,
    HELLO_WRITE,
    COPY,
    TRANSFORM,
    DONE,
    ERROR,
} proxyPopv3State;

typedef struct helloStruct {
    bufferADT          readBuffer;
    bufferADT          writeBuffer;
    helloParser        parser;
} helloStruct;

typedef struct checkCapabilitiesStruct {
    bufferADT          readBuffer;
    capabilities       capabilities;
    capaParser         parser;
} checkCapabilitiesStruct;

typedef struct copyStruct {
    int *           fd;
    bufferADT       readBuffer;
    bufferADT       writeBuffer;
    fdInterest      duplex;
    struct copyStruct *   other;
} copyStruct;

typedef struct transformStruct {
    int             infd[2];
    int             outfd[2];
    pid_t           slavePid;
    bufferADT       readBuffer;
    bufferADT       writeBuffer;
} transformStruct;

typedef struct proxyPopv3 {
    int originFd;
    int clientFd;
    bufferADT readBuffer;
    bufferADT writeBuffer;

    /** estados para el client_fd */
    union {
        helloStruct                hello;
        copyStruct                 copy;
        transformStruct            transform;
    } client;
    /** estados para el origin_fd */
    union {
        helloStruct                hello;
        checkCapabilitiesStruct    checkCapabilities;
        copyStruct                 copy;
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

typedef struct conf {
    int transformation;
} conf;

static capabilities originCapabilities = { 
    .pipeliningStatus = CAPA_NO_CHECKED,
};

static conf proxyConf = { 
    .transformation = 1,
};

static const eventHandler proxyPopv3Handler = {
    .read   = proxyPopv3Read,
    .write  = proxyPopv3Write,
    .block  = proxyPopv3Block,
    .close  = proxyPopv3Close,
};

static void proxyPopv3Read(MultiplexorKey key) {
    stateMachine stm = &ATTACHMENT(key)->stm;
    const proxyPopv3State state = stateMachineHandlerRead(stm, key);

    if(ERROR == state || DONE == state) {
        proxyPopv3Done(key);
    }
}

static void proxyPopv3Write(MultiplexorKey key) {
    stateMachine stm = &ATTACHMENT(key)->stm;
    const proxyPopv3State state = stateMachineHandlerWrite(stm, key);

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
static const unsigned       maxPool  = 50; // tamaño máximo
static unsigned             poolSize = 0;  // tamaño actual
static struct proxyPopv3 *  pool     = 0;  // pool propiamente dicho

static proxyPopv3 * newProxyPopv3(int clientFd, int originFd, size_t bufferSize) {
   
    struct proxyPopv3 * ret;
    if(pool == NULL) {
        ret = malloc(sizeof(*ret));
    } else {
        ret       = pool;
        pool      = pool->next;
        ret->next = 0;
    }
    memset(ret, 0x00, sizeof(*ret));

    ret->clientFd = clientFd;
    ret->originFd = originFd;
    ret->readBuffer = createBuffer(bufferSize);
    ret->writeBuffer = createBuffer(bufferSize);

    ret->stm    .initial   = HELLO_READ;
    ret->stm    .maxState  = ERROR;
    ret->stm    .states    = proxyPopv3DescribeStates();
    stateMachineInit(&ret->stm);

    ret->references = 1;
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
    exit(1);
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
    proxyPopv3  * proxy = ATTACHMENT(key);
    helloStruct * hello = &proxy->origin.hello;

    helloParserInit(&hello->parser);
    hello->readBuffer   = proxy->readBuffer;
    hello->writeBuffer  = proxy->writeBuffer;
}

/** lee todos los bytes del mensaje de tipo `hello' y inicia su proceso */
static unsigned helloRead(MultiplexorKey key) {
    helloStruct * hello = &ATTACHMENT(key)->origin.hello;
    unsigned  ret       = HELLO_READ;
        bool  error     = false;
     uint8_t *writePtr;
      size_t  count;
     ssize_t  n;

    logDebug("Server send something");

    writePtr = getWritePtr(hello->readBuffer, &count);
    n = recv(key->fd, writePtr, count, 0);
    if(n > 0) {
        updateWritePtr(hello->readBuffer, n);
        const helloState state = helloConsume(&hello->parser, hello->readBuffer, hello->writeBuffer, &error);
        if(error)       //EL SERVIDOR RESPONDIO -ERR O NO SABE POPV3 
            ret = ERROR;//PODRIA IR A UN ESTADO PARA INFORMAR AL CLIENTE Q PASO, TENGO LA RESPUESTA CAPAS PARTIDA EN LOS DOS BUFFERS DEBERIA LIMPIARLOS
        else if(helloIsDone(state, 0)) {
            if(originCapabilities.pipeliningStatus == CAPA_NO_CHECKED) {
                if(MUX_SUCCESS == setInterestKey(key, WRITE)) 
                    ret = CHECK_CAPABILITIES;
                else
                    ret = ERROR;
            }
            else if(MUX_SUCCESS == setInterestKey(key, NO_INTEREST) &&
                MUX_SUCCESS == setInterest(key->mux, ATTACHMENT(key)->clientFd, WRITE)) {
                ret = HELLO_WRITE;
            }
            else
                return ERROR; 
        }
    } else {
        ret = ERROR;
    }
    return ret;
}

static void checkCapabilitiesInit(const unsigned state, MultiplexorKey key) {
    checkCapabilitiesStruct * check = &ATTACHMENT(key)->origin.checkCapabilities;

    check->readBuffer               = check->readBuffer;
    capaParserInit(&check->parser, &originCapabilities);
}

static unsigned checkCapabilitiesWrite(MultiplexorKey key) {
    
    const char *capaMsg = "CAPA\r\n";
      unsigned  ret     = CHECK_CAPABILITIES;
        size_t  count   = strlen(capaMsg); 
       ssize_t  n;

    n = send(key->fd, capaMsg, count, MSG_NOSIGNAL);
    if(n == -1) {
        ret = ERROR;
    } else if(MUX_SUCCESS == setInterestKey(key, READ)) {
        ret = CHECK_CAPABILITIES;
    } else {
        ret = ERROR;
    }
    return ret;
}

static unsigned checkCapabilitiesRead(MultiplexorKey key) {
     
    checkCapabilitiesStruct * check = &ATTACHMENT(key)->origin.checkCapabilities;
    unsigned  ret                   = CHECK_CAPABILITIES;
        bool  error                 = false;
     uint8_t *writePtr;
      size_t  count;
     ssize_t  n;

    writePtr = getWritePtr(check->readBuffer, &count);
    n = recv(key->fd, writePtr, count, 0);
    if(n > 0) {   
        updateWritePtr(check->readBuffer, n);
        const capaState state = capaParserConsume(&check->parser, check->readBuffer, &error);
        if(error)       //EL SERVIDOR RESPONDIO -ERR O NO SABE POPV3
            ret = ERROR;//PODRIA PROBAR LOS CAPA A MANO O MANDAR POR DEFAULT QUE NO IMPLEMENTA, CAPAS TENGO LA RESPUESTA EN EL BUFFER DEBERIA LIMPIARLO
        else if(capaParserIsDone(state, 0)) {
            if(MUX_SUCCESS == setInterestKey(key, NO_INTEREST) &&
               MUX_SUCCESS == setInterest(key->mux, ATTACHMENT(key)->clientFd, WRITE)) {    
                logInfo("Capa Pipelining: %s", (originCapabilities.pipeliningStatus == CAPA_AVAILABLE)? "AVAILABLE" : "UNAVAILABLE");
                ret = HELLO_WRITE;
            }
            else
                ret = ERROR;
        }
    } else {
        ret = ERROR;
    }

    return ret;
}

static void helloWriteInit(const unsigned state, MultiplexorKey key) {
    proxyPopv3  * proxy = ATTACHMENT(key);
    helloStruct * hello = &proxy->client.hello;

    hello->readBuffer   = proxy->readBuffer;
    hello->writeBuffer  = proxy->writeBuffer;
}

/** escribe todos los bytes de la respuesta al mensaje `hello' */
static unsigned
helloWrite(MultiplexorKey key) {
    helloStruct * hello = &ATTACHMENT(key)->origin.hello;
    unsigned  ret     = HELLO_WRITE;
      size_t  count;// = strlen(helloMsg); //tamaño del buffer
     ssize_t  n;
     uint8_t * readPtr;
     
    readPtr = getReadPtr(hello->writeBuffer, &count);
    n = send(key->fd, readPtr, count, MSG_NOSIGNAL);
    if(n == -1) 
        ret = ERROR;
    else {
        updateReadPtr(hello->writeBuffer, n);
        if(!canRead(hello->writeBuffer)) {
            if(MUX_SUCCESS == setInterestKey(key, READ)) 
                ret = COPY;
            else 
                ret = ERROR;
        }
    }
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

static fdInterest copyComputeInterests(MultiplexorADT mux, copyStruct * copy) {
    fdInterest ret = NO_INTEREST;
    if ((copy->duplex & READ)  && canWrite(copy->readBuffer))
        ret |= READ;
    if ((copy->duplex & WRITE) && canRead(copy->writeBuffer)) {
        ret |= WRITE;
    }

    if(MUX_SUCCESS != setInterest(mux, *copy->fd, ret))
        fail("Problem trying to set interest: %d,to multiplexor in copy, fd: %d.", ret, *copy->fd);
    
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

    logMetric("Coppied from %s, total copied: %lu bytes.", (*copy->fd == ATTACHMENT(key)->clientFd)? "client to proxy" : "server to proxy", n);
    logDebug("Duplex READ interest: %d.", copy->duplex);
    
    if(*copy->fd == ATTACHMENT(key)->originFd && proxyConf.transformation == 1) {
        ret = TRANSFORM;
        setInterest(key->mux, ATTACHMENT(key)->originFd, NO_INTEREST);
        setInterest(key->mux, ATTACHMENT(key)->clientFd, NO_INTEREST);
    }
    else {
        copyComputeInterests(key->mux, copy);
        copyComputeInterests(key->mux, copy->other);
    }

    if(copy->duplex == NO_INTEREST) {
        ret = DONE;
    }
    return ret;
}

static unsigned copyWrite(MultiplexorKey key) {
    copyStruct * copy = copyPtr(key);
    checkAreEquals(*copy->fd, key->fd, "Must be equals both destinations file descriptors.");

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
    logMetric("Coppied from %s, total copied: %lu bytes.", (*copy->fd == ATTACHMENT(key)->clientFd)? "proxy to client" : "proxy to server", n);
    logDebug("Duplex WRITE interest: %d.", copy->duplex);

    copyComputeInterests(key->mux, copy);
    copyComputeInterests(key->mux, copy->other);
    if(copy->duplex == NO_INTEREST) {
        ret = DONE;
    }
    return ret;
}

static void errorTransformHandler(void * data) {
    MultiplexorKey key          = *((MultiplexorKey *) data);

    stateMachineJump(&ATTACHMENT(key)->stm, COPY, key);
}

static void transformInit(const unsigned state, MultiplexorKey key) {
    proxyPopv3      * proxy     = ATTACHMENT(key);
    transformStruct * transform = &proxy->client.transform;

    transform->slavePid = -1;
    for(int i = 0; i < 2; i++) {
        transform->infd[i] = -1;
        transform->outfd[i] = -1;
    }

    transform->readBuffer   = createBackUpBuffer(proxy->writeBuffer);
    transform->writeBuffer  = createBackUpBuffer(proxy->readBuffer);

    checkFailWithFinally(pipe(transform->infd), errorTransformHandler, &key, "Transform fail: cannot open a pipe.");
    checkFailWithFinally(pipe(transform->outfd), errorTransformHandler, &key, "Transform fail: cannot open a pipe.");

    pid_t pid = fork();
    checkFailWithFinally(pid, errorTransformHandler, &key, "Transform fail: cannot fork.");
    if(pid == 0) {
        close(transform->infd[1]);
        close(transform->outfd[0]);
        dup2(transform->infd[0], STDIN_FILENO);
        close(transform->infd[0]);
        dup2(transform->outfd[1], STDOUT_FILENO);
        close(transform->outfd[1]);
        checkFailWithFinally(system("cat"), errorTransformHandler, &key, "Transform fail: cannot execl.");
    } else {
        transform->slavePid = pid;
        close(transform->infd[0]);
        close(transform->outfd[1]);
        transform->infd[0] = -1;
        transform->outfd[1] = -1;
        multiplexorStatus status = registerFd(key->mux, transform->infd[1], &proxyPopv3Handler, WRITE, proxy);
        checkAreEqualsWithFinally(status, MUX_SUCCESS, errorTransformHandler, &key, "Transform fail: cannot register IN pipe in multiplexor.");
    }
}

static unsigned transformRead(MultiplexorKey key) {
    transformStruct * transform = &ATTACHMENT(key)->client.transform;

    size_t size;
    ssize_t n;
    bufferADT buffer = transform->readBuffer;
    unsigned ret = TRANSFORM;

    uint8_t *ptr = getWritePtr(buffer, &size);
    n = read(transform->outfd[0], ptr, size);
    if(n >= 0) {
        ret = COPY;
        setInterest(key->mux, transform->outfd[0], NO_INTEREST);
        updateWritePtr(buffer, n);    
        logMetric("Coppied from transform app to proxy, total copied: %lu bytes.", n);
    }
    else   
        ret = ERROR;
    return ret;
}

static unsigned transformWrite(MultiplexorKey key) {
    transformStruct * transform = &ATTACHMENT(key)->client.transform;

    size_t size;
    ssize_t n;
    bufferADT buffer = transform->readBuffer;
    unsigned ret = TRANSFORM;

    uint8_t *ptr = getReadPtr(buffer, &size);
    n = write(transform->infd[1], ptr, size);   //EXPLOTAR, SI MI BUFFER ES MUY GRANDE POSIBLE DEADLOCK, LA APP TAMBIEN HACE WRITE EN EL OTRO PIPE
    checkFailWithFinally(n, errorTransformHandler, key, "Transform fail: unnable to write in pipe");
    
    multiplexorStatus status = registerFd(key->mux, transform->outfd[0], &proxyPopv3Handler, READ, ATTACHMENT(key));
    checkAreEqualsWithFinally(status, MUX_SUCCESS, errorTransformHandler, &key, "Transform fail: cannot register OUT pipe in multiplexor.");

    logMetric("Coppied from proxy to transform app, total copied: %lu bytes.", n);
    
    updateReadPtr(buffer, n);
    setInterest(key->mux, transform->infd[1], NO_INTEREST);
    return ret;
}

static void transformClose(const unsigned state, MultiplexorKey key) {
    proxyPopv3      * proxy     = ATTACHMENT(key);
    transformStruct * transform = &proxy->client.transform;

    if(transform->slavePid > 0) 
        kill(transform->slavePid, SIGKILL);
    if(transform->slavePid == -1)
        exit(1);

    for(int i = 0; i < 2; i++) {
        if(transform->infd[i] >= 0) {
            unregisterFd(key->mux, transform->infd[i]);
            close(transform->infd[i]);
        }
        if(transform->outfd[i] >= 0) {
            unregisterFd(key->mux, transform->outfd[i]);
            close(transform->outfd[i]);
        }
    }
    deleteBuffer(proxy->readBuffer);
    proxy->readBuffer = transform->writeBuffer;
    deleteBuffer(transform->readBuffer);
    
    logDebug("Transform done");
    setInterest(key->mux, ATTACHMENT(key)->clientFd, WRITE);
}


/* definición de handlers para cada estado */
static const struct stateDefinition clientStatbl[] = {
    {
        .state            = HELLO_READ,
        .onArrival        = helloReadInit,
        .onReadReady      = helloRead,
    }, {
        .state            = CHECK_CAPABILITIES,
        .onArrival        = checkCapabilitiesInit,
        .onReadReady      = checkCapabilitiesRead,
        .onWriteReady     = checkCapabilitiesWrite,
    }, {
        .state            = HELLO_WRITE,
        .onArrival        = helloWriteInit,
        .onWriteReady     = helloWrite,
    },{
        .state            = COPY,
        .onArrival        = copyInit,
        .onReadReady      = copyReadAndQueue,
        .onWriteReady     = copyWrite,
    }, {
        .state            = TRANSFORM,
        .onArrival        = transformInit,
        .onDeparture      = transformClose,
        .onReadReady      = transformRead,
        .onWriteReady     = transformWrite,
    }, {
        .state            = DONE,

    },{
        .state            = ERROR,
    }
};

static const struct stateDefinition * proxyPopv3DescribeStates(void) {
    return clientStatbl;
}

