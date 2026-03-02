/*
 * Copyright (C) 2022, Syntacore Ltd.
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

#include <syntacore/scr_mpu.h>

#include <libfdt.h>
#include <sbi/riscv_io.h>
#include <sbi/riscv_asm.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_scratch.h>
#include <sbi/sbi_console.h>
#include <sbi_utils/fdt/fdt_helper.h>

/* MPU CSRs */
#define SCR_CSR_MPU_BASE	0xbc4
#define SCR_CSR_MPU_SEL		(SCR_CSR_MPU_BASE + 0x00)
#define SCR_CSR_MPU_CTRL	(SCR_CSR_MPU_BASE + 0x01)
#define SCR_CSR_MPU_ADDR	(SCR_CSR_MPU_BASE + 0x02)
#define SCR_CSR_MPU_MASK	(SCR_CSR_MPU_BASE + 0x03)

#define SCR_MPU_MK_ADDR(addr)	((addr) >> 2)
#define SCR_MPU_MK_MASK(size)	(~((size) - 1) >> 2)

static volatile long memregs_count;
static struct sbi_domain_memregion reset_regions[SCR_MPU_MAX_REGIONS];

/* Number of regions in nascent init */
#define SCR_MPU_NASCENT_INIT_OFFSET 4

/* Opensbi protect regions definitions */
#define OPENSBI_PROTECT_REG_MRX_FLAGS	(SCR_MPU_MMODE_READ | SCR_MPU_MMODE_EXECUTE)
#define OPENSBI_PROTECT_REG_MRW_FLAGS	(SCR_MPU_MMODE_READ | SCR_MPU_MMODE_WRITE)

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

/* must be called before caches init */
void scr_hart_nascent_mpu_configure_mcfg(unsigned long mcfg_base,
				 unsigned long mcfg_size)
{
	int i;

/**
 * Prevent rewriting of MPU region #0 to SO/NC.
 * Before jumping to OpenSBI from miniboot, cores reset their MPU
 * configuration to have only one entry - #0 set up as WO/C for
 * whole address space. This is done in order to make dram cacheable,
 * (miniboot is not aware of dram actual size, that's why we are
 * using whole address space) because caches are already initialized
 * when cores make their first apperance in OpenSBI. The same principle
 * is applied in scr_hart_nascent_mpu_configure_dram().
 */
	/* very early setup: MCFG region */
	scr_mpu_region_setup(1, mcfg_base, mcfg_size,
				SCR_MPU_NOCACHE_STRONG_ORDER | SCR_MPU_MMIO | SCR_MPU_MMODE_READ |
				SCR_MPU_MMODE_WRITE | SCR_MPU_CTRL_VALID);
	RISCV_FENCE_I;
	/* unused regions */
	for (i = 2; i < SCR_MPU_MAX_REGIONS; i++) {
		csr_write(SCR_CSR_MPU_SEL, i);
		if (csr_read(SCR_CSR_MPU_SEL) == 0) // paranoid
			break;

		csr_write(SCR_CSR_MPU_CTRL, 0);
		csr_write(SCR_CSR_MPU_ADDR, SCR_MPU_MK_ADDR(0));
		csr_write(SCR_CSR_MPU_MASK, ~0UL);
		RISCV_FENCE_I;
	}
}

/* must be called immediately after caches init */
void scr_hart_nascent_mpu_configure_dram(unsigned long dram_base,
				 unsigned long dram_size)
{
	int i;
	struct sbi_scratch *scratch = sbi_scratch_thishart_ptr();
	/* very early setup: Opensbi protect registers and DRAM region */
	scr_mpu_region_setup(2, scratch->fw_start, scratch->fw_rw_offset,
				SCR_MPU_CACHE_WEAK_ORDER |
				OPENSBI_PROTECT_REG_MRX_FLAGS | SCR_MPU_CTRL_VALID);
	RISCV_FENCE_I;

	scr_mpu_region_setup(3, (scratch->fw_start + scratch->fw_rw_offset),
				(scratch->fw_size - scratch->fw_rw_offset),
				SCR_MPU_CACHE_WEAK_ORDER |
				OPENSBI_PROTECT_REG_MRW_FLAGS | SCR_MPU_CTRL_VALID);
	RISCV_FENCE_I;

	scr_mpu_region_setup(4, dram_base, dram_size,
				SCR_MPU_CACHE_WEAK_ORDER |
				SCR_MPU_MODE_ALL | SCR_MPU_CTRL_VALID);
	RISCV_FENCE_I;
	/* unused regions */
	for (i = SCR_MPU_NASCENT_INIT_OFFSET + 1; i < SCR_MPU_MAX_REGIONS; i++) {
		csr_write(SCR_CSR_MPU_SEL, i);
		if (csr_read(SCR_CSR_MPU_SEL) == 0) // paranoid
			break;

		csr_write(SCR_CSR_MPU_CTRL, 0);
		csr_write(SCR_CSR_MPU_ADDR, SCR_MPU_MK_ADDR(0));
		csr_write(SCR_CSR_MPU_MASK, ~0UL);
		RISCV_FENCE_I;
	}
	/* disable region 0 */
	scr_mpu_region_disable(0);
}

static unsigned long pmp_to_mpu_flags(const struct sbi_domain_memregion *reg)
{
	unsigned long flags = 0;

	/* MPU_MMODE_X == (PMP_MMODE_X << 1) */
	if (!(reg->flags & (~SBI_DOMAIN_MEMREGION_M_ACCESS_MASK)))
		return ((reg->flags & SBI_DOMAIN_MEMREGION_M_ACCESS_MASK) << 1);

	if (reg->flags & SBI_DOMAIN_MEMREGION_READABLE)
		flags |= SCR_MPU_READ_ALL;

	if (reg->flags & SBI_DOMAIN_MEMREGION_WRITEABLE)
		flags |= SCR_MPU_WRITE_ALL;

	if (reg->flags & SBI_DOMAIN_MEMREGION_EXECUTABLE)
		flags |= SCR_MPU_EXECUTE_ALL;

	flags |= (reg->flags & SCR_MPU_MMIO);

	return flags;
}

static int scr_fdt_parse_regions(const void *fdt)
{
	const u32 *regions_map;
	uint32_t regn_phandle;
	int tdom_offset, region_offset, reg_len, len, i;
	u32 val32;
	u64 val64;
	const u32 *val;
	struct sbi_domain_memregion *regn;

	/* Find /trusted-domains DT node */
	tdom_offset = fdt_path_offset(fdt, "/chosen/opensbi-domains/trusted-domain");
	if (tdom_offset < 0)
		return tdom_offset;

	/** get early init regions */
	regions_map = fdt_getprop(fdt, tdom_offset, "regions", &reg_len);
	if (!regions_map || reg_len < 8)
		return SBI_EFAIL;

	reg_len = reg_len / (sizeof(u32) * 2);
	for (i = 0; i < reg_len; i++) {
		regn = &reset_regions[i];
		regn_phandle = fdt32_to_cpu(regions_map[2 * i]);
		region_offset = fdt_node_offset_by_phandle(fdt, regn_phandle);

		/* Read "base" DT property */
		val = fdt_getprop(fdt, region_offset, "base", &len);
		if (!val && len >= 8)
			return SBI_EINVAL;

		val64 = fdt32_to_cpu(val[0]);
		val64 = (val64 << 32) | fdt32_to_cpu(val[1]);
		regn->base = (unsigned long)val64;

		/* Read "order" DT property */
		val = fdt_getprop(fdt, region_offset, "order", &len);
		if (!val && len >= 4)
			return SBI_EINVAL;

		val32 = fdt32_to_cpu(*val);
		if (val32 < 3 || __riscv_xlen < val32)
			return SBI_EINVAL;

		regn->order = val32;
		regn->flags = fdt32_to_cpu(regions_map[2 * i + 1]) & ~SCR_MPU_DEFINED_FLAGS;

		memregs_count++;
	}

	return 0;
}

static void scr_mpu_early_init(unsigned long mcfg_addr, unsigned long dram_addr)
{
	int idx = 1, i;
	struct sbi_domain_memregion *reg;
	unsigned long flags;

	for (i = 0; i < memregs_count; i++) {
		reg = &reset_regions[i];
		flags = reg->flags;
		/* PMP to MPU flag conversion */
		if (!(flags & SCR_MPU_DEFINED_FLAGS))
			flags = pmp_to_mpu_flags(reg);
		/* skip if needed */
		if (flags & SCR_MPU_OPENSBI_SKIP)
			continue;
		/* MCFG and DRAM: already inited */
		if ((reg->base == mcfg_addr) || (reg->base == dram_addr))
			continue;
		/* clear SCR_MPU_DEFINED_FLAGS flags */
		flags &= ~(SCR_MPU_DEFINED_FLAGS);
		scr_mpu_region_setup(SCR_MPU_NASCENT_INIT_OFFSET + idx++, reg->base,
				     BIT(reg->order), flags | SCR_MPU_CTRL_VALID);
		RISCV_FENCE_I;
	}

	for (i = SCR_MPU_NASCENT_INIT_OFFSET + idx; i < SCR_MPU_MAX_REGIONS; i++) {
		csr_write(SCR_CSR_MPU_SEL, i);
		if (csr_read(SCR_CSR_MPU_SEL) == 0) // paranoid
			break;

		csr_write(SCR_CSR_MPU_CTRL, 0);
		csr_write(SCR_CSR_MPU_ADDR, SCR_MPU_MK_ADDR(0));
		csr_write(SCR_CSR_MPU_MASK, ~0UL);
		RISCV_FENCE_I;
	}
}

/* must be called in early_init */
int scr_hart_early_mpu_configure(bool cold_boot, const void *fdt,
				 unsigned long mcfg_base, unsigned long dram_base)
{
	int rc;

	if (cold_boot) {
		rc = scr_fdt_parse_regions(fdt);
		if (rc)
			return rc;
	}

	scr_mpu_early_init(mcfg_base, dram_base);

	return 0;
}

void scr_mpu_print_info(void)
{
	unsigned long rmax, ctrl, base, mask;
	int rn;

	csr_write(SCR_CSR_MPU_SEL, ~0);
	rmax = csr_read(SCR_CSR_MPU_SEL);
	sbi_printf("MPU regions (%lu):\n", rmax + 1);
	for (rn = 0; rn <= rmax; ++rn) {
		csr_write(SCR_CSR_MPU_SEL, rn);
		ctrl = csr_read(SCR_CSR_MPU_CTRL);
		if (ctrl & SCR_MPU_CTRL_VALID) {
			base =  csr_read(SCR_CSR_MPU_ADDR);
			mask =  csr_read(SCR_CSR_MPU_MASK);
			sbi_printf("%02d 0x%08lx 0x%016lx 0x%016lx\n", rn, ctrl, base << 2, mask << 2);
		}
	}
}