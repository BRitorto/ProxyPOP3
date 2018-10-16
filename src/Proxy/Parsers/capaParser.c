/**
 * capabilitiesParser.c -- parser del comando CAPA de POPV3
 */
#include <stdio.h>
#include <stdlib.h>

#include "capaParser.h"
#include "errorslib.h"

static const char * positiveIndicatorMsg = "+OK";
static const size_t positiveIndicatorMsgSize = 3;

static const char * pipeliningMsg = "PIPELINING\r\n";
static const size_t pipeliningMsgSize = 12;

static const char * crlfMsg = "\r\n.\r\n";
static const size_t crlfMsgSize = 5;

void capaParserInit(capaParser * parser, capabilities * capas) {
    parser->state                   = CAPA_PARSE_INDICATOR;
    parser->current.indicatorSize   = 0;
    parser->capas                   = capas;
    capas->pipeliningStatus         = CAPA_NOT_AVAILABLE;
}

capaState capaParserFeed(capaParser * parser, uint8_t c) {
    switch(parser->state) {
        case CAPA_PARSE_INDICATOR:
            if(c != positiveIndicatorMsg[parser->current.indicatorSize])
                parser->state = CAPA_PARSE_ERROR;
            else if(parser->current.indicatorSize == positiveIndicatorMsgSize - 1)
                parser->state = CAPA_PARSE_MSG;
            else
                parser->current.indicatorSize++;
            break;

        case CAPA_PARSE_MSG:
            if(c == crlfMsg[0]) {
                parser->state = CAPA_PARSE_CRLF;
                parser->current.crlfSize = 1;
            }
            else if(c == pipeliningMsg[0]) {
                parser->state = CAPA_PARSE_PIPELINING;
                parser->current.msgPipeliningSize = 1;
            }
            break;

        case CAPA_PARSE_PIPELINING:
            if(c != pipeliningMsg[parser->current.msgPipeliningSize])
                parser->state = CAPA_PARSE_MSG;
            else if(parser->current.msgPipeliningSize == pipeliningMsgSize - 1) {
                parser->state                   = CAPA_PARSE_CRLF;
                parser->capas->pipeliningStatus = CAPA_AVAILABLE;
                parser->current.crlfSize        = 2;
            }
            else
                parser->current.msgPipeliningSize++;
            break;

        case CAPA_PARSE_CRLF:
            if(c != crlfMsg[parser->current.crlfSize] && parser->current.crlfSize == 2) {
                if(c == pipeliningMsg[0]) {
                    parser->state = CAPA_PARSE_PIPELINING;
                    parser->current.msgPipeliningSize = 1;
                }
                else
                    parser->state = CAPA_PARSE_MSG;
            }
            else if(c != crlfMsg[parser->current.crlfSize])
                parser->state = CAPA_PARSE_ERROR;
            else if(parser->current.crlfSize == crlfMsgSize - 1)
                parser->state = CAPA_PARSE_DONE;
            else
                parser->current.crlfSize++;
            break;

        case CAPA_PARSE_DONE:
        case CAPA_PARSE_ERROR:
            // nada que hacer, nos quedamos en este estado
            break;
        default:
            fail("Capa parser not reconize state: %d", parser->state);
    }
    return parser->state;
}

bool capaParserIsDone(const capaState state, bool * errored) {
    bool ret;
    switch (state) {
        case CAPA_PARSE_ERROR:
            if (0 != errored) {
                *errored = true;
            }
            /* no break */
        case CAPA_PARSE_DONE:
            ret = true;
            break;
        default:
            ret = false;
            break;
    }
   return ret;
}

capaState capaParserConsume(capaParser * parser, bufferADT readBuffer, bool * errored) {
    capaState state = parser->state;

    while(canRead(readBuffer)) {
        const uint8_t c = readAByte(readBuffer);
        state = capaParserFeed(parser, c);
        if (capaParserIsDone(state, errored)) {
            break;
        }
    }
    return state;
}

