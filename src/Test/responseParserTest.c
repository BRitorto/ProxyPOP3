#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "CuTest.h"

#include "responseParserTest.h"

#include "responseParser.h"
#include "buffer.h"

void testNegativeIndicator(CuTest * tc) {
    responseParser parser;
    responseParserInit(&parser);
    queueADT commands = createQueue();
    commandStruct command = {.isMultiline = false};
    
    offer(commands, &command);
    char * testResponse = "-ERR\r\n";
    bufferADT buffer = createBuffer(strlen(testResponse));

    size_t size;
    bool errored;
    uint8_t * ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testResponse, size);
    updateWritePtr(buffer, size);
    
    responseParserConsume(&parser, buffer, commands, &errored);

    CuAssertIntEquals(tc, false, errored);
    CuAssertIntEquals(tc, false, command.indicator);
    CuAssertIntEquals(tc, false, canProcess(buffer));
}

void testPositiveIndicatorMultiline(CuTest * tc) {
    responseParser parser;
    responseParserInit(&parser);
    queueADT commands = createQueue();
    commandStruct command = {.isMultiline = true};
    
    offer(commands, &command);
    char * testResponse = "+OK\r\n.\r\n";
    bufferADT buffer = createBuffer(strlen(testResponse));

    size_t size;
    bool errored;
    uint8_t * ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testResponse, size);
    updateWritePtr(buffer, size);

    responseParserConsume(&parser, buffer, commands, &errored);

    CuAssertIntEquals(tc, false, errored);
    CuAssertIntEquals(tc, true, command.indicator);
    CuAssertIntEquals(tc, false, canProcess(buffer));
}

void testSingleLineAndMultiline(CuTest * tc) {
    responseParser parser;
    responseParserInit(&parser);
    queueADT commands = createQueue();
    commandStruct   commandsArray[5];

    commandsArray[0].isMultiline = false;
    commandsArray[1].isMultiline = true;
    commandsArray[2].isMultiline = true;
    commandsArray[3].isMultiline = false;
    commandsArray[4].isMultiline = true;

    for(int i = 0; i < 5; i++)
        offer(commands, commandsArray +i);
    char * testResponse = "+OK Logged in.\r\n+OK 14 messages:\r\n1 2335\r\n2 2335\r\n3 2681\r\n4 2546\r\n.\r\n-ERR problem in server\r\n+OK 203 octets\r\n+OK testing\r\nfirst line\r\nsecond line\r\n.\r\n";
    bufferADT buffer = createBuffer(strlen(testResponse));

    size_t size;
    bool errored;
    uint8_t * ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testResponse, size);
    updateWritePtr(buffer, size);

    responseParserConsume(&parser, buffer, commands, &errored);

    CuAssertIntEquals(tc, false, errored);
    CuAssertIntEquals(tc, true,  commandsArray[0].indicator);
    CuAssertIntEquals(tc, true,  commandsArray[1].indicator);
    CuAssertIntEquals(tc, false, commandsArray[2].indicator);
    CuAssertIntEquals(tc, true,  commandsArray[3].indicator);
    CuAssertIntEquals(tc, true,  commandsArray[4].indicator);
    CuAssertIntEquals(tc, false, canProcess(buffer));
}

void testInvalidTrickyResponse(CuTest * tc) {
    responseParser parser;
    queueADT commands;
    commandStruct commandsArray[2];
    bufferADT buffer;
    size_t size;
    bool errored = false;
    uint8_t * ptr; 

    char * testResponse1 = "+OK with -OK.\r\n-OK 14 messages:\r\n1 2335\r\n2 2335\r\n3 2681\r\n4 2546\r\n.\r\n";
    char * testResponse2 = "+OK with space before next +OK.\r\n +OK 14 messages:\r\n1 2335\r\n2 2335\r\n3 2681\r\n4 2546\r\n.\r\n";    
    char * testResponse3 = "+OK Singleline: More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line.\r\n";
    char * testResponse4 = "+OK This is not multiline.\r\n +OK 14 messages:\r\n1 2335\r\n2 2335\r\n3 2681\r\n4 2546\r\n.\r\n";    
    char * testResponse5 = "-ERR This is multiline but -ERR.\r\n +OK 14 messages:\r\n1 2335\r\n2 2335\r\n3 2681\r\n4 2546\r\n.\r\n";    
    char * testResponse6 = "+OK Multiline but with one line with more than 512 octects.\r\nMore than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line. More than 512 octects in this line.\r\n.\r\n";
    char * testResponse7 = "+OK\r\n.\r+OK\r\n";    

    //TestResponse1
    responseParserInit(&parser);
    commands = createQueue();
    buffer = createBuffer(strlen(testResponse1)); 
    ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testResponse1, size);
    updateWritePtr(buffer, size);

    commandsArray[0].isMultiline = false;
    commandsArray[1].isMultiline = true;
    for(int i = 0; i < 2; i++)
        offer(commands, commandsArray +i);

    responseParserConsume(&parser, buffer, commands, &errored);
    CuAssertIntEquals(tc, true, errored);    
    CuAssertIntEquals(tc, true, canProcess(buffer));
    deleteBuffer(buffer);
    deleteQueue(commands);

    //TestResponse2
    responseParserInit(&parser);
    commands = createQueue();
    buffer = createBuffer(strlen(testResponse2)); 
    ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testResponse2, size);
    updateWritePtr(buffer, size);

    commandsArray[0].isMultiline = false;
    commandsArray[1].isMultiline = true;
    for(int i = 0; i < 2; i++)
        offer(commands, commandsArray +i);

    responseParserConsume(&parser, buffer, commands, &errored);
    CuAssertIntEquals(tc, true, errored);
    CuAssertIntEquals(tc, true, canProcess(buffer));
    deleteBuffer(buffer);
    deleteQueue(commands);

    //TestResponse3
    responseParserInit(&parser);
    commands = createQueue();
    buffer = createBuffer(strlen(testResponse3)); 
    ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testResponse3, size);
    updateWritePtr(buffer, size);

    commandsArray[0].isMultiline = false;
    for(int i = 0; i < 2; i++)
        offer(commands, commandsArray +i);

    responseParserConsume(&parser, buffer, commands, &errored);
    CuAssertIntEquals(tc, true, errored);
    CuAssertIntEquals(tc, true, canProcess(buffer));
    deleteBuffer(buffer);
    deleteQueue(commands);

    //TestResponse4
    responseParserInit(&parser);
    commands = createQueue();
    buffer = createBuffer(strlen(testResponse4)); 
    ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testResponse4, size);
    updateWritePtr(buffer, size);

    commandsArray[0].isMultiline = false;
    for(int i = 0; i < 2; i++)
        offer(commands, commandsArray +i);

    responseParserConsume(&parser, buffer, commands, &errored);
    CuAssertIntEquals(tc, true, errored);
    CuAssertIntEquals(tc, true, canProcess(buffer));
    deleteBuffer(buffer);
    deleteQueue(commands);

    //TestResponse5
    responseParserInit(&parser);
    commands = createQueue();
    buffer = createBuffer(strlen(testResponse5)); 
    ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testResponse5, size);
    updateWritePtr(buffer, size);

    commandsArray[0].isMultiline = true;
    for(int i = 0; i < 2; i++)
        offer(commands, commandsArray +i);

    responseParserConsume(&parser, buffer, commands, &errored);
    CuAssertIntEquals(tc, true, errored);    
    CuAssertIntEquals(tc, true, canProcess(buffer));
    deleteBuffer(buffer);
    deleteQueue(commands);

    //TestResponse6
    responseParserInit(&parser);
    commands = createQueue();
    buffer = createBuffer(strlen(testResponse6)); 
    ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testResponse6, size);
    updateWritePtr(buffer, size);

    commandsArray[0].isMultiline = true;
    for(int i = 0; i < 2; i++)
        offer(commands, commandsArray +i);

    responseParserConsume(&parser, buffer, commands, &errored);
    CuAssertIntEquals(tc, true, errored);
    CuAssertIntEquals(tc, true, canProcess(buffer));
    deleteBuffer(buffer);
    deleteQueue(commands);

    //TestResponse7
    responseParserInit(&parser);
    commands = createQueue();
    buffer = createBuffer(strlen(testResponse7)); 
    ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testResponse7, size);
    updateWritePtr(buffer, size);

    commandsArray[0].isMultiline = true;
    for(int i = 0; i < 2; i++)
        offer(commands, commandsArray +i);

    responseParserConsume(&parser, buffer, commands, &errored);
    CuAssertIntEquals(tc, true, errored);
    CuAssertIntEquals(tc, true, canProcess(buffer));
    deleteBuffer(buffer);
    deleteQueue(commands);
}

void testValidTrickyResponse(CuTest * tc) {
    responseParser parser;
    queueADT commands;
    commandStruct commandsArray[4];
    bufferADT buffer;
    size_t size;
    bool errored;
    uint8_t * ptr; 

    char * testResponse1 = "+OK Logged in.\r\n+OK 14 messages:\r\n.1 2335\r\n2 2335\r\n3 2681\r\n4 2546\r\n.\r\n";
    char * testResponse2 = "-ERRThis dont have initial space.\r\n";    
    char * testResponse3 = "+OK\r\n.\r\n+OK\r\n.\r\n+OK\r\n+OK\r\n.\r\n";    

    //TestResponse1
    responseParserInit(&parser);
    commands = createQueue();
    buffer = createBuffer(strlen(testResponse1)); 
    ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testResponse1, size);
    updateWritePtr(buffer, size);

    commandsArray[0].isMultiline = false;
    commandsArray[1].isMultiline = true;
    for(int i = 0; i < 2; i++)
        offer(commands, commandsArray +i);

    responseParserConsume(&parser, buffer, commands, &errored);
    CuAssertIntEquals(tc, false, errored);    
    CuAssertIntEquals(tc, true,  commandsArray[0].indicator);
    CuAssertIntEquals(tc, true,  commandsArray[1].indicator);
    CuAssertIntEquals(tc, false, canProcess(buffer));
    deleteBuffer(buffer);
    deleteQueue(commands);

    //TestResponse2
    responseParserInit(&parser);
    commands = createQueue();
    buffer = createBuffer(strlen(testResponse2)); 
    ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testResponse2, size);
    updateWritePtr(buffer, size);

    commandsArray[0].isMultiline = false;
    for(int i = 0; i < 1; i++)
        offer(commands, commandsArray +i);

    responseParserConsume(&parser, buffer, commands, &errored);
    CuAssertIntEquals(tc, false, errored);
    CuAssertIntEquals(tc, false, commandsArray[0].indicator);
    CuAssertIntEquals(tc, false, canProcess(buffer));
    deleteBuffer(buffer);
    deleteQueue(commands);

    //TestResponse3
    responseParserInit(&parser);
    commands = createQueue();
    buffer = createBuffer(strlen(testResponse3)); 
    ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testResponse3, size);
    updateWritePtr(buffer, size);

    commandsArray[0].isMultiline = true;
    commandsArray[1].isMultiline = true;
    commandsArray[2].isMultiline = false;
    commandsArray[3].isMultiline = true;
    for(int i = 0; i < 4; i++)
        offer(commands, commandsArray +i);

    responseParserConsume(&parser, buffer, commands, &errored);
    CuAssertIntEquals(tc, false, errored);
    CuAssertIntEquals(tc, true,  commandsArray[0].indicator);
    CuAssertIntEquals(tc, true,  commandsArray[1].indicator);
    CuAssertIntEquals(tc, true,  commandsArray[2].indicator);
    CuAssertIntEquals(tc, true,  commandsArray[3].indicator);
    CuAssertIntEquals(tc, false, canProcess(buffer));
    deleteBuffer(buffer);
    deleteQueue(commands);
}

CuSuite * getResponseParserTest(void) {
    CuSuite* suite = CuSuiteNew();
    
    SUITE_ADD_TEST(suite, testNegativeIndicator);
    SUITE_ADD_TEST(suite, testPositiveIndicatorMultiline);
    SUITE_ADD_TEST(suite, testSingleLineAndMultiline);
    SUITE_ADD_TEST(suite, testInvalidTrickyResponse);
    SUITE_ADD_TEST(suite, testValidTrickyResponse);

    return suite;
}

