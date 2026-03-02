/*
 * Copyright (C) 2024, Syntacore Ltd.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of its
 *        contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <platform_override.h>
#include <libfdt.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_atomic.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_csr_detect.h>
#include <sbi/sbi_hartmask.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_string.h>
#include <sbi/sbi_hsm.h>
#include <sbi/sbi_scratch.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_unpriv.h>
#include <sbi/sbi_system.h>
#include <sbi/sbi_heap.h>

#include <sbi_utils/fdt/fdt_domain.h>
#include <sbi_utils/fdt/fdt_fixup.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/fdt/fdt_pmu.h>
#include <sbi_utils/irqchip/fdt_irqchip.h>
#include <sbi_utils/irqchip/imsic.h>
#include <sbi_utils/irqchip/aplic.h>
#include <sbi_utils/serial/fdt_serial.h>
#include <sbi_utils/timer/fdt_timer.h>
#include <sbi_utils/timer/scr_mtimer.h>
#include <sbi_utils/ipi/fdt_ipi.h>
#include <sbi_utils/ipi/scr_iccm.h>
#include <sbi_utils/ipi/scr_iccm_mmio.h>

#include <syntacore/dt-bindings/scr_pma.h>
#include <syntacore/scr_encoding.h>
#include <syntacore/scr_generic.h>
#include <syntacore/scr_cache.h>
#include <syntacore/scr_l2_pmu.h>
#include <syntacore/scr_l3_pmu.h>
#include <syntacore/scr_mpu.h>
#include <syntacore/sbi_vendor.h>
#include <syntacore/scr_platform_info.h>
#include <syntacore/scr_fdt_helper.h>
#include <syntacore/scr_hwinfo.h>

// Length of the buffer to convert 8 bytes value to null-terminated hex string
#define BUILDID_STR_LEN ((8*2) + 1)

static volatile u8 hartid_nascent[SCR_CPU_MAX_HARTS] __aligned(64);
static volatile u8 hartid_has_swpw[SCR_CPU_MAX_HARTS] __aligned(64);
static struct caches_info scr_cache_info;
static ulong dram_reg_base = 0;
static ulong dram_reg_order = 0;
static ulong mcfg_reg_base = 0;
static ulong mcfg_reg_order = 0;

static int scr_l3_platform = 0;
static int scr_mpu_platform = 0;
static atomic_t mpu_fdt_read_done = ATOMIC_INITIALIZER(0);
static volatile u32 coldboot_hart_id;

int scr_hartid_nascent_get(unsigned int hartid)
{
	if (hartid >= SCR_CPU_MAX_HARTS)
		return -SBI_EINVAL;

	return hartid_nascent[hartid];
}

int scr_hartid_nascent_set(unsigned int hartid, u8 val)
{
	if (hartid >= SCR_CPU_MAX_HARTS)
		return -SBI_EINVAL;

	hartid_nascent[hartid] = val;

	return 0;
}

void scr_hartid_nascent_flush(void)
{
	scr_cache_flush((void *)hartid_nascent, sizeof(hartid_nascent));
}

int mmio_read_allowed(uintptr_t addr)
{
	struct sbi_trap_info trap_info;
	trap_info.cause = 0;
	// Set MPP in cause of mprv in sbi_load_u32
	unsigned long mstatus_val = csr_read_set(CSR_MSTATUS, (PRV_M << MSTATUS_MPP_SHIFT));

	sbi_load_u32((const u32*)addr, &trap_info);

	// return mcause to original value
	csr_write(CSR_MSTATUS, mstatus_val);

	return (trap_info.cause == 0);
}

int scr_hartid_swpw_set(unsigned int hartid, u8 val)
{
	if(hartid >= array_size(hartid_has_swpw))
		return SBI_EINVAL;

	hartid_has_swpw[hartid] = val;

	return SBI_OK;
}

int scr_hartid_swpw_get(unsigned int hartid)
{
	if(hartid >= array_size(hartid_has_swpw))
		return SBI_EINVAL;

	return hartid_has_swpw[hartid];
}

static void scr_patch_hwinfo(void *fdt)
{
	unsigned long info_val;
	char buildid_str[BUILDID_STR_LEN] = {0};

	if(scr_hwinfo_init(fdt) != SCR_HWINFO_OK)
		return;		// Nothing to do here. None of hwinfo methods exist

	if(scr_hwinfo_get_sys_clk(&info_val) == SCR_HWINFO_OK) {
		scr_fdt_patch_sys_clk(fdt, info_val);
		scr_fdt_patch_axi_clk(fdt, info_val);
		scr_fdt_patch_eth_clk(fdt, info_val);
	}

	if(scr_hwinfo_get_uart_clk(&info_val) == SCR_HWINFO_OK)
		scr_fdt_patch_uart_clk(fdt, info_val);

	if(scr_hwinfo_get_mtimer_clk(&info_val) == SCR_HWINFO_OK)
		scr_fdt_patch_mtimer_clk(fdt, info_val);

	if(scr_hwinfo_get_build_id(&info_val) == SCR_HWINFO_OK) {
		sbi_snprintf(buildid_str, sizeof(buildid_str), "%lx", info_val);
		scr_fdt_patch_build_id_str(fdt, buildid_str);
	}

	scr_fdt_patch_cpu_frequency(fdt, scr_hwinfo_get_hart_clk);
}

/**
 * OpenSBI configures memory regions for IMSIC_M and APLIC_M by itself and
 * puts them into root domain. Then they are copied to other available domains.
 * We need to add memory type into regions flags - NC/SO.
 * This function must be called after sbi_domain_finalize() and before
 * sbi_hart_pmp_configure().
 */
static void scr_patch_aia_memregions(void *fdt)
{
	struct sbi_domain *dom;
	struct sbi_domain_memregion *reg;
	struct imsic_data *imsic;
	struct aplic_data *aplic;
	uint64_t reg_base, reg_size;
	int offset = -1, rc;
	bool patch_imsic = false, patch_aplic = false;

	/* If smaia extension is not available, nothing to do here */
	if (!sbi_hart_has_extension(sbi_scratch_thishart_ptr(), SBI_HART_EXT_SMAIA))
		return;

	/* Get IMSIC_M base and size. OpenSBI saves that data */
	imsic = imsic_get_data(sbi_hartid_to_hartindex(current_hartid()));

	/* Sanity checks */
	if (imsic && imsic->regs->addr && imsic->regs->size)
		patch_imsic = true;

	aplic = sbi_zalloc(sizeof(*aplic));

	/* But APLIC_M data is not saved, sadly, we have to parse dts nodes once more */
	if (aplic) {
		while ((offset = fdt_node_offset_by_compatible(fdt, offset, "riscv,aplic")) >= 0) {
			rc = fdt_parse_aplic_node(fdt, offset, aplic);

			if (!rc && aplic->targets_mmode) {
				patch_aplic = true;
				break;
			}
		}
	} else {
		sbi_printf("%s: failed to allocate memory! "
			   "APLIC M is not available!\n\n", __func__);
	}

	if (!patch_imsic && !patch_aplic)
		goto free_data;

	/* Iterate through each domain and find IMSIC_M and APLIC_M memory regions.
	 * Then expand region flags with memory type - NC/SO.
	*/
	sbi_domain_for_each(dom) {
		sbi_domain_for_each_memregion(dom, reg) {
			reg_base = reg->base;
			reg_size = (1UL << reg->order);

			if (patch_imsic &&
			    reg_base == imsic->regs->addr &&
			    reg_size == imsic->regs->size)
				reg->flags |= SCR_PMA_UNC;

			if (patch_aplic &&
			    reg_base == aplic->addr &&
			    reg_size == aplic->size)
				reg->flags |= SCR_PMA_UNC;
		}
	}

free_data:
	sbi_free(aplic);
}

static void syntacore_scr_fw_init(const void *fdt, const struct fdt_match *match)
{
	int offset;
	// Discover boot hart
	coldboot_hart_id = current_hartid();
	// Found root offset, if not found, fdt may be corrupted or not exist
	offset = fdt_path_offset(fdt, "/");
	if (offset < 0) {
		sbi_hart_hang();
	}

	scr_l2c_probe(fdt);
	if (scr_l3c_probe(fdt) == SBI_OK)
		scr_l3_platform = 1;

	scr_l2c_set_llc(!scr_l3_platform);

	int mtimer_clk_src = SCR_CLKSRC_INTERNAL;
	if(scr_fdt_check_mtimer_scr_external(fdt) == SBI_OK)
		mtimer_clk_src = SCR_CLKSRC_EXTERNAL;

	scr_mtimer_set_clocksource(mtimer_clk_src);

	scr_patch_hwinfo((void *)fdt);

	// Check for mpu
	offset = fdt_path_offset(fdt, "/chosen/opensbi-domains/scr_mpu");
	if (offset >= 0) {
		scr_mpu_platform = 1;
	}
	// Read dram and mcfg properties from dtb for nascent_init()
	offset = fdt_path_offset(fdt, "/chosen/opensbi-domains/dram_reg");
	if (offset >= 0) {
		scr_get_node_property_val_64(fdt, offset, "base", &dram_reg_base);
		scr_get_node_property_val_64(fdt, offset, "order", &dram_reg_order);
	} else {
		sbi_hart_hang();
	}
	offset = fdt_path_offset(fdt, "/chosen/opensbi-domains/mcfg_reg");
	if (offset >= 0) {
		scr_get_node_property_val_64(fdt, offset, "base", &mcfg_reg_base);
		scr_get_node_property_val_64(fdt, offset, "order", &mcfg_reg_order);
	} else {
		sbi_hart_hang();
	}

	scr_fdt_detect_swpw((void *)fdt);
}

static int syntacore_scr_nascent_init(void)
{
	generic_nascent_init();

	u32 hartid = current_hartid();
	// If hart already inited, nothing to do here
	if (scr_hartid_nascent_get(hartid) > 0)
		return 0;

	if (scr_mpu_platform) {
		scr_hart_nascent_mpu_configure_mcfg(mcfg_reg_base, BIT(mcfg_reg_order));
	} else {
		// Early set for mcfg region, others will be set later
		pmp_set(EARLY_PMP_MCFG_REG, PMP_R | PMP_W | PMP_SCR_PMA_NC_MMCFG,
			mcfg_reg_base, mcfg_reg_order);
	}

	if (scr_mpu_platform) {
		scr_hart_nascent_mpu_configure_dram(dram_reg_base, BIT(dram_reg_order));
	}

	// Fill info about actual online harts
	if (hartid < SCR_CPU_MAX_HARTS)
		scr_hartid_nascent_set(hartid, 1);
	// Get L1/L2 caches info
	scr_get_cache_l1d_info(&scr_cache_info.l1d[hartid]);
	scr_get_cache_l1i_info(&scr_cache_info.l1i[hartid]);
	scr_get_cache_l2_info(&scr_cache_info.l2[hartid]);

	return 0;
}

static int syntacore_scr_early_init(bool cold_boot)
{
	int iccm_offset, err = 0;
	unsigned hart_count = 0;
	void *fdt_rw = fdt_get_address_rw();

	if (scr_mpu_platform) {
		if (!cold_boot)
			while (!atomic_read(&mpu_fdt_read_done));
		err = scr_hart_early_mpu_configure(cold_boot, ((const void*)fdt_rw), mcfg_reg_base, dram_reg_base);
		if (err)
			return err;
		if (cold_boot)
			atomic_write(&mpu_fdt_read_done, 1);
	}
	if (cold_boot) {
		/* If ICCM present, count harts here */
		iccm_offset = fdt_node_offset_by_compatible(((const void*)fdt_rw), -1, "syntacore,iccm");
		if (iccm_offset >= 0) {
			for (int i = 0; i < SCR_CPU_MAX_HARTS; i++)
				hart_count += (scr_hartid_nascent_get(i) > 0) ? 1 : 0;
			if (hart_count == 1)
				scr_iccm_disable();
		}
	}

	generic_early_init(cold_boot);

	return err;
}

static void scr_set_swpw_feat_en(void)
{
	/*
	 * For scr7/9 harts: set SWPW through feat_en register
	 * if it occures in dtb and possible.
	 */
	unsigned hartid;
	unsigned long val;
	struct sbi_trap_info trap = {0};

	hartid = current_hartid();

	/* Check if cpu node has software-pagewalker on in dtb property */
	if (scr_hartid_swpw_get(hartid) == 1) {
		/*
		 * If dtb for cpu@x node contains "scr,software-pagewalker" property,
		 * try to switch on pagewalker
		 */
		val = csr_read_allowed(SCR_CSR_FEAT_EN, (ulong)&trap);
		/* If feat_en is available */
		if (!trap.cause)  {
			/* If feat_en isn't locked (MSB=0), we'll try to rewrite EN_HPW_BIT */
			if (!(val & FEAT_EN_LOCK_BIT)) {
				/* Write and then re-read FEAT_EN csr value */
				csr_write(SCR_CSR_FEAT_EN, (val & ~FEAT_EN_HPW_BIT));
				val = csr_read(SCR_CSR_FEAT_EN);
			}
			/* If HPW bit is set, set software pagewalker extension off */
			if (val & FEAT_EN_HPW_BIT) {
				scr_hartid_swpw_set(hartid, 0);
				sbi_printf("\nSWPW: HART %x FEAT_EN 0x%08lx, unable to unset HWPW bit.\n", hartid, val);
			}
		}
		/*
		 * Set extension for all valid cases (feat_en + no_hpw or
		 * if feat_en not exists, but dtb for cpu@x node contains
		 * "software-pagewalker" property.
		 */
		if (scr_hartid_swpw_get(hartid) == 1) {
			sbi_hart_update_extension(sbi_scratch_thishart_ptr(),
					SBI_HART_EXT_XSCSWPW, true);
		}
	}
}

static int syntacore_scr_extensions_init(struct sbi_hart_features *hfeatures)
{
	/*
	 * Reading MARCHID csr
	 * After that, for SCR5 software pagewalker extension is on
	 * For other core types, depends on dtb node
	 */
	unsigned long csr_val;
	int rc;

	rc = generic_extensions_init(hfeatures);
	if (rc)
		return rc;

	csr_val = csr_read(CSR_MARCHID);
	if ((csr_val & MARCHID_CORE_MASK) == MARCHID_CORE_SCR5) {
		scr_hartid_swpw_set(current_hartid(), 1);
		sbi_hart_update_extension(sbi_scratch_thishart_ptr(), SBI_HART_EXT_XSCSWPW, true);
	} else {
		scr_set_swpw_feat_en();
	}

	return 0;
}

void fdt_cpu_scr_fixup(void *fdt)
{
	int err, cpu_offset, cpus_offset;
	u32 hartid;
	u32 pause = 100000;

	// Wait for secondary harts in case
	while (pause--)
		cpu_relax();

	// Extend space for dtb in case more space needed for changed status
	err = fdt_open_into(fdt, fdt, fdt_totalsize(fdt) + SCR_CPU_MAX_HARTS*4);
	if (err < 0)
		return;
	if (scr_l3_platform)
		// Get L3 cache info
		scr_get_cache_l3_info(&scr_cache_info.l3);
	// Patch CPU nodes
	cpus_offset = fdt_path_offset(fdt, "/cpus");
	if (cpus_offset < 0)
		return;

	fdt_for_each_subnode(cpu_offset, fdt, cpus_offset) {
		err = fdt_parse_hart_id(fdt, cpu_offset, &hartid);
		if (err)
			continue;
		if (!fdt_node_is_enabled(fdt, cpu_offset))
			continue;
		// Disable a HART DT node if hart is not online
		if (scr_hartid_nascent_get(hartid) <= 0)
			fdt_setprop_string(fdt, cpu_offset, "status",
					   "fail");
		// Patch nodes for existing harts
		if (scr_hartid_nascent_get(hartid) > 0) {
			// L1 properties patch
			fdt_setprop_inplace_u32(fdt, cpu_offset, "d-cache-block-size", scr_cache_info.l1d[hartid].block_size);
			fdt_setprop_inplace_u32(fdt, cpu_offset, "d-cache-sets", scr_cache_info.l1d[hartid].sets);
			fdt_setprop_inplace_u32(fdt, cpu_offset, "d-cache-size", scr_cache_info.l1d[hartid].size);
			fdt_setprop_inplace_u32(fdt, cpu_offset, "i-cache-block-size", scr_cache_info.l1i[hartid].block_size);
			fdt_setprop_inplace_u32(fdt, cpu_offset, "i-cache-sets", scr_cache_info.l1i[hartid].sets);
			fdt_setprop_inplace_u32(fdt, cpu_offset, "i-cache-size", scr_cache_info.l1i[hartid].size);

			uint32_t cpu_cbom_block_size = MAX(scr_cache_info.l3.block_size,
							MAX(scr_cache_info.l2[hartid].block_size,
							MAX(scr_cache_info.l1d[hartid].block_size, scr_cache_info.l1i[hartid].block_size)));
			fdt_setprop_inplace_u32(fdt, cpu_offset, "riscv,cbom-block-size", cpu_cbom_block_size);
			if (scr_l3_platform) {
				// L2 properties patch (for L3 platforms)
				int l2_offset = fdt_node_offset_by_compatible(fdt, cpu_offset, "cache");
				if (l2_offset >= 0) {
					fdt_setprop_inplace_u32(fdt, l2_offset, "cache-size", scr_cache_info.l2[hartid].size);
					fdt_setprop_inplace_u32(fdt, l2_offset, "cache-sets", scr_cache_info.l2[hartid].sets);
					fdt_setprop_inplace_u32(fdt, l2_offset, "cache-block-size", scr_cache_info.l2[hartid].block_size);
				}
			}
		}
	}
	if (scr_l3_platform) {
		// L3 properties patch
		cpu_offset = fdt_path_offset(fdt, "/cpus/l3-cache");
		if (cpu_offset >= 0) {
			fdt_setprop_inplace_u32(fdt, cpu_offset, "cache-size", scr_cache_info.l3.size);
			fdt_setprop_inplace_u32(fdt, cpu_offset, "cache-sets", scr_cache_info.l3.sets);
			fdt_setprop_inplace_u32(fdt, cpu_offset, "cache-block-size", scr_cache_info.l3.block_size);
		}
	} else {
		// L2 properties patch (for L2 platforms)
		cpu_offset = fdt_path_offset(fdt, "/cpus/l2-cache");
		if (cpu_offset >= 0) {
			hartid = current_hartid();
			fdt_setprop_inplace_u32(fdt, cpu_offset, "cache-size", scr_cache_info.l2[hartid].size);
			fdt_setprop_inplace_u32(fdt, cpu_offset, "cache-sets", scr_cache_info.l2[hartid].sets);
			fdt_setprop_inplace_u32(fdt, cpu_offset, "cache-block-size", scr_cache_info.l2[hartid].block_size);
		}
	}
}

static int syntacore_scr_final_init(bool cold_boot)
{
	void *fdt = fdt_get_address_rw();

	generic_final_init(cold_boot);

	if (cold_boot) {
		scr_cache_ops_init();

		scr_patch_aia_memregions(fdt);
		fdt_cpu_fixup(fdt);
		fdt_cpu_scr_fixup(fdt);
		fdt_fixups(fdt);
		fdt_domain_fixup(fdt);

		scr_mtimer_print_info();
		scr_l1c_info();
		scr_l2c_info();
		if (scr_l3_platform) {
			scr_l3c_info();
		}
	}

	if (scr_mpu_platform) {
		if (cold_boot)
			scr_mpu_print_info();
	} else {
		// Remove doubling of MCFG region from PMP
		csr_write(CSR_PMPCFG2, csr_read(CSR_PMPCFG2) & (~EARLY_PMP_MCFG_MASK));
	}

	return 0;
}

/* Vendor-Specific SBI handler */
static int scr_vendor_ext_provider(long funcid,
				   struct sbi_trap_regs *regs,
				   struct sbi_ecall_return *out)
{
	if (funcid == SBI_SCR_L2_PMU_FN) {
		return scr_l2_pmu_ext_provider(regs, out);
	} else if ((funcid == SBI_SCR_L3_PMU_FN) && scr_l3_platform) {
		return scr_l3_pmu_ext_provider(regs, out);
	} else if (funcid == SBI_EXT_SCR_READ_PLFINFO) {
		return scr_get_platform_info(regs, out, scr_l3_platform);
	} else {
		return SBI_ENOTSUPP;
	}
}

static int scr_emulate_store(int wlen, unsigned long addr, union sbi_ldst_data in_val)
{
	struct sbi_scratch *scratch = sbi_scratch_thishart_ptr();
	struct sbi_trap_context *ctx = sbi_trap_get_context(scratch);
	struct sbi_trap_info *orig_trap = &ctx->trap;

	orig_trap->tval = addr;

	return wlen;
}

static int scr_emulate_load(int rlen, unsigned long addr, union sbi_ldst_data *out_val)
{
	struct sbi_scratch *scratch = sbi_scratch_thishart_ptr();
	struct sbi_trap_context *ctx = sbi_trap_get_context(scratch);
	struct sbi_trap_info *orig_trap = &ctx->trap;

	orig_trap->tval = addr;

	return rlen;
}

static int syntacore_platform_init(const void *fdt, int nodeoff,
					const struct fdt_match *match)
{
	generic_platform_ops.nascent_init = syntacore_scr_nascent_init;
	generic_platform_ops.early_init = syntacore_scr_early_init;
	generic_platform_ops.extensions_init = syntacore_scr_extensions_init;
	generic_platform_ops.final_init = syntacore_scr_final_init;
	generic_platform_ops.vendor_ext_provider = scr_vendor_ext_provider;
	generic_platform_ops.emulate_load = scr_emulate_load;
	generic_platform_ops.emulate_store = scr_emulate_store;

	syntacore_scr_fw_init(fdt, match);

	return 0;
}

static const struct fdt_match syntacore_scr_match[] = {
	{ .compatible = "syntacore,scr_sdk" },
	{ /* */ },
};

const struct fdt_driver syntacore_generic = {
	.match_table = syntacore_scr_match,
	.init = syntacore_platform_init,
};
