#ifndef QUEUE_H
#define QUEUE_H


typedef struct queueCDT * queueADT;


queueADT createQueue(void);

void deleteQueue(queueADT queue);

int isEmptyQueue(queueADT queue);

int isProcessedReadyQueue(queueADT queue);

size_t getQueueSize(queueADT queue);

int offer(queueADT queue, void * data);

void * poll(queueADT queue);

void * peek(queueADT queue);

void * peekLast(queueADT queue);

void * peekProcessed(queueADT queue);

void * processQueue(queueADT queue);

#endif

