#include <stdlib.h>

#include "stack.h"
#include "linkedList.h"
#include "errorslib.h"


typedef struct stackCDT {
    linkedListADT list;
} stackCDT;


stackADT createStack(void) {
	stackADT stack = malloc(sizeof(stackADT));
    checkIsNotNull(stack, "Creating a Stack.");

    stack->list = createLinkedList();
    return stack;
}

void deleteStack(stackADT stack) {
    if(stack != NULL) {
        deleteLinkedList(stack->list);
        free(stack);
    }
}

int isEmptyStack(stackADT stack) {
    return stack == NULL || isEmptyLinkedList(stack->list);
}

size_t getStackSize(stackADT stack) {
    return getListSize(stack->list);
}

int push(stackADT stack, void * data) {
    if(stack == NULL)
        return -1;
        
    return addFirst(stack->list, data);
}

void * pop(stackADT stack) {
    if (stack == NULL)
        return NULL;
    process(stack->list);
    return removeFirst(stack->list);
}

void * peekStack(stackADT stack) {
    if (stack == NULL)
        return NULL;

    return getFirst(stack->list);
}

