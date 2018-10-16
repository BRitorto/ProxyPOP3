/**
 * helloParser.c -- parser del hello de POPV3
 */
#include <stdio.h>
#include <stdlib.h>

#include "helloParser.h"
#include "errorslib.h"

#define HELLO_MAX_MSG_SIZE 512            //RFC max response = 512 bytes

static const char * positiveIndicatorMsg = "+OK";
static const size_t positiveIndicatorMsgSize = 3;

static const char * crlfMsg = "\r\n";

void helloParserInit(helloParser * parser) {
    parser->state     = HELLO_INDICATOR;
    parser->msgSize   = 0;
}

helloState helloParserFeed(helloParser * parser, uint8_t c) {
    switch(parser->state) {
        case HELLO_INDICATOR:
            if(c != positiveIndicatorMsg[parser->msgSize])
                parser->state = HELLO_ERROR;
            else if(parser->msgSize == positiveIndicatorMsgSize - 1)
                parser->state = HELLO_MSG;
            break;

        case HELLO_MSG:
            if(c == crlfMsg[0])
                parser->state = HELLO_CRLF;
            break;

        case HELLO_CRLF:
            if(c == crlfMsg[1])
                parser->state = HELLO_DONE;
            else
                parser->state = HELLO_ERROR;
            break;

        case HELLO_DONE:
        case HELLO_ERROR:
            // nada que hacer, nos quedamos en este estado
            break;
        default:
            fail("Hello parser not reconize state: %d", parser->state);
    }
    if(parser->msgSize++ == HELLO_MAX_MSG_SIZE)
        parser->state = HELLO_ERROR;
    return parser->state;
}

bool helloIsDone(const helloState state, bool * errored) {
    bool ret;
    switch (state) {
        case HELLO_ERROR:
            if (0 != errored) {
                *errored = true;
            }
            /* no break */
        case HELLO_DONE:
            ret = true;
            break;
        default:
            ret = false;
            break;
    }
   return ret;
}

helloState helloConsume(helloParser * parser, bufferADT readBuffer, 
                        bufferADT writeBuffer, bool * errored) {
    helloState state = parser->state;

    while(canRead(readBuffer) && canWrite(writeBuffer)) {
        const uint8_t c = readAByte(readBuffer);
        state = helloParserFeed(parser, c);
        writeAByte(writeBuffer, c);
        if (helloIsDone(state, errored)) {
            break;
        }
    }
    return state;
}

