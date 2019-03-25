#ifndef RAP_H
#define RAP_H

#include "buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

typedef struct requestRAP * requestRAP;
typedef struct responseRAP * responseRAP;
typedef struct authCredentials * authCredentials;


typedef enum dataType {
        CREDENTIAL_TYPE  = 1,
        TEXT_TYPE        = 2,
        INT_TYPE         = 3,
} dataType;


typedef enum opCodeType {
        LOGIN                   = 1,
        GET_NBYTES              = 2,
        GET_NUSERS              = 3,
        GET_NCONNECTIONS        = 4,
        GET_BUFFER_SIZE         = 5,
        UPDATE_BUFER_SIZE       = 6,
        UPDATE_CREDENTIALS      = 7,
        SET_FILTER              = 8,
        GET_FILTER              = 9,
        SET_FILTERCOMMAND       = 10,
        GET_STATISTICS          = 11,
        GET_FILTERCOMMAND       = 12,
        GET_AVERAGE_BUFFER_SIZE = 13,
        GET_ACTIVECONNECTIONS   = 14,
        SET_MEDIA_RANGE         = 15,
        GET_MEDIA_RANGE         = 16,
        SET_REPLACE_MSG         = 17,
        GET_REPLACE_MSG         = 18,
        ADD_REPLACE_MSG         = 19,
        SET_ERROR_FILE          = 20,
        GET_ERROR_FILE          = 21,
        
} opCodeType;



// defino los codigos handeleados por el server
#define RESP_SERVER_ERROR 0
#define RESP_LOGIN_ERROR 101
#define RESP_SET_BUFFERSIZE_ERROR 601
#define RESP_SET_CREDENTIALS_ERROR 702
#define RESP_SET_FILTER_ERROR 801
#define RESP_SET_FILTERCOMMAND_ERROR 1001
#define RESP_SET_REPLACE_MSG_ERROR  1701
#define RESP_ADD_REPLACE_MSG_ERROR  1901
#define RESP_SET_ERROR_FILE_ERROR   2001
#define RESP_GET_ERROR_FILE_ERROR   2101

#define RESP_OK 200

#define REQUEST_HEADER_SIZE (4 * sizeof(uint32_t))
#define RESPONSE_HEADER_SIZE (4 * sizeof(uint32_t))
#define OP_CODE_SIZE sizeof(uint32_t)
#define ETAG_SIZE sizeof(uint32_t)

#define DATA_LENGTH_SIZE sizeof(uint32_t)

#define DATA_TYPE_SIZE sizeof(uint32_t)

#define RESP_CODE_SIZE sizeof(uint32_t)

#define ARG_DELIMITER "\0"

#define HELLO_MSG "OK"

/*
Estructura de request en nuestro protocolo RAP
 atributos:
 - op-code:     es un int de 4 bytes que representa la operacion
 
 - etag:        es un int de 4 bytes que representa el id de un recurso, 
                si se modifica el recurso cambia esto
 
 - dataLength:  es un int de 4 bytes que representa la cantidad de bytes en data

 - Data:        es el vector de los valores actuales que se pasaron a 
                traves de los argumentos ( cada uno es null separated ).

 - Enconding:   the Data type of data
*/
struct requestRAP{
    int opCode;
    int etag;
    void * data;
    int encoding;
    size_t dataLength;
};


/*
Estructura de response en nuestro protocolo RAP
atributos:
 - respCode:   Es un int de 4 bytes que tiene el numero de la respuesta. 
  
- etag:         es un int de 4 bytes que representa el id de un recurso, 
                si se modifica el recurso cambia esto.
                
- dataLength:  es un int de 4 bytes que representa la cantidad de bytes en data

 - data:        es el vector de los valores actuales que se pasaron a 
                traves de los argumentos ( cada uno es null separated ).
                En caso que el mensaje supere un maximo de 4096 bytes el mismo DEBE ser
                chunkeado.


 - Enconding:   the Data type of data

*/
struct responseRAP{
    int respCode;
    int etag;
    void * data;
    int encoding;
    size_t dataLength;
};

// generan un nuevo datagrama
requestRAP  newRequest();
responseRAP newResponse();

// hacen el free de las estructuras
void destroyRequest(requestRAP req);
void destroyResponse(responseRAP resp);

// funciones para leer la data que viene por el socket
// ACLARACION: estas funciones no van a parsear la data, para interpretar la misma en una estrutra 
//  es necesario usar otras funciones.
void readRequest(requestRAP req, bufferADT socketBuffer);
void readResponse(responseRAP resp, bufferADT socketBuffer);

// estos metodos son los encargados de traducir la informacion en base a que operacion se quiere realizar
// FLOW:
//  Tomo lo que me devuleve el socket, 
//  lo llamo al metodo de parse
//  el req o resp que salga de eso lo metodo en el handler 
//  este trabaja sobre el datagrama para que, quien deba, se encargue.
//void handleRequest(requestRAP req);
//void handleResponse(responseRAP resp);

// funciones para generara la estrutura con la informacion que se quiere mandar
// las primeras dos son genericas, toman los bytes a corde al datagrama de RAP
bool parseAuthentication(requestRAP req, char * validCredential);
int parseHello(requestRAP req);
// funciones para convertir la estructura en un vector que se manda a traves del socket

void prepareResponse(responseRAP resp, char buffer []);
void prepareRequest(requestRAP req, char buffer []);

size_t responseSize(responseRAP resp);
size_t requestSize(requestRAP req);


#endif

