#ifndef dev_fsl_usb2_udc_h
#define dev_fsl_usb2_udc_h

/*
 * Driver for Freescale USB2 USB Device Controller
 */

#ifdef __cplusplus
extern "C" {
#endif

struct fsl_usb2_udc_desc {
	const char *name;
	unsigned long base;
	int irq;
	int ipl;
};

void fsl_usb2_udc_init(const struct fsl_usb2_udc_desc *);

#ifdef __cplusplus
}
#endif

#endif
