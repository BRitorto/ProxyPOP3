#ifndef POP3CTL_H
#define POP3CTL_H

#define VALID 1
#define INVALID -1
#define QUIT 1
#define NO_QUIT 0
#define COMMAND_QTY 19
#define BUFFER_LENGTH 256

typedef struct {
    char * name;
    char * arguments;
    char * description;
    int (*function)(int socket, void * arg);
    char * tolower;
} command_t;

int run(int command, char * arguments);

char * getInput(void);

int validateUser(char * user);

int validatePassword(char * password);

void askForCommand(void);

void errorMessage(char * param);

void quitMessage(void);

int extractCommand(char * command, const char * buffer);

void extractArguments(char * arguments, int index, const char * buffer);

int validateCommand(char * input);

int isValid(char * command);

int helpFunction(int socket, void * arguments);

void printCommands();

#endif

