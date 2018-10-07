#ifndef PROCESS_LIB_H
#define PROCESS_LIB_H

#include <sys/select.h>

pid_t * childFactory(int qty, char * childName);

pid_t * childFactoryWithArgs(int qty, char * childPath, char ** args);

fd_set createASetOfFds(int qty, ...);

void waitForFds(int lastFd, fd_set fdReadSet);

void freeSpace(int qty, ...);

#endif

