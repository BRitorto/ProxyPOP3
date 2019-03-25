#ifndef RESPONSE_PARSER_H
#define RESPONSE_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#include "buffer.h"
#include "queue.h"
#include "commandParser.h"


typedef enum responseState {
    RESPONSE_INIT,
    RESPONSE_INDICATOR_NEG,
    RESPONSE_INDICATOR_POS,
    RESPONSE_INDICATOR_MSG,
    RESPONSE_BODY,
    RESPONSE_INLINE_CRLF,
    RESPONSE_MULTILINE_CRLF,
    RESPONSE_INTEREST,
    RESPONSE_ERROR,
} responseState;

typedef struct responseParser {
    size_t        lineSize;    
    size_t        stateSize;
    commandType   commandInterest;
    responseState state;
} responseParser;

/** Inicializa el parser */
void responseParserInit(responseParser * parser);

/** Entrega un byte al parser. retorna true si se llego al final  */
responseState responseParserFeed(responseParser * parser, const uint8_t c, queueADT commands);

/**
 * Por cada elemento del buffer llama a `responseParserFeed' hasta que
 * el parseo se encuentra completo o se requieren mas bytes.
 *
 * @param errored parametro de salida. Si es diferente de NULL se deja dicho
 *   si el parsing se debió a una condición de error
 */
responseState responseParserConsume(responseParser * parser, bufferADT buffer, queueADT commands, bool * errored);


responseState responseParserConsumeUntil(responseParser * parser, bufferADT buffer, queueADT commands, bool interested, bool toNewCommand, bool * errored);

#endif

