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

#if !defined(CONFIG_PLATFORM_SYNTACORE_MPU)
#error "Please check MPU/PMP definitions."
#else

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

void scr_hart_early_mpu_configure()
{
	int i;

	/* update default 0 region */
	scr_mpu_region_update(0, 0, 0, SCR_MPU_MMODE_ALL | SCR_MPU_CTRL_VALID);
	RISCV_FENCE_I;

	/* mtimer, l2$ */
	scr_mpu_region_setup(1, MCFG_REGION_BASE, MCFG_REGION_SIZE, \
				SCR_MPU_NOCACHE_STRONG_ORDER | SCR_MPU_MMIO | \
				SCR_MPU_MMODE_RW | SCR_MPU_CTRL_VALID);

	/* mmio region: uart */
	scr_mpu_region_setup(2, MMIO_REGION_BASE, MMIO_REGION_SIZE, \
				SCR_MPU_NOCACHE_STRONG_ORDER | \
				SCR_MPU_MMODE_RW | SCR_MPU_CTRL_VALID);

	/* plic */
	scr_mpu_region_setup(3, PLIC_REGION_BASE, PLIC_REGION_SIZE, \
				SCR_MPU_NOCACHE_STRONG_ORDER | \
				SCR_MPU_MMODE_RW | SCR_MPU_CTRL_VALID);

	RISCV_FENCE_I;

	for (i=4;;i++) {
		csr_write(SCR_CSR_MPU_SEL, i);
		if (csr_read(SCR_CSR_MPU_SEL) == 0)
			break;
		csr_write(SCR_CSR_MPU_CTRL, 0);
		RISCV_FENCE_I;
	}
}
#endif
