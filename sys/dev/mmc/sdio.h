#pragma once

/*
 * SDIO support
 */

namespace mmc { class host; }

namespace mmc::sdio {

int reset(host *);

}
