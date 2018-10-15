/**
 * responseParser.c -- parser for POPV3 response
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "responseParser.h"
#include "errorslib.h"
#include "string.h"


#define MAX_MSG_SIZE 512

static const char * positiveIndicatorMsg        = "+OK";
static const int    positiveIndicatorMsgSize    = 3;
static const char * negativeIndicatorMsg        = "-ERR";
static const int    negativeIndicatorMsgSize    = 4;
static const char * crlfInlineMsg               = "\r\n";
static const int    crlfInlineMsgSize           = 2;
static const char * crlfInlineMsg               = "\r\n";
static const int    crlfInlineMsgSize           = 2;
static const char * crlfMultilineMsg            = "\r\n.\r\n";
static const char * crlfMultilineMsgSize        = 5;


void responseParserInit(responseParser * parser) {
    parser->state     = RESPONSE_INIT;
    parser->lineSize  = 0;
    parser->stateSize = 0;
}

responseState responseParserFeed(responseParser * parser, const uint8_t * ptr, commandStruct * commands, size_t * commandsSize) {
    commandStruct currentCommand = commands[*commandsSize];
    const uint8_t c = *ptr; 

    switch(parser->state) {
        case RESPONSE_INIT:
            if(c == positiveIndicatorMsg[0]) 
                parser->state = RESPONSE_INDICATOR_POS;
            else if(c == negativeIndicatorMsg[0]) 
                parser->state = RESPONSE_INDICATOR_NEG;
            else
                parser->state = RESPONSE_ERROR;
            break;

        case RESPONSE_INDICATOR_POS:
            if(c != positiveIndicatorMsg[parser->lineSize]) 
                parser->state = RESPONSE_ERROR;
            else if(parser->lineSize == positiveIndicatorMsgSize - 1) {
                currentCommand.indicator = true;
                parser->stateSize = 0;
                parser->state = RESPONSE_INDICATOR_MSG;
            }
            break;
        
        case RESPONSE_INDICATOR_NEG:
            if(c != negativeIndicatorMsg[parser->lineSize]) 
                parser->state = RESPONSE_ERROR;
            else if(parser->lineSize == negativeIndicatorMsgSize - 1) {
                currentCommand.indicator = false;
                parser->stateSize = 0;
                parser->state = RESPONSE_INDICATOR_MSG;
            }
            break;

        case RESPONSE_INDICATOR_MSG:
            if(c == crlfInlineMsg[0]) {
                parser->stateSize = 1;
                parser->state = RESPONSE_INLINE_CRLF;
            }
            break;

        case RESPONSE_INLINE_CRLF:
            if(c == crlfInlineMsg[parser->stateSize++]) {
                if(parser->stateSize == crlfInlineMsgSize) {
                    if(currentCommand.isMultiline) {
                        currentCommand.startResponsePtr = ptr + 1;                        currentCommand.startResponsePtr = ptr + 1;
                        currentCommand.responseSize     = 0;
                        parser->state     = RESPONSE_BODY;
                        parser->stateSize = 0;
                    }
                    else {
                        parser->state     = RESPONSE_INIT;
                        parser->lineSize  = 0;
                        parser->stateSize = 0;
                        *commandsSize++;
                    }
                }
            }
            else
                parser->state = RESPONSE_ERROR;
            break;

        case RESPONSE_BODY:
            if(c == crlfMultilineMsg[3]) {
                parser->state     = RESPONSE_MULTILINE_CRLF;
                parser->stateSize = 2;
            } else if(c == crlfMultilineMsg[0])
                parser->stateSize++;
            else if(c == crlfMultilineMsg[1]) {
                if(parser->stateSize == 1) 
                    parser->stateSize = 0;
                else
                    parser->state = RESPONSE_ERROR;
            }
            currentCommand.responseSize++;
            break;
         
        case RESPONSE_MULTILINE_CRLF:
            if(c == crlfMultilineMsg[parser->stateSize++]) {
                if(parser->stateSize == crlfMultilineMsgSize) {
                    parser->state     = RESPONSE_INIT;
                    parser->lineSize  = 0;  //si quiero sacarle el puntito lo hago aca
                    parser->stateSize = 0;
                    *commandsSize++;
                }
            }
            else
                parser->state = RESPONSE_ERROR;
            currentCommand.responseSize++;
            break;
   
        default:
            fail("RESPONSE parser not reconize state: %d", parser->state);
    }
    if(parser->lineSize++ == MAX_MSG_SIZE && !currentCommand.isMultiline)
        parser->state = RESPONSE_ERROR;
    return parser->state;
}

responseState responseConsume(responseParser * parser, bufferADT buffer, commandStruct * commands, size_t * commandsSize, bool * errored) {
    responseState state = parser->state;
    size_t bufferSize;
    uint8_t * ptr = getReadPtr(buffer, &bufferSize);  

    for(size_t i = 0; i < bufferSize; i++) {
        state = responseParserFeed(parser, ptr[i], commands, commandsSize);
        if(state == RESPONSE_ERROR) {
            errored = true;
            break;
        }
    }
    return state;
}

