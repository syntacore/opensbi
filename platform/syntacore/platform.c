/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Syntacore
 *
 * this is actually a copy of generic/platform.c with sifive boards
 * removed.
 *
 * derivate: generic/platform.c
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <libfdt.h>
#include <sbi/riscv_io.h>
#include <sbi/riscv_asm.h>
#include <sbi/sbi_hartmask.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_system.h>
#include <sbi/sbi_string.h>
#include <sbi/sbi_hsm.h>
#include <sbi/sbi_scratch.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_pmu.h>
#include <sbi/sbi_hart.h>

#include <sbi_utils/fdt/fdt_domain.h>
#include <sbi_utils/fdt/fdt_fixup.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/fdt/fdt_pmu.h>
#include <sbi_utils/irqchip/fdt_irqchip.h>
#include <sbi_utils/serial/fdt_serial.h>
#include <sbi_utils/timer/fdt_timer.h>
#include <sbi_utils/ipi/fdt_ipi.h>
#include <sbi_utils/reset/fdt_reset.h>
#include <sbi_utils/timer/aclint_mtimer.h>

#include "platform.h"
#include "scr_mpu.h"
#include "scr_cache.h"
#include "scr_mtimer.h"
#include "scr_iccm.h"
#include "scr_plic.h"
#include "scr_l2_pmu.h"

void *platform_fdt;
extern struct sbi_platform platform;
static u32 generic_hart_index2id[SBI_HARTMASK_MAX_BITS] = { 0 };

/*
 * The fw_platform_init() function is called very early on the boot HART
 * OpenSBI reference firmwares so that platform specific code get chance
 * to update "platform" instance before it is used.
 *
 * The arguments passed to fw_platform_init() function are boot time state
 * of A0 to A4 register. The "arg0" will be boot HART id and "arg1" will
 * be address of FDT passed by previous booting stage.
 *
 * The return value of fw_platform_init() function is the FDT location. If
 * FDT is unchanged (or FDT is modified in-place) then fw_platform_init()
 * can always return the original FDT location (i.e. 'arg1') unmodified.
 */
unsigned long fw_platform_init(unsigned long arg0, unsigned long arg1,
			       unsigned long arg2, unsigned long arg3,
			       unsigned long arg4)
{
	const char *model;
	void *fdt = (void *)arg1;
	u32 hartid, hart_count = 0;
	int rc, root_offset, cpus_offset, cpu_offset, len;

	/* use bundled fdt if have no fdt passed from ancestor */
	root_offset = fdt_path_offset(fdt, "/");
	if (root_offset < 0) {
		fdt = platform_bundled_fdt();
		root_offset = fdt_path_offset(fdt, "/");
		if (root_offset < 0)
			goto fail;
	}
	/* next-arg1 could be changed for domains in fdt,
	   to preserve this tricky feature we use own fdt pointer
	   for future needs of initialization instead of scratch->next-arg1
	   */
	platform_set_fdt(fdt);

#if CONFIG_PLATFORM_SYNTACORE_SDK_VCU118
	/* initial MPU configuration from fdt/<scr-mpu-early-init> */
	scr_hart_early_mpu_configure(true, fdt);
#endif

	/** char name[64]; */
	model = fdt_getprop(fdt, root_offset, "model", &len);
	if (model) {
		len = len < sizeof(platform.name) - 1
			? len : sizeof(platform.name) - 1;
		sbi_strncpy(platform.name, model, len);
	}

	cpus_offset = fdt_path_offset(fdt, "/cpus");
	if (cpus_offset < 0)
		goto fail;

	/* TODO: add runtime hart enumeration routine with fixing fdt in place */
	fdt_for_each_subnode(cpu_offset, fdt, cpus_offset) {
		rc = fdt_parse_hart_id(fdt, cpu_offset, &hartid);
		if (rc)
			continue;

		if (hartid >= SBI_HARTMASK_MAX_BITS)
			continue;

		generic_hart_index2id[hart_count++] = hartid;
	}
	platform.hart_count = hart_count;

	/* return fdt to descendant (original or bundled) */
	return (unsigned long)fdt;
fail:
	while (1)
		wfi();
}

static int scr_hart_start(u32 hartid, ulong saddr)
{
	atomic_t *state = sbi_hsm_get_state_ptr(hartid);

	scr_cache_flush(state, sizeof(atomic_t));

	scr_ipi_send(hartid);

	sbi_printf("opensbi: start hart#%d @ %lx\n", hartid, saddr);

	return 0;
}

static struct sbi_hsm_device scr_hsm = {
	.name = "scr_hsm",
	.hart_start = scr_hart_start,
};

static int scr_system_reset_check(u32 type, u32 reason)
{
	return 1;
}

static void scr_system_reset(u32 type, u32 reason)
{
	// TODO: add system reset
	sbi_printf("opensbi: system reset... (not yet implemented)\n");

	while (1)
		wfi();
}

static struct sbi_system_reset_device scr_reset = {
	.name = "scr_reset",
	.system_reset_check = scr_system_reset_check,
	.system_reset = scr_system_reset
};

static int scr_early_init(bool cold_boot)
{
	scr_cache_l1_disable();
#ifdef CONFIG_PLATFORM_SYNTACORE_L1_CACHE
	scr_cache_l1_enable();
#endif

	if (cold_boot) {
		sbi_hsm_set_device(&scr_hsm);
		sbi_system_reset_add_device(&scr_reset);
	}

	return 0;
}

static int scr_final_init(bool cold_boot)
{
	void *fdt = platform_get_fdt();
	int rc;

	if (cold_boot) {
		fdt_reset_init();
		fdt_cpu_fixup(fdt);
		fdt_fixups(fdt);
		fdt_domain_fixup(fdt);

		scr_mtimer_print_info();

		/* do we still needed our own PLIC? */
		rc = src_fdt_plic_fixup(fdt, false);
		if (rc) {
			sbi_printf("failed to init PLIC with %d\n", rc);
			return rc;
		}

		scr_print_l1cache_info();

		/* init SCR L2 Cache - it's okay to fail */
		rc = scr_fdt_l2_cache_init(fdt);
		if (!rc) {
			sbi_printf("L2$ was %s at start\n", scr_l2cache_is_enabled()?"enabled":"disabled");
#ifdef CONFIG_PLATFORM_SYNTACORE_L2_CACHE
			scr_l2cache_enable();
#else
			if (scr_l2cache_is_enabled())
				scr_l2cache_disable();
#endif
		}
		else {
			sbi_printf("failed to init SCR L2 Cache with %d\n", rc);
		}

		scr_print_l2cache_info();

		/* init SCR L2 Cache PMU extension - it's okay to fail */
		rc = scr_fdt_l2_pmu_init(fdt);
		if (rc && rc != SBI_ENODEV)
			sbi_printf("failed to init SCR L2 Cache PMU with %d\n",
				   rc);
	}

	scr_hart_mpu_configure(fdt);

	if (cold_boot)
		scr_mpu_print_info();

	return 0;
}

static int scr_domains_init(void)
{
	return fdt_domains_populate(platform_get_fdt());
}

static int scr_ipi_init(bool cold_boot)
{
	int rc;

	if (cold_boot) {
		rc = scr_fdt_iccm_init(platform_get_fdt());
		if (rc && rc != SBI_ENODEV)
			return rc;
	}

	scr_iccm_warm_init();

	return 0;
}

static void scr_ipi_exit(void)
{
}

/* Syntacore SCR7 mode inhibit bits quirk */
/* 30..29 MFILT 0 Privilege level M/S/U Filter
 *	00 - User; 01 - Supervisor; 11 - Machine
 * 0x00000000 - U-Mode
 * 0x20000000 - S-Mode
 * 0x60000000 - M-Mode
 * 31 MF_EN enable/disable filter
 */
#define SCR7_MHPMEVENT_U	0x00000000
#define SCR7_MHPMEVENT_S	0x20000000
#define SCR7_MHPMEVENT_M	0x60000000
#define SCR7_MHPMEVENT_MASK	_ULL(0x60000000)
#define SCR7_MODE_FILTER_ENABLE	(_ULL(1) << 31)

static void scr_update_inhibit_flags(unsigned long flags,
				      uint64_t *mhpmevent_val)
{
	/* clear all filters */
	*mhpmevent_val = (*mhpmevent_val & ~SCR7_MHPMEVENT_MASK);
	/* disable filtering */
	*mhpmevent_val = (*mhpmevent_val & ~SCR7_MODE_FILTER_ENABLE);

	if ((flags & SBI_PMU_CFG_FLAG_SET_UINH) &&
		(flags & SBI_PMU_CFG_FLAG_SET_SINH)) {
		*mhpmevent_val |= SCR7_MHPMEVENT_M;
		goto set_filter;
	}

	if (!(flags & SBI_PMU_CFG_FLAG_SET_MINH))
		return;

	if (flags & SBI_PMU_CFG_FLAG_SET_UINH) {
		*mhpmevent_val |= SCR7_MHPMEVENT_S;
		goto set_filter;
	} else if (flags & SBI_PMU_CFG_FLAG_SET_SINH) {
		*mhpmevent_val |= SCR7_MHPMEVENT_U;
		goto set_filter;
	}

	return;
set_filter:
	/* if any filter selector is set enable MFILT */
	*mhpmevent_val |= SCR7_MODE_FILTER_ENABLE;
}

static struct sbi_pmu_device scr_pmu_device = {
	.name			 = "scr_pmu",
	.hw_update_inhibit_flags = scr_update_inhibit_flags,
};

static int scr_pmu_init(void)
{
	return fdt_pmu_setup(platform_get_fdt());
}

static int scr_extensions_init(struct sbi_hart_features *hfeatures)
{
#ifdef CONFIG_PLATFORM_SYNTACORE_SCR7
	hfeatures->extensions |= BIT(SBI_HART_EXT_SSCOFPMF);
#endif
	sbi_pmu_set_device(&scr_pmu_device);

	return 0;
}

static uint64_t scr_pmu_xlate_to_mhpmevent(uint32_t event_idx,
					   uint64_t data)
{
	uint64_t evt_val = 0;

	/* data is valid only for raw events and is equal to event selector */
	if (event_idx == SBI_PMU_EVENT_RAW_IDX)
		evt_val = data;
	else {
		/**
		 * Generic platform follows the SBI specification recommendation
		 * i.e. zero extended event_idx is used as mhpmevent value for
		 * hardware general/cache events if platform does't define one.
		 */
		evt_val = fdt_pmu_get_select_value(event_idx);
		if (!evt_val)
			evt_val = (uint64_t)event_idx;
	}

	return evt_val;
}

static u64 scr_tlbr_flush_limit(void)
{
	return SBI_PLATFORM_TLB_RANGE_FLUSH_LIMIT_DEFAULT;
}

/**
 * The time init code below is just a reflection
 * of lib/utils/timer/fdt_timer.c once OpenSBI supports
 * driver init, we can drop the code above and simply add it
 * to global fdt_timer driver list.
 */
static struct fdt_timer *current_timer_driver;

static int scr_timer_warm_init(void)
{
	if (current_timer_driver == 0)
		return SBI_ENODEV;

	return current_timer_driver->warm_init();
}

static int scr_timer_cold_init(void)
{
	int noff, rc;
	struct fdt_timer *drv = &fdt_scr_mtimer;
	const struct fdt_match *match;
	void *fdt = platform_get_fdt();

	noff = -1;
	while ((noff = fdt_find_match(fdt, noff,
		drv->match_table, &match)) >= 0) {
		if (drv->cold_init) {
			rc = drv->cold_init(fdt, noff, match);
			if (rc == SBI_ENODEV)
				continue;
			if (rc)
				return rc;

			current_timer_driver = drv;
		}
	}

	return current_timer_driver == 0;
}

static int scr_timer_init(bool cold_boot)
{
	int rc;

	if (cold_boot) {
		rc = scr_timer_cold_init();
		if (rc)
			return rc;
	}

	return scr_timer_warm_init();
}

static int scr_vendor_ext_check(long extid)
{
	if (extid == SBI_EXT_SCR_L2_CACHE_PMU)
		return 1;

	return 0;
}

/* Vendor-Specific SBI handler */
static int scr_vendor_ext_provider(long extid, long funcid,
				const struct sbi_trap_regs *regs,
				unsigned long *out_value,
				struct sbi_trap_info *out_trap)
{
	/* only single L2 PMU extension is currently provided */
	return scr_pmu_ext_provider(extid, funcid, regs, out_value, out_trap);
}

const struct sbi_platform_operations platform_ops = {
	.early_init		= scr_early_init,
	.final_init		= scr_final_init,
	.domains_init		= scr_domains_init,
	.console_init		= fdt_serial_init,
	.irqchip_init		= fdt_irqchip_init,
	.irqchip_exit		= fdt_irqchip_exit,
	.ipi_init		= scr_ipi_init,
	.ipi_exit		= scr_ipi_exit,
	.pmu_init		= scr_pmu_init,
	.pmu_xlate_to_mhpmevent = scr_pmu_xlate_to_mhpmevent,
	.get_tlbr_flush_limit	= scr_tlbr_flush_limit,
	.timer_init		= scr_timer_init,
	.vendor_ext_check	= scr_vendor_ext_check,
	.vendor_ext_provider	= scr_vendor_ext_provider,
	.extensions_init	= scr_extensions_init,
};

struct sbi_platform platform = {
	.opensbi_version	= OPENSBI_VERSION,
	.platform_version	= SBI_PLATFORM_VERSION(SYNTACORE_PLATFORM_MAJOR_VER,
						       SYNTACORE_PLATFORM_MINOR_VER),
	/* will be replaced to model name from device tree */
	.name			= "Syntacore",
	.features		= SBI_PLATFORM_DEFAULT_FEATURES,
	.hart_count		= SBI_HARTMASK_MAX_BITS,
	.hart_index2id		= generic_hart_index2id,
	.hart_stack_size	= SBI_PLATFORM_DEFAULT_HART_STACK_SIZE,
	.platform_ops_addr	= (unsigned long)&platform_ops
};
