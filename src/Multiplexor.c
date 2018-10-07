

//Multiplexor.c
#include "Multiplexor.h"
#include <assert.h>  
#include <stdio.h>  
#include <string.h> 
#include <errno.h>
#include <pthread.h>
#include <stdint.h> // SIZE_MAX
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/signal.h>


#define USED_FD_TYPE(i) ( ( FD_UNUSED != (i)->fd) )
#define INVALID_FD(fd)  ((fd) < 0 || (fd) >= FDS_MAX_SIZE)
#define ERROR_DEFAULT_MSG "something failed"

typedef struct fdType {
    int                  fd;
    fdInterest           interest;
    const eventHandler * handler;
    void *               data;
} fdType;

typedef struct blockingTask {
    MultiplexorADT        mux;
    int                   fd;
    void *                data;
    struct blockingTask * next;
}blockingTask;

typedef struct MultiplexorCDT {
    fdType * fds;
    size_t   size;

    size_t   maxFd;
    fd_set   readSet;
    fd_set   writeSet;
    fd_set   backUpReadSet;
    fd_set   backUpWriteSet;
    
    struct timespec prototipicTimeout;
    struct timespec backUpPrototipicTimeout;

    volatile pthread_t muxThread;
    pthread_mutex_t    resolutionMutex;
    blockingTask *     resolutionTasks;
} MultiplexorCDT;

// señal a usar para las notificaciones de resolución
struct multiplexorInit conf;
static sigset_t emptyset, blockset;

static const int FD_UNUSED = -1;


static void wakeHandler(const int signal) {
    // nada que hacer. está solo para interrumpir el select
}

multiplexorStatus multiplexorInit(const struct multiplexorInit * c) {
    memcpy(&conf, c, sizeof(conf));

    multiplexorStatus retVal = SUCCESS;
    struct sigaction act     = {
        .sa_handler = wakeHandler,
    };

    sigemptyset(&blockset);
    sigaddset  (&blockset, conf.signal);
    if(-1 == sigprocmask(SIG_BLOCK, &blockset, NULL)) {
        retVal = IO_ERROR;
        goto finally;
    }

    if (sigaction(conf.signal, &act, 0)) {
        retVal = IO_ERROR;
        goto finally;
    }
    sigemptyset(&emptyset);

finally:
    return retVal;
}

const char * multiplexorError(const multiplexorStatus status) {
    const char * msg;
    switch(status) {
        case SUCCESS:
            msg = "Success";
            break;
        case MAX_FDS:
            msg = "Can't handle any more file descriptors";
            break;
        case NO_MEMORY:
            msg = "Not enough memory";
            break;
        case INVALID_ARGUMENTS:
            msg = "Invalid arguments";
            break;
        case FD_IN_USE:
            msg = "File descriptor already in use";
            break;
        case IO_ERROR:
            msg = "I/O error";
            break;
        default:
            msg = ERROR_DEFAULT_MSG;
    }
    return msg;
}




multiplexorStatus multiplexorClose(void) {
    // Nada para liberar.
    // TODO(juan): podriamos reestablecer el handler de la señal.
    return SUCCESS;
}

static inline void fdInitialize(fdType * fd) {
    fd->fd = FD_UNUSED;
    fd->data = NULL;
}

static void initialize(MultiplexorADT mux, const size_t lastIndex) {
    assert(lastIndex <= mux->size);
    for(size_t i = lastIndex; i < mux->size ; i++)
        fdInitialize(mux->fds + i);
}

static int getMaxFd(MultiplexorADT mux) {
    int maxFd = 0;
    for(int i = 0; i <= (int)mux->maxFd; i++) {
        fdType * fd = mux->fds + i; // == &(mux->fds[i])
        if(USED_FD_TYPE(fd)) {
            if(fd->fd > maxFd)
                maxFd = fd->fd;
        }
    }
    return maxFd;
}

static void updateSet(MultiplexorADT mux, const fdType * fd) {
    FD_CLR(fd->fd, &(mux->readSet));
    FD_CLR(fd->fd, &(mux->writeSet));

    if(USED_FD_TYPE(fd)) {
        if(fd->interest & READ)
            FD_SET(fd->fd, &(mux->readSet));
        if(fd->interest & WRITE)
            FD_SET(fd->fd, &(mux->writeSet));
    }
}

static size_t nextCapacity(const size_t n) {
    unsigned bits = 0;
    size_t temp = n;
    while(temp != 0) { //next highest power of 2
        temp >>= 1;
        bits++;
    }
    temp = 1UL << bits;

    assert(temp >= n);
    if(temp > FDS_MAX_SIZE) {
        temp = FDS_MAX_SIZE;
    }

    return temp + 1;
}

/*
static size_t nextHighestPowerOf2(size_t n) 
{
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;   
    
}*/

static multiplexorStatus ensureCapacity(MultiplexorADT mux, const size_t n) {
    multiplexorStatus retVal = SUCCESS;

    const size_t elementSize = sizeof(*mux->fds);
    if(n < mux->size) 
        retVal = SUCCESS;
    else if(n > FDS_MAX_SIZE) 
        retVal = MAX_FDS;
    else if(NULL == mux->fds) {
        const size_t newSize = nextCapacity(n);
        mux->fds = calloc(newSize, elementSize);
        if(NULL == mux->fds)
            retVal = NO_MEMORY;
        else {
            mux->size = newSize;
            initialize(mux, 0);
        }
    } else {
        const size_t newSize = nextCapacity(n);
        if (newSize > SIZE_MAX/elementSize)
            retVal = NO_MEMORY;
        else {
            fdType * temp = realloc(mux->fds, newSize * elementSize);
            if(NULL == temp) {
                retVal = NO_MEMORY;
            } else {
                mux->fds  = temp;
                const size_t oldSize = mux->size;
                mux->size = newSize;

                initialize(mux, oldSize);
            }
        }
    }

    return retVal;
}


MultiplexorADT createMultiplexorADT (const size_t initialElements) {
    size_t size = sizeof(MultiplexorCDT);
    MultiplexorADT mux = malloc(size);
    if(mux != NULL) {
        memset(mux, 0x00, size);
        mux->prototipicTimeout.tv_sec  = conf.selectTimeout.tv_sec;
        mux->prototipicTimeout.tv_nsec = conf.selectTimeout.tv_nsec;
        assert(mux->maxFd == 0);
        mux->resolutionTasks  = 0;
        pthread_mutex_init(&mux->resolutionMutex, 0);
        if(0 != ensureCapacity(mux, initialElements)) {
            deleteMultiplexorADT(mux);
            mux = NULL;
        }
    }
    return mux;
}

void deleteMultiplexorADT(MultiplexorADT mux) {
    if(mux != NULL) {
        if(mux->fds != NULL) {
            printf("SIZE: %d\n", (int)mux->size);
            for(size_t i = 0; i < mux->size; i++) {
                if(USED_FD_TYPE(mux->fds + i)) {
                    printf("%d\n", (int)i);
                    unregisterFd(mux, i);
                }
            }
            pthread_mutex_destroy(&mux->resolutionMutex);
            blockingTask * t, *next;
            for(t = mux->resolutionTasks; t != NULL; t = next) {
                next = t->next;
                free(t);
            }
            free(mux->fds);
            mux->fds = NULL;
            mux->size = 0;
        }
        free(mux);
    }
}

multiplexorStatus registerFd(MultiplexorADT mux, const int fd, const eventHandler * handler, const fdInterest interest, void * data) {
    multiplexorStatus retVal = SUCCESS;
    if(mux == NULL || INVALID_FD(fd) || handler == NULL) {
        retVal = INVALID_ARGUMENTS;
        goto finally;
    }
    size_t ufd = (size_t)fd;
    if(ufd > mux->size) {
        retVal = ensureCapacity(mux, ufd);
        if(SUCCESS != retVal) {
            goto finally;
        }
    }

    fdType * newFdType = mux->fds + ufd;
    if(USED_FD_TYPE(newFdType)) {
        retVal = FD_IN_USE;
            goto finally;
        } else {
        newFdType->fd       = fd;
        newFdType->handler  = handler;
        newFdType->interest = interest;
        newFdType->data     = data;

        if(fd > (int)mux->maxFd) {
            mux->maxFd = fd;
        }
        updateSet(mux, newFdType);
    }

finally:
    return retVal;
}

multiplexorStatus unregisterFd(MultiplexorADT mux, const int fd) {
    multiplexorStatus retVal = SUCCESS;

    if(NULL == mux || INVALID_FD(fd)) {
        retVal = INVALID_ARGUMENTS;
        goto finally;
    }

    fdType * newFdType = mux->fds + fd;
    assert(fd == newFdType->fd);
    if(!USED_FD_TYPE(newFdType)) {
        retVal = INVALID_ARGUMENTS;
        goto finally;
    }

    if(newFdType->handler != NULL && newFdType->handler->close != NULL) {
        //printf("%d, %p\n", fd, (void *)newFdType->handler);

        MultiplexorKeyCDT key = {
            .mux    = mux,
            .fd   = newFdType->fd,
            .data = newFdType->data,
        };
        newFdType->handler->close(&key);
    }

    newFdType->interest = NO_INTEREST;
    updateSet(mux, newFdType);

    memset(newFdType, 0x00, sizeof(*newFdType));
    fdInitialize(newFdType);
    newFdType->handler = NULL;

    mux->maxFd = getMaxFd(mux);

finally:
    return retVal;
}

multiplexorStatus setInterest(MultiplexorADT mux, int fd, fdInterest interest) {
    multiplexorStatus retVal = SUCCESS;

    if(NULL == mux || INVALID_FD(fd)) {
        retVal = INVALID_ARGUMENTS;
        goto finally;
    }
    fdType * newFdType = mux->fds + fd;
    if(!USED_FD_TYPE(newFdType)) {
        retVal = INVALID_ARGUMENTS;
        goto finally;
    }
    newFdType->interest = interest;
    updateSet(mux, newFdType);
finally:
    return retVal;
}

multiplexorStatus setInterestKey(MultiplexorKey key, fdInterest interest) {
    multiplexorStatus retVal;

    if(NULL == key || NULL == key->mux || INVALID_FD(key->fd))
        retVal = INVALID_ARGUMENTS;
    else 
        retVal = setInterest(key->mux, key->fd, interest);
    
    return retVal;
}

static void manageIteration(MultiplexorADT mux) {
    int n = mux->maxFd;
    MultiplexorKeyCDT key = {
        .mux = mux,
    };

    for (int i = 0; i <= n; i++) {
        fdType * currentFdType = mux->fds + i;
        if(USED_FD_TYPE(currentFdType)) {
            key.fd   = currentFdType->fd;
            key.data = currentFdType->data;
            if(FD_ISSET(currentFdType->fd, &mux->backUpReadSet)) {
                if(READ & currentFdType->interest) {
                    if(0 == currentFdType->handler->read) {
                        assert(("READ arrived but no handler. bug!" == 0)); //LOG VILLA
                    } else {
                        currentFdType->handler->read(&key);
                    }
                }
            }
            if(FD_ISSET(i, &mux->backUpWriteSet)) {
                if(WRITE & currentFdType->interest) {
                    if(0 == currentFdType->handler->write) {
                        assert(("WRITE arrived but no handler. bug!" == 0)); // LOG VILLA
                    } else {
                        currentFdType->handler->write(&key);
                    }
                }
            }
        }
    }
}

static void manageBlockNotifications(MultiplexorADT mux) {
    MultiplexorKeyCDT key = {
        .mux = mux,
    };
    pthread_mutex_lock(&mux->resolutionMutex);

    blockingTask * t, * next;
    for(t = mux->resolutionTasks; t != NULL ; t  = next) {

        fdType * currentFdType = mux->fds + t->fd;
        if(USED_FD_TYPE(currentFdType)) {
            key.fd   = currentFdType->fd;
            key.data = currentFdType->data;
            currentFdType->handler->block(&key);
        }
        
        next = t->next;
        free(t);
    }
    mux->resolutionTasks = 0;
    pthread_mutex_unlock(&mux->resolutionMutex);
}


multiplexorStatus notifyBlock(MultiplexorADT  mux, const int fd) {
    multiplexorStatus retVal = SUCCESS;

    blockingTask * task = malloc(sizeof(*task));
    if(task == NULL) {
        retVal = NO_MEMORY;
        goto finally;
    }
    task->mux  = mux;
    task->fd = fd;

    pthread_mutex_lock(&mux->resolutionMutex);
    task->next = mux->resolutionTasks;
    mux->resolutionTasks = task;
    pthread_mutex_unlock(&mux->resolutionMutex);
    pthread_kill(mux->muxThread, conf.signal);

finally:
    return retVal;
}

multiplexorStatus muxSelect(MultiplexorADT mux) {
    multiplexorStatus retVal = SUCCESS;

    memcpy(&mux->backUpReadSet, &mux->readSet, sizeof(mux->backUpReadSet));
    memcpy(&mux->backUpWriteSet, &mux->writeSet, sizeof(mux->backUpWriteSet));
    memcpy(&mux->backUpPrototipicTimeout, &mux->prototipicTimeout, sizeof(mux->backUpPrototipicTimeout));

    mux->muxThread = pthread_self();

    int fds = pselect(mux->maxFd + 1, &mux->backUpReadSet, &mux->backUpWriteSet, 0, &mux->backUpPrototipicTimeout,
                      &emptyset);
    if(-1 == fds) {
        switch(errno) {
            case EAGAIN:
            case EINTR:
                // si una señal nos interrumpio. ok!
                break;
            case EBADF:
                // ayuda a encontrar casos donde se cierran los fd pero no
                // se desregistraron
                for(int i = 0 ; i < (int)mux->maxFd; i++) {
                    if(FD_ISSET(i, &mux->readSet)|| FD_ISSET(i, &mux->writeSet)) {
                        if(-1 == fcntl(i, F_GETFD, 0)) {
                            fprintf(stderr, "Bad file descriptor detected: %d\n", i);
                        }
                    }
                }
                retVal = IO_ERROR;
                break;
            default:
                retVal = IO_ERROR;
                goto finally;

        }
    } else {
        manageIteration(mux);
    }
    if(retVal == SUCCESS) {
        manageBlockNotifications(mux);
    }
finally:
    return retVal;
}

int fdSetNIO(const int fd) {
    int ret = 0;
    int flags = fcntl(fd, F_GETFD, 0);
    if(flags == -1) {
        ret = -1;
    } else {
        if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            ret = -1;
        }
    }
    return ret;
}



