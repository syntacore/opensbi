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

#include <syntacore/scr_cache.h>
#include <libfdt.h>
#include <sbi/sbi_error.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_types.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_platform.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <syntacore/scr_cache_drv.h>
#include <syntacore/scr_fdt_helper.h>
#include <syntacore/scr_l2_pmu.h>
#include <syntacore/scr_l3_pmu.h>
#include <syntacore/sbi_vendor.h>

// cache control CSRs
#define SCR_CSR_CACHE_GLBL		0xbd4

// cache info CSRs
#define SCR_CSR_CACHE_DSCR_L1		0xfc3

#define SCR_CSR_ICACHE_INFO_MASK	0x000ffff
#define SCR_CSR_DCACHE_INFO_MASK	0xfff0000

#define SCR_CSR_DCACHE_INFO_OFFS	16

#define SCR_CSR_CACHE_WAYS_MASK		(0x7)
#define SCR_CSR_CACHE_LINESZ_MASK	(0xf0)
#define SCR_CSR_CACHE_WIDTH_MASK	(0x1f00)

// global cache's control bits
#define SCR_CACHE_GLBL_L1I_EN		(_UL(1) << 0)
#define SCR_CACHE_GLBL_L1D_EN		(_UL(1) << 1)
#define SCR_CACHE_GLBL_L1I_INV		(_UL(1) << 2)
#define SCR_CACHE_GLBL_L1D_INV		(_UL(1) << 3)
#define SCR_CACHE_GLBL_ENABLE		(SCR_CACHE_GLBL_L1I_EN | SCR_CACHE_GLBL_L1D_EN)
#define SCR_CACHE_GLBL_DISABLE		_UL(0)
#define SCR_CACHE_GLBL_INV		(SCR_CACHE_GLBL_L1I_INV | SCR_CACHE_GLBL_L1D_INV)

static bool l1c_available(void)
{
	return csr_read(SCR_CSR_CACHE_DSCR_L1) != 0;
}

static inline void l1_ctrl(unsigned long val)
{
	if (!l1c_available())
		return;

	csr_write(SCR_CSR_CACHE_GLBL, val);
	RISCV_FENCE_I;
}

static void l1_parse_descr(unsigned int descr, struct cache_info *l1_info)
{
	unsigned int ways = 1 << EXTRACT_FIELD(descr, SCR_CSR_CACHE_WAYS_MASK);
	unsigned int indexes = 1 << EXTRACT_FIELD(descr, SCR_CSR_CACHE_WIDTH_MASK);

	l1_info->block_size = 1 << EXTRACT_FIELD(descr, SCR_CSR_CACHE_LINESZ_MASK);
	l1_info->size = indexes * l1_info->block_size * ways;
	l1_info->sets = indexes;
}

void scr_l1c_inval(void)
{
	if (l1c_available()) {
		csr_write(SCR_CSR_CACHE_GLBL,csr_read(SCR_CSR_CACHE_GLBL) |  SCR_CACHE_GLBL_INV);
		RISCV_FENCE_I;
		// wait until invalidation complete
		while (csr_read(SCR_CSR_CACHE_GLBL) & (SCR_CACHE_GLBL_INV))
			/* not sure if we need a full barrier here */
			mb();
	}
}

void scr_l1c_disable(void)
{
	if (l1c_available()) {
		csr_write(SCR_CSR_CACHE_GLBL, SCR_CACHE_GLBL_DISABLE | SCR_CACHE_GLBL_INV);
		RISCV_FENCE_I;
		// wait until invalidation complete
		while (csr_read(SCR_CSR_CACHE_GLBL) & (SCR_CACHE_GLBL_INV))
			/* not sure if we need a full barrier here */
			mb();
	}
}

void scr_l1c_enable(void)
{
	l1_ctrl(SCR_CACHE_GLBL_ENABLE);
	RISCV_FENCE_I;
}

void scr_l1c_info(void)
{
	unsigned long ci;
	unsigned int status = csr_read(SCR_CSR_CACHE_GLBL);
	unsigned int info = csr_read(SCR_CSR_CACHE_DSCR_L1);

	ci = (info & SCR_CSR_ICACHE_INFO_MASK);
	if (ci)
		sbi_printf("L1i [%08lx] %s\n", ci,
			   (status & SCR_CACHE_GLBL_L1I_EN) ? "enabled" : "disabled");

	ci = (info & SCR_CSR_DCACHE_INFO_MASK) >> SCR_CSR_DCACHE_INFO_OFFS;
	if (ci)
		sbi_printf("L1d [%08lx] %s\n", ci,
			   (status & SCR_CACHE_GLBL_L1D_EN) ? "enabled" : "disabled");
}

void scr_get_cache_l1d_info (struct cache_info *l1d_info)
{
	unsigned int l1dscr = EXTRACT_FIELD(csr_read(SCR_CSR_CACHE_DSCR_L1), SCR_CSR_DCACHE_INFO_MASK);

	l1_parse_descr(l1dscr, l1d_info);
}

void scr_get_cache_l1i_info (struct cache_info *l1i_info)
{
	unsigned int l1dscr = EXTRACT_FIELD(csr_read(SCR_CSR_CACHE_DSCR_L1), SCR_CSR_ICACHE_INFO_MASK);

	l1_parse_descr(l1dscr, l1i_info);
}

/************** L2 & L3 Cache ****************/

#define CALL_CB(drv, cb, ...)   \
		{                       \
			if ((drv)->cb)      \
				(drv)->cb(drv, ##__VA_ARGS__); \
		}

enum scr_cache_type {
	SCR_PRIVATE_CACHE = 0,
	SCR_GENERIC_CACHE,
};

struct scr_cache
{
	struct scr_cache_drv *drv;
	enum scr_cache_type   type;
};

struct scr_cache_descr
{
	scr_cache_probe_t     probe;
	enum scr_cache_type   type;
};

static struct scr_cache l2;
static struct scr_cache l3;

static unsigned long scr_cache_pmu_match(struct scr_cache *cache,
		unsigned long mask, unsigned long flags, unsigned long event_idx,
		unsigned long event_data, unsigned long *out)
{
	unsigned long cnt_idx;
	bool auto_start;
	bool clear;
	int ret;

	if (!cache->drv->pmu_get_cntrs ||
			!cache->drv->pmu_get_free_cntr ||
			!cache->drv->pmu_assign_cntr ||
			!cache->drv->pmu_enable_cntr)
		return SBI_ENOTSUPP;


	if (sbi_fls(mask) >= cache->drv->pmu_get_cntrs(cache->drv))
		return SBI_EINVAL;

	if (flags & SBI_PMU_CFG_FLAG_SKIP_MATCH) {
		cnt_idx = sbi_ffs(mask);
	} else {
		ret = cache->drv->pmu_get_free_cntr(cache->drv, mask);
		if (ret < 0)
			return ret;

		cnt_idx = ret;
	}

	ret = cache->drv->pmu_assign_cntr(cache->drv,
			cnt_idx, event_idx, event_data);
	if (ret)
		return ret;

	clear = !!(flags & SBI_PMU_CFG_FLAG_CLEAR_VALUE);
	auto_start = !!(flags & SBI_PMU_CFG_FLAG_AUTO_START);

	ret = cache->drv->pmu_enable_cntr(cache->drv,
			cnt_idx, auto_start, clear, 0u);
	*out = cnt_idx;

	return ret;
}

static unsigned long scr_cache_pmu_start(struct scr_cache *cache,
					  unsigned long mask, unsigned long flags, uint64_t init)
{
	unsigned long idx = 0u;
	unsigned long cntrs;
	int ret = SBI_EINVAL;
	bool update = false;

	if (!cache->drv->pmu_get_cntrs ||
			!cache->drv->pmu_enable_cntr)
		return SBI_ENOTSUPP;

	cntrs = cache->drv->pmu_get_cntrs(cache->drv);

	if (sbi_fls(mask) >= cntrs)
		return ret;

	update = !!(flags & SBI_PMU_START_FLAG_SET_INIT_VALUE);

	for_each_set_bit_from(idx, &mask, cntrs) {
		ret = cache->drv->pmu_enable_cntr(cache->drv, idx, true, update, init);
		if (ret) {
			if (ret == SBI_EINVAL)
				/* Continue the start operation for other counters */
				continue;

			break;
		}
	}

	return ret;
}

static int scr_cache_pmu_stop(struct scr_cache *cache,
		unsigned long mask, unsigned long flags)
{
	unsigned long idx = 0u;
	unsigned long cntrs;
	bool reset;
	int ret;

	if (!cache->drv->pmu_get_cntrs ||
			!cache->drv->pmu_free_cntr)
		return SBI_ENOTSUPP;

	cntrs = cache->drv->pmu_get_cntrs(cache->drv);

	if (sbi_fls(mask) >= cntrs)
		return SBI_EINVAL;

	for_each_set_bit_from(idx, &mask, cntrs) {
		reset = !!(flags & SBI_PMU_STOP_FLAG_RESET);
		ret = cache->drv->pmu_enable_cntr(cache->drv,
				idx, false, reset, 0u);
		if (ret) {
			if (ret == SBI_EINVAL)
				/* Continue the stop operation for other counters */
				continue;

			return ret;
		}

		if (reset)
			ret = cache->drv->pmu_free_cntr(cache->drv, idx);

		if (ret)
			return ret;
	}

	return ret;
}

static int scr_cache_pmu_read_cntr(struct scr_cache *cache,
		unsigned long cidx, uint64_t *out)
{
	if (!cache->drv->pmu_get_cntrs)
		return SBI_ENOTSUPP;

	if (cidx >= cache->drv->pmu_get_cntrs(cache->drv))
		return SBI_EINVAL;

	return cache->drv->pmu_read_cntr(cache->drv, cidx, out);
}

static int scr_cache_pmu_ext_provider(struct scr_cache *cache,
			struct sbi_trap_regs *regs,	struct sbi_ecall_return *out)
{
	int ret = SBI_OK;
	unsigned long id = regs->a0;
	unsigned long cmask = regs->a2;
	unsigned long cbase = regs->a1;
	unsigned long mask = cmask << cbase;
	uint64_t tmp;

	if (!cache->drv)
		return SBI_ENOTSUPP;

	switch (id) {
	case SBI_EXT_SCR_PMU_PROBE:
		out->value = cache->drv->llc ? 0u : SCR_PMU_DEDICATED_FLAG;
		break;
	case SBI_EXT_PMU_NUM_COUNTERS:
		if (cache->drv->pmu_get_cntrs)
			out->value = cache->drv->pmu_get_cntrs(cache->drv);
		else
			ret = SBI_ENOTSUPP;
		break;
	case SBI_EXT_PMU_COUNTER_CFG_MATCH:
		ret = scr_cache_pmu_match(cache, mask, regs->a3, regs->a4, regs->a5, &out->value);
		break;
	case SBI_EXT_PMU_COUNTER_START:
#if __riscv_xlen == 32
		tmp = ((uint64_t)regs->a5 << 32) | regs->a4;
#else
		tmp = regs->a4;
#endif
		ret = scr_cache_pmu_start(cache, mask, regs->a3, tmp);
		break;
	case SBI_EXT_PMU_COUNTER_STOP:
		ret = scr_cache_pmu_stop(cache, mask, regs->a3);
		break;
	case SBI_EXT_SCR_PMU_COUNTER_HW_READ:
		ret = scr_cache_pmu_read_cntr(cache, regs->a1, &tmp);
		out->value = tmp;
		break;
	case SBI_EXT_SCR_PMU_VID:
		if (cache->drv->get_version)
			out->value = cache->drv->get_version(cache->drv);
		else
			ret = SBI_ENOTSUPP;
		break;
	default:
		ret = SBI_ENOTSUPP;
		break;
	}

	return ret;
}

void scr_l2c_info(void)
{
	CALL_CB(l2.drv, print_info);
}

int scr_l2c_domain_init(void)
{
	if (l2.drv->domain_init)
		return l2.drv->domain_init(l2.drv);

	return SBI_OK;
}

void scr_l2c_set_llc(bool llc)
{
	l2.drv->llc = llc;
}

unsigned long scr_l2c_get_version(void)
{
	if (l2.drv->get_version)
		return l2.drv->get_version(l2.drv);
	return 0u;
}

void scr_l2c_enable(u32 flags)
{
	CALL_CB(l2.drv, enable, flags);
}

void scr_l2c_disable(u32 flags)
{
	CALL_CB(l2.drv, disable, flags);
}

void scr_get_cache_l2_info (struct cache_info *l2_info)
{
	CALL_CB(l2.drv, get_info, l2_info);
}

int scr_l2_pmu_ext_provider(struct sbi_trap_regs *regs,
			struct sbi_ecall_return *out)
{
	return scr_cache_pmu_ext_provider(&l2, regs, out);
}

void scr_l3c_info(void)
{
	CALL_CB(l3.drv, print_info);
}

int scr_l3c_domain_init(void)
{
	if (l3.drv->domain_init)
		return l3.drv->domain_init(l3.drv);

	return SBI_OK;
}

unsigned long scr_l3c_get_version(void)
{
	if (l3.drv->get_version)
		return l3.drv->get_version(l3.drv);
	return 0u;
}

void scr_l3c_inval(void)
{
	CALL_CB(l3.drv, inval);
}

void scr_get_cache_l3_info (struct cache_info *l3_info)
{
	CALL_CB(l3.drv, get_info, l3_info);
}

int scr_l3_pmu_ext_provider(struct sbi_trap_regs *regs,
			struct sbi_ecall_return *out)
{
	return scr_cache_pmu_ext_provider(&l3, regs, out);
}

static int scr_cache_probe(const void *fdt, const struct fdt_match *match_tbl,
		struct scr_cache *cache, int level)
{
	const struct fdt_match *match;
	const struct scr_cache_descr *descr;
	uint64_t addr, size;

	match = scr_fdt_find_cache(fdt, match_tbl, &addr, &size);
	if (!match)
		return SBI_ENOTSUPP;

	descr = (struct scr_cache_descr *)match->data;

	cache->type = descr->type;
	cache->drv = descr->probe(addr, size, level);

	return cache->drv ? SBI_OK : SBI_ENOTSUPP;
}

static const struct scr_cache_descr l2_cache_private =
{
	.probe = scr_l2_pcache_probe,
	.type  = SCR_PRIVATE_CACHE,
};

static const struct scr_cache_descr l3_cache_private = {
	.probe = scr_l3_pcache_probe,
	.type  = SCR_PRIVATE_CACHE,
};

static const struct scr_cache_descr cache_generic =
{
	.probe = scr_gcache_probe,
	.type  = SCR_GENERIC_CACHE,
};

static struct fdt_match scr_cache_l2_match[] = {
	{ .compatible = "syntacore,l2-generic-cache", .data = &cache_generic },
	{ .compatible = "syntacore,l2-cache", .data = &l2_cache_private },
	{ },
};

static struct fdt_match scr_cache_l3_match[] = {
	{ .compatible = "syntacore,l3-generic-cache", .data = &cache_generic },
	{ .compatible = "syntacore,l3-cache", .data = &l3_cache_private },
	{ },
};

int scr_l2c_probe(const void *fdt)
{
	return scr_cache_probe(fdt, scr_cache_l2_match, &l2, L2_CACHE_LEVEL);
}

int scr_l3c_probe(const void *fdt)
{
	return scr_cache_probe(fdt, scr_cache_l3_match, &l3, L3_CACHE_LEVEL);
}

/************** Common cache ops ***************/

#define PLF_CACHELINE_SIZE 16

/* Syntacore cache instructions */
#define SCR_CLINV(regn)   (0x10800073 | (((regn) & 0x1f) << 15))
#define SCR_CLFLUSH(regn) (0x10900073 | (((regn) & 0x1f) << 15))

/* ZICBOM cache instructions */
#define RV_OPCODE(v)        __ASM_STR(v)
#define RV_FUNC3(v)         __ASM_STR(v)
#define RV_SIMM12(v)        __ASM_STR(v)
#define __RV_REG(v)         __ASM_STR(x ## v)
#define RV_RD(v)            __ASM_STR(v)
#define RV_RS1(v)           __ASM_STR(v)
#define RV___RD(v)          __RV_REG(v)

#define RV_OPCODE_MISC_MEM  RV_OPCODE(15)

#define __INSN_I(opcode, func3, rd, rs1, simm12)		\
	".insn	i " opcode ", " func3 ", " rd ", " rs1 ", " simm12

#define INSN_I(opcode, func3, rd, rs1, simm12)			\
	__INSN_I(RV_##opcode, RV_##func3, RV_##rd,		\
		 RV_##rs1, RV_##simm12)

#define CBO_INVAL(base)					\
	INSN_I(OPCODE_MISC_MEM, FUNC3(2), __RD(0),		\
	       RS1(base), SIMM12(0))

#define CBO_FLUSH(base)					\
	INSN_I(OPCODE_MISC_MEM, FUNC3(2), __RD(0),		\
	       RS1(base), SIMM12(2))

static struct cache_ops *cache_ops;

static void xsyntacore_flush_block(uintptr_t vaddr)
{
	register uintptr_t a0 asm("a0") = vaddr;
	__asm__ __volatile__ (".word %0" :: "i"(SCR_CLFLUSH(10)), "r"(a0) : "memory");
}

static void xsyntacore_inv_block(uintptr_t vaddr)
{
	register uintptr_t a0 asm("a0") = vaddr;
	__asm__ __volatile__ (".word %0" :: "i"(SCR_CLINV(10)), "r"(a0) : "memory");
}

static void cbo_flush_block(uintptr_t vaddr)
{
	__asm__ __volatile__ (CBO_FLUSH(%0) :: "r"(vaddr) : "memory");
}

static void cbo_inv_block(uintptr_t vaddr)
{
	__asm__ __volatile__ (CBO_INVAL(%0) :: "r"(vaddr) : "memory");
}

static bool validate_cache_ops(const struct cache_ops *ops)
{
	return ops && ops->cbom_block_size;
}

void scr_cache_invalidate(void *vaddr, unsigned long size)
{
	/* in fsbl/common/cache.h the following is used:
	 * asm volatile ("fence" ::: "memory");
	 * which translates to fence iorw, iorw,
	 * i am not sure if all of this is required,
	 * or we can simply do smp_mb, i.e.
	 * fence rw, rw
	 */
	mb();

	if (size) {
		if (validate_cache_ops(cache_ops) && cache_ops->inv_block) {
			/* Invalidate the cache for the requested range */
			unsigned long cnt = size / cache_ops->cbom_block_size;
			uintptr_t p = (uintptr_t)vaddr;

			do {
				cache_ops->inv_block(p);
				p += cache_ops->cbom_block_size;
			} while (cnt--);
		} else {
			sbi_printf("Error! Cache OPS are not initialized\n");
			sbi_hart_hang();
		}
	}
}

void scr_cache_flush(void *vaddr, unsigned long size)
{
	if (size) {
		if (validate_cache_ops(cache_ops) && cache_ops->flush_block) {
			/* Flush the cache for the requested range */
			unsigned long cnt = size / cache_ops->cbom_block_size;
			uintptr_t p = (uintptr_t)vaddr;

			do {
				cache_ops->flush_block(p);
				p += cache_ops->cbom_block_size;
			} while (cnt--);
		} else {
			sbi_printf("Error! Cache OPS are not initialized\n");
			sbi_hart_hang();
		}
	}

	/* same here
	 * asm volatile ("fence" ::: "memory");
	 */
	mb();
}

static struct cache_ops xsyntacore_ops = {
		.cbom_block_size = PLF_CACHELINE_SIZE,
		.type = XSYNTACORE_CACHE_OPS,
		.flush_block = xsyntacore_flush_block,
		.inv_block = xsyntacore_inv_block,
};

static struct cache_ops cbo_ops = {
		.cbom_block_size = 0, /* receive from dtb */
		.type = CBO_CACHE_OPS,
		.flush_block = cbo_flush_block,
		.inv_block = cbo_inv_block,
};

void scr_cache_ops_init(void)
{
	struct sbi_scratch *scratch = sbi_scratch_thishart_ptr();
	const struct sbi_platform *plat = sbi_platform_thishart_ptr();

	if (sbi_hart_has_extension(scratch, SBI_HART_EXT_ZICBOM)
			&& plat->cbom_block_size) {
		cbo_ops.cbom_block_size = plat->cbom_block_size;
		cache_ops = &cbo_ops;
	} else
		cache_ops = &xsyntacore_ops;
}

/* Used to flush stack, so implemented in asm */
asm (	".section .text							\n"
	".align 3							\n"
	".local	scr_cache_flush_asm					\n"
	"scr_cache_flush_asm:						\n"
	"beqz		a1, 2f						\n"
	"1:								\n"
	".word		" STRINGIFY(SCR_CLFLUSH(10)) "			\n"
	"addi		a0, a0, " STRINGIFY(PLF_CACHELINE_SIZE) "	\n"
	"addi		a1, a1, -" STRINGIFY(PLF_CACHELINE_SIZE) "	\n"
	"bnez		a1, 1b						\n"
	"fence								\n"
	"2:								\n"
	"ret								\n"
	);

asm (
	".section .text							\n"
	".align 3							\n"
	".local	zicbom_cache_flush_asm					\n"
	"zicbom_cache_flush_asm:					\n"
	"beqz		a1, 2f						\n"
	"1:								\n"
	CBO_FLUSH(a0) "							\n"
	"add		a0, a0, a2					\n"
	"sub		a1, a1, a2					\n"
	"bnez		a1, 1b						\n"
	"fence								\n"
	"2:								\n"
	"ret								\n"
	);

asm (".macro l1c_dis							\n"
	"li		t0, (" STRINGIFY(SCR_CACHE_GLBL_L1I_INV) " | "
			       STRINGIFY(SCR_CACHE_GLBL_L1D_INV)")	\n"
	"csrw		" STRINGIFY(SCR_CSR_CACHE_GLBL) ", t0		\n"
	"1:								\n"
	"fence.i							\n"
	"csrr		t0, " STRINGIFY(SCR_CSR_CACHE_GLBL) "		\n"
	"andi		t0, t0, (" STRINGIFY(SCR_CACHE_GLBL_L1I_INV) " | "
				   STRINGIFY(SCR_CACHE_GLBL_L1D_INV)")	\n"
	"bnez		t0, 1b						\n"
	".endm								\n"
	);


/*
 * Flush stack area, disable local caches & suspend.
 * No stack access is allowed after flush.
 */
void __noreturn scr9_flush_suspend(unsigned long stack_addr,
				   unsigned long stack_size)
{
/* Move under __riscv_xlen != 32 because of GC */
#if __riscv_xlen != 32
	unsigned long size = ROUNDUP(stack_size, cache_ops->cbom_block_size);

	if (!validate_cache_ops(cache_ops))
		sbi_panic("%s: Cache ops are not set", __func__);

#define FLUSH_SUSPEND(ops, cache) \
	({ \
		register ulong a0 asm ("a0") = (ulong)stack_addr;          \
		register ulong a1 asm ("a1") = size;                       \
		register ulong a2 asm ("a2") = cache_ops->cbom_block_size; \
		\
		__asm__ __volatile__ (    \
				"call " __ASM_STR(ops ## _cache_flush_asm) "\n" \
				"l1c_dis                        \n" \
				"call " __ASM_STR(scr_l2_ ## cache ## _disable_asm) "\n" \
				"1:                             \n" \
				"wfi                            \n" \
				"j 1b                           \n" \
				: "+&r"(a0), "+&r" (a1), "+&r" (a2) \
				:                                   \
				: "memory", "t0", "t1",             \
				  "t2", "t3", "ra");                \
	})

	if ((l2.type == SCR_PRIVATE_CACHE) &&
			(cache_ops->type == XSYNTACORE_CACHE_OPS)) {
		FLUSH_SUSPEND(scr, pcache);
		__builtin_unreachable();
	} else if ((l2.type == SCR_GENERIC_CACHE) &&
			(cache_ops->type == XSYNTACORE_CACHE_OPS)) {
		FLUSH_SUSPEND(scr, gcache);
		__builtin_unreachable();
	} else if ((l2.type == SCR_PRIVATE_CACHE) &&
			(cache_ops->type == CBO_CACHE_OPS)) {
		FLUSH_SUSPEND(zicbom, pcache);
		__builtin_unreachable();
	} else {
		FLUSH_SUSPEND(zicbom, gcache);
		__builtin_unreachable();
	}

#undef FLUSH_SUSPEND
#else
	sbi_hart_hang();
#endif
}

void __noreturn scr_cache_core_suspend(void)
{
#if __riscv_xlen != 32
	scr_l1c_disable();
	scr_l2c_disable(0);

	while (1)
		wfi();

	__builtin_unreachable();
#else
	sbi_hart_hang();
#endif
}
