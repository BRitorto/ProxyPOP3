#include <stdlib.h>
#include <stdbool.h>
#include "CuTest.h"
#include "stateMachineTest.h"
#include "multiplexor.h"
#include "stateMachine.h"


enum test_states {
    A,
    B,
    C,
};

struct data {
    bool arrived  [3];
    bool departed[3];
    unsigned i;
};

static void onArrival(const unsigned state, MultiplexorKey key) {
    struct data *d = (struct data *)key->data;
    d->arrived[state] = true;
}

static void onDeparture(const unsigned state,MultiplexorKey key) {
    struct data *d = (struct data *)key->data;
    d->departed[state] = true;
}

static unsigned onReadReady(MultiplexorKey key) {
    struct data *d = (struct data *)key->data;
    unsigned ret;

    if(d->i < C) {
        ret = ++d->i;
    } else {
        ret = C;
    }
    return ret;
}

static unsigned onWriteReady(MultiplexorKey key) {
    return onReadReady(key);
}

static const struct stateDefinition statbl[] = {
    {
        .state          = A,
        .onArrival     = onArrival,
        .onDeparture   = onDeparture,
        .onReadReady  = onReadReady,
        .onWriteReady = onWriteReady,
    },{
        .state          = B,
        .onArrival     = onArrival,
        .onDeparture   = onDeparture,
        .onReadReady  = onReadReady,
        .onWriteReady = onWriteReady,
    },{
        .state          = C,
        .onArrival     = onArrival,
        .onDeparture   = onDeparture,
        .onReadReady  = onReadReady,
        .onWriteReady = onWriteReady,
    }
};

void testStateMachine (CuTest* tc) {
    struct stateMachineCDT stm = {
        .initial   = A,
        .maxState = C,
        .states    = statbl,
    };
    struct data data = {
        .i = 0,
    };
    struct MultiplexorKeyCDT  key = {
        .data = &data,
    };
    stateMachineInit(&stm);

    CuAssertIntEquals(tc, A, getState(&stm));
    CuAssertIntEquals(tc, false, data.arrived[A]);
    CuAssertIntEquals(tc, false, data.arrived[B]);
    CuAssertIntEquals(tc, false, data.arrived[C]);

    CuAssertTrue(tc, stm.current == NULL);

    stateMachineHandlerWrite(&stm, &key);
    CuAssertIntEquals(tc, B,     getState(&stm));
    CuAssertIntEquals(tc, true,  data.arrived[A]);
    CuAssertIntEquals(tc, true,  data.arrived[B]);
    CuAssertIntEquals(tc, false, data.arrived[C]);
    CuAssertIntEquals(tc, true,  data.departed[A]);
    CuAssertIntEquals(tc, false, data.departed[B]);
    CuAssertIntEquals(tc, false, data.departed[C]);

    /*stateMachineHandlerWrite(&stm, &key);
    CuAssertIntEquals(tc, C,     getState(&stm));
    CuAssertIntEquals(tc, true,  data.arrived[A]);
    CuAssertIntEquals(tc, true,  data.arrived[B]);
    CuAssertIntEquals(tc, true,  data.arrived[C]);
    CuAssertIntEquals(tc, true,  data.departed[A]);
    CuAssertIntEquals(tc, true,  data.departed[B]);
    CuAssertIntEquals(tc, false, data.departed[C]);

    stateMachineHandlerRead(&stm, &key);
    CuAssertIntEquals(tc, C,     getState(&stm));
    CuAssertIntEquals(tc, true,  data.arrived[A]);
    CuAssertIntEquals(tc, true,  data.arrived[B]);
    CuAssertIntEquals(tc,true,  data.arrived[C]);
    CuAssertIntEquals(tc, true,  data.departed[A]);
    CuAssertIntEquals(tc, true,  data.departed[B]);
    CuAssertIntEquals(tc, false, data.departed[C]);

    stateMachineHandlerClose(&stm, &key);*/
}


CuSuite * getSateMachineTest(void) {
    CuSuite* suite = CuSuiteNew();
    
    SUITE_ADD_TEST(suite, testStateMachine);

    return suite;
}


