#include <stdio.h>

#include "CuTest.h"
#include "multiplexorTest.h"
#include "bufferTest.h"

CuSuite* CuGetSuite();
CuSuite* CuStringGetSuite();

void RunAllTests(void)
{
	CuString *output = CuStringNew();
	CuSuite* suite = CuSuiteNew();
	CuSuiteAddSuite(suite, CuGetSuite());

	CuSuiteAddSuite(suite, getMultiplexorTest());
	CuSuiteAddSuite(suite, getBufferTest());
	
	CuSuiteRun(suite);
	CuSuiteSummary(suite, output);
	CuSuiteDetails(suite, output);
	printf("%s\n", output->buffer);
}

int main(void)
{
	RunAllTests();
}
