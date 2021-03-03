#pragma once

#include <dev/gpio/gpio.h>

/*
 * Driver for GPIO controller on IMXRT10xx processors
 */

#ifdef __cplusplus
extern "C" {
#endif

struct fsl_imxrt10xx_gpio_desc {
	const char *name;
	unsigned long base;
	int irqs[2];
	int ipl;
};

void fsl_imxrt10xx_gpio_init(const struct fsl_imxrt10xx_gpio_desc *);

#ifdef __cplusplus
}
#endif
