#ifndef PARSER_H
#define PARSER_H

/**
 * parser.c -- pequeño motor para parsers/lexers.
 *
 * El usuario describe estados y transiciones.
 * Las transiciones contienen una condición, un estado destino y acciones.
 *
 * El usuario provee al parser con bytes y éste retona eventos que pueden
 * servir para delimitar tokens o accionar directamente.
 */
#include <stdint.h>
#include <stddef.h>

typedef struct parserCDT * parserADT;
/**
 * Evento que retorna el parser.
 * Cada tipo de evento tendrá sus reglas en relación a data.
 */
typedef struct parserEvent {
    /** Tipo de evento */
    unsigned type;
    /** Caracteres asociados al evento */
    uint8_t  data[3];
    /** Cantidad de datos en el buffer `data' */
    uint8_t  n;

    /** Lista de eventos: si es diferente de null ocurrieron varios eventos */
    struct parserEvent * next;
} parserEvent;

/** Describe una transición entre estados  */
typedef struct parserStateTransition {
    /* Condición: un caracter o una clase de caracter. Por ej: '\r' */
    int       when;
    /** Descriptor del estado destino cuando se cumple la condición */
    unsigned  destination;
    /** Acción 1 que se ejecuta cuando la condición es verdadera. requerida. */
    void    (*action1)(parserEvent * ret, const uint8_t character);
    /** Otra acción opcional */
    void    (*action2)(parserEvent * ret, const uint8_t character);
} parserStateTransition;

/** Predicado para utilizar en `when' que retorna siempre true */
#define ANY (1 << 9)

/** Declaración completa de una máquina de estados */
typedef struct parserDefinition {
    /** Cantidad de estados */
    const unsigned                          statesCount;
    /** Por cada estado, sus transiciones */
    const struct parserStateTransition ** states;
    /** Cantidad de estados por transición */
    const size_t                          * statesQty;
    /** Estado inicial */
    const unsigned                         startState;
} parserDefinition;

/**
 * Inicializa el parser.
 *
 * `classes`: caracterización de cada caracter (256 elementos)
 */
parserADT initializeParser(const unsigned * classes, const parserDefinition * definition);

/** Destruye el parser */
void destroyParser(parserADT parser);

/** Permite resetear el parser al estado inicial */
void resetParser(parserADT parser);

/**
 * El usuario alimenta el parser con un caracter, y el parser retorna un evento
 * de parsing. Los eventos son reusado entre llamadas por lo que si se desea
 * capturar los datos se debe clonar.
 */
const parserEvent * feedParser(parserADT parser, const uint8_t character);

/**
 * En caso de la aplicacion no necesite clases caracteres, se
 * provee dicho arreglo para ser usando en `initializeParser'
 */
const unsigned * noClassesParser(void);


#endif

