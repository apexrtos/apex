#include "imxrt10xx-xbara.h"

#include "init.h"
#include <arch.h>
#include <debug.h>

#define trace(...)

namespace {

std::aligned_storage_t<sizeof(imxrt10xx::xbara), alignof(imxrt10xx::xbara)> mem;

}

namespace imxrt10xx {

#define CPPREG(name) \
	name() : r{0} { } \
	name(auto v) : r{v} { }
/*
 * Registers
 */
struct xbara::regs {
	union xbara_select {
		CPPREG(xbara_select);
		struct {
			uint8_t SEL : 7;
			uint8_t : 1;
		};
		uint8_t r;
	} SEL[xbara::outputs];
	union xbara_ctrl {
		CPPREG(xbara_ctrl);
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
xbara::set_connection(input in, output out)
{
	auto input = static_cast<uint8_t>(in);
	auto output = static_cast<uint8_t>(out);

	assert(input < inputs);
	assert(output < outputs);

	trace("XBARA(%p) Set connection: IN:%d -> OUT:%d\n",
		r_, output, input);
	write8(&r_->SEL[output], input);
}

}

/*
 * nxp_imxrt10xx_xbara_init
 */
extern "C" void
nxp_imxrt10xx_xbara_init(const nxp_imxrt10xx_xbara_desc *d)
{
	dbg("XBARA(%p) Init\n", (void*)d->base);
	new(&mem) imxrt10xx::xbara{d};
}
