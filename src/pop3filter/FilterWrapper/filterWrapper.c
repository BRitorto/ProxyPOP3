#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "buffer.h"
#include "multiplexor.h"
#include "bodyPop3Parser.h"

#define SELECT_SET_SIZE 1024
#define BUFFER_SIZE 1000

typedef struct copyStruct {
    int                 fd[2];
    bufferADT           readBuffer;
    bufferADT           writeBuffer;
    fdInterest          duplex;
    struct copyStruct * other;
} copyStruct;

typedef struct filterWrapper {
    int inProxyFd;
    int outProxyFd;
    int inFilterFd;
    int outFilterFd;

    bufferADT           readBuffer;    
    bufferADT           writeBuffer;
    bufferADT           skipBuffer;
    bufferADT           addBuffer;

    bodyPop3Parser      skipParser;
    bodyPop3Parser      addParser;

    size_t              sizeEndCopied;

    union {
        copyStruct      copy;
    } proxy;
    union {
        copyStruct      copy;
    } filter;
} filterWrapper;

/** Obtiene el filterWrapper* desde la llave de selección.  */
#define ATTACHMENT(key) ( (struct filterWrapper *)(key)->data)

static void copyInit(filterWrapper * wrapper) {
    copyStruct * copy  = &(wrapper->proxy.copy);
    copy->fd[0]        = wrapper->outProxyFd;
    copy->fd[1]        = wrapper->inProxyFd;
    copy->readBuffer   = wrapper->readBuffer;
    copy->writeBuffer  = wrapper->writeBuffer;
    copy->duplex       = READ | WRITE;
    copy->other        = &(wrapper->filter.copy);

    copy               = &(wrapper->filter.copy);
    copy->fd[0]        = wrapper->outFilterFd;
    copy->fd[1]        = wrapper->inFilterFd;
    copy->readBuffer   = wrapper->addBuffer;
    copy->writeBuffer  = wrapper->skipBuffer;
    copy->duplex       = READ | WRITE;
    copy->other        = &(wrapper->proxy.copy);
}

static inline fdInterest copyComputeInterest(MultiplexorADT mux, copyStruct * copy) {
    fdInterest retWrite = NO_INTEREST, retRead = NO_INTEREST;
    size_t size;

    if(copy->fd[0] != -1) {
        getWritePtr(copy->other->writeBuffer, &size);
        if((copy->duplex & READ) && size >= 2 && canWrite(copy->readBuffer)) 
            retRead = READ;
        if(MUX_SUCCESS != setInterest(mux, copy->fd[0], retRead))
            abort();
    }

    if(copy->fd[1] != -1) {
        if((copy->duplex & WRITE) && canRead(copy->writeBuffer))
            retWrite = WRITE;
        if(MUX_SUCCESS != setInterest(mux, copy->fd[1], retWrite))
            abort();
    }

    return retRead | retWrite;
}

static copyStruct * copyPtr(MultiplexorKey key) {
    filterWrapper * wrapper = ATTACHMENT(key);

    if(key->fd == wrapper->inProxyFd || key->fd == wrapper->outProxyFd)
        return &wrapper->proxy.copy;
    return  &wrapper->filter.copy;
}

static const char * terminationMsg      = ".\r\n";
static const size_t terminationMsgSize  = 3;

static void terminationToBuffer(bufferADT buffer, filterWrapper * wrapper) {
    size_t    size;
    uint8_t * ptr = getWritePtr(buffer, &size);

    size = (size > terminationMsgSize - wrapper->sizeEndCopied) ? terminationMsgSize - wrapper->sizeEndCopied : size;
    memcpy(ptr, terminationMsg + wrapper->sizeEndCopied, size);
    wrapper->sizeEndCopied += size;
    updateWriteAndProcessPtr(buffer, size);
}

static void endHandler(filterWrapper * wrapper, MultiplexorADT mux, bool workAsSlave);

static void copyRead(MultiplexorKey key) {
    filterWrapper * wrapper = ATTACHMENT(key);
    copyStruct    * copy    = copyPtr(key);

    size_t size;
    ssize_t n;
    bool skip, errored;
    bufferADT buffer = copy->readBuffer;

    uint8_t * ptr = getWritePtr(buffer, &size);
    n = read(key->fd, ptr, size);
    if(n <= 0) {
        unregisterFd(key->mux, key->fd);
        close(key->fd);
        copy->fd[0] = -1;
        copy->duplex &= ~READ;        
        if(key->fd == wrapper->outProxyFd)
            wrapper->outProxyFd = -1;
        else {
            if(key->fd == wrapper->outFilterFd && !canRead(copy->other->writeBuffer)) 
                terminationToBuffer(copy->other->writeBuffer, wrapper);
            wrapper->outFilterFd = -1;
        }
        if(!canRead(copy->other->writeBuffer)) {        
            unregisterFd(key->mux, copy->other->fd[1]);
            close(copy->other->fd[1]);
            copy->other->fd[1] = -1;
            copy->other->duplex &= ~WRITE;
        }
    } else {        
        updateWritePtr(buffer, n);
        skip = key->fd == wrapper->outProxyFd;
        if(skip)
            bodyPop3ParserConsume(&wrapper->skipParser, buffer, copy->other->writeBuffer, skip, &errored);
        else
            bodyPop3ParserConsume(&wrapper->addParser, buffer, copy->other->writeBuffer, skip, &errored);
        if(errored) {
            endHandler(wrapper, key->mux, false);
        }
    }
    copyComputeInterest(key->mux, copy);
    copyComputeInterest(key->mux, copy->other);
    
    if(copy->duplex == NO_INTEREST && !canRead(copy->other->writeBuffer)) {
        endHandler(wrapper, key->mux, false);
    }
}

/** escribe bytes encolados */
static void copyWrite(MultiplexorKey key) {
    filterWrapper  * wrapper = ATTACHMENT(key);
    struct copyStruct * copy = copyPtr(key);

    size_t size;
    ssize_t n;  
    bufferADT buffer = copy->writeBuffer;

    uint8_t * ptr = getReadPtr(buffer, &size);
    n = write(key->fd, ptr, size);
    if(n == -1) {        
        unregisterFd(key->mux, key->fd);
        close(key->fd);
        copy->fd[1] = -1;
        copy->duplex &= ~WRITE;
        endHandler(wrapper, key->mux, false);
    } else {
        updateReadPtr(buffer, n);
        if((copy->other->duplex & READ) == NO_INTEREST && !canRead(buffer)) {
            if(key->fd == wrapper->inProxyFd && wrapper->sizeEndCopied < terminationMsgSize) 
                terminationToBuffer(copy->writeBuffer, wrapper);
            else {  
                unregisterFd(key->mux, key->fd); 
                close(key->fd);
                copy->fd[1] = -1;
                copy->duplex &= ~WRITE;
                if(key->fd == wrapper->inProxyFd)
                    endHandler(wrapper, key->mux, false);
            }                
        }
    }
    copyComputeInterest(key->mux, copy);
    copyComputeInterest(key->mux, copy->other);

    if(copy->duplex == NO_INTEREST) 
        endHandler(wrapper, key->mux, false);
}

static bool done = false;
static int infd, outfd;
static pid_t pid;
static size_t bufferSize;

/**
 * Manejador de la señal SIGTERM.
 */
static void sigTermHandler(const int signal) {
    done = true;
    if(infd != -1)
        close(infd);
    if(outfd != -1)
        close(outfd);
}

/**
 * Manejador de la señal SIGCHILD.
 */
static void sigChildHandler(const int signal) {
    while(waitpid(-1, 0, WNOHANG) != -1);
}

/**
 * Pasa las variables de entorno para el programa de filtro externo.
 */
static void passEnvironment(void) {
    char * mediaRange = getenv("FILTER_MEDIAS");
    if(mediaRange != NULL)
        setenv("FILTER_MEDIAS", mediaRange, 1);

    setenv("FILTER_MSG",         getenv("FILTER_MSG"), 1);
    setenv("POP3FILTER_VERSION", getenv("POP3FILTER_VERSION"), 1);
    setenv("POP3_USERNAME",      getenv("POP3_USERNAME"), 1);
    setenv("POP3_SERVER",        getenv("POP3_SERVER") ,1);
}

static void workBlockingSlave(void) {
    uint8_t dataBuffer[BUFFER_SIZE];
    ssize_t n;
    do {
        n = read(STDIN_FILENO, dataBuffer, sizeof(dataBuffer));
        if(n > 0)
            write(STDOUT_FILENO, dataBuffer, n);
    } while(n > 0);
    exit(1);
}

int main(int argc, char * argv[]) { 
    int inFd[2], outFd[2];
    filterWrapper wrapper;
    memset(&wrapper, 0, sizeof(filterWrapper));
    MultiplexorADT mux = NULL;
    pid = -1;
       
    if(argc != 2)
        abort();

    infd  = -1;
    outfd = -1;

    if(pipe(inFd) < 0)
        abort();
    if(pipe(outFd) < 0)
        abort();

    pid = fork();
    if(pid < 0)
        abort();
    else if(pid == 0) {
        close(inFd[1]);
        close(outFd[0]);
        dup2(inFd[0], STDIN_FILENO);
        close(inFd[0]);
        dup2(outFd[1], STDOUT_FILENO);
        close(outFd[1]);
        close(STDERR_FILENO);
        open(argv[1], O_WRONLY | O_APPEND);
        passEnvironment();
        execl("/bin/sh", "sh", "-c", argv[0], (char *)0);
        workBlockingSlave();
    } else {
        close(inFd[0]);
        close(outFd[1]);
        infd = inFd[1];
        outfd = outFd[0];
        char * bufferSizeStr = getenv("BUFFER_SIZE");
        if(bufferSizeStr != NULL)
            bufferSize = atoi(bufferSizeStr);
        else
            bufferSize = BUFFER_SIZE;

        signal(SIGTERM,  sigTermHandler);
        signal(SIGINT,   sigTermHandler);
        signal(SIGCHLD, sigChildHandler);

        const struct multiplexorInit conf = {
            .signal = SIGALRM,
            .selectTimeout = {
                .tv_sec  = 10,
                .tv_nsec = 0,
            },
        };

        if(multiplexorInit(&conf) != 0)
            endHandler(&wrapper, mux, true);

        mux = createMultiplexorADT(SELECT_SET_SIZE);
        if(mux == NULL)
            endHandler(NULL, mux, true);

        const eventHandler wrapperHandler = {
            .read       = copyRead,
            .write      = copyWrite,
            .block      = NULL,
            .close      = NULL, // Nada que liberar por ahora.
            .timeout    = NULL,
        };

        if(fdSetNIO(infd) < 0)
            endHandler(NULL, mux, true);
        if(fdSetNIO(outfd) < 0)
            endHandler(NULL, mux, true);

        wrapper.inProxyFd   = STDOUT_FILENO;
        wrapper.outProxyFd  = STDIN_FILENO;
        wrapper.inFilterFd  = infd;
        wrapper.outFilterFd = outfd;
        wrapper.readBuffer  = createBuffer(bufferSize);
        /* el parser para consumir necesita un espacio mas en el buffer destino */
        wrapper.writeBuffer = createBuffer(bufferSize + 1);
        wrapper.skipBuffer  = createBuffer(bufferSize + 1);
        wrapper.addBuffer   = createBuffer(bufferSize);
        bodyPop3ParserInit(&wrapper.skipParser);
        bodyPop3ParserInit(&wrapper.addParser);
        copyInit(&wrapper);

        if(registerFd(mux, STDIN_FILENO, &wrapperHandler, READ, &wrapper) != MUX_SUCCESS)
            endHandler(&wrapper, mux, true);
        if(registerFd(mux, STDOUT_FILENO, &wrapperHandler, NO_INTEREST, &wrapper) != MUX_SUCCESS)
            endHandler(&wrapper, mux, true);
        if(registerFd(mux, infd, &wrapperHandler, NO_INTEREST, &wrapper) != MUX_SUCCESS)
            endHandler(&wrapper, mux, true);
        if(registerFd(mux, outfd, &wrapperHandler, NO_INTEREST, &wrapper) != MUX_SUCCESS)
            endHandler(&wrapper, mux, true);

        for(;!done;) {
            if(muxSelect(mux) != MUX_SUCCESS)
                endHandler(&wrapper, mux, true);
        }
        return 0;
    }

    return 1;
}

static void endHandler(filterWrapper * wrapper, MultiplexorADT mux, bool workAsSlave) {
    if(pid > 0)
        kill(pid, SIGKILL);
    
    if(wrapper != NULL) {
        deleteBuffer(wrapper->readBuffer);
        deleteBuffer(wrapper->writeBuffer);
        deleteBuffer(wrapper->addBuffer);
        deleteBuffer(wrapper->skipBuffer);
    }
    deleteMultiplexorADT(mux);
    close(infd);
    close(outfd);        
    if(workAsSlave)
        workBlockingSlave();
    exit(0);
}

