#ifndef CAPA_PARSER_H
#define CAPA_PARSER_H

#include "buffer.h"

typedef enum capaState {
    CAPA_PARSE_INDICATOR  = 0,
    CAPA_PARSE_MSG        = 1,
    CAPA_PARSE_PIPELINING = 2,
    CAPA_PARSE_CRLF       = 3,
    CAPA_PARSE_DONE       = 4,
    CAPA_PARSE_ERROR      = 5,
} capaState;

typedef enum capabilitieStatus {
    CAPA_NO_CHECKED    = 0,
    CAPA_AVAILABLE     = 1,
    CAPA_NOT_AVAILABLE = 2,
} capabilitieStatus;

typedef struct capabilities {
    capabilitieStatus pipeliningStatus;
} capabilities;

typedef struct capaParser {
    /** permite al usuario del parser almacenar sus datos */
    size_t msgSize;
    size_t msgPipeliningSize;    
    size_t crlfSize;
    capabilities * capas;
    /******** zona privada *****************/
    capaState state;
} capaParser;

/** inicializa el parser */
void capaParserInit(capaParser * parser, capabilities * capas);

/** entrega un byte al parser. retorna true si se llego al final  */
capaState capaParserFeed(capaParser * parser, uint8_t c);

/**
 * por cada elemento del buffer llama a `capaParserFeed' hasta que
 * el parseo se encuentra completo o se requieren mas bytes.
 *
 * @param errored parametro de salida. si es diferente de NULL se deja dicho
 *   si el parsing se debió a una condición de error
 */
capaState capaParserConsume(capaParser * parser, bufferADT readBuffer, bool * errored);

/**
 * Permite distinguir a quien usa helloParserFeed si debe seguir
 * enviando caracters o no. 
 *
 * En caso de haber terminado permite tambien saber si se debe a un error
 */
bool capaParserIsDone(const capaState state, bool * errored);


#endif

