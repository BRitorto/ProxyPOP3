#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "parserUtils.h"

static void mayEq(parserEvent * ret, const uint8_t character);

static void eq(parserEvent * ret, const uint8_t character);

static void neq(parserEvent * ret, const uint8_t character);

parserDefinition stringCompareParserUtils(const char * string) {
    const size_t n = strlen(string);

    parserStateTransition ** states   = calloc(n + 2, sizeof(*states));
    size_t * nstates                           = calloc(n + 2, sizeof(*nstates));
    parserStateTransition * transitions= calloc(3 *(n + 2), sizeof(*transitions));
    if(states == NULL || nstates == NULL || transitions == NULL) {
        free(states);
        free(nstates);
        free(transitions);

        struct parserDefinition definition = {
            .startState   = 0,
            .statesCount  = 0,
            .states        = NULL,
            .statesQty      = NULL,
        };
        return definition;
    }

    // estados fijos
    const size_t stEq  = n;
    const size_t stNeq = n + 1;

    for(size_t i = 0; i < n; i++) {
        const size_t dest = (i + 1 == n) ? stEq : i + 1;

        transitions[i * 3 + 0].when        = tolower(string[i]);
        transitions[i * 3 + 0].destination = dest;
        transitions[i * 3 + 0].action1     = i + 1 == n ? eq : mayEq;;
        transitions[i * 3 + 1].when        = toupper(string[i]);
        transitions[i * 3 + 1].destination = dest;
        transitions[i * 3 + 1].action1     = i + 1 == n ? eq : mayEq;;
        transitions[i * 3 + 2].when        = ANY;
        transitions[i * 3 + 2].destination = stNeq;
        transitions[i * 3 + 2].action1     = neq;
        states     [i]                     = transitions + (i * 3 + 0);
        nstates    [i]                     = 3;
    }
    // EQ
    transitions[(n + 0) * 3].when          = ANY;
    transitions[(n + 0) * 3].destination   = stNeq;
    transitions[(n + 0) * 3].action1       = neq;
    states     [(n + 0)]                   = transitions + ((n + 0) * 3 + 0);
    nstates    [(n + 0)]                   = 1;
    // NEQ
    transitions[(n + 1) * 3].when          = ANY;
    transitions[(n + 1) * 3].destination   = stNeq;
    transitions[(n + 1) * 3].action1       = neq;
    states     [(n + 1)]                   = transitions + ((n + 1) * 3 + 0);
    nstates    [(n + 1)]                   = 1;

    struct parserDefinition definition = {
        .startState   = 0,
        .statesCount  = n + 2,
        .states        = (const parserStateTransition **) states,
        .statesQty      = (const size_t *) nstates,
    };

    return definition;
}

void destroyStringCompareParserUtils(const parserDefinition * parserDefinition) {
    free((void *)parserDefinition->states[0]);
    free((void *)parserDefinition->states);
    free((void *)parserDefinition->statesQty);
}


static void mayEq(parserEvent * ret, const uint8_t character) {
    ret->type    = STRING_CMP_EQ;
    ret->n       = 1;
    ret->data[0] = character;
}


static void eq(parserEvent * ret, const uint8_t character) {
    ret->type    = STRING_CMP_EQ;
    ret->n       = 1;
    ret->data[0] = character;
}

static void neq(parserEvent * ret, const uint8_t character) {
    ret->type    = STRING_CMP_NEQ;
    ret->n       = 1;
    ret->data[0] = character;
}


