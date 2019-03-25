/**
 * bodyPop3Parser.c -- parser for POPV3 body
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "bodyPop3Parser.h"
#include "errorslib.h"
#include "logger.h"
#include "string.h"


#define MAX_MSG_SIZE 512

static const char * crlfMsg = "\r\n.\r\n";

void bodyPop3ParserInit(bodyPop3Parser * parser) {
    parser->state     = BODY_POP3_MSG;
    parser->lineSize  = 0;
    parser->stateSize = 0;
}

bodyPop3State bodyPop3ParserFeed(bodyPop3Parser * parser, const uint8_t c, bufferADT dest, bool skip) {
    switch(parser->state) {
        case BODY_POP3_MSG:
            if(!skip)
                writeAndProcessAByte(dest, c);
            if(c == crlfMsg[2] && parser->lineSize == 0) {
                parser->state = BODY_POP3_DOT;
                parser->stateSize = 3;
            } else if(c == crlfMsg[0]) 
                parser->stateSize = 1;
            else if(c == crlfMsg[1]) {
                if(parser->stateSize == 1) {
                    parser->lineSize = -1;
                    parser->stateSize = 0;
                    if(skip) {
                        writeAndProcessAByte(dest, crlfMsg[0]);
                        writeAndProcessAByte(dest, crlfMsg[1]);
                    }
                } else if(!skip) {
                    parser->lineSize = -1;
                    parser->stateSize = 0;
                    writeAndProcessAByte(dest, crlfMsg[0]);
                    writeAndProcessAByte(dest, crlfMsg[1]);
                } else
                    parser->state = BODY_POP3_ERROR;
            } else if(skip)
                writeAndProcessAByte(dest, c);
            break;

        case BODY_POP3_DOT:
            if(skip) {
                if(c == crlfMsg[3]) {
                    parser->state = BODY_POP3_CRLF;
                    parser->stateSize = 4;
                } else if(c == crlfMsg[2]) {
                    writeAndProcessAByte(dest, c);
                    parser->state = BODY_POP3_MSG;
                    parser->stateSize = 0;
                } else
                    parser->state = BODY_POP3_ERROR;
            } else {
                parser->state = BODY_POP3_MSG;
                parser->stateSize = 0;
                if(c == crlfMsg[0]) {
                    parser->stateSize = 1;
                } else if(c == crlfMsg[1]) {
                    parser->state = BODY_POP3_ERROR;
                }
                writeAndProcessAByte(dest, crlfMsg[2]);
                writeAndProcessAByte(dest, c);
            }
            break;

        case BODY_POP3_CRLF:
            if(c == crlfMsg[4])
                parser->state = BODY_POP3_DONE;
            else
                parser->state = BODY_POP3_ERROR;
            break;

        case BODY_POP3_DONE:
        case BODY_POP3_ERROR:
            // nada que hacer, nos quedamos en este estado
            break;
        default:
            fail("Body pop3 parser not reconize state: %d", parser->state);
    }
    if(parser->lineSize++ == MAX_MSG_SIZE)
        parser->state = BODY_POP3_ERROR;
    return parser->state;
}

bool bodyPop3IsDone(const bodyPop3State state, bool * errored) {
    bool ret;
    switch (state) {
        case BODY_POP3_ERROR:
            if (0 != errored) {
                *errored = true;
            }
            /* no break */
        case BODY_POP3_DONE:
            ret = true;
            break;
        default:
            ret = false;
            break;
    }
   return ret;
}

bodyPop3State bodyPop3ParserConsume(bodyPop3Parser * parser, bufferADT src, bufferADT dest, bool skip, bool * errored) {
    bodyPop3State state = parser->state;
    *errored = false;
    size_t size;
    getWritePtr(dest, &size);

    while(canProcess(src) && size >= 2) {
        const uint8_t c = processAByte(src);
        readAByte(src);
        state = bodyPop3ParserFeed(parser, c, dest, skip);
        if(bodyPop3IsDone(state, errored)) {
            break;
        } else
            getWritePtr(dest, &size);
    }
    return state;
}

