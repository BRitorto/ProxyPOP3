#ifndef MULTIPLEXOR_TEST
#define MULTIPLEXOR_TEST

#include "CuTest.h"
//#include "Multiplexor.h"

CuSuite * getMultiplexoTest(void);

void testEnsureCapacity (CuTest * tc);

void testRegisterFd (CuTest * tc);
void testSelectorRegisterUnregisterRegister(CuTest * tc);


#endif