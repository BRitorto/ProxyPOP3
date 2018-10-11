#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "Multiplexor.h"

/**
 * stateMachine.h - pequeño motor de maquina de estados donde los eventos son los
 *         del Multiplexor.c
 *
 * Los estados se identifican con un número entero (típicamente proveniente de
 * un enum).
 *
 *  - El usuario instancia un `struct stateMachineCDT'
 *  - Describe la maquina de estados:
 *      - describe el estado inicial en `initial'
 *      - todos los posibles estados en `states' (el orden debe coincidir con
 *        el identificador)
 *      - describe la cantidad de estados en `states'.
 *
 * Provee todas las funciones necesitadas en un `eventHandler'
 * de Multiplexor.c.
 */

struct stateMachineCDT {
    /** declaración de cual es el estado inicial */
    unsigned                            initial;
    /**
     * declaracion de los estados: deben estar ordenados segun .[].state.
     */
    const struct stateDefinition *     states;
    /** cantidad de estados */
    unsigned                            maxState;
    /** estado actual */
    const struct stateDefinition *     current;
};

typedef struct stateMachineCDT * stateMachine;


/**
 * definición de un estado de la máquina de estados
 */
struct stateDefinition {
    /**
     * identificador del estado: típicamente viene de un enum que arranca
     * desde 0 y no es esparso.
     */
    unsigned state;

    /** ejecutado al arribar al estado */
    void     (*onArrival)    (const unsigned state, MultiplexorKey key);
    /** ejecutado al salir del estado */
    void     (*onDeparture)  (const unsigned state, MultiplexorKey key);
    /** ejecutado cuando hay datos disponibles para ser leidos */
    unsigned (*onReadReady) (MultiplexorKey key);
    /** ejecutado cuando hay datos disponibles para ser escritos */
    unsigned (*onWriteReady)(MultiplexorKey key);
    /** ejecutado cuando hay una resolución de nombres lista */
    unsigned (*onBlockReady)(MultiplexorKey key);
};


/** inicializa el la máquina */
void stateMachineInit(stateMachine stm);

/** obtiene el identificador del estado actual */
unsigned getState        (stateMachine stm);

/** indica que ocurrió el evento read. retorna nuevo id de nuevo estado. */
unsigned stateMachineHandlerRead(stateMachine stm, MultiplexorKey key);

/** indica que ocurrió el evento write. retorna nuevo id de nuevo estado. */
unsigned stateMachineHandlerWrite(stateMachine stm, MultiplexorKey key);

/** indica que ocurrió el evento block. retorna nuevo id de nuevo estado. */
unsigned stateMachineHandlerBlock(stateMachine stm, MultiplexorKey key);

/** indica que ocurrió el evento close. retorna nuevo id de nuevo estado. */
void stateMachineHandlerClose(stateMachine stm, MultiplexorKey key);

#endif
