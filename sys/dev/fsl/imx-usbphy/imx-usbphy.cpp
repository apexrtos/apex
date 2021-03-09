#include "init.h"

#include <arch/mmio.h>
#include <cassert>
#include <conf/config.h>
#include <cstdint>
#include <debug.h>

/*
 * USBPHY registers
 */
union usbphy_ctrl {
	struct {
		uint32_t ENOTG_ID_CHG_IRQ : 1;
		uint32_t ENHOSTDISCONDETECT : 1;
		uint32_t ENIRQHOSTDISCON : 1;
		uint32_t HOSTDISCONDETECT_IRQ : 1;
		uint32_t ENDEVPLUGINDETECT : 1;
		uint32_t DEVPLUGIN_POLARITY : 1;
		uint32_t OTG_ID_CHG_IRQ : 1;
		uint32_t ENOTGIDDETECT : 1;
		uint32_t RESUMEIRQSTICKY : 1;
		uint32_t ENIRQRESUMEDETECT : 1;
		uint32_t RESUME_IRQ : 1;
		uint32_t ENIRQDEVPLUGIN : 1;
		uint32_t DEVPLUGIN_IRQ : 1;
		uint32_t DATA_ON_LRADC : 1;
		uint32_t ENUTMILEVEL2 : 1;
		uint32_t ENUTMILEVEL3 : 1;
		uint32_t ENIRQWAKEUP : 1;
		uint32_t WAKEUP_IRQ : 1;
		uint32_t ENAUTO_POWERON_PLL : 1;
		uint32_t ENAUTOCLR_CLKGATE : 1;
		uint32_t ENAUTOCLR_PHY_PWD : 1;
		uint32_t ENDPDMCHG_WKUP : 1;
		uint32_t ENIDCHG_WKUP : 1;
		uint32_t ENVBUSCHG_WKUP : 1;
		uint32_t FSDLL_RST_EN : 1;
		uint32_t : 2;
		uint32_t OTG_ID_VALUE : 1;
		uint32_t HOST_FORCE_LS_SE0 : 1;
		uint32_t UTMI_SUSPENDM : 1;
		uint32_t CLKGATE : 1;
		uint32_t SFTRST : 1;
	};
	uint32_t r;
};
union usbphy_tx {
	struct {
		uint32_t D_CAL : 4;
		uint32_t : 4;
		uint32_t TXCAL45DN : 4;
		uint32_t : 4;
		uint32_t TXCAL45DP : 4;
		uint32_t : 6;
		uint32_t USBPHY_TX_EDGECTRL : 3;
		uint32_t : 3;
	};
	uint32_t r;
};
struct usbphy {
	uint32_t PWD;
	uint32_t PWD_SET;
	uint32_t PWD_CLR;
	uint32_t PWD_TOG;
	usbphy_tx TX;
	usbphy_tx TX_SET;
	usbphy_tx TX_CLR;
	usbphy_tx TX_TOG;
	uint32_t RX;
	uint32_t RX_SET;
	uint32_t RX_CLR;
	uint32_t RX_TOG;
	usbphy_ctrl CTRL;
	usbphy_ctrl CTRL_SET;
	usbphy_ctrl CTRL_CLR;
	usbphy_ctrl CTRL_TOG;
	uint32_t STATUS;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t DEBUG;
	uint32_t DEBUG_SET;
	uint32_t DEBUG_CLR;
	uint32_t DEBUG_TOG;
	uint32_t DEBUG0_STATUS;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t DEBUG1;
	uint32_t DEBUG1_SET;
	uint32_t DEBUG1_CLR;
	uint32_t DEBUG1_TOG;
	union usbphy_version {
		struct {
			uint32_t STEP : 16;
			uint32_t MINOR : 8;
			uint32_t MAJOR : 8;
		};
		uint32_t r;
	} VERSION;
};
static_assert(sizeof(usbphy) == 0x84, "");

/*
 * USB_ANALOG registers
 */
union usb_analog_chrg_detect {
	struct {
		uint32_t : 18;
		uint32_t CHK_CONTACT : 1;
		uint32_t CHK_CHRG_B : 1;
		uint32_t EN_B : 1;
		uint32_t : 11;
	};
	uint32_t r;
};
struct usb_analog {
	uint32_t VBUS_DETECT;
	uint32_t VBUS_DETECT_SET;
	uint32_t VBUS_DETECT_CLR;
	uint32_t VBUS_DETECT_TOG;
	usb_analog_chrg_detect CHRG_DETECT;
	usb_analog_chrg_detect CHRG_DETECT_SET;
	usb_analog_chrg_detect CHRG_DETECT_CLR;
	usb_analog_chrg_detect CHRG_DETECT_TOG;
	uint32_t VBUS_DETECT_STAT;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t CHRG_DETECT_STAT;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t MISC;
	uint32_t MISC_SET;
	uint32_t MISC_CLR;
	uint32_t MISC_TOG;
};
static_assert(sizeof(usb_analog) == 0x60, "");

void
fsl_imx_usbphy_init(const fsl_imx_usbphy_desc *d)
{
	usbphy *const USBPHY = (usbphy *)d->base;
	usb_analog *const USB_ANALOG = (usb_analog *)d->analog_base;

	/* disable charger & data pin contact detection */
	write32(&USB_ANALOG->CHRG_DETECT, (usb_analog_chrg_detect) {
		.EN_B = 1,
		.CHK_CHRG_B = 1,
		.CHK_CONTACT = 0,
	}.r);

	/* release from reset & ungate clock */
	write32(&USBPHY->CTRL, 0);

	/* configure trimming resistors */
	usbphy_tx tx = USBPHY->TX;
	tx.D_CAL = d->d_cal;
	tx.TXCAL45DP = d->txcal45dp;
	tx.TXCAL45DN = d->txcal45dn;
	write32(&USBPHY->TX, tx.r);

	/* power on phy */
	write32(&USBPHY->PWD, 0);

	/* enable UTMI+ Level 2 & 3 */
	write32(&USBPHY->CTRL, (usbphy_ctrl) {
		.ENUTMILEVEL3 = 1,
		.ENUTMILEVEL2 = 1,
	}.r);

#if defined(CONFIG_DEBUG)
	const usbphy_version v = read32(&USBPHY->VERSION);
	dbg("IMX-USBPHY RTL %d.%d.%d initialised\n", v.MAJOR, v.MINOR, v.STEP);
#endif
}
