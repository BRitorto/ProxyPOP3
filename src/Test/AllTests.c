#include <stdio.h>

#include "CuTest.h"
#include "multiplexorTest.h"
#include "commandParserTest.h"
#include "responseParserTest.h"
#include "stateMachineTest.h"

CuSuite* CuGetSuite();
CuSuite* CuStringGetSuite();

void RunAllTests(void)
{
	CuString *output = CuStringNew();
	CuSuite* suite = CuSuiteNew();
	CuSuiteAddSuite(suite, CuGetSuite());

	CuSuiteAddSuite(suite, getMultiplexorTest());
	CuSuiteAddSuite(suite, getCommandParserTest());	
	CuSuiteAddSuite(suite, getResponseParserTest());
	CuSuiteAddSuite(suite, getSateMachineTest());

	
	CuSuiteRun(suite);
	CuSuiteSummary(suite, output);
	CuSuiteDetails(suite, output);
	printf("%s\n", output->buffer);
}

int main(void)
{
	RunAllTests();
}
