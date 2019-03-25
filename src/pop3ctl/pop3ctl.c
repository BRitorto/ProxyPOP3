#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>

#include "pop3ctl.h"
#include "netutils.h"
#include "admin.h"
#include "errorslib.h"
#include "logger.h"

#define MAX_BUFFER 2048
#define MY_PORT_NUM 9090
#define METRICS_SIZE 4

static int quit = NO_QUIT;
static int connSock;

static command_t shellCommands[] = {
    {"help", NULL, "Print available commands", helpFunction, "help"}, 
    {"setCredential", "[PASSWORD]", "Change the current password for all admins", setCredentials, "setcredential"},
    {"setFilterProgram", "[PROGRAM_NAME]", "Set the program to use for the filters",  setFilterCommandClient, "setfilterprogram"},
    {"viewMetrics", NULL, "View proxy's statistics", getMetricsClient, "viewmetrics"},
    {"getNBytes", NULL, "View the number of bytes transfered", getNBytesClient, "getnbytes"},
    {"getBufferSize", NULL, "View the size of the buffer", getBufferSizeClient, "getbuffersize"},
    {"getFilter", NULL, "View whether or not the filter is being applied", getFilterClient, "getfilter"},
    {"setFilter", "[Y/N]", "Set the filter on or off", setFilterClient, "setfilter"},
    {"getFilterProgram", NULL, "View the current filtering program being used", getFilterCommandClient, "getfilterprogram"},
    {"setBufferSize", "[SIZE]", "Set the buffer size", setBufferSizeClient, "setbuffersize"},
    {"getNConnections", NULL, "View the number of current connections", getNConnectionsClient, "getnconnections"},
    {"getNUsers", NULL, "View the number of current users", getNUSersClient, "getnusers"},
	{"setMediaRange", "[media, ...]", "Set the array of media to be filter by mime parser", setMediaRangeClient, "setmediarange"},
	{"getMediaRange", NULL, "Get media types to be filtered by the parser", getMediaRangeClient, "getmediarange"},
	{"setReplaceMsg", "[MSG]", "Replace existing 'replace message' on server", setReplaceMsgClient,"setreplacemsg"},
	{"getReplceMsg", NULL, "Get 'replace message' from server", getReplaceMsgClient, "getreplacemsg"},
	{"addReplaceMsg", "[MSG]", "Append message to the 'replace message' from server", addReplaceMsgClient, "addreplacemsg"},
	{"setErrorFilePath","[PATH]", "Set the error path of the server", setErrorFilePathClient, "seterrorfilepath"},
	{"getErrorFilePath",NULL, "Get the error path of the server", getErrorFilePathClient, "geterrorfilepath"},
};


static addressData serverAddr;
static const char * serverAddrString;

static void parseOptionArguments(int argc, const char * argv[]);


/**
 * Procesa las opciones compatibles con el estilo POSIX de opciones 
 * pasadas como argumento de un programa.
 */
static void parseOptionArguments(int argc, const char * argv[]) {

    serverAddr.port = 9090;
    int optionArg;
    while ((optionArg = getopt(argc, (char * const *)argv, "o:L:")) != -1) {

        switch(optionArg){
			case 'L':
				serverAddrString = optarg;
				break;
			case 'o':
				serverAddr.port = atoi(optarg);
				break;
			case '?':
                if (optopt == 'o' || optopt == 'l')
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint (optopt))
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt); 
                else
                    fprintf (stderr,"Unknown option character `\\x%x'.\n", optopt); 
                break;
            default:
                fprintf(stderr, "Pop3ctl -Invalid Options\n");
                exit(1);
                break;
		}

	}
}

int main(int argc, char const *argv[]) {
	char * password;
	int ret;
	char buffer[MAX_BUFFER + 1];
	int datalen = 0;

	datalen = strlen(buffer);
	serverAddrString = "127.0.0.1";
	parseOptionArguments(argc, argv);
	setAddress(&serverAddr, serverAddrString);

	if(serverAddr.type == ADDR_DOMAIN) {
		struct addrinfo * serverResolution = 0;

		struct addrinfo hints;
		memset(&hints, 0, sizeof(struct addrinfo));
	    hints.ai_family    = AF_UNSPEC; 
	    /** Permite IPv4 o IPv6. */
	    hints.ai_socktype  = SOCK_STREAM;  
	    hints.ai_flags     = AI_PASSIVE;   
	    hints.ai_protocol  = 0;        
	    hints.ai_canonname = NULL;
	    hints.ai_addr      = NULL;
	    hints.ai_next      = NULL;

	    char buffer[7];
	    snprintf(buffer, sizeof(buffer), "%d", serverAddr.port);
	    int s = getaddrinfo(serverAddr.addr.fqdn, buffer, &hints, &serverResolution);

	    if (s != 0) {
        	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        	exit(1);
    	}

    	if(serverResolution != 0) {
	    	serverAddr.domain = serverResolution->ai_family;
	       	serverAddr.addrLength = serverResolution->ai_addrlen;
	       	memcpy(&serverAddr.addr.addrStorage, serverResolution->ai_addr, serverResolution->ai_addrlen);
	       	
	       	connSock = socket(serverResolution->ai_family, SOCK_STREAM, IPPROTO_SCTP);
	    	freeaddrinfo(serverResolution);

	    }

    	/*
	    for(serverResolutionCurrent = serverResolution; serverResolutionCurrent != NULL; serverResolutionCurrent = serverResolutionCurrent->ai_next) {
	    	serverAddr.domain = serverResolutionCurrent->ai_family;
        	serverAddr.addrLength = serverResolutionCurrent->ai_addrlen;
        	memcpy(&serverAddr.addr.addrStorage, serverResolutionCurrent->ai_addr, serverResolutionCurrent->ai_addrlen);
       		connSock = socket(serverResolutionCurrent->ai_family, serverResolutionCurrent->ai_socktype, serverResolutionCurrent->ai_protocol);
	        if (connSock == -1)
	            continue;
	        if(connect(connSock, (struct sockaddr *) &serverAddr.addr.addrStorage , serverAddr.addrLength) > 0)
	        	break;
		}
		if(serverResolutionCurrent == NULL) {
			fprintf(stderr, "Could not connect()\n");
        	exit(1);
		}*/

	}
	else {
		connSock = socket(serverAddr.domain, SOCK_STREAM, IPPROTO_SCTP);
		checkFail(connSock, "Impossible to get an sctp socket");
	}
	ret = connect(connSock, (struct sockaddr *) &serverAddr.addr.addrStorage , serverAddr.addrLength);
	checkFail(ret, "Connect() Failed.");
	printf("Welcome to pop3ctl. Please enter the admin PASSWORD to continue\n");
	printf("Type 'q' to quit\n\n");
	readHello(connSock);

	do {
		printf("PASS: ");
		password = getInput();

	} while(!quit && !validateCredentials(password, connSock));
	if(quit) {
		quitMessage();
		exit(1);
	}

	printf("\n**Successfull login**\n");
	printf("These are the available commands:\n");
	printCommands();
	askForCommand();
}

char * getInput(void) {
	char * buffer = malloc(sizeof(char)*BUFFER_LENGTH);

	fgets(buffer, BUFFER_LENGTH, stdin);
	strtok(buffer, "\n");

	if (strcmp(buffer, "q") == 0){
		quit = QUIT;
		return buffer;
	}

	return buffer;
}

void askForCommand(void) {
	int validInput = INVALID;
	char * input;

	do{
		printf(">> ");
		input = getInput();
		validInput = validateCommand(input);
		if (validInput == INVALID && !quit) {
			errorMessage("input");
		}
	} while (!quit );
	
	if (quit)
		quitMessage();

}

int validateCommand(char * input) {
	char command[BUFFER_LENGTH], arguments[BUFFER_LENGTH];
	int index, commandNumber, i;

	index = extractCommand(command, input);
	for(i = 0; command[i]; i++)
  		command[i] = tolower(command[i]);
	
	commandNumber = isValid(command);
	if (commandNumber != INVALID){
		extractArguments(arguments, index, input);
		run(commandNumber, arguments);
		return VALID;
	}
	else{
		return INVALID;
	}
}

int isValid(char * command) {
	int ret = INVALID, i;
    for (i = 0; i < COMMAND_QTY; i++){	

        if (strcmp(command, shellCommands[i].tolower) == 0) {
            ret = i;
            i = COMMAND_QTY;
        }
    }
	return ret;
}

void extractArguments(char * arguments, int index, const char * buffer) {
    int j = 0;

    while(buffer[index] == ' ')
        index++;
    while(buffer[index] != '\0')
        arguments[j++] = buffer[index++];

    arguments[j] = '\0';
}

int extractCommand(char * command, const char * buffer) {
    int i = 0;
    for (i = 0; buffer[i] != '\0' && buffer[i] != ' ' && buffer[i] != '&' && buffer[i] != '|'; i++)
        command[i] = buffer[i];
    command[i] = '\0';
    return i;
}

int helpFunction(int socket, void * arguments) {
	printf("\n");
	printCommands();
	return VALID;
}

void printCommands(void) {
	int i;

	printf("(All commands are cAsE iNsEnSiTiVe)\n\n");
	for (i = 0; i < COMMAND_QTY; i++){
		printf("%s ", shellCommands[i].name);
		if (shellCommands[i].arguments == NULL)
			printf("(no arguments)\n");
		else
			printf("%s\n",  shellCommands[i].arguments);
		printf("\t -%s\n", shellCommands[i].description);
	}
}

void errorMessage(char * param) {
	printf("Wrong %s. Try again or press q to quit\n", param);
}

void quitMessage(void) {
	printf("\n**See you soon!**\n");
}

int run(int command, char * arguments) {
	char * answer = NULL;
	int numericAnswer = -1;
	int metrics [METRICS_SIZE] = {0};
	int result = 0;
	if (shellCommands[command].function == NULL) {
		printf("Function under construction, please try again later\n");
		return INVALID;
	}
	
	switch(command) {
		case 0:
		
			shellCommands[command].function(connSock, arguments);
			break;
		
		case 1:
		
			result = shellCommands[command].function(connSock, arguments);
			if(result)
				printf("[SUCCESS] Credentials updated!\n");
			else	
				fprintf(stderr, "[FAILURE] There was a problem updating the credentials\n");
			break;
		
		case 2:
		
			result = shellCommands[command].function(connSock, arguments);
			if(result)
				printf("[SUCCESS] Filter updated!\n");
			else	
				fprintf(stderr, "[FAILURE] There was a problem updating the filtering program\n");			
			break;
		
		case 3:
		
			result = shellCommands[command].function(connSock, metrics);
			if(result)
				printf("[SUCCESS] Metrics:\n\tNumber of Bytes: %d \n\tBuffer Size:  %d \n\tAvr Buffer Size: %d \n\tTotal Users: %d\n", 
						metrics[0], metrics[1], metrics[2], metrics[3]);
			else
				fprintf(stderr, "[FAILURE] There was an error fetching the metrics");
			break;
		
		case 4:
		
			result = shellCommands[command].function(connSock, &numericAnswer);
			if(result)
				printf("[SUCCESS] The number of transfered bytes is: %d\n", numericAnswer);
			else
				fprintf(stderr, "[FAILURE] There was an error fetching the transfered bytes\n");
			break;
		
		case 5:
		
			result = shellCommands[command].function(connSock,&numericAnswer);
			if(result)
				printf("[SUCCESS] The buffer size is: %d bytes\n", numericAnswer);
			else
				fprintf(stderr, "[FAILURE] There was an error fetching the buffer size\n");
			break;
		
		case 6:
		
			result = shellCommands[command].function(connSock,&numericAnswer);
			if(result)
				printf("[SUCCESS] The filter is: %s\n",(numericAnswer)? "ON" : "OFF");
			else
				fprintf(stderr, "[FAILURE] There was an error fetching the filter state\n");
			break;

		case 7:
		
			if ((arguments[0] != 'Y' || arguments[0] != 'N') && arguments[1] != '\0')
				return INVALID;
			result = shellCommands[command].function(connSock, arguments);
			if(result)
				printf("[SUCCESS] Filter updated!\n");
			else
				fprintf(stderr, "[FAILURE] There was a problem updating the filter\n");
			break;
		
		case 8:
		
			result = shellCommands[command].function(connSock, &answer);
			if(result)
				printf("[SUCCESS] The filter command is: \" %s \" \n", answer );
			else
				fprintf(stderr, "[FAILURE] There was an error fetching the filter command\n");	
			free(answer);
			break;
		
		case 9:
		
			numericAnswer = atoi(arguments);
			result = shellCommands[command].function(connSock, &numericAnswer);
			if(result)
				printf("[SUCCESS] Buffer size updated!\n");
			else
				fprintf(stderr, "[FAILURE] There was a problem udpating the buffer size\n");
			numericAnswer = -1;
			break;
		
		case 10:
		
			result = shellCommands[command].function(connSock, &numericAnswer);
			if(result)
				printf("[SUCCESS] The number of total active connections is: %d \n", numericAnswer);
			else	
				fprintf(stderr, "[FAILURE] There was an error fetching the number of total active connections\n");

			break;
		
		case 11:
		
			result = shellCommands[command].function(connSock, &numericAnswer);
			if(result)
				printf("[SUCCESS] The number of active connected users is: %d\n", numericAnswer);
			else
				fprintf(stderr, "[FAILURE] There was an error fetching the number of active connected users\n");
			break;
		 
		case 12:
		
			result = shellCommands[command].function(connSock, arguments);
			if(result)
				printf("[SUCCESS] Media range updated!\n");
			else
				fprintf(stderr, "[FAILURE] There was a problem udpating the media range\n");

			break;
		
		case 13:
		
			result = shellCommands[command].function(connSock, &answer);
			if(result)
				printf("[SUCCESS] Media range is: %s\n", answer);
			else
				fprintf(stderr, "[FAILURE] There was a problem getting the media range\n");

			break;
			free(answer);
			answer = NULL;
			break;
		
		case 14:

			result = shellCommands[command].function(connSock, arguments);
			if(result)
				printf("[SUCCESS] Replace message updated!\n");
			else
				fprintf(stderr, "[FAILURE] There was a problem setting the new replace message\n");
			break;

		case 15:

			result = shellCommands[command].function(connSock, &answer);
			if(result)
				printf("[SUCCESS] Replace message is: %s\n", answer);
			else
				fprintf(stderr, "[FAILURE] There was a problem getting the replace message\n");
			break;
			free(answer);
			answer = NULL;
			break;

		case 16:

			result = shellCommands[command].function(connSock, arguments);
			if(result)
				printf("[SUCCESS] New replace message added!\n");
			else
				fprintf(stderr, "[FAILURE] There was a problem adding the replace message \n");
			break;

		case 17:

			result = shellCommands[command].function(connSock, arguments);
			if(result)
				printf("[SUCCESS] Error path file updated!\n");
			else
				fprintf(stderr, "[FAILURE] There was a problem replacing the error path file\n");
			break;

		case 18:

			result = shellCommands[command].function(connSock, &answer);
			if(result)
				printf("[SUCCESS] The error path is: %s\n", answer);
			else
				fprintf(stderr, "[FAILURE] There was a problem getting the error path\n");
			break;
	}
	return VALID;
}
