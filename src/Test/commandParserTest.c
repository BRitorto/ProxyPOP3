#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "CuTest.h"

#include "commandParserTest.h"

#include "commandParser.h"
#include "buffer.h"

void testUserCommand(CuTest * tc) {
    commandParser parser;
    commandParserInit(&parser);


    char * testCommand = "USER Test\r\n";
    bufferADT buffer = createBuffer(strlen(testCommand));

    size_t size;
    uint8_t * ptr = getWritePtr(buffer, &size);
    memcpy(ptr, testCommand, size);
    updateWritePtr(buffer, size);

    printf("%s", ptr);
    commandStruct commands[1];
    printf("%p\n", (void *) commands);
    size_t commandQty = 0;
    commandParserConsume(&parser, buffer, commands, &commandQty);

    printf("%s\n", getUsername(commands[0]));
    CuAssertIntEquals(tc, 1, commandQty);
    CuAssertIntEquals(tc, commands[0].type, CMD_USER);


}

CuSuite * getCommandParserTest(void) {
    CuSuite* suite = CuSuiteNew();
    
    SUITE_ADD_TEST(suite, testUserCommand);

    return suite;
}
