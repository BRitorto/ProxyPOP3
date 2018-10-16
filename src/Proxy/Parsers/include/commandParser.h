#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#include "buffer.h"

typedef enum commandType {
    CMD_OTHER     = -1,
    CMD_USER      =  0,
    CMD_PASS      =  1,
    CMD_APOP      =  2,
    CMD_RETR      =  3,
    CMD_LIST      =  4,
    CMD_CAPA      =  5,
    CMD_TYPES_QTY =  6,
} commandType;

typedef struct commandStruct {
    commandType  type;
    size_t       argsQty;
    bool         isMultiline;
    bool         indicator;
    char *       startCommandPtr;
    char *       startResponsePtr;
    size_t       responseSize;
    bool         isResponseComplete;
} commandStruct;

typedef enum commandState {
    COMMAND_TYPE,
    COMMAND_ARGS,
    COMMAND_CRLF,
    COMMAND_ERROR,
} commandState;

typedef struct commandParser {
    size_t lineSize;    
    size_t stateSize;
    size_t invalidQty;
    bool   invalidType[CMD_TYPES_QTY];
    commandState state;
} commandParser;

/** inicializa el parser */
void commandParserInit(commandParser * parser);

/** entrega un byte al parser. retorna true si se llego al final  */
commandState commandParserFeed(commandParser * parser, const uint8_t * ptr, commandStruct * commands, size_t * commandsSize);

/**
 * por cada elemento del buffer llama a `commandParserFeed' hasta que
 * el parseo se encuentra completo o se requieren mas bytes.
 *
 * @param errored parametro de salida. si es diferente de NULL se deja dicho
 *   si el parsing se debió a una condición de error
 */
commandState commandParserConsume(commandParser * parser, bufferADT buffer, commandStruct * commands, size_t * commandsSize);

char * getUsername(const commandStruct command);

#endif

