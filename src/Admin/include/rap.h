#ifndef RAP_H
#define RAP_H
#include "buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
typedef struct requestRAP * requestRAP;
typedef struct responseRAP* responseRAP;


requestRAP  newRequest();
responseRAP newResponse();
void readRequest(requestRAP req, bufferADT buffer);
void readResponse(responseRAP resp, bufferADT buffer);

void prepareResponse(responseRAP resp, char buffer []);
void prepareRequest(requestRAP req, char buffer []);
bool parseAuthentication(requestRAP req);
#endif

