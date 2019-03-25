#ifndef SOCKADDR_TO_STRING_TEST
#define SOCKADDR_TO_STRING_TEST

#include "CuTest.h"

CuSuite * getSockaddrToStringTest(void);

void testSockAddrToStringIPV4(CuTest* tc);

void testSockAddrToStringIPV6(CuTest* tc);

#endif

