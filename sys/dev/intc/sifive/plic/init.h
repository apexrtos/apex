#pragma once

/*
 * Device driver for SiFive PLIC 1.0.0
 */

#include <address.h>

struct intc_sifive_plic_desc {
	phys base;
};

void intc_sifive_plic_init(const intc_sifive_plic_desc *);
