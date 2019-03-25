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
#include "logger.h"


#define MAX_MSG_SIZE 512

static const char * positiveIndicatorMsg        = "+OK";
static const int    positiveIndicatorMsgSize    = 3;
static const char * negativeIndicatorMsg        = "-ERR";
static const int    negativeIndicatorMsgSize    = 4;
static const char * crlfInlineMsg               = "\r\n";
static const int    crlfInlineMsgSize           = 2;
static const char * crlfMultilineMsg            = "\r\n.\r\n";
static const int    crlfMultilineMsgSize        = 5;


void responseParserInit(responseParser * parser) {
    parser->state     = RESPONSE_INIT;
    parser->lineSize  = 0;
    parser->stateSize = 0;
    parser->commandInterest = CMD_RETR;
}

responseState responseParserFeed(responseParser * parser, const uint8_t c, queueADT commands) {
    commandStruct * currentCommand = peekProcessed(commands);
    if(currentCommand == NULL)
        parser->state = RESPONSE_ERROR;

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
                currentCommand->indicator = true;
                parser->stateSize = 0;
                parser->state = RESPONSE_INDICATOR_MSG;
            }
            break;
        
        case RESPONSE_INDICATOR_NEG:
            if(c != negativeIndicatorMsg[parser->lineSize]) 
                parser->state = RESPONSE_ERROR;
            else if(parser->lineSize == negativeIndicatorMsgSize - 1) {
                currentCommand->indicator = false;
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
                    parser->lineSize     = -1;
                    parser->stateSize    =  2;
                    if(currentCommand->indicator && currentCommand->type == parser->commandInterest && currentCommand->isMultiline)
                        parser->state    = RESPONSE_INTEREST;
                    else if(currentCommand->indicator && currentCommand->isMultiline)
                        parser->state    = RESPONSE_BODY;                
                    else {
                        parser->stateSize    =  0;
                        parser->state    = RESPONSE_INIT;
                        processQueue(commands);
                    }
                }
            } else
                parser->state = RESPONSE_ERROR;
            break;

        case RESPONSE_BODY:
            if(c == crlfMultilineMsg[2] && parser->lineSize == 0) {
                parser->state     = RESPONSE_MULTILINE_CRLF;
                parser->stateSize = 3;
            } else if(c == crlfMultilineMsg[0])
                parser->stateSize = 1;
            else if(c == crlfMultilineMsg[1]) {
                if(parser->stateSize == 1) {
                    parser->lineSize  = -1;
                    parser->stateSize = 0;
                } else
                    parser->state = RESPONSE_ERROR;
            }
            break;
         
        case RESPONSE_MULTILINE_CRLF:
            if(c == crlfMultilineMsg[parser->stateSize++]) {
                if(parser->stateSize == crlfMultilineMsgSize) {
                    parser->state     = RESPONSE_INIT;
                    parser->lineSize  = -1;  
                    parser->stateSize = 0;
                    processQueue(commands);
                }
            } else if(parser->stateSize == crlfMultilineMsgSize - 1) {            
                parser->state     = RESPONSE_BODY;
                parser->stateSize = 0;
            } else
                parser->state = RESPONSE_ERROR;
            break;

        case RESPONSE_INTEREST:
            parser->state = RESPONSE_BODY;
            if(c == crlfMultilineMsg[2] && parser->lineSize == 0) {
                parser->state     = RESPONSE_MULTILINE_CRLF;
                parser->stateSize = 3;
            } else if(c == crlfMultilineMsg[0])
                parser->stateSize = 1;
            else if(c == crlfMultilineMsg[1]) {
                if(parser->stateSize == 1) {
                    parser->lineSize  = -1;
                    parser->stateSize = 0;
                } else
                    parser->state = RESPONSE_ERROR;
            }
            break;

        case RESPONSE_ERROR:
            break;

        default:
            fail("Response parser not reconize state: %d", parser->state);
    }
    if(parser->lineSize++ == MAX_MSG_SIZE)
        parser->state = RESPONSE_ERROR;
    return parser->state;
}

responseState responseParserConsume(responseParser * parser, bufferADT buffer, queueADT commands, bool * errored) {
    responseState state = parser->state;
    *errored = false;

    while(canProcess(buffer)) {
        const uint8_t c = processAByte(buffer);
        state = responseParserFeed(parser, c, commands);
        if(state == RESPONSE_ERROR) {
            *errored = true;
            break;
        }
    }
    return state;
}

responseState responseParserConsumeUntil(responseParser * parser, bufferADT buffer, queueADT commands, bool interested, bool toNewCommand, bool * errored) {
    responseState state = parser->state;
    *errored = false;
    if(toNewCommand && state == RESPONSE_INIT)
        return state;

    while(canProcess(buffer)) {
        const uint8_t c = processAByte(buffer);
        state = responseParserFeed(parser, c, commands);
        if(state == RESPONSE_ERROR) {
            *errored = true;
            break;
        } else if((state == RESPONSE_INTEREST && interested) || (state == RESPONSE_INIT && toNewCommand))
            break;
    }
    return state;
}

