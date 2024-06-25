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

#include "scr_cache.h"

#include <libfdt.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_error.h>
#include <sbi_utils/fdt/fdt_helper.h>

#include "asm-magic.h"

static ulong l2_cache_addr = -1;

bool scr_cache_l1_available(void)
{
	return csr_read(SCR_CSR_CACHE_DSCR_L1) != 0;
}

static inline void scr_cache_l1_ctrl(unsigned long val)
{
	if (!scr_cache_l1_available())
		return;

	csr_write(SCR_CSR_CACHE_GLBL, val);
	RISCV_FENCE_I;
}

void scr_cache_l1_disable(void)
{
	if (scr_cache_l1_available()) {
		csr_write(SCR_CSR_CACHE_GLBL, SCR_CACHE_GLBL_DISABLE | SCR_CACHE_GLBL_INV);
		RISCV_FENCE_I;
		// wait until invalidation complete
		while (csr_read(SCR_CSR_CACHE_GLBL) & (SCR_CACHE_GLBL_INV))
			/* not sure if we need a full barrier here */
			mb();
	}
}

bool scr_cache_l1_enabled(void)
{
	if (!scr_cache_l1_available())
		return false;

	return csr_read(SCR_CSR_CACHE_GLBL) &
		(SCR_CACHE_GLBL_L1I_EN | SCR_CACHE_GLBL_L1D_EN);
}

void scr_cache_l1_enable(void)
{
	scr_cache_l1_ctrl(SCR_CACHE_GLBL_ENABLE);
	RISCV_FENCE_I;
}

#define PLF_CACHELINE_SIZE 16

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
		/* Invalidate the cache for the requested range */
		unsigned long a0 = (unsigned long)vaddr;
		unsigned long cnt = size / PLF_CACHELINE_SIZE;

		do {
			asm ("clinvd %0" :: "r"(a0) : "memory");
			a0 += PLF_CACHELINE_SIZE;
		} while (cnt--);
	}
}

void scr_cache_flush(void *vaddr, unsigned long size)
{
	if (size) {
		/* Flush the cache for the requested range */
		unsigned long a0 = (unsigned long)vaddr;
		unsigned long cnt = size / PLF_CACHELINE_SIZE;

		do {
			asm ("clflush %0" :: "r"(a0) : "memory");
			a0 += PLF_CACHELINE_SIZE;
		} while (cnt--);
	}

	/* same here
	 * asm volatile ("fence" ::: "memory");
	 */
	mb();
}

#define SCR_L2_DESCR_BANKS(val)		(((val >> L2_CSR_DESCR_OFFS_BANKS) & \
						L2_CSR_DESCR_MASK_BANKS) + 1)
#define SCR_L2_DESCR_WAYS(val)		(1UL << ((val >> L2_CSR_DESCR_OFFS_WAYS) & \
						L2_CSR_DESCR_MASK_WAYS))
#define SCR_L2_DESCR_LINESZ(val)	(1UL << ((val >> L2_CSR_DESCR_OFFS_LINESZ_LG2) & \
						L2_CSR_DESCR_MASK_LINESZ_LG2))
#define SCR_L2_DESCR_LINES(val)		(1UL << ((val >> L2_CSR_DESCR_OFFS_LINES_LG2) & \
						L2_CSR_DESCR_MASK_LINES_LG2))
#define SCR_L2_DESCR_CORES(val)		(((val >> L2_CSR_DESCR_OFFS_CORES) & \
						L2_CSR_DESCR_MASK_CORES) + 1)

bool scr_l2cache_is_enabled(void)
{
	volatile void *addr;

	if (l2_cache_addr == -1UL)
		return false;

	addr = (void *)l2_cache_addr;
	return !!readl_relaxed(addr + SCR_L2_CACHE_ENABLE);
}

void scr_l2cache_enable(void)
{
	volatile void *addr;
	u32 l2desc, l2ctl;
	uint32_t cbmask;

	if (l2_cache_addr == -1UL)
		return;

	addr = (void *)l2_cache_addr;
	l2ctl = readl_relaxed(addr + SCR_L2_VERSION);
	if (!l2ctl)
		return;

	l2desc = readl_relaxed(addr + SCR_L2_DESCR_0);

	// @note this make no sense to me
	// original from fsbl: cbmask = (1 << (((l2ctl[L2_CSR_DESCR_IDX] >> 16) & 0xf) + 1)) - 1;
	cbmask = (1 << SCR_L2_DESCR_BANKS(l2desc)) - 1;
	// disable
	writel(0, addr + SCR_L2_CACHE_ENABLE);
	// confirm state
	while (readl(addr + SCR_L2_CACHE_ENABLE))
		;
	// invalidate
	writel(cbmask, addr + SCR_L2_CACHE_INVAL);
	// confirm state
	while (readl(addr + SCR_L2_CACHE_INVAL))
		;
	// enable
	writel(cbmask, addr + SCR_L2_CACHE_ENABLE);
	// confirm state
	while (readl(addr + SCR_L2_CACHE_ENABLE) != cbmask)
		;
	RISCV_FENCE_I;
}

void scr_l2cache_disable(void)
{
	volatile void *addr;
	u32 l2ctl;

	if (l2_cache_addr == -1UL)
		return;

	addr = (void *)l2_cache_addr;
	l2ctl = readl_relaxed(addr + SCR_L2_VERSION);
	if (!l2ctl)
		return;

	// disable
	writel_relaxed(0, addr + SCR_L2_CACHE_ENABLE);
	// confirm state
	while (readl_relaxed(addr + SCR_L2_CACHE_ENABLE))
		;
	// flush overall
	writel_relaxed(~0, addr + SCR_L2_CACHE_FLUSH);
	// confirm state
	while (readl_relaxed(addr + SCR_L2_CACHE_FLUSH))
		;
	// invalidate
	writel_relaxed(~0, addr + SCR_L2_CACHE_INVAL);
	// confirm state
	while (readl_relaxed(addr + SCR_L2_CACHE_INVAL))
		;
	RISCV_FENCE_I;
}

void scr_print_l1cache_info(void)
{
	unsigned long ci;
	unsigned int status = csr_read(SCR_CSR_CACHE_GLBL);
	unsigned int info = csr_read(SCR_CSR_CACHE_DSCR_L1);

	ci = (info & SCR_CSR_ICACHE_INFO_MASK);
	if (ci)
		sbi_printf("L1i [%08lx] %s\n", ci,
			   (status & SCR_CACHE_GLBL_L1I_EN) ? "enabled" : "disabled");

	ci = (info & SCR_CSR_DCACHE_INFO_MASK) >> 16;
	if (ci)
		sbi_printf("L1d [%08lx] %s\n", ci,
			   (status & SCR_CACHE_GLBL_L1D_EN) ? "enabled" : "disabled");
}

void scr_print_l2cache_info(void)
{
	unsigned int l2ver, banks, ways, l2dscr;
	unsigned int linesz, lines, cores, size_kb;
	volatile void *addr;

	if (l2_cache_addr == -1UL)
		return;

	addr = (void *)l2_cache_addr;
	l2ver = readl_relaxed(addr + SCR_L2_VERSION);
	if (l2ver) {
		l2dscr = readl_relaxed(addr + SCR_L2_DESCR_0);

		banks = SCR_L2_DESCR_BANKS(l2dscr);
		ways = SCR_L2_DESCR_WAYS(l2dscr);
		linesz = SCR_L2_DESCR_LINESZ(l2dscr);
		lines = SCR_L2_DESCR_LINES(l2dscr);
		cores = SCR_L2_DESCR_CORES(l2dscr);
		size_kb = (lines * linesz * ways * banks) / 1024;
		sbi_printf("L2 [%08x %08x] %uK, %u-way, %u-byte line, %s\n",
			l2ver, l2dscr, size_kb, ways, linesz,
			(cores > 1 ? "shared" : "dedicated"));
		sbi_printf("L2 %u cores, status: %x\n", cores,
			   readl_relaxed(addr + SCR_L2_CACHE_ENABLE));
	}
}

unsigned int scr_l2c_get_cpunum(void)
{
	unsigned int l2ver, l2dscr;
	void *addr;

	if (l2_cache_addr == -1UL)
		return 0;

	addr = (void *)l2_cache_addr;

	/* Check if L2 Cache exists */
	l2ver = readl_relaxed(addr + SCR_L2_VERSION);
	if (!l2ver)
		return 0;

	l2dscr = readl_relaxed(addr + SCR_L2_DESCR_0);

	return SCR_L2_DESCR_CORES(l2dscr);
}

static const struct fdt_match scr_l2_cache_match[] = {
	{ .compatible = "syntacore,l2-cache" },
	{ },
};

int scr_fdt_l2_cache_init(void *fdt)
{
	int nodeoff = -1;
	const struct fdt_match *match;
	uint64_t reg_addr, reg_size;
	int rc;

	nodeoff = fdt_find_match(fdt, 0, scr_l2_cache_match, &match);
	if (nodeoff < 0)
		return SBI_ENODEV;

	rc = fdt_get_node_addr_size(fdt, nodeoff, 0,
				    &reg_addr, &reg_size);
	if (rc < 0 || !reg_size)
		return SBI_ENODEV;

	l2_cache_addr = (ulong)reg_addr;

	return 0;
};
