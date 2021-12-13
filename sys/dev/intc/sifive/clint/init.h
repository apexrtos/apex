#pragma once

/*
 * Device driver for SiFive CLINT v0
 */

#include <address.h>

struct intc_sifive_clint_desc {
	phys base;
	unsigned long clock;
};

void intc_sifive_clint_init(const intc_sifive_clint_desc *);
