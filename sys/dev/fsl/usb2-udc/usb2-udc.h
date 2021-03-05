#pragma once

/*
 * Driver for Freescale USB2 USB Device Controller
 */

struct fsl_usb2_udc_desc {
	const char *name;
	unsigned long base;
	int irq;
	int ipl;
};

void fsl_usb2_udc_init(const fsl_usb2_udc_desc *);
