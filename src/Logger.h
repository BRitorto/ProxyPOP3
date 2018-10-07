#ifndef LOGGGER_H
#define LOGGGER_H

#define MAX_DATE_SIZE 64

typedef enum logType {
	LOG_WARNING = 0,
	LOG_ERROR 	= 1,
	LOG_DEBUG 	= 2,
	LOG_METRIC 	= 3,
	LOG_INFO 	= 4,
	LOG_FATAL 	= 5,
} logType;

typedef enum logStatus {
	LOGG_ERROR = 1,
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

