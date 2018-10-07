#ifndef LOGGGER_H
#define LOGGGER_H

#define MAX_DATE_SIZE 64

typedef enum logType {
	WARNING = 0,
	ERROR 	= 1,
	DEBUG 	= 2,
	METRIC 	= 3,
	INFO 	= 4,
	FATAL 	= 5,
} logType;

typedef enum logStatus {
	LOG_ERROR = 1,
} logStatus;

typedef struct Log {
	logType type;
	char * 	message;
	char 	date[MAX_DATE_SIZE];
	int 	pid;
	char * 	remoteIp;
	char * 	localIp;
} Log;

typedef struct LoggerCDT * LoggerADT;

LoggerADT createLogger(int warningFd, int metricFd);

LoggerADT createLoggerWithFiles(const char * warningFilePath, const char * metricFilePath);

logStatus logLogger(LoggerADT logger, Log * log);

void deleteLogger(LoggerADT logger);

#endif

