#ifndef PARSER_UTILS_H
#define PARSER_UTILS_H

/**
 * parserUtils.c -- factory de ciertos parsers típicos
 *
 * Provee parsers reusables, como por ejemplo para verificar que
 * un string es igual a otro de forma case insensitive.
 */
#include "parser.h"

typedef enum stringCompareEventTypes {
    STRING_CMP_MAYEQ,
    /** Hay posibilidades de que el string sea igual */
    STRING_CMP_EQ,
    /** NO hay posibilidades de que el string sea igual */
    STRING_CMP_NEQ,
} stringCompareEventTypes;

/*
 * Crea un parser que verifica que los caracteres recibidos forment el texto
 * descripto por `sting'.
 *
 * Si se recibe el evento `STRING_CMP_NEQ' el texto entrado no matchea.
 */
parserDefinition stringCompareParserUtils(const char * string);

/**
 * Libera recursos asociado a una llamada de `stringCompareParserUtils'
 */
void destroyStringCompareParserUtils(const parserDefinition * parserDefinition);

#endif

