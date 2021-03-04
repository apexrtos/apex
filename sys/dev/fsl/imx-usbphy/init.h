#pragma once

/*
 * Driver for USB PHY in IMX processors.
 */

struct fsl_imx_usbphy_desc {
	unsigned long base;
	unsigned long analog_base;
	unsigned d_cal;
	unsigned txcal45dp;
	unsigned txcal45dn;
};

void fsl_imx_usbphy_init(const struct fsl_imx_usbphy_desc *);
