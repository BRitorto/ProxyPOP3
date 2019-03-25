#include <stdlib.h>

#include "CuTest.h"

#include "sockaddrToStringTest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <netdb.h>
#include "netutils.h"


void testSockAddrToStringIPV4(CuTest* tc) {
    char buffer[50] = {0};

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(9898),
    };
    addr.sin_addr.s_addr = htonl(0x01020304);
    const struct sockaddr * sockAddr = (const struct sockaddr *) &addr;

    sockaddrToString(buffer, sizeof(buffer)/sizeof(buffer[0]), sockAddr);
    CuAssertStrEquals(tc, buffer, "1.2.3.4:9898");
    sockaddrToString(buffer, 5,  sockAddr);
    CuAssertStrEquals(tc, buffer, "Unkn");
    sockaddrToString(buffer, 8,  sockAddr);
    CuAssertStrEquals(tc, buffer , "1.2.3.4");
    sockaddrToString(buffer, 9,  sockAddr);
    CuAssertStrEquals(tc, buffer, "1.2.3.4:");
    sockaddrToString(buffer, 10, sockAddr);
    CuAssertStrEquals(tc, buffer, "1.2.3.4:9");
    sockaddrToString(buffer, 11, sockAddr);
    CuAssertStrEquals(tc, buffer, "1.2.3.4:98");
    sockaddrToString(buffer, 12, sockAddr);
    CuAssertStrEquals(tc, buffer, "1.2.3.4:989");
    sockaddrToString(buffer, 13, sockAddr);
    CuAssertStrEquals(tc, buffer, "1.2.3.4:9898");
}

void testSockAddrToStringIPV6(CuTest* tc) {
    char buffer[50] = {0};

    struct sockaddr_in6 addr = {
        .sin6_family = AF_INET6,
        .sin6_port   = htons(9898),
    };
    uint8_t *d = ((uint8_t *)&addr.sin6_addr);
    for(int i = 0; i < 16; i++) {
        d[i] = 0xFF;
    }

    const struct sockaddr * sockAddr = (const struct sockaddr *) &addr;
    sockaddrToString(buffer, 10, sockAddr);
    CuAssertStrEquals(tc, buffer, "Unknown I");
    sockaddrToString(buffer, 39, sockAddr);
    CuAssertStrEquals(tc, buffer, "Unknown IP Address:9898");
    sockaddrToString(buffer, 40, sockAddr);
    CuAssertStrEquals(tc, buffer, "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
    sockaddrToString(buffer, 41, sockAddr);
    CuAssertStrEquals(tc, buffer, "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:");
    sockaddrToString(buffer, 42, sockAddr);
    CuAssertStrEquals(tc, buffer, "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:9");
    sockaddrToString(buffer, 43, sockAddr);
    CuAssertStrEquals(tc, buffer, "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:98");
    sockaddrToString(buffer, 44, sockAddr);
    CuAssertStrEquals(tc, buffer, "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:989");
    sockaddrToString(buffer, 45, sockAddr);
    CuAssertStrEquals(tc, buffer, "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:9898");
}



CuSuite * getSockaddrToStringTest(void) {
    CuSuite* suite = CuSuiteNew();
    
    SUITE_ADD_TEST(suite, testSockAddrToStringIPV4);
    SUITE_ADD_TEST(suite, testSockAddrToStringIPV6);
    return suite;
}
