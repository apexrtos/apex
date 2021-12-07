#pragma once

/*
 * Interrupt priority levels
 */
#define IPL_MIN 1
#define IPL_MAX 7

/*
 * intc_sifive_plic_irq - handle SiFive PLIC interrupt
 */
void intc_sifive_plic_irq();
