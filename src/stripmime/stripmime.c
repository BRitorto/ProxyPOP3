#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "parser.h"
#include "parserUtils.h"
#include "mimeCharacters.h"

#include "stack.h"
#include "mediaTypeContainer.h"
#include "mimeMessage.h"
#include "mediaType.h"

#define BOUNDARY_MAX_LENGTH 70 + 2 + 2 
/** 70 es el maximo  para el valor del argumento del boundary  y 2 es el size de "--" */
#define REPLACE_CONTENT_TYPE "text/plain; charset=\"UTF-8\""
#define REPLACE_CONTENT_TRANSFER_ENCODING " quoted-printable"
#define BUFFER_SIZE 4096
#define VALUE_LENGTH 2048

typedef struct boundary_t {
    char boundaryString[BOUNDARY_MAX_LENGTH + 1];
    uint8_t boundarySize;
    parserDefinition boundaryStartParserDefinition;
    parserADT boundaryStartParser;
    parserDefinition boundaryEndParserDefinition;
    parserADT boundaryEndParser;
} boundary_t;

static const char * replaceMessage;
static bool trueToPoint = true;
static bool falseToPoint = false;
static bool addEncoding = false;
static bool auxEvent = false;
static bool replaceEncoding = false;


/** Mantiene el estado durante el parseo */
struct ctx {

    /** Valor del header */
    char valueData[VALUE_LENGTH];

    /** Indice del valueData */
    size_t valueDataIndex;

    /** Delimitador mensaje "tipo-rfc 822" */
    parserADT     messageParser;
    /** Detector de field-name "Content-Type" */
    parserADT    contentTypeHeaderParser;

    /** Detector de field-name "Content-Transfer-Encoding" */
    parserADT     contentTransferEncodingParser;

    /** Detector de media type */
    parserADT     mediaTypeParser;

    /** Detector de argumentos de media type */
    parserADT     argumentParser;

    bool                messageReplaced;
    bool                replace;

    /** Container de media types censurables */
    mediaTypeContainer  container;


    mediaTypeNode       subtypePtr;

    /** Pila de argumentos tipo boundary */
    stackADT            boundaryStack;

    /**
     * ¿hemos detectado si el field-name que estamos procesando refiere
     * a Content-Type?. Utilizando dentro msg para los field-name.
     */
    bool *              messageContentTypeFieldDetected;
    bool *              messageContentTransferEncodingDetected;
    bool *              messageToReplaceDetected;
    bool *              boundaryArgumentDetected;
    bool *              boundaryValueDetected;
    bool *              boundaryValueEndDetected;
};


/** Setea lo necesario para la detección de los delimitadores de partes "boundaries" */
static void setBoundaryEnd(boundary_t * boundary) {
    if(boundary == NULL)
        return;

    boundary->boundaryString[boundary->boundarySize] = 0;

    if(boundary->boundaryStartParser != NULL) {
        destroyParser(boundary->boundaryStartParser);
        destroyStringCompareParserUtils(&boundary->boundaryStartParserDefinition);
    }
    boundary->boundaryStartParserDefinition = stringCompareParserUtils(boundary->boundaryString);
    boundary->boundaryStartParser = initializeParser(initializeCharactersClass(), &boundary->boundaryStartParserDefinition);

    if(boundary->boundaryStartParser == NULL)
        destroyStringCompareParserUtils(&boundary->boundaryStartParserDefinition);

    boundary->boundaryString[boundary->boundarySize] = '-';
    boundary->boundaryString[boundary->boundarySize + 1] = '-';
    boundary->boundaryString[boundary->boundarySize + 2] = 0;

    if(boundary->boundaryEndParser != NULL) {
        destroyParser(boundary->boundaryEndParser);
        destroyStringCompareParserUtils(&boundary->boundaryEndParserDefinition);
    }
    
    boundary->boundaryEndParserDefinition = stringCompareParserUtils(boundary->boundaryString);
    boundary->boundaryEndParser = initializeParser(initializeCharactersClass(), &boundary->boundaryEndParserDefinition);
    if(boundary->boundaryEndParser == NULL)
        destroyStringCompareParserUtils(&boundary->boundaryEndParserDefinition);

}

/** Libera los recursos de un "boundary_t" */
static void deleteBoundary(boundary_t * boundary) {
    if(boundary->boundaryStartParser != NULL) {
        destroyParser(boundary->boundaryStartParser);
        destroyStringCompareParserUtils(&boundary->boundaryStartParserDefinition);
    }

    if(boundary->boundaryEndParser != NULL) {
        destroyParser(boundary->boundaryEndParser);
        destroyStringCompareParserUtils(&boundary->boundaryEndParserDefinition);
    }
    free(boundary);
}

/** Manejador de errores */
static void endHandler(struct ctx * ctx) {
    
    if(ctx == NULL)
        return;
    if(ctx->container != NULL)
        deleteMediaTypeContainer(ctx->container);
    if(ctx->messageParser != NULL)
        destroyParser(ctx->messageParser);
    if(ctx->contentTypeHeaderParser != NULL)
        destroyParser(ctx->contentTypeHeaderParser);
    if(ctx->contentTransferEncodingParser != NULL)
        destroyParser(ctx->contentTransferEncodingParser);
    if(ctx->mediaTypeParser != NULL)
        destroyParser(ctx->mediaTypeParser);
    if(ctx->argumentParser != NULL)
        destroyParser(ctx->argumentParser);
    if(ctx->boundaryStack != NULL) {
        while(!isEmptyStack(ctx->boundaryStack)) {
            boundary_t * bound = pop(ctx->boundaryStack);
            if(bound != NULL)
                deleteBoundary(bound);
        }
        deleteStack(ctx->boundaryStack);
    }

    fprintf(stderr, "stripmime - Fatal error\n");
    exit(1);

}

/**
 * Detecta si un header-field-name equivale a Content-Type.
 * Deja el valor en `ctx->messageContentTypeFieldDetected'. Tres valores
 * posibles: NULL (no tenemos información suficiente todavia, por ejemplo
 * viene diciendo Conten), true si matchea, false si no matchea.
 */
static void contentTypeHeader(struct ctx * ctx, const uint8_t character) {
    const parserEvent * event = feedParser(ctx->contentTypeHeaderParser, character);
    do {
        switch(event->type) {
            case STRING_CMP_EQ:
                ctx->messageContentTypeFieldDetected = &trueToPoint;
                break;
            case STRING_CMP_NEQ:
                ctx->messageContentTypeFieldDetected = &falseToPoint;
                break;
        }
        event = event->next;
    } while(event != NULL);
}


/**
 * Detecta si un header-field-name equivale a Content-Transfer-Encoding.
 */
static void contentTransferEncodingHeader(struct ctx * ctx, const uint8_t character) {
    const parserEvent * event = feedParser(ctx->contentTransferEncodingParser, character);
    do {
        switch(event->type) {
            case STRING_CMP_EQ:
                ctx->messageContentTransferEncodingDetected = &trueToPoint;
                break;
            case STRING_CMP_NEQ:
                ctx->messageContentTransferEncodingDetected = &falseToPoint;
                break;
        }
        event = event->next;
    } while(event != NULL);
}

/**
 * Almacena la lista de subtypes en el contexto.
 */
static void setType(struct ctx * ctx) {
    mediaTypeNode node = ctx->container->medias;
    if(node->indicatorParserEvent->type == STRING_CMP_EQ) {
        ctx->subtypePtr = node->subtypesPtr;
        return;
    }

    while(node->next != NULL) {
        node = node->next;
        if(node->indicatorParserEvent->type == STRING_CMP_EQ) {
            ctx->subtypePtr = node->subtypesPtr;
            return;
        }
    }
}

/**
 * Procesa el argumento "boundary".
 */
static void argumentBoundary(struct ctx * ctx, const uint8_t character) {
    const parserEvent * event = feedParser(ctx->argumentParser, character);
    do {
        switch(event->type) {
            case STRING_CMP_EQ:
                ctx->boundaryArgumentDetected = &trueToPoint;
                break;
            case STRING_CMP_NEQ:
                ctx->boundaryArgumentDetected = &falseToPoint;
                break;
        }
        event = event->next;
    } while(event != NULL);
}


/**
 *  Consume el tipo de un media type en procesamiento.
 */
static const parserEvent * typeConsume(struct ctx * ctx, const uint8_t character) {
    mediaTypeNode node = ctx->container->medias;
    const parserEvent * event;
    node->indicatorParserEvent = feedParser(node->indicatorParser, character);
    event = node->indicatorParserEvent;
    while(node->next != NULL) {
        node = node->next;
        node->indicatorParserEvent = feedParser(node->indicatorParser, character);
        if(node->indicatorParserEvent->type == STRING_CMP_EQ) {
            event = node->indicatorParserEvent;
        }
    }
    return event;
}


/**
 * Procesa el tipo del valor del argumento de un media type proveniente de
 * un header Content-Type.
 */
static void contentTypeHeaderValueType(struct ctx *ctx, const uint8_t character) {
    const parserEvent * event = typeConsume(ctx, character);
    do {
        switch(event->type) {
            case STRING_CMP_EQ:
                ctx->messageToReplaceDetected = &trueToPoint;
                break;
            case STRING_CMP_NEQ:
                ctx->messageToReplaceDetected = &falseToPoint;
                break;
        }
        event = event->next;
    } while(event != NULL);
}


/**
 * Consume el subtipo de un media type en procesamiento.
 */
const parserEvent * subtypeConsume(struct ctx * ctx, const uint8_t character) {
    struct parserEvent * event;
    mediaTypeNode node = ctx->subtypePtr;

    if(node->allPermited) {
        auxEvent = true;
        event = malloc(sizeof(parserEvent));
        if(event == NULL) {
            return NULL;
        }
        event->type = STRING_CMP_EQ;
        event->next = NULL;
        event->n = 1;
        event->data[0] = character;
        return event;
    }
    auxEvent = false;
    node->indicatorParserEvent = feedParser(node->indicatorParser, character);
    event = (parserEvent *) node->indicatorParserEvent;

    while(node->next != NULL) {
        node = node->next;
        node->indicatorParserEvent = feedParser(node->indicatorParser, character);
        if(node->indicatorParserEvent->type == STRING_CMP_EQ)
            event = (parserEvent *) node->indicatorParserEvent;
    }
    return event;
}


/**
 * Procesa el subtipo del valor del argumento de un media type proveniente de
 * un header Content-Type.
 */
static void contentTypeHeaderValueSubtype(struct ctx * ctx, const uint8_t character) {
    const parserEvent * event = subtypeConsume(ctx, character);
    if(event == NULL)
        return;
    do {
        switch(event->type) {
            case STRING_CMP_EQ:
                ctx->messageToReplaceDetected = &trueToPoint;
                break;
            case STRING_CMP_NEQ:
                ctx->messageToReplaceDetected = &falseToPoint;
                break;
        }

        const parserEvent * next = event->next;
        if(auxEvent) {
            free((void *) event);
            auxEvent = false;
        }
        event = next;
    } while(event != NULL);
}


/**
 *  Procesa el valor del header detectado del tipo Content-Type.
 */
static void contentTypeHeaderValue(struct ctx * ctx, const uint8_t character) {
    const parserEvent * event = feedParser(ctx->mediaTypeParser, character);
    do {

        switch(event->type) {
            case MEDIA_TYPE_TYPE:
                if(ctx->messageToReplaceDetected != 0 || *ctx->messageToReplaceDetected)
                    for(int i = 0; i < event->n; i++)
                        contentTypeHeaderValueType(ctx, event->data[i]);
                break;

            case MEDIA_TYPE_TYPE_END:
                if(ctx->messageToReplaceDetected != 0 || *ctx->messageToReplaceDetected)
                    setType(ctx);
                break;

            case MEDIA_TYPE_SUBTYPE:
                if(ctx->messageToReplaceDetected != 0 && *ctx->messageToReplaceDetected)
                    contentTypeHeaderValueSubtype(ctx, character);
                break;

            case MEDIA_TYPE_ARGUMENT:
                argumentBoundary(ctx, character);
                break;

            case MEDIA_TYPE_VALUE_START:
                if(ctx->boundaryArgumentDetected != 0 && *ctx->boundaryArgumentDetected) {
                    boundary_t * newBoundary = malloc(sizeof(boundary_t));
                    if(newBoundary == NULL)
                        endHandler(ctx);

                    memset(newBoundary, 0, sizeof(boundary_t));
                    newBoundary->boundarySize = 2;
                    newBoundary->boundaryString[0] = '-';
                    newBoundary->boundaryString[1] = '-';

                    push(ctx->boundaryStack, newBoundary);
                }
                break;

            case MEDIA_TYPE_VALUE:
                if(ctx->boundaryArgumentDetected != 0 && *ctx->boundaryArgumentDetected) {
                    for(int i = 0; i < event->n; i++) {
                        boundary_t * boundary = peekStack(ctx->boundaryStack);
                        boundary->boundaryString[boundary->boundarySize++] = event->data[i];

                    }
                }
                break;
            }
        event = event->next;
    } while(event != NULL);
}


/**
 * Procesa el comienzo de un marca o delimitador del tipo boundary.
 */
static void boundaryStart(struct ctx * ctx, const uint8_t character) {
    const parserEvent * event = feedParser(((boundary_t *)peekStack(ctx->boundaryStack))->boundaryStartParser, character);
    do {
        switch(event->type) {
            case STRING_CMP_EQ:
                ctx->boundaryValueDetected = &trueToPoint;
                break;
            case STRING_CMP_NEQ:
                ctx->boundaryValueDetected = &falseToPoint;
        }
        event = event->next;
    } while(event != NULL);
}


/**
 * Procesa el final de un marca o delimitador del tipo boundary.
 */
static void boundaryEnd(struct ctx * ctx, const uint8_t character) {
    const parserEvent * event = feedParser(((boundary_t *)peekStack(ctx->boundaryStack))->boundaryEndParser, character);
    do {
        switch(event->type) {
            case STRING_CMP_EQ:
                ctx->boundaryValueEndDetected = &trueToPoint;
                break;

            case STRING_CMP_NEQ:
                ctx->boundaryValueEndDetected = &falseToPoint;
                break;
        }
        event = event->next;
    } while(event != NULL);
}


/**
 * Procesa un mensaje `tipo-rfc822'.
 * Si reconoce un al field-header-name Content-Type lo interpreta.
 *
 */
static void mimeMessage(struct ctx * ctx, const uint8_t character) {
    const parserEvent * event = feedParser(ctx->messageParser, character);
    bool replacePrinted = false;
    do {
        switch(event->type) {
            case MIME_MSG_NAME:
                if( ctx->messageContentTypeFieldDetected == 0 || *ctx->messageContentTypeFieldDetected) {
                    for(int i = 0; i < event->n; i++)
                        contentTypeHeader(ctx, event->data[i]);
                } 
                if(ctx->messageContentTransferEncodingDetected == 0 || *ctx->messageContentTransferEncodingDetected) {
                    for(int i = 0; i < event->n; i++)
                        contentTransferEncodingHeader(ctx, event->data[i]);
                }
                break;

            case MIME_MSG_NAME_END:
                replaceEncoding = *ctx->messageContentTransferEncodingDetected;
                ctx->messageContentTransferEncodingDetected = 0;

                resetParser(ctx->contentTypeHeaderParser);
                resetParser(ctx->contentTransferEncodingParser);
                break;

            case MIME_MSG_VALUE:
 
                for(int i = 0; i < event->n; i++) {
                    ctx->valueData[ctx->valueDataIndex++] = event->data[i];
                    if(ctx->valueDataIndex >= VALUE_LENGTH)
                        endHandler(ctx);

                    if(ctx->messageContentTypeFieldDetected != 0 && *ctx->messageContentTypeFieldDetected)
                        contentTypeHeaderValue(ctx, event->data[i]);
                }
                break;

            case MIME_MSG_VALUE_END:
                if(ctx->messageToReplaceDetected != 0 && *ctx->messageToReplaceDetected) {
                    ctx->replace = true;
                    fprintf(stdout,"%s", REPLACE_CONTENT_TYPE);
                } 
                else {
                    if(ctx->replace && replaceEncoding){
                        fprintf(stdout, "%s\r\n", REPLACE_CONTENT_TRANSFER_ENCODING);
                        addEncoding = true;
                    }
                    else
                        fprintf(stdout, "%s\r\n", ctx->valueData);
                }
                memset(ctx->valueData, 0, ctx->valueDataIndex);
                ctx->valueDataIndex = 0;
            
                setBoundaryEnd((boundary_t *)peekStack(ctx->boundaryStack));
                resetParser(ctx->mediaTypeParser);
                mediaTypeContainerParserReset(ctx->container);
                resetParser(ctx->argumentParser);
                resetParser(ctx->contentTransferEncodingParser);
                ctx->messageContentTypeFieldDetected = 0;
                ctx->messageToReplaceDetected = &falseToPoint;
                ctx->messageContentTransferEncodingDetected = 0;
                break;

            case MIME_MSG_BODY:
                if(ctx->replace && !ctx->messageReplaced) {
                    if(!addEncoding)
                        fprintf(stdout, "Content-Transfer-Encoding: quoted-printable\r\n");
                    fprintf(stdout, "%s\r\n", replaceMessage);
                    ctx->messageReplaced = true;
                } else if (!ctx->replace){
                    putchar(character);
                    replacePrinted = true;
                }
                if ((ctx->boundaryArgumentDetected != 0 && *ctx->boundaryArgumentDetected) || !isEmptyStack(ctx->boundaryStack)) {
                    for(int i = 0; i < event->n; i++) {
                        boundaryStart(ctx, event->data[i]);
                        boundaryEnd(ctx, event->data[i]);
                        if(!replacePrinted && ctx->boundaryValueEndDetected != 0 && *ctx->boundaryValueEndDetected)
                            putchar(character);
                    }
                }
                break;

            case MIME_MSG_BODY_NEWLINE:
                if(ctx->boundaryValueDetected != 0 && ctx->boundaryValueEndDetected != 0
                    && (*ctx->boundaryValueEndDetected && !*ctx->boundaryValueDetected)) {
                    boundary_t * boundary = (boundary_t *)pop(ctx->boundaryStack);
                    if(boundary != NULL) 
                        deleteBoundary(boundary);
                }
                if(ctx->boundaryValueDetected != 0 && *ctx->boundaryValueDetected) {
                    ctx->replace = false;
                    ctx->messageReplaced = false;
                    ctx->messageToReplaceDetected = &falseToPoint;
                    ctx->boundaryArgumentDetected = &falseToPoint;
                    ctx->boundaryValueDetected = NULL;
                    ctx->boundaryValueEndDetected = NULL;
                    ctx->subtypePtr = NULL;
                    ctx->messageContentTypeFieldDetected = NULL;
                    ctx->messageContentTransferEncodingDetected = NULL;
                    resetParser(ctx->messageParser);
                    mediaTypeContainerParserReset(ctx->container);
                    resetParser(ctx->mediaTypeParser);
                    resetParser(ctx->argumentParser);
                    resetParser(ctx->contentTypeHeaderParser);
                    resetParser(ctx->contentTransferEncodingParser);
                    boundary_t * boundary = peekStack(ctx->boundaryStack);
                    resetParser(boundary->boundaryEndParser);
                    resetParser(boundary->boundaryStartParser);
                }
                boundary_t * aux = (boundary_t *)peekStack(ctx->boundaryStack);
                if(aux != NULL) {
                    resetParser(aux->boundaryStartParser);
                    resetParser(aux->boundaryEndParser);
                }
                break;

            case MIME_MSG_VALUE_FOLD:
                for(int i = 0; i < event->n; i++) {
                    ctx->valueData[ctx->valueDataIndex++] = event->data[i];
                    if(ctx->valueDataIndex >= VALUE_LENGTH)
                        endHandler(ctx);
                }
                break;

            default:
                break;

        }
        if(event->type != MIME_MSG_BODY && event->type != MIME_MSG_VALUE && event->type != MIME_MSG_VALUE_END && event->type != MIME_MSG_WAIT && event->type != MIME_MSG_VALUE_FOLD && !replacePrinted) {
            //if (!(( event->type == MIME_MSG_BODY_CR || event->type == MIME_MSG_BODY_NEWLINE) && ctx->messageReplaced)) {
              //  putchar(character);
                //replacePrinted = true;
            //}
            if ((character == '\r' || character == '\n') && (event->type == MIME_MSG_BODY_NEWLINE || event->type == MIME_MSG_BODY_CR) && ctx->messageReplaced) {
                // nada por hacer
            } else {
                if(event->type != MIME_MSG_BODY_NEWLINE && character == '\n')
                   putchar('\r');

                putchar(character);
                replacePrinted = true;
            }
        }
        event = event->next;
    } while(event != NULL);
}

/** 
 * Para evitar el argumento 'q' en el mediaRange (RFC 7231 Sec 5.3.2)
 */ 
static size_t stringLengthSubtype(char * string) {
    size_t length = 0;
    if(string == NULL)
        return length;
    while(string[length] != 0 && string[length] != ';' && string[length++] != ' ');
    return length-1;
}

/**
 * Crea y completa un "mediaTypeContainer" para almacenar el media range especificado.
 */
static mediaTypeContainer createAndFillMediaTypeContainer(char * mediaRange) {
    if(mediaRange == NULL)
        return NULL;

    char  * aux = malloc(strlen(mediaRange) + 1);
    memcpy(aux, mediaRange, strlen(mediaRange)+1);
    mediaTypeContainer newContainer = createMediaTypeContainer("*");

    if(newContainer == NULL)
        return newContainer;
    const char * delimiter = ",";
    char * mediaRangeContextLast;
    const char * subtypeDelimiter = "/";
    char * mediaTypeContextLast;

    char * mediaTypeString = strtok_r((char *)aux, delimiter, &mediaRangeContextLast);

    mediaType_t mediaType;
    size_t length = 0;

    while(mediaTypeString != NULL) {
        length = strlen(mediaTypeString) + 1;
        char * newString = malloc(length);
        if(newString == NULL) {
            deleteMediaTypeContainer(newContainer);
            return NULL;
        }
        memcpy(newString, mediaTypeString, length);
        char * type = strtok_r(newString, subtypeDelimiter, &mediaTypeContextLast);
        if(type == NULL) {
            free(newString);
            deleteMediaTypeContainer(newContainer);
            return NULL;
        }
        length = strlen(type) + 1;
        mediaType.type = malloc(length);
        if(mediaType.type == NULL) {
            free(newString);
            deleteMediaTypeContainer(newContainer);
            return NULL;
        }
        memcpy(mediaType.type, type, strlen(type)+1);

        char * subtype = strtok_r(NULL, subtypeDelimiter, &mediaTypeContextLast);
        if(subtype == NULL) {
            free(newString);
            free(mediaType.type);
            deleteMediaTypeContainer(newContainer);
            return NULL;
        }
        length = stringLengthSubtype(subtype) + 1;
        mediaType.subtype = malloc(length);
        if(mediaType.subtype == NULL) {
            free(newString);
            free(mediaType.type);
            deleteMediaTypeContainer(newContainer);
            return NULL;
        }
        memcpy(mediaType.subtype, subtype, length);

        insertMediaType(newContainer, mediaType);
        free(newString);
        free(mediaType.type);
        free(mediaType.subtype);
        mediaTypeString  = strtok_r(NULL, delimiter, &mediaRangeContextLast);
    }
    free(aux);

    return newContainer;
}


int main(void) {
    const unsigned int * noClass = noClassesParser();
    parserDefinition mediaHeaderDefinition = stringCompareParserUtils("content-type");
    parserDefinition argumentParserDefinition = stringCompareParserUtils("boundary");
    parserDefinition transferEncodingParserDefinition = stringCompareParserUtils("content-transfer-encoding");
    const char * mediaRange =  getenv("FILTER_MEDIAS");
    replaceMessage = getenv("FILTER_MSG");
    mediaTypeContainer container = createAndFillMediaTypeContainer((char *)mediaRange);


    if(container == NULL)
        exit(1);

    struct ctx ctx;

    memset(&ctx, 0, sizeof(struct ctx));
    ctx.messageParser                 = initializeParser(initializeCharactersClass(), mimeMessageParser());
    ctx.contentTypeHeaderParser       = initializeParser(noClass, &mediaHeaderDefinition);
    ctx.contentTransferEncodingParser = initializeParser(noClass, &transferEncodingParserDefinition);
    ctx.mediaTypeParser               = initializeParser(initializeCharactersClass(), mediaTypeParser());
    ctx.argumentParser                = initializeParser(noClass, &argumentParserDefinition);
    ctx.container                     = container;
    ctx.boundaryStack                 = createStack();

    uint8_t dataBuffer[BUFFER_SIZE];
    int n;
    do {
        n = read(STDIN_FILENO, dataBuffer, sizeof(dataBuffer));
        for(ssize_t i = 0; i < n ; i++) {
            mimeMessage(&ctx, dataBuffer[i]);
        }

    } while(n > 0);

    while(!isEmptyStack(ctx.boundaryStack)) {
        boundary_t * bound = pop(ctx.boundaryStack);
        if(bound != NULL)
            deleteBoundary(bound);
    }
    deleteStack(ctx.boundaryStack);
    deleteMediaTypeContainer(container);
    destroyParser(ctx.messageParser);
    destroyParser(ctx.contentTypeHeaderParser);
    destroyParser(ctx.contentTransferEncodingParser);
    destroyParser(ctx.mediaTypeParser);
    destroyParser(ctx.argumentParser);
    destroyStringCompareParserUtils(&mediaHeaderDefinition);
    destroyStringCompareParserUtils(&argumentParserDefinition);
    destroyStringCompareParserUtils(&transferEncodingParserDefinition);

    return 0;
}

