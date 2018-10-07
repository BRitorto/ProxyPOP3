#include <stdlib.h>

#include "CuTest.h"

#include "MultiplexorTest.h"

#include "Multiplexor.c"


// para poder testear las funciones estaticas
#define INITIAL_SIZE ((size_t) 1024)
#define N(x) (sizeof(x)/sizeof((x)[0]))
static void destroyCallback(MultiplexorKey key);



void testNextCapacity (CuTest* tc) {
    const size_t data[] = {
         0,  1,
         1,  2,
         2,  4,
         3,  4,
         4,  8,
         7,  8,
         8, 16,
        15, 16,
        31, 32,
        16, 32,
        FDS_MAX_SIZE, FDS_MAX_SIZE,
        FDS_MAX_SIZE + 1, FDS_MAX_SIZE,
    };
    for(unsigned i = 0; i < N(data) / 2; i++ ) {
        CuAssertIntEquals(tc,data[i * 2 + 1] + 1, nextCapacity(data[i*2]));
    }
}



void testEnsureCapacity (CuTest * tc) {
    MultiplexorADT mux = createMultiplexorADT(0);
    for(size_t i = 0; i < mux->size; i++) {
        CuAssertIntEquals(tc, -1, mux->fds[i].fd);
    }
    
    size_t n = 1;
    CuAssertIntEquals(tc,SUCCESS, ensureCapacity(mux, n));
    int cond = n < mux->size;
    CuAssertTrue(tc,  cond);
    
    n = 10;
    CuAssertIntEquals(tc, SUCCESS, ensureCapacity(mux, n));
    cond = n < mux->size;
    CuAssertTrue(tc,  cond);

    const size_t lastSize = mux->size;
    n = FDS_MAX_SIZE + 1;
    CuAssertIntEquals(tc, MAX_FDS, ensureCapacity(mux, n));
    CuAssertIntEquals(tc, lastSize, mux->size);

    for(size_t i = 0; i < mux->size; i++) {
        CuAssertIntEquals(tc, -1, mux->fds[i].fd);
    }

    deleteMultiplexorADT(mux);

    CuAssertPtrEquals(tc, NULL, createMultiplexorADT(FDS_MAX_SIZE + 1));
    
}


// callbacks de prueba
static void *dataMark = (void *)0x0FF1CE;
static unsigned destroyCount = 0;

static void destroyCallback(MultiplexorKey key) {
 
    destroyCount++;
}



void testRegisterFd (CuTest * tc) {
    destroyCount = 0;
    MultiplexorADT mux = createMultiplexorADT(INITIAL_SIZE);
    CuAssertPtrNotNull(tc, mux);

    CuAssertIntEquals(tc, INVALID_ARGUMENTS, registerFd(0, -1, 0, 0, dataMark));

    const eventHandler h = {
        .read   = NULL,
        .write  = NULL,
        .close  = destroyCallback,
    };
    int fd = FDS_MAX_SIZE - 1;
    CuAssertIntEquals(tc, SUCCESS,
                      registerFd(mux, fd, &h, 0, dataMark));
    const fdType * newFdTpe = mux->fds + fd;
    CuAssertIntEquals (tc,fd, mux->maxFd);
    CuAssertIntEquals (tc, fd, newFdTpe->fd);
   

    deleteMultiplexorADT(mux);
    CuAssertIntEquals(tc, 1, destroyCount);

}


CuSuite * 
getMultiplexoTest(void) {
    CuSuite* suite = CuSuiteNew();
    
    SUITE_ADD_TEST(suite, testNextCapacity);
    SUITE_ADD_TEST(suite, testEnsureCapacity);
    SUITE_ADD_TEST(suite, testRegisterFd);
    return suite;
}
