/**
 * stateMachine.c - pequeño motor de maquina de estados donde los eventos son los
 *         del Multiplexor.c
 */
#include <stdlib.h>
#include "stateMachine.h"
#include "errorslib.h"

void stateMachineInit(stateMachine stm) {
    // verificamos que los estados son correlativos, y que están bien asignados.
    for(unsigned i = 0 ; i <= stm->maxState; i++) {
        checkAreEquals(i, stm->states[i].state, "Error checking states definition, %d state does not match.", i);
    }

    checkGreaterThan(stm->maxState, stm->initial, "Error max state is lower or equals to initial state.");
    stm->current = NULL;
}

inline static void handleFirst(stateMachine stm, MultiplexorKey key) {
    if(stm->current == NULL) {
        stm->current = stm->states + stm->initial;
        if(stm->current->onArrival != NULL) {
            stm->current->onArrival(stm->current->state, key);
        }
    }
}

void stateMachineJump(stateMachine stm, unsigned next, MultiplexorKey key) {
    checkGreaterOrEqualsThan(stm->maxState, next, "Error the next state is grather than max state.");
   
    if(stm->current != stm->states + next) {
        if(stm->current != NULL && stm->current->onDeparture != NULL) {
            stm->current->onDeparture(stm->current->state, key);
        }
        stm->current = stm->states + next;

        if(NULL != stm->current->onArrival) {
            stm->current->onArrival(stm->current->state, key); //Podriamos especializar las transiciones si mandamos el estado del que venimos
        }
    }
}

unsigned stateMachineHandlerRead(stateMachine stm, MultiplexorKey key) {
    handleFirst(stm, key);
    checkIsNotNull(stm->current->onReadReady, "Null pointer on read ready function.");

    const unsigned int ret = stm->current->onReadReady(key);
    stateMachineJump(stm, ret, key);

    return ret;
}

unsigned stateMachineHandlerWrite(stateMachine stm, MultiplexorKey key) {
    handleFirst(stm, key);
    checkIsNotNull(stm->current->onWriteReady, "Null pointer on write ready function.");

    const unsigned int ret = stm->current->onWriteReady(key);
    stateMachineJump(stm, ret, key);

    return ret;
}

unsigned stateMachineHandlerBlock(stateMachine stm, MultiplexorKey key) {
    handleFirst(stm, key);
    checkIsNotNull(stm->current->onBlockReady, "Null pointer on block ready function.");

    const unsigned int ret = stm->current->onBlockReady(key);
    stateMachineJump(stm, ret, key);

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
