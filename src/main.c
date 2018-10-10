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
#include <netinet/in.h>
#include <arpa/inet.h>

#include "Multiplexor.h"
#include "logger.h"
#include "popv3nio.h"

static bool done = false;

static void
sigtermHandler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}

int main(const int argc, const char **argv) {
    loggerSetColor(true);
	loggerSetQuiet(false);
	loggerSetColor(true);
	loggerSetLevel(LOG_LEVEL_TRACE);
	int fds[] = {-1, -1, -1, -1, -1, -1, -1};
	loggerSetFdsByLevel(fds);	

    unsigned port = 1114;
    close(0);

    const char * err_msg = NULL;
    multiplexorStatus status = MUX_SUCCESS;
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

    logInfo("Listening on TCP port %d", port);
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

    ///////datos de origin harcodeados
    in_port_t originPort = 110;
    originServerAddr originAddr;
    memset(&(originAddr.ipv4), 0, sizeof(originAddr.ipv4));

    originAddr.ipv4.sin_family = AF_INET;
    originAddr.ipv4.sin_port = htons(originPort); 
    if(inet_pton(AF_INET, "127.0.0.1", &originAddr.ipv4.sin_addr.s_addr) <= 0) {
        // si devuelve 0 es que el string con la ip es invalido y si devuelve <0 es que fallo
        goto finally;
    }
    ////////

    logInfo("Ready to register passive socket in fd: %d", server);

    status = registerFd(mux, server, &popv3, READ, &originAddr);
    if(status != MUX_SUCCESS) {
        err_msg = "registering fd";
        goto finally;
    }
    logInfo("Passive socket registered in fd: %d", server);

    for(;!done;) {
        err_msg = NULL;
        status = muxSelect(mux);
        if(status != MUX_SUCCESS) {
            err_msg = "serving";
            goto finally;
        }
    }
    if(err_msg == NULL) {
        err_msg = "closing";
    }

    int ret = 0;
finally:
    if(status != MUX_SUCCESS) {
        fprintf(stderr, "%s: %s\n", (err_msg == NULL) ? "": err_msg, status == MUX_IO_ERROR ? strerror(errno) : multiplexorError(status));
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

