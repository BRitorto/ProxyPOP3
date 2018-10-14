#ifndef AUTH_PARSER_H
#define AUTH_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#include "buffer.h"


typedef enum authState {
    AUTH_INDICATOR,
    AUTH_USER,
    AUTH_CRLF,
    AUTH_DONE,
    AUTH_ERROR,
} authState;

typedef struct authParser {
    /** permite al usuario del parser almacenar sus datos */
    size_t msgSize;
    /******** zona privada *****************/
    authState state;
} authParser;

/** inicializa el parser */
void authParserInit(authParser * parser);

/** entrega un byte al parser. retorna true si se llego al final  */
authState authParserFeed(authParser * parser, uint8_t c);

/**
 * por cada elemento del buffer llama a `authParserFeed' hasta que
 * el parseo se encuentra completo o se requieren mas bytes.
 *
 * @param errored parametro de salida. si es diferente de NULL se deja dicho
 *   si el parsing se debió a una condición de error
 */
authState helloConsume(authParser * parser, bufferADT readBuffer, bufferADT writeBuffer, bool * errored, char * user);

/**
 * Permite distinguir a quien usa helloParserFeed si debe seguir
 * enviando caracters o no. 
 *
 * En caso de haber terminado permite tambien saber si se debe a un error
 */
bool authIsDone(const authState state, bool * errored);

#endif

