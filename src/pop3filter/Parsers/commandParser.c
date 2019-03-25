/**
 * commandParser.c -- parser for POPV3 commands
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "commandParser.h"
#include "errorslib.h"
#include "logger.h"


#define MAX_MSG_SIZE 512
#define MAX_ARG_SIZE 40

typedef struct commandProps {
    commandType type;
    char *      name;
    size_t      length;
    size_t      argsQtyMin;
    size_t      argsQtyMax;
} commandProps;

#define IS_MULTILINE(command, argsQty) (command->type == CMD_CAPA       \
                ||  (command->type == CMD_LIST && argsQty == 0)         \
                ||  (command->type == CMD_TOP  && argsQty == 2)         \
                ||  (command->type == CMD_RETR && argsQty == 1)         \
                ||  (command->type == CMD_UIDL && argsQty == 0))        


static const commandProps commandTable[] = {
    {
        .type = CMD_USER, .name = "USER", .length = 4, .argsQtyMin = 1, .argsQtyMax = 512 - 7,  //un user puede contener espacios
    } , {
        .type = CMD_PASS, .name = "PASS", .length = 4, .argsQtyMin = 1, .argsQtyMax = 512 - 7,
    } , {        
        .type = CMD_APOP, .name = "APOP", .length = 4, .argsQtyMin = 2, .argsQtyMax = 512 - 7,
    } , {
        .type = CMD_RETR, .name = "RETR", .length = 4, .argsQtyMin = 1, .argsQtyMax = 1,
    } , {
        .type = CMD_LIST, .name = "LIST", .length = 4, .argsQtyMin = 0, .argsQtyMax = 1,
    } , {
        .type = CMD_CAPA, .name = "CAPA", .length = 4, .argsQtyMin = 0, .argsQtyMax = 512 - 7,
    } , {
        .type = CMD_TOP,  .name = "TOP" , .length = 3, .argsQtyMin = 2, .argsQtyMax = 2,
    } , {
        .type = CMD_UIDL, .name = "UIDL", .length = 4, .argsQtyMin = 0, .argsQtyMax = 1,
    }
};

static const char * crlfMsg     = "\r\n";
static const int    crlfMsgSize = 2;

static void initializeCommand(commandStruct * command);

static void hanndleCommandParsed(commandStruct * currentCommand, commandParser * parser, queueADT commands, bool * newCommand, bool notMatch);


void commandParserInit(commandParser * parser) {
    parser->state          = COMMAND_TYPE;
    parser->lineSize       = 0;
    parser->stateSize      = 0;
    parser->argsQty        = 0;
}

commandState commandParserFeed(commandParser * parser, const uint8_t c, queueADT commands, bool * newCommand) {
    commandStruct * currentCommand = &parser->currentCommand;

    if(parser->lineSize == 0) {
        initializeCommand(currentCommand);
        parser->argsQty    = 0;
        parser->invalidQty = 0;
        for(int i = 0; i < CMD_TYPES_QTY; i++) 
            parser->invalidType[i] = false;
    }

    switch(parser->state) {
        case COMMAND_TYPE: 
            if(c != crlfMsg[1]) {
                for(size_t i = 0; i < CMD_TYPES_QTY; i++) {
                    if(!parser->invalidType[i]) {
                        if(toupper(c) != commandTable[i].name[parser->lineSize]) {
                            parser->invalidType[i] = true;
                            parser->invalidQty++;
                        } else if(parser->lineSize == commandTable[i].length-1) {
                            currentCommand->type = commandTable[i].type;
                            parser->stateSize = 0;
                            if(commandTable[i].argsQtyMax > 0) {
                                if(currentCommand->type == CMD_USER || currentCommand->type == CMD_APOP)
                                    currentCommand->data = malloc((MAX_ARG_SIZE + 1) * sizeof(uint8_t));    //NULL TERMINATED
                                parser->state = COMMAND_ARGS;
                            } else
                                parser->state = COMMAND_CRLF;
                            break;
                        }
                    }
                    if(parser->invalidQty == CMD_TYPES_QTY) 
                        parser->state = COMMAND_ERROR;
                }
            } else 
                hanndleCommandParsed(currentCommand, parser, commands, newCommand, true);
            break;

        case COMMAND_ARGS:
            if(c == ' ') {
                if(parser->argsQty == commandTable[currentCommand->type].argsQtyMax) 
                    parser->state = COMMAND_ERROR;
                else if(parser->stateSize == 0) 
                    parser->stateSize++;
                else if(parser->stateSize > 1 && parser->argsQty < commandTable[currentCommand->type].argsQtyMax) {
                    parser->stateSize = 1;
                    parser->argsQty++;
                }
            } else if(c != crlfMsg[0] && c != crlfMsg[1]) {
                if(parser->stateSize == 0)
                    parser->state = COMMAND_ERROR;
                else {
                    if(parser->argsQty == 0 && (currentCommand->type == CMD_USER || currentCommand->type == CMD_APOP)) 
                        ((uint8_t *)currentCommand->data)[parser->stateSize-1] = c;
                    parser->stateSize++;
                }
            } else if(c == crlfMsg[0]) {
                if(parser->argsQty == 0 && (currentCommand->type == CMD_USER || currentCommand->type == CMD_APOP))
                        ((uint8_t *)currentCommand->data)[parser->stateSize-1] = 0;     //username null terminated
                if(parser->stateSize > 1)
                    parser->argsQty++;
                if(commandTable[currentCommand->type].argsQtyMin <= parser->argsQty && parser->argsQty <= commandTable[currentCommand->type].argsQtyMax) {
                    parser->state     = COMMAND_CRLF;
                    parser->stateSize = 1;
                } else
                    parser->state     = COMMAND_ERROR;
            } else if(c == crlfMsg[1]) {  
                if(parser->argsQty == 0 && (currentCommand->type == CMD_USER || currentCommand->type == CMD_APOP))
                    ((uint8_t *)currentCommand->data)[parser->stateSize-1] = 0;     //username null terminated
                if(parser->stateSize > 1)
                    parser->argsQty++;
                if(commandTable[currentCommand->type].argsQtyMin <= parser->argsQty && parser->argsQty <= commandTable[currentCommand->type].argsQtyMax) {
                    hanndleCommandParsed(currentCommand, parser, commands, newCommand, false);
                } else 
                    hanndleCommandParsed(currentCommand, parser, commands, newCommand, true);            
            } else
                parser->state = COMMAND_ERROR;
            break;

        case COMMAND_CRLF:
            if(c == crlfMsg[parser->stateSize]) {
                if(parser->stateSize == crlfMsgSize - 1) {       
                    hanndleCommandParsed(currentCommand, parser, commands, newCommand, false);
                }
            } else {
                parser->state = COMMAND_ERROR;
            }
            break;

        case COMMAND_ERROR:
            if(c == crlfMsg[1]) 
                hanndleCommandParsed(currentCommand, parser, commands, newCommand, true);
            break;
        default:
            fail("Command parser not reconize state: %d", parser->state);
    }
    if(parser->lineSize++ == MAX_MSG_SIZE || (parser->state == COMMAND_ARGS && parser->stateSize == MAX_ARG_SIZE))
        parser->state = COMMAND_ERROR;
    return parser->state;
}

commandState commandParserConsume(commandParser * parser, bufferADT buffer, queueADT commands, bool pipelining, bool * newCommand) {
    commandState state = parser->state;

    while(canProcess(buffer)) {
        const uint8_t c = processAByte(buffer);
        state = commandParserFeed(parser, c, commands, newCommand);
        if(!pipelining && *newCommand) {
            break;
        }
    }
    return state;
}

char * getUsername(const commandStruct command) {
    if(command.type == CMD_APOP || command.type == CMD_USER)
        return (char * ) command.data;
    return NULL;
}

void deleteCommand(commandStruct * command) {
    if(command == NULL)
        return;
    if(command->data != NULL)
        free(command->data);
    free(command);
}

static void initializeCommand(commandStruct * command) {
    command->type = CMD_OTHER;
    command->indicator = false;
    command->data = NULL;
}

static void hanndleCommandParsed(commandStruct * currentCommand, commandParser * parser, queueADT commands, bool * newCommand, bool notMatch) {
    commandStruct * offerCommand = malloc(sizeof(commandStruct));
    if(notMatch) {
        currentCommand->type = CMD_OTHER;
        if(currentCommand->data != NULL) {
            free(currentCommand->data);
            currentCommand->data = NULL; 
        }  
    }   

    currentCommand->isMultiline = IS_MULTILINE(currentCommand, parser->argsQty);
    memcpy(offerCommand, currentCommand, sizeof(commandStruct));
    offer(commands, offerCommand);
    
    parser->state     = COMMAND_TYPE;
    parser->lineSize  = -1;
    parser->stateSize =  0;
    *newCommand = true;
}

