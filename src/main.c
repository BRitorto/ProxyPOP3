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
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>



#include "multiplexor.h"
#include "logger.h"
#include "errorslib.h"
#include "proxyPopv3nio.h"
#include "adminnio.h"

#define HAS_REQUIRED_ARGUMENTS(k) ((k) == 'e' || (k) == 'l' || (k) == 'L' || (k) == 'm' || (k) == 'M' || (k) == 'o' || (k) == 'p' || (k) == 'P' || (k) == 't')


#define BACKLOG 20
#define VERSION_NUMBER "1.0"


static bool done = false;

static void sigTermHandler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);//CERRAR LOS SOCKETS PASIVOS
    done = true;
}

static void sigChildHandler(const int signal) {
    while(waitpid(-1, 0, WNOHANG) != -1);
}

static char * stdErrorFilePath = "/dev/null";

static char * defaultReplaceMsg = "Parte remplazada";

static char * replaceMsg = NULL;

static unsigned adminPort = 9090;

static unsigned port = 1110;

static in_port_t originPort = 110;

static char * transformCommand = NULL;

static void help(int argc) {
    if(argc == 2) {
        printf("Pop3Filter Help\n\nOptions:\n\t-e <error-file> : set the file for stderr.\n\t-h for help.\n\t-l <pop3-address> : set the address for pop3Filter service\n\t-L <admin-address> : set the address for management service.\n\t-m <replace-message> : set the replace message for the transformation.\n\t-M <media-range> : list of media types for transformation.\n\t-o <management-port> : set the port for management service.\n\t-p <local-port> : set the port of service Pop3Filter\n\t-P <origin-port> : set the port of the origin server.\n\t-t <command> the command for transformations.\n\t-v to get the version number of the Pop3Filter.\n\n");
        exit(0);
    }
    fprintf(stderr, "Invalid use of -h option.\n");
    exit(1);
}

static void setReplaceMsg(char * arg) {
    replaceMsg = arg;
}

static void printVersion(int argc) {
    if(argc == 2) {
        printf("Pop3Filter Version: %s\n", VERSION_NUMBER);
        exit(0);
    }
    fprintf(stderr, "Invalid use of -v option.\n");
    exit(1);
}

static void parseOptionArguments(int argc, const char * argv[]) {
    if(argc < 2) {
        fprintf(stderr, "Pop3filter -Invalid Arguments\n");
        exit(1);
    }
    int optionArg;
    while ((optionArg = getopt(argc, (char * const *)argv, "e:hl:L:m:M:o:p:P:t:v")) != -1) {

        switch(optionArg) {
            case 'e':
                stdErrorFilePath = optarg;
                break;
            case 'h':
                help(argc);
                break;
            case 'l':

                break;
            case 'L':

                break;
            case 'm':
                setReplaceMsg(optarg);
                break;
            case 'M':

                break;
            case 'o':
                adminPort = atoi(optarg);
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'P':
                originPort = atoi(optarg);
                break;
            case 't':
                transformCommand = optarg;
                break;
            case 'v':
                printVersion(argc);
                break;
            case '?':
                if (HAS_REQUIRED_ARGUMENTS(optopt))
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint (optopt))
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt); 
                else
                    fprintf (stderr,"Unknown option character `\\x%x'.\n", optopt); 
                break;
            default:
                fprintf(stderr, "Pop3filter -Invalid Options\n");
                exit(1);
                break;
        }
    }
    if(argc - optind != 1) {
        fprintf(stderr, "Pop3filter -Invalid Arguments\n");
        exit(1);
    }

    bool nonOptionArgFlag = false;
    for (int index = optind; index < argc-1; index++) {
        printf ("Non-option argument %s\n", argv[index]);
        nonOptionArgFlag = true;
    }
    if(nonOptionArgFlag)
        exit(1);

    //por ahora print, hay q saber si es ipv4, ipv6 o fqdn
    printf("Origin Server Address: %s\n",argv[optind] );

    if(replaceMsg == NULL)
        replaceMsg = defaultReplaceMsg;
}

static void loggerInit(void) {
    loggerSetColor(true);
    loggerSetQuiet(false);
    loggerSetColor(true);
    loggerSetLevel(LOG_LEVEL_TRACE);
    int fds[] = {-1, -1, -1, -1, -1, -1, -1};
    loggerSetFdsByLevel(fds);
}

int main(const int argc, const char ** argv) {
    
    loggerInit();

    parseOptionArguments(argc, argv);	   

    const char * errMsg = NULL;
    multiplexorStatus status = MUX_SUCCESS;
    MultiplexorADT mux = NULL;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);

    struct sockaddr_in adminAddr;
    memset(&adminAddr, 0, sizeof(adminAddr));
    adminAddr.sin_family      = AF_INET;
    adminAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    adminAddr.sin_port        = htons(adminPort);


    const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    const int adminServer = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);

    if(server < 0) {
        errMsg = "Unable to create socket";
        goto finally;
    }

    logInfo("Listening on TCP port %d", port);
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

    logInfo("Listening on SCTP port %d", adminPort);
    setsockopt(adminServer, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));


    if(bind(server, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        errMsg = "Unable to bind socket";
        goto finally;
    }

    if(bind(adminServer, (struct sockaddr*) &adminAddr, sizeof(adminAddr)) < 0) {
        errMsg = "Unable to bind admin socket";
        goto finally;
    }

    if (listen(server, BACKLOG) < 0) {
        errMsg = "Unable to listen";
        goto finally;
    }

    if (listen(adminServer, BACKLOG) < 0) {
        errMsg = "Unable to listen admin";
        goto finally;
    }

    signal(SIGTERM,  sigTermHandler);
    signal(SIGINT,   sigTermHandler);
    signal(SIGCHLD, sigChildHandler);

    if(fdSetNIO(server) == -1) {
        errMsg = "Getting server socket flags";
        goto finally;
    }

    if(fdSetNIO(adminServer) == -1) {
        errMsg = "Getting admin server socket flags";
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
        errMsg = "Initializing Multiplexor";
        goto finally;
    }

    mux = createMultiplexorADT(1024);
    if(mux == NULL) {
        errMsg = "Unable to create MultiplexorADT";
        goto finally;
    }
    const eventHandler popv3 = {
        .read       = proxyPopv3PassiveAccept,
        .write      = NULL,
        .block      = NULL,
        .close      = NULL, // nada que liberar por ahora
    };

    const eventHandler adminHandler = {
        .read       = adminPassiveAccept, 
        .write      = NULL,
        .block      = NULL,
        .close      = NULL, // nada que liberar por ahora
    };

    ///////datos de origin harcodeados
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
        errMsg = "Registering fd";
        goto finally;
    }
    logInfo("Passive socket registered in fd: %d", server);

    status = registerFd(mux, adminServer, &adminHandler, READ, NULL);
    if(status != MUX_SUCCESS) {
        errMsg = "Registering fd for admin";
        goto finally;
    }
    logInfo("Passive socket registered in fd: %d", adminServer);


    for(;!done;) {
        errMsg = NULL;
        status = muxSelect(mux);
        if(status != MUX_SUCCESS) {
            errMsg = "Serving";
            goto finally;
        }
    }
    if(errMsg == NULL) {
        errMsg = "Closing";
    }

    int ret = 0;
finally:
    if(status != MUX_SUCCESS) {
        fprintf(stderr, "%s: %s\n", (errMsg == NULL) ? "": errMsg, status == MUX_IO_ERROR ? strerror(errno) : multiplexorError(status));
        ret = 2;
    } else if(errMsg) {
        perror(errMsg);
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

