#include "mediaType.h"
#include "parser.h"
#include "mimeCharacters.h"


/**
 * MEDIA-TYPE:
 *
 * type/subtype ; argument="value"
 *
 */

 enum state {
	TYPE0,
    TYPE,
    SUBTYPE,
    ARGUMENT,
    VALUE_START,
    VALUE,
    VALUE_END,
    ERROR,
};

//////////////////////////////////////////////////////////////////////////////
// Acciones

static void type(parserEvent * ret, const uint8_t character) {
    ret->type    = MEDIA_TYPE_TYPE;
    ret->n       = 1;
    ret->data[0] = character;
}

static void typeEnd(parserEvent * ret,const uint8_t character) {
    ret->type    = MEDIA_TYPE_TYPE_END;
    ret->n       = 1;
	ret->data[0] = '/';
}

static void subtype(parserEvent * ret, const uint8_t character) {
    ret->type    = MEDIA_TYPE_SUBTYPE;
    ret->n       = 1;
    ret->data[0] = character;
}

static void subtypeEnd(parserEvent * ret, const uint8_t character) {
    ret->type    = MEDIA_TYPE_SUBTYPE_END;
    ret->n       = 1;
    ret->data[0] = ';';
}

static void argument(parserEvent * ret, const uint8_t character) {
    ret->type    = MEDIA_TYPE_ARGUMENT;
    ret->n       = 1;
    ret->data[0] = character;
}

static void argumentEnd(parserEvent * ret, const uint8_t character) {
    ret->type    = MEDIA_TYPE_ARGUMENT_END;
    ret->n       = 1;
    ret->data[0] = '=';
}

static void valueStart(parserEvent * ret, const uint8_t character) {
    ret->type    = MEDIA_TYPE_VALUE_START;
    ret->n       = 1;
    ret->data[0] = '\"';
}

static void value(parserEvent * ret, const uint8_t character) {
    ret->type    = MEDIA_TYPE_VALUE;
    ret->n       = 1;
    ret->data[0] = character;
}

static void valueEnd(parserEvent * ret, const uint8_t character) {
    ret->type 	 = MEDIA_TYPE_VALUE_END;
    ret->n    	 = 1;
    ret->data[0] = character;
}

static void unexpected(parserEvent * ret, const uint8_t character) {
	ret->type    = MEDIA_TYPE_UNEXPECTED;
    ret->n       = 1;
    ret->data[0] = character;	
}

///////////////////////////////////////////////////////////////////////////////
// Transiciones

static const parserStateTransition ST_TYPE0[] = {
    {.when = '/',        				.destination = ERROR,         	   .action1 = unexpected,},
    {.when = TOKEN_LWSP,   				.destination = ERROR,         	   .action1 = unexpected,},
    {.when = TOKEN_REST_NAME_FIRST,		.destination = TYPE,			   .action1 = type 	  ,},
    {.when = ANY, 						.destination = ERROR, 			   .action1 = unexpected,},
};

static const parserStateTransition ST_TYPE[] =  {
    {.when = '/',        				.destination = SUBTYPE,            .action1 = typeEnd   ,},
    {.when = TOKEN_LWSP,   				.destination = ERROR,          	   .action1 = unexpected,},
    {.when = TOKEN_REST_NAME_CHARS,		.destination = TYPE,			   .action1 = type 	  ,},
    {.when = ANY, 						.destination = ERROR, 			   .action1 = unexpected,},
};

static const parserStateTransition ST_SUBTYPE[] = {
	{.when = TOKEN_LWSP,   				.destination = ERROR,          	   .action1 = unexpected,},
    {.when = ';',                       .destination = ARGUMENT,           .action1 = subtypeEnd,},
    {.when = TOKEN_REST_NAME_CHARS,		.destination = SUBTYPE,			   .action1 = subtype   ,},
    {.when = ANY, 						.destination = ERROR, 			   .action1 = unexpected,},
};

static const parserStateTransition ST_ARGUMENT[] = {
    {.when = TOKEN_LWSP,                .destination = ERROR,              .action1 = unexpected ,},
    {.when = TOKEN_REST_NAME_CHARS,     .destination = ARGUMENT,           .action1 = argument   ,},
    {.when = '=',                       .destination = VALUE_START,        .action1 = argumentEnd,},
    {.when = ANY,                       .destination = ERROR,              .action1 = unexpected ,},
};

static const parserStateTransition ST_VALUE_START[] = {
    {.when = '\"',      				.destination = VALUE,              .action1 = valueStart,},
    {.when = ANY,       				.destination = ERROR,              .action1 = unexpected,},
};

static const parserStateTransition ST_VALUE[] = {
    {.when = TOKEN_REST_NAME_CHARS,     .destination = VALUE,         	   .action1 = value     ,},
    {.when = TOKEN_BCHARS_NOSPACE,  	.destination = VALUE,         	   .action1 = value 	  ,},
    {.when = '\"',                  	.destination = VALUE_END,     	   .action1 = valueEnd  ,},
    {.when = ANY,                       .destination = ERROR,                     .action1 = unexpected,},
};

static const parserStateTransition ST_VALUE_END[] = {
    {.when = ANY,       				.destination = VALUE_END,          .action1 = valueEnd  ,},
};

static const parserStateTransition ST_ERROR[] =  {
    {.when = ANY,        				.destination = ERROR,         		.action1 = unexpected,},
};


///////////////////////////////////////////////////////////////////////////////
// Declaración formal

static const parserStateTransition * states [] = {
	ST_TYPE0,
    ST_TYPE,
    ST_SUBTYPE,
    ST_ARGUMENT,
    ST_VALUE_START,
    ST_VALUE,
    ST_VALUE_END,
    ST_ERROR,
};

#define N(x) (sizeof(x)/sizeof((x)[0]))

static const size_t statesQty [] = {
    N(ST_TYPE0),
    N(ST_TYPE),
    N(ST_SUBTYPE),
    N(ST_ARGUMENT),
    N(ST_VALUE_START),
    N(ST_VALUE),
    N(ST_VALUE_END),
    N(ST_ERROR),
};

static parserDefinition definition = {
    .statesCount = N(states),
    .states       = states,
    .statesQty     = statesQty,
    .startState  = TYPE0,
};

const parserDefinition * mediaTypeParser(void) {
    return &definition;
}

