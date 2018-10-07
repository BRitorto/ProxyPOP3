/**
* Logger.c - Un ADT que permite manejar el logging del servidor.
*/

#include "Logger.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_LOG_TYPE_SIZE 10
#define MAX_LOG_MSG_SIZE 64
#define MAX_IP_SIZE 16
#define MAX_LOG_SIZE MAX_LOG_TYPE_SIZE + MAX_DATE_SIZE + MAX_LOG_TYPE_SIZE + MAX_LOG_MSG_SIZE + 2*MAX_IP_SIZE

typedef struct LoggerCDT {
	const char * 	warningFilePath;
	const char * 	metricFilePath;
	int 			warningFd;
	int 			metricFd;

} LoggerCDT;

LoggerADT createLogger(int warningFd, int metricFd) {
	LoggerADT newLogger = malloc(sizeof(LoggerCDT));
	if(newLogger != NULL) {
		memset(newLogger, 0, sizeof(LoggerCDT));
		newLogger->warningFd = warningFd;
		newLogger->metricFd = metricFd;
	}
	return newLogger;
}

LoggerADT createLoggerWithFiles(const char * warningFilePath, const char * metricFilePath) {
	int warningFd = open(warningFilePath, O_WRONLY | O_CREAT);
	int metricFd = open(metricFilePath, O_WRONLY | O_CREAT);

	if(warningFd >= 0 && metricFd >= 0) {
		LoggerADT newLogger = createLogger(warningFd, metricFd);
		if(newLogger != NULL) {
			newLogger->warningFilePath = warningFilePath;
			newLogger->metricFilePath = metricFilePath;
		}
		return newLogger;
	}
	return NULL;
}

static inline void setDateTime(Log * log) {
	struct timeval tv;
	gettimeofday (&tv, NULL);
	struct tm *nowtm;
	time_t nowtime = tv.tv_sec;
	nowtm = localtime(&nowtime);
	strftime(log->date, sizeof(log->date), "%Y-%m-%d %H:%M:%S", nowtm);
}

static inline const char * logTypeToString(logType type) {
	char * ret = NULL;

	switch(type) {
		case WARNING:
			ret = "WARNING";
			break;
		case ERROR:
			ret = "ERROR";
			break;
		case INFO:
			ret = "INFO";
			break;
		case DEBUG:
			ret = "DEBUG";
			break;
		case METRIC:
			ret = "METRIC";
			break;
		case FATAL:
			ret = "FATAL";
			break;
		default:
			ret = "UNDEFINED";
			break;
	}
	return ret;
} 

logStatus logLogger(LoggerADT logger, Log * log) {
	if(logger != NULL && log != NULL) {

		setDateTime(log);
		char logString[MAX_LOG_SIZE];
		sprintf(logString, "%s - %s: <%s> PID:%d RIP:%s LIP: %s\n", log->date, logTypeToString(log->type), log->message, log->pid, log->remoteIp, log-> localIp);
		int fd = -1;
		if(log->type == METRIC) {
			fd = logger->metricFd;
		} else if (log->type == WARNING || log->type == ERROR || log->type == FATAL || log->type == DEBUG || log ->type == INFO) {
			fd = logger->warningFd;
		}
		if(fd > 0)
			write(fd, logString, MAX_LOG_SIZE);

	}
	return LOG_ERROR;
}

void deleteLogger(LoggerADT logger) {
	if(logger != NULL) {
		if(logger->warningFd >= 0)
			close(logger->warningFd);
		if(logger->metricFd >= 0)
			close(logger->metricFd);
		free(logger);
	}
}

