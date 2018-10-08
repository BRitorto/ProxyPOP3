#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/types.h>
#include <stdarg.h>
#include "errorslib.h"
#include "processlib.h"

pid_t * childFactory(int qty, char * childName)
{
	pid_t * children = malloc(qty * sizeof(pid_t*));

	for (int i = 0; i < qty; i++)
	{ 
		children[i] = fork();
		checkFail(children[i], "fork() child: %d", i);
		
		if (children[i] == 0)
		{
			execlp(childName, " ", ((char *)NULL));
			fail("exec() child: %d", i);
		}
	}
	return children;
}

pid_t * childFactoryWithArgs(int qty, char * childPath, char ** args)
{
	pid_t * children = malloc(qty * sizeof(pid_t*));

	for (int i = 0; i < qty; i++)
	{ 
		children[i] = fork();
		checkFail(children[i], "fork() child: %d", i);

		if (children[i] == 0)
		{
			execlp(childPath, args[i], ((char *)NULL));
			fail("exec() child: %d", i);
		}
	}
	return children;
}

fd_set createASetOfFds(int qty, ...)
{
	va_list args;
    va_start(args, qty);
	fd_set fdSet;
    FD_ZERO(&fdSet);

	for (int i = 0; i < qty; i++)
	{
		int fd = va_arg(args, int);
        FD_SET(fd, &fdSet);
	}
    va_end(args);
    return fdSet;
}

void waitForFds(int lastFd, fd_set fdReadSet)
{
    int result = select(lastFd + 1, &fdReadSet, 0, 0, NULL);
   	checkFail(result, "select()");
}

void freeSpace(int qty, ...) 
{
	va_list args;
    va_start(args, qty);

    for (int i = 0; i < qty; i++)
        free(va_arg(args, void *));

    va_end(args);
}

