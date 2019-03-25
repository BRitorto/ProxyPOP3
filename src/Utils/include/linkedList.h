#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <stdbool.h>

typedef struct linkedListCDT * linkedListADT;

typedef struct iteratorCDT * iteratorADT;

linkedListADT createLinkedList(void);

void deleteLinkedList(linkedListADT list);

int isEmptyLinkedList(linkedListADT list);

int isProcessedReadyLinkedList(linkedListADT list);

size_t getListSize(linkedListADT list);

int addFirst(linkedListADT list, void * data);

int addLast(linkedListADT list, void * data);

void * removeFirst(linkedListADT list);

void * getFirst(linkedListADT list);

void * getProcessed(linkedListADT list);

void * getLast(linkedListADT list);

void * process(linkedListADT list);


iteratorADT iterator(linkedListADT list);

void resetIterator(iteratorADT iterator, linkedListADT list);

void deleteIterator(iteratorADT iterator);

void * next(iteratorADT iterator);

bool hasNext(iteratorADT iterator);

void deleteCurrent(iteratorADT iterator);

#endif

