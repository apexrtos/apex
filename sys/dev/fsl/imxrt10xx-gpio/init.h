#pragma once

/*
 * Driver for GPIO controller on IMXRT10xx processors
 */

struct fsl_imxrt10xx_gpio_desc {
	const char *name;
	unsigned long base;
	int irqs[2];
	int ipl;
};

void fsl_imxrt10xx_gpio_init(const struct fsl_imxrt10xx_gpio_desc *);
