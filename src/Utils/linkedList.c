
#include <stdlib.h>
#include "linkedList.h"
#include "errorslib.h"

typedef struct nodeCDT {
    void *           data;
    struct nodeCDT * next;
} nodeCDT;

typedef struct nodeCDT * nodeADT; 

typedef struct linkedListCDT {
    size_t  amountOfNodes;
    nodeADT first;
    nodeADT processed;
    nodeADT last;
} linkedListCDT;

typedef struct iteratorCDT {
    linkedListADT list;
    nodeADT       current;
    nodeADT       prev;
} iteratorCDT;


static nodeADT createNode(void * data);

static void deleteNode(nodeADT node);

static void deleteAllNodes(nodeADT node, int size);


linkedListADT createLinkedList(void) {
    linkedListADT newList = malloc(sizeof(linkedListCDT));
    checkIsNotNull(newList, "Creating a LikedList.");
    
    newList->amountOfNodes = 0;
    newList->first = NULL;
    newList->processed = NULL;
    newList->last = NULL;

    return newList;
}

void deleteLinkedList(linkedListADT list) {
    deleteAllNodes(list->first, list->amountOfNodes);
    free(list);
}

inline int isEmptyLinkedList(linkedListADT list) {
    return list == NULL || list->amountOfNodes == 0;
}

inline int isProcessedReadyLinkedList(linkedListADT list) {
    return !isEmptyLinkedList(list) && list->processed != list->first && list->first != NULL;
}

int addFirst(linkedListADT list, void * data) {
    if(list == NULL)
        return -1;

    nodeADT node = createNode(data);
    list->amountOfNodes++;
    
    if(list->amountOfNodes == 1) {
        list->first     = node;
        list->processed = node;
        list->last      = node;
        return 0;
    }

    if(list->processed == NULL)
        list->processed = node;
    
    node->next = list->first;
    list->first = node;
    return 0;
}

int addLast(linkedListADT list, void * data) {
    if(list == NULL)
        return -1;

    nodeADT node = createNode(data);
    list->amountOfNodes++;

    if(list->amountOfNodes == 1) {
        list->first     = node;
        list->processed = node;
        list->last      = node;
        return 0;
    }

    if(list->processed == NULL)
        list->processed = node;
    
    list->last->next = node;
    list->last = node;
    return 0;
}

void * removeFirst(linkedListADT list) {
    if(isEmptyLinkedList(list))
        return NULL;
    
    checkAreNotEquals(list->processed, list->first, "Invalid remove first in list");

    nodeADT exFirst = list->first;
    void * data = exFirst->data;
    list->amountOfNodes--;

    list->first = list->first->next;
    if(list->amountOfNodes == 0) {
        list->first     = NULL;
        list->processed = NULL;
        list->last      = NULL;
    }
    
    deleteNode(exFirst);
    return data;
}

inline size_t getListSize(linkedListADT list) {
    if(isEmptyLinkedList(list))
        return 0;

    return list->amountOfNodes;
}

inline void * getFirst(linkedListADT list) {
    if(isEmptyLinkedList(list))
        return NULL;

    return list->first->data;
}

inline void * getProcessed(linkedListADT list) {
    if(isEmptyLinkedList(list) || list->processed == NULL)
        return NULL;

    return list->processed->data;
}

inline void * getLast(linkedListADT list) {
    if(isEmptyLinkedList(list))
        return NULL;

    return list->last->data;
}

inline void * process(linkedListADT list)  {
    if(isEmptyLinkedList(list) || list->processed == NULL)
        return NULL;
    
    list->processed = list->processed->next;
    if(list->processed == NULL)
        return NULL;
    return list->processed->data;
}

static inline void deleteAllNodes(nodeADT node, int size) {
    if(node != NULL) {
        nodeADT nextNode = node->next;
        deleteNode(node);
        if (size > 1)
            deleteAllNodes(nextNode, size-1);
    }
}

static inline nodeADT createNode(void * data) {
    nodeADT newNode = malloc(sizeof(nodeCDT));
    checkIsNotNull(newNode, "Creating a Node.");
    
    newNode->data = data;
    newNode->next = NULL;

    return newNode;
}

static inline void deleteNode(nodeADT node) {
    free(node);
}


iteratorADT iterator(linkedListADT list) {
    iteratorADT iterator = malloc(sizeof(iteratorCDT));
    checkIsNotNull(iterator, "Creating a LinkedListIterator");
    
    resetIterator(iterator, list);
    return iterator;
}

inline void resetIterator(iteratorADT iterator, linkedListADT list) {
    if(iterator == NULL || list == NULL)
        return;
        
    iterator->list = list;
    iterator->prev = NULL;

    if(list->processed == list->first)
        iterator->current = NULL;
    else
        iterator->current = list->first;

}

inline void deleteIterator(iteratorADT iterator) {
    free(iterator);
}

inline void * next(iteratorADT iterator) {
    if(iterator == NULL || iterator->list->amountOfNodes == 0 || iterator->current == NULL)
         return NULL;

    if(iterator->current == iterator->list->processed)
        return NULL;

    iterator->prev = iterator->current;
    iterator->current = iterator->current->next;

    return iterator->current->data;
}

inline bool hasNext(iteratorADT iterator) {
    if(iterator == NULL || iterator->list->amountOfNodes == 0 || iterator->current == NULL)
         return false;

    if(iterator->current->next == iterator->list->processed)
        return false;

    return iterator->current->next != NULL;
}

void deleteCurrent(iteratorADT iterator) {
    if(iterator == NULL || iterator->list->amountOfNodes == 0)
        return;

    nodeADT exCurrent  = iterator->current;
    iterator->current  = exCurrent->next;
    linkedListADT list = iterator->list;
    list->amountOfNodes--;
    
    if(list->amountOfNodes == 0) {
        list->first     = NULL;
        list->processed = NULL;
        list->last      = NULL;
        return;
    }

    iterator->prev->next = iterator->current;
    deleteNode(exCurrent);

    if(iterator->current == iterator->list->processed)
        iterator->current = NULL;
}

