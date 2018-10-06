#include <stdlib.h>
#include <check.h>

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

START_TEST (testNextCapacity) {
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
        ck_assert_uint_eq(data[i * 2 + 1] + 1, nextCapacity(data[i*2]));
    }
}
END_TEST

START_TEST (testEnsureCapacity) {
    MultiplexorADT mux = createMultiplexorADT(0);
    for(size_t i = 0; i < mux->size; i++) {
        ck_assert_int_eq(FD_UNUSED, mux->fds[i].fd);
    }

    size_t n = 1;
    ck_assert_int_eq(SUCCESS, ensureCapacity(mux, n));
    ck_assert_uint_ge(mux->size, n);

    n = 10;
    ck_assert_int_eq(SUCCESS, ensureCapacity(mux, n));
    ck_assert_uint_ge(mux->size, n);

    const size_t lastSize = mux->size;
    n = FDS_MAX_SIZE + 1;
    ck_assert_int_eq(MAX_FD, ensureCapacity(mux, n));
    ck_assert_uint_eq(lastSize, mux->size);

    for(size_t i = 0; i < mux->size; i++) {
        ck_assert_int_eq(FD_UNUSED, mux->fds[i].fd);
    }

    deleteMultiplexorADT(mux);

    ck_assert_ptr_null(createMultiplexorADT(FDS_MAX_SIZE + 1));
}
END_TEST

// callbacks de prueba
static void *dataMark = (void *)0x0FF1CE;
static unsigned destroyCount = 0;

static void destroyCallback(MultiplexorKey key) {
    ck_assert_ptr_nonnull(key->mux);
    ck_assert_int_ge(key->fd, 0);
    ck_assert_int_lt(key->fd, FDS_MAX_SIZE);

    ck_assert_ptr_eq(dataMark, key->data);
    destroyCount++;
}

START_TEST (testRegisterFd) {
    destroyCount = 0;
    MultiplexorADT mux = createMultiplexorADT(INITIAL_SIZE);
    ck_assert_ptr_nonnull(mux);

    ck_assert_uint_eq(INVALID_ARGUMENTS, registerFd(0, -1, 0, 0, dataMark));

    const eventHandler h = {
        .read   = NULL,
        .write  = NULL,
        .close  = destroyCallback,
    };
    int fd = FDS_MAX_SIZE - 1;
    ck_assert_uint_eq(SUCCESS,
                      registerFd(s, fd, &h, 0, dataMark));
    const fdType * newFdTpe = mux->fds + fd;
    ck_assert_int_eq (fd, mux->maxFd);
    ck_assert_int_eq (fd, newFdTpe->fd);
    ck_assert_ptr_eq (&h, newFdTpe->handler);
    ck_assert_uint_eq(0,  newFdTpe->interest);
    ck_assert_ptr_eq (dataMark,  newFdTpe->data);

    deleteMultiplexorADT(mux);
    ck_assert_uint_eq(1, destroyCount);

}
END_TEST

START_TEST (testSelectorRegisterUnregisterRegister) {
    destroyCount = 0;
    MultiplexorADT mux = createMultiplexorADT(INITIAL_SIZE);
    ck_assert_ptr_nonnull(s);

    const struct fd_handler h = {
        .read   = NULL,
        .write  = NULL,
        .close  = destroyCallback,
    };
    int fd = FDS_MAX_SIZE - 1;
    ck_assert_uint_eq(SUCCESS,
                      registerFd(s, fd, &h, 0, dataMark));
    ck_assert_uint_eq(SUCCESS,
                      unregisterFd(mux, fd));

    const fdType * newFdType = mux->fds + fd;
    ck_assert_int_eq (0,          mux->maxFd);
    ck_assert_int_eq (FD_UNUSED,  newFdType->fd);
    ck_assert_ptr_eq (0x00,       newFdType->handler);
    ck_assert_uint_eq(0,          newFdType->interest);
    ck_assert_ptr_eq (0x00,       newFdType->data);

    ck_assert_uint_eq(SUCCESS,
                      registerFd(mux, fd, &h, 0, dataMark));
    newFdType = mux->fds + fd;
    ck_assert_int_eq (fd, mux->maxFd);
    ck_assert_int_eq (fd, newFdType->fd);
    ck_assert_ptr_eq (&h, newFdType->handler);
    ck_assert_uint_eq(0,  newFdType->interest);
    ck_assert_ptr_eq (dataMark, newFdType->data);

    selector_destroy(mux);
    ck_assert_uint_eq(2, destroyCount);

}
END_TEST

Suite * 
suite(void) {
    Suite * s  = suite_create("nio");
    TCase * tc = tcase_create("nio");

    tcase_add_test(tc, testNextCapacity);
    tcase_add_test(tc, testSelectorError);
    tcase_add_test(tc, testEnsureCapacity);
    tcase_add_test(tc, testSelectorRegisterFd);
    tcase_add_test(tc, testSelectorRegisterUnregisterRegister);
    suite_add_tcase(s, tc);

    return s;
}

int 
main(void) {
    int numberFailed;
    SRunner *sr = srunner_create(suite());

    srunner_run_all(sr, CK_NORMAL);
    numberFailed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (numberFailed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
