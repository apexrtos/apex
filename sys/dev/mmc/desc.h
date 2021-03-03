#pragma once

/*
 * Generic SD/MMC support
 */

#include <dev/regulator/voltage/desc.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Supported power supply configurations:
 *
 * 1. single supply
 *  - bga
 *  - non-uhsi sd card host
 *  - set vcc_supply == vio_supply == vccq_supply
 * 2. dual supply for device & i/o (host i/o only)
 *  - uhsi sd card host
 *  - set vcc_supply, vio_supply, vccq_supply = vcc_supply
 * 3. dual supply for device & i/o (host & device i/o)
 *  - bga
 *  - set vcc_supply, vio_supply, vccq_supply = vio_supply
 *
 * vccq_supply must be set equal to vcc_supply or vio_supply.
 */

struct mmc_desc {
	const char *name;		    /* mmc controller name */
	bool removable;			    /* true if device is removable */
	unsigned data_lines;		    /* number of connected data lines */
	unsigned power_stable_delay_ms;	    /* power supply stabilisation time */
	unsigned power_off_delay_ms;	    /* power supply decay time */
	struct volt_reg_desc vcc_supply;    /* power supply for VDD(card)/VCC(bga) */
	struct volt_reg_desc vio_supply;    /* power supply for host I/O lines */
	struct volt_reg_desc vccq_supply;   /* power supply for device I/O lines (VCCQ) */
	unsigned load_capacitance_pf;	    /* bulk capacitive load on data lines */
	unsigned long max_rate;		    /* maximum clock/data rate (Hz) */
};

#ifdef __cplusplus
}
#endif
