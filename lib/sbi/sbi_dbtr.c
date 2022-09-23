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
#include <sbi/sbi_csr_detect.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_dbtr.h>

#include <sbi/riscv_encoding.h>
#include <sbi/riscv_asm.h>

static struct sbi_dbtr_trig_info triggers[SBI_HARTMASK_MAX_BITS][RV_MAX_TRIGGERS] = {0};
static uint32_t total_trigs;

static void sbi_triggers_table_init(u32 hartid, int idx, unsigned long type_mask)
{
	triggers[hartid][idx].type_mask = type_mask;
	triggers[hartid][idx].state.value = 0;
	triggers[hartid][idx].tdata1 = 0;
	triggers[hartid][idx].tdata2 = 0;
	triggers[hartid][idx].tdata3 = 0;
}

int sbi_dbtr_init(struct sbi_scratch *scratch)
{
	struct sbi_trap_info trap = {0};
	u32 hartid = current_hartid();
	union riscv_dbtr_tdata1 tdata1;
	unsigned long val;
	int i;

	total_trigs = 0;

	for (i = 0; i < RV_MAX_TRIGGERS; i++) {
		csr_write_allowed(CSR_TSELECT, (ulong)&trap, i);
		if (trap.cause)
			break;

		val = csr_read_allowed(CSR_TSELECT, (ulong)&trap);
		if (trap.cause)
			break;

		/* Read back tselect and check that it contains the written value */
		if (val != i)
			break;

		val = csr_read_allowed(CSR_TINFO, (ulong)&trap);
		if (trap.cause) {
			/**
			 * If reading tinfo caused an exception, the debugger
			 * must read tdata1 to discover the type.
			 */
			tdata1.value = csr_read_allowed(CSR_TDATA1, (ulong)&trap);
			if (trap.cause)
				break;

			if (tdata1.type == 0)
				break;


			sbi_triggers_table_init(hartid, i, BIT(tdata1.type));
			total_trigs++;
		} else {
			if (val == 1)
				break;

			sbi_triggers_table_init(hartid, i, val);
			total_trigs++;
		}
	}

	return 0;
}

int sbi_dbtr_probe(unsigned long *out)
{
	*out  = total_trigs;

	return 0;
}


static void dbtr_trigger_init(unsigned int hartid, unsigned int idx,
			      struct sbi_dbtr_data_msg *recv)
{
	union riscv_dbtr_tdata1 tdata1;

	triggers[hartid][idx].tdata1 = lle_to_cpu(recv->tdata1);
	triggers[hartid][idx].tdata2 = lle_to_cpu(recv->tdata2);
	triggers[hartid][idx].tdata3 = lle_to_cpu(recv->tdata3);
	triggers[hartid][idx].state.mapped = 1;

	tdata1.value = lle_to_cpu(recv->tdata1);

	switch (tdata1.type) {
	case RISCV_DBTR_TRIG_MCONTROL:
		triggers[hartid][idx].state.u = __test_bit(MCONTROL_U_OFFSET, &tdata1.value);
		triggers[hartid][idx].state.s = __test_bit(MCONTROL_S_OFFSET, &tdata1.value);
		triggers[hartid][idx].state.vu = 0;
		triggers[hartid][idx].state.vs = 0;
		break;
	case RISCV_DBTR_TRIG_MCONTROL6:
		triggers[hartid][idx].state.u = __test_bit(MCONTROL6_U_OFFSET, &tdata1.value);
		triggers[hartid][idx].state.s = __test_bit(MCONTROL6_S_OFFSET, &tdata1.value);
		triggers[hartid][idx].state.vu = __test_bit(MCONTROL6_VU_OFFSET, &tdata1.value);
		triggers[hartid][idx].state.vs = __test_bit(MCONTROL6_VS_OFFSET, &tdata1.value);
		break;
	default:
		break;
	}
}

static inline void update_bit(unsigned long new, int nr, volatile unsigned long *addr)
{
	if (new)
		__set_bit(nr, addr);
	else
		__clear_bit(nr, addr);
}

static void dbtr_trigger_enable(unsigned int hartid, unsigned int idx)
{
	union sbi_dbtr_trig_state state;
	union riscv_dbtr_tdata1 tdata1;

	if (!triggers[hartid][idx].state.mapped)
		return;

	state.value = triggers[hartid][idx].state.value;
	tdata1.value = triggers[hartid][idx].tdata1;

	switch (tdata1.type) {
	case RISCV_DBTR_TRIG_MCONTROL:
		update_bit(state.u, MCONTROL_U_OFFSET, &triggers[hartid][idx].tdata1);
		update_bit(state.s, MCONTROL_S_OFFSET, &triggers[hartid][idx].tdata1);
		break;
	case RISCV_DBTR_TRIG_MCONTROL6:
		update_bit(state.vu, MCONTROL6_VU_OFFSET, &triggers[hartid][idx].tdata1);
		update_bit(state.vs, MCONTROL6_VS_OFFSET, &triggers[hartid][idx].tdata1);
		update_bit(state.u, MCONTROL6_U_OFFSET, &triggers[hartid][idx].tdata1);
		update_bit(state.s, MCONTROL6_S_OFFSET, &triggers[hartid][idx].tdata1);
		break;
	default:
		break;
	}

	/*
	 * RISC-V Debug Support v1.0.0 section 5.5:
	 * Debugger cannot simply set a trigger by writing tdata1, then tdata2, etc. The current
	 * value of tdata2 might not be legal with the new value of tdata1. To help with this
	 * situation, it is guaranteed that writing 0 to tdata1 disables the trigger, and
	 * leaves it in a state where tdata2 and tdata3 can be written with any value
	 * that makes sense for any trigger type supported by this trigger.
	 */
	csr_write(CSR_TSELECT, idx);
	csr_write(CSR_TDATA1, 0x0);
	csr_write(CSR_TDATA2, triggers[hartid][idx].tdata2);
	csr_write(CSR_TDATA1, triggers[hartid][idx].tdata1);
}

static void dbtr_trigger_disable(unsigned int hartid, unsigned int idx)
{
	union riscv_dbtr_tdata1 tdata1;

	if (!triggers[hartid][idx].state.mapped)
		return;

	tdata1.value = triggers[hartid][idx].tdata1;

	switch (tdata1.type) {
	case RISCV_DBTR_TRIG_MCONTROL:
		__clear_bit(MCONTROL_U_OFFSET, &triggers[hartid][idx].tdata1);
		__clear_bit(MCONTROL_S_OFFSET, &triggers[hartid][idx].tdata1);
		break;
	case RISCV_DBTR_TRIG_MCONTROL6:
		__clear_bit(MCONTROL6_VU_OFFSET, &triggers[hartid][idx].tdata1);
		__clear_bit(MCONTROL6_VS_OFFSET, &triggers[hartid][idx].tdata1);
		__clear_bit(MCONTROL6_U_OFFSET, &triggers[hartid][idx].tdata1);
		__clear_bit(MCONTROL6_S_OFFSET, &triggers[hartid][idx].tdata1);
		break;
	default:
		break;
	}

	csr_write(CSR_TSELECT, idx);
	csr_write(CSR_TDATA1, triggers[hartid][idx].tdata1);
}

static void dbtr_trigger_clear(unsigned int hartid, unsigned int idx)
{
	if (!triggers[hartid][idx].state.mapped)
		return;

	csr_write(CSR_TSELECT, idx);
	csr_write(CSR_TDATA1, 0x0);
	csr_write(CSR_TDATA2, 0x0);
}

static int dbtr_trigger_supported(unsigned long type)
{
	switch (type) {
	case RISCV_DBTR_TRIG_MCONTROL:
	case RISCV_DBTR_TRIG_MCONTROL6:
		return 1;
	default:
		break;
	}

	return 0;
}

static int dbtr_trigger_valid(unsigned long type, unsigned long tdata)
{
	union riscv_dbtr_tdata1_mcontrol6 control6;
	union riscv_dbtr_tdata1_mcontrol control;

	switch (type) {
	case RISCV_DBTR_TRIG_MCONTROL:
		control.value = tdata;
		if (!control.action && !control.dmode && !control.m)
			return 1;
		break;
	case RISCV_DBTR_TRIG_MCONTROL6:
		control6.value = tdata;
		if (!control6.action && !control6.dmode && !control6.m)
			return 1;
		break;
	default:
		break;
	}

	return 0;
}

int sbi_dbtr_num_trig(unsigned long data, unsigned long *out)
{
	unsigned long type = ((union riscv_dbtr_tdata1)data).type;
	u32 hartid = current_hartid();
	unsigned long total = 0;
	int i;


	if (data == 0) {
		sbi_dprintf("%s: hart%d: total triggers: %u\n",
			    __func__, hartid, total_trigs);
		*out = total_trigs;
		return SBI_SUCCESS;
	}

	for (i = 0; i < total_trigs; i++) {
		if (__test_bit(type, &triggers[hartid][i].type_mask))
			total++;
	}


	sbi_dprintf("%s: hart%d: total triggers of type %lu: %lu\n",
		    __func__, hartid, type, total);

	*out = total;
	return SBI_SUCCESS;
}

int sbi_dbtr_read_trig(const struct sbi_domain *dom, unsigned long smode,
		unsigned long trig_idx_base, unsigned long trig_count,
		unsigned long out_addr_div_by_16)
{
	unsigned long out_addr = (out_addr_div_by_16 << 4);
	struct sbi_dbtr_data_msg *xmit;
	u32 hartid = current_hartid();
	int i;

	if (smode != PRV_S)
		return SBI_ERR_DENIED;
	if (dom && !sbi_domain_is_assigned_hart(dom, hartid))
		return SBI_ERR_DENIED;
	if (dom && !sbi_domain_check_addr(dom, out_addr, smode, SBI_DOMAIN_READ | SBI_DOMAIN_WRITE))
		return SBI_ERR_INVALID_ADDRESS;

	if (trig_idx_base >= total_trigs || trig_idx_base + trig_count >= total_trigs) {
		sbi_dprintf("%s: hart%d: invalid trigger index\n", __func__, hartid);
		return SBI_ERR_INVALID_PARAM;
	}

	for (i = 0; i < trig_count; i++) {
		xmit = (struct sbi_dbtr_data_msg *)(out_addr + i * sizeof(*xmit));

		sbi_dprintf("%s: hart%d: read trigger %d\n", __func__, hartid, i);

		xmit->tstate = cpu_to_lle(triggers[hartid][i + trig_idx_base].state.value);
		xmit->tdata1 = cpu_to_lle(triggers[hartid][i + trig_idx_base].tdata1);
		xmit->tdata2 = cpu_to_lle(triggers[hartid][i + trig_idx_base].tdata2);
		xmit->tdata3 = cpu_to_lle(triggers[hartid][i + trig_idx_base].tdata3);
	}

	return SBI_SUCCESS;
}

int sbi_dbtr_install_trig(const struct sbi_domain *dom, unsigned long smode,
		unsigned long trig_count, unsigned long in_addr_div_by_16,
		unsigned long out_addr_div_by_16, unsigned long *out)
{
	unsigned long out_addr = (out_addr_div_by_16 << 4);
	unsigned long in_addr = (in_addr_div_by_16 << 4);
	u32 hartid = current_hartid();
	struct sbi_dbtr_data_msg *recv;
	struct sbi_dbtr_id_msg *xmit;
	union riscv_dbtr_tdata1 ctrl;
	int i, k;

	if (smode != PRV_S)
		return SBI_ERR_DENIED;
	if (dom && !sbi_domain_is_assigned_hart(dom, hartid))
		return SBI_ERR_DENIED;
	if (dom && !sbi_domain_check_addr(dom, in_addr, smode, SBI_DOMAIN_READ | SBI_DOMAIN_WRITE))
		return SBI_ERR_INVALID_ADDRESS;
	if (dom && !sbi_domain_check_addr(dom, out_addr, smode, SBI_DOMAIN_READ | SBI_DOMAIN_WRITE))
		return SBI_ERR_INVALID_ADDRESS;

	/* TODO: check chained triggers configurations */

	/* Check requested triggers configuration */
	for (k = 0; k < trig_count; k++) {
		recv = (struct sbi_dbtr_data_msg *)(in_addr + k * sizeof(*recv));
		ctrl = (union riscv_dbtr_tdata1)recv->tdata1;

		if (!dbtr_trigger_supported(ctrl.type)) {
			sbi_dprintf("%s: invalid type of trigger %d\n", __func__, k);
			*out = k;
			return SBI_ERR_FAILED;
		}

		if (!dbtr_trigger_valid(ctrl.type, ctrl.value)) {
			sbi_dprintf("%s: invalid configuration of trigger %d\n", __func__, k);
			*out = k;
			return SBI_ERR_FAILED;
		}
	}

	/* Check if we have enough spare triggers */
	for (i = 0, k = 0; i < total_trigs; i++) {
		if (!triggers[hartid][i].state.mapped)
			k++;
	}

	if (k < trig_count) {
		sbi_dprintf("%s: hartid%d: not enough spare triggers\n", __func__, hartid);
		*out = k;
		return SBI_ERR_FAILED;
	}

	/* Install triggers */
	for (i = 0, k = 0; i < total_trigs; i++) {
		if (triggers[hartid][i].state.mapped)
			continue;

		recv = (struct sbi_dbtr_data_msg *)(in_addr + k * sizeof(*recv));
		xmit = (struct sbi_dbtr_id_msg *)(out_addr + k * sizeof(*xmit));

		sbi_dprintf("%s: hart%d: idx[%d] tdata1[0x%lx] tdata2[0x%lx]\n",
			    __func__, hartid, i, recv->tdata1, recv->tdata2);

		dbtr_trigger_init(hartid, i,  recv);
		dbtr_trigger_enable(hartid, i);
		xmit->idx = cpu_to_lle(i);

		if (++k >= trig_count)
			break;
	}

	return SBI_SUCCESS;
}

int sbi_dbtr_uninstall_trig(unsigned long trig_idx_base, unsigned long trig_idx_mask)
{
	unsigned long trig_mask = trig_idx_mask << trig_idx_base;
	unsigned long idx = trig_idx_base;
	u32 hartid = current_hartid();

	sbi_dprintf("%s: hart%d: triggers mask: 0x%lx\n",
		    __func__, hartid, trig_mask);

	for_each_set_bit_from(idx, &trig_mask, total_trigs) {
		if (!triggers[hartid][idx].state.mapped) {
			sbi_dprintf("%s: trigger %lu not mapped\n", __func__, idx);
			return SBI_ERR_INVALID_PARAM;
		}

		sbi_dprintf("%s: clear trigger %lu\n", __func__, idx);
		dbtr_trigger_clear(hartid, idx);

		triggers[hartid][idx].state.value = 0;
		triggers[hartid][idx].tdata1 = 0;
		triggers[hartid][idx].tdata2 = 0;
		triggers[hartid][idx].tdata3 = 0;
	}

	return SBI_SUCCESS;
}

int sbi_dbtr_enable_trig(unsigned long trig_idx_base, unsigned long trig_idx_mask)
{
	unsigned long trig_mask = trig_idx_mask << trig_idx_base;
	unsigned long idx = trig_idx_base;
	u32 hartid = current_hartid();

	sbi_dprintf("%s: hart%d: triggers mask: 0x%lx\n",
		    __func__, hartid, trig_mask);

	for_each_set_bit_from(idx, &trig_mask, total_trigs) {
		sbi_dprintf("%s: enable trigger %lu\n", __func__, idx);
		dbtr_trigger_enable(hartid, idx);
	}

	return SBI_SUCCESS;
}

int sbi_dbtr_update_trig(const struct sbi_domain *dom, unsigned long smode,
		unsigned long trig_idx_base, unsigned long trig_idx_mask,
		unsigned long in_addr_div_by_16)
{
	unsigned long in_addr = (in_addr_div_by_16 << 4);
	unsigned long trig_mask = trig_idx_mask << trig_idx_base;
	unsigned long idx = trig_idx_base;
	u32 hartid = current_hartid();
	struct sbi_dbtr_data_msg *recv;
	unsigned long uidx = 0;

	sbi_dprintf("%s: hart%d: triggers mask: 0x%lx\n",
		    __func__, hartid, trig_mask);

	if (smode != PRV_S)
		return SBI_ERR_DENIED;
	if (dom && !sbi_domain_is_assigned_hart(dom, hartid))
		return SBI_ERR_DENIED;
	if (dom && !sbi_domain_check_addr(dom, in_addr, smode, SBI_DOMAIN_READ | SBI_DOMAIN_WRITE))
		return SBI_ERR_INVALID_ADDRESS;

	for_each_set_bit_from(idx, &trig_mask, total_trigs) {
		if (!triggers[hartid][idx].state.mapped) {
			sbi_dprintf("%s: trigger %lu not mapped\n", __func__, idx);
			return SBI_ERR_INVALID_PARAM;
		}

		recv = (struct sbi_dbtr_data_msg *)(in_addr + uidx * sizeof(*recv));

		sbi_dprintf("%s: update trigger %lu: newaddr 0x%lx\n",
			    __func__, idx, recv->tdata2);

		triggers[hartid][idx].tdata2 = lle_to_cpu(recv->tdata2);
		dbtr_trigger_enable(hartid, idx);
		uidx++;
	}

	return SBI_SUCCESS;
}

int sbi_dbtr_disable_trig(unsigned long trig_idx_base, unsigned long trig_idx_mask)
{
	unsigned long trig_mask = trig_idx_mask << trig_idx_base;
	unsigned long idx = trig_idx_base;
	u32 hartid = current_hartid();

	sbi_dprintf("%s: hart%d: triggers mask: 0x%lx\n",
		    __func__, hartid, trig_mask);

	for_each_set_bit_from(idx, &trig_mask, total_trigs) {
		sbi_dprintf("%s: disable trigger %lu\n", __func__, idx);
		dbtr_trigger_disable(hartid, idx);
	}

	return SBI_SUCCESS;
}
