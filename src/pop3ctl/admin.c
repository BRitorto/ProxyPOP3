#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>
#include "rap.h"
#include <stdlib.h>
#include <string.h>
#include "errorslib.h"
#include "buffer.h"
#include "logger.h"
#include "admin.h"

#define MAX_BUFFER_SCTP 4207

#define ETAGS_NUMBER 12
static void sendCredentials(char * auth, int socket);
static void sendRequest(requestRAP req, int socket);

int etags[ETAGS_NUMBER] = {0};

typedef enum etagIndex {
    filterEtag              = 0,
    stdErrorFilePathEtag    = 1,
    replaceMsgEtag          = 2,
    transformCommandEtag    = 3,
    stringServerEtag        = 4,
    mediaRangeEtag          = 5,
    listenPop3AddressEtag   = 6,
    listenAdminAddressEtag  = 7,
    replaceMsgSizeEtag      = 8,
    credentialEtag          = 9,
    bufferSizeEtag          = 10,
} etagIndex;

static void sendRequest(requestRAP req, int socket) {
    char buffer [requestSize(req)];
    prepareRequest(req, buffer);
    int ret = sctp_sendmsg (socket, buffer , requestSize(req),
        NULL, 0, 0, 0, 0, 0, 0);
    checkFail(ret,"Error sending the message");

}

void receiveResponse(int socket, responseRAP resp) {
    ssize_t n;
    size_t size;
    bufferADT buffer = createBuffer(MAX_BUFFER_SCTP);
    uint8_t * ptr = getWritePtr(buffer, &size);
    n = sctp_recvmsg(socket, ptr, size, NULL, 0, 0, 0);
    if(n < 0) {
        fprintf(stderr, "Coudn't receive info from server");
        shutdown(socket, SHUT_RD);
    }
    updateWriteAndProcessPtr(buffer, n);
    readResponse(resp, buffer);
    deleteBuffer(buffer);
}

static void sendCredentials(char * auth, int socket) {
    checkIsNotNull(auth,"Credentials are null");
    char separator  = '\0';

    requestRAP req = newRequest();
    int n = 0;
    n = strlen(auth);
    bufferADT buffer = createBuffer(n + 1);
    size_t size;
    uint8_t * bufferPtr = getWritePtr(buffer, &size);
    memcpy(bufferPtr, auth, n);
    updateWriteAndProcessPtr(buffer, n);
    bufferPtr = getWritePtr(buffer, &size);
    * bufferPtr = separator;
    updateWriteAndProcessPtr(buffer, 1);
    bufferPtr = getReadPtr(buffer, &size);

    req->opCode                 = 1;
    req->etag                   = 0;
    req->data                   = bufferPtr;
    req->dataLength             = size;
    req->encoding               = CREDENTIAL_TYPE;

    sendRequest(req, socket);
    destroyRequest(req);
    deleteBuffer(buffer);
}

bool validateCredentials(char* auth, int socket) {
    printf("Sending credentials...\n");
    sendCredentials(auth, socket);
    responseRAP resp = newResponse();
    printf("Waiting response...\n");
    receiveResponse(socket, resp);
    bool answer = resp->respCode == RESP_OK;
    if(resp->data != NULL)
        free(resp->data);
    destroyResponse(resp);
    return answer;
}

void sendHelloServer(int socket) {
    requestRAP req = newRequest();

    int etag = 0;

    req->opCode             = 0;
    req->etag               = etag;
    req->data               = "HELLO";
    req->dataLength         = 6;
    req->encoding           = TEXT_TYPE;

    sendRequest(req, socket);
    destroyRequest(req);

}

void readHello(int socket) {
    responseRAP resp = newResponse();
    receiveResponse(socket, resp);
    checkAreEquals(resp->encoding, TEXT_TYPE, "The encoding for hello msg was wrong\n");
    checkGreaterOrEqualsThan(strlen(HELLO_MSG), resp->dataLength, "Message is bigger than a hello\n");
    checkFail(strcmp(HELLO_MSG, resp->data), "The message wasn't a hello\n");
    if(resp->data != NULL)
        free(resp->data);
    destroyResponse(resp);
}

/* Funciones para settear el servidior u obtener cosas del mismo */

int setCredentials(int socket, void * pass) {
    if( pass == NULL)
        return -1;
    requestRAP req = newRequest();
    req->opCode             = UPDATE_CREDENTIALS;
    req->etag               = etags[credentialEtag];
    req->encoding           = CREDENTIAL_TYPE;
    req->data               = pass;
    req->dataLength         =  strlen((char *) pass);
    sendRequest(req, socket);
    printf("New credentials were sent...\n");
    responseRAP resp = newResponse();
    receiveResponse(socket, resp);

    int ret = 0;
    if(resp->respCode == RESP_OK) {
        ret = 1;
    } else {
        fprintf(stderr, "[ERROR] the server answer a %d code\n", resp->respCode);
        printf("Credential etags where updated, try again\n");
    }
    etags[credentialEtag] = resp->etag;
    if(resp->data != NULL)
        free(resp->data);
    destroyRequest(req);
    destroyResponse(resp);
    return ret;
}

int getFilterClient(int socket, void * state) {
    if(state == NULL )
        return -1;
    requestRAP req = newRequest();
    req->opCode                 = GET_FILTER;
    sendRequest(req, socket);
    printf("Trying to fetch filter...\n");
    responseRAP resp = newResponse();
    receiveResponse(socket, resp);
    int ret = 0;
    if(resp->respCode == RESP_OK){
        * ((int *)state) = ntohl(*((uint32_t *)resp->data));
        ret = 1;
    }else{
        fprintf(stderr, "[ERROR] server answered with code %d\n", resp->respCode);
        state = NULL;
    }
    etags[filterEtag] = resp->etag;
    if(resp->data != NULL)
        free(resp->data);
    destroyRequest(req);
    destroyResponse(resp);
    return ret;
}

int setFilterClient(int socket, void * valuePtr) {
    if(valuePtr == NULL)
        return -1;
    requestRAP req = newRequest();
    char value = *((char*) valuePtr);
    int data = htonl((toupper(value) == 'Y')? 1 : 0);
    req->opCode                 = SET_FILTER;
    req->etag                   = etags[filterEtag];
    req->data                   = &data;
    req->encoding               = INT_TYPE;
    req->dataLength             = sizeof(uint32_t);
    sendRequest(req, socket);
    printf("Trying to set filter...\n");
    responseRAP resp = newResponse();
    receiveResponse(socket, resp);
    int ret = 0;
    if(resp->respCode == RESP_OK){
        ret = 1;
    }else{
        fprintf(stderr, "[ERROR] server answer with code %d\n", resp->respCode);
        printf("The filter e-tag has been updated, try again\n");
    }
    if(resp->data != NULL)
        free(resp->data);
    etags[filterEtag] = resp->etag;
    destroyRequest(req);
    destroyResponse(resp);
    return ret;
}

int setFilterCommandClient(int socket, void * programName) {
    requestRAP req = newRequest();
    req->opCode                 = SET_FILTERCOMMAND;
    req->etag                   = etags[transformCommandEtag];
    req->encoding               = TEXT_TYPE;
    req->data                   = programName;
    req->dataLength             = strlen((char *) programName) + 1;

    sendRequest(req, socket);
    printf("Updating tranformation command...\n");
    responseRAP resp = newResponse();
    receiveResponse(socket, resp);
    int ret = 0;
    if(resp->respCode == RESP_OK) {
        ret = 1;
    }else{
        fprintf(stderr, "[ERROR] server answer with code %d\n", resp->respCode);
        printf("The e-tag for transformation command has been updated\n");
    }
    if(resp->data != NULL)
        free(resp->data);
    etags[transformCommandEtag] = resp->etag;
    destroyRequest(req);
    destroyResponse(resp);
    return ret;
}

int getFilterCommandClient(int socket, void * answer) {
    requestRAP req = newRequest();
    req->opCode                 = GET_FILTERCOMMAND;
    sendRequest(req, socket);
    printf("Getting tranformation name...\n");
    responseRAP resp = newResponse();
    receiveResponse(socket, resp);
    int ret = 0;
    if(resp->respCode == RESP_OK){
        char * ptr = calloc((resp->dataLength) + 1, sizeof(char));
        checkAreNotEquals(ptr, NULL, "Out of memory, calloc through null\n");
        memcpy(ptr, resp->data, resp->dataLength);
        * ((char **)answer) = ptr;
        ret = 1;
    }else{
        fprintf(stderr, "[ERROR] server answer with a code %d\n", resp->respCode);
        printf("E-tags updated, please try again\n");
        answer = NULL;

    }
    if(resp->data != NULL)
        free(resp->data);
    etags[transformCommandEtag] = resp->etag;
    destroyRequest(req);
    destroyResponse(resp);
    return ret;
}

int setBufferSizeClient(int socket, void * size) {
    requestRAP req = newRequest();
    int data = htonl(*((int*)size));
    req->opCode                 = UPDATE_BUFER_SIZE;
    req->etag                   = etags[bufferSizeEtag];
    req->encoding               = INT_TYPE;
    req->data                   = &data;
    req->dataLength             = sizeof(uint32_t);

    sendRequest(req, socket);
    printf("Setting buffer size..\n");
    responseRAP resp = newResponse();
    receiveResponse(socket, resp);
    int ret = 0;
    if(resp->respCode == RESP_OK) {
        ret = 1;
    }else{
        fprintf(stderr, "[ERROR] server answered a %d code\n", resp->respCode);
        printf("E-tags for buffer size are now updated, try again\n");
    }
    if(resp->data != NULL)
        free(resp->data);
    etags[bufferSizeEtag] = resp->etag;
    destroyRequest(req);
    destroyResponse(resp);
    return ret;
}


int getBufferSizeClient(int socket, void * size) {
    requestRAP req = newRequest();
    req->opCode                 = GET_BUFFER_SIZE;
    req->etag                   = 0;
    req->encoding               = 0;
    req->data                   = NULL;
    req->dataLength             = 0;
    sendRequest(req, socket);
    printf("Getting buffer size...\n");
    responseRAP resp = newResponse();
    receiveResponse(socket, resp);
    int ret = 0;
    if(resp->respCode == RESP_OK){
        *((int *) size) = ntohl(*((int*)resp->data));
        ret = 1;
    }else{
        fprintf(stderr, "[ERROR] Server answered a %d code\n", resp->respCode);
        printf("E-tags were updated try again...\n");
    }
    if(resp->data != NULL)
        free(resp->data);
    etags[bufferSizeEtag] = resp->etag;
    destroyRequest(req);
    destroyResponse(resp);
    return ret;
}

int getNUSersClient(int socket, void * users) {
    requestRAP req = newRequest();
    req->opCode                 = GET_ACTIVECONNECTIONS;
    req->etag                   = 0;
    req->encoding               = 0;
    req->data                   = NULL;
    req->dataLength             = 0;

    sendRequest(req, socket);
    printf("Getting current active connections...\n");
    responseRAP resp = newResponse();
    receiveResponse(socket, resp);
    int ret = 0;
    if(resp->respCode == RESP_OK && resp->encoding == INT_TYPE){
        * ((int *) users) = ntohl(*((int*)resp->data));
        ret = 1;
    }else{
        fprintf(stderr, "[ERROR] Server answered a %d code\n", resp->respCode);
        *((int *) users) = -1;
    }
    destroyRequest(req);
    destroyResponse(resp);
    return ret;
}

int getNConnectionsClient(int socket, void * connections) {
    requestRAP req = newRequest();
    req->opCode                 = GET_NCONNECTIONS;
    req->etag                   = 0;
    req->encoding               = 0;
    req->data                   = NULL;
    req->dataLength             = 0;

    sendRequest(req, socket);
    printf("Fetching connections ..\n");
    responseRAP resp =newResponse();
    receiveResponse(socket, resp);

    int ret = 0;
    if(resp->respCode == RESP_OK && resp->encoding == INT_TYPE){
        * ((int *) connections) = ntohl(*((int*)resp->data));
        ret = 1;
    }else{
        fprintf(stderr, "[ERROR] Server answered a %d code\n", resp->respCode);
        * ((int *) connections) = -1;
    }
    if(resp->data != NULL)
        free(resp->data);
    destroyResponse(resp);
    destroyRequest(req);
    return ret;
}

int getNBytesClient(int socket, void * bytes) {
    requestRAP req = newRequest();
    req->opCode                 = GET_NBYTES;
    req->etag                   = 0;
    req->encoding               = 0;
    req->data                   = NULL;
    req->dataLength             = 0;

    sendRequest(req, socket);
    printf("Fetching bytes ..\n");
    responseRAP resp =newResponse();
    receiveResponse(socket, resp);

    int ret = 0;
    if(resp->respCode == RESP_OK && resp->encoding == INT_TYPE){
        * ((int *) bytes) = ntohl(*((int*)resp->data));
        ret = 1;
    }else{
        fprintf(stderr, "[ERROR] Server answered a %d code\n", resp->respCode);
        * ((int *) bytes) = -1;
    }
    if(resp->data != NULL)
        free(resp->data);
    destroyResponse(resp);
    destroyRequest(req);
    return ret;
}

int getMetricsClient(int socket, void * metrics) {
    requestRAP req = newRequest();
    req->opCode                 = GET_NBYTES;
    req->etag                   = 0;
    req->encoding               = 0;
    req->data                   = NULL;
    req->dataLength             = 0;

    sendRequest(req, socket);
    responseRAP resp = newResponse();
    receiveResponse(socket, resp);
    int ret = 1;
    if(resp->respCode != RESP_OK) {
        ret = -1;
        fprintf(stderr, "[ERROR] Server answered with %d code\n", resp->respCode);
        destroyResponse(resp);
        destroyRequest(req); 
        ((int *) metrics)[0] = -1;  
    }else{
        ((int *) metrics)[0] = ntohl(*((int *)resp->data));
    }
    if(resp->data != NULL)
        free(resp->data);

    req->opCode                 = GET_BUFFER_SIZE;
    sendRequest(req, socket);
    receiveResponse(socket, resp);
    if(resp->respCode != RESP_OK) {
        ret = -1;
        fprintf(stderr, "[ERROR] Server answered with %d code\n", resp->respCode);
        destroyResponse(resp);
        destroyRequest(req);
        ((int *) metrics)[1] = -1;
    }else{
        ((int *) metrics)[1] = ntohl(*((int *)resp->data));
    }
    if(resp->data != NULL)
        free(resp->data);

    req->opCode                 = GET_AVERAGE_BUFFER_SIZE;
    sendRequest(req, socket);
    receiveResponse(socket, resp);
    if(resp->respCode != RESP_OK) {
        ret = -1;
        fprintf(stderr, "[ERROR] server answered with %d code\n", resp->respCode);
        destroyResponse(resp);
        destroyRequest(req);
        ((int *) metrics)[2] = -1;
    }else{
        ((int *) metrics)[2] = ntohl(*((int *)resp->data));
    }
    if(resp->data != NULL)
        free(resp->data);

    req->opCode                 = GET_NCONNECTIONS;
    sendRequest(req, socket);
    receiveResponse(socket, resp);
    if(resp->respCode != RESP_OK) {
        ret = -1;
        fprintf(stderr, "[ERROR] Server answered with %d code\n", resp->respCode);
        destroyResponse(resp);
        destroyRequest(req);
        ((int *) metrics)[3] = -1;
    }else{
        ((int *) metrics)[3] = ntohl(*((int *)resp->data));
    }
    if(resp->data != NULL)
        free(resp->data);
    destroyResponse(resp);
    destroyRequest(req);
    return ret;

}

int setMediaRangeClient(int socket, void * mediaRange) {
    requestRAP req = newRequest();
    req->opCode                 = SET_MEDIA_RANGE;
    req->etag                   = etags[mediaRangeEtag];
    req->encoding               = TEXT_TYPE;
    req->data                   = mediaRange;
    req->dataLength             = strlen((char *) mediaRange) + 1;

    sendRequest(req, socket);
    printf("Updating media range../n");
    responseRAP resp = newResponse();
    receiveResponse(socket, resp);
    int ret = 0;
    if(resp->respCode == RESP_OK) {
        ret = 1;
    }else{
        fprintf(stderr, "[ERROR] Server answered with code %d\n", resp->respCode);
        printf("The e-tag for media range has been updated\n");
    }
    if(resp->data != NULL)
        free(resp->data);
    etags[mediaRangeEtag] = resp->etag;
    destroyRequest(req);
    destroyResponse(resp);
    return ret;
}

int getMediaRangeClient(int socket, void * answer) {
    requestRAP req = newRequest();
    req->opCode                 = GET_MEDIA_RANGE;
    sendRequest(req, socket);
    printf("Getting media range name...\n");
    responseRAP resp = newResponse();
    receiveResponse(socket, resp);
    int ret = 0;
    if(resp->respCode == RESP_OK){
        char * ptr = calloc((resp->dataLength) + 1, sizeof(char));
        checkAreNotEquals(ptr, NULL, "Out of memory, calloc through null\n");
        memcpy(ptr, resp->data, resp->dataLength);
        * ((char **)answer) = ptr;
        ret = 1;
    }else{
        fprintf(stderr, "[ERROR] Server answered with a code %d", resp->respCode);
        printf("E-tags updated, please try again\n");
        answer = NULL;

    }
    if(resp->data != NULL)
        free(resp->data);
    etags[mediaRangeEtag] = resp->etag;
    destroyRequest(req);
    destroyResponse(resp);
    return ret;
}

int setReplaceMsgClient(int socket, void * msg) {
    if(msg == NULL)
        return -1;
    requestRAP req = newRequest();
    req->opCode                 = SET_REPLACE_MSG;
    req->etag                   = etags[replaceMsgEtag];
    req->encoding               = TEXT_TYPE;
    req->dataLength             = strlen(msg) + 1;
    req->data                   = calloc(req->dataLength, sizeof(char));
    checkIsNotNull(req->data, "Out of memory");
    memcpy(req->data, msg, req->dataLength - 1);
    sendRequest(req, socket);
    responseRAP resp = newResponse();
    receiveResponse(socket, resp);
    int ret = 0;
    if(resp->respCode == RESP_OK){
        ret = 1;
    }else{
        fprintf(stderr, "[ERROR] Server answered with code %d\n", resp->respCode);
        printf("The e-tag for replace message has been updated\n");
    }
    etags[replaceMsgEtag] = resp->etag;
    if(resp->data != NULL)
        free(resp->data);
    destroyRequest(req);
    destroyResponse(resp);
    return ret;
}

int getReplaceMsgClient(int socket, void * answer) {
    requestRAP req = newRequest();
    req->opCode                 = GET_REPLACE_MSG;
    sendRequest(req, socket);
    printf("Getting replace message name...\n");
    responseRAP resp = newResponse();
    receiveResponse(socket, resp);
    int ret = 0;
    if(resp->respCode == RESP_OK){
        char * ptr = calloc((resp->dataLength) + 1, sizeof(char));
        checkAreNotEquals(ptr, NULL, "Out of memory, calloc through null\n");
        memcpy(ptr, resp->data, resp->dataLength);
        * ((char **)answer) = ptr;
        ret = 1;
    }else{
        fprintf(stderr, "[ERROR] Server answered with a code %d\n", resp->respCode);
        printf("E-tags updated, please try again\n");
        answer = NULL;
    }
    if(resp->data != NULL)
        free(resp->data);
    etags[replaceMsgEtag] = resp->etag;
    destroyRequest(req);
    destroyResponse(resp);
    return ret;
}

int addReplaceMsgClient(int socket, void * msg) {
    if(msg == NULL)
        return -1;
    requestRAP req = newRequest();
    req->opCode                 = ADD_REPLACE_MSG;
    req->etag                   = etags[replaceMsgEtag];
    req->encoding               = TEXT_TYPE;
    req->dataLength             = strlen(msg) + 1;
    req->data                   = calloc(req->dataLength, sizeof(char));
    checkIsNotNull(req->data, "Out of memory\n");
    memcpy(req->data, msg, req->dataLength - 1);
    sendRequest(req, socket);
    responseRAP resp = newResponse();
    receiveResponse(socket, resp);
    int ret = 0;
    if(resp->respCode == RESP_OK){
        ret = 1;
    }else{
        fprintf(stderr, "[ERROR] Server answered with code %d\n", resp->respCode);
        printf("The e-tag for replace message has been updated\n");
    }
    etags[replaceMsgEtag] = resp->etag;
    if(resp->data != NULL)
        free(resp->data);
    destroyRequest(req);
    destroyResponse(resp);
    return ret;
}

int setErrorFilePathClient(int socket, void * msg) {
    if(msg == NULL)
        return -1;
    requestRAP req = newRequest();
    req->opCode                 = SET_ERROR_FILE;
    req->etag                   = etags[stdErrorFilePathEtag];
    req->encoding               = TEXT_TYPE;
    req->dataLength             = strlen(msg) + 1;
    req->data                   = calloc(req->dataLength, sizeof(char));
    checkIsNotNull(req->data, "Out of memory\n");
    memcpy(req->data, msg, req->dataLength - 1);
    sendRequest(req, socket);
    responseRAP resp = newResponse();
    receiveResponse(socket, resp);
    int ret = 0;
    if(resp->respCode == RESP_OK){
        ret = 1;
    }else{
        fprintf(stderr, "[ERROR] Server answered with code %d\n", resp->respCode);
        printf("The e-tag for error file path has been updated\n");
    }
    etags[stdErrorFilePathEtag] = resp->etag;
    if(resp->data != NULL)
        free(resp->data);
    destroyRequest(req);
    destroyResponse(resp);
    return ret;
}

int getErrorFilePathClient(int socket, void * answer) {
    requestRAP req = newRequest();
    req->opCode                 = GET_ERROR_FILE;
    sendRequest(req, socket);
    printf("Getting error file path name...\n");
    responseRAP resp = newResponse();
    receiveResponse(socket, resp);
    int ret = 0;
    if(resp->respCode == RESP_OK){
        char * ptr = calloc((resp->dataLength) + 1, sizeof(char));
        checkAreNotEquals(ptr, NULL, "Out of memory, calloc through null\n");
        memcpy(ptr, resp->data, resp->dataLength);
        * ((char **)answer) = ptr;
        ret = 1;
    }else{
        fprintf(stderr, "[ERROR] Server answered with a code %d\n", resp->respCode);
        printf("E-tags updated, please try again\n");
        answer = NULL;
    }
    if(resp->data != NULL)
        free(resp->data);
    etags[stdErrorFilePathEtag] = resp->etag;
    destroyRequest(req);
    destroyResponse(resp);
    return ret;
}

