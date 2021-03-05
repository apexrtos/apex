#include "imxrt10xx-xbara.h"

#include "init.h"
#include <arch/mmio.h>
#include <debug.h>
#include <endian.h>
#include <new>

#define trace(...)

namespace {

std::aligned_storage_t<sizeof(imxrt10xx::xbara), alignof(imxrt10xx::xbara)> mem;

}

namespace imxrt10xx {

/*
 * Registers
 */
struct xbara::regs {
	union select {
		struct {
			uint8_t SEL : 7;
			uint8_t : 1;
		};
		uint8_t r;
	} SEL[132];
	union ctrl {
		struct {
			uint8_t DEN : 1;
			uint8_t IEN : 1;
			uint8_t EDGE : 2;
			uint8_t STS : 1;
			uint8_t : 3;
		};
		uint8_t r;
	} CTRL[4];
};

/*
 * XBAR(A) Module
 */

xbara::xbara(const nxp_imxrt10xx_xbara_desc *d)
: r_{reinterpret_cast<regs*>(d->base)}
{
	static_assert(sizeof(regs) == 0x88, "");
	static_assert(BYTE_ORDER == LITTLE_ENDIAN, "");
}

xbara *
xbara::inst()
{
	return reinterpret_cast<xbara *>(&mem);
}

void
xbara::set_connection(output out, input in)
{
	const auto o{static_cast<unsigned>(out)};
	const auto i{static_cast<unsigned>(in)};

	trace("XBARA(%p) Set connection: OUT:%u <- IN:%u\n", r_, o, i);
	write8(&r_->SEL[o], i);
}

}

/*
 * nxp_imxrt10xx_xbara_init
 */
void
nxp_imxrt10xx_xbara_init(const nxp_imxrt10xx_xbara_desc *d)
{
	dbg("XBARA(%p) Init\n", (void*)d->base);

	auto x{new(&mem) imxrt10xx::xbara{d}};

	for (const auto &c : d->config)
		x->set_connection(c.first, c.second);
}
