//MultiplexorADT.h
#ifndef MULTIPLEXORADT_H
#define MULTIPLEXORADT_H

#define FDS_MAX_SIZE FD_SETSIZE


#define USED_FD_TYPE(i) ( ( FD_UNUSED != (i)->fd) )
#define INVALID_FD(fd)  ((fd) < 0 || (fd) >= FDS_MAX_SIZE)



typedef MultiplexorCDT * MultiplexorADT;

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
