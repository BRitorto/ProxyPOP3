//Multiplexor.h
#ifndef MULTIPLEXOR_H
#define MULTIPLEXOR_H

#include <sys/time.h>
#include <stdbool.h>

#define FDS_MAX_SIZE FD_SETSIZE


typedef struct MultiplexorCDT * MultiplexorADT;

typedef enum multiplexorStatus {
        SUCCESS             = 0,
        MAX_FDS             = 1,
        NO_MEMORY           = 2,
        ILLEGAL_ARGUMENTS   = 3,
        FD_IN_USE           = 4,
        IO_ERROR            = 5,    
} multiplexorStatus;

typedef enum fdInterest {
        NO_INTEREST = 0,
        READ        = 1 << 0,
        WRITE       = 1 << 2,
} fdInterest;


typedef struct MultiplexorKeyCDT {
    MultiplexorADT mux;
    int            fd;
    void *         data;
} MultiplexorKeyCDT;

typedef MultiplexorKeyCDT * MultiplexorKey;

typedef struct eventHandler {
    void (* read)  (MultiplexorKey key);
    void (* write) (MultiplexorKey key);
    void (* block) (MultiplexorKey key);
    void (* close) (MultiplexorKey key);
} eventHandler;

struct multiplexorInit {
    const int signal;
    struct timespec selectTimeout;
};

const char * multiplexorError(const multiplexorStatus status);


/** inicializa la librería */
multiplexorStatus
selector_init(const struct selector_init *c);

/** deshace la incialización de la librería */
multiplexorStatus
selector_close(void);

MultiplexorADT createMultiplexorADT (const size_t initialElements);

void deleteMultiplexorADT(MultiplexorADT mux);

multiplexorStatus registerFd(MultiplexorADT mux, const int fd, const eventHandler * handler, const fdInterest interest, void * data);

multiplexorStatus unregisterFd(MultiplexorADT mux, const int fd);

multiplexorStatus setInterest(MultiplexorADT mux, int fd, fdInterest interest);

multiplexorStatus setInterestKey(MultiplexorKey key, fdInterest interest);

multiplexorStatus notifyBlock(MultiplexorADT  mux, const int fd);

multiplexorStatus muxSelect(MultiplexorADT mux);

int fdSetNIO(const int fd);


#endif
