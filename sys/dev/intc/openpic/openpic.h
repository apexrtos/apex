#pragma once

/*
 * Interrupt Priority Levels for OpenPIC
 */
#define IPL_MIN 0
#define IPL_MAX 15

/*
 * Interrupt Type for OpenPIC
 */
#define OPENPIC_EDGE_FALLING 0
#define OPENPIC_EDGE_RISING 1
#define OPENPIC_LEVEL_LOW 2
#define OPENPIC_LEVEL_HIGH 3

/*
 * intc_openpic_irq - OpenPIC interrupt handler
 *
 * Called from hardware depeendent interrupt handler.
 */
void intc_openpic_irq();
