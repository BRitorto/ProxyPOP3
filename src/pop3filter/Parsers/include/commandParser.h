#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#include "buffer.h"
#include "queue.h"

typedef enum commandType {
    CMD_OTHER     = -1,
    CMD_USER      =  0,
    CMD_PASS      =  1,
    CMD_APOP      =  2,
    CMD_RETR      =  3,
    CMD_LIST      =  4,
    CMD_CAPA      =  5,
    CMD_TOP       =  6,
    CMD_UIDL      =  7,
    CMD_TYPES_QTY =  8,
} commandType;

typedef struct commandStruct {
    commandType  type;
    bool         isMultiline;
    bool         indicator;
    void *       data;
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
    size_t argsQty;
    size_t invalidQty;
    bool   invalidType[CMD_TYPES_QTY];
    commandState state;
    commandStruct currentCommand;
} commandParser;

/** inicializa el parser */
void commandParserInit(commandParser * parser);

/** entrega un byte al parser. retorna true si se llego al final  */
commandState commandParserFeed(commandParser * parser, const uint8_t c, queueADT commands, bool * newCommand);

/**
 * por cada elemento del buffer llama a `commandParserFeed' hasta que
 * el parseo se encuentra completo o se requieren mas bytes.
 *
 * @param errored parametro de salida. si es diferente de NULL se deja dicho
 *   si el parsing se debió a una condición de error
 */
commandState commandParserConsume(commandParser * parser, bufferADT buffer, queueADT commands, bool pipelining, bool * newCommand);

char * getUsername(const commandStruct command);

void deleteCommand(commandStruct * command);

#endif

