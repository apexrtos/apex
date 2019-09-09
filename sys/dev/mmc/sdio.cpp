#include "sdio.h"

#include "command.h"
#include "host.h"
#include <cassert>
#include <debug.h>
#include <errno.h>

namespace mmc::sdio {

namespace {

/*
 * io_rw_direct_write - write register using IO_RW_DIRECT command
 */
int
io_rw_direct_write(host *h, unsigned function_number,
    unsigned register_address, uint8_t data)
{
	if (function_number > 7)
		return DERR(-EINVAL);
	if (register_address > 131071)
		return DERR(-EINVAL);

	const uint32_t rw_flag = 1;
	const uint32_t raw_flag = 0;
	command cmd{52,
	    rw_flag << 31 | function_number << 28 | raw_flag << 27 |
	    register_address << 9 | data,
	    command::response_type::r5};

	return h->run_command(cmd, 0);
}

}

/*
 * reset - reset IO portion of SDIO or SD combo card
 */
int reset(host *h)
{
	/* write a '1' to the RES bit in the CCCR */
	/* REVISIT: if we support SDIO properly one day fix these magics */
	return io_rw_direct_write(h, 0, 6, 8);
}

}
