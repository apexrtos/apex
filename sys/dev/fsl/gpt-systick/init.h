#pragma once

/*
 * Freescale GPT (as system tick) Driver
 */

namespace fsl {

struct gpt {
	enum class clock {
		ipg = 1,
		ipg_highfreq = 2,
		external = 3,
		ipg_32k = 4,
		ipg_24M = 5,
	};
};

}

struct fsl_gpt_systick_desc {
	unsigned long base;		/* module base address */
	unsigned long clock;		/* module clock frequency */
	fsl::gpt::clock clksrc;		/* module clock source */
	unsigned prescaler;		/* clock prescaler (1 to 4096) */
	unsigned prescaler_24M;		/* 24MHz clock prescaler (1 to 16) */
	int ipl;			/* interrupt priority level */
	int irq;			/* interrupt request number */
};

void fsl_gpt_systick_init(const fsl_gpt_systick_desc *);
