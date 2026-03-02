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

/*
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

#include <syntacore/scr_mmu.h>

int scr_tlb_miss_trap_handler(const struct sbi_trap_regs *regs)
{
#if __riscv_xlen == 32
	/* sv32 mmu */
	const unsigned long pte1_addr = csr_read(CSR_TLB_PTE1_ADDR);
	const unsigned long pte0_offs = csr_read(CSR_TLB_PTE0_OFFS);
	pgd_t const* pgd = (pgd_t*)pte1_addr;
	pte_t const* pte;

	/* Note: as per SCR5 EAS Appendix, for invalid page entry only
	 * single PAGE_VALID non-zero bit is set in page attrs register
	 */

	/* 4M pages */
	if((pgd->pgd & PAGE_RWX) || !(pgd->pgd & PAGE_VALID)) {
		csr_write(CSR_MMU_PATTR,
			  (pgd->pgd & PAGE_VALID) ? pgd->pgd : PAGE_VALID);
		csr_write(CSR_MMU_UPDATE, CSR_MMU_UPDATE_MPAGE);
		goto done;
	}

	pte = pgd_to_pte(pgd, pte0_offs / sizeof(pte_t));

	/* 4k pages */
	csr_write(CSR_MMU_PATTR,
		  (pte->pte & PAGE_VALID) ? pte->pte : PAGE_VALID);
	csr_write(CSR_MMU_UPDATE, CSR_MMU_UPDATE_4KPAGE);
#else
	/* sv39 mmu */
	const unsigned long pte2_addr = csr_read(CSR_TLB_PTE2_ADDR);
	const unsigned long pte1_offs = csr_read(CSR_TLB_PTE1_OFFS);
	const unsigned long pte0_offs = csr_read(CSR_TLB_PTE0_OFFS);
	pgd_t const* pgd = (pgd_t*)pte2_addr;
	pmd_t const* pmd;
	pte_t const* pte;

	/* Note: as per SCR5 EAS Appendix, for invalid page entry only
	 * single PAGE_VALID non-zero bit is set in page attrs register
	 */

	/* gigapages */
	if((pgd->pgd & PAGE_RWX) || !(pgd->pgd & PAGE_VALID)) {
		csr_write(CSR_MMU_PATTR,
			  (pgd->pgd & PAGE_VALID) ? pgd->pgd : PAGE_VALID);
		csr_write(CSR_MMU_UPDATE, CSR_MMU_UPDATE_GPAGE);
		goto done;
	}

	pmd = pgd_to_pmd(pgd, pte1_offs/sizeof(*pgd));

	/* megapages */
	if((pmd->pmd & PAGE_RWX) || !(pmd->pmd & PAGE_VALID)) {
		csr_write(CSR_MMU_PATTR,
			  (pmd->pmd & PAGE_VALID) ? pmd->pmd : PAGE_VALID);
		csr_write(CSR_MMU_UPDATE, CSR_MMU_UPDATE_MPAGE);
		goto done;
	}

	pte = pmd_to_pte(pmd, pte0_offs/sizeof(*pmd));

	/* 4k pages */
	csr_write(CSR_MMU_PATTR,
		  (pte->pte & PAGE_VALID) ? pte->pte : PAGE_VALID);
	csr_write(CSR_MMU_UPDATE, CSR_MMU_UPDATE_4KPAGE);
#endif
done:
	return SBI_OK;
}
