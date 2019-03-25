#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "CuTest.h"

#include "commandParserTest.h"
#include "commandParser.h"
#include "queue.h"
#include "buffer.h"

void testGetUsernameUser(CuTest * tc) {
    commandParser parser;
    commandParserInit(&parser);
    queueADT commands = createQueue();
    commandStruct * currentCommand;

    char * testCommand = "USER Test\r\n";
    bufferADT buffer = createBuffer(strlen(testCommand));

    size_t size;
    bool pipelining = false, newCommand = false;
    uint8_t * ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testCommand, size);
    updateWritePtr(buffer, size);

    commandParserConsume(&parser, buffer, commands, pipelining, &newCommand);
    currentCommand = peekProcessed(commands);

    CuAssertIntEquals(tc, 1, getQueueSize(commands));  
    CuAssertIntEquals(tc, true, newCommand); 
    CuAssertTrue(tc, 0 == strcmp("Test", getUsername(*currentCommand)));
    CuAssertIntEquals(tc, CMD_USER, currentCommand->type);
}

void testGetUsernameApop(CuTest * tc) {
    commandParser parser;
    commandParserInit(&parser);
    queueADT commands = createQueue();
    commandStruct * currentCommand;

    char * testCommand = "APOP Test Hash\r\n";
    bufferADT buffer = createBuffer(strlen(testCommand));

    size_t size;
    bool pipelining = false, newCommand = false;
    uint8_t * ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testCommand, size);
    updateWritePtr(buffer, size);

    commandParserConsume(&parser, buffer, commands, pipelining, &newCommand);
    currentCommand = peekProcessed(commands);

    CuAssertIntEquals(tc, 1, getQueueSize(commands));    
    CuAssertIntEquals(tc, true, newCommand); 
    CuAssertTrue(tc, 0 == strcmp("Test", getUsername(*currentCommand)));
    CuAssertIntEquals(tc, CMD_APOP, currentCommand->type);

}

void testParseCommands(CuTest * tc) {
    commandParser parser;
    commandParserInit(&parser);   
    queueADT commands = createQueue();
    commandStruct * currentCommand;

    char * testCommands = "USER Test\r\nPASS 1234\r\nLIST\r\nCAPA\r\nRETR 2\r\nDELE 2\r\nQUIT\r\nAPOP Test Hash\r\n";
    bufferADT buffer = createBuffer(strlen(testCommands));

    size_t size;
    bool pipelining = true, newCommand = false;
    uint8_t * ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testCommands, size);
    updateWritePtr(buffer, size);

    commandParserConsume(&parser, buffer, commands, pipelining, &newCommand);

    CuAssertIntEquals(tc, 8, getQueueSize(commands));    
    CuAssertIntEquals(tc, true, newCommand);     
    
    currentCommand = peekProcessed(commands);
    CuAssertIntEquals(tc, CMD_USER,  currentCommand->type);

    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_PASS,  currentCommand->type);

    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_LIST,  currentCommand->type);
    CuAssertIntEquals(tc, 1, currentCommand->isMultiline);

    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_CAPA,  currentCommand->type);

    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_RETR,  currentCommand->type);

    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_OTHER, currentCommand->type);

    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_OTHER, currentCommand->type);

    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_APOP,  currentCommand->type); 
}

void testInvalidCommands(CuTest * tc) {
    commandParser parser;
    commandParserInit(&parser);    
    queueADT commands = createQueue();
    commandStruct * currentCommand;

    char * testCommands = "asdasdPASS 1234\r\nLIST\r\nCAPA\r\nLIST 1 2\r\nRETRrive\r\nDELETE\r\n USER username\r\nAPOP Test Hash\r\n";
    bufferADT buffer = createBuffer(strlen(testCommands));

    size_t size;
    bool pipelining = true, newCommand = false;
    uint8_t * ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testCommands, size);
    updateWritePtr(buffer, size);

    commandParserConsume(&parser, buffer, commands, pipelining, &newCommand);
    CuAssertIntEquals(tc, 8, getQueueSize(commands));        
    CuAssertIntEquals(tc, true, newCommand); 

    currentCommand = peekProcessed(commands);
    CuAssertIntEquals(tc, CMD_OTHER, currentCommand->type);
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_LIST,  currentCommand->type);
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_CAPA,  currentCommand->type);
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_OTHER, currentCommand->type);
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_OTHER, currentCommand->type);
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_OTHER, currentCommand->type);
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_OTHER, currentCommand->type);
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_APOP,  currentCommand->type); 
}

void testMultilinesCommands(CuTest * tc) {
    commandParser parser;
    commandParserInit(&parser);
    queueADT commands = createQueue();
    commandStruct * currentCommand;

    char * testCommands = "TOP 1 2\r\nTOP 1\r\nTOP \r\n TOP 1 2\r\n   LIST    1\r\nLIST     1\r\nLIST   \r\nTOP     1     2\r\nUIDL   1\r\n UIDL\r\nUIDL 1  2\r\nUIDL      \r\n";
    bufferADT buffer = createBuffer(strlen(testCommands));

    size_t size;
    bool pipelining = true, newCommand = false;
    uint8_t * ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testCommands, size);
    updateWritePtr(buffer, size);

    commandParserConsume(&parser, buffer, commands, pipelining, &newCommand);
    CuAssertIntEquals(tc, 12, getQueueSize(commands));        
    CuAssertIntEquals(tc, true, newCommand); 

    //TOP 1 2\r\n    
    currentCommand = peekProcessed(commands);
    CuAssertIntEquals(tc, CMD_TOP, currentCommand->type);
    CuAssertIntEquals(tc, true, currentCommand->isMultiline);

    //TOP 1\r\n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_OTHER, currentCommand->type);
    CuAssertIntEquals(tc, false, currentCommand->isMultiline);

    //TOP \r\n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_OTHER, currentCommand->type);
    CuAssertIntEquals(tc, false, currentCommand->isMultiline);

    // TOP 1 2\r\n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_OTHER, currentCommand->type);
    CuAssertIntEquals(tc, false, currentCommand->isMultiline);

    //   LIST    1\r\n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_OTHER, currentCommand->type);
    CuAssertIntEquals(tc, false, currentCommand->isMultiline);

    //LIST     1\r\n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_LIST, currentCommand->type);
    CuAssertIntEquals(tc, false, currentCommand->isMultiline);

    //LIST   \r\n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_LIST, currentCommand->type);
    CuAssertIntEquals(tc, true, currentCommand->isMultiline);

    //TOP     1     2\r\n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_TOP, currentCommand->type);
    CuAssertIntEquals(tc, true, currentCommand->isMultiline);

    //UIDL   1\r\n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_UIDL, currentCommand->type);
    CuAssertIntEquals(tc, false, currentCommand->isMultiline);

    // UIDL\r\n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_OTHER, currentCommand->type);
    CuAssertIntEquals(tc, false, currentCommand->isMultiline);

    //UIDL 1 2\r\n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_OTHER, currentCommand->type);
    CuAssertIntEquals(tc, false, currentCommand->isMultiline);

    //UIDL      \r\n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_UIDL, currentCommand->type);
    CuAssertIntEquals(tc, true, currentCommand->isMultiline);
}

void testWithoutCarrigeReturnCommands(CuTest * tc) {
    commandParser parser;
    commandParserInit(&parser);
    queueADT commands = createQueue();
    commandStruct * currentCommand;

    char * testCommands = "TOP 1 2\ncapa this is a test\n TOP 1 2\npass pepe\r\n   LIST    1\nUSER fran\nhola mundo \r\nTOP     1     2\nUIDL      \nUSER\nUSE\nLIST\n";
    bufferADT buffer = createBuffer(strlen(testCommands));

    size_t size;
    bool pipelining = true, newCommand = false;
    uint8_t * ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testCommands, size);
    updateWritePtr(buffer, size);

    commandParserConsume(&parser, buffer, commands, pipelining, &newCommand);
    CuAssertIntEquals(tc, 12, getQueueSize(commands));     
    CuAssertIntEquals(tc, true, newCommand); 

    //TOP 1 2\n
    currentCommand = peekProcessed(commands);
    CuAssertIntEquals(tc, CMD_TOP, currentCommand->type);
    CuAssertIntEquals(tc, true, currentCommand->isMultiline);

    //capa this is a test\n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_CAPA, currentCommand->type);
    CuAssertIntEquals(tc, true, currentCommand->isMultiline);

    // TOP 1 2\n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_OTHER, currentCommand->type);
    CuAssertIntEquals(tc, false, currentCommand->isMultiline);

    //pass pepe\r\n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_PASS, currentCommand->type);
    CuAssertIntEquals(tc, false, currentCommand->isMultiline);

    //   LIST    1\n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_OTHER, currentCommand->type);
    CuAssertIntEquals(tc, false, currentCommand->isMultiline);

    //USER fran\n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_USER, currentCommand->type);
    CuAssertIntEquals(tc, false, currentCommand->isMultiline);

    //hola mundo \r\n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_OTHER, currentCommand->type);
    CuAssertIntEquals(tc, false, currentCommand->isMultiline);

    //TOP     1     2\n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_TOP, currentCommand->type);
    CuAssertIntEquals(tc, true, currentCommand->isMultiline);

    //UIDL      \n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_UIDL, currentCommand->type);
    CuAssertIntEquals(tc, true, currentCommand->isMultiline);

    //USER\n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_OTHER, currentCommand->type);
    CuAssertIntEquals(tc, false, currentCommand->isMultiline);

    //USE\n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_OTHER, currentCommand->type);
    CuAssertIntEquals(tc, false, currentCommand->isMultiline);
    
    //LIST\n
    currentCommand = processQueue(commands);
    CuAssertIntEquals(tc, CMD_LIST, currentCommand->type);
    CuAssertIntEquals(tc, true, currentCommand->isMultiline);
}

CuSuite * getCommandParserTest(void) {
    CuSuite* suite = CuSuiteNew();
    
    SUITE_ADD_TEST(suite, testGetUsernameUser);
    SUITE_ADD_TEST(suite, testGetUsernameApop);
    SUITE_ADD_TEST(suite, testParseCommands);
    SUITE_ADD_TEST(suite, testInvalidCommands);
    SUITE_ADD_TEST(suite, testMultilinesCommands);
    SUITE_ADD_TEST(suite, testWithoutCarrigeReturnCommands);

    return suite;
}
