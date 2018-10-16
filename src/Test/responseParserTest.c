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


    char * testResponse = "-ERR\r\n";
    bufferADT buffer = createBuffer(strlen(testResponse));

    size_t size;
    bool errored;
    uint8_t * ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testResponse, size);
    updateWritePtr(buffer, size);

    commandStruct commands[1];
    size_t responseCount = 0;
    commands[0].isMultiline = true;
    responseParserConsume(&parser, buffer, commands, &responseCount, &errored);

    CuAssertIntEquals(tc, false, errored);
    CuAssertIntEquals(tc, false, commands[0].indicator);
    CuAssertIntEquals(tc, 1,     responseCount); 

}

void testPositiveIndicatorMultiline(CuTest * tc) {
    responseParser parser;
    responseParserInit(&parser);


    char * testResponse = "+OK\r\n.\r\n";
    bufferADT buffer = createBuffer(strlen(testResponse));

    size_t size;
    bool errored;
    uint8_t * ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testResponse, size);
    updateWritePtr(buffer, size);

    commandStruct commands[1];
    size_t responseCount = 0;
    commands[0].isMultiline = true;
    responseParserConsume(&parser, buffer, commands, &responseCount, &errored);

    CuAssertIntEquals(tc, false, errored);
    CuAssertIntEquals(tc, true,  commands[0].indicator);    
    CuAssertIntEquals(tc, 1,     responseCount);

}

void testSingleLineAndMultiline(CuTest * tc) {
    responseParser parser;
    responseParserInit(&parser);


    char * testResponse = "+OK Logged in.\r\n+OK 14 messages:\r\n1 2335\r\n2 2335\r\n3 2681\r\n4 2546\r\n.\r\n";
    bufferADT buffer = createBuffer(strlen(testResponse));

    size_t size;
    bool errored;
    uint8_t * ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testResponse, size);
    updateWritePtr(buffer, size);

    commandStruct commands[2];
    size_t responseCount = 0;
    commands[0].isMultiline = false;
    commands[1].isMultiline = true;
    responseParserConsume(&parser, buffer, commands, &responseCount, &errored);

    CuAssertIntEquals(tc, false, errored);
    CuAssertIntEquals(tc, true,  commands[0].indicator);
    CuAssertIntEquals(tc, true,  commands[1].indicator);
    CuAssertIntEquals(tc, 2,     responseCount); 

}

void testInvalidResponse(CuTest * tc) {
    responseParser parser;
    responseParserInit(&parser);


    char * testResponse = "+OK Logged in.\r\n-OK 14 messages:\r\n1 2335\r\n2 2335\r\n3 2681\r\n4 2546\r\n.\r\n";
    bufferADT buffer = createBuffer(strlen(testResponse));

    size_t size;
    bool errored;
    uint8_t * ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testResponse, size);
    updateWritePtr(buffer, size);

    commandStruct commands[2];
    size_t responseCount = 0;
    commands[0].isMultiline = false;
    commands[1].isMultiline = true;
    responseParserConsume(&parser, buffer, commands, &responseCount, &errored);

    CuAssertIntEquals(tc, true, errored);

}

CuSuite * getResponseParserTest(void) {
    CuSuite* suite = CuSuiteNew();
    
    SUITE_ADD_TEST(suite, testNegativeIndicator);
    SUITE_ADD_TEST(suite, testPositiveIndicatorMultiline);
    SUITE_ADD_TEST(suite, testSingleLineAndMultiline);
    SUITE_ADD_TEST(suite, testInvalidResponse);

    return suite;
}
