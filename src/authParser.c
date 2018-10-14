/**
 * authParser.c -- parser del USER de POPV3
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "authParser.h"
#include "errorslib.h"

#define MAX_MSG_SIZE 512

static const char * userIndicatorMsg = "USER";
static const char * apopIndicatorMsg = "APOP";
static const size_t indicatorMsgSize = 4;


static const char * crlfMsg = "\r\n";

void authParserInit(authParser * parser) {
    parser->state     = AUTH_INITIAL;
    parser->msgSize   = 0;
}

authState authParserFeed(authParser * parser, uint8_t c, bool * apop) {
    switch(parser->state) {
        case AUTH_INITIAL:
            if(c == 'U' || c == 'u')
                parser->state = AUTH_USER_INDICATOR;
            else if (c == 'A' || c == 'a') {
                parser->state = AUTH_APOP_INDICATOR;
                *apop = true;
            }
            else
                parser->state = AUTH_ERROR;
            break;
        case AUTH_USER_INDICATOR:
            if(c != userIndicatorMsg[parser->msgSize])
                parser->state = AUTH_ERROR;
            else if(parser->msgSize == indicatorMsgSize) {
                if(c == ' ')
                    parser->state = AUTH_USER;
                else
                    parser->state = AUTH_ERROR;
            }
            break;
        case AUTH_APOP_INDICATOR:
            if(c != apopIndicatorMsg[parser->msgSize])
                parser->state = AUTH_ERROR;
            else if(parser->msgSize == indicatorMsgSize) {
                if(c == ' ')
                    parser->state = AUTH_USER;
                else
                    parser->state = AUTH_ERROR;
            }
            break;
        case AUTH_USER:
            if(c == crlfMsg[0])
                parser->state = AUTH_CRLF;
            else if (c == ' ' && *apop)
                parser->state = AUTH_DIGEST;
            break;

        case AUTH_CRLF:
            if(c == crlfMsg[1])
                parser->state = AUTH_DONE;
            else
                parser->state = AUTH_ERROR;
            break;
        case AUTH_DIGEST:
            if(c == crlfMsg[0])
                parser->state = AUTH_CRLF;
            break;
        case AUTH_DONE:
        case AUTH_ERROR:
            // nada que hacer, nos quedamos en este estado
            break;
        default:
            fail("Auth parser not reconize state: %d", parser->state);
    }
    if(parser->msgSize++ == MAX_MSG_SIZE)
        parser->state = AUTH_ERROR;
    return parser->state;
}

bool authIsDone(const authState state, bool * errored) {
    bool ret;
    switch (state) {
        case AUTH_ERROR:
            if (0 != errored) {
                *errored = true;
            }
            /* no break */
        case AUTH_DONE:
            ret = true;
            break;
        default:
            ret = false;
            break;
    }
   return ret;
}

authState authConsume(authParser * parser, bufferADT readBuffer, 
                        bufferADT writeBuffer, bool * errored, char * user) {
    authState state = parser->state;
    char currentUser[MAX_MSG_SIZE - indicatorMsgSize - 2];
    int i = 0;
    bool apop = false;

    while(canRead(readBuffer) && canWrite(writeBuffer)) {
        const uint8_t c = readAByte(readBuffer);
        state = authParserFeed(parser, c, &apop);
        if(state == AUTH_USER)
            currentUser[i++] = c;
        writeAByte(writeBuffer, c);
        if (authIsDone(state, errored)) {
            break;
        }
    }
    if(i == 0) {
        user = NULL;
        return state;
    }
    currentUser[i] = 0;
    memcpy(user, currentUser, i+1);
    return state;
}

