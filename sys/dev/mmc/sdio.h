#ifndef dev_mmc_sdio_h
#define dev_mmc_sdio_h

/*
 * SDIO support
 */

namespace mmc { class host; }

namespace mmc::sdio {

int reset(host *);

}

#endif
