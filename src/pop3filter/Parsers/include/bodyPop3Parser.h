#ifndef BODY_POP3_PARSER_H
#define BODY_POP3_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#include "buffer.h"


typedef enum bodyPop3State {
    BODY_POP3_MSG,
    BODY_POP3_DOT,
    BODY_POP3_PRE_DOT,
    BODY_POP3_CRLF,
    BODY_POP3_DONE,
    BODY_POP3_ERROR,
} bodyPop3State;

typedef struct bodyPop3Parser {
    size_t          lineSize;    
    size_t          stateSize;
    bodyPop3State   state;
} bodyPop3Parser;

/** Inicializa el parser */
void bodyPop3ParserInit(bodyPop3Parser * parser);

/** 
 * Entrega un byte al parser. Copia ese byte al buffer dest  
 * y puede agregar un byte de mas si este debia ser escado
 * segun POP3, o bien, no copiar la marca de final escapada.
 */
bodyPop3State bodyPop3ParserFeed(bodyPop3Parser * parser, const uint8_t c, bufferADT dest, bool skip);

bool bodyPop3IsDone(const bodyPop3State state, bool * errored);

/**
 * Por cada elemento del buffer llama a `bodyPop3ParserFeed' hasta que
 * el parseo se encuentra completo o se requieren mas bytes.
 *
 * @param errored parametro de salida. Si es diferente de NULL se deja dicho
 *   si el parsing se debió a una condición de error
 * @param dest parametro de salida. Se copia los bytes del buffer src 
 * escapando o agregando la marca de final segun sea indicado en el
 * parametro skip.
 */
bodyPop3State bodyPop3ParserConsume(bodyPop3Parser * parser, bufferADT src, bufferADT dest, bool skip, bool * errored);

#endif

