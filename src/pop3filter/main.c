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
#include <netinet/sctp.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <time.h> 
#include <fcntl.h>

#include "netutils.h"
#include "multiplexor.h"
#include "logger.h"
#include "errorslib.h"
#include "proxyPopv3nio.h"
#include "adminnio.h"

#define HAS_REQUIRED_ARGUMENTS(k) ((k) == 'e' || (k) == 'l' || (k) == 'L' || (k) == 'm' || (k) == 'M' || (k) == 'o' || (k) == 'p' || (k) == 'P' || (k) == 't')

#define BACKLOG 20
#define SELECT_TIMEOUT 10
#define SELECT_SET_SIZE 1024

/* Variable global de la estructura de configuración. */
extern conf proxyConf;

static bool done = false;

static time_t lastTimeout;

static int proxy = -1;
static int adminProxy = -1;


static addressData proxyAddr;
static addressData adminProxyAddr;


static addressData originAddrData;
    
/**
 * Manejador de la señal SIGTERM.
 */
static void sigTermHandler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
    if(proxy != -1)
        close(proxy);
    if(adminProxy != -1)
        close(adminProxy);
}

/**
 * Manejador de la señal SIGCHILD.
 */
static void sigChildHandler(const int signal) {
    while(waitpid(-1, 0, WNOHANG) != -1);
}

/**
 * Imprime la ayuda del programa.
 */
static void help(int argc) {
    if(argc == 2) {
        printf("Pop3Filter Help\n\nOptions:\n\t-e <error-file> : set the file for stderr.\n\t-h for help.\n\t-l <pop3-address> : set the address for pop3Filter service\n\t-L <admin-address> : set the address for management service.\n\t-m <replace-message> : set the replace message for the filter.\n\t-M <media-range> : list of media types for filter.\n\t-o <management-port> : set the port for management service.\n\t-p <local-port> : set the port of service Pop3Filter\n\t-P <origin-port> : set the port of the origin server.\n\t-t <command> the command for filters.\n\t-v to get the version number of the Pop3Filter.\n\n");
        exit(0);
    }
    fprintf(stderr, "Invalid use of -h option.\n");
    exit(1);
}

/**
 * Imprime la version del programa.
 */
static void printVersion(int argc) {
    if(argc == 2) {
        printf("Pop3Filter Version: %s\n", VERSION_NUMBER);
        exit(0);
    }
    fprintf(stderr, "Invalid use of -v option.\n");
    exit(1);
}

/**
 * Procesa las opciones compatibles con el estilo POSIX de opciones 
 * pasadas como argumento de un programa.
 */
static void parseOptionArguments(int argc, const char * argv[]) {
    if(argc < 2) {
        fprintf(stderr, "Pop3filter -Invalid Arguments\n");
        exit(1);
    }
    proxyAddr.port = 1110;
    adminProxyAddr.port = 9090;
    originAddrData.port   = 110;
    proxyConf.messageCount = 0;
    int optionArg;
    while ((optionArg = getopt(argc, (char * const *)argv, "e:hl:L:m:M:o:p:P:t:v")) != -1) {

        switch(optionArg) {
            case 'e':
                proxyConf.stdErrorFilePath = optarg;
                break;
            case 'h':
                help(argc);
                break;
            case 'l':
                proxyConf.listenPop3Address = optarg;
                break;
            case 'L':
                proxyConf.listenAdminAddress = optarg;
                break;
            case 'm':
                proxyConf.messageCount++;
                if(proxyConf.messageCount == 1) {
                    proxyConf.replaceMsgSize = strlen(optarg);
                    proxyConf.replaceMsg = malloc(proxyConf.replaceMsgSize + 1);
                    memcpy(proxyConf.replaceMsg, optarg, proxyConf.replaceMsgSize + 1);
                }
                else {
                    size_t size = strlen(optarg);
                    char * ptr  = realloc(proxyConf.replaceMsg, proxyConf.replaceMsgSize + size + 2);
                    checkIsNotNull(ptr, "out of Memory");
                    proxyConf.replaceMsg = ptr;
                    proxyConf.replaceMsg[proxyConf.replaceMsgSize] = '\n';
                    proxyConf.replaceMsg[proxyConf.replaceMsgSize + 1] = '\0';
                    strcat(proxyConf.replaceMsg, optarg);
                    proxyConf.replaceMsgSize += size;
                    proxyConf.replaceMsgSize++;
                }   

                break;
            case 'M':
                proxyConf.mediaRange = optarg;
                break;
            case 'o':
                adminProxyAddr.port = atoi(optarg);
                break;
            case 'p':
                proxyAddr.port = atoi(optarg);
                break;
            case 'P':
                originAddrData.port = atoi(optarg);
                break;
            case 't':
                proxyConf.filterCommand = optarg;
                proxyConf.filterActivated = true;
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
        fprintf(stderr, "Pop3filter - Invalid Arguments! Please use: pop3Filter [POSIX style options] <origin-address>\n");
        exit(1);
    }

    bool nonOptionArgFlag = false;
    for (int index = optind; index < argc-1; index++) {
        printf ("Non-option argument %s\n", argv[index]);
        nonOptionArgFlag = true;
    }
    if(nonOptionArgFlag)
        exit(1);

    proxyConf.stringServer = argv[optind];
}

/**
 * Inicializa la configuración del logger.
 */
static void loggerInit(int * fds) {
    FILE * errors  = fopen("./../errors.log", "w+");
    FILE * metrics = fopen("./../metrics.log", "w+");
    loggerClearFiles();
    if(metrics != NULL)
        loggerSetFileByLevel(metrics, LOG_LEVEL_METRIC);
    if(errors != NULL) {
        loggerSetFileByLevel(errors, LOG_LEVEL_WARN);
        loggerSetFileByLevel(errors, LOG_LEVEL_ERROR);
        loggerSetFileByLevel(errors, LOG_LEVEL_FATAL);
    }
    loggerSetColor(true);
    loggerSetQuiet(false);
    loggerSetColor(true);
    loggerSetLevel(LOG_LEVEL_INFO);
}


/**
 * Setea las configuraciones por defecto.
 */
static void setUpConfigurations(void) {
    proxyConf.filterActivated = false;
    proxyConf.stdErrorFilePath = "/dev/null";
    proxyConf.replaceMsg = "Parte remplazada";
    proxyConf.filterCommand = NULL;
    proxyConf.mediaRange = NULL;
    proxyConf.listenPop3Address = "0.0.0.0";
    proxyConf.listenAdminAddress = "127.0.0.1";
    proxyConf.replaceMsgSize = 0;
    proxyConf.credential = calloc(strlen("admin"), sizeof(char));
    memcpy(proxyConf.credential, "admin", strlen("admin"));
    proxyConf.bufferSize = BUFFER_SIZE;
    proxyConf.mediaRangeAdminChanged = false;
    proxyConf.filterCommanAdminChanged = false;
    proxyConf.replaceMsgAdminChanged = false;
    proxyConf.stdErrorFilePathAdminChanged = false;
    proxyConf.etags = calloc(12, sizeof(int));
}

typedef struct pack {
    MultiplexorADT      mux;
    multiplexorStatus * status;
    int                 retVal;
} pack;

/**
 * Manejador de errores para la función main.
 */
static void errorHandler(void * data) {
    pack * dataPack = (pack *)data;
    logFatal("An error ocurred.");
    if(dataPack->mux != NULL) {
        deleteMultiplexorADT(dataPack->mux);
    }
    multiplexorClose();
    if(proxy >= 0) {
        close(proxy);
    }
    if(adminProxy >= 0) {
        close(adminProxy);
    }
    poolProxyPopv3Destroy();
    poolAdminDestroy();
    if(proxyConf.messageCount > 1)
        free(proxyConf.replaceMsg);
    if(proxyConf.filterCommand != NULL && proxyConf.filterCommanAdminChanged)
        free(proxyConf.filterCommand);
    if(proxyConf.credential != NULL)
        free(proxyConf.credential);
    if(proxyConf.mediaRangeAdminChanged)
        free(proxyConf.mediaRange);
    if(proxyConf.filterCommanAdminChanged)
        free(proxyConf.filterCommand);
    if(proxyConf.replaceMsgAdminChanged)
        free(proxyConf.replaceMsg);
    if(proxyConf.stdErrorFilePathAdminChanged)
        free(proxyConf.stdErrorFilePath);
    free(proxyConf.etags);
    exit(dataPack->retVal);
}

int main(const int argc, const char ** argv) {
    
    int fds[] = {-1, -1, -1, -1, -1, -1, -1};
    loggerInit(fds);

    setUpConfigurations();
    parseOptionArguments(argc, argv);

    lastTimeout = time(NULL);
    multiplexorStatus status = MUX_SUCCESS;
    MultiplexorADT mux = NULL;
    pack dataPack = {.status = &status, .mux = mux, .retVal = 1}; 


    setAddress(&proxyAddr, proxyConf.listenPop3Address);
    checkAreNotEquals(proxyAddr.type, ADDR_DOMAIN, "Invalid Arguments.");

    setAddress(&adminProxyAddr, proxyConf.listenAdminAddress);
    checkAreNotEquals(adminProxyAddr.type, ADDR_DOMAIN, "Invalid Arguments.");


    proxy      = socket(proxyAddr.domain, SOCK_STREAM, IPPROTO_TCP);
    adminProxy = socket(adminProxyAddr.domain, SOCK_STREAM, IPPROTO_SCTP);

    checkFailWithFinally(proxy, errorHandler, &dataPack, "Unable to create proxy popv3 socket.");
    checkFailWithFinally(adminProxy, errorHandler, &dataPack, "Unable to create admin socket.");

    struct sctp_initmsg initmsg;
    memset (&initmsg, 0, sizeof (initmsg));
    initmsg.sinit_num_ostreams = 5;
    initmsg.sinit_max_instreams = 5;
    initmsg.sinit_max_attempts = 4;

    int result = setsockopt(proxy, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));
    checkFailWithFinally(result, errorHandler, &dataPack, "setsockopt(SO_REUSEADDR) in proxy popv3 socket failed.");

    result = setsockopt(adminProxy, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof(initmsg));
    checkFailWithFinally(result, errorHandler, &dataPack, "setsockopt(SO_REUSEADDR) in admin socket failed.");


    result = bind(proxy, (struct sockaddr*) &proxyAddr.addr.addrStorage, sizeof(proxyAddr.addr.addrStorage));
    checkFailWithFinally(result, errorHandler, &dataPack, "bind() in proxy popv3 socket failed.");

    result = bind(adminProxy, (struct sockaddr*) &adminProxyAddr.addr.addrStorage, sizeof(adminProxyAddr.addr.addrStorage));
    checkFailWithFinally(result, errorHandler, &dataPack, "bind() in admin socket failed.");


    result = listen(proxy, BACKLOG);
    checkFailWithFinally(result, errorHandler, &dataPack, "listen() in proxy popv3 socket failed.");

    result = listen(adminProxy, BACKLOG);
    checkFailWithFinally(result, errorHandler, &dataPack, "listen() in admin socket failed.");

    logInfo("Listening on TCP port %d", proxyAddr.port);
    logInfo("Listening on SCTP port %d", adminProxyAddr.port);

    signal(SIGTERM,  sigTermHandler);
    signal(SIGINT,   sigTermHandler);
    signal(SIGCHLD, sigChildHandler);

    result = fdSetNIO(proxy);
    checkFailWithFinally(result, errorHandler, &dataPack, "fdSetNIO() in proxy popv3 socket failed.");

    result = fdSetNIO(adminProxy);
    checkFailWithFinally(result, errorHandler, &dataPack, "fdSetNIO() in admin socket failed.");

    const struct multiplexorInit conf = {
        .signal = SIGALRM,
        .selectTimeout = {
            .tv_sec  = SELECT_TIMEOUT,
            .tv_nsec = 0,
        },
    };

    checkAreEqualsWithFinally(multiplexorInit(&conf), 0, errorHandler, &dataPack, "Initializing Multiplexor");
    mux = createMultiplexorADT(SELECT_SET_SIZE);
    checkIsNotNullWithFinally(mux, errorHandler, &dataPack, "Unable to create MultiplexorADT");

    const eventHandler popv3 = {
        .read       = proxyPopv3PassiveAccept,
        .write      = NULL,
        .block      = NULL,
        .close      = NULL, // Nada que liberar por ahora.
        .timeout    = NULL,
    };

    const eventHandler adminHandler = {
        .read       = adminPassiveAccept, 
        .write      = NULL,
        .block      = NULL,
        .close      = NULL, // Nada que liberar por ahora.
        .timeout    = NULL, 
    };

    setAddress(&originAddrData, proxyConf.stringServer);

    status = registerFd(mux, proxy, &popv3, READ, &originAddrData);
    checkAreEqualsWithFinally(status, MUX_SUCCESS, errorHandler, &dataPack, "Registering fd for proxy popv3");
    logInfo("Passive socket registered in fd: %d", proxy);

    status = registerFd(mux, adminProxy, &adminHandler, READ, NULL);
    checkAreEqualsWithFinally(status, MUX_SUCCESS, errorHandler, &dataPack, "Registering fd for admin");
    logInfo("Passive socket registered in fd: %d", adminProxy);

    for(;!done;) {
        status = muxSelect(mux);
        checkAreEqualsWithFinally(status, MUX_SUCCESS, errorHandler, &dataPack, "Serving");
        time_t current = time(NULL);
        if(difftime(current, lastTimeout) >= TIMEOUT/4) {
            lastTimeout = current;
            checkTimeout(mux);
        }
    }

    dataPack.retVal = 0;
    errorHandler(&dataPack);
    return 0;
}

