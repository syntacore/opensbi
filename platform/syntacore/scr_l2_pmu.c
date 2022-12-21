/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Syntacore
 *
 * SCR L2 cache performance counters
 *
 *
 */
#include "scr_l2_pmu.h"

#include <sbi/sbi_bitops.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_pmu.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi_utils/fdt/fdt_helper.h>

// #define SCR_L2_PMU_DEBUG 1

#ifdef SCR_L2_PMU_DEBUG
#define pr_l2_debug sbi_printf
#else
#define pr_l2_debug(...)
#endif

static ulong l2_cache_addr = -1;

#define SCR_L2_PMU_CONTROL_BASE		0x400
#define SCR_L2_PMU_CTR_NUM		4

#define SCR_L2_CTR_CONTROL		0x00
#define SCR_L2_CTR_LOW			0x08
#define SCR_L2_CTR_HIGH			0x0c

/** SCR L2 counters control registers */
#define SCR_PMU_BANKS_LOW_BIT		16
#define SCR_PMU_BANKS_HIGH_BIT		19
#define SCR_PMU_BANKS_SEL_MASK		GENMASK(SCR_PMU_BANKS_HIGH_BIT, SCR_PMU_BANKS_LOW_BIT)

#define SCR_L2_PMU_EVENT_SELECTOR	0
#define SCR_L2_PMU_EVENT_SELECTOR_MASK	GENMASK(3, 0)

/** SCR L2 event types */
enum scr_l2_event_types {
	SCR_L2_CACHE_HIT = 0,
	SCR_L2_CACHE_MISS,
	SCR_L2_CACHE_REFILL,
	SCR_L2_CACHE_EVICT_CLEAR,
	SCR_L2_CACHE_EVICT_DIRTY,
	SCR_L2_CACHE_EVICT_ROLLBACK,
	SCR_L2_CACHE_EVICT_COLLISION,
	SCR_L2_CACHE_EVICT_REQUEST,
	SCR_L2_CACHE_EVICT_SNOOP,
	SCR_L2_CACHE_MAX,
};

static uint32_t active_events[SCR_L2_PMU_CTR_NUM];

#define get_cidx_addr(x, offset, cidx) ((x) + SCR_L2_PMU_CONTROL_BASE + (offset) + ((cidx) << 4))

static inline void scr_l2_pmu_ctr_write_hw(uint32_t cidx, uint64_t ival)
{
	uint32_t low = 0, high = 0;
	void *addr_low = 0, *addr_high = 0;

	low = (uint32_t)(ival & 0xFFFFFFFF);
	high = (uint32_t)(ival >> 32);

	addr_low = (void *)get_cidx_addr(l2_cache_addr, SCR_L2_CTR_LOW, cidx);
	addr_high = (void *)get_cidx_addr(l2_cache_addr, SCR_L2_CTR_HIGH, cidx);

	writel(low, addr_low);
	writel(high, addr_high);
}

static inline void scr_l2_pmu_ctr_start_hw(uint32_t cidx, uint64_t ival, bool ival_update)
{
	if (ival_update)
		scr_l2_pmu_ctr_write_hw(cidx, ival);
}

static inline void scr_l2_pmu_ctr_write_event(uint32_t cidx, uint64_t ival)
{
	void *addr = (void *)get_cidx_addr(l2_cache_addr, 0x0, cidx);

	writel(ival, addr);
	pr_l2_debug("%s : addr=0x%p, ival=0x%lx\n", __func__, addr, ival);
}

static int scr_l2_pmu_reset_event(int ctr_idx)
{
	if (ctr_idx < 0 || ctr_idx >= SCR_L2_PMU_CTR_NUM)
		return SBI_EFAIL;

	scr_l2_pmu_ctr_write_event(ctr_idx, 0);

	return 0;
}

static uint32_t scr_l2_banks_num(uint64_t event_data)
{
	return (event_data & SCR_PMU_BANKS_SEL_MASK) >> SCR_PMU_BANKS_LOW_BIT;
}

int scr_l2_pmu_init(struct sbi_scratch *scratch, bool cold_boot)
{
	return 0;
}

void scr_l2_pmu_exit(struct sbi_scratch *scratch)
{
}

int scr_l2_pmu_read(uint32_t cidx, unsigned long *cval)
{
	unsigned long low = 0, high_prev = 0, high = 0;
	void *addr_low = 0, *addr_high = 0;

	addr_low  = (void *)get_cidx_addr(l2_cache_addr, SCR_L2_CTR_LOW, cidx);
	addr_high = (void *)get_cidx_addr(l2_cache_addr, SCR_L2_CTR_HIGH, cidx);

	high_prev = readl(addr_high);
	low = readl(addr_low);
	high = readl(addr_high);

	if (high != high_prev){
		low = readl(addr_low);
	}

	*cval = ((uint64_t)high << 32) | low;
	pr_l2_debug("%s cidx=0x%x, addr=0x%p, val=0x%lx\n", __func__, cidx, addr, *cval);

	return 0;
}

#define get_cidx_type(x) ((x & SBI_PMU_EVENT_IDX_TYPE_MASK) >> 16)
#define get_cidx_code(x) (x & SBI_PMU_EVENT_IDX_CODE_MASK)

static int pmu_ctr_validate(uint32_t cidx, uint32_t *event_idx_code)
{
	if (cidx >= SCR_L2_PMU_CTR_NUM || (active_events[cidx] == SBI_PMU_EVENT_IDX_INVALID))
		return SBI_EINVAL;

	if (active_events[cidx] >= SCR_L2_CACHE_MAX)
		return SBI_EINVAL;

	return active_events[cidx];
}

int scr_l2_pmu_stop(unsigned long cbase, unsigned long cmask,
		    unsigned long flag)
{
	int event_idx_type;
	uint32_t event_code;
	unsigned long ctr_mask = cmask << cbase;

	pr_l2_debug("%s cidx_base=0x%lx, cidx_mask=0x%lx, flag=0x%lx\n",
		   __func__, cbase, cmask, flag);

	if (sbi_fls(ctr_mask) >= SCR_L2_PMU_CTR_NUM)
		return SBI_EINVAL;

	for_each_set_bit_from(cbase, &ctr_mask, SCR_L2_PMU_CTR_NUM) {
		event_idx_type = pmu_ctr_validate(cbase, &event_code);
		if (event_idx_type < 0)
			/* Continue the stop operation for other counters */
			continue;

		if (flag & SBI_PMU_STOP_FLAG_RESET) {
			active_events[cbase] = SBI_PMU_EVENT_IDX_INVALID;
			scr_l2_pmu_reset_event(cbase);
		}
	}

	return 0;
}

int scr_l2_pmu_start(unsigned long cbase, unsigned long cmask,
		     unsigned long flags, unsigned long data1,
		     unsigned long data2)
{
	int event_idx_type;
	uint32_t event_code;
	unsigned long ctr_mask = cmask << cbase;
	int ret = SBI_EINVAL;
	bool bUpdate = FALSE;
	uint64_t init;

#if __riscv_xlen == 32
	init = ((uint64_t)data2 << 32) | data1;
#else
	init = data1;
#endif

	pr_l2_debug("%s cidx_base=0x%lx, cidx_mask=0x%lx, flags=0x%lx, init=0x%lx\n",
		   __func__, cbase, cmask, flags, init);

	if (sbi_fls(ctr_mask) >= SCR_L2_PMU_CTR_NUM)
		return ret;

	if (flags & SBI_PMU_START_FLAG_SET_INIT_VALUE)
		bUpdate = TRUE;

	for_each_set_bit_from(cbase, &ctr_mask, SCR_L2_PMU_CTR_NUM) {
		event_idx_type = pmu_ctr_validate(cbase, &event_code);
		if (event_idx_type < 0)
			/* Continue the start operation for other counters */
			continue;

		scr_l2_pmu_ctr_start_hw(cbase, init, bUpdate);
		ret = 0;
	}

	return ret;
}

int scr_l2_pmu_get_info(uint32_t cidx, unsigned long *ctr_info)
{
	pr_l2_debug("%s cidx=0x%x\n", __func__, cidx);
	return 0;
}

int scr_l2_pmu_cfg_match(unsigned long cidx_base, unsigned long cidx_mask,
			 unsigned long flags, unsigned long event_idx,
			 unsigned long data1, unsigned long data2)
{
	int ctr_idx = SBI_ENOTSUPP;
	unsigned int event_type = event_idx & SCR_L2_PMU_EVENT_SELECTOR_MASK;
	uint64_t pmu_event_val = 0;
	uint64_t event_data;
	unsigned long tmp = cidx_mask << cidx_base;
	int i;

#if __riscv_xlen == 32
	event_data = ((uint64_t)data2 << 32) | data1;
#else
	event_data = data1;
#endif

	pr_l2_debug("%s cidx_base=0x%lx, cidx_mask=0x%lx, flags=0x%lx, event_idx=0x%lx, event_data=0x%lx\n",
		   __func__, cidx_base, cidx_mask, flags, event_idx, event_data);

	/* Do a basic sanity check of counter base & mask */
	if (sbi_fls(tmp) >= SCR_L2_PMU_CTR_NUM || event_type >= SCR_L2_CACHE_MAX ||
		!scr_l2_banks_num(event_data))
		return SBI_EINVAL;

	if (flags & SBI_PMU_CFG_FLAG_SKIP_MATCH) {
		if (active_events[cidx_base] == SBI_PMU_EVENT_IDX_INVALID)
			return SBI_EINVAL;

		ctr_idx = cidx_base;
		goto skip_match;
	}

	if (event_idx > SCR_L2_CACHE_MAX)
		return SBI_ENOTSUPP;

	/* find free counter */
	for (i = 0; i < SCR_L2_PMU_CTR_NUM; i++) {
		if (active_events[i] == SBI_PMU_EVENT_IDX_INVALID)
			ctr_idx = i;
	}

	if (ctr_idx < 0)
		return SBI_ENOTSUPP;

	/*
	 * 31..20	RSV	RZ	0	Reserved
	 * 19..16	BANK	RW	0	Bank selector (one bit for each bank)
	 * 15..4	RSV	RZ	0	Reserved
	 * 3..0		PCE	RW	0	Performance counter event selector.
	 */
	/* set counters + banks */
	pmu_event_val = event_data | event_type;
	scr_l2_pmu_ctr_write_event(ctr_idx, pmu_event_val);
	active_events[ctr_idx] = event_idx;
skip_match:
	if (flags & SBI_PMU_CFG_FLAG_CLEAR_VALUE)
		scr_l2_pmu_ctr_write_hw(ctr_idx, 0);
	if (flags & SBI_PMU_CFG_FLAG_AUTO_START)
		scr_l2_pmu_ctr_start_hw(ctr_idx, 0, false);

	return ctr_idx;
}

unsigned long scr_l2_pmu_num_ctr(void)
{
	return SCR_L2_PMU_CTR_NUM;
};

static const struct fdt_match scr_l2_cache_match[] = {
	{ .compatible = "syntacore,l2-cache" },
	{ },
};

int scr_fdt_l2_pmu_init(void *fdt)
{
	int nodeoff = -1;
	const struct fdt_match *match;
	uint64_t reg_addr, reg_size;
	int rc;
	int i;

	nodeoff = fdt_find_match(fdt, 0, scr_l2_cache_match, &match);
	if (nodeoff < 0)
		return SBI_ENODEV;

	rc = fdt_get_node_addr_size(fdt, nodeoff, 0,
				    &reg_addr, &reg_size);
	if (rc < 0 || !reg_size)
		return SBI_ENODEV;

	l2_cache_addr = reg_addr;

	for (i = 0; i < SCR_L2_PMU_CTR_NUM; i++)
		active_events[i] = SBI_PMU_EVENT_IDX_INVALID;

	return 0;
};

int scr_pmu_ext_provider(long extid, long funcid,
			const struct sbi_trap_regs *regs, unsigned long *out_value,
			struct sbi_trap_info *out_trap)
{
	int ret = 0;

	/* sanity check */
	if (extid != SBI_EXT_SCR_L2_CACHE_PMU || l2_cache_addr == -1)
		return SBI_ENOTSUPP;

	switch (funcid) {
	case SBI_EXT_PMU_NUM_COUNTERS:
		ret = scr_l2_pmu_num_ctr();
		if (ret >= 0) {
			*out_value = ret;
			ret = 0;
		}
		break;
	/* may be unneeded */
	case SBI_EXT_PMU_COUNTER_GET_INFO:
		ret = scr_l2_pmu_get_info(regs->a0, out_value);
		break;
	/* may be unneeded */
	case SBI_EXT_PMU_COUNTER_CFG_MATCH:
		ret = scr_l2_pmu_cfg_match(regs->a0, regs->a1, regs->a2, regs->a3, regs->a4, regs->a5);
		if (ret >= 0) {
			*out_value = ret;
			ret = 0;
		}
		break;
	/* surely needed */
	case SBI_EXT_PMU_COUNTER_START:
		ret = scr_l2_pmu_start(regs->a0, regs->a1, regs->a2, regs->a3, regs->a4);
		break;
	/* surely needed */
	case SBI_EXT_PMU_COUNTER_STOP:
		ret = scr_l2_pmu_stop(regs->a0, regs->a1, regs->a2);
		break;
	case SBI_EXT_SCR_PMU_COUNTER_HW_READ:
		ret = scr_l2_pmu_read(regs->a0, out_value);
		break;
	default:
		sbi_printf("Unsupported vendor sbi call : %ld\n", funcid);
		asm volatile("ebreak");
	}

	return ret;
}
