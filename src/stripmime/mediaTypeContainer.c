#include "mediaTypeContainer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static const char * allPermitedIndicatorString;

static mediaTypeNode createMediaTypeNode(const char * indicator, bool allPermited);

static void deleteMediaTypeNode(mediaTypeNode node);

static mediaTypeNode findByCriteria(mediaTypeNode node, const char * indicator, bool * locate, findCriteria criteria);

static void deleteMediaTypeNodeSubtypePtr(mediaTypeNode node);



mediaTypeContainer createMediaTypeContainer(const char * allPermitedIndicator) {
	if(allPermitedIndicator == NULL)
		return NULL;
	allPermitedIndicatorString = allPermitedIndicator;
	return calloc(1, sizeof(mediaTypeNodeCDT));
}

void deleteMediaTypeContainer(mediaTypeContainer container) {
	mediaTypeNode node = container->medias;
	mediaTypeNode aux, subtypePtr;

	while(node != NULL) {
		subtypePtr = node->subtypesPtr;
		while(subtypePtr != NULL) {
			aux = subtypePtr;
			if(aux->indicatorParser != NULL) {
				destroyParser(aux->indicatorParser);
				destroyStringCompareParserUtils(&aux->parserDefinition);
			}
			subtypePtr = subtypePtr->next;
			free(aux);
		}
		aux = node;
		if(node->indicatorParser != NULL) {
			destroyParser(node->indicatorParser);
			destroyStringCompareParserUtils(&node->parserDefinition);
		}
		node = node->next;
		free(aux);
	}

	free(container);
}

mediaTypeStatus insertMediaType(mediaTypeContainer container, mediaType_t mediaType) {
	if(container == NULL || mediaType.type == NULL || mediaType.subtype == NULL)
		return MEDIA_TYPE_ERROR;
	bool allPermited = false;
	if(container->medias == NULL) {
		if(strcmp(allPermitedIndicatorString, mediaType.type) == 0)
			return MEDIA_TYPE_ERROR;
		allPermited = strcmp(mediaType.subtype, allPermitedIndicatorString) == 0 ? true : false;
		container->medias = createMediaTypeNode(mediaType.type, false);
		container->medias->subtypesPtr = createMediaTypeNode(mediaType.subtype, allPermited);
		return MEDIA_TYPE_SUCCESS;
	}
	bool locate = false;
	allPermited = strcmp(allPermitedIndicatorString, mediaType.subtype) == 0 ? true : false;
	mediaTypeNode node = findByCriteria(container->medias, mediaType.type, &locate, TYPE);
	if(locate) {
		if(allPermited) {
			deleteMediaTypeNodeSubtypePtr(node);
			node->subtypesPtr = createMediaTypeNode(allPermitedIndicatorString, allPermited);
		} else {
			bool locateSubtype = false;
			node = findByCriteria(node->subtypesPtr, mediaType.subtype, &locateSubtype, SUBTYPE);
			if(!locateSubtype) {
				node->next = createMediaTypeNode(mediaType.subtype, false);
				return MEDIA_TYPE_SUCCESS;
			} else {
				return MEDIA_TYPE_ERROR;
			}
		}
	} else {
		node->next = createMediaTypeNode(mediaType.type, false);
		node->next = createMediaTypeNode(mediaType.subtype, allPermited);
		return MEDIA_TYPE_SUCCESS;
	}
	return MEDIA_TYPE_ERROR;
}

void mediaTypeContainerParserReset(mediaTypeContainer container) {
    mediaTypeNode node = container->medias;
    mediaTypeNode subtypePtr;
    while(node != NULL) {
        subtypePtr = node->subtypesPtr;
        while(subtypePtr != NULL) {
			if (!subtypePtr->allPermited)
				resetParser(subtypePtr->indicatorParser);
            subtypePtr = subtypePtr->next;
        }
        resetParser(node->indicatorParser);
        node = node->next;
    }
}

static mediaTypeNode createMediaTypeNode(const char * indicator, bool allPermited) {
	mediaTypeNode newNode = calloc(1, sizeof(mediaTypeNodeCDT));
	if(newNode == NULL || indicator == NULL)
		return NULL;
	newNode->indicator = indicator;
	newNode->allPermited = allPermited;
	if(allPermited)
		return newNode;
	newNode->parserDefinition = stringCompareParserUtils(indicator);
	newNode->indicatorParser = initializeParser(noClassesParser(), &newNode->parserDefinition);
	return newNode;
} 

static void deleteMediaTypeNode(mediaTypeNode node) {
	if(node != NULL && !node->allPermited) {
		destroyStringCompareParserUtils(&node->parserDefinition);
		destroyParser(node->indicatorParser);
	}
	if(node != NULL)
		free(node);
}


static mediaTypeNode findByCriteria(mediaTypeNode node, const char * indicator, bool * locate, findCriteria criteria) {
	switch(criteria) {
			case TYPE:
				if(strcmp(indicator, node->indicator) == 0) {
					*locate = true;
					return node;
				}
				break;
			case SUBTYPE:
				if(node->allPermited || strcmp(indicator, node->indicator) == 0) {
					*locate = true;
					return node;
				}
				break;
			default:
				break;
		}
	while(node->next != NULL) {
		node = node->next;
		switch(criteria) {
			case TYPE:
				if(strcmp(indicator, node->indicator) == 0) {
					*locate = true;
					return node;
				}
				break;
			case SUBTYPE:
				if(node->allPermited || strcmp(indicator, node->indicator) == 0) {
					*locate = true;
					return node;
				}
				break;
			default:
				break;
		}
	}
	return node;
}


static void deleteMediaTypeNodeSubtypePtr(mediaTypeNode node) {

	mediaTypeNode subtypePtr = node->subtypesPtr;
	mediaTypeNode aux;

	while (subtypePtr != NULL) {
		aux = subtypePtr;
		subtypePtr = subtypePtr->next;
		deleteMediaTypeNode(aux);
	}
	subtypePtr->subtypesPtr = NULL;
}


