#ifndef RESPONSE_PARSER_TEST
#define RESPONSE_PARSER_TEST

#include "CuTest.h"

CuSuite * getResponseParserTest(void);

void testNegativeIndicator(CuTest * tc);

void testPositiveIndicatorMultiline(CuTest * tc);

void testSingleLineAndMultiline(CuTest * tc);

void testInvalidTrickyResponse(CuTest * tc);

void testValidTrickyResponse(CuTest * tc);

#endif

