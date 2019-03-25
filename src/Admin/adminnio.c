#include "adminnio.h"
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
#include <netinet/sctp.h>
#include <sys/types.h>
#include "proxyPopv3nio.h"
#include "buffer.h"
#include "logger.h"
#include "errorslib.h"
#include "stateMachine.h"
#include "rap.h"
#include "netutils.h"

/** Tamaño maximo que ocupa convertir una ip en un string. */
#define MAX_STRING_IP_LENGTH 50

#define BUFFER_SIZE_SCTP 4112
#define ATTACHMENT(key) ( (struct admin *)(key)->data)
#define ETAG 0;



#define N(x) (sizeof(x)/sizeof((x)[0]))

extern conf proxyConf;
extern metrics proxyMetrics;

typedef enum adminState {
    HELLO,
    AUTHENTICATION,
    TRANSACTION,
    DONE,
    ERROR,
} adminState;

typedef struct admin {
    adminState state;
    int clientFd;
    bufferADT readBuffer;
    bufferADT writeBuffer;
    char clientAddress[MAX_STRING_IP_LENGTH];

    /** para el timeout */
    time_t lastUse;
    /** maquinas de estados */
    struct stateMachineCDT stm;
    /** cantidad de referencias a este objeto. si es uno se debe destruir */
    unsigned references;
    /** siguiente en el pool */
    struct admin * next;
} admin;




// Static function definitions
static void adminRead(MultiplexorKey key);
static void adminWrite(MultiplexorKey key);
static void adminTimeout(MultiplexorKey key);
static admin * newAdmin(int clientFd, size_t bufferSize);
static void destroyAdmin(admin * adm);
static void adminClose(MultiplexorKey key);
static const struct stateDefinition * adminDescribeStates(void);
static unsigned readCredentials(MultiplexorKey key);
static void adminDone(MultiplexorKey key);
static int checkCredentials(MultiplexorKey key);
static void deleteAdmin( admin* a);

static void  welcomeAdmin(const unsigned state, MultiplexorKey key);

//static unsigned reply_auth(MultiplexorKey key);
static void deleteAdmin( admin* a);

static fdInterest computeInterests(MultiplexorKey key);
static void setSuccessfulLogIn(MultiplexorKey key);
static void setErrorLogIn(MultiplexorKey key);

//static void setHelloSuccessful(MultiplexorKey key);
static inline void updateLastUsedTime(MultiplexorKey key);

//static unsigned readHello(MultiplexorKey key);
static unsigned sendHello(MultiplexorKey key);
static unsigned sendMsg(MultiplexorKey key);

static unsigned analyzeRequest(MultiplexorKey key);
static int checkRequest(MultiplexorKey key);

unsigned sendTransactionResponse(MultiplexorKey key);
unsigned updateCredentials(requestRAP req, MultiplexorKey key);
unsigned setFilter(requestRAP req, MultiplexorKey key);
unsigned getFilter(requestRAP req, MultiplexorKey key);
unsigned setFilterCommand(requestRAP req, MultiplexorKey key);
unsigned getFilterCommand(requestRAP req, MultiplexorKey key);
unsigned setBuffersize(requestRAP req, MultiplexorKey key);
unsigned getBufferSize(requestRAP req, MultiplexorKey key);
unsigned handleErrorMsg(requestRAP req, MultiplexorKey key);
unsigned getBufferStats(requestRAP req, MultiplexorKey key);
unsigned getCurrentConnections(requestRAP req, MultiplexorKey key);
unsigned getNBytes(requestRAP req, MultiplexorKey key);
unsigned getConnections(requestRAP req, MultiplexorKey key);
unsigned setMediaRange(requestRAP req, MultiplexorKey key);
unsigned getMediaRange(requestRAP req, MultiplexorKey key);
unsigned setReplaceMsg(requestRAP req, MultiplexorKey key);
unsigned getReplaceMsg(requestRAP req, MultiplexorKey key);
unsigned addReplaceMsg(requestRAP req, MultiplexorKey key);
unsigned setErrorFilePath(requestRAP req, MultiplexorKey key);
unsigned getErrorFilePath(requestRAP req, MultiplexorKey key);
// end definitions


static const eventHandler adminHandler = {
    .read   = adminRead,
    .write  = adminWrite,
    .block  = NULL,//adminBlock,
    .close  = adminClose,
    .timeout = adminTimeout,
};

/**
 * Pool de `struct admin', para ser reusados.
 *
 * Como tenemos un unico hilo que emite eventos no necesitamos barreras de
 * contención.
 * 
 * copiado de pop3
 */

static const unsigned  maxPool  = 50; // tamaño máximo
static unsigned        poolSize = 0;  // tamaño actual
static struct admin  * pool      = 0;  // pool propiamente dicho


static admin * newAdmin(int clientFd, size_t bufferSize) {
   
    struct admin * ret;
    bufferADT readBuffer, writeBuffer;
    if(pool == NULL) {
        ret = malloc(sizeof(*ret));
        checkAreNotEquals(ret, NULL, "out of memory , malloc throw null");
        readBuffer = createBuffer(bufferSize);
        writeBuffer = createBuffer(bufferSize);
    } else {
        ret         = pool;
        pool        = pool->next;
        ret->next   = 0;
        readBuffer  = ret->readBuffer;
        writeBuffer = ret->writeBuffer;
        reset(readBuffer);
        reset(writeBuffer);
    }
    if(ret == NULL) {
        goto finally;
    }
    memset(ret, 0x00, sizeof(*ret));

    ret->state                      = HELLO;
    ret->clientFd                   = clientFd;
    ret->readBuffer                 = readBuffer;
    ret->writeBuffer                = writeBuffer;
    ret->stm    .initial            = HELLO;
    ret->stm    .maxState           = ERROR;
    ret->stm    .states             = adminDescribeStates();
    stateMachineInit(&ret->stm);
    
    ret->references = 1;
finally:
    return ret;
}

static void destroyAdmin(admin * adm) {
    if(adm == NULL)
        return;
    
    deleteBuffer(adm->readBuffer);
    deleteBuffer(adm->writeBuffer);
    free(adm);
}

void poolAdminDestroy(void) {
    admin * next, * current;
    for(current = pool; current != NULL ; current = next) {
        next = current->next;
        destroyAdmin(current);
    }
}

void adminPassiveAccept(MultiplexorKey key)
{
    struct sockaddr_storage       clientAddr;
    socklen_t                     client_addr_len = sizeof(clientAddr);
    admin * clientAdmin = NULL;

    const int clientFd = accept(key->fd, (struct sockaddr*) &clientAddr, &client_addr_len);
    if(clientFd == -1) {
        goto fail;
    }
    if(fdSetNIO(clientFd) == -1) {
        goto fail;
    }
    logInfo("Accepting new admin");
    
    clientAdmin = newAdmin(clientFd, BUFFER_SIZE_SCTP);

    if(MUX_SUCCESS != registerFd(key->mux, clientFd, &adminHandler, WRITE, clientAdmin)) {
        goto fail;
    }
    const struct sockaddr * client = (const struct sockaddr *) &clientAddr;
    sockaddrToString(clientAdmin->clientAddress, MAX_STRING_IP_LENGTH, client);

    logInfo("Connection established, client : %d", clientAdmin->clientAddress);

    return ;
fail:
    if(clientFd != -1) {
        close(clientFd);
    }
    deleteAdmin(clientAdmin);
}

/**
 * Actualiza la ultima interacción de una sesión.
 */
static inline void updateLastUsedTime(MultiplexorKey key) {
    admin * a = ATTACHMENT(key);
    a->lastUse = time(NULL);
}

/**
 * Manejador del evento de Tiempo Transcurrido para proxyPopv3.
 */
static void adminTimeout(MultiplexorKey key) {
    admin * a = ATTACHMENT(key);
    if(a != NULL && difftime(time(NULL), a->lastUse) >= TIMEOUT) {    
        logDebug("Timeout");
        if(key->fd != -1)
            adminDone(key); 
            //Avisar al cliente que le cerramos -ERR Timeout algo asi
    }
    
}

static void adminRead(MultiplexorKey key) {
    stateMachine stm = &ATTACHMENT(key)->stm;
    updateLastUsedTime(key);
    const adminState state = stateMachineHandlerRead(stm, key);

    admin * adm = ATTACHMENT(key);
    adm->state = state;
    if(ERROR == state || DONE == state) {
        adminDone(key);
    }
}

static void adminWrite(MultiplexorKey key) {
    stateMachine stm = &ATTACHMENT(key)->stm;
    updateLastUsedTime(key);
    const adminState state = stateMachineHandlerWrite(stm, key);

    admin * adm = ATTACHMENT(key);
    adm->state = state;
    if(ERROR == state || DONE == state) {
        adminDone(key);
    }
}

static void adminClose(MultiplexorKey key) {
    deleteAdmin(ATTACHMENT(key));
}

static void adminDone(MultiplexorKey key) {
    const int fds[] = {
        ATTACHMENT(key)->clientFd,
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


static void welcomeAdmin(const unsigned state, MultiplexorKey key) {
    admin * adm = ATTACHMENT(key);
    responseRAP resp = newResponse();
    resp->respCode                  = 1;
    resp->etag                      = 0;
    resp->data                      = HELLO_MSG;
    resp->dataLength                = strlen(HELLO_MSG);
    resp->encoding                  = TEXT_TYPE;

    size_t bufSize;
    bufferADT buffer = adm->writeBuffer;
    char* ptr = (char*) getWritePtr(buffer, &bufSize);
    prepareResponse(resp, ptr);
    updateWriteAndProcessPtr(buffer, responseSize(resp));
    destroyResponse(resp);
}

static unsigned sendHello(MultiplexorKey key) {
    int result = sendMsg(key);
    if(result <= 0)
        logError("error sending admin hello msg");
    return (result >= 0)? AUTHENTICATION : ERROR;
}

static unsigned readCredentials(MultiplexorKey key) {
    admin * d = ATTACHMENT(key);
    size_t size;
    ssize_t n;
    bufferADT buffer = d->readBuffer;
    unsigned ret = AUTHENTICATION;

    uint8_t *ptr = getWritePtr(buffer, &size);
    n = sctp_recvmsg(key->fd, ptr, size, NULL, 0, 0, 0);

    if(n <= 0) {
        logDebug(" %s CERRO LA CONEXION", d->clientAddress);
        shutdown(d->clientFd, SHUT_RD);
        return ERROR;
    } else {
        updateWriteAndProcessPtr(buffer, n);
    }

    if(checkCredentials(key)) {
        setSuccessfulLogIn(key);
        logInfo("successful login for client %d", d->clientAddress );
        ret = TRANSACTION;
    }else{
        setErrorLogIn(key);
        logInfo("the credentials were wrong for %s", d->clientAddress);
    }
    
    getReadPtr(buffer, &size);
    updateReadPtr(buffer, size);
    setInterest(key->mux, d->clientFd, READ);
    return ret;
}

static int checkCredentials(MultiplexorKey key) {
    bufferADT buffer = ATTACHMENT(key)->readBuffer;
    requestRAP req = newRequest();
    readRequest(req, buffer);
    int ret = parseAuthentication(req, proxyConf.credential);
    if(req->data != NULL)
        free(req->data);
    destroyRequest(req);
    return ret;
}

/** Puede ser que todas estas funciones las lleve a una generica que tenga un vector con respuestas 
 *  una list que tiene el codigo y la respuesta asociada para generar la response */
static void setSuccessfulLogIn(MultiplexorKey key) {
    admin *  adm = ATTACHMENT(key);
    bufferADT buffer = adm->writeBuffer;
    size_t size;
    void * bufferPtr = getWritePtr(buffer, &size);
    // para ver si puedo escribir el datagrama
    checkGreaterThan(size, RESPONSE_HEADER_SIZE, "error no enough space in write buffer");
    responseRAP resp = newResponse();
    resp->respCode = RESP_OK;
    resp->etag = ETAG;
    resp->encoding = TEXT_TYPE;
    resp->data = "OK";
    resp->dataLength = 3;

    size = responseSize(resp);
    prepareResponse(resp, bufferPtr);
    updateWriteAndProcessPtr(buffer, responseSize(resp));
    destroyResponse(resp);
    sendMsg(key);
}

static void setErrorLogIn(MultiplexorKey key) {
    admin *  adm = ATTACHMENT(key);
    bufferADT buffer = adm->writeBuffer;
    size_t size;
    void * bufferPtr = getWritePtr(buffer, &size);
    // para ver si puedo escribir el datagrama
    checkGreaterThan(size, RESPONSE_HEADER_SIZE, "error no enough space in write buffer");
    responseRAP resp = newResponse();
    resp->respCode = RESP_LOGIN_ERROR;
    resp->etag = ETAG;
    resp->encoding = TEXT_TYPE;
    resp->data = "ERROR.";
    resp->dataLength = 7;
    size = responseSize(resp);
    updateWriteAndProcessPtr(buffer, size);
    prepareResponse(resp, bufferPtr);
    destroyResponse(resp);
    sendMsg(key);
}

static unsigned sendMsg(MultiplexorKey key) {
    admin * adm = ATTACHMENT(key);
    bufferADT buff = adm->writeBuffer;
    size_t size;
    void * buffer = getReadPtr(buff, &size);
    ssize_t ret = sctp_sendmsg(key->fd, buffer , size,
        NULL, 0, 0, 0, 0, 0, 0);
    checkFail(ret,"error sending the message");

    updateReadPtr(buff, size);
    setInterest(key->mux, adm->clientFd, READ);
    return ret;
}

/* Me llega un request del admin */
static unsigned analyzeRequest(MultiplexorKey key) {
    admin * d = ATTACHMENT(key);
    size_t size;
    ssize_t n;
    bufferADT buffer = d->readBuffer;
    unsigned ret = TRANSACTION;
    uint8_t * ptr = getWritePtr(buffer, &size);
    n = recv(key->fd, ptr, size, 0);
    if(n <= 0) {
        logDebug("%s CERRO LA CONEXION", d->clientAddress);
        shutdown(d->clientFd, SHUT_RD);
        computeInterests(key);
        return ret = DONE;
    }else{

        updateWriteAndProcessPtr(buffer, n);
    }
    
    ret = checkRequest(key);
    getReadPtr(buffer, &size);
    updateReadPtr(buffer, size);
    computeInterests(key);
    return ret;
}

/* Deriva el requesta quien sea necesario */
static int checkRequest(MultiplexorKey key) {
    admin * adm = ATTACHMENT(key);
    // primero parseamos el request 
    requestRAP req = newRequest();
    readRequest(req, adm->readBuffer);
    int ret = DONE;
    switch(req->opCode) {
        case SET_FILTER:   
            ret = setFilter(req, key);
            break;
        case UPDATE_CREDENTIALS:
            ret = updateCredentials(req, key);
            break;
        case GET_FILTER:
            ret = getFilter(req, key);
            break;
        case SET_FILTERCOMMAND:
            ret = setFilterCommand(req, key);
            break;
        case GET_FILTERCOMMAND:
            ret = getFilterCommand(req, key);
            break;
        case UPDATE_BUFER_SIZE:
            ret = setBuffersize(req, key);
            break;
        case GET_BUFFER_SIZE:
            ret = getBufferSize(req, key);
            break;
        case GET_AVERAGE_BUFFER_SIZE:
            ret = getBufferStats(req, key);
            break;
        case GET_NCONNECTIONS:
            ret = getConnections(req, key);
            break;
        case GET_NBYTES:
            ret = getNBytes(req,key);
            break;
        case GET_ACTIVECONNECTIONS:
            ret = getCurrentConnections(req, key);
            break;
        case SET_MEDIA_RANGE:
            ret = setMediaRange(req, key);
            break;
        case GET_MEDIA_RANGE:
            ret = getMediaRange(req, key);
            break;
        case GET_REPLACE_MSG:
            ret = getReplaceMsg(req,key);
            break;
        case SET_REPLACE_MSG:
            ret = setReplaceMsg(req, key);
            break;
        case ADD_REPLACE_MSG:
            ret = addReplaceMsg(req, key);
            break;
        case SET_ERROR_FILE:
            ret = setErrorFilePath(req, key);
            break;
        case GET_ERROR_FILE:
            ret = getErrorFilePath(req, key);
            break;
        default:
            ret = handleErrorMsg(req, key);
            break;
    }
    if(req->data != NULL)
        free(req->data);
    destroyRequest(req);
    return ret;
}

unsigned handleErrorMsg(requestRAP req, MultiplexorKey key) {
    responseRAP resp = newResponse();
    resp->respCode                  = RESP_SERVER_ERROR;
    resp->encoding                  = TEXT_TYPE;
    resp->data                      = "there was an error with the server";
    resp->dataLength                = strlen("there was an error with the server");

    admin * adm = ATTACHMENT(key);
    size_t size;
    char * ptr = (char *) getWritePtr(adm->writeBuffer, &size);
    prepareResponse(resp, ptr);
    updateWriteAndProcessPtr(adm->writeBuffer, responseSize(resp));
    return DONE;
}

unsigned setFilter(requestRAP req, MultiplexorKey key) {
    checkAreEquals(req->encoding, INT_TYPE,"invalid type for filter set");
    responseRAP resp = newResponse();
    if(req->etag != (proxyConf.etags)[filterEtag]){
        resp->respCode = RESP_SET_FILTER_ERROR;
        resp->etag = (proxyConf.etags)[filterEtag];
    } else{
        proxyConf.filterActivated = (ntohl(*((int*)(req->data))))? true : false;
        admin * adm = ATTACHMENT(key);
        logInfo(" admin %d set filter %s",adm->clientAddress, (proxyConf.filterActivated)? "ON" : "OFF" );
        (proxyConf.etags)[filterEtag]++;
        resp->respCode = RESP_OK;
        resp->etag = (proxyConf.etags)[filterEtag];
        
    }
    admin * adm = ATTACHMENT(key);
    size_t size;
    prepareResponse(resp, (char*) getWritePtr(adm->writeBuffer, &size));
    updateWriteAndProcessPtr(adm->writeBuffer, responseSize(resp));
    destroyResponse(resp);
    return TRANSACTION;
}

unsigned updateCredentials(requestRAP req, MultiplexorKey key) {
    responseRAP resp = newResponse();
    
    if((req->etag) !=  (proxyConf.etags)[credentialEtag] || req->encoding != CREDENTIAL_TYPE ){

        resp->respCode = RESP_SET_CREDENTIALS_ERROR;
        resp->etag = (proxyConf.etags)[credentialEtag];
    }else{
        free(proxyConf.credential);
        proxyConf.credential = calloc(req->dataLength, sizeof(char));
        checkAreNotEquals(proxyConf.credential, NULL, "out of memory, calloc throw null");
        memcpy(proxyConf.credential, req->data, req->dataLength);
        (proxyConf.etags)[credentialEtag]++;
        
        resp->respCode = RESP_OK;
        resp->etag = (proxyConf.etags)[credentialEtag];
    }
        
    admin * adm = ATTACHMENT(key);
    logInfo("admin %s %s",adm->clientAddress, 
        (resp->respCode == RESP_OK)? "changed the password successfully" : "failed to change passwords");
    size_t size;
    prepareResponse(resp, (char*) getWritePtr(adm->writeBuffer, &size));
    updateWriteAndProcessPtr(adm->writeBuffer,responseSize(resp));
    destroyResponse(resp);
    return TRANSACTION;
}

unsigned getFilter(requestRAP req, MultiplexorKey key) {
    responseRAP resp = newResponse();
    int data = (htonl((uint32_t) (proxyConf.filterActivated)));;
    resp->respCode                  = RESP_OK;
    resp->etag                      = (proxyConf.etags)[filterEtag];
    resp->encoding                  = INT_TYPE;
    resp->data                      = &data;
    resp->dataLength                = sizeof(uint32_t);

    admin * adm = ATTACHMENT(key);
    size_t size;
    char * ptr = (char *) getWritePtr(adm->writeBuffer, &size);
    prepareResponse(resp, ptr);
    updateWriteAndProcessPtr(adm->writeBuffer, responseSize(resp));
    destroyResponse(resp);
    return TRANSACTION;
}


unsigned setFilterCommand(requestRAP req, MultiplexorKey key) {

    responseRAP resp = newResponse();
    if(req->etag == (proxyConf.etags)[transformCommandEtag] && req->encoding == TEXT_TYPE){    
        if(proxyConf.filterCommand != NULL && proxyConf.filterCommanAdminChanged)
            free(proxyConf.filterCommand);
        proxyConf.filterCommand = calloc(req->dataLength + 1, sizeof(char));
        checkAreNotEquals(proxyConf.filterCommand, NULL, "out of memory, calloc throw null");
        proxyConf.filterCommanAdminChanged = true;
        memcpy(proxyConf.filterCommand, req->data, req->dataLength);
        admin * adm = ATTACHMENT(key);
        logInfo("admin %s successfully changed filter command to %s",adm->clientAddress, req->data);
    
        (proxyConf.etags)[transformCommandEtag]++;
        resp->respCode                  = RESP_OK;
    }else{
        resp->respCode             = RESP_SET_FILTERCOMMAND_ERROR;
    }
    resp->etag                      = (proxyConf.etags)[transformCommandEtag];
    
    admin * adm = ATTACHMENT(key);
    
    size_t size;
    char * ptr = (char *) getWritePtr(adm->writeBuffer, &size);
    prepareResponse(resp, ptr);
    updateWriteAndProcessPtr(adm->writeBuffer, responseSize(resp));
    destroyResponse(resp);
    return TRANSACTION;
}

unsigned getFilterCommand(requestRAP req, MultiplexorKey key) {
    responseRAP resp = newResponse();
    resp->respCode                  = RESP_OK;
    resp->etag                      = (proxyConf.etags)[transformCommandEtag];
    resp->encoding                  = TEXT_TYPE;
    resp->data                      = calloc(strlen(proxyConf.filterCommand)+1, sizeof(char));
    checkAreNotEquals(resp->data, NULL, "out of memory, calloc throw null");
    resp->dataLength                = strlen(proxyConf.filterCommand);
    memcpy(resp->data, proxyConf.filterCommand, resp->dataLength);
    
    admin * adm = ATTACHMENT(key);
    size_t size;
    char * ptr = (char *) getWritePtr(adm->writeBuffer, &size);
    prepareResponse(resp, ptr);
    updateWriteAndProcessPtr(adm->writeBuffer, responseSize(resp));
    free(resp->data);
    destroyResponse(resp);
    return TRANSACTION;
}

unsigned setBuffersize(requestRAP req, MultiplexorKey key) {
    responseRAP resp = newResponse();
    if((proxyConf.etags)[bufferSizeEtag] == req->etag && req->encoding == INT_TYPE ){
        int newSize = ntohl(*((uint32_t * ) req->data));
        proxyConf.bufferSize = newSize;
        (proxyConf.etags)[bufferSizeEtag]++;
        admin * adm = ATTACHMENT(key);
        logInfo("admin %s successfully changed buffer size to %d",adm->clientAddress, newSize);

        resp->respCode                  = RESP_OK;
    }else{
        resp->respCode                  = RESP_SET_BUFFERSIZE_ERROR;
    }
    resp->etag                      = (proxyConf.etags)[bufferSizeEtag];

    admin * adm = ATTACHMENT(key);

    size_t size;
    char * ptr = (char *) getWritePtr(adm->writeBuffer, &size);
    prepareResponse(resp, ptr);
    updateWriteAndProcessPtr(adm->writeBuffer, responseSize(resp));
    destroyResponse(resp);
    return TRANSACTION;
}

unsigned getBufferSize(requestRAP req, MultiplexorKey key) {
    responseRAP resp = newResponse();
    int data = (htonl(proxyConf.bufferSize));
    resp->respCode                  = RESP_OK;
    resp->etag                      = (proxyConf.etags)[bufferSizeEtag];
    resp->encoding                  = INT_TYPE;
    resp->data                      = &data;
    resp->dataLength                = sizeof(int);


    admin * adm = ATTACHMENT(key);
    size_t size;
    char * ptr = (char *) getWritePtr(adm->writeBuffer, &size);
    prepareResponse(resp, ptr);
    updateWriteAndProcessPtr(adm->writeBuffer, responseSize(resp));
    destroyResponse(resp);
    return TRANSACTION;
}

unsigned getBufferStats(requestRAP req, MultiplexorKey key) {
    responseRAP resp = newResponse();
    size_t bufferAccess = proxyMetrics.writesQtyReadBuffer + proxyMetrics.writesQtyWriteBuffer + proxyMetrics.writesQtyFilterBuffer;
    bufferAccess += proxyMetrics.readsQtyReadBuffer + proxyMetrics.readsQtyWriteBuffer + proxyMetrics.readsQtyFilterBuffer;
    logDebug("aca");
    size_t bufferBytesCount = proxyMetrics.totalBytesToClient + proxyMetrics.totalBytesToFilter + proxyMetrics.totalBytesToOrigin;
    int data = 0;
    if(bufferAccess != 0)
        data = htonl(bufferBytesCount / bufferAccess);
    resp->respCode                  = RESP_OK;
    resp->etag                      = 0;
    resp->encoding                  = INT_TYPE;
    resp->data                      = &data;
    resp->dataLength                = sizeof(uint32_t) + 1;

    admin * adm = ATTACHMENT(key);
    bufferADT buffer = adm->writeBuffer;
    size_t size;
    uint8_t * ptr = getWritePtr(buffer, &size);
    prepareResponse(resp, (char *) ptr);
    updateWriteAndProcessPtr(buffer, responseSize(resp));
    destroyResponse(resp);
    return TRANSACTION;
}

unsigned getCurrentConnections(requestRAP req, MultiplexorKey key) {
    responseRAP resp = newResponse();
    int data = htonl(proxyMetrics.activeConnections);
    resp->respCode                  = RESP_OK;
    resp->etag                      = 0;
    resp->encoding                  = INT_TYPE;
    resp->data                      = &data;
    resp->dataLength                = sizeof(uint32_t) + 1;

    admin * adm = ATTACHMENT(key);
    bufferADT buffer = adm->writeBuffer;
    size_t size;
    uint8_t * ptr = getWritePtr(buffer, &size);
    prepareResponse(resp, (char *) ptr);
    updateWriteAndProcessPtr(buffer, responseSize(resp));
    destroyResponse(resp);
    return TRANSACTION;
}

unsigned getConnections(requestRAP req, MultiplexorKey key) {
    responseRAP resp = newResponse();
    int data = htonl(proxyMetrics.totalConnections);
    resp->respCode                  = RESP_OK;
    resp->etag                      = 0;
    resp->encoding                  = INT_TYPE;
    resp->data                      = &data;
    resp->dataLength                = sizeof(uint32_t) + 1;

    admin * adm = ATTACHMENT(key);
    bufferADT buffer = adm->writeBuffer;
    size_t size;
    uint8_t * ptr = getWritePtr(buffer, &size);
    prepareResponse(resp, (char *) ptr);
    updateWriteAndProcessPtr(buffer, responseSize(resp));
    destroyResponse(resp);
    return TRANSACTION;
}

unsigned getNBytes(requestRAP req, MultiplexorKey key) {
    responseRAP resp = newResponse();
    int data = htonl(proxyMetrics.totalBytesToClient);
    resp->respCode                  = RESP_OK;
    resp->etag                      = 0;
    resp->encoding                  = INT_TYPE;
    resp->data                      = &data;
    resp->dataLength                = sizeof(uint32_t) + 1;

    admin * adm = ATTACHMENT(key);
    bufferADT buffer = adm->writeBuffer;
    size_t size;
    uint8_t * ptr = getWritePtr(buffer, &size);
    prepareResponse(resp,(char *) ptr);
    updateWriteAndProcessPtr(buffer, responseSize(resp));
    destroyResponse(resp);
    return TRANSACTION;
}

unsigned setMediaRange(requestRAP req, MultiplexorKey key) {
    checkAreEquals(req->encoding, TEXT_TYPE, "incorrect encoding media range");
    responseRAP resp = newResponse();
    if(req->etag == (proxyConf.etags)[mediaRangeEtag]){    
        if(proxyConf.mediaRange != NULL && proxyConf.mediaRangeAdminChanged)
            free(proxyConf.mediaRange);
        
        proxyConf.mediaRange = calloc(req->dataLength + 1, sizeof(char));
        checkAreNotEquals(proxyConf.mediaRange, NULL, "out of memory, calloc throw null");
        proxyConf.mediaRangeAdminChanged = true;
        memcpy(proxyConf.mediaRange, req->data, req->dataLength);
        admin * adm = ATTACHMENT(key);
        logInfo("admin %s successfully changed media range to %s",adm->clientAddress, req->data);
    
        (proxyConf.etags)[mediaRangeEtag]++;
        resp->respCode                  = RESP_OK;
    }else{
        resp->respCode             = RESP_SET_FILTERCOMMAND_ERROR;
    }
    resp->etag                      = (proxyConf.etags)[mediaRangeEtag];
    
    admin * adm = ATTACHMENT(key);
    bufferADT buffer = adm->writeBuffer;
    size_t size;
    uint8_t * ptr = getWritePtr(buffer, &size);
    prepareResponse(resp,(char *) ptr);
    updateWriteAndProcessPtr(buffer, responseSize(resp));
    destroyResponse(resp);
    return TRANSACTION;
}

unsigned getMediaRange(requestRAP req, MultiplexorKey key) {
    responseRAP resp = newResponse();
    resp->respCode                  = RESP_OK;
    resp->etag                      = (proxyConf.etags)[mediaRangeEtag];
    resp->encoding                  = TEXT_TYPE;
    if(proxyConf.mediaRange != NULL){
    resp->data                      = calloc(strlen(proxyConf.mediaRange)+1, sizeof(char));
    checkAreNotEquals(resp->data, NULL, "out of memory");
    memcpy(resp->data, proxyConf.mediaRange, strlen(proxyConf.mediaRange));
    resp->dataLength                = strlen(proxyConf.mediaRange);
    }
    admin * adm = ATTACHMENT(key);
    bufferADT buffer = adm->writeBuffer;
    size_t size;
    uint8_t * ptr = getWritePtr(buffer, &size);
    prepareResponse(resp,(char *) ptr);
    updateWriteAndProcessPtr(buffer, responseSize(resp));
    destroyResponse(resp);
    return TRANSACTION;
}

/* Reemplaza el replaceMsg por lo que te manden */
unsigned setReplaceMsg(requestRAP req, MultiplexorKey key) {
    responseRAP resp = newResponse();
    admin * adm = ATTACHMENT(key);
    if(req->etag == (proxyConf.etags)[replaceMsgEtag] && req->encoding == TEXT_TYPE){
        if(proxyConf.messageCount >= 1 )
            free(proxyConf.replaceMsg);
        proxyConf.replaceMsgSize = strlen(req->data);
        proxyConf.replaceMsg = calloc(proxyConf.replaceMsgSize, sizeof(char));
        proxyConf.replaceMsgAdminChanged = true;
        memcpy(proxyConf.replaceMsg, req->data, req->dataLength);
        (proxyConf.etags)[replaceMsgEtag]++;
        resp->respCode              = RESP_OK;
        resp->encoding              = TEXT_TYPE;
        logInfo("admin %s set replace msg to %s", adm->clientAddress, proxyConf.replaceMsg);
        
    }else{
        resp->respCode              = RESP_SET_REPLACE_MSG_ERROR;
    }
    resp->etag                  = (proxyConf.etags)[replaceMsgEtag];
    bufferADT buffer = adm->writeBuffer;
    size_t size;
    uint8_t * ptr = getWritePtr(buffer, &size);
    prepareResponse(resp,(char *) ptr);
    updateWriteAndProcessPtr(buffer, responseSize(resp));
    destroyResponse(resp);
    return TRANSACTION;
}

unsigned getReplaceMsg(requestRAP req, MultiplexorKey key) {
    responseRAP resp = newResponse();
    resp->respCode                  = RESP_OK;
    resp->etag                      = proxyConf.etags[replaceMsgEtag];
    resp->encoding                  = TEXT_TYPE;
    resp->dataLength                = strlen(proxyConf.replaceMsg) + 1;
    resp->data                      = calloc(resp->dataLength, sizeof(char));
    memcpy(resp->data, proxyConf.replaceMsg, resp->dataLength -1);
    admin * adm = ATTACHMENT(key);
    bufferADT buffer = adm->writeBuffer;
    size_t size;
    uint8_t * ptr = getWritePtr(buffer, &size);
    prepareResponse(resp,(char *) ptr);
    updateWriteAndProcessPtr(buffer, responseSize(resp));
    free(resp->data);
    destroyResponse(resp);
    return TRANSACTION;
}

unsigned addReplaceMsg(requestRAP req, MultiplexorKey key) {
    responseRAP resp = newResponse();
    admin * adm = ATTACHMENT(key);
    if(req->encoding == TEXT_TYPE && req->etag == proxyConf.etags[replaceMsgEtag]){
        if(proxyConf.messageCount >= 1){
           char * newMsg = realloc(proxyConf.replaceMsg, proxyConf.replaceMsgSize + strlen(req->data));
           if(newMsg == NULL){

                shutdown(adm->clientFd, SHUT_RD);
                computeInterests(key);
                return  DONE;
            }
           proxyConf.replaceMsg = newMsg;
           newMsg[proxyConf.replaceMsgSize] = '\0';
           newMsg = strcat(newMsg, "\n");
           newMsg = strcat(newMsg, req->data);
           proxyConf.replaceMsgSize += 1; // por el \n
        }else{
            proxyConf.replaceMsgSize = strlen(req->data);
            char * newMsg = calloc(proxyConf.replaceMsgSize, sizeof(char));
            if(newMsg == NULL){

                shutdown(adm->clientFd, SHUT_RD);
                computeInterests(key);
                return DONE;
            }
            proxyConf.replaceMsg = newMsg;
            memcpy(proxyConf.replaceMsg, req->data, req->dataLength);
        }
        proxyConf.etags[replaceMsgEtag]++;
        proxyConf.replaceMsgAdminChanged = true;
        proxyConf.messageCount++;
        proxyConf.replaceMsgSize += strlen(req->data);
        resp->respCode              = RESP_OK;
        logInfo("admin %s add replace msg to %s", adm->clientAddress, proxyConf.replaceMsg);
    }else{
        resp->respCode              = RESP_ADD_REPLACE_MSG_ERROR;
    }
    resp->etag                  = proxyConf.etags[replaceMsgEtag];
    
    bufferADT buffer = adm->writeBuffer;
    size_t size;
    uint8_t * ptr = getWritePtr(buffer, &size);
    prepareResponse(resp,(char *) ptr);
    updateWriteAndProcessPtr(buffer, responseSize(resp));
    free(resp->data);
    destroyResponse(resp);
    return TRANSACTION;
}

unsigned setErrorFilePath(requestRAP req, MultiplexorKey key) {
    responseRAP resp = newResponse();
    admin * adm = ATTACHMENT(key);
    if(req->encoding == TEXT_TYPE && req->etag == proxyConf.etags[stdErrorFilePathEtag]){
        if(proxyConf.stdErrorFilePathAdminChanged)
            free(proxyConf.stdErrorFilePath);
        proxyConf.stdErrorFilePathAdminChanged = true;
        proxyConf.stdErrorFilePath = calloc(req->dataLength, sizeof(char));
        checkIsNotNull(proxyConf.stdErrorFilePath, "Out of memory");
        logInfo("%s admin updated error file path to %s", adm->clientAddress, req->data );
        memcpy(proxyConf.stdErrorFilePath, req->data, req->dataLength - 1);
        proxyConf.etags[stdErrorFilePathEtag]++;
        resp->respCode              = RESP_OK;
    }else{
        resp->respCode              = RESP_SET_ERROR_FILE_ERROR;
    }
    resp->etag                      = proxyConf.etags[stdErrorFilePathEtag];
    bufferADT buffer = adm->writeBuffer;
    size_t size;
    uint8_t * ptr = getWritePtr(buffer, &size);
    prepareResponse(resp,(char *) ptr);
    updateWriteAndProcessPtr(buffer, responseSize(resp));
    free(resp->data);
    destroyResponse(resp);
    return TRANSACTION;
}

unsigned getErrorFilePath(requestRAP req, MultiplexorKey key) {
    responseRAP resp = newResponse();
    resp->respCode                  = RESP_OK;
    resp->etag                      = proxyConf.etags[stdErrorFilePathEtag];
    resp->encoding                  = TEXT_TYPE;
    resp->dataLength                = strlen(proxyConf.stdErrorFilePath) + 1;
    resp->data                      = calloc(resp->dataLength, sizeof(char));
    memcpy(resp->data, proxyConf.stdErrorFilePath, resp->dataLength - 1);
    admin * adm = ATTACHMENT(key);
    bufferADT buffer = adm->writeBuffer;
    size_t size;
    uint8_t * ptr = getWritePtr(buffer, &size);
    prepareResponse(resp,(char *) ptr);
    updateWriteAndProcessPtr(buffer, responseSize(resp));
    free(resp->data);
    destroyResponse(resp);
    return TRANSACTION;
}

unsigned sendTransactionResponse(MultiplexorKey key) {
    admin * adm = ATTACHMENT(key);
    sendMsg(key);
    return adm->state;
}

static fdInterest computeInterests(MultiplexorKey key) {
    fdInterest ret = NO_INTEREST;
    admin * adm = ATTACHMENT(key);
    bufferADT readBuffer  = adm->readBuffer;
    bufferADT writeBuffer = adm->writeBuffer;
    int clientFd = adm->clientFd;
    if (canRead(readBuffer))
        ret |= READ;
    if (canRead(writeBuffer)) 
        ret |= WRITE;
    
    if(MUX_SUCCESS != setInterest(key->mux, clientFd, ret))
        fail("Problem trying to set interest: %d,to multiplexor  fd: %d.", ret, clientFd);
    
    return ret;
}

static void deleteAdmin( admin* a) {
    if(a != NULL) {
        if(a->references == 1) {
            if(poolSize < maxPool) {
                a->next = pool;
                pool    = a;
                poolSize++;
            } else {
                destroyAdmin(a);
            }
        } else {
            a->references -= 1;
        }
    } 
}


/* definición de handlers para cada estado */
static const struct stateDefinition clientStatbl[] = {
    {
        .state            = HELLO,
        .onArrival        = welcomeAdmin,
        .onWriteReady     = sendHello,

    }, {
        .state            = AUTHENTICATION,
        .onReadReady      = readCredentials,

    }, {
        .state            = TRANSACTION,
        .onReadReady      = analyzeRequest,
        .onWriteReady     = sendTransactionResponse,
  
    }, {
        .state            = DONE,

    }, {
        .state            = ERROR,
    }
};

static const struct stateDefinition * adminDescribeStates(void) {
    return clientStatbl;
}


