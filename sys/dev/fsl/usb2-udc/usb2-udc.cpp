#include "init.h"

#include <arch/barrier.h>
#include <arch/cache.h>
#include <arch/mmio.h>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <debug.h>
#include <dev/usb/gadget/transaction.h>
#include <dev/usb/gadget/udc.h>
#include <dma.h>
#include <endian.h>
#include <irq.h>
#include <kernel.h>
#include <mutex>
#include <page.h>

#define trace(...)

namespace {

using namespace usb;

class fsl_usb2_udc;
class fsl_usb2_transaction;

/*
 * regs
 */
struct regs {
	union id {
		struct {
			uint32_t ID : 6;
			uint32_t : 2;
			uint32_t NID : 6;
			uint32_t : 2;
			uint32_t REVISION : 8;
			uint32_t : 8;
		};
		uint32_t r;
	} ID;
	uint32_t HWGENERAL;
	uint32_t : 32;
	uint32_t HWDEVICE;
	uint32_t HWTXBUF;
	uint32_t HWRXBUF;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t GPTIMER0LD;
	uint32_t GPTIMER0CTRL;
	uint32_t GPTIMER1LD;
	uint32_t GPTIMER1CTRL;
	uint32_t SBUSCFG;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint8_t CAPLENGTH;
	uint8_t : 8;
	uint16_t : 16;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint16_t DCIVERSION;
	uint16_t : 16;
	union dccparams {
		struct {
			uint32_t DEN : 5;
			uint32_t : 2;
			uint32_t DC : 1;
			uint32_t HC : 1;
			uint32_t : 23;
		};
		uint32_t r;
	} DCCPARAMS;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	union usbcmd {
		/* The IMXRT manual is wrong and the ATDTW bit is in position
		 * 14 like previous generations of hardware. */
		struct {
			uint32_t RS : 1;
			uint32_t RST : 1;
			uint32_t : 2;
			uint32_t PSE : 1;
			uint32_t ASE : 1;
			uint32_t IAA : 1;
			uint32_t : 1;
			uint32_t ASP : 2;
			uint32_t : 1;
			uint32_t ASPE : 1;
			uint32_t : 1;
			uint32_t SUTW : 1;
			uint32_t ATDTW : 1;
			uint32_t : 1;
			uint32_t ITC : 8;
			uint32_t : 8;
		};
		uint32_t r;
	} USBCMD;
	union usbsts {
		struct {
			uint32_t UI : 1;
			uint32_t UEI : 1;
			uint32_t PCI : 1;
			uint32_t : 3;
			uint32_t URI : 1;
			uint32_t SRI : 1;
			uint32_t SLI : 1;
			uint32_t : 1;
			uint32_t ULPII : 1;
			uint32_t : 5;
			uint32_t NAKI : 1;
			uint32_t : 7;
			uint32_t TI0 : 1;
			uint32_t TI1 : 1;
			uint32_t : 6;
		};
		uint32_t r;
	} USBSTS;
	union usbintr {
		struct {
			uint32_t UE : 1;
			uint32_t UEE : 1;
			uint32_t PCE : 1;
			uint32_t : 3;
			uint32_t URE : 1;
			uint32_t SRE : 1;
			uint32_t SLE : 1;
			uint32_t : 1;
			uint32_t ULPIE : 1;
			uint32_t : 5;
			uint32_t NAKE : 1;
			uint32_t : 7;
			uint32_t TIE0 : 1;
			uint32_t TIE1 : 1;
			uint32_t : 6;
		};
		uint32_t r;
	} USBINTR;
	uint32_t FRINDEX;
	uint32_t : 32;
	union deviceaddr {
		struct {
			uint32_t : 24;
			uint32_t USBADRA : 1;
			uint32_t USBADR : 7;
		};
		uint32_t r;
	} DEVICEADDR;
	phys ENDPOINTLISTADDR;
	uint32_t : 32;
	uint32_t BURSTSIZE;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t ENDPTNAK;
	uint32_t ENDPTNAKEN;
	uint32_t : 32;
	union portsc1 {
		struct {
			uint32_t CCS : 1;
			uint32_t : 3;
			uint32_t OCA : 1;
			uint32_t OCC : 1;
			uint32_t FPR : 1;
			uint32_t SUSP : 1;
			uint32_t PR : 1;
			uint32_t HSP : 1;
			uint32_t LS : 2;
			uint32_t PP : 1;
			uint32_t PO : 1;
			uint32_t PIC : 2;
			uint32_t PTC : 4;
			uint32_t : 2;
			uint32_t WKOC : 1;
			uint32_t PHCD : 1;
			uint32_t PFSC : 1;
			uint32_t PTS_2 : 1;
			uint32_t PSPD : 2;
			uint32_t PTW : 1;
			uint32_t STS : 1;
			uint32_t PTS_1 : 2;
		};
		uint32_t r;
	} PORTSC1;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t OTGSC;
	union usbmode {
		struct {
			uint32_t CM : 2;
			uint32_t ES : 1;
			uint32_t SLOM : 1;
			uint32_t SDIS : 1;
			uint32_t : 27;
		};
		uint32_t r;
	} USBMODE;
	uint32_t ENDPTSETUPSTAT;
	uint32_t ENDPTPRIME;
	uint32_t ENDPTFLUSH;
	uint32_t ENDPTSTAT;
	uint32_t ENDPTCOMPLETE;
	union endptctrl {
		struct {
			uint32_t RXS : 1;
			uint32_t RXD : 1;   /* not for endpoint 0 */
			uint32_t RXT : 2;
			uint32_t : 1;
			uint32_t RXI : 1;   /* not for endpoint 0 */
			uint32_t RXR : 1;   /* not for endpoint 0 */
			uint32_t RXE : 1;
			uint32_t : 8;
			uint32_t TXS : 1;
			uint32_t TXD : 1;   /* not for endpoint 0 */
			uint32_t TXT : 2;
			uint32_t : 1;
			uint32_t TXI : 1;   /* not for endpoint 0 */
			uint32_t TXR : 1;   /* not for endpoint 0 */
			uint32_t TXE : 1;
			uint32_t : 8;
		};
		uint32_t r;
	} ENDPTCTRL[8];
};
static_assert(sizeof(regs) == 0x1e0);
static_assert(BYTE_ORDER == LITTLE_ENDIAN);

/*
 * dtd
 *
 * endpoint transfer descriptor
 */
struct dtd {
	dtd *next_link;
	union {
		struct {
			struct {
				uint8_t : 3;
				uint8_t transaction_error : 1;
				uint8_t : 1;
				uint8_t data_buffer_error : 1;
				uint8_t halted : 1;
				uint8_t active : 1;
			} status;
			uint32_t : 2;
			uint32_t multo : 2;
			uint32_t : 3;
			uint32_t ioc : 1;
			uint32_t total_bytes : 15;
			uint32_t : 1;
		};
		uint32_t r;
	} token;
	uint32_t buffer[5];
	uint32_t : 32;			/* for 32-byte alignment */
};
static_assert(sizeof(dtd) == 32);
inline constexpr size_t dtd_max_buffer_size = 0x1000;

/*
 * dqh
 *
 * device endpoint queue head
 */
struct dqh {
	union {
		struct {
			uint32_t : 15;
			uint32_t ios : 1;
			uint32_t max_packet_len : 11;
			uint32_t : 2;
			uint32_t zlt : 1;
			uint32_t mult : 2;
		};
		uint32_t r;
	} capabilities;
	dtd *current_dtd;
	dtd dtd_overlay;
	uint8_t setup[8];		/* h/w mapped i/o ends here */
	fsl_usb2_transaction *transaction;
	bool open;
	uint32_t : 24;			/* for 64-byte alignment */
	uint32_t : 32;			/* for 64-byte alignment */
	uint32_t : 32;			/* for 64-byte alignment */
};
static_assert(sizeof(dqh) == 64);
dtd *const dtd_terminate = reinterpret_cast<dtd *>(1);

/*
 * sanity checks
 */
static_assert(static_cast<int>(ch9::TransferType::Control) == 0);
static_assert(static_cast<int>(ch9::TransferType::Isochronous) == 1);
static_assert(static_cast<int>(ch9::TransferType::Bulk) == 2);
static_assert(static_cast<int>(ch9::TransferType::Interrupt) == 3);
static_assert(static_cast<int>(ch9::Direction::HostToDevice) == 0);
static_assert(static_cast<int>(ch9::Direction::DeviceToHost) == 1);
static_assert(sizeof(ch9::setup_data) == sizeof(((dqh *)0)->setup));

/*
 * fsl_usb2_udc
 */
class fsl_usb2_udc final : public gadget::udc {
public:
	fsl_usb2_udc(const char *name, regs *r);

	void isr();

	dtd *alloc_dtd();
	void free_dtd(dtd *);

private:
	int v_start() override;
	void v_stop() override;
	int v_reset() override;
	int v_bus_reset() override;
	int v_port_change() override;
	int v_open_endpoint(size_t endpoint, ch9::Direction, ch9::TransferType,
	    size_t max_packet_len) override;
	void v_close_endpoint(size_t endpoint, ch9::Direction) override;
	std::unique_ptr<gadget::transaction> v_alloc_transaction() override;
	int v_queue(size_t endpoint, ch9::Direction,
	    gadget::transaction*) override;
	int v_queue_setup(size_t endpoint, ch9::Direction,
	    gadget::transaction*) override;
	int v_flush(size_t endpoint, ch9::Direction) override;
	void v_complete(size_t endpoint, ch9::Direction) override;
	void v_set_stall(size_t endpoint, bool) override;
	void v_set_stall(size_t endpoint, ch9::Direction, bool) override;
	int v_get_stall(size_t endpoint, ch9::Direction) override;
	void v_set_address(unsigned address) override;
	void v_setup_aborted(size_t endpoint) override;

	int do_queue(size_t endpoint, ch9::Direction, gadget::transaction *);
	void hw_init();
	void reset_queues();
	dqh &get_dqh(size_t endpoint, ch9::Direction);
	uint32_t epbit(size_t endpoint, ch9::Direction);

	regs *const r_;
	dqh *const dqh_;
	dtd *dtd_free_;

	a::spinlock_irq setup_lock_;	/* for setup requests */
	a::spinlock_irq dtd_lock_;	/* for dtd free list */
	a::mutex lock_;			/* for device controller state */

	static constexpr size_t mem_size = 4096;
	static constexpr size_t num_dqh = 32;
	static constexpr size_t num_dtd = (mem_size - num_dqh * sizeof(dqh)) /
					  sizeof(dtd);
};

/*
 * fsl_usb2_transaction
 */
class fsl_usb2_transaction final : public gadget::transaction {
public:
	fsl_usb2_transaction(fsl_usb2_udc *);
	~fsl_usb2_transaction();

	fsl_usb2_transaction *next();
	dtd *dtd_head();
	void enqueue(fsl_usb2_transaction *);
	int status() const;

	int start(size_t, ch9::Direction);
	void transferred(size_t, ch9::Direction);
	void retire(int status);

private:
	void free_dtds();

	fsl_usb2_udc *udc_;
	dtd *dtd_head_;
	dtd *dtd_tail_;
	fsl_usb2_transaction *next_;
};

/*
 * fsl_usb2_udc implementation
 */
fsl_usb2_udc::fsl_usb2_udc(const char *name, regs *r)
: gadget::udc{name, r->DCCPARAMS.DEN}
, r_{r}
, dqh_{static_cast<dqh *>(phys_to_virt(page_alloc(
    mem_size, MA_FAST | MA_DMA | MA_CACHE_COHERENT,
    static_cast<void *>(this)).release()))}
{
	/* dqh must be 2k aligned */
	assert(dqh_);
	assert(!(reinterpret_cast<uintptr_t>(dqh_) & 2047));

	/* up to 16 endpoints supported */
	assert(endpoints() > 0);
	assert(endpoints() * 2 <= num_dqh);

	std::lock_guard l{lock_};

	/* initialise dtd free list */
	auto dtd_mem = reinterpret_cast<dtd *>(
	    reinterpret_cast<uintptr_t>(dqh_) + num_dqh * sizeof(dqh));
	dtd_free_ = dtd_mem;
	for (size_t i = 1; i < num_dtd - 1; ++i)
		dtd_mem[i - 1].next_link = &dtd_mem[i];
	dtd_mem[num_dtd - 1].next_link = nullptr;

	/* initialise queue heads */
	for (size_t i = 0; i < num_dqh; ++i) {
		dqh_[i].transaction = nullptr;
		dqh_[i].open = false;
	}

	/* issue controller reset */
	write32(&r_->USBCMD, {{.RST = 1}});

	dbg("FSL-USB2-UDC ID %d REVISION %d initialised\n",
	    r_->ID.ID, r_->ID.REVISION);
}

void
fsl_usb2_udc::isr()
{
	uint32_t v;

	/* read & ack interrupts */
	const auto s = read32(&r_->USBSTS);
	write32(&r_->USBSTS, s);

	/* reset received */
	if (s.URI) {
		write32(&r_->ENDPTSETUPSTAT, 0xffffffff);
		write32(&r_->ENDPTCOMPLETE, 0xffffffff);
		while (read32(&r_->ENDPTPRIME));
		write32(&r_->ENDPTFLUSH, 0xffffffff);

		/* issue controller reset if we missed reset window */
		if (read32(&r_->PORTSC1).PR)
			bus_reset_irq();
		else {
			write32(&r_->USBCMD, {{.RST = 1}});
			reset_irq();
		}
	}

	/* completed transaction where descriptor requested interrupt */
	/* short packet detected */
	if (s.UI) {
		v = read32(&r_->ENDPTSETUPSTAT);

		if (v) {
			trace("ENDPTSETUPSTAT %x\n", v);
			write32(&r_->ENDPTSETUPSTAT, v);

			/* synchronise with v_queue_setup */
			std::lock_guard l{setup_lock_};

			auto usbcmd = read32(&r_->USBCMD);
			usbcmd.SUTW = 1;

			for (int e; (e = __builtin_ffsl(v)); v -= 1UL << e) {
				e -= 1; /* ffsl returns 1 + bit number */

				/* cancel any primed transfers pending from a
				 * previous setup transaction - note that the
				 * hardware clears ENDPTSTAT if a setup request
				 * is received, and won't prime if a setup
				 * request is pending */
				setup_aborted_irq(e);

				/* read using tripwire for synchronisation */
				ch9::setup_data s;
				do {
					write32(&r_->USBCMD, usbcmd);
					/* ensure that tripwire is observable
					   before reading setup data */
					write_memory_barrier();
					memcpy(&s, dqh_[e].setup, sizeof(s));
					/* ensure that setup data reads
					   complete before checking tripwire */
					read_memory_barrier();
				} while (!read32(&r_->USBCMD).SUTW);

				setup_request_irq(e, s);
			}

			/* writing the tripwire back to 0 is unnecessary */
		}

		v = read32(&r_->ENDPTCOMPLETE);

		if (v) {
			trace("ENDPTCOMPLETE %x\n", v);
			write32(&r_->ENDPTCOMPLETE, v);
			for (int i; (i = __builtin_ffsl(v)); v -= 1UL << i) {
				i -= 1; /* ffsl returns 1 + bit number */
				ep_complete_irq(i & 0xf, i & 0x10
				    ? ch9::Direction::DeviceToHost
				    : ch9::Direction::HostToDevice);
			}
		}
	}

	/* connection state changed */
	if (s.PCI) {
		auto portsc1 = read32(&r_->PORTSC1);
		Speed spd;
		switch (portsc1.PSPD) {
		case 0: spd = Speed::Full; break;
		case 1: spd = Speed::Low; break;
		case 2: spd = Speed::High; break;
		case 3: spd = Speed::Low; break;   /* undefined */
		}
		port_change_irq(portsc1.CCS, spd);
	}
}

dtd *
fsl_usb2_udc::alloc_dtd()
{
	std::lock_guard l{dtd_lock_};

	auto p = dtd_free_;
	dtd_free_ = dtd_free_->next_link;
	return p;
}

void
fsl_usb2_udc::free_dtd(dtd *p)
{
	std::lock_guard l{dtd_lock_};

	p->next_link = dtd_free_;
	dtd_free_ = p;
}

int
fsl_usb2_udc::v_start()
{
	std::lock_guard l{lock_};

	hw_init();

	/* start controller */
	write32(&r_->USBCMD, [&]{
		auto v = read32(&r_->USBCMD);
		v.RS = 1;
		return v;
	}());

	return 0;
}

void
fsl_usb2_udc::v_stop()
{
	std::lock_guard l{lock_};

	/* issue controller reset */
	write32(&r_->USBCMD, {{.RST = 1}});

	/* wait for reset to complete */
	while (read32(&r_->USBCMD).RST);

	reset_queues();
}

int
fsl_usb2_udc::v_reset()
{
	std::lock_guard l{lock_};

	hw_init();

	return 0;
}

int
fsl_usb2_udc::v_bus_reset()
{
	std::lock_guard l{lock_};

	reset_queues();

	return 0;
}

int
fsl_usb2_udc::v_port_change()
{
	return 0;
}

int
fsl_usb2_udc::v_open_endpoint(size_t endpoint, ch9::Direction dir,
    ch9::TransferType tt, size_t max_packet_len)
{
	std::lock_guard l{lock_};

	if (endpoint >= endpoints())
		return DERR(-EINVAL);

	auto &q = get_dqh(endpoint, dir);

	if (q.open)
		return DERR(-EBUSY);

	decltype(q.capabilities) cap{};
	if (tt == ch9::TransferType::Isochronous ||
	    tt == ch9::TransferType::Interrupt)
		cap.mult = (max_packet_len >> 11) + 1;
	else
		cap.mult = 0;
	cap.zlt = 1; /* disable automatic zero length packet generation */
	cap.max_packet_len = max_packet_len;
	cap.ios = tt == ch9::TransferType::Control;
	q.capabilities.r = cap.r;

	q.dtd_overlay.next_link = dtd_terminate;
	q.dtd_overlay.token.r = 0;

	/* ensure writes to queue are observable */
	write_memory_barrier();

	auto ctrl = read32(&r_->ENDPTCTRL[endpoint]);
	if (dir == ch9::Direction::HostToDevice) {
		ctrl.RXE = 1;
		ctrl.RXR = 1;
		ctrl.RXT = static_cast<int>(tt);
	} else {
		ctrl.TXE = 1;
		ctrl.TXR = 1;
		ctrl.TXT = static_cast<int>(tt);
	}
	write32(&r_->ENDPTCTRL[endpoint], ctrl);

	q.open = true;

	return 0;
}

void
fsl_usb2_udc::v_close_endpoint(size_t endpoint, ch9::Direction dir)
{
	std::lock_guard l{lock_};

	if (endpoint >= endpoints())
		return;

	auto &q = get_dqh(endpoint, dir);

	if (!q.open)
		return;

	/* shutdown endpoint */
	auto ctrl = read32(&r_->ENDPTCTRL[endpoint]);
	if (dir == ch9::Direction::HostToDevice)
		ctrl.RXE = 0;
	else
		ctrl.TXE = 0;
	write32(&r_->ENDPTCTRL[endpoint], ctrl);

	/* abort any queued transactions */
	while (q.transaction) {
		/* careful, retire can free */
		const auto n = q.transaction->next();
		q.transaction->retire(-ECANCELED);
		q.transaction = n;
	}
	q.open = false;

	trace("v_close_endpoint ep %zu dir %d transaction %p\n",
	    endpoint, static_cast<int>(dir), q.transaction);
}

std::unique_ptr<gadget::transaction>
fsl_usb2_udc::v_alloc_transaction()
{
	return std::make_unique<fsl_usb2_transaction>(this);
}

int
fsl_usb2_udc::v_queue(size_t endpoint, ch9::Direction dir,
    gadget::transaction *tb)
{
	std::lock_guard l{lock_};

	return do_queue(endpoint, dir, tb);
}

int
fsl_usb2_udc::v_queue_setup(size_t endpoint, ch9::Direction dir,
			    gadget::transaction *tb)
{
	std::lock_guard l{lock_};

	/* setup transactions need to be queued with interrupts disabled as we
	 * must never respond to a new setup request with old data */
	std::lock_guard sl{setup_lock_};

	/* a new setup request was received before we responded */
	if (setup_requested(endpoint))
		return 0;

	return do_queue(endpoint, dir, tb);
}

int
fsl_usb2_udc::v_flush(size_t endpoint, ch9::Direction dir)
{
	std::lock_guard l{lock_};

	trace("v_flush endpoint:%zu dir:%d\n", endpoint, static_cast<int>(dir));

	if (endpoint >= endpoints())
		return DERR(-EINVAL);

	auto &q = get_dqh(endpoint, dir);

	if (!q.open)
		return DERR(-EINVAL);

	const auto epb = epbit(endpoint, dir);

	/* flush endpoint */
	while (read32(&r_->ENDPTPRIME) & epb);
	while (read32(&r_->ENDPTSTAT) & epb) {
		write32(&r_->ENDPTFLUSH, epb);
		while (read32(&r_->ENDPTFLUSH));
	}

	/* abort any queued transactions */
	while (q.transaction) {
		/* careful, retire can free */
		const auto n = q.transaction->next();
		q.transaction->retire(-ECANCELED);
		q.transaction = n;
	}

	return 0;
}

void
fsl_usb2_udc::v_complete(size_t endpoint, ch9::Direction dir)
{
	std::unique_lock l{lock_};

	trace("v_complete endpoint:%zu dir:%d\n",
	    endpoint, static_cast<int>(dir));
	auto &q = get_dqh(endpoint, dir);
	int status;
	while (q.transaction && (status = q.transaction->status()) != -EBUSY) {
		/* careful, retire can free or requeue */
		const auto t = q.transaction;
		q.transaction = q.transaction->next();
		l.unlock();
		t->transferred(status, dir);
		t->retire(status);
		l.lock();
	}
	trace("v_complete ep %zu dir %d transaction %p\n",
	    endpoint, static_cast<int>(dir), q.transaction);
}

void
fsl_usb2_udc::v_set_stall(size_t endpoint, bool stall)
{
	std::lock_guard l{lock_};

	trace("v_set_stall ep:%zu stall:%d\n", endpoint, stall);

	assert(endpoint < endpoints());

	auto ctrl = read32(&r_->ENDPTCTRL[endpoint]);
	ctrl.TXS = stall;
	ctrl.RXS = stall;
	if (!stall && endpoint != 0) {
		ctrl.TXR = true;    /* reset TX data toggle */
		ctrl.RXR = true;    /* reset RX data toggle */
	}
	write32(&r_->ENDPTCTRL[endpoint], ctrl);
}

void
fsl_usb2_udc::v_set_stall(size_t endpoint, ch9::Direction dir, bool stall)
{
	std::lock_guard l{lock_};

	trace("v_set_stall ep:%zu dir:%d stall:%d\n", endpoint,
	    static_cast<int>(dir), stall);

	assert(endpoint < endpoints());

	auto ctrl = read32(&r_->ENDPTCTRL[endpoint]);
	if (dir == ch9::Direction::DeviceToHost && ctrl.TXE) {
		ctrl.TXS = stall;
		if (!stall && endpoint != 0)
			ctrl.TXR = true;    /* reset TX data toggle */
	}
	if (dir == ch9::Direction::HostToDevice && ctrl.RXE) {
		ctrl.RXS = stall;
		if (!stall && endpoint != 0)
			ctrl.RXR = true;    /* reset RX data toggle */
	}
	write32(&r_->ENDPTCTRL[endpoint], ctrl);
}

int
fsl_usb2_udc::v_get_stall(size_t endpoint, ch9::Direction dir)
{
	std::lock_guard l{lock_};

	assert(endpoint < endpoints());

	auto ctrl = read32(&r_->ENDPTCTRL[endpoint]);
	if (dir == ch9::Direction::DeviceToHost) {
		if (!ctrl.TXE)
			return -EINVAL;	    /* endpoint not enabled */
		return ctrl.TXS;
	} else {
		if (!ctrl.RXE)
			return -EINVAL;	    /* endpoint not enabled */
		return ctrl.RXS;
	}
}

void
fsl_usb2_udc::v_set_address(unsigned address)
{
	std::lock_guard l{lock_};

	assert(address < 128);
	write32(&r_->DEVICEADDR, {{.USBADR = address}});
}

void
fsl_usb2_udc::v_setup_aborted(size_t endpoint)
{
	std::lock_guard l{lock_};

	/* we probably don't need to flush the endpoint here as the hardware
	 * has probably done it for us but the available documentation is a
	 * bit unclear on this particular point */
	const auto epb =
	    epbit(endpoint, ch9::Direction::HostToDevice) |
	    epbit(endpoint, ch9::Direction::DeviceToHost);
	while (read32(&r_->ENDPTPRIME) & epb);
	while (read32(&r_->ENDPTSTAT) & epb) {
		write32(&r_->ENDPTFLUSH, epb);
		while (read32(&r_->ENDPTFLUSH));
	}

	auto cancel_txn = [](dqh &q) {
		if (!q.transaction)
			return;
		assert(!q.transaction->next());
		q.transaction->retire(-ECANCELED);
		q.transaction = nullptr;
	};

	cancel_txn(get_dqh(endpoint, ch9::Direction::DeviceToHost));
	cancel_txn(get_dqh(endpoint, ch9::Direction::HostToDevice));
}

int
fsl_usb2_udc::do_queue(size_t endpoint, ch9::Direction dir,
		       gadget::transaction *tb)
{
	lock_.assert_locked();

	trace("do_queue ep:%zu dir:%d t:%p\n", endpoint, static_cast<int>(dir),
	    tb);

	auto t = dynamic_cast<fsl_usb2_transaction *>(tb);

	if (!t)
		return DERR(-EINVAL);

	if (endpoint >= endpoints())
		return DERR(-EINVAL);

	auto &q = get_dqh(endpoint, dir);

	if (!q.open)
		return DERR(-EINVAL);

	/* prepare transfer descriptors */
	if (auto r = t->start(q.capabilities.max_packet_len, dir); r < 0)
		return r;

	const auto epb = epbit(endpoint, dir);

	trace("dump queue ep %zu dir %d\n", endpoint, static_cast<int>(dir));
	for (auto qt = q.transaction; qt; qt = qt->next()) {
		trace("-> %p\n", qt);
		assert(qt != t);
	}

	/* enqueue transacation */
	if (q.transaction) {
		/* iterate to queue transaction list tail */
		auto qt = q.transaction;
		while (qt->next())
			qt = qt->next();

		qt->enqueue(t);

		/* ensure writes to transfer descriptors are observable */
		write_memory_barrier();

		/* perform synchronisation dance with hardware */
		/* this comes straight from the "Executing A Transfer
		 * Descriptor" section of the imxRT1060 reference manual */
		if (read32(&r_->ENDPTPRIME) & epb)
			return 0;
		auto cmd = read32(&r_->USBCMD);
		cmd.ATDTW = 1;
		uint32_t stat;
		do {
			write32(&r_->USBCMD, cmd);
			stat = read32(&r_->ENDPTSTAT);
		} while (!read32(&r_->USBCMD).ATDTW);
		cmd.ATDTW = 0;
		write32(&r_->USBCMD, cmd);
		if (stat & epb) {
			trace("enqueue continue ep %zu dir %d q %p txn %p\n",
			    endpoint, static_cast<int>(dir), t, qt);
			return 0;
		}
	} else
		q.transaction = t;

	trace("enqueue prime ep %zu dir %d q %p txn %p\n",
	    endpoint, static_cast<int>(dir), t, q.transaction);

	/* fill in queue head */
	q.dtd_overlay.next_link = t->dtd_head();
	q.dtd_overlay.token.r = 0;

	/* ensure writes to transfer descriptors & queue head are observable */
	write_memory_barrier();

	/* prime endpoint */
	write32(&r_->ENDPTPRIME, epb);

	return 0;
}

void
fsl_usb2_udc::hw_init()
{
	lock_.assert_locked();

	reset_queues();

	/* wait for reset to complete */
	while (read32(&r_->USBCMD).RST);

	/* configure as device controller */
	write32(&r_->USBMODE.r, []{
		decltype(r_->USBMODE) v{};
		v.CM = 2;
		v.SLOM = 1;
		return v.r;
	}());

	/* configure queue head */
	write32(&r_->ENDPOINTLISTADDR, virt_to_phys(dqh_));

	/* configure interrupts */
	write32(&r_->USBINTR, {{
		.UE = 1,
		.PCE = 1,
		.URE = 1,
	}});

	/* set interrupt threshold to immediate */
	write32(&r_->USBCMD, [&]{
		auto v = read32(&r_->USBCMD);
		v.ITC = 0;
		return v;
	}());
}

void
fsl_usb2_udc::reset_queues()
{
	lock_.assert_locked();

	/* abort any outstanding transcations & reset queue heads */
	for (size_t i = 0; i < num_dqh; ++i) {
		auto &q = dqh_[i];
		while (q.transaction) {
			/* careful, retire can free */
			const auto n = q.transaction->next();
			q.transaction->retire(-ECANCELED);
			q.transaction = n;
		}
		q.transaction = nullptr;
		q.open = false;
	}
}

dqh&
fsl_usb2_udc::get_dqh(size_t endpoint, ch9::Direction dir)
{
	assert(endpoint < endpoints());
	return dqh_[endpoint * 2 + static_cast<int>(dir)];
}

uint32_t
fsl_usb2_udc::epbit(size_t endpoint, ch9::Direction dir)
{
	/* corresponds to bit in ENDPTPRIME, ENDPTFLUSH, ENDPTSTAT,
	 * ENDPTCOMPLETE registers for this endpoint */
	return 1UL << (endpoint + 16 * static_cast<int>(dir));
}

/*
 * fsl_usb2_transaction implementation
 */
fsl_usb2_transaction::fsl_usb2_transaction(fsl_usb2_udc *udc)
: udc_{udc}
, dtd_head_{nullptr}
, dtd_tail_{nullptr}
, next_{nullptr}
{ }

fsl_usb2_transaction::~fsl_usb2_transaction()
{
	free_dtds();
}

fsl_usb2_transaction *
fsl_usb2_transaction::next()
{
	return next_;
}

dtd *
fsl_usb2_transaction::dtd_head()
{
	return dtd_head_;
}

int
fsl_usb2_transaction::status() const
{
	auto p = dtd_head_;
	if (!p)
		return DERR(-EINVAL);
	int rem = 0;
	while (true) {
		const auto t = p->token;
		if (t.status.active)
			return -EBUSY;
		if (t.status.halted)
			return DERR(-EPIPE);
		if (t.status.data_buffer_error)
			return DERR(-EPROTO);
		if (t.status.transaction_error)
			return DERR(-EILSEQ);
		rem += t.total_bytes;
		if (p == dtd_tail_)
			break;
		p = p->next_link;
	}
	return len() - rem;
}

void
fsl_usb2_transaction::enqueue(fsl_usb2_transaction *t)
{
	assert(!next_);
	assert(dtd_tail_ && dtd_tail_->next_link == dtd_terminate);

	next_ = t;
	dtd_tail_->next_link = t->dtd_head();
}

int
fsl_usb2_transaction::start(const size_t max_packet_len,
    const ch9::Direction dir)
{
	assert(!dtd_head_);

	if (!buf() || !len()) {
		dtd_head_ = dtd_tail_ = udc_->alloc_dtd();
		if (!dtd_head_)
			return DERR(-ENOMEM);
		dtd_tail_->token.r = [&]() {
			decltype(dtd_tail_->token) v{};
			v.total_bytes = 0;
			v.ioc = 1;
			v.multo = 0;	/* only for tx ISOs */
			v.status.active = 1;
			return v.r;
		}();
		dtd_tail_->next_link = dtd_terminate;
		started();
		return 0;
	}

	char *tbuf = static_cast<char *>(buf());
	size_t tlen = len();

	while (tlen) {
		auto n = udc_->alloc_dtd();
		if (!n) {
			free_dtds();
			return DERR(-ENOMEM);
		}
		if (!dtd_head_)
			dtd_head_ = dtd_tail_ = n;
		else {
			dtd_tail_->next_link = n;
			dtd_tail_ = n;
		}

		auto base = virt_to_phys(tbuf).phys();

		dtd_tail_->buffer[0] = base;
		base = TRUNCn(base, dtd_max_buffer_size);
		dtd_tail_->buffer[1] = base += dtd_max_buffer_size;
		dtd_tail_->buffer[2] = base += dtd_max_buffer_size;
		dtd_tail_->buffer[3] = base += dtd_max_buffer_size;
		dtd_tail_->buffer[4] = base += dtd_max_buffer_size;

		const auto l = std::min({
			static_cast<size_t>(
				static_cast<char *>(phys_to_virt(phys{base})) - tbuf),
			tlen,
			max_packet_len});

		dtd_tail_->token.r = [&]() {
			decltype(dtd_tail_->token) v{};
			v.total_bytes = l;
			v.ioc = 0;
			v.multo = 0;	/* only for tx ISOs */
			v.status.active = 1;
			return v.r;
		}();
		dtd_tail_->next_link = dtd_terminate;

		tlen -= l;
		tbuf += l;
	}

	if (zero_length_termination() && len() % max_packet_len == 0) {
		trace("zero terminate!\n");
		auto n = udc_->alloc_dtd();
		if (!n) {
			free_dtds();
			return DERR(-ENOMEM);
		}
		dtd_tail_->next_link = n;
		dtd_tail_ = n;
		dtd_tail_->token.r = []() {
			decltype(dtd_tail_->token) v{};
			v.total_bytes = 0;
			v.ioc = 0;
			v.multo = 0;	/* only for tx ISOs */
			v.status.active = 1;
			return v.r;
		}();
		dtd_tail_->next_link = dtd_terminate;
	}

	dtd_tail_->token.ioc = 1;

	if (dir == ch9::Direction::DeviceToHost)
		cache_flush(buf(), len());
	else {
		assert(cache_aligned(buf(), len()) ||
		    cache_coherent_range(buf(), len()));
		cache_invalidate(buf(), len());
	}

	started();

	return 0;
}

void
fsl_usb2_transaction::transferred(size_t bytes, ch9::Direction dir)
{
	if (bytes && dir == ch9::Direction::HostToDevice)
		cache_invalidate(buf(), bytes);
}

void
fsl_usb2_transaction::retire(int status)
{
	free_dtds();
	next_ = nullptr;
	retired(status);
}

void
fsl_usb2_transaction::free_dtds()
{
	auto p = dtd_head_;
	if (!p)
		return;
	while (true) {
		const auto n = p->next_link;
		udc_->free_dtd(p);
		if (p == dtd_tail_)
			break;
		p = n;
	}
	dtd_head_ = dtd_tail_ = nullptr;
}

/*
 * isr
 */
int
isr(int vector, void *data)
{
	auto u = static_cast<fsl_usb2_udc *>(data);
	u->isr();
	return INT_DONE;
}

}

/*
 * fsl_usb2_udc_init
 */
void
fsl_usb2_udc_init(const fsl_usb2_udc_desc *d)
{
	auto u = new fsl_usb2_udc{d->name, reinterpret_cast<regs *>(d->base)};
	usb::gadget::udc::add(u);
	irq_attach(d->irq, d->ipl, 0, isr, nullptr, u);
}
