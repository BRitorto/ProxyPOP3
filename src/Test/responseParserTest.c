#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "CuTest.h"

#include "responseParserTest.h"

#include "responseParser.h"
#include "buffer.h"

void testPositiveIndicator(CuTest * tc) {
    responseParser parser;
    responseParserInit(&parser);


    char * testResponse = "+OK\r\n";
    bufferADT buffer = createBuffer(strlen(testResponse));

    size_t size;
    bool errored;
    uint8_t * ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testResponse, size);
    updateWritePtr(buffer, size);

    commandStruct commands[1];
    size_t responseCount = 0;
    commands[0].isMultiline = false;
    responseParserConsume(&parser, buffer, commands, &responseCount, &errored);

    CuAssertIntEquals(tc, true, commands[0].indicator);    
    CuAssertIntEquals(tc, 1, responseCount);

}

void testNegativeIndicatorMultiline(CuTest * tc) {
    responseParser parser;
    responseParserInit(&parser);


    char * testResponse = "-ERR\r\n.\r\n";
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

    CuAssertIntEquals(tc, false, commands[0].indicator);    
    CuAssertIntEquals(tc, 1, responseCount);

}

CuSuite * getResponseParserTest(void) {
    CuSuite* suite = CuSuiteNew();
    
    SUITE_ADD_TEST(suite, testPositiveIndicator);
    SUITE_ADD_TEST(suite, testNegativeIndicatorMultiline);


    return suite;
}
