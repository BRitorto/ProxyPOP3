#include "parser.h"
#include "mimeCharacters.h"
#include "mimeMessage.h"

/**
 * RFC822 Messages
 */
enum state {
    NAME0,
    NAME,
    VALUE,
    VALUE_CR,
    VALUE_CRLF,
    FOLD,
    VALUE_CRLF_CR,
    BODY,
    BODY_CR,
    ERROR,
};

///////////////////////////////////////////////////////////////////////////////
// Acciones

static void name(parserEvent * ret, const uint8_t character) {
    ret->type    = MIME_MSG_NAME;
    ret->n       = 1;
    ret->data[0] = character;
}

static void nameEnd(parserEvent * ret, const uint8_t character) {
    ret->type    = MIME_MSG_NAME_END;
    ret->n       = 1;
    ret->data[0] = ':';
}

static void value(parserEvent * ret, const uint8_t character) {
    ret->type    = MIME_MSG_VALUE;
    ret->n       = 1;
    ret->data[0] = character;
}

static void valueCR(parserEvent * ret, const uint8_t character) {
    ret->type    = MIME_MSG_VALUE;
    ret->n       = 1;
    ret->data[0] = '\r';
}

static void valueFoldCRLF(parserEvent * ret, const uint8_t character) {
    ret->type    = MIME_MSG_VALUE_FOLD;
    ret->n       = 2;
    ret->data[0] = '\r';
    ret->data[1] = '\n';
}

static void valueFold(parserEvent * ret, const uint8_t character) {
    ret->type    = MIME_MSG_VALUE_FOLD;
    ret->n       = 1;
    ret->data[0] = character;
}

static void valueEnd(parserEvent * ret, const uint8_t character) {
    ret->type    = MIME_MSG_VALUE_END;
    ret->n       = 2;
    ret->data[0] = '\r';
    ret->data[1] = '\n';
}

static void wait(parserEvent * ret, const uint8_t character) {
    ret->type    = MIME_MSG_WAIT;
    ret->n       = 0;
}

static void bodyStart(parserEvent * ret, const uint8_t character) {
    ret->type    = MIME_MSG_BODY_START;
    ret->n       = 2;
    ret->data[0] = '\r';
    ret->data[1] = '\n';
}

static void body(parserEvent * ret, const uint8_t character) {
    ret->type    = MIME_MSG_BODY;
    ret->n       = 1;
    ret->data[0] = character;
}

static void unexpected(parserEvent * ret, const uint8_t character) {
    ret->type    = MIME_MSG_UNEXPECTED;
    ret->n       = 1;
    ret->data[0] = character;
}

static void bodyNewLine(parserEvent * ret, const uint8_t character) {
    ret->type   = MIME_MSG_BODY_NEWLINE;
    ret->n      = 1;
    ret->data[0]= character;
}

static void bodyCRLF(parserEvent * ret, const uint8_t character) {
    ret->type   = MIME_MSG_BODY_CR;
    ret->n      = 1;
    ret->data[0]= character;
}

///////////////////////////////////////////////////////////////////////////////
// Transiciones

static const parserStateTransition ST_NAME0[] =  {
    {.when = ':',        .destination = ERROR,         .action1 = unexpected,},
    {.when = ' ',        .destination = ERROR,         .action1 = unexpected,},
    {.when = TOKEN_CTL,  .destination = ERROR,         .action1 = unexpected,},
    {.when = TOKEN_CHAR, .destination = NAME,          .action1 = name,      },
    {.when = ANY,        .destination = ERROR,         .action1 = unexpected,},
};

static const parserStateTransition ST_NAME[] =  {
    {.when = ':',           .destination = VALUE,          .action1 = nameEnd,  },
    {.when = ' ',           .destination = ERROR,          .action1 = unexpected,},
    {.when = TOKEN_CTL,     .destination = ERROR,          .action1 = unexpected,},
    {.when = TOKEN_CHAR,    .destination = NAME,           .action1 = name,      },
    {.when = ANY,           .destination = ERROR,          .action1 = unexpected,},
};

static const parserStateTransition ST_VALUE[] =  {
    {.when = TOKEN_LWSP,    .destination = FOLD,           .action1 = valueFold,},
    {.when = '\r',          .destination = VALUE_CR,       .action1 = wait,      },
    {.when = ANY,           .destination = VALUE,          .action1 = value,     },
};

static const parserStateTransition ST_VALUE_CR[] =  {
    {.when = '\n',          .destination = VALUE_CRLF,     .action1 = wait,      },
    {.when = ANY,           .destination = VALUE,          .action1 = valueCR, .action2 = value,},
};

static const parserStateTransition ST_VALUE_CRLF[] =  {
    {.when = ':',           .destination = ERROR,          .action1 = unexpected,},
    {.when = '\r',          .destination = VALUE_CRLF_CR,  .action1 = wait,},
    {.when = TOKEN_LWSP,    .destination = FOLD,           .action1 = valueFoldCRLF, .action2 = valueFold,},
    {.when = TOKEN_CTL,     .destination = ERROR,          .action1 = valueEnd, .action2 = unexpected,},
    {.when = TOKEN_CHAR,    .destination = NAME,           .action1 = valueEnd, .action2 = name,      },
    {.when = ANY,           .destination = ERROR,          .action1 = unexpected,},
};

static const parserStateTransition ST_FOLD[] =  {
    {.when = TOKEN_LWSP,    .destination = FOLD,           .action1 = valueFold,},
    {.when = ANY,           .destination = VALUE,          .action1 = value,    },
};

static const parserStateTransition ST_VALUE_CRLF_CR[] =  {
    {.when = '\n',          .destination = BODY,           .action1 = valueEnd, .action2 = bodyStart,},
    {.when = ANY,           .destination = ERROR,          .action1 = valueEnd, .action2 = unexpected,},
};

static const parserStateTransition ST_BODY[] =  {
    {.when = '\r',          .destination = BODY_CR,        .action1 = bodyCRLF,},
    {.when = ANY,           .destination = BODY,           .action1 = body,},
};

static const parserStateTransition ST_BODY_CR[] = {
    {.when = '\n',          .destination = BODY,           .action1 = bodyNewLine,},
    {.when = ANY,           .destination = ERROR,          .action1 = unexpected,},
};

static const parserStateTransition ST_ERROR[] =  {
    {.when = ANY,           .destination = ERROR,          .action1 = unexpected,},
};

///////////////////////////////////////////////////////////////////////////////
// Declaración formal

static const parserStateTransition * states [] = {
    ST_NAME0,
    ST_NAME,
    ST_VALUE,
    ST_VALUE_CR,
    ST_VALUE_CRLF,
    ST_FOLD,
    ST_VALUE_CRLF_CR,
    ST_BODY,
    ST_BODY_CR,
    ST_ERROR,
};

#define N(x) (sizeof(x)/sizeof((x)[0]))

static const size_t statesQty [] = {
    N(ST_NAME0),
    N(ST_NAME),
    N(ST_VALUE),
    N(ST_VALUE_CR),
    N(ST_VALUE_CRLF),
    N(ST_FOLD),
    N(ST_VALUE_CRLF_CR),
    N(ST_BODY),
    N(ST_BODY_CR),
    N(ST_ERROR),
};

static parserDefinition definition = {
    .statesCount    = N(states),
    .states         = states,
    .statesQty      = statesQty,
    .startState     = NAME0,
};

const parserDefinition * mimeMessageParser(void) {
    return &definition;
}

