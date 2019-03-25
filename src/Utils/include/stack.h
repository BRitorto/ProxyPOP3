#ifndef STACK_H
#define STACK_H


typedef struct stackCDT * stackADT;

stackADT createStack(void);

void deleteStack(stackADT stack);

int isEmptyStack(stackADT stack);

size_t getStackSize(stackADT stack);

int push(stackADT stack, void * data);

void * pop(stackADT stack);
 
void * peekStack(stackADT stack);


#endif

