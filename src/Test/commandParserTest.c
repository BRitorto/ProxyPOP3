#include <stdint.h>
#include <stdlib.h>
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

    commandStruct command;
    size_t commandQty = 1;
    commandParserConsume(&parser, buffer, &command, &commandQty);

    CuAssertIntEquals(tc, 1, commandQty);
    CuAssertIntEquals(tc, command.type, CMD_USER);


}

CuSuite * getCommandParserTest(void) {
    CuSuite* suite = CuSuiteNew();
    
    SUITE_ADD_TEST(suite, testUserCommand);

    return suite;
}
