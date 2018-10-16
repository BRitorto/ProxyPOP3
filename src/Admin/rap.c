#include "rap.h"
#include <arpa/inet.h>
#include "errorslib.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "logger.h"

#define REQUEST_HEADER_SIZE 100
#define RESPONSE_HEADER_SIZE 100
#define OP_CODE_SIZE 4
#define ETAG_SIZE 32
#define OP_ARGS_SIZE 64

#define RESP_CODE_SIZE 4
#define RESP_ARGS_SIZE 64


// authentication
#define AUTH_ARGS "UP"

#define USER "admin"
#define PASS "admin"

/*
Estructura de request en nuestro protocolo RAP
 atributos:
 - op-code: es un int de 4 bytes que representa la operacion
 
 - etag:    es un string de 32 bytes que representa el id de un recurso, 
            si se modifica el recurso cambia esto
 
 - op_args: es un vector de hasta 64 bytes que tiene los posibles argumentos 
            necesarios para la operacion
            estos argumentos son son en formato ascii y determinan el orden 
            en que van a aparecer en el data frame.
 
 - Data:    es el vector de los valores actuales que se pasaron a 
            traves de los argumentos ( cada uno es null separated ).
*/
struct requestRAP{
    int op_code;
    char * etag;
    char * op_args;
    void * data;
    size_t dataLength;
};


/*
Estructura de response en nuestro protocolo RAP
atributos:
 - resp_code:   Es un int de 4 bytes que tiene el numero de la respuesta. 
  
- etag:         es un string de 32 bytes que representa el id de un recurso, 
                si se modifica el recurso cambia esto.
 
 - resp_args:   es un vector de hasta 64 bytes que tiene los posibles argumentos 
                necesarios para la codificacion de la respuesta
                estos argumentos son son en formato ascii y determinan el orden 
                en que van a aparecer en el data frame.

 - data:        es el vector de los valores actuales que se pasaron a 
                traves de los argumentos ( cada uno es null separated ).


*/
struct responseRAP{
    int resp_code;
    char * etag;
    char * resp_args;
    void * data;
    size_t dataLength;
};



// Posibles estructuras del tipo data

// struct authCredentials{
//     char* username;
//     char* password;
// };

//static functions
static void * copyDataValue(void ** data, int* size);



requestRAP newRequest(){
    requestRAP new = calloc(1, sizeof(requestRAP));
    return new;
}


responseRAP newResponse(){
    responseRAP new = calloc(1, sizeof(responseRAP));
    return new;
}


void readRequest(requestRAP req, bufferADT buffer){
    
    checkIsNotNull(req, "request adt is null ");
    checkIsNotNull(buffer, "buffer is null");
    size_t requestSize;
    void* bufferPtr = getReadPtr(buffer,&requestSize);
    checkIsNotNull(requestSize - REQUEST_HEADER_SIZE + 1,"Request header are incomplete" );
    char op_code [5] = {0};
    op_code[0] = ((char*) bufferPtr)[0];
    op_code[1] = ((char*) bufferPtr)[1];
    op_code[2] = ((char*) bufferPtr)[2];
    op_code[3] = ((char*) bufferPtr)[3];

    req->op_code  = atoi(op_code);

    updateReadPtr(buffer, OP_CODE_SIZE);

    // tomamos el etag;
    req->etag = calloc(ETAG_SIZE, sizeof(char));
    bufferPtr = getReadPtr(buffer,&requestSize);
    memcpy(req->etag, bufferPtr, ETAG_SIZE);
    updateReadPtr(buffer, ETAG_SIZE);
    // op_args
    req->op_args = calloc(OP_ARGS_SIZE, sizeof(char));
    bufferPtr = getReadPtr(buffer,&requestSize);
    memcpy(req->op_args, bufferPtr, OP_ARGS_SIZE );
    updateReadPtr(buffer, OP_ARGS_SIZE);
    bufferPtr = getReadPtr(buffer,&requestSize);
    req->data = calloc(requestSize, sizeof(char));
    memcpy(req->data, bufferPtr, requestSize );
    req->dataLength = requestSize;
}

void readResponse(responseRAP resp, bufferADT buffer){

    checkIsNotNull(resp, "response adt is null ");
    checkIsNotNull(buffer, " buffer is null ");

    size_t responseSize;
    void * bufferPtr = getReadPtr(buffer, &responseSize);

    checkGreaterThan(responseSize, RESPONSE_HEADER_SIZE-1, "Response header are incomplete" );

    char resp_code [5] = {0};
    resp_code[0] = ((char*) bufferPtr)[0];
    resp_code[1] = ((char*) bufferPtr)[1];
    resp_code[2] = ((char*) bufferPtr)[2];
    resp_code[3] = ((char*) bufferPtr)[3];
    resp->resp_code  = atoi(resp_code);
    updateReadPtr(buffer, RESP_CODE_SIZE);
    // tomamos el etag;
    bufferPtr = getReadPtr(buffer,&responseSize);
    resp->etag = calloc(ETAG_SIZE, sizeof(char));
    memcpy(resp->etag, bufferPtr , ETAG_SIZE);
    updateReadPtr(buffer, ETAG_SIZE);
    bufferPtr = getReadPtr(buffer,&responseSize);
    resp->resp_args = calloc(RESP_ARGS_SIZE, sizeof(char));
    memcpy(resp->resp_args, bufferPtr, RESP_ARGS_SIZE);
    updateReadPtr(buffer, RESP_ARGS_SIZE);
    bufferPtr = getReadPtr(buffer,&responseSize);
    resp->data = calloc(responseSize , sizeof(char));
    memcpy(resp->data, bufferPtr, responseSize);
    resp->dataLength = responseSize;
}


/*
    por ahora el unico metodo de auth que tenemos es
    user pass
    para ello tienen que mandar los args US ( uppercase)
    y estos deben ser null terminated y estar juntos en la seccion de data
*/
bool parseAuthentication(requestRAP req){

    checkIsNotNull(req,"the request is null");

    int sizeAux;
    req->op_args = (char*) copyDataValue((void**)&req->op_args,&sizeAux);
    (req->op_args)[2] = 0;

    logDebug("op_args : %s",req->op_args);
    if(strcmp(req->op_args, AUTH_ARGS) != 0){
        logError("authentication headers are wrong!");
        return false;
    }
    int size;
    char * username = copyDataValue(&(req->data), &size);
    char * password = copyDataValue(&((req->data) ), &size ); 
    if(strcmp(username, USER) == 0 && strcmp(password, PASS) == 0){
        logInfo("Successful login!");
    }
    return true;
}

//copia datos del data frame segun null terminated
static void * copyDataValue(void ** data, int * size){
    *size = strlen(*data); // como esta terminado en null puedo usar esta funcion
    void * ret = calloc(*size, sizeof(char));
    memcpy(ret, *data, *size );
    *data = (void *) ((long) (* data ) + (long) (* size) + 1); 
    return ret;
}   

size_t requestSize(requestRAP req){
    checkIsNotNull(req, "null request");
    return req->dataLength + REQUEST_HEADER_SIZE;
}

size_t responseSize(responseRAP resp){
    checkIsNotNull(resp, "null response");
    return resp->dataLength + RESPONSE_HEADER_SIZE;
}

void prepareResponse(responseRAP resp, char buffer []) {
    checkIsNotNull(resp, "null Response datagram");
    
    //limpiar buffer
    for(size_t i = 0; i < responseSize(resp); i++)
        buffer[i] = 0;
    //resp_code
    sprintf(buffer, "%4d", resp->resp_code);
    //etag
    char* pointer = (char*) ((long) buffer + RESP_CODE_SIZE);
    memcpy(pointer, resp->etag, ETAG_SIZE);
    //args
    pointer = (char*) ((long) pointer + RESP_ARGS_SIZE);
    memcpy(pointer, resp->resp_args, RESP_ARGS_SIZE);
    //data
    pointer = (char*) ((long) pointer + RESPONSE_HEADER_SIZE);
    memcpy(pointer, resp->data, resp->dataLength);

}


void prepareRequest(requestRAP req, char buffer []) {
    checkIsNotNull(req, "null Request datagram");
    
    //limpiar buffer
    for(size_t i = 0; i < requestSize(req); i++)
        buffer[i] = 0;

    //op_code
    sprintf(buffer, "%4d", req->op_code);
    //etag
    char* pointer = (char*) ((long) buffer + RESP_CODE_SIZE);
    memcpy(pointer, req->etag, ETAG_SIZE);
    //args
    pointer = (char*) ((long) pointer + RESP_ARGS_SIZE);
    memcpy(pointer, req->op_args, RESP_ARGS_SIZE);
    //data
    pointer = (char*) ((long) pointer + RESPONSE_HEADER_SIZE);
    memcpy(pointer, req->data, req->dataLength);

}



