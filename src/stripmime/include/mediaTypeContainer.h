#ifndef MEDIA_TYPE_CONTAINER_H
#define MEDIA_TYPE_CONTAINER_H

#include <stdbool.h>

#include "parser.h"
#include "parserUtils.h"

typedef struct mediaType_t {
	char * type;
	char * subtype;
} mediaType_t;

typedef enum findCriteria {
	TYPE,
	SUBTYPE,
} findCriteria;

typedef enum mediaTypeStatus {
	MEDIA_TYPE_SUCCESS,
	MEDIA_TYPE_ERROR,
} mediaTypeStatus;


typedef struct mediaTypeNodeCDT * mediaTypeNode;

typedef struct mediaTypeContainerCDT * mediaTypeContainer;

typedef struct mediaTypeNodeCDT {
	mediaTypeNode next;
	mediaTypeNode subtypesPtr;
	bool allPermited;
	const char * indicator;
	parserDefinition parserDefinition;
	parserADT indicatorParser;
	const parserEvent * indicatorParserEvent;
} mediaTypeNodeCDT;

typedef struct mediaTypeContainerCDT {
	mediaTypeNode medias;
} mediaTypeContainerCDT;



mediaTypeContainer createMediaTypeContainer(const char * allPermitedIndicator);

void deleteMediaTypeContainer(mediaTypeContainer container);

mediaTypeStatus insertMediaType(mediaTypeContainer container, mediaType_t mediaType);

void mediaTypeContainerParserReset(mediaTypeContainer container);

#endif

