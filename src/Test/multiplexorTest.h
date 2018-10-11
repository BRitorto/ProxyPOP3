#ifndef MULTIPLEXOR_TEST
#define MULTIPLEXOR_TEST

#include "CuTest.h"

CuSuite * getMultiplexorTest(void);

void testEnsureCapacity (CuTest * tc);

void testRegisterFd (CuTest * tc);
void testSelectorRegisterUnregisterRegister(CuTest * tc);


#endif

