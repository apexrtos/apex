#pragma once

/*
 * PowerPC BookE Decrementer system tick timer
 */

struct powerpc_booke_decrementer_systick_desc {
	unsigned long clock;
};

void powerpc_booke_decrementer_systick_init(
	const struct powerpc_booke_decrementer_systick_desc *);
