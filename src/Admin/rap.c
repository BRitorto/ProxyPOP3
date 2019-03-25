#include "rap.h"
#include <arpa/inet.h>
#include "errorslib.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "logger.h"
#include "linkedList.h"

// authentication
#define AUTH_ARGS 2

#define CREDENTIAL "admin"



typedef enum requestCode{
        OK_RESPONSE             = 200,
        BAD_CREDENTIAL          = 101,
        NOT_LAST_HASH           = 301,
}requestCode;


// define helper structures for variable arguments and data types
struct argNode{
    char argValue;
    void * dataValue;
};
typedef struct argNode * argNode;

struct statistic {
    size_t currentConnections;
    size_t bufferSize;
}; 

typedef struct statistic * statiscADT;


struct update {
    int property;
    void * newValue;
};
typedef struct update * updateADT;

//static functions
static void * copyDataValue(void ** data, int* size);

requestRAP newRequest() {
    requestRAP new = calloc(1, sizeof(struct requestRAP));
    checkAreNotEquals(new, NULL, "ouf of memory, calloc through null");
    return new;
}

responseRAP newResponse() {
    responseRAP new = calloc(1, sizeof(struct responseRAP));
    checkAreNotEquals(new, NULL, "ouf of memory, calloc through null");
    return new;
}

void destroyRequest(requestRAP req) {
    checkIsNotNull(req, " the request was null");
    free(req);
}

void destroyResponse(responseRAP resp) {
    checkIsNotNull(resp, " the response was null");
    free(resp);
}

void readRequest(requestRAP req, bufferADT buffer){
    checkIsNotNull(req, "request adt is null ");
    checkIsNotNull(buffer, "buffer is null");
    size_t size;
    void * bufferPtr = getReadPtr(buffer,&size);
    checkIsNotNull(size - REQUEST_HEADER_SIZE + 1,"Request header are incomplete" );
    req->opCode  = ntohl(* ((uint32_t * )bufferPtr));
    updateReadPtr(buffer, OP_CODE_SIZE);
    // tomamos el etag;
    bufferPtr = getReadPtr(buffer,&size);
    req->etag = ntohl(* ((uint32_t * )bufferPtr));
    updateReadPtr(buffer, ETAG_SIZE);

    bufferPtr = getReadPtr(buffer,&size);
    req->dataLength = ntohl(*(uint32_t * )(bufferPtr));
    updateReadPtr(buffer, DATA_LENGTH_SIZE);
    bufferPtr = getReadPtr(buffer, &size);
    req->encoding = ntohl(*(uint32_t *) (bufferPtr));
    updateReadPtr(buffer, DATA_TYPE_SIZE);
    bufferPtr = getReadPtr(buffer, &size);
    req->data = calloc(size, sizeof(char));
    checkAreNotEquals(req->data, NULL, "ouf of memory, calloc through null");
    memcpy(req->data, bufferPtr, size );
}

void readResponse(responseRAP resp, bufferADT buffer) {
    checkIsNotNull(resp, "response adt is null ");
    checkIsNotNull(buffer, " buffer is null ");

    size_t size;
    void * bufferPtr = getReadPtr(buffer, &size);
    checkGreaterThan(size, RESPONSE_HEADER_SIZE-1, "Response header are incomplete" );
    resp->respCode                      = ntohl(*(uint32_t *) bufferPtr);

    updateReadPtr(buffer, RESP_CODE_SIZE);
    bufferPtr = getReadPtr(buffer,&size);
    resp->etag                          = ntohl(*(uint32_t *) bufferPtr);

    updateReadPtr(buffer, ETAG_SIZE);
    bufferPtr = getReadPtr(buffer,&size);
    resp->dataLength                    = ntohl(*(uint32_t *) bufferPtr);

    updateReadPtr(buffer, DATA_LENGTH_SIZE);
    bufferPtr = getReadPtr(buffer, &size);
    resp->encoding                      = ntohl(*(uint32_t *) bufferPtr);

    updateReadPtr(buffer, DATA_TYPE_SIZE);
    bufferPtr = getReadPtr(buffer,&size);

    resp->data                          = calloc(size , sizeof(char));
    checkAreNotEquals(resp->data, NULL, "ouf of memory, calloc through null");
    memcpy(resp->data, bufferPtr, size);
    resp->dataLength                    = size;
    updateReadPtr(buffer, size);
}

/* Copia datos del data frame segun null terminated */
static void * copyDataValue(void ** data, int * size) {
    *size = strlen(*data); // como esta terminado en null puedo usar esta funcion
    void * ret = calloc(*size, sizeof(char));
    checkAreNotEquals(ret, NULL, "ouf of memory, calloc through null");
    memcpy(ret, *data, *size );
    *data = (void *) ((long) (* data ) + (long) (* size) + 1); 
    return ret;
}   

size_t requestSize(requestRAP req) {
    checkIsNotNull(req, "null request");
    size_t result = 0;
    result = (req->dataLength);
    result += (size_t) REQUEST_HEADER_SIZE;
    return result;
}

size_t responseSize(responseRAP resp) {
    checkIsNotNull(resp, "null response");
    return (int) (resp->dataLength) + RESPONSE_HEADER_SIZE;
}

void prepareResponse(responseRAP resp, char buffer []) {
    checkIsNotNull(resp, "null Response datagram");
    //limpiar buffer
    memset(buffer, 0, responseSize(resp));
    
    //respCode
    int number = htonl(resp->respCode);
    memcpy(buffer, &number, sizeof(uint32_t));
    //etag
    char * pointer = (char*) ((long) buffer + RESP_CODE_SIZE);
    number = htonl(resp->etag);
    memcpy(pointer, &number, sizeof(uint32_t));

    //data size
    pointer = (char*) ((long) pointer + ETAG_SIZE);
    number = htonl(resp->dataLength);
    memcpy(pointer, &number, DATA_LENGTH_SIZE);
    // data type
    pointer = (char*) ((long) pointer + DATA_LENGTH_SIZE);
    number = htonl(resp->encoding);
    memcpy(pointer, &number, DATA_TYPE_SIZE);
    //data
    pointer = (char*) ((long) pointer + DATA_TYPE_SIZE);
    memcpy(pointer, resp->data, resp->dataLength);
}

void prepareRequest(requestRAP req, char buffer []) {
    checkIsNotNull(req, "null Request datagram");
    
    //limpiar buffer
    memset(buffer, 0, requestSize(req));

    //opCode
    int number = htonl(req->opCode);
    memcpy(buffer, &number, OP_CODE_SIZE);
    //etag
    char * pointer = (char *) ((long) buffer + OP_CODE_SIZE);
    number = htonl(req->etag);
    memcpy(pointer, &number, ETAG_SIZE);
    //data length
    pointer = (char*) ((long) pointer + ETAG_SIZE);
    number = htonl(req->dataLength);
    memcpy(pointer, &number, DATA_LENGTH_SIZE);
    //dataEncoding
    pointer = (char*) ((long) pointer + DATA_LENGTH_SIZE);
    number = htonl(req->encoding);
    memcpy(pointer, &number, DATA_TYPE_SIZE);
    //data
    pointer = (char*) ((long) pointer + DATA_TYPE_SIZE );
    memcpy(pointer, req->data, req->dataLength);    
}

/*
    Por ahora el unico metodo de auth que tenemos es
    user pass
    para ello tienen que mandar los args US ( uppercase)
    y estos deben ser null terminated y estar juntos en la seccion de data
*/
bool parseAuthentication(requestRAP req, char * validCredential) {
    checkIsNotNull(req,"the request is null");

    int size;
    if( req->encoding != CREDENTIAL_TYPE){
        logError("authentication headers are wrong!");
        return false;
    }
    void * dataPtr = req->data;
    char * credential = copyDataValue(&(dataPtr), &size);
    bool ret = false;
    if(strcmp(credential, validCredential) == 0 ){
        ret = true;
    }
    free(credential);
    return ret;
}

// Devuelve 1 si el response es un hello
// 0 en caso contrario, el metodo que se encargue de este estado tomara
// la accion correspondiente
int parseHello(requestRAP resp) {
    checkIsNotNull(resp,"the response is null");

    if((resp->encoding != TEXT_TYPE) || strcmp(resp->data, "HELLO") != 0)
        return 0;

    return 1;
}
