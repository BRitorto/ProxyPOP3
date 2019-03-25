#ifndef MIME_MESSAGE_H
#define MIME_MESSAGE_H

/**
 * mimeMessage.c - tokenizador de mensajes "tipo" message/rfc822.
 *
 * "Tipo" porque simplemente detectamos partes pero no requerimos ningún
 * header en particular.
 *
 */
#include "parser.h"

/** Tipo de eventos de un mensaje mime */

typedef enum mimeMessageEventType {
    /** Caracter del nombre de un header. payload: caracter. */
    MIME_MSG_NAME,

    /** El nombre del header está completo. payload: ':'. */
    MIME_MSG_NAME_END,

    /** Caracter del value de un header. payload: caracter. */
    MIME_MSG_VALUE,

    /** Se ha foldeado el valor. payload: CR LF */
    MIME_MSG_VALUE_FOLD,

    /** El valor de un header está completo. CR LF  */
    MIME_MSG_VALUE_END,

    /** Comienza el body */
    MIME_MSG_BODY_START,

    /** Se recibió un caracter que pertence al body */
    MIME_MSG_BODY,

    /** Se recibió el retorno de carro del body */
    MIME_MSG_BODY_CR,

    /** Se recibió el salto de linea del body */
    MIME_MSG_BODY_NEWLINE,

    /** No tenemos idea de qué hacer hasta que venga el proximo caracter */
    MIME_MSG_WAIT,

    /** Se recibió un caracter que no se esperaba */
    MIME_MSG_UNEXPECTED,
} mimeMessageEventType;

/** La definición del parser */
const parserDefinition * mimeMessageParser(void);

#endif

