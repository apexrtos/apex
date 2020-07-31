#ifndef dev_nxp_imxrt_lpuart_regs_h
#define dev_nxp_imxrt_lpuart_regs_h

#include <assert.h>
#include <endian.h>
#include <stdint.h>

/*
 * Hardware register description for Freescale LPUART
 */
struct lpuart_regs {
	uint32_t VERID;
	union lpuart_param {
		struct {
			uint32_t TXFIFO : 8;
			uint32_t RXFIFO : 8;
			uint32_t : 16;
		};
		uint32_t r;
	} PARAM;
	union lpuart_global {
		struct {
			uint32_t : 1;
			uint32_t RST : 1;
			uint32_t : 30;
		};
		uint32_t r;
	} GLOBAL;
	uint32_t PINCFG;
	union lpuart_baud {
		struct {
			uint32_t SBR : 13;
			uint32_t SBNS : 1;
			uint32_t RXEDGIE : 1;
			uint32_t LBKDIE : 1;
			uint32_t RESYNCDIS : 1;
			uint32_t BOTHEDGE : 1;
			uint32_t MATCFG : 2;
			uint32_t : 1;
			uint32_t RDMAE : 1;
			uint32_t : 1;
			uint32_t TDMAE : 1;
			uint32_t OSR : 5;
			uint32_t M10 : 1;
			uint32_t MAEN2 : 1;
			uint32_t MAEN1 : 1;
		};
		uint32_t r;
	} BAUD;
	union lpuart_stat {
		struct {
			uint32_t : 14;
			uint32_t MA2F : 1;
			uint32_t MA1F : 1;
			uint32_t PF : 1;
			uint32_t FE : 1;
			uint32_t NF : 1;
			uint32_t OR : 1;
			uint32_t IDLE : 1;
			uint32_t RDRF : 1;
			uint32_t TC : 1;
			uint32_t TDRE : 1;
			uint32_t RAF : 1;
			uint32_t LBKDE : 1;
			uint32_t BRK13 : 1;
			uint32_t RWUID : 1;
			uint32_t RXINV : 1;
			uint32_t MSBF : 1;
			uint32_t RXEDGIF : 1;
			uint32_t LBKDIF : 1;
		};
		uint32_t r;
	} STAT;
	union lpuart_ctrl {
		struct {
			uint32_t PT : 1;
			uint32_t PE : 1;
			uint32_t ILT : 1;
			uint32_t WAKE : 1;
			uint32_t M : 1;
			uint32_t RSRC : 1;
			uint32_t DOZEEN : 1;
			uint32_t LOOPS : 1;
			uint32_t IDLECFG : 3;
			uint32_t M7 : 1;
			uint32_t : 2;
			uint32_t MA2IE : 1;
			uint32_t MA1IE : 1;
			uint32_t SBK : 1;
			uint32_t RWU : 1;
			uint32_t RE : 1;
			uint32_t TE : 1;
			uint32_t ILIE : 1;
			uint32_t RIE : 1;
			uint32_t TCIE : 1;
			uint32_t TIE : 1;
			uint32_t PEIE : 1;
			uint32_t FEIE : 1;
			uint32_t NEIE : 1;
			uint32_t ORIE : 1;
			uint32_t TXINV : 1;
			uint32_t TXDIR : 1;
			uint32_t R9T8 : 1;
			uint32_t R8T9 : 1;
		};
		uint32_t r;
	} CTRL;
	uint32_t DATA;
	uint32_t MATCH;
	uint32_t MODIR;
	union lpuart_fifo {
		struct {
			uint32_t RXFIFOSIZE : 3;
			uint32_t RXFE : 1;
			uint32_t TXFIFOSIZE : 3;
			uint32_t TXFE : 1;
			uint32_t RXUFE : 1;
			uint32_t TXOFE : 1;
			uint32_t RXIDEN : 3;
			uint32_t : 1;
			uint32_t RXFLUSH : 1;
			uint32_t TXFLUSH : 1;
			uint32_t RXUF : 1;
			uint32_t TXOF : 1;
			uint32_t : 4;
			uint32_t RXEMPT : 1;
			uint32_t TXEMPT : 1;
			uint32_t : 8;
		};
		uint32_t r;
	} FIFO;
	union lpuart_water {
		struct {
			uint32_t TXWATER : 8;
			uint32_t TXCOUNT : 8;
			uint32_t RXWATER : 8;
			uint32_t RXCOUNT : 8;
		};
		uint32_t r;
	} WATER;
};
static_assert(sizeof(struct lpuart_regs) == 0x30, "");
static_assert(BYTE_ORDER == LITTLE_ENDIAN, "");

#endif
