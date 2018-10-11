/**
 * stateMachine.c - pequeño motor de maquina de estados donde los eventos son los
 *         del Multiplexor.c
 */
#include <stdlib.h>
#include "stateMachine.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

void stateMachineInit(stateMachine stm) {
    // verificamos que los estados son correlativos, y que están bien asignados.
    for(unsigned i = 0 ; i <= stm->maxState; i++) {
        if(i != stm->states[i].state) {
            abort();
        }
    }

    if(stm->initial < stm->maxState) {
        stm->current = NULL;
    } else {
        abort();
    }
}

inline static void handleFirst(stateMachine stm, MultiplexorKey key) {
    if(stm->current == NULL) {
        stm->current = stm->states + stm->initial;
        if(NULL != stm->current->onArrival) {
            stm->current->onArrival(stm->current->state, key);
        }
    }
}

inline static void jump(stateMachine stm, unsigned next, MultiplexorKey key) {
    if(next > stm->maxState) {
        abort();
    }
    if(stm->current != stm->states + next) {
        if(stm->current != NULL && stm->current->onDeparture != NULL) {
            stm->current->onDeparture(stm->current->state, key);
        }
        stm->current = stm->states + next;

        if(NULL != stm->current->onArrival) {
            stm->current->onArrival(stm->current->state, key);
        }
    }
}

unsigned stateMachineHandlerRead(stateMachine stm, MultiplexorKey key) {
    handleFirst(stm, key);
    if(stm->current->onReadReady == 0) {
        abort();
    }
    const unsigned int ret = stm->current->onReadReady(key);
    jump(stm, ret, key);

    return ret;
}

unsigned stateMachineHandlerWrite(stateMachine stm, MultiplexorKey key) {
    handleFirst(stm, key);
    if(stm->current->onWriteReady == 0) {
        abort();
    }
    const unsigned int ret = stm->current->onWriteReady(key);
    jump(stm, ret, key);

    return ret;
}

unsigned stateMachineHandlerBlock(stateMachine stm, MultiplexorKey key) {
    handleFirst(stm, key);
    if(stm->current->onBlockReady == 0) {
        abort();
    }
    const unsigned int ret = stm->current->onBlockReady(key);
    jump(stm, ret, key);

    return ret;
}

void stateMachineHandlerClose(stateMachine stm, MultiplexorKey key) {
    if(stm->current != NULL && stm->current->onDeparture != NULL) {
        stm->current->onDeparture(stm->current->state, key);
    }
}

unsigned getState(stateMachine stm) {
    unsigned ret = stm->initial;
    if(stm->current != NULL) {
        ret= stm->current->state;
    }
    return ret;
}
