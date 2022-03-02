/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Syntacore
 *
 * SCR L2 cache performance counters
 *
 */
#ifndef _SCR_L2_PMU_H_
#define _SCR_L2_PMU_H_

#include <sbi/sbi_trap.h>

#define SBI_EXT_SCR_L2_CACHE_PMU	0x09000001
#define SBI_EXT_SCR_PMU_COUNTER_HW_READ	0x6

int scr_fdt_l2_pmu_init(void *fdt);

int scr_pmu_ext_provider(long extid, long funcid,
			const struct sbi_trap_regs *regs, unsigned long *out_value,
			struct sbi_trap_info *out_trap);

#endif
