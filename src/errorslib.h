#ifndef ERRORS_LIB_H
#define ERRORS_LIB_H

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

typedef enum checkType {
	CHECK_FAIL   			= 0,
	CHECK_IS_NULL 	  		= 1,
	CHECK_NOT_NULL 	  		= 2,
	CHECK_EQUALS 			= 3,
	CHECK_NOT_EQUALS		= 4,
	CHECK_GREATER_THAN  	= 5,
	CHECK_GREATER_EQ_THAN 	= 6,
} checkType;

typedef void (* finallyFunc) (void *);

#define fail(...) inFail(__FILE__, __LINE__, __VA_ARGS__)

#define checkFail(aNumber, ...)                         		checkCondition(CHECK_FAIL,         aNumber  >= 0,           __FILE__, __LINE__, __VA_ARGS__)
#define checkIsNull(aPointer, ...)                      		checkCondition(CHECK_IS_NULL,      aPointer == 0,           __FILE__, __LINE__, __VA_ARGS__)
#define checkIsNotNull(aPointer, ...)                   		checkCondition(CHECK_NOT_NULL,     aPointer != 0,           __FILE__, __LINE__, __VA_ARGS__)
#define checkAreEquals(aNumber, otherNumber, ...)       		checkCondition(CHECK_EQUALS,       aNumber  == otherNumber, __FILE__, __LINE__, __VA_ARGS__)
#define checkAreNotEquals(aNumber, otherNumber, ...)    		checkCondition(CHECK_NOT_EQUALS,   aNumber  != otherNumber, __FILE__, __LINE__, __VA_ARGS__)
#define checkGreaterThan(bigNumber, smallNumber, ...)   		checkCondition(CHECK_GREATER_THAN, 	bigNumber > smallNumber, __FILE__, __LINE__, __VA_ARGS__)
#define checkGreaterOrEqualsThan(bigNumber, smallNumber, ...)   checkCondition(CHECK_GREATER_EQ_THAN, bigNumber >= smallNumber, __FILE__, __LINE__, __VA_ARGS__)


#define checkFailWithFinally(aNumber, finally, data, ...)                         checkConditionWithFinally(CHECK_FAIL,         aNumber  >= 0          , finally, data, __FILE__, __LINE__, __VA_ARGS__)
#define checkIsNullWithFinally(aPointer, finally, data, ...)                      checkConditionWithFinally(CHECK_IS_NULL,      aPointer == 0          , finally, data, __FILE__, __LINE__, __VA_ARGS__)
#define checkIsNotNullWithFinally(aPointer, finally, data, ...)                   checkConditionWithFinally(CHECK_NOT_NULL,     aPointer != 0          , finally, data, __FILE__, __LINE__, __VA_ARGS__)
#define checkAreEqualsWithFinally(aNumber, otherNumber, finally, data, ...)       checkConditionWithFinally(CHECK_EQUALS,       aNumber  == otherNumber, finally, data, __FILE__, __LINE__, __VA_ARGS__)
#define checkAreNotEqualsWithFinally(aNumber, otherNumber, finally, data, ...)    checkConditionWithFinally(CHECK_NOT_EQUALS,   aNumber  != otherNumber, finally, data, __FILE__, __LINE__, __VA_ARGS__)

void inFail(const char * file, int line, const char * fmt, ...);
void checkCondition(checkType type, int condition, const char * file, int line, const char * fmt, ...);
void checkConditionWithFinally(checkType type, int condition, finallyFunc finally, void * data, const char * file, int line, const char * fmt, ...);

#endif

