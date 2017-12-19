/*	$OpenBSD: est.c,v 1.11 2005/03/07 06:59:14 mbalmer Exp $ */
/*
 * Copyright (c) 2003 Michael Eriksson.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * This is a driver for Intel's Enhanced SpeedStep, as implemented in
 * Pentium M processors.
 *
 * Reference documentation:
 *
 * - IA-32 Intel Architecture Software Developer's Manual, Volume 3:
 *   System Programming Guide.
 *   Section 13.14, Enhanced Intel SpeedStep technology.
 *   Table B-2, MSRs in Pentium M Processors.
 *   http://www.intel.com/design/pentium4/manuals/245472.htm
 *
 * - Intel Pentium M Processor Datasheet.
 *   Table 5, Voltage and Current Specifications.
 *   http://www.intel.com/design/mobile/datashts/252612.htm
 *
 * - Intel Pentium M Processor on 90 nm Process with 2-MB L2 Cache Datasheet
 *   Table 3-4, Voltage and Current Specifications.
 *   http://www.intel.com/design/mobile/datashts/302189.htm
 *
 * - Linux cpufreq patches, speedstep-centrino.c.
 *   Encoding of MSR_PERF_CTL and MSR_PERF_STATUS.
 *   http://www.codemonkey.org.uk/projects/cpufreq/cpufreq-2.4.22-pre6-1.gz
 */

/*
 * cpu.c - Processor driver for Intel CPU
 */

#include <driver.h>
#include <sys/ioctl.h>
#include <cpufunc.h>

/* #define DEBUG_CPU 1 */

#ifdef DEBUG
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

/* Status/control registers (from the IA-32 System Programming Guide). */
#define MSR_PERF_STATUS		0x198
#define MSR_PERF_CTL		0x199

/* Register and bit for enabling SpeedStep. */
#define MSR_MISC_ENABLE		0x1a0
#define MSR_SS_ENABLE		(1<<16)

static int cpu_ioctl(device_t dev, u_long cmd, void *arg);
static int cpu_init(void);

/*
 * Driver structure
 */
struct driver cpu_drv = {
	/* name */	"Processor",
	/* order */	1,
	/* init */	cpu_init,
};

static struct devio cpu_io = {
	/* open */	NULL,
	/* close */	NULL,
	/* read */	NULL,
	/* write */	NULL,
	/* ioctl */	cpu_ioctl,
	/* event */	NULL,
};

static device_t cpu_dev;		/* Device object */

/*
 * Frequency tables
 */
struct fq_info {
	u_short mhz;
	u_short mv;
};

/* Ultra Low Voltage Intel Pentium M processor 900 MHz */
static const struct fq_info pentium_m_900[] = {
	{  900, 1004 },
	{  800,  988 },
	{  600,  844 },
};

/* Ultra Low Voltage Intel Pentium M processor 1.00 GHz */
static const struct fq_info pentium_m_1000[] = {
	{ 1000, 1004 },
	{  900,  988 },
	{  800,  972 },
	{  600,  844 },
};

/* Low Voltage Intel Pentium M processor 1.10 GHz */
static const struct fq_info pentium_m_1100[] = {
	{ 1100, 1180 },
	{ 1000, 1164 },
	{  900, 1100 },
	{  800, 1020 },
	{  600,  956 },
};

/* Low Voltage Intel Pentium M processor 1.20 GHz */
static const struct fq_info pentium_m_1200[] = {
	{ 1200, 1180 },
	{ 1100, 1164 },
	{ 1000, 1100 },
	{  900, 1020 },
	{  800, 1004 },
	{  600,  956 },
};

/* Intel Pentium M processor 1.30 GHz */
static const struct fq_info pentium_m_1300[] = {
	{ 1300, 1388 },
	{ 1200, 1356 },
	{ 1000, 1292 },
	{  800, 1260 },
	{  600,  956 },
};

/* Intel Pentium M processor 1.40 GHz */
static const struct fq_info pentium_m_1400[] = {
	{ 1400, 1484 },
	{ 1200, 1436 },
	{ 1000, 1308 },
	{  800, 1180 },
	{  600,  956 }
};

/* Intel Pentium M processor 1.50 GHz */
static const struct fq_info pentium_m_1500[] = {
	{ 1500, 1484 },
	{ 1400, 1452 },
	{ 1200, 1356 },
	{ 1000, 1228 },
	{  800, 1116 },
	{  600,  956 }
};

/* Intel Pentium M processor 1.60 GHz */
static const struct fq_info pentium_m_1600[] = {
	{ 1600, 1484 },
	{ 1400, 1420 },
	{ 1200, 1276 },
	{ 1000, 1164 },
	{  800, 1036 },
	{  600,  956 }
};

/* Intel Pentium M processor 1.70 GHz */
static const struct fq_info pentium_m_1700[] = {
	{ 1700, 1484 },
	{ 1400, 1308 },
	{ 1200, 1228 },
	{ 1000, 1116 },
	{  800, 1004 },
	{  600,  956 }
};


/* Intel Pentium M processor 723 1.0 GHz */
static const struct fq_info pentium_m_n723[] = {
	{ 1000,  940 },
	{  900,  908 },
	{  800,  876 },
	{  600,  812 }
};

/* Intel Pentium M processor 733 1.1 GHz */
static const struct fq_info pentium_m_n733[] = {
	{ 1100,  940 },
	{ 1000,  924 },
	{  900,  892 },
	{  800,  876 },
	{  600,  812 }
};

/* Intel Pentium M processor 753 1.2 GHz */
static const struct fq_info pentium_m_n753[] = {
	{ 1200,  940 },
	{ 1100,  924 },
	{ 1000,  908 },
	{  900,  876 },
	{  800,  860 },
	{  600,  812 }
};

/* Intel Pentium M processor 738 1.4 GHz */
static const struct fq_info pentium_m_n738[] = {
	{ 1400, 1116 },
	{ 1300, 1116 },
	{ 1200, 1100 },
	{ 1100, 1068 },
	{ 1000, 1052 },
	{  900, 1036 },
	{  800, 1020 },
	{  600,  988 }
};

#if 0
/* Intel Pentium M processor 758 1.5 GHz */
static const struct fq_info pentium_m_n758[] = {
	{ 1500, 1116 },
	{ 1400, 1116 },
	{ 1300, 1100 },
	{ 1200, 1084 },
	{ 1100, 1068 },
	{ 1000, 1052 },
	{  900, 1036 },
	{  800, 1020 },
	{  600,  988 }
};
#endif

/* Intel Pentium M processor 715 1.5 GHz */
static const struct fq_info pentium_m_n715[] = {
	{ 1500, 1340 },
	{ 1200, 1228 },
	{ 1000, 1148 },
	{  800, 1068 },
	{  600,  988 }
};

/* Intel Pentium M processor 725 1.6 GHz */
static const struct fq_info pentium_m_n725[] = {
	{ 1600, 1340 },
	{ 1400, 1276 },
	{ 1200, 1212 },
	{ 1000, 1132 },
	{  800, 1068 },
	{  600,  988 }
};

/* Intel Pentium M processor 735 1.7 GHz */
static const struct fq_info pentium_m_n735[] = {
	{ 1700, 1340 },
	{ 1400, 1244 },
	{ 1200, 1180 },
	{ 1000, 1116 },
	{  800, 1052 },
	{  600,  988 }
};

/* Intel Pentium M processor 745 1.8 GHz */
static const struct fq_info pentium_m_n745[] = {
	{ 1800, 1340 },
	{ 1600, 1292 },
	{ 1400, 1228 },
	{ 1200, 1164 },
	{ 1000, 1116 },
	{  800, 1052 },
	{  600,  988 }
};

/* Intel Pentium M processor 755 2.0 GHz */
static const struct fq_info pentium_m_n755[] = {
	{ 2000, 1340 },
	{ 1800, 1292 },
	{ 1600, 1244 },
	{ 1400, 1196 },
	{ 1200, 1148 },
	{ 1000, 1100 },
	{  800, 1052 },
	{  600,  988 }
};

/* Intel Pentium M processor 765 2.1 GHz */
static const struct fq_info pentium_m_n765[] = {
	{ 2100, 1340 },
	{ 1800, 1276 },
	{ 1600, 1228 },
	{ 1400, 1180 },
	{ 1200, 1132 },
	{ 1000, 1084 },
	{  800, 1036 },
	{  600,  988 }
};

struct fqlist {
	const char *brand_tag;
	const struct fq_info *table;
	int n;
};

static const struct fqlist pentium_m[] = {
#define ENTRY(s, v)	{ s, v, sizeof(v) / sizeof((v)[0]) }
	ENTRY(" 900", pentium_m_900),
	ENTRY("1000", pentium_m_1000),
	ENTRY("1100", pentium_m_1100),
	ENTRY("1200", pentium_m_1200),
	ENTRY("1300", pentium_m_1300),
	ENTRY("1400", pentium_m_1400),
	ENTRY("1500", pentium_m_1500),
	ENTRY("1600", pentium_m_1600),
	ENTRY("1700", pentium_m_1700),
#undef ENTRY
};

static const struct fqlist pentium_m_dothan[] = {
#define ENTRY(s, v)	{ s, v, sizeof(v) / sizeof((v)[0]) }
	ENTRY("1.00", pentium_m_n723),
	ENTRY("1.10", pentium_m_n733),
	ENTRY("1.20", pentium_m_n753),
	ENTRY("1.40", pentium_m_n738),
#if 0
	ENTRY("1.50", pentium_m_n758),
#endif
	ENTRY("1.50", pentium_m_n715),
	ENTRY("1.60", pentium_m_n725),
	ENTRY("1.70", pentium_m_n735),
	ENTRY("1.80", pentium_m_n745),
	ENTRY("2.00", pentium_m_n755),
	ENTRY("2.10", pentium_m_n765),
#undef ENTRY
};

struct est_cpu {
	const char *brand_prefix;
	const char *brand_suffix;
	const struct fqlist *list;
	int n;
};

static const struct est_cpu est_cpus[] = {
	{
		"Intel(R) Pentium(R) M processor ", "MHz",
		pentium_m,
		(sizeof(pentium_m) / sizeof(pentium_m[0]))
	},
	{
		"Intel(R) Pentium(R) M processor ", "GHz",
		pentium_m_dothan,
		(sizeof(pentium_m_dothan) / sizeof(pentium_m_dothan[0]))
	},
};

#define NESTCPUS	  (int)(sizeof(est_cpus) / sizeof(est_cpus[0]))


#define MSRVALUE(mhz, mv)	((((mhz) / 100) << 8) | (((mv) - 700) / 16))
#define MSR2MHZ(msr)		((((u_int) (msr) >> 8) & 0xff) * 100)
#define MSR2MV(msr)		(((int) (msr) & 0xff) * 16 + 700)

static const struct fqlist *est_fqlist;

static struct cpu_info cpu_info;
static struct cpu_stat cpu_stat;
#ifdef CONFIG_DVS_EMULATION
static int bochs;
#endif

/*
 * Set CPU performance
 *
 * @level: percent of cpu speed
 */
int
cpu_setperf(int level)
{
	int i, fq;
	u_int msr_lo, msr_hi;
	int max_mhz;

	ASSERT(cpu_info.clock_ctrl);

	max_mhz = est_fqlist->table[0].mhz;
	fq = max_mhz * level / 100;

	for (i = est_fqlist->n - 1; i > 0; i--)
		if (est_fqlist->table[i].mhz >= fq)
			break;

	if (est_fqlist->table[i].mhz == cpu_stat.speed)
		return 0;

	cpu_stat.speed = est_fqlist->table[i].mhz;
	cpu_stat.power = est_fqlist->table[i].mv;
#ifdef DEBUG_CPU
	DPRINTF(("setperf: %dMHz %dmV\n", cpu_stat.speed, cpu_stat.power));
#endif
#ifdef CONFIG_DVS_EMULATION
	if (bochs)
		return 0;
#endif
	rdmsr(MSR_PERF_CTL, &msr_lo, &msr_hi);
	msr_lo = (msr_lo & ~0xffff) |
		MSRVALUE(est_fqlist->table[i].mhz, est_fqlist->table[i].mv);
	wrmsr(MSR_PERF_CTL, msr_lo, msr_hi);
	return 0;
}

/*
 * Get CPU performance
 */
int
cpu_getperf(void)
{
	int max_mhz;
	int level;

	ASSERT(cpu_info.clock_ctrl);

	max_mhz = est_fqlist->table[0].mhz;
	ASSERT(max_mhz);
	level = cpu_stat.speed * 100 / max_mhz;
	return level;
}

/*
 * Initialize CPU performance
 * Return false on error.
 */
int
cpu_initperf(void)
{
	int i, j, n, mhz, mv;
	const struct est_cpu *cpu;
	u_int msr_lo, msr_hi;
	char *tag, *brand_str;
	const struct fqlist *fql;

	if (!cpu_info.clock_ctrl)
		return -1;

#ifdef CONFIG_DVS_EMULATION
	if (bochs) {
		msr_lo = 0x1031;
		cpu = &est_cpus[0];
		est_fqlist = &cpu->list[7];
	} else
		rdmsr(MSR_PERF_STATUS, &msr_lo, &msr_hi);
#else
	rdmsr(MSR_PERF_STATUS, &msr_lo, &msr_hi);
#endif

	mhz = MSR2MHZ(msr_lo);
	mv = MSR2MV(msr_lo);
	DPRINTF(("Enhanced SpeedStep %d MHz (%d mV)\n", mhz, mv));

#ifdef CONFIG_DVS_EMULATION
	if (!bochs) {
#endif
	/*
	 * Look for a CPU matching brand_str.
	 */
	brand_str = cpu_info.name;
	for (i = 0; est_fqlist == NULL && i < NESTCPUS; i++) {
		cpu = &est_cpus[i];
		n = strnlen(cpu->brand_prefix, 48);
		if (strncmp(cpu->brand_prefix, brand_str, n) != 0)
			continue;
		tag = brand_str + n;
		for (j = 0; j < cpu->n; j++) {
			fql = &cpu->list[j];
			n = strnlen(fql->brand_tag, 48);
			if (!strncmp(fql->brand_tag, tag, n) &&
			    !strncmp(cpu->brand_suffix, tag + n, 48)) {
				est_fqlist = fql;
				break;
			}
		}
	}
	if (est_fqlist == NULL) {
		DPRINTF(("Unknown EST cpu, no changes possible\n"));
		cpu_info.clock_ctrl = 0;
		return -1;
	}

	/*
	 * Check that the current operating point is in our list.
	 */
	for (i = est_fqlist->n - 1; i >= 0; i--)
		if (est_fqlist->table[i].mhz == mhz)
			break;
	if (i < 0) {
		DPRINTF((" (not in table)\n"));
		cpu_info.clock_ctrl = 0;
		return -1;
	}
#ifdef CONFIG_DVS_EMULATION
	}
#endif
	/*
	 * Store current state
	 */
	cpu_info.speed = est_fqlist->table[0].mhz;
	cpu_info.power = est_fqlist->table[0].mv;
	cpu_stat.speed = mhz;
	cpu_stat.power = mv;

	/*
	 * OK, tell the user the available frequencies.
	 */
#ifdef DEBUG
	printf("Speeds: ");
	for (i = 0; i < est_fqlist->n; i++)
		printf("%d%s", est_fqlist->table[i].mhz,
		       i < est_fqlist->n - 1 ? ", " : " MHz\n");
#endif
	return 0;
}

static int
cpu_ioctl(device_t dev, u_long cmd, void *arg)
{

	switch (cmd) {
	case CPUIOC_GET_INFO:
		if (umem_copyout(&cpu_info, arg, sizeof(cpu_info)))
			return EFAULT;
		break;
	case CPUIOC_GET_STAT:
		if (umem_copyout(&cpu_stat, arg, sizeof(cpu_stat)))
			return EFAULT;
		break;
	default:
		return EINVAL;
	}
	return 0;
}

/*
 * Initialize CPU addon feature
 *
 * FIXME: i486 does not support cpuid instruction
 */
static int
cpu_init(void)
{
	u_int regs[4];
	char brand_str[49];
	char *p, *q;

	/* Create device object */
	cpu_dev = device_create(&cpu_io, "cpu", DF_CHR);
	ASSERT(cpu_dev);

#ifdef CONFIG_DVS_EMULATION
	bochs = 0;
	if (inb(0xe9) == 0xe9) {
		/*
		 * Detect Bochs. Fake the cpuid value.
		 */
		bochs = 1;
		cpu_info.id = 0x6d6;
		cpu_info.clock_ctrl = 1;
		strncpy(cpu_info.name,
			"Intel(R) Pentium(R) M processor 1600MHz", 50);

		DPRINTF(("CPU ID: %08x\n", cpu_info.id));
		DPRINTF(("CPU brand: %s\n", cpu_info.name));
		return 0;
	}
#endif
	/*
	 * Check enhanced speed step capability
	 */
	cpuid(1, regs);
	cpu_info.id = regs[0];
	DPRINTF(("CPU ID: %08x\n", regs[0]));

	if ((regs[2] & 0x80) == 0) {
		DPRINTF(("cpu: Clock control not supported\n"));
		cpu_info.clock_ctrl = 0;
		return 0;
	}
	cpu_info.clock_ctrl = 1;

	/*
	 * Get CPU brand string
	 */
	cpuid(0x80000002, regs);
	memcpy(brand_str, regs, sizeof(regs));
	cpuid(0x80000003, regs);
	memcpy(brand_str + 16, regs, sizeof(regs));
	cpuid(0x80000004, regs);
	memcpy(brand_str + 32, regs, sizeof(regs));

	/* Store string with lef-align */
	q = &cpu_info.name[0];
	p = brand_str;
	while (*p == ' ')
		p++;
	while (*p)
		*q++ = *p++;
	*q = '\0';

	DPRINTF(("CPU brand: %s\n", cpu_info.name));
	return 0;
}
