#ifndef ERRORS_LIB_H
#define ERRORS_LIB_H

void checkFail(int aNumber, char * msg);

void fail(char * msg);

void checkIsNotNull(void * aPointer, char * msg);

void checkIsNull(void * aPointer, char * msg);

void checkAreEquals(int aNumber, int otherNumber, char * msg);

#endif

