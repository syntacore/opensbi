/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Syntacore
 *
 * Authors:
 *   Sergey Matyukevich <sergey.matyukevich@syntacore.com>
 *
 */

#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_ecall.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_version.h>
#include <sbi/sbi_dbtr.h>

static int sbi_ecall_dbtr_handler(unsigned long extid, unsigned long funcid,
				  const struct sbi_trap_regs *regs,
				  unsigned long *out_val,
				  struct sbi_trap_info *out_trap)
{
	unsigned long smode = (csr_read(CSR_MSTATUS) & MSTATUS_MPP) >>
			MSTATUS_MPP_SHIFT;
	const struct sbi_domain *dom = sbi_domain_thishart_ptr();
	int ret = 0;

	switch (funcid) {
	case SBI_EXT_DBTR_NUM_TRIGGERS:
		ret = sbi_dbtr_num_trig(regs->a0, out_val);
		break;
	case SBI_EXT_DBTR_TRIGGER_READ:
		ret = sbi_dbtr_read_trig(dom, smode, regs->a0, regs->a1, regs->a2);
		break;
	case SBI_EXT_DBTR_TRIGGER_INSTALL:
		ret = sbi_dbtr_install_trig(dom, smode, regs->a0, regs->a1, regs->a2, out_val);
		break;
	case SBI_EXT_DBTR_TRIGGER_UNINSTALL:
		ret = sbi_dbtr_uninstall_trig(regs->a0, regs->a1);
		break;
	case SBI_EXT_DBTR_TRIGGER_ENABLE:
		ret = sbi_dbtr_enable_trig(regs->a0, regs->a1);
		break;
	case SBI_EXT_DBTR_TRIGGER_UPDATE:
		ret = sbi_dbtr_update_trig(dom, smode, regs->a0, regs->a1, regs->a2);
		break;
	case SBI_EXT_DBTR_TRIGGER_DISABLE:
		ret = sbi_dbtr_disable_trig(regs->a0, regs->a1);
		break;
	default:
		ret = SBI_ENOTSUPP;
	};

	return ret;
}

static int sbi_ecall_dbtr_probe(unsigned long extid, unsigned long *out_val)
{
	return sbi_dbtr_probe(out_val);
}

struct sbi_ecall_extension ecall_dbtr = {
	.extid_start = SBI_EXT_DBTR,
	.extid_end = SBI_EXT_DBTR,
	.handle = sbi_ecall_dbtr_handler,
	.probe = sbi_ecall_dbtr_probe,
};
