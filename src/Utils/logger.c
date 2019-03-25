/**
* logger.c - Permite manejar el logging.
*/

#include <stdio.h>
#include "logger.h"
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

static struct {
	int 		level;
	bool 		quiet;
	bool 		color;
	FILE * 		fileLevel[LOG_LEVEL_METRIC + 1];
	void *		udata;
	log_LockFn 	lock;
} logger;

static const char * levelNames[] = {
	"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "METRIC"
};

static const char * levelColors[] = {
  "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m", "\x1b[94m"
};

void loggerSetUdata(void *udata) {
  	logger.udata = udata;
}

void loggerSetLock(log_LockFn fn) {
  	logger.lock = fn;
}

void loggerClearFiles(void) {
	for(size_t i = 0; i <= LOG_LEVEL_METRIC; i++)
 		logger.fileLevel[i] = NULL;
}

void loggerSetFileByLevel(FILE * fileLevel, size_t level) {
	if(level <= LOG_LEVEL_METRIC)
 		logger.fileLevel[level] = fileLevel;
}

void loggerSetLevel(int level) {
  	logger.level = level;
}

void loggerSetQuiet(bool enable) {
  	logger.quiet = enable;
}

void loggerSetColor(bool enable) {
  	logger.color = enable;
}

static inline void lock() {
  	if (logger.lock)
    	logger.lock(logger.udata, 1);
}

static inline void unlock() {
  	if (logger.lock)
    	logger.lock(logger.udata, 0);
}

logStatus vlogLogger(int level, const char * file, int line, const char *fmt, va_list args) {
	if (level < logger.level) {
		return LOG_NO_LEVEL;
	}

	/* Acquire lock */
	lock();

	/* Get current time */
	time_t t = time(NULL);
	struct tm localTm;
	struct tm *tm = localtime_r(&t, &localTm);

	/* Log to stderr */
	if (!logger.quiet) {
		char buf[64];
		buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm)] = '\0';
		if(logger.color)
			fprintf(stderr, "%s %s%-6s\x1b[0m \x1b[90m%s:%d:\x1b[0m ",
					buf, levelColors[level], levelNames[level], file, line);
		else
			fprintf(stderr, "%s %-6s %s:%d: ", buf, levelNames[level], file, line);
		vfprintf(stderr, fmt, args);
		fprintf(stderr, "\n");
		fflush(stderr);
	}

	/* Log to file */
	if (logger.fileLevel[level] != NULL) {
		char buf[64];
		buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm)] = '\0';
		fprintf(logger.fileLevel[level], "%s %-6s %s:%d: ", buf, levelNames[level], file, line);
		vfprintf(logger.fileLevel[level], fmt, args);
		fprintf(logger.fileLevel[level], "\n");
		fflush(logger.fileLevel[level]);
	}

	/* Release lock */
	unlock();
	return LOG_SUCCESS;
}

inline logStatus logLogger(int level, const char * file, int line, const char *fmt, ...) {
	va_list args;
	logStatus retVal;

    va_start(args, fmt);
	retVal = vlogLogger(level, file, line, fmt, args);
    va_end(args);
	return retVal;
}

