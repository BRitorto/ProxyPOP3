#ifndef ADMIN_H
#define ADMIN_H

#include <stdbool.h>
#include "rap.h"

bool validateCredentials(char* auth, int socket);

void sendHelloServer(int socket);

void receiveResponse(int socket, responseRAP resp);

void readHello(int socket);

// funciones para el admin
// para todas , las que no son void. Si te dan < 0 es que fallaron 

// esta le pasa el string the password
int setCredentials(int socket, void * pass);
// esta le pasas puntero a int que tiene el nuevo estado
int getFilterClient(int socket, void * state);
// esta le pasas un puntero a un char que es un Y o un N
int setFilterClient(int socket, void * value);
// esta le pasas un string que es te escriba el flaco en pantalla
int setFilterCommandClient(int socket, void * programName);
// esta le pasas un char * donde te deja la respuesta, no tiene que estar initialized te da null si hay error
int getFilterCommandClient(int socket, void * answer);
// esta le pasas el puntero a un int que tiene el size que quieren para los buffers
int setBufferSizeClient(int socket, void * size);
// esta le pasas un puntero a int no initialized en el que te deja la respuesta o -1 si no pudo
int getBufferSizeClient(int socket, void * size);
// esta le pasas el un int donde te deja la cant de usuarios activos
int getNUSersClient(int socket, void * users);
// esta le pasas un int donde te deja el numero de connections que hubo
int getNConnectionsClient(int socket, void * connections);
// esta le pasas un int donde te deja la cantidad de bytes
int getNBytesClient(int socket, void * bytes);
// a esta le pasas un vetor de ints donde te deja las 3 metrics
int getMetricsClient(int socket, void * metrics);
// a esta le pasas un char * no init en el que te dejo la respuesta 
int getMediaRangeClient(int socket, void * answer);
// a esta le pasas un string con los media range
int setMediaRangeClient(int socket, void * mediaRange);
// le pasas un char * con el string que queres pisar en el proxy
int setReplaceMsgClient(int socket, void * msg);
// le pasas un char * no init en donde te dejo el string o NULL en caso de no poder
int getReplaceMsgClient(int socket, void * answer);
// le pasas un char * con el string que queres appendar en el proxy
int addReplaceMsgClient(int socket, void * msg);
// le pasas un char * no init que te dejo el path
int getErrorFilePathClient(int socket, void * answer);
// le pasas el path en el que esta
int setErrorFilePathClient(int socket, void * path);

#endif

