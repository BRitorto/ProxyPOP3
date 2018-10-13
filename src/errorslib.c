#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "logger.h"
#include "errorslib.h"

static const char * msgError[] = {
	"Check fail, more details in next line.",
	"Check failed: null pointer expected, more details in next line.",
	"Check failed: null pointer received, more details in next line.",
	"Check failed: equals numbers expected, more details in next line.",
	"Check failed: differents numbers expected, more details in next line.",
	"Check failed: greater number expeted, more details in next line.",
};


static inline void fatalFinally(const char * file, int line, const char * fmt, va_list args) {
	vlogLogger(LOG_LEVEL_FATAL, file, line, fmt, args);
	va_end(args);
	exit(1);
}

void inFail(const char * file, int line, const char * fmt, ...) {		
	va_list args;

	va_start(args, fmt);
	fatalFinally(file, line, fmt, args);
}

void checkCondition(checkType type, int condition, const char * file, int line, const char * fmt, ...) {
	if (!condition) {
		va_list args;

		logFatal(msgError[type]);
		va_start(args, fmt);
		fatalFinally(file, line, fmt, args);
	}
}

void checkConditionWithFinally(checkType type, int condition, finallyFunc finally, void * data, const char * file, int line, const char * fmt, ...) {
	if (!condition) {
		va_list args;		
		
		logFatal(msgError[type]);

		va_start(args, fmt);
		vlogLogger(LOG_LEVEL_FATAL, file, line, fmt, args);
		va_end(args);
		finally(data);
	}
}

