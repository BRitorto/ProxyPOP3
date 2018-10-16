#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "CuTest.h"

#include "commandParserTest.h"

#include "commandParser.h"
#include "buffer.h"

void testGetUsername(CuTest * tc) {
    commandParser parser;
    commandParserInit(&parser);


    char * testCommand = "USER Test\r\n";
    bufferADT buffer = createBuffer(strlen(testCommand));

    size_t size;
    uint8_t * ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testCommand, size);
    updateWritePtr(buffer, size);

    commandStruct commands[1];
    size_t commandQty = 0;
    commandParserConsume(&parser, buffer, commands, &commandQty);

    CuAssertTrue(tc, 0 == strcmp("Test", getUsername(commands[0])));
    CuAssertIntEquals(tc, commandQty, 1);
    CuAssertIntEquals(tc, CMD_USER, commands[0].type);


}

void testParseCommands(CuTest * tc) {
    commandParser parser;
    commandParserInit(&parser);


    char * testCommands = "USER Test\r\nPASS 1234\r\nLIST\r\nCAPA\r\nRETR 2\r\nDELE 2\r\nQUIT\r\nAPOP Test Hash\r\n";
    bufferADT buffer = createBuffer(strlen(testCommands));

    size_t size;
    uint8_t * ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testCommands, size);
    updateWritePtr(buffer, size);

    commandStruct commands[8];
    size_t commandQty = 0;
    commandParserConsume(&parser, buffer, commands, &commandQty);
    CuAssertIntEquals(tc, commandQty, 8);
    CuAssertIntEquals(tc, CMD_USER, commands[0].type);
    CuAssertIntEquals(tc, CMD_PASS, commands[1].type);
    CuAssertIntEquals(tc, CMD_LIST, commands[2].type);
    CuAssertIntEquals(tc, CMD_CAPA, commands[3].type);
    CuAssertIntEquals(tc, CMD_RETR, commands[4].type);
    CuAssertIntEquals(tc, CMD_OTHER, commands[5].type);
    CuAssertIntEquals(tc, CMD_OTHER, commands[6].type);
    CuAssertIntEquals(tc, CMD_APOP, commands[7].type); 
}

CuSuite * getCommandParserTest(void) {
    CuSuite* suite = CuSuiteNew();
    
    SUITE_ADD_TEST(suite, testGetUsername);
    SUITE_ADD_TEST(suite, testParseCommands);

    return suite;
}
