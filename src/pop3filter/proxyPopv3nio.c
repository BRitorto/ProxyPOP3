/**
 * proxyPopv3nio.c  - controla el flujo de un proxy POPv3 (sockets no bloqueantes)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <netdb.h>
#include <time.h>
#include <fcntl.h>

#include "proxyPopv3nio.h"
#include "queue.h"
#include "buffer.h"
#include "logger.h"
#include "errorslib.h"
#include "multiplexor.h"
#include "stateMachine.h"
#include "helloParser.h"
#include "capaParser.h"
#include "commandParser.h"
#include "responseParser.h"
#include "netutils.h"

/**
 * Estados para la máquina de estados.
 */
typedef enum proxyPopv3State {
    CONNECTION_RESOLV,
    CONNECTING,
    HELLO,
    CHECK_CAPABILITIES,
    COPY,
    SEND_ERROR_MSG,
    DONE,
    ERROR,
} proxyPopv3State;


/** Tamaño maximo que ocupa convertir una ip en un string. */
#define MAX_STRING_IP_LENGTH 50
/** Tamaño maximo de un argumento en POP3.                 */
#define MAX_ARGS_LENGTH 40

#define SLAVE_BUFFER_SIZE 1

/**
 * Estructura de una sesión, guarda el nombre de usuario logeado en la sesion
 * un bool para saber si hay un usuario logeado las representaciones en string
 * de las direcciones utilizadas y la úĺtima vez que interaccionó la sesión.
 */
typedef struct sessionStruct {
    char                name[MAX_ARGS_LENGTH + 1];
    bool                isAuth;
    char                originString[MAX_STRING_IP_LENGTH];
    char                clientString[MAX_STRING_IP_LENGTH];
    time_t              lastUse;
} sessionStruct;

/**
 * Estructura que almacena lo necesario para el parseo del saludo enviado
 * por el origin server al iniciar la conexión.
 */
typedef struct helloStruct {
    bufferADT           writeBuffer;
    helloParser         parser;
} helloStruct;

/** 
 * Estructura que almacena lo necesario para el parseo de capacidades
 * soportadas por el servidor origen (Ej: PIPELINING).
 */
typedef struct checkCapabilitiesStruct {
    bufferADT           readBuffer;
    ssize_t             sentSize;
    capabilities *      capabilities;
    capaParser          parser;
} checkCapabilitiesStruct;

/**
 * Posibles objetivos para el estado COPY.
 */
typedef enum copyTarget {
    COPY_CLIENT,
    COPY_ORIGIN,
    COPY_FILTER,
} copyTarget;

/**
 * Posibles estados copy.
 */
typedef enum copyState {
    ALL_GOOD,
    CLIENT_READ_DOWN,
    ORIGIN_WRITE_DOWN,
    ORIGIN_READ_DOWN,
    CLIENT_WRITE_DOWN,
} copyState;

/**
 * Estructura que almacena lo necesario para llevar acabo la copia de datos.
 */
typedef struct copyStruct {
    int *               fd;
    bufferADT           readBuffer;
    bufferADT           writeBuffer;
    fdInterest          duplex;
    copyTarget          target;
    copyState *         state;
    struct copyStruct * other;
} copyStruct;

/**
 * Posibles estados del filtro.
 */
typedef enum filterState {
    FILTER_CLOSE,
    FILTER_ENDING,
    FILTER_ALL_SENT,
    FILTER_STARTING,
    FILTER_FILTERING,
} filterState;

/**
 * Estructura que contiene lo necesario para llevar a cabo el filtro.
 */
typedef struct filterDataStruct {
    int                 infd[2];
    int                 outfd[2];
    pid_t               slavePid;
    filterState         state;
} filterDataStruct;

/**
 * Estructura con lo necesario para parsear commands enviados por
 * un cliente.
 */
typedef struct requestStruct {
    queueADT            commands; 
    bool                waitingResponse;
} requestStruct;

/**
 * Estructura con lo necesario para enviar errores al cliente.
 */
typedef struct errorContainer {
    char * message;
    size_t messageLength;
    size_t sendedSize;
} errorContainer;

/**
 * Estructura con lo necesario para el atender una conexión con el 
 * servidor proxy.
 */
typedef struct proxyPopv3 {
    int originFd;
    int clientFd;
    sessionStruct session;
    
    /** Informacion que puede persistir a través de los estados. */
    bufferADT                      readBuffer;
    bufferADT                      writeBuffer;
    bufferADT                      filterBuffer;

    copyState                      copyState;
    filterDataStruct               filterData;
    requestStruct                  request;

    commandParser                  commandParser;
    responseParser                 responseParser;

    capabilities                   originCapabilities;
    errorContainer                 errorSender;

    /** Estados para el clientFd. */
    union {    
        helloStruct                hello;
        copyStruct                 copy;
    } client;

    /** Estados para el originFd. */
    union {
        helloStruct                hello;
        checkCapabilitiesStruct    checkCapabilities;
        copyStruct                 copy;
    } origin;

    /** Estados para el filter. */
    union {
        copyStruct                 copy;
    } filter;

    addressData                    originAddrData;
    /** Resolución de la dirección del origin server. */
    struct addrinfo               *originResolution;
    /** Intento actual de la dirección del origin server. */
    struct addrinfo               *originResolutionCurrent;

    /** Maquinas de estados. */
    struct stateMachineCDT stm;

    /** Cantidad de referencias a este objeto. si es uno se debe destruir. */
    unsigned references;
    /** Siguiente en el pool. */
    struct proxyPopv3 * next;
} proxyPopv3;

/**
 * Pool de estructuras proxyPopv3, para ser reusados.
 *
 * Como tenemos un unico hilo que emite eventos no necesitamos barreras de
 * contención.
 */
static const unsigned       maxPool  = 50; // Tamaño máximo.
static unsigned             poolSize = 0;  // Tamaño actual.
static struct proxyPopv3 *  pool     = 0;  // Pool propiamente dicho.

static const struct stateDefinition * proxyPopv3DescribeStates(void);

/** 
 * Crea un nuevo `proxyPopv3' 
 */
static proxyPopv3 * newProxyPopv3(int clientFd, addressData originAddrData, size_t bufferSize) {
   
    struct proxyPopv3 * ret;
    bufferADT readBuffer, writeBuffer, filterBuffer;
    queueADT commands;

    if(pool == NULL) {
        ret = malloc(sizeof(*ret));
        readBuffer          = createBuffer(bufferSize);
        /** Para poder leer '.\r\n' antes de mandar al filter. */
        writeBuffer         = createBuffer((bufferSize > 2)? bufferSize : 3); 
        filterBuffer        = createBuffer(bufferSize);      
        commands            = createQueue();
    } else {
        ret                 = pool;
        pool                = pool->next;
        ret->next           = 0;
        readBuffer          = ret->readBuffer;
        writeBuffer         = ret->writeBuffer;
        filterBuffer        = ret->filterBuffer;
        commands            = ret->request.commands;
        reset(readBuffer);
        reset(writeBuffer);
        reset(filterBuffer);
    }
    memset(ret, 0x00, sizeof(*ret));

    ret->clientFd           = clientFd;
    ret->originFd           = -1;
    ret->readBuffer         = readBuffer;
    ret->writeBuffer        = writeBuffer;
    ret->filterBuffer       = filterBuffer;
    ret->request.commands   = commands;

    commandParserInit(&ret->commandParser);
    responseParserInit(&ret->responseParser);

    ret->session.lastUse        = time(NULL);
    ret->originAddrData         = originAddrData;

    ret->stm.initial            = CONNECTION_RESOLV;
    ret->stm.maxState           = ERROR;
    ret->stm.states             = proxyPopv3DescribeStates();
    stateMachineInit(&ret->stm);

    ret->references = 1;
    return ret;
}

/**
 *  Destruye y libera un proxyPopv3
 */
static void realDeleteProxyPopv3(proxyPopv3 * proxy) {
    deleteBuffer(proxy->readBuffer);
    deleteBuffer(proxy->writeBuffer);
    deleteBuffer(proxy->filterBuffer);
    
    commandStruct * current;
    while((current = peekProcessed(proxy->request.commands)) != NULL)
        processQueue(proxy->request.commands);
    current = poll(proxy->request.commands);
    for(;current != NULL; current = poll(proxy->request.commands))
        deleteCommand(current);
    deleteQueue(proxy->request.commands);

    if(proxy->originResolution != NULL) {
        freeaddrinfo(proxy->originResolution);
        proxy->originResolution = 0;
    }
    free(proxy);
}

/**
 *  Destruye un  proxyPopv3, tiene en cuenta las referencias
 *  y el pool de objetos.
 */
static void deleteProxyPopv3(proxyPopv3 * proxy) {
    if(proxy != NULL) {
        if(proxy->references == 1) {
            if(poolSize < maxPool) {
                proxy->next = pool;
                pool        = proxy;
                poolSize++;
                commandStruct * current;
                while((current = peekProcessed(proxy->request.commands)) != NULL)
                    processQueue(proxy->request.commands);
                current = poll(proxy->request.commands);
                for(;current != NULL; current = poll(proxy->request.commands))
                    deleteCommand(current);
            
            } else {
                realDeleteProxyPopv3(proxy);
            }
        } else {
            proxy->references -= 1;
        }
    }
}

void poolProxyPopv3Destroy(void) {
    proxyPopv3 * next, * current;
    for(current = pool; current != NULL ; current = next) {
        next = current->next;
        realDeleteProxyPopv3(current);
    }
}

/** Obtiene el proxyPopv3* desde la llave de selección.  */
#define ATTACHMENT(key) ( (struct proxyPopv3 *)(key)->data)

#define N(x) (sizeof(x)/sizeof((x)[0]))

/* 
 * Declaración forward de los handlers de selección de una conexión
 * establecida entre un cliente y el proxy.
 */
static void proxyPopv3Read(MultiplexorKey key);
static void proxyPopv3Write(MultiplexorKey key);
static void proxyPopv3Block(MultiplexorKey key);
static void proxyPopv3Close(MultiplexorKey key);
static void proxyPopv3Timeout(MultiplexorKey key);
static void proxyPopv3Done(MultiplexorKey key);

/**
 * Manejador de eventos para proxyPopv3.
 */
static const eventHandler proxyPopv3Handler = {
    .read    = proxyPopv3Read,
    .write   = proxyPopv3Write,
    .block   = proxyPopv3Block,
    .close   = proxyPopv3Close,
    .timeout = proxyPopv3Timeout
};

/* 
 * Declaración forward de los handlers de selección de una conexión
 * establecida entre un cliente y el proxy.
 */
static void * resolvBlocking(void * data);
static unsigned connecting(MultiplexorADT mux, proxyPopv3  * proxy);

/**
 * Intenta aceptar la nueva conexión entrante.
 */
void proxyPopv3PassiveAccept(MultiplexorKey key) {

    struct sockaddr_storage       clientAddr;
    socklen_t                     clientAddrSize = sizeof(clientAddr);
    addressData *                 originAddrData = (addressData *) key->data;
    proxyPopv3 *                  proxy          = NULL;
    pthread_t                     tid;
    
    proxyMetrics.activeConnections++;
    proxyMetrics.totalConnections++;

    const int clientFd = accept(key->fd, (struct sockaddr*) &clientAddr, &clientAddrSize);
    if(clientFd == -1) {
        goto fail;
    }
    if(fdSetNIO(clientFd) == -1) {
        goto fail;
    }
    proxy = newProxyPopv3(clientFd, *originAddrData, proxyConf.bufferSize);

    if(proxy == NULL) {
        /** 
         * Sin un estado, nos es imposible manejaro.
         * Tal vez deberiamos apagar accept() hasta que detectemos
         * que se liberó alguna conexión.
         */
        goto fail;
    }

    const struct sockaddr * client = (const struct sockaddr *) &clientAddr;
    sockaddrToString(proxy->session.clientString, MAX_STRING_IP_LENGTH, client);

    logInfo("Accepting new client with address %s.", proxy->session.clientString);

    if(MUX_SUCCESS != registerFd(key->mux, clientFd, &proxyPopv3Handler, NO_INTEREST, proxy)) {
        goto fail;
    }

    if(originAddrData->type != ADDR_DOMAIN) 
        proxy->stm.initial = connecting(key->mux, proxy);
    else {
        logInfo("Need to resolv the domain name: %s.", originAddrData->addr.fqdn);
        MultiplexorKey blockingKey = malloc(sizeof(*blockingKey));
        if(blockingKey == NULL)
            goto fail2;

        blockingKey->mux  = key->mux;
        blockingKey->fd   = clientFd;
        blockingKey->data = proxy;
        if(-1 == pthread_create(&tid, 0, resolvBlocking, blockingKey)) {            
            logError("Unable to create a new thread. Client Address: %s", proxy->session.clientString);
            
            proxy->errorSender.message = "-ERR Unable to connect.\r\n";
            if(MUX_SUCCESS != setInterest(key->mux, proxy->clientFd, WRITE))
                goto fail2;
            proxy->stm.initial = SEND_ERROR_MSG;
        }
    }
    return;

fail2:
    unregisterFd(key->mux, clientFd);
fail:    
    logError("Proxy passive accept fail. Client Address: %s", proxy->session.clientString);
    proxyMetrics.activeConnections--;
    if(clientFd != -1) {
        close(clientFd);
    }
    deleteProxyPopv3(proxy);
}

////////////////////////////////////////////////////////////////////////////////
// CONNECTION_RESOLV
////////////////////////////////////////////////////////////////////////////////

/**
 * Realiza la resolución de DNS bloqueante.
 *
 * Una vez resuelto notifica al selector para que el evento esté
 * disponible en la próxima iteración.
 */
static void * resolvBlocking(void * data) {
    MultiplexorKey key = (MultiplexorKey) data;
    proxyPopv3 * proxy = ATTACHMENT(key);

    pthread_detach(pthread_self());
    proxy->originResolution = 0;
    struct addrinfo hints = {
        .ai_family    = AF_UNSPEC,    
        /** Permite IPv4 o IPv6. */
        .ai_socktype  = SOCK_STREAM,  
        .ai_flags     = AI_PASSIVE,   
        .ai_protocol  = 0,        
        .ai_canonname = NULL,
        .ai_addr      = NULL,
        .ai_next      = NULL,
    };

    char buff[7];
    snprintf(buff, sizeof(buff), "%d", proxy->originAddrData.port);
    getaddrinfo(proxy->originAddrData.addr.fqdn, buff, &hints, &proxy->originResolution);
    notifyBlock(key->mux, key->fd);

    free(data);
    return 0;
}

/**
 * Procesa el resultado de la resolución de nombres. 
 */
static unsigned resolvDone(MultiplexorKey key) {
    proxyPopv3 * proxy = ATTACHMENT(key);
    if(proxy->originResolution != 0) {
        proxy->originAddrData.domain = proxy->originResolution->ai_family;
        proxy->originAddrData.addrLength = proxy->originResolution->ai_addrlen;
        memcpy(&proxy->originAddrData.addr.addrStorage,
                proxy->originResolution->ai_addr,
                proxy->originResolution->ai_addrlen);
        freeaddrinfo(proxy->originResolution);
        proxy->originResolution = 0;
    } else {
        proxy->errorSender.message = "-ERR Connection refused.\r\n";
        if(MUX_SUCCESS != setInterest(key->mux, proxy->clientFd, WRITE))
            return ERROR;
        return SEND_ERROR_MSG;
    }

    return connecting(key->mux, proxy);
}

/** 
 * Intenta establecer una conexión con el origin server. 
 */
static unsigned connecting(MultiplexorADT mux, proxyPopv3  * proxy) {
    addressData originAddrData = proxy->originAddrData;
    
    proxy->originFd = socket(originAddrData.domain, SOCK_STREAM, IPPROTO_TCP);

    if(proxy->originFd == -1)
        goto finally;
    if(fdSetNIO(proxy->originFd) == -1)
        goto finally;

    if(connect(proxy->originFd, (const struct sockaddr *) &originAddrData.addr.addrStorage, originAddrData.addrLength) == -1) {
        if(errno == EINPROGRESS) {
            /**
             * Es esperable,  tenemos que esperar a la conexión.
             * Dejamos de pollear el socket del cliente.
             */
            multiplexorStatus status = setInterest(mux, proxy->clientFd, NO_INTEREST);
            if(status != MUX_SUCCESS) 
                goto finally;

            /** Esperamos la conexion en el nuevo socket. */
            status = registerFd(mux, proxy->originFd, &proxyPopv3Handler, WRITE, proxy);
            if(status != MUX_SUCCESS) 
                goto finally;
        
            proxy->references += 1;
        }
    } else {
        /**
         * Estamos conectados sin esperar... no parece posible
         * Saltaríamos directamente a COPY.
         */
        logError("Problem: connected to origin server without wait. Client Address: %s", proxy->session.clientString);
    }
    
    return CONNECTING;

finally:    
    logError("Problem connecting to origin server. Client Address: %s", proxy->session.clientString);
    proxy->errorSender.message = "-ERR Connection refused.\r\n";
    if(MUX_SUCCESS != setInterest(mux, proxy->clientFd, WRITE))
        return ERROR;
    return SEND_ERROR_MSG;
}


////////////////////////////////////////////////////////////////////////////////
// CONNECTING
////////////////////////////////////////////////////////////////////////////////

static unsigned connectionReady(MultiplexorKey key) {    
    proxyPopv3 * proxy = ATTACHMENT(key);
    int error;
    socklen_t len = sizeof(error);
    unsigned  ret = ERROR;
    
    if (getsockopt(key->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
        error = 1;

    if(error != 0) {
        logError("Problem connecting to origin server. Client Address: %s", proxy->session.clientString);
        proxy->errorSender.message = "-ERR Connection refused.\r\n";
        if(MUX_SUCCESS == setInterest(key->mux, proxy->clientFd, WRITE))
            ret = SEND_ERROR_MSG;
        else
            ret = ERROR;
    } else if(MUX_SUCCESS == setInterestKey(key, READ)) {
        const struct sockaddr * origin = (const struct sockaddr *) &proxy->originAddrData.addr.addrStorage;
        sockaddrToString(proxy->session.originString, MAX_STRING_IP_LENGTH, origin);
        logInfo("Connection established. Client Address: %s; Origin Address: %s.", proxy->session.clientString, proxy->session.originString);
        ret = HELLO;
    }
    return ret;
}


////////////////////////////////////////////////////////////////////////////////
// HELLO
////////////////////////////////////////////////////////////////////////////////

static void helloReadInit(const unsigned state, MultiplexorKey key) {
    proxyPopv3  * proxy = ATTACHMENT(key);
    helloStruct * hello = &proxy->origin.hello;

    helloParserInit(&hello->parser);
    hello->writeBuffer   = proxy->writeBuffer;
}

/** 
 * Lee todos los bytes del mensaje de saludo enviado por el servidor origen.
 */
static unsigned helloRead(MultiplexorKey key) {    
    proxyPopv3  * proxy  = ATTACHMENT(key);
    helloStruct * hello  = &proxy->origin.hello;
     unsigned  ret       = HELLO;
    bufferADT  buffer    = hello->writeBuffer;
         bool  error     = false;
      uint8_t *writePtr;
       size_t  count;
      ssize_t  n;

    writePtr = getWritePtr(buffer, &count);
    n = recv(key->fd, writePtr, count, 0);
    if(n > 0) {
        proxyMetrics.bytesWriteBuffer += n;
        proxyMetrics.writesQtyWriteBuffer++;
        updateWritePtr(buffer, n);
        helloConsume(&hello->parser, buffer, &error);
        if(!error && MUX_SUCCESS == setInterest(key->mux, ATTACHMENT(key)->originFd, NO_INTEREST) &&
             MUX_SUCCESS == setInterest(key->mux, ATTACHMENT(key)->clientFd, WRITE)) {
            ret = HELLO;
        } else
            error = true;
    } else {
        shutdown(key->fd, SHUT_RD);
        error = true;
    }

    if(error) {
        logError("Initial hello has an error. Client Address: %s", proxy->session.clientString);
        proxy->errorSender.message = "-ERR\r\n";
        if(MUX_SUCCESS == setInterest(key->mux, proxy->clientFd, WRITE))
            ret = SEND_ERROR_MSG;
        else
            ret = ERROR;
    }
    return ret;
}

/** 
 * Escribe todos los bytes deL saludo al cliente. 
 */
static unsigned helloWrite(MultiplexorKey key) {
    helloStruct * hello = &ATTACHMENT(key)->origin.hello;
     unsigned  ret     = HELLO;
    bufferADT  buffer    = hello->writeBuffer;
       size_t  count; /** Tamaño del buffer. */
      ssize_t  n;
      uint8_t * readPtr;
     
    readPtr = getReadPtr(buffer, &count);
    n = send(key->fd, readPtr, count, MSG_NOSIGNAL);
    if(n == -1) {
        shutdown(key->fd, SHUT_WR);
        ret = ERROR;
    }
    else {
        updateReadPtr(buffer, n);
        proxyMetrics.totalBytesToClient += n;
        proxyMetrics.readsQtyWriteBuffer++;
        if(helloIsDone(hello->parser.state, 0)) {
            logDebug("Hello is done.");
            if(MUX_SUCCESS == setInterest(key->mux, ATTACHMENT(key)->originFd, WRITE) &&
               MUX_SUCCESS == setInterestKey(key, NO_INTEREST))
                ret = CHECK_CAPABILITIES;
            else
                ret = ERROR; 
        } else if(!canRead(buffer) &&
                MUX_SUCCESS == setInterest(key->mux, ATTACHMENT(key)->clientFd, NO_INTEREST) &&
                MUX_SUCCESS == setInterest(key->mux, ATTACHMENT(key)->originFd, READ))
            ret = HELLO;
        else 
            ret = ERROR;
    }
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
// CHECK_CAPABILITIES
////////////////////////////////////////////////////////////////////////////////

static void checkCapabilitiesInit(const unsigned state, MultiplexorKey key) {
    checkCapabilitiesStruct * check = &ATTACHMENT(key)->origin.checkCapabilities;

    check->sentSize                 = 0;
    check->readBuffer               = check->readBuffer;
    check->capabilities             = &ATTACHMENT(key)->originCapabilities;
    capaParserInit(&check->parser, check->capabilities);
}

static const char    *capaMsg   = "CAPA\n";
static const size_t capaMsgSize = 5;
/**
 * Escribe el comando 'CAPA' al origin.. 
 */
static unsigned checkCapabilitiesWrite(MultiplexorKey key) {
    checkCapabilitiesStruct * check = &ATTACHMENT(key)->origin.checkCapabilities;
    
    unsigned  ret   = CHECK_CAPABILITIES;
     uint8_t *ptr   = (uint8_t *) capaMsg + check->sentSize;
      size_t  count = capaMsgSize - check->sentSize; 
     ssize_t  n;

    n = send(key->fd, ptr, count, MSG_NOSIGNAL);
    if(n > 0) {       //CHECKEAR QUE TODO LO Q QUERIAMOS ESCRIBIR FUE ESCRITO
        check->sentSize += n;
        proxyMetrics.totalBytesToOrigin += n;
        if(check->sentSize == capaMsgSize) {
            if(MUX_SUCCESS == setInterestKey(key, READ)) 
                ret = CHECK_CAPABILITIES;
            else
                ret = ERROR;
        }
    } else {
        shutdown(key->fd, SHUT_WR);
        ret = ERROR;
    }
    return ret;
}

/**
 * Lee y parsea la respuesta al comando 'CAPA' para saber si el
 * origin soporta pipelining.
 */
static unsigned checkCapabilitiesRead(MultiplexorKey key) {
    proxyPopv3  * proxy = ATTACHMENT(key);
    checkCapabilitiesStruct * check = &proxy->origin.checkCapabilities;
     unsigned  ret                   = CHECK_CAPABILITIES;
         bool  error                 = false;
    bufferADT  buffer                = check->readBuffer;
      uint8_t *writePtr;
       size_t  count;
      ssize_t  n;

    writePtr = getWritePtr(buffer, &count);
    n = recv(key->fd, writePtr, count, 0);
    if(n > 0) {
        proxyMetrics.bytesReadBuffer += n;
        proxyMetrics.writesQtyReadBuffer++;
        updateWriteAndProcessPtr(buffer, n);
        const capaState state = capaParserConsume(&check->parser, buffer, &error);
        if(error) {
            logError("Capa response has an error. Client Address: %s", proxy->session.clientString);
            proxy->errorSender.message = "-ERR Unexpected error.\r\n";
            if(MUX_SUCCESS == setInterest(key->mux, proxy->clientFd, WRITE))
                ret = SEND_ERROR_MSG;
            else
                ret = ERROR;  
        } else if(capaParserIsDone(state, 0)) {
            if(MUX_SUCCESS == setInterestKey(key, READ) &&
               MUX_SUCCESS == setInterest(key->mux, proxy->clientFd, READ)) {    
                logInfo("Capa Pipelining: %s", (check->capabilities->pipelining)? "AVAILABLE" : "UNAVAILABLE");
                ret = COPY;
            } else
                ret = ERROR;
        }
    } else {
        shutdown(key->fd, SHUT_RD);
        ret = ERROR;
    }
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
// COPY
////////////////////////////////////////////////////////////////////////////////

static void copyInit(const unsigned state, MultiplexorKey key) {
    struct proxyPopv3 * proxy = ATTACHMENT(key);

    copyStruct * copy  = &(proxy->client.copy);
    copy->fd           = &(proxy->clientFd);
    copy->readBuffer   = proxy->readBuffer;
    copy->writeBuffer  = proxy->writeBuffer;
    copy->duplex       = READ | WRITE;
    copy->other        = &(proxy->origin.copy);
    copy->state        = &proxy->copyState;
    copy->target       = COPY_CLIENT;

    copy               = &(proxy->origin.copy);
    copy->fd           = &(proxy->originFd);
    copy->readBuffer   = proxy->writeBuffer;
    copy->writeBuffer  = proxy->readBuffer;
    copy->duplex       = READ | WRITE;
    copy->other        = &(proxy->client.copy);
    copy->state        = &proxy->copyState;
    copy->target       = COPY_ORIGIN;

    copy               = &(proxy->filter.copy);
    copy->readBuffer   = proxy->filterBuffer;
    copy->writeBuffer  = proxy->writeBuffer;
    copy->duplex       = READ | WRITE;
    copy->target       = COPY_FILTER;
    copy->state        = &proxy->copyState;
}

/**
 *
 */
static inline fdInterest clientComputeInterests(MultiplexorADT mux, copyStruct * copy, copyStruct * copyFilter, filterState state) {
    fdInterest ret = NO_INTEREST;
    const bool wantWriteFromOrigin = canRead(copy->writeBuffer) && (state == FILTER_STARTING || state == FILTER_CLOSE);
    const bool wantWriteFromFilter = canRead(copyFilter->readBuffer) && (state == FILTER_FILTERING || state == FILTER_ALL_SENT);

    if ((copy->duplex & READ)  &&  canWrite(copy->readBuffer))
        ret |= READ;
    if ((copy->duplex & WRITE) && (wantWriteFromOrigin || wantWriteFromFilter)) 
        ret |= WRITE;
    
    logInfo("Seteando interes en client fd: %d, interes: %d", *copy->fd, ret);
    if(MUX_SUCCESS != setInterest(mux, *copy->fd, ret))
        fail("Problem trying to set interest: %d,to multiplexor in copy, fd: %d.", ret, *copy->fd);
    
    return ret;
}

/**
 *
 */
static inline fdInterest originComputeInterests(MultiplexorADT mux, copyStruct * copy, bool read, bool write) {
    fdInterest ret = NO_INTEREST;
    if (read  && (copy->duplex & READ)  &&  canWrite(copy->readBuffer))
        ret |= READ;
    if (write && (copy->duplex & WRITE) && (canRead(copy->writeBuffer) || canProcess(copy->writeBuffer)))
        ret |= WRITE;
    
    logInfo("Seteando interes en origin fd: %d, interes: %d", *copy->fd, ret);
    if(MUX_SUCCESS != setInterest(mux, *copy->fd, ret))
        fail("Problem trying to set interest: %d,to multiplexor in copy, fd: %d.", ret, *copy->fd);
    
    return ret;
}

/**
 *
 */
static inline fdInterest filterComputeInterest(MultiplexorADT mux, copyStruct * copy, filterDataStruct * filterData) {
    fdInterest retWrite = NO_INTEREST, retRead = NO_INTEREST;

    if(filterData->state == FILTER_FILTERING) {
        if(canRead(copy->writeBuffer) || canProcess(copy->writeBuffer)) //para saber si vino el .\r\n
            retWrite = WRITE;    
        if(MUX_SUCCESS != setInterest(mux, filterData->infd[1], retWrite))
            fail("Problem trying to set interest: %d, to multiplexor in filter, in pipe.", retWrite);       
    }
    if(canWrite(copy->readBuffer)) 
        retRead = READ;        
        
    if(MUX_SUCCESS != setInterest(mux, filterData->outfd[0], retRead))
        fail("Problem trying to set interest: %d, to multiplexor in filter, out pipe.", retRead);
    
    logInfo("Seteando interes en filter: WRITE: %d y READ %d.", retWrite, retRead);
    
    return retRead | retWrite;
}

/**
 *
 */
static void analizeResponse(proxyPopv3 * proxy, queueADT commands, bool * newResponse) {
    char * username;
    size_t usernameLength;
    
    while(isProcessedReadyQueue(commands)) {
        commandStruct * current = poll(commands);
        *newResponse = true;
        if(!proxy->session.isAuth) {
            if(current->type == CMD_USER && current->indicator) {
                username = getUsername(*current);
                usernameLength = strlen(username) + 1;  //checkear size mayor 40
                memcpy(proxy->session.name, username, usernameLength);
            } else if(current->type == CMD_PASS && current->indicator) {
                logDebug("Logged user: %s", proxy->session.name);
                proxy->session.isAuth = true;
            } else if(current->type == CMD_APOP && current->indicator) {
                username = getUsername(*current);
                usernameLength = strlen(username) + 1;  //checkear size mayor 40
                memcpy(proxy->session.name, username, usernameLength);
                proxy->session.isAuth = true;
            }
        }
        deleteCommand(current);
    }
}

/**
 *
 */
static unsigned analizeAndProcessResponse(proxyPopv3 * proxy, bufferADT buffer, bool interestRetr, bool toNewCommand) {
    unsigned ret = COPY;
    bool errored = false, newResponse = false;
    const responseState state = responseParserConsumeUntil(&proxy->responseParser, buffer, proxy->request.commands, interestRetr, toNewCommand, &errored); 
    if(errored) {
        proxy->errorSender.message = "-ERR Unexpected event\r\n";
        ret = SEND_ERROR_MSG;
    }
    else if(interestRetr && state == RESPONSE_INTEREST)
        proxy->filterData.state = FILTER_STARTING;

    analizeResponse(proxy, proxy->request.commands, &newResponse);
    if(newResponse)
        proxy->request.waitingResponse = false;

    return ret;        
}

static void filterInit(MultiplexorKey key);
static void filterClose(MultiplexorKey key);

/**
 * Computa los intereses en base a:
 *  - La disponiblidad de los buffer.
 *  - El estado de la etapa de filtro en caso de que se este filtrando.
 *  - El estado de la respuesta en caso de que el origin no soporte pipelining.
 * La variable duplex nos permite saber si alguna vía ya fue cerrada.
 * Arrancá OP_READ | OP_WRITE.
 */
static void computeInterestsCopy(MultiplexorKey key) {
    proxyPopv3 * proxy = ATTACHMENT(key);
    const bool originWantWrite = (proxy->originCapabilities.pipelining || !proxy->request.waitingResponse);

    if(proxyConf.filterActivated) {
        switch(proxy->filterData.state) {
            case FILTER_STARTING:
                if(proxy->filterData.slavePid == 0)
                    filterInit(key);
                if(!canRead(proxy->writeBuffer) && canProcess(proxy->writeBuffer)) {
                    proxy->filterData.state = FILTER_FILTERING;
                    proxyMetrics.commandsFilteredQty++;
                    filterComputeInterest(key->mux, &proxy->filter.copy, &proxy->filterData);
                }
                break;

            case FILTER_ALL_SENT:
                if(proxy->filterData.infd[1] != -1) {
                    unregisterFd(key->mux, proxy->filterData.infd[1]);
                    close(proxy->filterData.infd[1]);
                    proxy->filterData.infd[1] = -1;
                }
            case FILTER_FILTERING:
                filterComputeInterest(key->mux, &proxy->filter.copy, &proxy->filterData);
                break;

            case FILTER_ENDING:
                filterClose(key);
                proxy->filterData.state = FILTER_CLOSE;                
                break;
            default:
                break;
        }
    }
    clientComputeInterests(key->mux, &proxy->client.copy, &proxy->filter.copy, proxy->filterData.state);    
    originComputeInterests(key->mux, &proxy->origin.copy, true, originWantWrite);
}

/**
 * Elige la estructura de copia correcta de cada fd,
 * (origin, client o filter).
 */
static copyStruct * copyPtr(MultiplexorKey key) {
    proxyPopv3 * proxy = ATTACHMENT(key);

    if(key->fd == proxy->clientFd)
        return &proxy->client.copy;
    else if(key->fd == proxy->originFd)
        return &proxy->origin.copy;
    return  &proxy->filter.copy;
}

/**
 * 
 */
static inline void shutDownCopy(copyStruct * copy, bool read, bool closeOther, bool closeAll) {
    shutdown(*copy->fd, ((read)? SHUT_RD : SHUT_WR));
    copy->duplex &= ~((read)? READ : WRITE);
    if(closeOther) {
        shutdown(*copy->other->fd, ((!read)? SHUT_RD : SHUT_WR));
        copy->other->duplex &= ~((!read)? READ : WRITE);
    }
    if(closeAll) {
        shutdown(*copy->fd, ((!read)? SHUT_RD : SHUT_WR));
        copy->duplex &= ~((!read)? READ : WRITE);
        if(*copy->other->fd != -1) {
            shutdown(*copy->other->fd, ((read)? SHUT_RD : SHUT_WR));
            copy->other->duplex &= ~((read)? READ : WRITE);
        }
    }  
}

/**
 *
 */
static unsigned receiveFromClient(int fd, copyStruct * copy, uint8_t * ptr, size_t size, bufferADT buffer, proxyPopv3 * proxy) { 
    unsigned ret = COPY;
    ssize_t n = recv(fd, ptr, size, 0);
    
    if(n <= 0) {
        logDebug("Client close the connection in read ready.");
        shutDownCopy(copy, true, !(canProcess(buffer) || canRead(buffer)), *copy->state == ORIGIN_READ_DOWN);
        *copy->state = CLIENT_READ_DOWN;
    } else {
        proxyMetrics.bytesReadBuffer += n;
        proxyMetrics.writesQtyReadBuffer++;
        updateWritePtr(buffer, n);
        logMetric("Coppied from client to proxy, total copied: %zd bytes.", n);
    } 
    return ret;
}

/**
 *
 */
static unsigned receiveFromOrigin(int fd, copyStruct * copy, uint8_t * ptr, size_t size, bufferADT buffer, proxyPopv3 * proxy) { 
    unsigned ret = COPY;
    bool interestRetr = proxyConf.filterActivated, toNewCommand = false, wantToCloseAll;

    ssize_t n = recv(fd, ptr, size, 0);
    if(n <= 0) {
        logDebug("Origin close the connection in read ready.");
        *copy->state = ORIGIN_READ_DOWN;
        wantToCloseAll = proxy->filterData.state == FILTER_CLOSE;
        shutDownCopy(copy, true, false, wantToCloseAll);
        buffer = proxy->filter.copy.writeBuffer;
        if(proxy->filterData.state == FILTER_FILTERING && !canProcess(buffer) && !canRead(buffer))
            proxy->filterData.state = FILTER_ALL_SENT;
    } else {
        proxyMetrics.bytesWriteBuffer += n;
        proxyMetrics.writesQtyWriteBuffer++;
        updateWritePtr(buffer, n);
        if(proxy->filterData.state == FILTER_CLOSE) 
            ret = analizeAndProcessResponse(proxy, buffer, interestRetr, toNewCommand);

        logMetric("Coppied from origin to proxy, total copied: %zd bytes.", n);
    }
    return ret;
}

/**
 *
 */
static unsigned receiveFromFilter(int fd, copyStruct * copy, uint8_t * ptr, size_t size, bufferADT buffer, proxyPopv3 * proxy, MultiplexorKey key) { 
    unsigned ret = COPY;
    bool interestRetr = proxyConf.filterActivated, toNewCommand = false;

    ssize_t n = read(fd, ptr, size);
    if(n == -1) {
        logFatal("Se rompio el filter mientras el proxy recibia.");
        proxy->filterData.state = FILTER_ENDING;
    } else if(n == 0) {
        logDebug("Filter send EOF.");     
        filterClose(key);       
        ret = analizeAndProcessResponse(proxy, proxy->writeBuffer, interestRetr, toNewCommand);
        if(*copy->state == ORIGIN_READ_DOWN && !canRead(buffer) && !canRead(proxy->writeBuffer)) {
            *copy->state = CLIENT_WRITE_DOWN;
            shutDownCopy(&proxy->origin.copy, false, true, true);
            copy->duplex = NO_INTEREST;
        }
    } else {
        proxyMetrics.bytesFilterBuffer += n;
        proxyMetrics.writesQtyFilterBuffer++;
        updateWriteAndProcessPtr(buffer, n);
        logMetric("Coppied from filter to proxy, total copied: %zd bytes.", n);
    } 
    return ret;
}

/**
 *
 */
static unsigned copyRead(MultiplexorKey key) {
    copyStruct * copy  = copyPtr(key);       
    proxyPopv3 * proxy = ATTACHMENT(key);

    unsigned ret = COPY;
    size_t size;
    bufferADT buffer = copy->readBuffer;
    uint8_t *ptr = getWritePtr(buffer, &size);

    switch(copy->target) {
        case COPY_CLIENT:
            ret = receiveFromClient(key->fd, copy, ptr, size, buffer, proxy);
            break;
        case COPY_ORIGIN:
            ret = receiveFromOrigin(key->fd, copy, ptr, size, buffer, proxy);
            break;
        case COPY_FILTER:
            ret = receiveFromFilter(key->fd, copy, ptr, size, buffer, proxy, key);
            break;
    }
    computeInterestsCopy(key);

    if(copy->duplex == NO_INTEREST && (*copy->state == ORIGIN_WRITE_DOWN || proxy->filterData.state == FILTER_CLOSE))
        ret = DONE;
    return ret;
}

/**
 *
 */
static unsigned sendToClient(int fd, copyStruct * copy, uint8_t * ptr, size_t size, bufferADT buffer, proxyPopv3 * proxy) { 
    unsigned ret = COPY;    
    ssize_t n;
    const filterState state = proxy->filterData.state;
    const bool wantSendFromFilter = state == FILTER_FILTERING || state == FILTER_ALL_SENT;

    if(wantSendFromFilter) {
        logDebug("Sending to Client a filter body.");
        buffer = proxy->filter.copy.readBuffer;
        ptr = getReadPtr(buffer, &size);
        proxyMetrics.readsQtyFilterBuffer++;
    } else
        proxyMetrics.readsQtyReadBuffer++;

    n = send(fd, ptr, size, MSG_NOSIGNAL);
    if(n == -1) {        
        logDebug("Client close the connection in write ready.");
        *copy->state = CLIENT_WRITE_DOWN;
        //Si el cliente me cierra la conexion mientras estaba esperando una respuesta de un server sin pipelining, debo cerrar todo, debido a que el otro canal nunca va a ser invocado. 1 porque el cliente ya no escribe y 2 no puedo escribir en el servidor porque estoy esperando la respuesta 
        shutDownCopy(copy, false, true, true);
    } else {
        proxyMetrics.totalBytesToClient += n;
        updateReadPtr(buffer, n);
        logMetric("Coppied from proxy to client, total copied: %d bytes.", (int) n);
        if(*copy->state == ORIGIN_READ_DOWN && !canRead(buffer) && proxy->filterData.state == FILTER_CLOSE) {
            *copy->state = CLIENT_WRITE_DOWN;
            shutDownCopy(copy, false, true, true);
        } 
    }
    return ret;
}

/**
 *
 */
static unsigned sendToOrigin(int fd, copyStruct * copy, uint8_t * ptr, size_t size, bufferADT buffer, proxyPopv3 * proxy) { 
    unsigned ret = COPY;    
    ssize_t n;
    
    commandParserConsume(&proxy->commandParser, buffer, proxy->request.commands, proxy->originCapabilities.pipelining, &proxy->request.waitingResponse);
    ptr = getReadPtr(buffer, &size);

    n = send(fd, ptr, size, MSG_NOSIGNAL);
    if(n == -1) {
        logDebug("Origin close the connection in write ready.");
        *copy->state = ORIGIN_WRITE_DOWN;
        shutDownCopy(copy, false, true, true);
    } else {
        proxyMetrics.readsQtyWriteBuffer++;
        proxyMetrics.totalBytesToOrigin += n;
        updateReadPtr(buffer, n);
        logMetric("Coppied from proxy to origin, total copied: %zd bytes.", n);
        if(*copy->state == CLIENT_READ_DOWN && !(canProcess(buffer) || canRead(buffer))) {
            *copy->state = ORIGIN_WRITE_DOWN;
            shutDownCopy(copy, false, false, false);
        }
    }
    return ret;
}

/**
 *
 */
static unsigned sendToFilter(int fd, copyStruct * copy, uint8_t * ptr, size_t size, bufferADT buffer, proxyPopv3 * proxy) { 
    unsigned ret = COPY;    
    ssize_t n;
    bool interestRetr = false, toNewCommand = true, allReceived;

    ret = analizeAndProcessResponse(proxy, buffer, interestRetr, toNewCommand);
    allReceived = proxy->responseParser.state == RESPONSE_INIT;
    ptr = getReadPtr(buffer, &size);

    n = write(fd, ptr, size);
    if(n == -1) {
        proxy->filterData.state = FILTER_ALL_SENT;
        logWarn("Filter fail: unnable to write in pipe.");
    } else {    
        proxyMetrics.readsQtyWriteBuffer++;
        proxyMetrics.totalBytesToFilter += n;
        updateReadPtr(buffer, n);    

        if(allReceived && !canRead(buffer)) 
            proxy->filterData.state = FILTER_ALL_SENT;
        logMetric("Coppied from proxy to filter, total copied: %zd bytes.", n);
    }
    return ret;
}

/**
 *
 */
static unsigned copyWrite(MultiplexorKey key) {
    copyStruct * copy = copyPtr(key);    
    proxyPopv3 * proxy = ATTACHMENT(key);

    size_t size;
    bufferADT buffer = copy->writeBuffer;
    unsigned ret = COPY;
    uint8_t * ptr = getReadPtr(buffer, &size);

    switch(copy->target) {
        case COPY_CLIENT:
            ret = sendToClient(key->fd, copy, ptr, size, buffer, proxy);
            break;
        case COPY_ORIGIN:
            ret = sendToOrigin(key->fd, copy, ptr, size, buffer, proxy);
            break;
        case COPY_FILTER:
            ret = sendToFilter(key->fd, copy, ptr, size, buffer, proxy);
            break;
    }

    computeInterestsCopy(key);
    if(copy->duplex == NO_INTEREST) {
        ret = DONE;
    }
    return ret;
}

/**
 * Manejador de errores para el estado de filter.
 */
static void errorFilterHandler(void * data) {
    MultiplexorKey key      = *((MultiplexorKey *) data);
    proxyPopv3     * proxy  = ATTACHMENT(key);

    logDebug("Filter have some error.");
    proxy->filterData.state = FILTER_ENDING;
    filterClose(key);
}

/**
 * Setea las variables de entorno para el programa de filtro externo.
 */
static void setEnvironment(const proxyPopv3 * proxy) {
    char bufferSizeStr[10] = {0};
    snprintf(bufferSizeStr, 10, "%zu", proxyConf.bufferSize);

    if(proxyConf.mediaRange != NULL)
        setenv("FILTER_MEDIAS", proxyConf.mediaRange, 1);
    setenv("FILTER_MSG", proxyConf.replaceMsg, 1);
    setenv("POP3FILTER_VERSION", VERSION_NUMBER, 1);
    setenv("POP3_USERNAME",proxy->session.name, 1);
    setenv("POP3_SERVER", proxyConf.stringServer, 1);
    setenv("BUFFER_SIZE", bufferSizeStr, 1);
}

static void workBlockingSlave(void * data) {
    uint8_t dataBuffer[SLAVE_BUFFER_SIZE];
    ssize_t n;
    do {
        n = read(STDIN_FILENO, dataBuffer, sizeof(dataBuffer));
        if(n > 0)
            write(STDOUT_FILENO, dataBuffer, n);
    } while(n > 0);
    errorFilterHandler(data);
}

/**
 *
 */
static void filterInit(MultiplexorKey key) {
    proxyPopv3       * proxy      = ATTACHMENT(key);
    filterDataStruct * filterData = &proxy->filterData;
    multiplexorStatus status;

    logDebug("Filter init.");

    for(int i = 0; i < 2; i++) {
        filterData->infd[i]  = -1;
        filterData->outfd[i] = -1;
    }
    
    reset(proxy->filterBuffer);

    checkFailWithFinally(pipe(filterData->infd), errorFilterHandler, &key, "Filter fail: cannot open a pipe.");
    checkFailWithFinally(pipe(filterData->outfd), errorFilterHandler, &key, "Filter fail: cannot open a pipe.");
       
    pid_t pid = fork();
    checkFailWithFinally(pid, errorFilterHandler, &key, "Filter fail: cannot fork.");
    if(pid == 0) {       
        filterData->slavePid = -1;
        close(filterData->infd[1]);
        close(filterData->outfd[0]);
        dup2(filterData->infd[0], STDIN_FILENO);
        close(filterData->infd[0]);
        dup2(filterData->outfd[1], STDOUT_FILENO);
        close(filterData->outfd[1]);
        close(STDERR_FILENO);
        open(proxyConf.stdErrorFilePath, O_WRONLY | O_APPEND);
        for(int i = 3; i < 1024; i++)
            close(i);

        setEnvironment(proxy);
        execl("./Proxy/FilterWrapper/filterWrapper.out", proxyConf.filterCommand, proxyConf.stdErrorFilePath, (char *)0);
        workBlockingSlave(&key);
    } else {
        filterData->slavePid = pid;
        close(filterData->infd[0]);
        close(filterData->outfd[1]);
        filterData->infd[0]  = -1;
        filterData->outfd[1] = -1;

        checkFailWithFinally(fdSetNIO(filterData->infd[1]), errorFilterHandler, &key, "Filter fail: cannot set nio IN pipe.");
        checkFailWithFinally(fdSetNIO(filterData->outfd[0]), errorFilterHandler, &key, "Filter fail: cannot set nio OUT pipe.");

        status = registerFd(key->mux, filterData->infd[1], &proxyPopv3Handler, NO_INTEREST, proxy);        
        checkAreEqualsWithFinally(status, MUX_SUCCESS, errorFilterHandler, &key, "Filter fail: cannot register IN pipe in multiplexor.");
        proxy->references++;
        status = registerFd(key->mux, filterData->outfd[0], &proxyPopv3Handler, NO_INTEREST, proxy);
        checkAreEqualsWithFinally(status, MUX_SUCCESS, errorFilterHandler, &key, "Filter fail: cannot register OUT pipe in multiplexor.");
        proxy->references++;
    }
}

/**
 *
 */
static void filterClose(MultiplexorKey key) {
    proxyPopv3       * proxy      = ATTACHMENT(key);
    filterDataStruct * filterData = &proxy->filterData;
 
    if(filterData->slavePid > 0) 
        kill(filterData->slavePid, SIGKILL);
    else if(filterData->slavePid == -1)
        exit(1);

    for(int i = 0; i < 2; i++) {
        if(filterData->infd[i] > 0) {
            unregisterFd(key->mux, filterData->infd[i]);
            close(filterData->infd[i]);
        }
        if(filterData->outfd[i] > 0) {
            unregisterFd(key->mux, filterData->outfd[i]);
            close(filterData->outfd[i]);
        }
    }
    memset(filterData, 0, sizeof(filterDataStruct));

    proxy->filterData.state = FILTER_CLOSE;
    logDebug("Filter done");
}


////////////////////////////////////////////////////////////////////////////////
// SEND_ERROR_MSG
////////////////////////////////////////////////////////////////////////////////

static unsigned writeErrorMsg(MultiplexorKey key) {
    proxyPopv3 * proxy = ATTACHMENT(key);
    unsigned ret = SEND_ERROR_MSG;

    if(proxy->errorSender.message == NULL)
        return ERROR;
    if(proxy->errorSender.messageLength == 0)
        proxy->errorSender.messageLength = strlen(proxy->errorSender.message);
        
    logDebug("Enviando error: %s", proxy->errorSender.message);
    char *   ptr  = proxy->errorSender.message + proxy->errorSender.sendedSize;
    ssize_t  size = proxy->errorSender.messageLength - proxy->errorSender.sendedSize;
    ssize_t  n    = send(proxy->clientFd, ptr, size, MSG_NOSIGNAL);
    if(n == -1) {
        shutdown(proxy->clientFd, SHUT_WR);
        ret = ERROR;
    } else {
        proxy->errorSender.sendedSize += n;
        if(proxy->errorSender.sendedSize == proxy->errorSender.messageLength) 
            return ERROR;
    }
    return ret;
}


/**
 * Definición de handlers para cada estado.
 */
static const struct stateDefinition clientStatbl[] = {
    {
        .state            = CONNECTION_RESOLV,
        .onBlockReady     = resolvDone,
    }, {
        .state            = CONNECTING,
        .onWriteReady     = connectionReady,
    }, {
        .state            = HELLO,
        .onArrival        = helloReadInit,
        .onReadReady      = helloRead,        
        .onWriteReady     = helloWrite,
    }, {
        .state            = CHECK_CAPABILITIES,
        .onArrival        = checkCapabilitiesInit,
        .onReadReady      = checkCapabilitiesRead,
        .onWriteReady     = checkCapabilitiesWrite,
    }, {
        .state            = COPY,
        .onArrival        = copyInit,
        .onReadReady      = copyRead,
        .onWriteReady     = copyWrite,
    }, {
        .state            = SEND_ERROR_MSG,
        .onWriteReady     = writeErrorMsg,
    }, {
        .state            = DONE,

    }, {
        .state            = ERROR,
    }
};

static const struct stateDefinition * proxyPopv3DescribeStates(void) {
    return clientStatbl;
}

/**
 * Actualiza la ultima interacción de una sesión.
 */
static inline void updateLastUsedTime(MultiplexorKey key) {
    proxyPopv3 * proxy = ATTACHMENT(key);
    proxy->session.lastUse = time(NULL);
}

///////////////////////////////////////////////////////////////////////////////
/**
 * Handlers top level de la conexión pasiva.
 * Son los que emiten los eventos a la maquina de estados.
 */

static void proxyPopv3Read(MultiplexorKey key) {
    updateLastUsedTime(key);
    stateMachine stm = &ATTACHMENT(key)->stm;
    const proxyPopv3State state = stateMachineHandlerRead(stm, key);

    if(ERROR == state || DONE == state) {
        proxyPopv3Done(key);
    }
}

static void proxyPopv3Write(MultiplexorKey key) {
    updateLastUsedTime(key);
    stateMachine stm = &ATTACHMENT(key)->stm;
    const proxyPopv3State state = stateMachineHandlerWrite(stm, key);

    if(ERROR == state || DONE == state) {
        proxyPopv3Done(key);
    }
}

static void proxyPopv3Block(MultiplexorKey key) {
    updateLastUsedTime(key);
    stateMachine stm = &ATTACHMENT(key)->stm;
    logDebug("Handling blocking.");
    const proxyPopv3State state = stateMachineHandlerBlock(stm, key);

    if(ERROR == state || DONE == state) {
        proxyPopv3Done(key);
    }
}

static void proxyPopv3Close(MultiplexorKey key) {
    deleteProxyPopv3(ATTACHMENT(key));
}

/**
 * Manejador del evento de Tiempo Transcurrido para proxyPopv3.
 */
static void proxyPopv3Timeout(MultiplexorKey key) {
    proxyPopv3 * proxy = ATTACHMENT(key);
    if(proxy != NULL && difftime(time(NULL), proxy->session.lastUse) >= TIMEOUT) {    
        logDebug("Timeout");
        proxy->errorSender.message = "-ERR Disconnected for inactivity.\r\n";
        if(MUX_SUCCESS == setInterest(key->mux, proxy->clientFd, WRITE))
            stateMachineJump(&proxy->stm, SEND_ERROR_MSG, key);
        else
            proxyPopv3Done(key);
    }
}

static void proxyPopv3Done(MultiplexorKey key) {
    logDebug("Connection close, unregistering file descriptors from multiplexor");
    const int fds[] = {
        ATTACHMENT(key)->clientFd,
        ATTACHMENT(key)->originFd,
    };
    if(ATTACHMENT(key)->filterData.state != FILTER_CLOSE)
        filterClose(key);
    for(unsigned i = 0; i < N(fds); i++) {
        if(fds[i] != -1) {
            if(MUX_SUCCESS != unregisterFd(key->mux, fds[i])) {
                fail("Problem trying to unregister a fd: %d.", fds[i]);
            }
            close(fds[i]);
        }
    }
}

