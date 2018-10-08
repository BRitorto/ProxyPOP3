#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

typedef enum logType {
	LOG_LEVEL_TRACE   = 0,
	LOG_LEVEL_DEBUG   = 1,
	LOG_LEVEL_INFO 	  = 2,
	LOG_LEVEL_WARNING = 3,
	LOG_LEVEL_ERROR   = 4,
	LOG_LEVEL_FATAL   = 5,
	LOG_LEVEL_METRIC  = 6,
} logType;

typedef enum logStatus {
	LOG_SUCCESS  = 0,
	LOG_NO_LEVEL = 1,
	LOG_ERROR    = 2,
} logStatus;


typedef void (*log_LockFn)(void *udata, int lock);


#define logTrace(...) logLogger(LOG_LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define logDebug(...) logLogger(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define logInfo(...)  logLogger(LOG_LEVEL_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define logWarn(...)  logLogger(LOG_LEVEL_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define logError(...) logLogger(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define logFatal(...) logLogger(LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)

void loggerSetUdata(void *udata);
void loggerSetLock(log_LockFn fn);
void loggerSetFdsByLevel(int * fdsLevels);
void loggerSetLevel(int level);
void loggerSetQuiet(bool enable);
void loggerSetColor(bool enable);

logStatus vlogLogger(int level, const char * file, int line, const char *fmt, va_list args);
logStatus logLogger(int level, const char * file, int line, const char *fmt, ...);

#endif

