/**
 * main.c - servidor proxy POP3 concurrente
 *
 * Interpreta los argumentos de línea de comandos, y monta un socket
 * pasivo.
 *
 * Todas las conexiones entrantes se manejarán en éste hilo.
 *
 * Se descargará en otro hilos las operaciones bloqueantes, 
 *pero toda esa complejidad está oculta en el multiplexor.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "Multiplexor.h"
#include "popv3nio.h"


static bool done = false;

static void
sigtermHandler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}

int
main(const int argc, const char **argv) {
    unsigned port = 1111;
    
    close(0);

    const char * err_msg = NULL;
    multiplexorStatus status = SUCCESS;
    MultiplexorADT mux = NULL;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);

    const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(server < 0) {
        err_msg = "unable to create socket";
        goto finally;
    }

    fprintf(stdout, "Listening on TCP port %d\n", port);

    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

    if(bind(server, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        err_msg = "unable to bind socket";
        goto finally;
    }

    if (listen(server, 20) < 0) {
        err_msg = "unable to listen";
        goto finally;
    }
    signal(SIGTERM, sigtermHandler);
    signal(SIGINT,  sigtermHandler);

    if(fdSetNIO(server) == -1) {
        err_msg = "getting server socket flags";
        goto finally;
    }
    const struct multiplexorInit conf = {
        .signal = SIGALRM,
        .selectTimeout = {
            .tv_sec  = 10,
            .tv_nsec = 0,
        },
    };
    if(0 != multiplexorInit(&conf)) {
        err_msg = "initializing Multiplexor";
        goto finally;
    }

    mux = createMultiplexorADT(1024);
    if(mux == NULL) {
        err_msg = "unable to create MultiplexorADT";
        goto finally;
    }
    const eventHandler popv3 = {
        .read       = popv3PassiveAccept,
        .write      = NULL,
        .block      = NULL,
        .close      = NULL, // nada que liberar por ahora
    };

    status = registerFd(mux, server, &popv3, READ, NULL);
    if(status != SUCCESS) {
        err_msg = "registering fd";
        goto finally;
    }
    for(;!done;) {
        err_msg = NULL;
        status = muxSelect(mux);
        if(status != SUCCESS) {
            err_msg = "serving";
            goto finally;
        }
    }
    if(err_msg == NULL) {
        err_msg = "closing";
    }

    int ret = 0;
finally:
    if(status != SUCCESS) {
        fprintf(stderr, "%s: %s\n", (err_msg == NULL) ? "": err_msg, status == IO_ERROR ? strerror(errno) : multiplexorError(status));
        ret = 2;
    } else if(err_msg) {
        perror(err_msg);
        ret = 1;
    }
    if(mux != NULL) {
        deleteMultiplexorADT(mux);
    }
    multiplexorClose();
    if(server >= 0) {
        close(server);
    }
    return ret;
}

