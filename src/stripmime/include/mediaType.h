#ifndef MEDIA_TYPE_H
#define MEDIA_TYPE_H

/**
 * mediaType.c - tokenizador de media types.
 */
#include "parser.h"

/** Tipo de eventos de un media type. */
struct parser;

typedef enum mediaTypeEventType {
    /** Caracter del tipo de un media type. payload: caracter. */
    MEDIA_TYPE_TYPE,

    /** El tipo del media type está completo. payload: '/'. */
    MEDIA_TYPE_TYPE_END,

    /** Caracter del subtipo de un media type. payload: caracter. */
    MEDIA_TYPE_SUBTYPE,

    /** El subtipo del media type está completo. payload: ';'. */    
    MEDIA_TYPE_SUBTYPE_END,

    /** Caracter del argumento de un media type. payload: caracter. */
    MEDIA_TYPE_ARGUMENT,

    /** El argumento del media type está completo. payload: '='. */    
    MEDIA_TYPE_ARGUMENT_END,

    /** El valor del argumento del media type comenzó. payload: '\"'. */    
    MEDIA_TYPE_VALUE_START,

    /** Caracter del valor del argumento de un media type. payload: caracter. */
    MEDIA_TYPE_VALUE,

    /** El valor del argumento del media type está completo. payload: '\"'. */    
    MEDIA_TYPE_VALUE_END,

    /** Se recibió un caracter inesperado. */
    MEDIA_TYPE_UNEXPECTED,
} mediaTypeEventType;

/** La definición del parser */
const parserDefinition * mediaTypeParser(void);

#endif


