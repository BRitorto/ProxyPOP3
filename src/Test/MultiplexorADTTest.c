#include <stdlib.h>
#include <check.h>
#include "CuTest.h"

#define INITIAL_SIZE ((size_t) 1024)

// para poder testear las funciones estaticas
#include "MultiplexorADT.c"

/*START_TEST (test_multiplexor_error) {
    const multiplexorStatus data[] = {
        SUCCESS,
        NO_MEMORY,
        MAX_FD,
        INVALID_ARGUMENTS,
        IO_ERROR,
    };
    // verifica que `selector_error' tiene mensajes especificos
    for(unsigned i = 0 ; i < N(data); i++) {
        ck_assert_str_ne(ERROR_DEFAULT_MSG, selector_error(data[i]));
    }
}
END_TEST*/


testNextCapacity (CuTest* tc) {
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


testEnsureCapacity (CuTest * tc) {
    MultiplexorADT mux = createMultiplexorADT(0);
    for(size_t i = 0; i < mux->size; i++) {
        CuAssertIntEquals(tc, FD_UNUSED, mux->fds[i].fd);
    }

    size_t n = 1;
    CuAssertIntEquals(tc,SUCCESS, ensureCapacity(mux, n));
    CuAssertIntEquals(tc, n, mux->size);

    n = 10;
    CuAssertIntEquals(tc, SUCCESS, ensureCapacity(mux, n));
    CuAssertIntEquals(tc, n, mux->size);

    const size_t lastSize = mux->size;
    n = FDS_MAX_SIZE + 1;
    CuAssertIntEquals(tc, MAX_FD, ensureCapacity(mux, n));
    CuAssertIntEquals(tc, lastSize, mux->size);

    for(size_t i = 0; i < mux->size; i++) {
        CuAssertIntEquals(tc, FD_UNUSED, mux->fds[i].fd);
    }

    deleteMultiplexorADT(mux);

    CuAssertPtrNotNull(tc, createMultiplexorADT(FDS_MAX_SIZE + 1));
}


// callbacks de prueba
static void *dataMark = (void *)0x0FF1CE;
static unsigned destroyCount = 0;

static void destroyCallback(MultiplexorKey key, CuTest* tc) {
    CuAssertPtrNotNull(tc, key->mux);
    CuAssertIntEquals(tc, key->fd, 0);
    CuAssertIntEquals(tc, key->fd, FDS_MAX_SIZE);

    CuAssertPtrEquals(tc, dataMark, key->data);
    destroyCount++;
}

testRegisterFd (CuTest * tc) {
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
                      registerFd(s, fd, &h, 0, dataMark));
    const fdType * newFdTpe = mux->fds + fd;
    CuAssertIntEquals (tc,fd, mux->maxFd);
    CuAssertIntEquals (tc, fd, newFdTpe->fd);
    CuAssertIntEquals (tc, &h, newFdTpe->handler);
    CuAssertIntEquals(tc, 0,  newFdTpe->interest);
    CuAssertIntEquals (tc, dataMark,  newFdTpe->data);

    deleteMultiplexorADT(mux);
    CuAssertIntEquals(tc, 1, destroyCount);

}

 testSelectorRegisterUnregisterRegister(CuTest * tc) {
    destroyCount = 0;
    MultiplexorADT mux = createMultiplexorADT(INITIAL_SIZE);
    CuAssertPtrNotNull(tc, s);

    const struct fd_handler h = {
        .read   = NULL,
        .write  = NULL,
        .close  = destroyCallback,
    };
    int fd = FDS_MAX_SIZE - 1;
    CuAssertIntEquals(tc, SUCCESS,
                      registerFd(s, fd, &h, 0, dataMark));
    CuAssertIntEquals(tc, SUCCESS,
                      unregisterFd(mux, fd));

    const fdType * newFdType = mux->fds + fd;
    CuAssertIntEquals (tc, 0,          mux->maxFd);
    CuAssertIntEquals (tc, FD_UNUSED,  newFdType->fd);
    CuAssertIntEquals (tc, 0x00,       newFdType->handler);
    CuAssertIntEquals(tc, 0,          newFdType->interest);
    CuAssertPtrEquals (tc,0x00,       newFdType->data);

    CuAssertIntEquals(tc, SUCCESS,
                      registerFd(mux, fd, &h, 0, dataMark));
    newFdType = mux->fds + fd;
    CuAssertIntEquals (tc, fd, mux->maxFd);
    CuAssertIntEquals (tc, fd, newFdType->fd);
    CuAssertPtrEquals (tc, &h, newFdType->handler);
    CuAssertIntEquals(tc, 0,  newFdType->interest);
    CuAssertPtrEquals (tc, dataMark, newFdType->data);

    selector_destroy(mux);
    CuAssertIntEquals(tc, 2, destroyCount);

}

CuSuite * 
getMultiplexoADTTest(void) {
    CuSuite* suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, testEnsureCapacity);
    SUITE_ADD_TEST(suite, testNextCapacity);
    SUITE_ADD_TEST(suite, testRegisterFd);
    SUITE_ADD_TEST(suite, testSelectorRegisterUnregisterRegister);
    return suite;
}


/*
int 
main(void) {
    int numberFailed;
    SRunner *sr = srunner_create(suite());

    srunner_run_all(sr, CK_NORMAL);
    numberFailed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (numberFailed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
*/