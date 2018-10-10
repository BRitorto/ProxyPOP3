#ifndef ERRORS_LIB_H
#define ERRORS_LIB_H

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>


#define fail(...)                                       vfail(                                __FILE__, __LINE__, __VA_ARGS__)
#define checkFail(aNumber, ...)                         vcheckFail(aNumber,                   __FILE__, __LINE__, __VA_ARGS__)
#define checkIsNotNull(aPointer, ...)                   vcheckIsNotNull(aPointer,             __FILE__, __LINE__, __VA_ARGS__)
#define checkIsNull(aPointer, ...)                      vcheckIsNull(aPointer,                __FILE__, __LINE__, __VA_ARGS__)
#define checkAreEquals(aNumber, otherNumber, ...)       vcheckAreEquals(aNumber, otherNumber, __FILE__, __LINE__, __VA_ARGS__)
#define checkAreNotEquals(aNumber, otherNumber, ...)    vcheckAreNotEquals(aNumber, otherNumber, __FILE__, __LINE__, __VA_ARGS__)

void vfail(const char * file, int line, const char * fmt, ...);
void vcheckFail(int aNumber, const char * file, int line, const char * fmt, ...);
void vcheckIsNotNull(void * aPointer, const char * file, int line, const char * fmt, ...);
void vcheckIsNull(void * aPointer, const char * file, int line, const char * fmt, ...);
void vcheckAreEquals(int aNumber, int otherNumber, const char * file, int line, const char * fmt, ...);
void vcheckAreNotEquals(int aNumber, int otherNumber, const char * file, int line, const char * fmt, ...);

#endif

