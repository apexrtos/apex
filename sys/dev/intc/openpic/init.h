#pragma once

/*
 * Device driver for OpenPIC compatible interrupt controllers
 *
 * As far as I can tell there's no published OpenPIC standard, although it is
 * referenced in various places such as:
 *
 * Motorola MPC8245, MPC8540
 * Intel GW80314
 * AMD 19725c
 *
 * This implementation has only run against the QEMU OpenPIC emulation.
 *
 * YMMV.
 */

#include <address.h>

struct intc_openpic_desc {
	phys base;
};

void intc_openpic_init(const intc_openpic_desc *);
