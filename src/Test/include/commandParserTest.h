#ifndef COMMAND_PARSER_TEST
#define COMMAND_PARSER_TEST

#include "CuTest.h"

CuSuite * getCommandParserTest(void);

void testGetUsernameUser(CuTest * tc);

void testGetUsernameApop(CuTest * tc);

void testParseCommands(CuTest * tc);

void testInvalidCommands(CuTest * tc);

#endif

