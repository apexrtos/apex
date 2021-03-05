#include "imxrt10xx-trng.h"

#include "init.h"
#include <algorithm>
#include <arch/mmio.h>
#include <debug.h>
#include <errno.h>
#include <fs/util.h>
#include <irq.h>
#include <wait.h>

#define trace(...)

namespace {

std::aligned_storage_t<sizeof(imxrt10xx::trng), alignof(imxrt10xx::trng)> mem;

}

namespace imxrt10xx {

struct trng::regs {
	union mctl {
		struct {
			uint32_t SAMP_MODE : 2;
			uint32_t OSC_DIV : 2;
			uint32_t : 2;
			uint32_t RST_DEF : 1;
			uint32_t FOR_SCLK : 1;
			uint32_t FCT_FAIL : 1;
			uint32_t FCT_VAL : 1;
			uint32_t ENT_VAL : 1;
			uint32_t TST_OUT : 1;
			uint32_t ERR : 1;
			uint32_t TSTOP_OK : 1;
			uint32_t LRUN_CONT : 1;
			uint32_t : 1;
			uint32_t PRGM : 1;
			uint32_t : 15;
		};
		uint32_t r;
	} MCTL;

	union scmisc {
		struct {
			uint32_t LRUN_MAX : 8;
			uint32_t : 8;
			uint32_t RTY_CT : 4;
			uint32_t : 12;
		};
		uint32_t r;
	} SCMISC;

	union pkrrng {
		struct {
			uint32_t PRK_RNG : 16;
			uint32_t : 16;
		};
		uint32_t r;
	} PKRRNG;

	union pkrmaxsq {
		struct {
			uint32_t PRK_MAX : 24;
			uint32_t : 8;
		} MAX;
		struct {
			uint32_t PRK_SQ : 24;
			uint32_t : 8;
		} SQ;
		uint32_t r;
	} PKRMAXSQ;

	union sdctl {
		struct {
			uint32_t SAMP_SIZE : 16;
			uint32_t ENT_DLY : 16;
		};
		uint32_t r;
	} SDCTL;

	union sblimtotsam {
		struct {
			uint32_t SB_LIM : 10;
			uint32_t ENT_DLY : 22;
		} SBLIM;
		struct {
			uint32_t TOT_SAM : 20;
			uint32_t : 12;
		} TOTSAM;
		uint32_t r;
	} SBLIMTOTSAM;

	union frqmin {
		struct {
			uint32_t FRQ_MIN : 22;
			uint32_t : 10;
		};
		uint32_t r;
	} FRQMIN;

	union frqcntmax {
		struct {
			uint32_t FRQ_CT : 22;
			uint32_t : 10;
		} CNT;
		struct {
			uint32_t FRQ_MAX : 22;
			uint32_t : 10;
		} MAX;
		uint32_t r;
	} FRQCNTMAX;

	union scmcml {
		struct {
			uint32_t MONO_CT : 16;
			uint32_t : 16;
		} MC;
		struct {
			uint32_t MONO_MAX : 16;
			uint32_t MONO_RNG : 16;
		} ML;
		uint32_t r;
	} SCMCML;

	union scr1cl {
		struct {
			uint32_t R1_0_CT : 15;
			uint32_t : 1;
			uint32_t R1_1_CT : 15;
			uint32_t : 1;
		} COUNT;
		struct {
			uint32_t RUN1_MAX : 15;
			uint32_t : 1;
			uint32_t RUN1_RNG : 15;
			uint32_t : 1;
		} LIMIT;
		uint32_t r;
	} SRC1CL;

	union scr2cl {
		struct {
			uint32_t R2_0_CT : 14;
			uint32_t : 2;
			uint32_t R2_1_CT : 14;
			uint32_t : 2;
		} COUNT;
		struct {
			uint32_t RUN2_MAX : 14;
			uint32_t : 2;
			uint32_t RUN2_RNG : 14;
			uint32_t : 2;
		} LIMIT;
		uint32_t r;
	} SRC2CL;

	union scr3cl {
		struct {
			uint32_t R3_0_CT : 13;
			uint32_t : 3;
			uint32_t R3_1_CT : 13;
			uint32_t : 3;
		} COUNT;
		struct {
			uint32_t RUN3_MAX : 13;
			uint32_t : 3;
			uint32_t RUN3_RNG : 13;
			uint32_t : 3;
		} LIMIT;
		uint32_t r;
	} SRC3CL;

	union scr4cl {
		struct {
			uint32_t R4_0_CT : 12;
			uint32_t : 4;
			uint32_t R4_1_CT : 12;
			uint32_t : 4;
		} COUNT;
		struct {
			uint32_t RUN4_MAX : 12;
			uint32_t : 4;
			uint32_t RUN4_RNG : 12;
			uint32_t : 4;
		} LIMIT;
		uint32_t r;
	} SRC4CL;

	union scr5cl {
		struct {
			uint32_t R5_0_CT : 11;
			uint32_t : 5;
			uint32_t R5_1_CT : 11;
			uint32_t : 5;
		} COUNT;
		struct {
			uint32_t RUN5_MAX : 11;
			uint32_t : 5;
			uint32_t RUN5_RNG : 11;
			uint32_t : 5;
		} LIMIT;
		uint32_t r;
	} SRC5CL;

	union scr6pcl {
		struct {
			uint32_t R6P_0_CT : 11;
			uint32_t : 5;
			uint32_t R6P_1_CT : 11;
			uint32_t : 5;
		} COUNT;
		struct {
			uint32_t RUN6_MAX : 11;
			uint32_t : 5;
			uint32_t RUN6_RNG : 11;
			uint32_t : 5;
		} LIMIT;
		uint32_t r;
	} SRC6PCL;

	union status {
		struct {
			uint32_t TF1BR0 : 1;
			uint32_t TF1BR1 : 1;
			uint32_t TF2BR0 : 1;
			uint32_t TF2BR1 : 1;
			uint32_t TF3BR0 : 1;
			uint32_t TF3BR1 : 1;
			uint32_t TF4BR0 : 1;
			uint32_t TF4BR1 : 1;
			uint32_t TF5BR0 : 1;
			uint32_t TF5BR1 : 1;
			uint32_t TF6PBR0 : 1;
			uint32_t TF6PBR1 : 1;
			uint32_t TFSB : 1;
			uint32_t TFLR : 1;
			uint32_t TFP : 1;
			uint32_t TFMB : 1;
			uint32_t RETRY_CT : 4;
			uint32_t : 12;
		};
		uint32_t r;
	} STATUS;

	uint32_t ENT[16];
	uint32_t PKR[8];

	union seccfg {
		struct {
			uint32_t : 1;
			uint32_t NO_PRGM : 1;
			uint32_t : 30;
		};
		uint32_t r;
	} SEC_CFG;

	union intctrl {
		struct {
			uint32_t HW_ERR : 1;
			uint32_t ENT_VAL : 1;
			uint32_t FRQ_CT_FAIL : 1;
			uint32_t : 29;
		};
		uint32_t r;
	} INT_CTRL;
	intctrl INT_MASK;
	intctrl INT_STATUS;

	uint32_t reserved[16];

	union vid1 {
		struct {
			uint32_t MIN_REV : 8;
			uint32_t MAJ_REV : 8;
			uint32_t IP_ID : 16;
		};
		uint32_t r;
	} VID1;

	union vid2 {
		struct {
			uint32_t CNOFIG_OPT : 8;
			uint32_t ECO_REV : 8;
			uint32_t INTG_OPT :8;
			uint32_t ERA : 8;
		};
		uint32_t r;
	} VID2;
};

trng::trng(const nxp_imxrt10xx_trng_desc *d)
: r_{reinterpret_cast<regs *>(d->base)}
, index_{sizeof(r_->ENT)}
{
	static_assert(sizeof(regs) == 0xF8);
	static_assert(BYTE_ORDER == LITTLE_ENDIAN);

	event_init(&wakeup_, "trng", event::ev_IO);

	/* Put TRNG into program mode and reset to defaults */
	write32(&r_->MCTL, [&]{
		decltype(r_->MCTL) v{};
		v.PRGM = 1;
		v.RST_DEF = 1;
		v.FOR_SCLK = 0; /* Ring Oscillator */
		v.OSC_DIV = 0; /* Ring Oscillator / 1 */
		v.SAMP_MODE = 0; /* Von Neumann */
		return v.r;
	}());

	/*
	 * Enable interrupts:
	 * Entropy ready
	 * Hardware error
	 */
	write32(&r_->INT_MASK, [&]{
		decltype(r_->INT_MASK) v{};
		v.ENT_VAL = 1;
		v.HW_ERR = 1;
		return v.r;
	}());

	/* Set to Run mode */
	write32(&r_->MCTL, [&]{
		auto v = read32(&r_->MCTL);
		v.PRGM = 0;
		return v.r;
	}());

	/* Lock registers */
	write32(&r_->SEC_CFG, [&]{
		decltype(r_->SEC_CFG) v{};
		v.NO_PRGM = 1;
		return v.r;
	}());

	/* Read last entropy register to start entropy generation */
	read32(std::end(r_->ENT) - 1);

	irq_attach(d->irq, d->ipl, 0, isr_wrapper, nullptr, this);
}

trng *
trng::inst()
{
	return reinterpret_cast<trng *>(&mem);
}

ssize_t
trng::read(std::span<std::byte> buf)
{
	trace("trng::read: buf: %p len: %d\n", data(buf), size(buf));

	const auto len{ssize(buf)};
	std::unique_lock l{lock_};
	while(true) {
		while (index_ != sizeof(r_->ENT) && !empty(buf)) {
			const uint32_t tmp{read32(&r_->ENT[index_ / 4])};
			const auto rd{index_ % 4};
			const auto sz{std::min(size(buf), 4 - rd)};
			memcpy(data(buf), reinterpret_cast<const std::byte *>(&tmp) + rd, sz);
			buf = buf.subspan(sz);
			index_ += sz;
		}

		if (empty(buf))
			break;
		/*
		 * SDK_2.8.0_EVK-MIMXRT1060 states the following:
		 *
		 * " Dummy read. Defect workaround.
		 * TRNG could not clear ENT_VAL  flag automatically, application
		 * had to do a dummy reading operation for anyone TRNG register
		 * to clear it firstly, then to read the RTENT0 to RTENT15 again. "
		 *
		 * This is not mentioned in:
		 * MIMXRT1060 Errata REV 1.1
		 * MIMXRT1050 Errata REV 2.2
		 * MIMXRT1020 Errata REV 1.0
		 */
		read32(&r_->ENT[0]);

		/* wait for more entropy */
		regs::mctl r;
		wait_event_lock(wakeup_, l, [&] {
			r = read32(&r_->MCTL);
			return r.ENT_VAL || r.ERR;
		});
		if (r.ERR) {
			trace("TRNG(%p): entropy generation error\n", r_);

			/* Write 1 to ERR to clear ERR or FCT_FAIL */
			write32(&r_->MCTL, [&]{
				auto v = read32(&r_->MCTL);
				v.ERR = 1;
				return v.r;
			}());

			trace("TRNG(%p): restarting entropy generation\n", r_);
			read32(std::end(r_->ENT) - 1);
			return DERR(-EIO);
		}

		index_ = 0;
	}

	return len;
}

void
trng::isr()
{
	trace("trng::isr\n");

	/*
	 * The following can generate this interrupt:
	 * 1. Entropy valid
	 * 2. Hardware error
	 * 3. Frequency count failure
	 *
	 * Clear all.
	 */
	write32(&r_->INT_CTRL, 0);
	sch_wakeone(&wakeup_);
}

int
trng::isr_wrapper(int vector, void *data)
{
	static_cast<trng *>(data)->isr();
	return INT_DONE;
}

}

ssize_t
trng_read_iov(file *f, const iovec *iov, size_t count, off_t offset)
{
	return for_each_iov(iov, count, offset,
	    [](std::span<std::byte> buf, off_t offset) {
		return imxrt10xx::trng::inst()->read(buf);
	});
}

/*
 * nxp_imxrt10xx_trng_init
 */
void
nxp_imxrt10xx_trng_init(const nxp_imxrt10xx_trng_desc *d)
{
	new(&mem) imxrt10xx::trng{d};

#if defined(CONFIG_DEBUG)
	const auto v = read32(&imxrt10xx::trng::inst()->r_->VID1);
	dbg("TRNG ID %d REVISION %d.%d initialised\n", v.IP_ID, v.MAJ_REV, v.MIN_REV);
#endif

	static constinit devio io{
		.read = trng_read_iov,
	};

	if (!device_create(&io, d->name, DF_CHR, &mem))
		DERR(-EINVAL);
}
