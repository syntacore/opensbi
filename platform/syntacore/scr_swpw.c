/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) Syntacore 2022. All rights reserved.
 *
 * Software Pagewalker (TLB miss trap handler) for sv32/sv39 MMUs
 *
 * Called from lib/sbi/sbi_trap.c:sbi_trap_handler() routine
 * for Syntacore specific TLB_MISS exception (n=14)
 */
#include <sbi/riscv_io.h>
#include <sbi/riscv_asm.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_error.h>

#include "scr_mmu.h"

int scr_tlb_miss_trap_handler(struct sbi_trap_regs *regs)
{
#if __riscv_xlen == 32
	/* sv32 mmu */
	const unsigned long pte1_addr = csr_read(CSR_TLB_PTE1_ADDR);
	const unsigned long pte0_offs = csr_read(CSR_TLB_PTE0_OFFS);
	pgd_t const* pgd = (pgd_t*)pte1_addr;
	pte_t const* pte;

	if (!(pgd->pgd & PAGE_VALID))
		return SBI_EINVALID_ADDR;

	/* 4M pages */
	if (pgd->pgd & PAGE_RWX) {
		csr_write(CSR_MMU_PATTR, pgd->pgd);
		csr_write(CSR_MMU_UPDATE, CSR_MMU_UPDATE_MPAGE);
		goto done;
	}

	pte = pgd_to_pte(pgd, pte0_offs / sizeof(pte_t));

	if (!(pte->pte & PAGE_VALID))
		return SBI_EINVALID_ADDR;

	/* 4k pages */
	csr_write(CSR_MMU_PATTR, pte->pte);
	csr_write(CSR_MMU_UPDATE, CSR_MMU_UPDATE_4KPAGE);
#else
	/* sv39 mmu */
	const unsigned long pte2_addr = csr_read(CSR_TLB_PTE2_ADDR);
	const unsigned long pte1_offs = csr_read(CSR_TLB_PTE1_OFFS);
	const unsigned long pte0_offs = csr_read(CSR_TLB_PTE0_OFFS);
	pgd_t const* pgd = (pgd_t*)pte2_addr;
	pmd_t const* pmd;
	pte_t const* pte;

	if (!(pgd->pgd & PAGE_VALID))
		return SBI_EINVALID_ADDR;

	/* gigapages */
	if (pgd->pgd & PAGE_RWX) {
		csr_write(CSR_MMU_PATTR, pgd->pgd);
		csr_write(CSR_MMU_UPDATE, CSR_MMU_UPDATE_GPAGE);
		goto done;
	}

	pmd = pgd_to_pmd(pgd, pte1_offs/sizeof(*pgd));

	if (!(pmd->pmd & PAGE_VALID))
		return SBI_EINVALID_ADDR;

	/* megapages */
	if (pmd->pmd & PAGE_RWX) {
		csr_write(CSR_MMU_PATTR, pmd->pmd);
		csr_write(CSR_MMU_UPDATE, CSR_MMU_UPDATE_MPAGE);
		goto done;
	}

	pte = pmd_to_pte(pmd, pte0_offs/sizeof(*pmd));

	if (!(pte->pte & PAGE_VALID))
		return SBI_EINVALID_ADDR;

	/* 4k pages */
	csr_write(CSR_MMU_PATTR, pte->pte);
	csr_write(CSR_MMU_UPDATE, CSR_MMU_UPDATE_4KPAGE);
#endif
done:
	return SBI_OK;
}
