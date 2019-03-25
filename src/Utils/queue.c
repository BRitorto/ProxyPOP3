#include <stdlib.h>

#include "queue.h"
#include "linkedList.h"
#include "errorslib.h"


typedef struct queueCDT {
    linkedListADT list;
} queueCDT;


queueADT createQueue(void) {
    queueADT queue = malloc(sizeof(queueCDT));
    checkIsNotNull(queue, "Creating a Queue.");

    queue->list = createLinkedList();
    return queue;
}

void deleteQueue(queueADT queue) {
    if(queue != NULL) {
        deleteLinkedList(queue->list);
        free(queue);
    }
}
 
inline int isEmptyQueue(queueADT queue) {
    return queue == NULL || isEmptyLinkedList(queue->list);
}

int isProcessedReadyQueue(queueADT queue) {
    return queue != NULL && isProcessedReadyLinkedList(queue->list);
}

inline size_t getQueueSize(queueADT queue) {
    return getListSize(queue->list);
}

inline int offer(queueADT queue, void * data) {
    if(queue == NULL)
        return -1;
        
    return addLast(queue->list, data);
}
 
inline void * poll(queueADT queue) {
    if (queue == NULL)
        return NULL;

    return removeFirst(queue->list);
}

inline void * peek(queueADT queue) {
    if (queue == NULL)
        return NULL;

    return getFirst(queue->list);
}

inline void * peekLast(queueADT queue) {
    if (queue == NULL)
        return NULL;

    return getLast(queue->list);
}

inline void * peekProcessed(queueADT queue) {
    if (queue == NULL)
        return NULL;

    return getProcessed(queue->list);
}

inline void * processQueue(queueADT queue) {
    if (queue == NULL)
        return NULL;

    return process(queue->list);
}

