/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Syntacore
 *
 */

#include <libfdt.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_timer.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_console.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/timer/fdt_timer.h>

#define SCR_MTIMER_CONTROL_OFFSET	0x00
#define SCR_MTIMER_DIVIDER_OFFSET	0x04
#define SCR_MTIMER_TIME_OFFSET		0x08
#define SCR_MTIMER_TIME_SIZE		0x08
#define SCR_MTIMER_TIMECMP_OFFSET	0x10
#define SCR_MTIMER_TIMECMP_SIZE		0x08

struct scr_mtimer_data {
	/* PMP/MPU region information */
	unsigned long reg_base;
	unsigned long reg_size;

	/* Public details */
	unsigned long mtime_freq;
	unsigned long mtime_addr;
	unsigned long mtime_size;
	unsigned long mtimecmp_addr;
	unsigned long mtimecmp_size;
};

static struct scr_mtimer_data mt;

static u64 mtimer_time_rd64(void *addr)
{
#if __riscv_xlen == 32
	volatile u32 mtime_h;
	u32 mtime_l;

	do {
		mtime_h = readl_relaxed(addr + 4);
		mtime_l = readl_relaxed(addr);
	} while (mtime_h != readl_relaxed(addr + 4));

	return (((u64)mtime_h) << 32 | mtime_l);
#else
	return readq_relaxed(addr);
#endif
}

static void mtimer_time_wr64(u64 value, void *addr)
{
#if __riscv_xlen == 32
	writel_relaxed(0x0, addr);
	writel_relaxed((u32)(value >> 32), addr + 4);
	writel_relaxed((u32)(value & 0xffffffff), addr);
#else
	writeq_relaxed(value, addr);
#endif
}

static inline u64 scr_mtimer_value(void)
{
	/* Read MTIMER Time Value */
	return mtimer_time_rd64((void *)mt.mtime_addr);
}

static inline void scr_mtimer_event_start(u64 next_event)
{
	/* Program MTIMER Time Compare */
	mtimer_time_wr64(next_event, (void *)mt.mtimecmp_addr);
}

static inline void scr_mtimer_event_stop(void)
{
	/* Clear MTIMER Time Compare */
	mtimer_time_wr64(-1ULL, (void *)mt.mtimecmp_addr);
}

static struct sbi_timer_device scr_mtimer = {
	.name = "scr_mtimer",
	.timer_value = scr_mtimer_value,
	.timer_event_start = scr_mtimer_event_start,
	.timer_event_stop = scr_mtimer_event_stop
};

static int scr_warm_timer_init(void)
{
	return 0;
}

static int scr_div_init(const struct scr_mtimer_data *mt, unsigned long clock_freq)
{
	uint32_t mtimer_clock_div = clock_freq / mt->mtime_freq - 1;

	/** disable MTIMER, set Core Clock source */
	writel_relaxed(0, (void *)mt->reg_base + SCR_MTIMER_CONTROL_OFFSET);

	/* workaround for mtime != 0 on reset */
#if __riscv_xlen == 32
	writel(0, (void *)mt->mtime_addr);
	writel(0, (void *)mt->mtime_addr + 4);
#else
	writeq(0ULL, (void *)mt->mtime_addr);
#endif
	/** setup divider */
	writel(mtimer_clock_div, (void *)mt->reg_base + SCR_MTIMER_DIVIDER_OFFSET);

	/** enable MTIMER */
	writel_relaxed(1, (void *)mt->reg_base + SCR_MTIMER_CONTROL_OFFSET);

	return 0;
}

/** @todo move helper to fdt_helper.c */
static int fdt_parse_clock_frequency(void *fdt, int nodeoff, unsigned long *freq)
{
	const fdt32_t *clocks_map, *val;
	uint32_t clock_phandle;
	int clock_offset, len;

	clocks_map = fdt_getprop(fdt, nodeoff, "clocks", &len);
	if (!clocks_map || len < 4)
		return SBI_EFAIL;

	clock_phandle = fdt32_to_cpu(*clocks_map);
	clock_offset = fdt_node_offset_by_phandle(fdt, clock_phandle);
	if (clock_offset < 0)
		return clock_offset;

	val = fdt_getprop(fdt, clock_offset, "clock-frequency", &len);
	if (!val || len < 4)
		return SBI_EFAIL;

	*freq = (unsigned long)fdt32_to_cpu(*val);

	return 0;
}

static int scr_cold_timer_init(void *fdt, int nodeoff,
			 const struct fdt_match *match)
{
	int rc;
	uint64_t reg_addr, reg_size;
	unsigned long clock_freq;

	/** sanity check */
	if (nodeoff < 0 || !fdt)
		return SBI_EINVAL;

	rc = fdt_get_node_addr_size(fdt, nodeoff, 0,
				    &reg_addr, &reg_size);
	if (rc < 0 || !reg_size)
		return SBI_ENODEV;

	mt.reg_base = reg_addr;
	mt.reg_size = reg_size;
	mt.mtime_addr = reg_addr + SCR_MTIMER_TIME_OFFSET;
	mt.mtime_size = SCR_MTIMER_TIME_SIZE;
	mt.mtimecmp_addr = reg_addr + SCR_MTIMER_TIMECMP_OFFSET;
	mt.mtimecmp_size = SCR_MTIMER_TIMECMP_SIZE;

	rc = fdt_parse_timebase_frequency(fdt, &mt.mtime_freq);
	if (rc)
		return rc;

	rc = fdt_parse_clock_frequency(fdt, nodeoff, &clock_freq);
	if (rc)
		return rc;

	/** calc divider for freq */
	rc = scr_div_init(&mt, clock_freq);
	if (rc)
		return rc;

	scr_mtimer.timer_freq = mt.mtime_freq;
	sbi_timer_set_device(&scr_mtimer);

	return 0;
}

static const struct fdt_match scr_mtimer_match[] = {
	{ .compatible = "syntacore,mtimer" },
	{ },
};

struct fdt_timer fdt_scr_mtimer = {
	.match_table = scr_mtimer_match,
	.cold_init = scr_cold_timer_init,
	.warm_init = scr_warm_timer_init,
	.exit = NULL,
};

void scr_mtimer_print_info(void)
{
	u32 val;

	val = readl_relaxed((void *)mt.reg_base + SCR_MTIMER_CONTROL_OFFSET);
	sbi_printf("SCR_MTIMER_CONTROL : 0x%x\n", val);
	val = readl_relaxed((void *)mt.reg_base + SCR_MTIMER_DIVIDER_OFFSET);
	sbi_printf("SCR_MTIMER_DIVIDER : %u\n", val);
};
