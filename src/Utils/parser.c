#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "parser.h"

/* CDT del parser */
typedef struct parserCDT {
    /** Tipificación para cada caracter */
    const unsigned     * classes;
    /** Definición de estados */
    const parserDefinition * definition;

    /** Estado actual */
    unsigned            state;

    /** Evento que se retorna */
    parserEvent event1;
    /** Evento que se retorna */
    parserEvent event2;
} parserCDT;

void destroyParser(parserADT parser) {
    if(parser != NULL)
        free(parser);
}

parserADT initializeParser(const unsigned * classes, const parserDefinition * definition) {
    parserADT ret = malloc(sizeof(*ret));
    if(ret != NULL) {
        memset(ret, 0, sizeof(*ret));
        ret->classes = classes;
        ret->definition     = definition;
        ret->state   = definition->startState;
    }
    return ret;
}

void resetParser(parserADT parser) {
    parser->state   = parser->definition->startState;
}

const parserEvent * feedParser(parserADT parser, const uint8_t character) {
    const unsigned type = parser->classes[character];

    parser->event1.next = parser->event2.next = 0;

    const parserStateTransition * state = parser->definition->states[parser->state];
    const size_t n                      = parser->definition->statesQty[parser->state];
    bool matched   = false;

    for(unsigned i = 0; i < n ; i++) {
        const int when = state[i].when;
        if (state[i].when <= 0xFF) {
            matched = (character == when);
        } else if(state[i].when == ANY) {
            matched = true;
        } else if(state[i].when > 0xFF) {
            matched = (type & when);
        } else {
            matched = false;
        }

        if(matched) {
            state[i].action1(&parser->event1, character);
            if(state[i].action2 != NULL) {
                parser->event1.next = &parser->event2;
                state[i].action2(&parser->event2, character);
            }
            parser->state = state[i].destination;
            break;
        }
    }
    return &parser->event1;
}


static const unsigned classes[0xFF] = {0x00};

const unsigned * noClassesParser(void) {
    return classes;
}


