#ifndef HELLO_PARSER_H
#define HELLO_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#include "buffer.h"


typedef enum helloState {
    HELLO_INDICATOR,
    HELLO_MSG,
    HELLO_CRLF,
    HELLO_DONE,
    HELLO_ERROR,
} helloState;

typedef struct helloParser {
    /** permite al usuario del parser almacenar sus datos */
    size_t msgSize;
    /******** zona privada *****************/
    helloState state;
} helloParser;

/** inicializa el parser */
void helloParserInit(helloParser * parser);

/** entrega un byte al parser. retorna true si se llego al final  */
helloState helloParserFeed(helloParser * parser, uint8_t c);

/**
 * por cada elemento del buffer llama a `helloParserFeed' hasta que
 * el parseo se encuentra completo o se requieren mas bytes.
 *
 * @param errored parametro de salida. si es diferente de NULL se deja dicho
 *   si el parsing se debió a una condición de error
 */
helloState helloConsume(helloParser * parser, bufferADT readBuffer, bool * errored);

/**
 * Permite distinguir a quien usa helloParserFeed si debe seguir
 * enviando caracters o no. 
 *
 * En caso de haber terminado permite tambien saber si se debe a un error
 */
bool helloIsDone(const helloState state, bool * errored);

#endif

