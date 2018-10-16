#include "adminnio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
//#include <netinet/sctp.h>
#include <sys/types.h>
#include "proxyPopv3nio.h"
#include "buffer.h"
#include "logger.h"
#include "errorslib.h"
#include "stateMachine.h"
#include "rap.h"

#define BUFFER_SIZE 2048
#define ATTACHMENT(key) ( (struct admin *)(key)->data)



#define N(x) (sizeof(x)/sizeof((x)[0]))

typedef enum adminState {
    AUTHENTIFICATION,
    REPLY_AUTHENTIFICATION,
    TRANSACTION,
    UPDATE,
    DONE,
    ERROR,
} adminState;

typedef struct copy {
    int *           fd;
    bufferADT       readBuffer;
    bufferADT       writeBuffer;
    fdInterest      duplex;
    struct copy *   other;
} copy;

typedef struct admin {
    adminState state;
    int clientFd;
    bufferADT readBuffer;
    bufferADT writeBuffer;

    copy clientCopy;
    copy originCopy;

    /** maquinas de estados */
    struct stateMachineCDT stm;
    /** cantidad de referencias a este objeto. si es uno se debe destruir */
    unsigned references;
    /** siguiente en el pool */
    struct admin * next;
} admin;




// Static function definitions
static void adminRead(MultiplexorKey key);
static void adminWrite(MultiplexorKey key);
static admin * newAdmin(int clientFd, size_t bufferSize);
static const struct stateDefinition * adminDescribeStates(void);
static void connectionInit(const unsigned state, MultiplexorKey key);
static unsigned readCredentials(MultiplexorKey key);
static void adminDone(MultiplexorKey key);
static int checkCredentials(MultiplexorKey key);
static void deleteAdmin( admin* a);
static unsigned reply_auth(MultiplexorKey key);
//static void deleteAdmin( admin* a);
// end definitions




static const eventHandler adminHandler = {
    .read   = adminRead,
    .write  = adminWrite,//adminWrite,
    .block  = NULL,//adminBlock,
    .close  = NULL,//adminClose,
};

/**
 * Pool de `struct admin', para ser reusados.
 *
 * Como tenemos un unico hilo que emite eventos no necesitamos barreras de
 * contención.
 * 
 * copiado de pop3
 */

static const unsigned  maxPool  = 50; // tamaño máximo
static unsigned        poolSize = 0;  // tamaño actual
static struct admin  * pool      = 0;  // pool propiamente dicho






static admin * newAdmin(int clientFd, size_t bufferSize) {
   
    struct admin * ret;
    if(pool == NULL) {
        ret = malloc(sizeof(*ret));
    } else {
        ret       = pool;
        pool      = pool->next;
        ret->next = 0;
    }
    if(ret == NULL) {
        goto finally;
    }
    memset(ret, 0x00, sizeof(*ret));

    ret->state = AUTHENTIFICATION;
    ret->clientFd = clientFd;
    ret->readBuffer = createBuffer(BUFFER_SIZE);
    ret->writeBuffer = createBuffer(BUFFER_SIZE);

    ret->stm    .initial   = AUTHENTIFICATION;
    ret->stm    .maxState = ERROR;
    ret->stm    .states    = adminDescribeStates();
    stateMachineInit(&ret->stm);
    
    ret->references = 1;
finally:
    return ret;
}

static void adminRead(MultiplexorKey key) {
    stateMachine stm = &ATTACHMENT(key)->stm;
    logDebug("tamo en read");
    const adminState state = stateMachineHandlerRead(stm, key);
    
    logDebug("state: %d", state);
    if(ERROR == state || DONE == state) {
        adminDone(key);
    }
}

static void adminWrite(MultiplexorKey key) {
    stateMachine stm = &ATTACHMENT(key)->stm;
    logDebug("tamo bien");
    const adminState state = stateMachineHandlerWrite(stm, key);
    logDebug("state: %d", state);
    if(ERROR == state || DONE == state) {
        adminDone(key);
    }
}

static unsigned readCredentials(MultiplexorKey key) {
    
    copy * d = &(ATTACHMENT(key)->clientCopy);
    checkAreEquals(*d->fd, key->fd, "Copy destination and source have the same fd");
    size_t size;
    ssize_t n;
    bufferADT buffer = d->readBuffer;
    unsigned ret = AUTHENTIFICATION;

    uint8_t *ptr = getWritePtr(buffer, &size);
    n = recv(key->fd, ptr, size, 0);
    logDebug("we read %d bytes", n);
    if(n <= 0) {
        logDebug("ALGUN ADMIN CERRO LA CONEXION");
        shutdown(*d->fd, SHUT_RD);
        d->duplex &= ~READ;
        if(*d->other->fd != -1) {
            shutdown(*d->other->fd, SHUT_WR);
            d->other->duplex &= ~WRITE;
        }
    } else {
        updateWritePtr(buffer, n);
    }


    if(checkCredentials(key)) {
        
        ret = REPLY_AUTHENTIFICATION;
    }else{
        logInfo("the credentials were wrong");
    }
    return ret;
}

static int checkCredentials(MultiplexorKey key) {
    copy * p = &(ATTACHMENT(key)->clientCopy);
    bufferADT buffer = p->readBuffer;
    //no valido can read porque si entre es que me llamo el multiplexor
    requestRAP req = newRequest();
    readRequest(req, buffer);
    return parseAuthentication(req);
}


static unsigned reply_auth(MultiplexorKey key){
    copy * d = &ATTACHMENT(key)->clientCopy;
    checkAreEquals(*d->fd, key->fd, "Copy destination and source have the same fd");
    bufferADT buffer = d->readBuffer;// no tengo en claro cual de los dos buffers es
    size_t size;
    ssize_t n;
    uint8_t *ptr = getReadPtr(buffer, &size);
    logDebug("recien mande info");
    n = send(key->fd, ptr, size, MSG_NOSIGNAL);
    unsigned ret = DONE;
    if(n == -1) {
        shutdown(*d->fd, SHUT_WR);
        d->duplex &= ~WRITE;
        if(*d->other->fd != -1) {                       //shutdeteo tanto socket cliente como servidor
            shutdown(*d->other->fd, SHUT_RD);
            d->other->duplex &= ~READ;
        }
    } else {
        updateReadPtr(buffer, n);
    }

    if(d->duplex == NO_INTEREST) {
        ret = DONE;
    }
    return ret;    

}



static void adminDone(MultiplexorKey key) {
    logDebug("Estoy por desregistrar los fds en done");
    const int fds[] = {
        ATTACHMENT(key)->clientFd,
    };
    for(unsigned i = 0; i < N(fds); i++) {
        if(fds[i] != -1) {
            if(MUX_SUCCESS != unregisterFd(key->mux, fds[i])) {
                fail("Problem trying to unregister a fd: %d.", fds[i]);
            }
            close(fds[i]);
        }
    }
}



static void connectionInit(const unsigned state, MultiplexorKey key) {
    struct admin * p = ATTACHMENT(key);

    copy * d        = &(p->clientCopy);
    d->fd           = &(p->clientFd);
    d->readBuffer   = p->readBuffer;
    d->writeBuffer  = p->writeBuffer;
    d->duplex       = READ | WRITE;
    d->other        = &(p->originCopy);

    d               = &(p->originCopy);
    d->fd           = NULL;//&(p->originFd);
    d->readBuffer   = p->writeBuffer;
    d->writeBuffer  = p->readBuffer;
    d->duplex       = READ | WRITE;
    d->other        = &(p->clientCopy);
}


static void deleteAdmin( admin* a) {
    if(a != NULL) {
        if(a->references == 1) {
            if(poolSize < maxPool) {
                a->next = pool;
                pool    = a;
                poolSize++;
            } else {
                free(a);
            }
        } else {
            a->references -= 1;
        }
    } 
}


void adminPassiveAccept(MultiplexorKey key)
{
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len = sizeof(client_addr);
    admin * new_admin = NULL;

    const int client = accept(key->fd, (struct sockaddr*) &client_addr, &client_addr_len);
    if(client == -1) {
        goto fail;
    }
    if(fdSetNIO(client) == -1) {
        goto fail;
    }
    logInfo("Accepting new admin from: ");
    

    new_admin = newAdmin(client, BUFFER_SIZE);

    /// borrar
    poolSize++;
    int i = maxPool;
    i++;
    ///

    if(MUX_SUCCESS != registerFd(key->mux, client, &adminHandler, READ, new_admin)) {
        goto fail;
    }
    
    logInfo("Connection established, client fd: %d", client);

    return ;
fail:
    if(client != -1) {
        close(client);
    }
    deleteAdmin(new_admin);
}



/* definición de handlers para cada estado */
static const struct stateDefinition clientStatbl[] = {
    {
        .state            = AUTHENTIFICATION,
        .onArrival        = connectionInit,
        .onReadReady      = readCredentials,
    }, {
        .state            = REPLY_AUTHENTIFICATION,
        .onReadReady      = reply_auth,
    }, {
        .state            = TRANSACTION,
  
    }, {
        .state            = UPDATE,

    }, {
        .state            = DONE,

    }, {
        .state            = ERROR,
    }
};

static const struct stateDefinition * adminDescribeStates(void) {
    return clientStatbl;
}


