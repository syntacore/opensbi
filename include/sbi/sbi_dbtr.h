/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Syntacore
 *
 * Authors:
 *   Sergey Matyukevich <sergey.matyukevich@syntacore.com>
 *
 */

#ifndef __SBI_DBTR_H__
#define __SBI_DBTR_H__

#include <sbi/riscv_endian.h>
#include <sbi/riscv_dbtr.h>

#include <sbi/sbi_hartmask.h>
#include <sbi/sbi_scratch.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_types.h>

/** Representation of trigger state */
union sbi_dbtr_trig_state {
	unsigned long value;
	struct {
		unsigned long mapped:1;
		unsigned long u:1;
		unsigned long s:1;
		unsigned long vu:1;
		unsigned long vs:1;
#if __riscv_xlen == 64
		unsigned long reserved:59;
#elif __riscv_xlen == 32
		unsigned long reserved:27;
#else
#error "Unexpected __riscv_xlen"
#endif
	};
};

struct sbi_dbtr_trig_info {
	unsigned long type_mask;
	union sbi_dbtr_trig_state state;
	unsigned long tdata1;
	unsigned long tdata2;
	unsigned long tdata3;
};

/** SBI shared mem messages layout */
struct sbi_dbtr_data_msg {
	unsigned long tstate;
	unsigned long tdata1;
	unsigned long tdata2;
	unsigned long tdata3;
};

struct sbi_dbtr_id_msg {
	unsigned long idx;
};

/** Initialize PMU */
int sbi_dbtr_init(struct sbi_scratch *scratch);

/** SBI DBTR extension functions */
int sbi_dbtr_probe(unsigned long *out);
int sbi_dbtr_num_trig(unsigned long trig_tdata1, unsigned long *out);
int sbi_dbtr_read_trig(const struct sbi_domain *dom, unsigned long smode,
		unsigned long trig_idx_base, unsigned long trig_count,
		unsigned long out_addr_div_by_16);
int sbi_dbtr_install_trig(const struct sbi_domain *dom, unsigned long smode,
		unsigned long trig_count, unsigned long in_addr_div_by_16,
		unsigned long out_addr_div_by_16, unsigned long *out);
int sbi_dbtr_uninstall_trig(unsigned long trig_idx_base, unsigned long trig_idx_mask);
int sbi_dbtr_enable_trig(unsigned long trig_idx_base, unsigned long trig_idx_mask);
int sbi_dbtr_update_trig(const struct sbi_domain *dom, unsigned long smode,
		unsigned long trig_count, unsigned long in_addr_div_by_16,
		unsigned long out_addr_div_by_16);
int sbi_dbtr_disable_trig(unsigned long trig_idx_base, unsigned long trig_idx_mask);

#endif
