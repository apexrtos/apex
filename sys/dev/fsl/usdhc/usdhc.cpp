#include "init.h"

#include <arch.h>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <debug.h>
#include <dev/mmc/command.h>
#include <dev/mmc/host.h>
#include <dma.h>
#include <endian.h>
#include <event.h>
#include <irq.h>
#include <page.h>
#include <wait.h>

#define trace(...)

namespace {

/*
 * regs
 */
struct regs {
	union int_flags {
		struct {
			uint32_t CC : 1;
			uint32_t TC : 1;
			uint32_t BGE : 1;
			uint32_t DINT : 1;
			uint32_t BWR : 1;
			uint32_t BRR : 1;
			uint32_t CINS : 1;
			uint32_t CRM : 1;
			uint32_t CINT : 1;
			uint32_t : 3;
			uint32_t RTE : 1;
			uint32_t : 1;
			uint32_t TP : 1;
			uint32_t : 1;
			uint32_t CTOE : 1;
			uint32_t CCE : 1;
			uint32_t CEBE : 1;
			uint32_t CIE : 1;
			uint32_t DTOE : 1;
			uint32_t DCE : 1;
			uint32_t DEBE : 1;
			uint32_t : 1;
			uint32_t AC12E : 1;
			uint32_t : 1;
			uint32_t TNE : 1;
			uint32_t : 1;
			uint32_t DMAE : 1;
			uint32_t : 3;
		};
		uint32_t r;
	};
	uint32_t DS_ADDR;
	union blk_att {
		struct {
			uint32_t BLKSIZE : 13;
			uint32_t : 3;
			uint32_t BLKCNT : 16;
		};
		uint32_t r;
	} BLK_ATT;
	uint32_t CMD_ARG;
	union cmd_xfr_typ {
		struct {
			uint32_t : 16;
			uint32_t RSPTYP : 2;
			uint32_t : 1;
			uint32_t CCCEN : 1;
			uint32_t CICEN : 1;
			uint32_t DPSEL : 1;
			uint32_t CMDTYP : 2;
			uint32_t CMDINX : 6;
			uint32_t : 2;
		};
		uint32_t r;
	} CMD_XFR_TYP;
	uint32_t CMD_RSP0;
	uint32_t CMD_RSP1;
	uint32_t CMD_RSP2;
	uint32_t CMD_RSP3;
	uint32_t DATA_BUFF_ACC_PORT;
	union pres_state {
		struct {
			uint32_t CIHB : 1;
			uint32_t CDIHB : 1;
			uint32_t DLA : 1;
			uint32_t SDSTB : 1;
			uint32_t IPGOFF : 1;
			uint32_t HCKOFF : 1;
			uint32_t PEROFF : 1;
			uint32_t SDOFF : 1;
			uint32_t WTA : 1;
			uint32_t RTA : 1;
			uint32_t BWEN : 1;
			uint32_t BREN : 1;
			uint32_t RTR : 1;
			uint32_t : 2;
			uint32_t TSCD : 1;
			uint32_t CINST : 1;
			uint32_t : 1;
			uint32_t CDPL : 1;
			uint32_t WPSPL : 1;
			uint32_t : 3;
			uint32_t CLSL : 1;
			uint32_t DLSL : 8;
		};
		uint32_t r;
	} PRES_STATE;
	union prot_ctrl {
		struct {
			uint32_t LCTL : 1;
			uint32_t DTW : 2;
			uint32_t D3CD : 1;
			uint32_t EMODE : 2;
			uint32_t CDTL : 1;
			uint32_t CDSS : 1;
			uint32_t DMASEL : 2;
			uint32_t : 6;
			uint32_t SABGREQ : 1;
			uint32_t CREQ : 1;
			uint32_t RWCTL : 1;
			uint32_t IABG : 1;
			uint32_t RD_DONE_NO_8CLK : 1;
			uint32_t : 3;
			uint32_t WECINT : 1;
			uint32_t WECINS : 1;
			uint32_t WECRM : 1;
			uint32_t BURST_LEN_EN : 3;
			uint32_t NON_EXACT_BLK_RD : 1;
			uint32_t : 1;
		};
		uint32_t r;
	} PROT_CTRL;
	union sys_ctrl {
		struct {
			uint32_t : 4;
			uint32_t DVS : 4;
			uint32_t SDCLKFS : 8;
			uint32_t DTOCV : 4;
			uint32_t : 3;
			uint32_t IPP_RST_N : 1;
			uint32_t RSTA : 1;
			uint32_t RSTC : 1;
			uint32_t RSTD : 1;
			uint32_t INITA : 1;
			uint32_t RSTT : 1;
			uint32_t : 3;
		};
		uint32_t r;
	} SYS_CTRL;
	int_flags INT_STATUS;
	int_flags INT_STATUS_EN;
	int_flags INT_SIGNAL_EN;
	union autocmd12_err_status {
		struct {
			uint32_t AC12NE : 1;
			uint32_t AC12TOE : 1;
			uint32_t AC12EBE : 1;
			uint32_t AC12CE : 1;
			uint32_t AC12IE : 1;
			uint32_t : 2;
			uint32_t CNIBAC12E : 1;
			uint32_t : 14;
			uint32_t EXECUTE_TUNING : 1;
			uint32_t SMP_CLK_SEL : 1;
			uint32_t : 8;
		};
		uint32_t r;
	} AUTOCMD12_ERR_STATUS;
	union host_ctrl_cap {
		struct {
			uint32_t SDR50_SUPPORT : 1;
			uint32_t SDR104_SUPPORT : 1;
			uint32_t DDR50_SUPPORT : 1;
			uint32_t : 5;
			uint32_t TIME_COUNT_RETUNING : 4;
			uint32_t : 1;
			uint32_t USE_TUNING_SDR50 : 1;
			uint32_t RETUNING_MODE : 2;
			uint32_t MBL : 3;
			uint32_t : 1;
			uint32_t ADMAS : 1;
			uint32_t HSS : 1;
			uint32_t DMAS : 1;
			uint32_t SRS : 1;
			uint32_t VS33 : 1;
			uint32_t VS30 : 1;
			uint32_t VS18 : 1;
			uint32_t : 5;
		};
		uint32_t r;
	} HOST_CTRL_CAP;
	union wtmk_lvl {
		struct {
			uint32_t RD_WML : 8;
			uint32_t RD_BRST_LEN : 5;
			uint32_t : 3;
			uint32_t WR_WML : 8;
			uint32_t WR_BRST_LEN : 5;
			uint32_t : 3;
		};
		uint32_t r;
	} WTMK_LVL;
	union mix_ctrl {
		struct {
			uint32_t DMAEN : 1;
			uint32_t BCEN : 1;
			uint32_t AC12EN : 1;
			uint32_t DDR_EN : 1;
			uint32_t DTDSEL : 1;
			uint32_t MSBSEL : 1;
			uint32_t NIBBLE_POS : 1;
			uint32_t AC23EN : 1;
			uint32_t : 14;
			uint32_t EXE_TUNE : 1;
			uint32_t SMP_CLK_SEL : 1;
			uint32_t AUTO_TUNE_EN : 1;
			uint32_t FBCLK_SEL : 1;
			uint32_t : 6;
		};
		uint32_t r;
	} MIX_CTRL;
	uint32_t : 32;
	uint32_t FORCE_EVENT;
	uint32_t ADMA_ERR_STATUS;
	uint32_t ADMA_SYS_ADDR;
	uint32_t : 32;
	uint32_t DLL_CTRL;
	uint32_t DLL_STATUS;
	union clk_tune_ctrl_status {
		struct {
			uint32_t DLY_CELL_SET_POS : 4;
			uint32_t DLY_CELL_SET_OUT : 4;
			uint32_t DLY_CELL_SET_PRE : 7;
			uint32_t NXT_ERR : 1;
			uint32_t TAP_SEL_POS : 4;
			uint32_t TAP_SEL_OUT : 4;
			uint32_t TAP_SEL_PRE : 7;
			uint32_t PRE_ERR : 1;
		};
		uint32_t r;
	} CLK_TUNE_CTRL_STATUS;
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
	union vend_spec {
		struct {
			uint32_t : 1;
			uint32_t VSELECT : 1;
			uint32_t CONFLICT_CHK_EN : 1;
			uint32_t AC12_WR_CHKBUSY_EN : 1;
			uint32_t : 4;
			uint32_t FRC_SDCLK_ON : 1;
			uint32_t : 6;
			uint32_t CRC_CHK_DIS : 1;
			uint32_t : 15;
			uint32_t CMD_BYTE_EN : 1;
		};
		uint32_t r;
	} VEND_SPEC;
	uint32_t MMC_BOOT;
	union vend_spec2 {
		struct {
			uint32_t : 3;
			uint32_t CARD_INT_D3_TEST : 1;
			uint32_t TUNING_8BIT_EN : 1;
			uint32_t TUNING_1BIT_EN : 1;
			uint32_t TUNING_CMD_EN : 1;
			uint32_t : 1;
			uint32_t EN_BUSY_IRQ : 1;	/* UNDOCUMENTED */
			uint32_t : 3;
			uint32_t ACMD23_ARGU2_EN : 1;
			uint32_t PART_DLL_DEBUG : 1;
			uint32_t BUS_RST : 1;
			uint32_t : 17;
		};
		uint32_t r;
	} VEND_SPEC2;
	union tuning_ctrl {
		struct {
			uint32_t TUNING_START_TAP : 8;
			uint32_t TUNING_COUNTER : 8;
			uint32_t TUNING_STEP : 3;
			uint32_t : 1;
			uint32_t TUNING_WINDOW : 3;
			uint32_t : 1;
			uint32_t STD_TUNING_EN : 1;
			uint32_t : 7;
		};
		uint32_t r;
	} TUNING_CTRL;
};
static_assert(sizeof(regs) == 0xd0, "");
static_assert(BYTE_ORDER == LITTLE_ENDIAN, "");

/*
 * adma2_descriptor
 */
struct adma2_descriptor {
	uint16_t attr;
	uint16_t length;
	phys *address;
};
static_assert(sizeof(adma2_descriptor) == 8);
constexpr auto adma2_attr_tran = 0x20;
constexpr auto adma2_attr_valid = 0x1;
constexpr auto adma2_attr_end = 0x2;
constexpr auto adma2_attr_int = 0x4;
constexpr auto adma2_max_length = 65535u;
constexpr auto adma2_address_align = 4u;
constexpr auto adma2_length_align = 4u;

/*
 * calculate_dividers - find dividers to get card clock <= hz.
 *
 * Prefer larger values of sdclkfs as this is recommended in the manual.
 */
auto
calculate_dividers(const unsigned long clock, unsigned long hz,
    mmc::clock_mode mode)
{
	struct {
		unsigned sdclkfs;
		unsigned dvs;
		unsigned long actual;
	} r = {0, 0, 0};
	unsigned long error = ULONG_MAX;

	/* Card clock can't be greater than module clock. */
	if (hz > clock)
		hz = clock;

	const unsigned ddr_div = mode == mmc::clock_mode::ddr ? 2 : 1;
	const unsigned ideal = clock / (hz * ddr_div);

	for (unsigned sdclkfs = 1; sdclkfs <= 256; sdclkfs *= 2) {
		const unsigned dvs = std::min(div_ceil(ideal, sdclkfs), 16u);
		if (dvs == 0)
			break;
		const auto actual = clock / (sdclkfs * dvs * ddr_div);
		const auto e = hz - actual;
		if (e <= error) {
			error = e;
			r.sdclkfs = sdclkfs;
			r.dvs = dvs;
			r.actual = actual;
		}
	}

	/* Convert sdclkfs & dvs to register values. */
	r.sdclkfs = floor_log2(r.sdclkfs);
	if (r.sdclkfs)
		r.sdclkfs = 1u << (r.sdclkfs - 1);
	r.dvs -= 1;

	return r;
}

/*
 * fsl_usdhc
 */
class fsl_usdhc final : public mmc::host {
public:
	fsl_usdhc(const fsl_usdhc_desc &, const regs::host_ctrl_cap);

private:
	void v_reset() override;
	void v_disable_device_clock() override;
	unsigned long v_set_device_clock(unsigned long, mmc::clock_mode, bool)
	    override;
	void v_enable_device_clock() override;
	void v_auto_device_clock() override;
	void v_assert_hardware_reset() override;
	void v_release_hardware_reset() override;
	int v_run_command(mmc::command &) override;
	bool v_device_attached() override;
	bool v_device_busy() override;
	void v_set_bus_width(unsigned) override;
	void v_enable_tuning() override;
	bool v_require_tuning() override;
	int v_run_tuning(unsigned) override;
	void v_running_bus_test(bool) override;

	void reset_tuning();
	int do_tuning(unsigned);
	ssize_t setup_adma2_transfer(const mmc::command &, size_t);
	void finalise_adma2_transfer(const mmc::command &, size_t, size_t);

	event event_;
	regs::int_flags int_mask;
	bool tuning_;

	regs *const r_;
	const unsigned long clock_;
	adma2_descriptor *const dma_desc_;
	void *const bounce_;
	bool bus_test_;
	std::atomic_bool retuning_required_;

	static constexpr auto bounce_sz_ = 4096u;
	static constexpr auto dma_desc_sz_ = 16u;

	void isr();
	static int isr_wrapper(int, void *);
};

/*
 * fsl_usdhc::fsl_usdhc
 */
constexpr bool hs400_support = false;
constexpr bool hs400_es_support = false;
fsl_usdhc::fsl_usdhc(const fsl_usdhc_desc &d, const regs::host_ctrl_cap cap)
: mmc::host{d.mmc,
	    cap.SDR104_SUPPORT ? true : false,
	    cap.DDR50_SUPPORT ? true : false,
	    cap.SDR50_SUPPORT ? true : false,
	    hs400_es_support,
	    hs400_support,
	    cap.SDR104_SUPPORT ? true : false,
	    cap.DDR50_SUPPORT ? true : false,
	    cap.SDR50_SUPPORT ? true : false,
	    cap.USE_TUNING_SDR50 ? true : false,
	    512u << cap.MBL}
, tuning_{false}
, r_{reinterpret_cast<regs *>(d.base)}
, clock_{d.clock}
, dma_desc_{static_cast<adma2_descriptor *>(
    dma_alloc(sizeof(adma2_descriptor) * dma_desc_sz_))}
, bounce_{phys_to_virt(page_alloc(bounce_sz_, MA_NORMAL | MA_DMA, this))}
, bus_test_{false}
, retuning_required_{false}
{
	if (!dma_desc_ || !bounce_)
		panic("OOM");
	if (cap.RETUNING_MODE != 2)
		panic("Incompatible Hardware");

	/* Disable all interrupts until controller is reset. */
	write32(&r_->INT_SIGNAL_EN, 0);

	event_init(&event_, "usdhc", event::ev_IO);
	irq_attach(d.irq, d.ipl, 0, isr_wrapper, nullptr, this);

	dbg("FSL-USDHC initialised\n");
}

void
fsl_usdhc::v_reset()
{
	retuning_required_ = false;

	/* disable all interrupts */
	write32(&r_->INT_SIGNAL_EN, 0);

	/* issue controller reset */
	v_disable_device_clock();	    /* to prevent clock glitch */
	write32(&r_->VEND_SPEC2, [&]{
		auto v = read32(&r_->VEND_SPEC2);
		v.BUS_RST = 1;	    /* to avoid bus hang */
		return v.r;
	}());
	write32(&r_->SYS_CTRL, [&]{
		auto v = read32(&r_->SYS_CTRL);
		v.RSTA = 1;
		return v.r;
	}());
	write32(&r_->VEND_SPEC2, [&]{
		auto v = read32(&r_->VEND_SPEC2);
		v.BUS_RST = 0;
		return v.r;
	}());

	/* wait for reset to complete */
	while (read32(&r_->SYS_CTRL).RSTA)
		timer_delay(0);

	/* configure controller */
	write32(&r_->PROT_CTRL, [&]{
		auto v = read32(&r_->PROT_CTRL);
		v.BURST_LEN_EN = 3; /* enable burst length for all transfers */
		v.DMASEL = 2;	    /* ADMA2 mode */
		return v.r;
	}());
	write32(&r_->WTMK_LVL, [] {
		regs::wtmk_lvl v{};
		v.WR_BRST_LEN = 16;
		v.WR_WML = 64;
		v.RD_BRST_LEN = 16;
		v.RD_WML = 64;
		return v.r;
	}());
	write32(&r_->TUNING_CTRL, [&] {
		auto v = read32(&r_->TUNING_CTRL);
		v.STD_TUNING_EN = 0;
		return v.r;
	}());

	/* This bit is not documented in the i.MX RT1060 reference manual as at
	 * Rev. 2, 12/2019. Setting it makes the controller generate a transfer
	 * complete interrupt when the command inhibit (CDIHB) bit changes from
	 * 1 to 0 as manual suggests it should. */
	write32(&r_->VEND_SPEC2, [&]{
		auto v = read32(&r_->VEND_SPEC2);
		v.EN_BUSY_IRQ = 1;
		return v.r;
	}());

	/* Initialise interrupt mask. */
	int_mask.r = 0;
	int_mask.CC = 1;
	int_mask.TC = 1;
	int_mask.CINS = 1;
	int_mask.CRM = 1;
	int_mask.RTE = 1;
	int_mask.CTOE = 1;
	int_mask.CCE = 1;
	int_mask.CEBE = 1;
	int_mask.CIE = 1;
	int_mask.DTOE = 1;
	int_mask.DCE = 1;
	int_mask.DEBE = 1;
	int_mask.AC12E = 1;
	int_mask.TNE = 1;
	int_mask.DMAE = 1;

	/* configure interrupts */
	write32(&r_->INT_STATUS_EN, 0xffffffff);
	write32(&r_->INT_SIGNAL_EN, int_mask.r);
}

void
fsl_usdhc::v_disable_device_clock()
{
	/* USCHC automatically gates the clock. */
	write32(&r_->VEND_SPEC, [&]{
		auto v = read32(&r_->VEND_SPEC);
		v.FRC_SDCLK_ON = 0;
		return v.r;
	}());
}

/*
 * fsl_usdhc::v_set_device_clock
 */
unsigned long
fsl_usdhc::v_set_device_clock(unsigned long hz, mmc::clock_mode mode,
    bool enhanced_strobe)
{
	assert(!enhanced_strobe);

	const auto clk_forced = read32(&r_->VEND_SPEC).FRC_SDCLK_ON;

	/* Card clock must not be forced while adjusting dividers. */
	if (clk_forced)
		v_disable_device_clock();

	/* Internal clock must be stable before adjusting dividers. */
	while (!read32(&r_->PRES_STATE).SDSTB);

	/* Configure dividers. */
	const auto d = calculate_dividers(clock_, hz, mode);
	write32(&r_->SYS_CTRL, [&]{
		auto v = read32(&r_->SYS_CTRL);
		v.SDCLKFS = d.sdclkfs;
		v.DVS = d.dvs;
		return v.r;
	}());
	trace("fsl_usdhc::v_set_device_clock: desired %lu actual %lu\n",
	    hz, d.actual);

	/* Wait for internal clock to stabilise. */
	while (!read32(&r_->PRES_STATE).SDSTB);

	/* Configure DDR. */
	write32(&r_->MIX_CTRL, [&]{
		auto v = read32(&r_->MIX_CTRL);
		v.DDR_EN = mode == mmc::clock_mode::ddr;
		return v.r;
	}());

	/* Restore clock state. */
	if (clk_forced)
		v_enable_device_clock();

	return d.actual;
}

void
fsl_usdhc::v_enable_device_clock()
{
	/* Force clock output. */
	write32(&r_->VEND_SPEC, [&]{
		auto v = read32(&r_->VEND_SPEC);
		v.FRC_SDCLK_ON = 1;
		return v.r;
	}());
}

void
fsl_usdhc::v_auto_device_clock()
{
	/* Stop forcing clock output - clock is then automatically gated. */
	write32(&r_->VEND_SPEC, [&]{
		auto v = read32(&r_->VEND_SPEC);
		v.FRC_SDCLK_ON = 0;
		return v.r;
	}());
}

void
fsl_usdhc::v_assert_hardware_reset()
{
	write32(&r_->SYS_CTRL, [&]{
		auto v = read32(&r_->SYS_CTRL);
		v.IPP_RST_N = 0;
		return v.r;
	}());
}

void
fsl_usdhc::v_release_hardware_reset()
{
	write32(&r_->SYS_CTRL, [&]{
		auto v = read32(&r_->SYS_CTRL);
		v.IPP_RST_N = 1;
		return v.r;
	}());
}

ssize_t
fsl_usdhc::v_run_command(mmc::command &c)
{
	size_t dma_min;
	ssize_t len = 0;

	trace("fsl_usdhc::v_run_command %d arg %x\n", c.index(), c.argument());

	/* A previous command may have failed and issued a command and/or data
	 * reset. Wait for any previous reset to complete. */
	for (auto s = read32(&r_->SYS_CTRL); s.RSTD || s.RSTC;
	    s = read32(&r_->SYS_CTRL))
		timer_delay(0);

	auto pres_state = read32(&r_->PRES_STATE);

	/* If command inhibit is set a previous command failed and state was
	 * not properly reset. */
	if (pres_state.CIHB) {
		/* issue reset to recover from errors */
		write32(&r_->SYS_CTRL, [&]{
			auto v = read32(&r_->SYS_CTRL);
			v.RSTC = 1;
			v.RSTD = 1;
			return v.r;
		}());
		dbg("fsl_usdhc: reset command inhibit\n");
	}

	/* If this command requires free data lines and the previous command
	 * is still using data lines (most likely busy signalling) wait for
	 * device to finish the previous command before starting this one. */
	if (pres_state.CDIHB && c.uses_data_lines()) {
		const auto r = wait_event_timeout(event_, 1e9, [&] {
			return !read32(&r_->PRES_STATE).CDIHB;
		});
		if (r < 0) {
			/* issue reset to recover from errors */
			write32(&r_->SYS_CTRL, [&]{
				auto v = read32(&r_->SYS_CTRL);
				v.RSTC = 1;
				v.RSTD = 1;
				return v.r;
			}());
			dbg("fsl_usdhc: reset data inhibit\n");
		}
	}

	/* Wait for reset to complete. */
	for (auto s = read32(&r_->SYS_CTRL); s.RSTD || s.RSTC;
	    s = read32(&r_->SYS_CTRL))
		timer_delay(0);

	auto mix_ctrl = read32(&r_->MIX_CTRL);
	regs::cmd_xfr_typ cmd_xfr_typ{};

	mix_ctrl.DMAEN = 0;
	mix_ctrl.BCEN = 0;
	mix_ctrl.AC12EN = 0;
	mix_ctrl.DTDSEL = 0;
	mix_ctrl.MSBSEL = 0;
	mix_ctrl.AC23EN = 0;

	/* Configure DMA for data transfers. */
	if (c.data_direction() != mmc::command::data_direction::none) {
		/* Total transfer must be a whole number of blocks. */
		if (c.data_size() % c.transfer_block_size())
			return DERR(-EINVAL);

		if (c.data_size()) {
			/* Make sure that with the number of descriptors we
			 * have available we can always transfer at least one
			 * block. */
			dma_min = div_ceil(c.transfer_block_size(), dma_desc_sz_);

			/* Build DMA descriptor table. Due to our fixed number
			 * of transfer descriptors we may not be able to
			 * complete the entire transfer in one go. */
			if (len = setup_adma2_transfer(c, dma_min); len < 0)
				return len;

			/* Ensure writes to dma descriptor table are observable
			 * before starting dma engine. */
			write_memory_barrier();

			/* Set DMA descriptor table address. */
			write32(&r_->ADMA_SYS_ADDR, reinterpret_cast<uint32_t>(
			    virt_to_phys(dma_desc_)));
		}

		const auto blkcnt = len / c.transfer_block_size();

		/* Use auto-CMD23 functionality for multi block transfers. */
		/* REVISIT: support MMC reliable write? */
		if (c.index() == 18 || c.index() == 25) {
			mix_ctrl.MSBSEL = 1;
			mix_ctrl.BCEN = 1;
			mix_ctrl.AC23EN = 1;
			write32(&r_->DS_ADDR, blkcnt);
		}

		/* Set transfer block size & block count. */
		write32(&r_->BLK_ATT, [&]{
			regs::blk_att v{};
			v.BLKSIZE = c.transfer_block_size();
			v.BLKCNT = blkcnt;
			return v.r;
		}());

		/* Set data direction. */
		mix_ctrl.DTDSEL = c.data_direction() ==
		    mmc::command::data_direction::device_to_host;

		mix_ctrl.DMAEN = len > 0;
		cmd_xfr_typ.DPSEL = 1;
		int_mask.CC = 0;
	} else {
		cmd_xfr_typ.DPSEL = 0;
		int_mask.CC = 1;
	}

	cmd_xfr_typ.CMDINX = c.index();
	cmd_xfr_typ.CMDTYP = 0; /* normal command */
	cmd_xfr_typ.CICEN = c.response_contains_index();
	cmd_xfr_typ.CCCEN = c.response_crc_valid();
	switch (c.response_length()) {
	case 0:
		cmd_xfr_typ.RSPTYP = 0;
		break;
	case 48:
		cmd_xfr_typ.RSPTYP = c.busy_signalling() ? 3 : 2;
		break;
	case 136:
		cmd_xfr_typ.RSPTYP = 1;
		break;
	default:
		return DERR(-EINVAL);
	}

	write32(&r_->MIX_CTRL, mix_ctrl.r);
	write32(&r_->CMD_ARG, c.argument());

	/* Don't wait for command completion when running tuning as tuning uses
	 * the BRR interrupt. */
	if (tuning_) {
		write32(&r_->CMD_XFR_TYP, cmd_xfr_typ.r);
		return 0;
	}

	write32(&r_->INT_SIGNAL_EN, int_mask.r);

	/* Atomically start command & sleep on completion event. */
	const k_sigset_t sig_mask = sig_block_all();
	sch_prepare_sleep(&event_, 1e9);
	write32(&r_->CMD_XFR_TYP, cmd_xfr_typ.r);
	if (auto r = sch_continue_sleep(); r < 0) {
		dbg("fsl_usdhc::v_run_command %d arg %x failed %d\n",
		    c.index(), c.argument(), r);
		sig_restore(&sig_mask);
		/* Issue reset to recover from errors. */
		write32(&r_->SYS_CTRL, [&]{
			auto v = read32(&r_->SYS_CTRL);
			v.RSTC = 1;
			v.RSTD = 1;
			return v.r;
		}());
		/* Ignore I/O errors if bus test is running. */
		if (r != -EIO || !bus_test_)
			return r;
	}
	sig_restore(&sig_mask);

	/* Retrieve response data. */
	uint32_t tmp;
	switch (c.response_length()) {
	case 136:
		/* Shuffle up by 8 bits as the response registers don't contain
		 * a CRC but the device register descriptions do. */
		tmp = htobe32(read32(&r_->CMD_RSP3) << 8);
		memcpy(c.response().data() + 0, &tmp, 3);
		tmp = htobe32(read32(&r_->CMD_RSP2));
		memcpy(c.response().data() + 3, &tmp, 4);
		tmp = htobe32(read32(&r_->CMD_RSP1));
		memcpy(c.response().data() + 7, &tmp, 4);
		tmp = htobe32(read32(&r_->CMD_RSP0));
		memcpy(c.response().data() + 11, &tmp, 4);
		break;
	case 48:
		tmp = htobe32(read32(&r_->CMD_RSP0));
		memcpy(c.response().data(), &tmp, 4);
		break;
	}

	/* Finalise DMA transfer. */
	if (len)
		finalise_adma2_transfer(c, dma_min, len);

	return len;
}

/*
 * fsl_usdhc::v_device_attached
 */
bool
fsl_usdhc::v_device_attached()
{
	return read32(&r_->PRES_STATE).CINST;
}

/*
 * fsl_usdhc::v_device_busy
 */
bool
fsl_usdhc::v_device_busy()
{
	/* busy if data line 1 is low */
	return !(read32(&r_->PRES_STATE).DLSL & 1);
}

/*
 * fsl_usdhc::v_set_bus_width
 */
void
fsl_usdhc::v_set_bus_width(unsigned w)
{
	assert(w == 1 || w == 4 || w == 8);
	write32(&r_->PROT_CTRL, [&]{
		auto v = read32(&r_->PROT_CTRL);
		v.DTW = w / 4;
		return v.r;
	}());
	write32(&r_->VEND_SPEC2, [&]{
		auto v = read32(&r_->VEND_SPEC2);
		v.TUNING_CMD_EN = 1;
		v.TUNING_1BIT_EN = w == 1;
		v.TUNING_8BIT_EN = w == 8;
		return v.r;
	}());
}

/*
 * fsl_usdhc::v_enable_tuning
 */
void
fsl_usdhc::v_enable_tuning()
{
	write32(&r_->TUNING_CTRL, [&] {
		auto v = read32(&r_->TUNING_CTRL);
		v.STD_TUNING_EN = 1;	/* enable standard tuning procedure */
		v.TUNING_WINDOW = 4;
		v.TUNING_COUNTER = 60;	/* USDHC has 128 taps */
		v.TUNING_STEP = 2;
		v.TUNING_START_TAP = 10;
		return v.r;
	}());
	write32(&r_->MIX_CTRL, [&]{
		auto v = read32(&r_->MIX_CTRL);
		v.FBCLK_SEL = 1;	/* use pad for feedback clock */
		return v.r;
	}());
	reset_tuning();
}

/*
 * fsl_usdhc::v_require_tuning
 */
bool
fsl_usdhc::v_require_tuning()
{
	return retuning_required_;
}

/*
 * fsl_usdhc::v_run_tuning
 */
int
fsl_usdhc::v_run_tuning(const unsigned cmd_index)
{
	/* Attempt to tune from current point. */
	if (do_tuning(cmd_index) == 0) {
		retuning_required_ = false;
		return 0;
	}

	/* If that fails do a full tune. */
	reset_tuning();
	if (auto r = do_tuning(cmd_index); r < 0)
		return r;

	retuning_required_ = false;
	return 0;
}

/*
 * fsl_usdhc::v_running_bus_test
 */
void
fsl_usdhc::v_running_bus_test(bool v)
{
	bus_test_ = v;
}

/*
 * fsl_usdhc::reset_tuning
 */
void
fsl_usdhc::reset_tuning()
{
	write32(&r_->AUTOCMD12_ERR_STATUS, [&]{
		auto v = read32(&r_->AUTOCMD12_ERR_STATUS);
		v.SMP_CLK_SEL = 0;
		return v.r;
	}());
}

/*
 * fsl_usdhc::do_tuning
 */
int
fsl_usdhc::do_tuning(const unsigned cmd_index)
{
	const unsigned bus_width = read32(&r_->PROT_CTRL).DTW * 4;

	/* Start tuning procedure. */
	write32(&r_->AUTOCMD12_ERR_STATUS, [&]{
		auto v = read32(&r_->AUTOCMD12_ERR_STATUS);
		v.EXECUTE_TUNING = 1;
		return v.r;
	}());

	mmc::command c{cmd_index, 0, mmc::command::response_type::r1};
	c.setup_data_transfer(mmc::command::data_direction::device_to_host,
	    bus_width * 16, nullptr, 0, 0);

	tuning_ = true;

	/* Enable buffer read ready interrupt. */
	regs::int_flags brr_mask{};
	brr_mask.BRR = 1;
	write32(&r_->INT_SIGNAL_EN, brr_mask.r);

	/* Request tuning data until hardware is happy. */
	const k_sigset_t sig_mask = sig_block_all();
	do {
		sch_prepare_sleep(&event_, 10e6);
		v_run_command(c);
		if (auto r = sch_continue_sleep(); r < 0)
			dbg("fsl_usdhc::do_tuning: timeout!\n");
	} while(read32(&r_->AUTOCMD12_ERR_STATUS).EXECUTE_TUNING);
	sig_restore(&sig_mask);

	tuning_ = false;

	/* Restore interrupt mask. */
	write32(&r_->INT_SIGNAL_EN, int_mask.r);

	/* Tuning failed if we are now using fixed clock. */
	if (!read32(&r_->AUTOCMD12_ERR_STATUS).SMP_CLK_SEL)
		return DERR(-EIO);

	/* Enable automatic tuning. */
	write32(&r_->MIX_CTRL, [&]{
		auto v = read32(&r_->MIX_CTRL);
		v.AUTO_TUNE_EN = 1;
		return v.r;
	}());

	return 0;
}

/*
 * fsl_usdhc::setup_adma2_transfer
 */
ssize_t
fsl_usdhc::setup_adma2_transfer(const mmc::command &c, size_t dma_min)
{
	const adma2_descriptor *dma_end_ = dma_desc_ + dma_desc_sz_;
	adma2_descriptor *d = dma_desc_;

	/* Build ADMA2 descriptor table. */
	ssize_t len = dma_prepare(
	    c.data_direction() == mmc::command::data_direction::host_to_device,
	    c.iov(), c.iov_offset(), c.data_size(), dma_min, adma2_max_length,
	    adma2_length_align, adma2_address_align, bounce_, bounce_sz_,
	    [&](phys *p, size_t len) {
		d->address = p;
		d->length = len;
		d->attr = adma2_attr_tran | adma2_attr_valid;
		return ++d < dma_end_;
	});
	if (len < 0)
		return len;

	/* Because we have a limited number of transfer descriptors the
	 * transaction may not be an even number of blocks long. Truncate in
	 * this case. */
	if (len % c.transfer_block_size()) {
		len = len / c.transfer_block_size() * c.transfer_block_size();
		d = dma_desc_;
		for (size_t l = len; l; ++d) {
			if (d->length >= l)
				d->length = l;
			l -= d->length;
		}
	}

	/* Must be at least one non-zero length buffer. */
	if (d == dma_desc_)
		return DERR(-EINVAL);
	(d - 1)->attr |= adma2_attr_end;
	return len;
}

/*
 * fsl_usdhc::finalise_adma2_transfer
 */
void
fsl_usdhc::finalise_adma2_transfer(const mmc::command &c, size_t dma_min,
    size_t len)
{
	dma_finalise(
	    c.data_direction() == mmc::command::data_direction::host_to_device,
	    c.iov(), c.iov_offset(), c.data_size(), dma_min, adma2_max_length,
	    adma2_length_align, adma2_address_align, bounce_, bounce_sz_, len);
}

/*
 * fsl_usdhc::isr
 */
void
fsl_usdhc::isr()
{
	const auto v = read32(&r_->INT_STATUS);
	write32(&r_->INT_STATUS, v.r);

	if (v.RTE)
		retuning_required_ = true;
	if (v.CTOE || v.CCE || v.CEBE || v.CIE || v.DTOE || v.DCE ||
	    v.DEBE || v.DMAE || v.TNE || v.AC12E)
		sch_wakeup(&event_, -EIO);
	else if (v.CC || v.TC || v.BRR)
		sch_wakeup(&event_, 0);
	if (v.CINS || v.CRM)
		bus_changed_irq();
}

/*
 * fsl_usdhc::isr_wrapper
 */
int
fsl_usdhc::isr_wrapper(int vector, void *data)
{
	auto p = reinterpret_cast<fsl_usdhc *>(data);
	p->isr();
	return INT_DONE;
}

}

extern "C" void
fsl_usdhc_init(const fsl_usdhc_desc *d)
{
	auto cap = read32(&reinterpret_cast<regs *>(d->base)->HOST_CTRL_CAP);
	mmc::host::add(new fsl_usdhc(*d, cap));
}
