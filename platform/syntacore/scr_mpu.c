/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Syntacore
 *
 *
 * derivate: fsbl/src/mmu.h
 * Authors:
 *   Mikhail Nefedov <mikhail.nefedov@syntacore.com>
 */
#include "scr_mpu.h"

#include <libfdt.h>
#include <sbi/riscv_io.h>
#include <sbi/riscv_asm.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_scratch.h>
#include <sbi_utils/fdt/fdt_helper.h>

static void scr_mpu_region_update(unsigned int regn,
				  unsigned long base,
				  unsigned long size,
				  unsigned long attr)
{
	csr_write(SCR_CSR_MPU_SEL, regn);
	csr_write(SCR_CSR_MPU_ADDR, SCR_MPU_MK_ADDR(base));
	csr_write(SCR_CSR_MPU_MASK, SCR_MPU_MK_MASK(size));
	csr_write(SCR_CSR_MPU_CTRL, attr);
}

static void scr_mpu_region_setup(unsigned int regn,
			         unsigned long base,
			         unsigned long size,
			         unsigned long attr)
{
	csr_write(SCR_CSR_MPU_SEL, regn);
	csr_write(SCR_CSR_MPU_CTRL, 0);
	csr_write(SCR_CSR_MPU_ADDR, SCR_MPU_MK_ADDR(base));
	csr_write(SCR_CSR_MPU_MASK, SCR_MPU_MK_MASK(size));
	csr_write(SCR_CSR_MPU_CTRL, attr);
}

static inline void scr_mpu_region_disable(unsigned int regn)
{
	csr_write(SCR_CSR_MPU_SEL, regn);
	csr_write(SCR_CSR_MPU_CTRL, 0);
}

static unsigned long pmp_to_mpu_flags(const struct sbi_domain_memregion *reg)
{
	unsigned long flags = 0;

	if (reg->flags & SBI_DOMAIN_MEMREGION_READABLE)
		flags |= SCR_MPU_READ_ALL;

	if (reg->flags & SBI_DOMAIN_MEMREGION_WRITEABLE)
		flags |= SCR_MPU_WRITE_ALL;

	if (reg->flags & SBI_DOMAIN_MEMREGION_EXECUTABLE)
		flags |= SCR_MPU_EXECUTE_ALL;

	if (reg->flags & SBI_DOMAIN_MEMREGION_MMIO)
		flags |= SCR_MPU_NOCACHE_STRONG_ORDER;

	/** 0 - flags means protected from U/S-Mode region usually OpenSBI itself */
	if (!(reg->flags & SBI_DOMAIN_MEMREGION_ACCESS_MASK)) {
		if (reg->flags & SBI_DOMAIN_MEMREGION_MMIO)
			flags |= SCR_MPU_MMODE_RW;
		else
			flags |= SCR_MPU_MMODE_ALL;
	}

	return flags;
}

/** don't access anything here it will lead to exception,
 *  only RAM, and OpenSBI memory region is allowed
 */
void scr_hart_mpu_configure(void *fdt)
{
	const struct sbi_domain *dom = sbi_hartid_to_domain(current_hartid());
	struct sbi_domain_memregion *reg;
	unsigned long flags;
	int i, idx = 1;

	sbi_domain_for_each_memregion(dom, reg) {
		flags = reg->flags;

		if (flags & SCR_MPU_OPENSBI_SKIP)
			continue;

		if (!(flags & SCR_MPU_DEFINED_FLAGS))
			flags = pmp_to_mpu_flags(reg);

		/** clear SCR_MPU_DEFINED_FLAGS flags */
		flags &= ~(SCR_MPU_DEFINED_FLAGS);

		if (reg->order == __riscv_xlen)
			scr_mpu_region_setup(idx++, reg->base, 0,
					     flags | SCR_MPU_CTRL_VALID);
		else
			scr_mpu_region_setup(idx++, reg->base, BIT(reg->order),
					     flags | SCR_MPU_CTRL_VALID);

		RISCV_FENCE_I;
	}

	for (i = idx; i < SCR_MPU_MAX_REGIONS; i++) {
		csr_write(SCR_CSR_MPU_SEL, i);
		if (csr_read(SCR_CSR_MPU_SEL) == 0) // paranoid
			break;

		csr_write(SCR_CSR_MPU_CTRL, 0);
		csr_write(SCR_CSR_MPU_ADDR, SCR_MPU_MK_ADDR(0));
		csr_write(SCR_CSR_MPU_MASK, ~0UL);
		RISCV_FENCE_I;
	}

	// disable region 0
	scr_mpu_region_disable(0);
}

void scr_mpu_print_info(void)
{
	unsigned long rmax, ctrl, base, mask;
	int rn;

	csr_write(SCR_CSR_MPU_SEL, ~0);
	rmax = csr_read(SCR_CSR_MPU_SEL);
	sbi_printf("\nMPU regions (%lu):\n", rmax + 1);
	for (rn = 0; rn <= rmax; ++rn) {
		csr_write(SCR_CSR_MPU_SEL, rn);
		ctrl = csr_read(SCR_CSR_MPU_CTRL);
		if (ctrl & SCR_MPU_CTRL_VALID) {
			base =  csr_read(SCR_CSR_MPU_ADDR);
			mask =  csr_read(SCR_CSR_MPU_MASK);
#if __riscv_xlen == 32
			sbi_printf("%2d 0x%08lx 0x%lx 0x%lx\n",
#else
			sbi_printf("%2d 0x%014lx 0x%lx 0x%lx\n",
#endif
				   rn, (base << 2), (mask << 2), ctrl);
		}
	}
	sbi_printf("\n");
}

static int scr_fdt_parse_regions(void *fdt, struct sbi_domain_memregion *reset_regions)
{
	const u32 *regions_map;
	uint32_t regn_phandle;
	int cpus_offset, region_offset, reg_len, len, i, regs_num = 0;
	u32 val32;
	u64 val64;
	const u32 *val;
	struct sbi_domain_memregion *regn;

	/* Find /cpus DT node */
	cpus_offset = fdt_path_offset(fdt, "/cpus");
	if (cpus_offset < 0)
		return 0;

	/** get early init regions */
	regions_map = fdt_getprop(fdt, cpus_offset, "scr-mpu-early-init", &reg_len);
	if (!regions_map || reg_len < 8)
		return 0;

	reg_len = reg_len / (sizeof(u32) * 2);
	for (i = 0; i < reg_len; i++) {
		regn = &reset_regions[i];
		regn_phandle = fdt32_to_cpu(regions_map[2 * i]);
		region_offset = fdt_node_offset_by_phandle(fdt, regn_phandle);

		/* Read "base" DT property */
		val = fdt_getprop(fdt, region_offset, "base", &len);
		if (!val && len >= 8)
			return regs_num;

		val64 = fdt32_to_cpu(val[0]);
		val64 = (val64 << 32) | fdt32_to_cpu(val[1]);
		regn->base = val64;

		/* Read "order" DT property */
		val = fdt_getprop(fdt, region_offset, "order", &len);
		if (!val && len >= 4)
			return regs_num;

		val32 = fdt32_to_cpu(*val);
		if (val32 < 3 || __riscv_xlen < val32)
			return regs_num;

		regn->order = val32;
		regn->flags = fdt32_to_cpu(regions_map[2 * i + 1]) & ~(SCR_MPU_DEFINED_FLAGS);

		regs_num++;
	}

	return regs_num;
}

static void scr_mpu_early_init(struct sbi_domain_memregion *reset_regions, int memregs_count)
{
	int idx = 1, i;
	struct sbi_domain_memregion *reg;

	// region 0 is supposed to be set up after reset,
	// but we still leave it here, just in case
	scr_mpu_region_update(0, 0, 0, (SCR_MPU_MMODE_ALL | SCR_MPU_NOCACHE_STRONG_ORDER | SCR_MPU_CTRL_VALID));
	RISCV_FENCE_I;

	for (i = 0; i < memregs_count; i++) {
		reg = &reset_regions[i];

		scr_mpu_region_setup(idx++, reg->base,
				     BIT(reg->order), reg->flags | SCR_MPU_CTRL_VALID);
		RISCV_FENCE_I;
	}

	for (i = idx; i < SCR_MPU_MAX_REGIONS; i++) {
		csr_write(SCR_CSR_MPU_SEL, i);
		if (csr_read(SCR_CSR_MPU_SEL) == 0) // paranoid
			break;

		csr_write(SCR_CSR_MPU_CTRL, 0);
		csr_write(SCR_CSR_MPU_ADDR, SCR_MPU_MK_ADDR(0));
		csr_write(SCR_CSR_MPU_MASK, ~0UL);
		RISCV_FENCE_I;
	}
}

int scr_hart_early_mpu_configure(bool cold_boot, void *fdt)
{
	struct sbi_domain_memregion reset_regions[SCR_MPU_MAX_REGIONS];
	int regs_num = scr_fdt_parse_regions(fdt, reset_regions);

	scr_mpu_early_init(reset_regions, regs_num);

	return 0;
}
