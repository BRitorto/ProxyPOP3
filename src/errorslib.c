#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "logger.h"
#include "errorslib.h"


static inline void finally(const char * file, int line, const char * fmt, va_list args) {
	fflush(stdout);
	vlogLogger(LOG_LEVEL_FATAL, file, line, fmt, args);
	va_end(args);
	exit(1);
}

void vfail(const char * file, int line, const char * fmt, ...) {
	va_list args;

	va_start(args, fmt);
	finally(file, line, fmt, args);
}

void vcheckFail(int aNumber, const char * file, int line, const char * fmt, ...) {
	if (aNumber < 0) {
		va_list args;
		logFatal("Failed, receive: %d, more details in next line:", aNumber);

		va_start(args, fmt);
		finally(file, line, fmt, args);
	}
}

void vcheckIsNotNull(void * aPointer, const char * file, int line, const char * fmt, ...) {
	if(aPointer == (void *) 0) {
		va_list args;
		logFatal("Failed: null pointer received, more details in next line:", aPointer);

		va_start(args, fmt);
		finally(file, line, fmt, args);
	}
}

void vcheckIsNull(void * aPointer, const char * file, int line, const char * fmt, ...) {
	if(aPointer != (void *) 0) {
		va_list args;
		logFatal("Failed: null pointer expected, received: %d, more details in next line:", aPointer);

		va_start(args, fmt);
		finally(file, line, fmt, args);
	}
}

void vcheckAreEquals(int aNumber, int otherNumber, const char * file, int line, const char * fmt, ...) {
	if(aNumber != otherNumber) {
		va_list args;
		logFatal("Failed: equals numbers expected, receive: %d and %d, more details in next line:", aNumber, otherNumber);

		va_start(args, fmt);
		finally(file, line, fmt, args);
	}
}

