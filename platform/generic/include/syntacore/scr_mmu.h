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

#ifndef _SCR_MMU_H_
#define _SCR_MMU_H_

#define CSR_MMU_BASE		0xbc0
#define CSR_MMU_PATTR		(CSR_MMU_BASE + 0)
#define CSR_MMU_VADDR		(CSR_MMU_BASE + 1)
#define CSR_MMU_UPDATE		(CSR_MMU_BASE + 2)
#define CSR_MMU_SCAN		(CSR_MMU_BASE + 3)

#define CSR_MMU_UPDATE_4KPAGE	(0UL << 2)
#define CSR_MMU_UPDATE_MPAGE	(1UL << 2)

#if __riscv_xlen == 32
# define CSR_TLB_PTE1_ADDR	0xfd0
# define CSR_TLB_PTE0_OFFS	0xfd1
# define pgd_to_pte(t, i)	(pte_t *)pgd_to_pmd(t, i)
#else
# define CSR_TLB_PTE2_ADDR	0xfd0
# define CSR_TLB_PTE1_OFFS	0xfd1
# define CSR_TLB_PTE0_OFFS	0xfd2
# define CSR_MMU_UPDATE_GPAGE	(2UL << 2)
# define CSR_MMU_UPDATE_TPAGE	(3UL << 2)
#endif

#define PAGE_VALID		(1UL << 0)
#define PAGE_READ		(1UL << 1)
#define PAGE_WRITE		(1UL << 2)
#define PAGE_EXEC		(1UL << 3)
#define PAGE_RWX		(PAGE_READ | PAGE_WRITE | PAGE_EXEC)

#define PPN0_SHIFT		12
#define PPN0_MASK		0x1ff
#define PPN0_PTE_SHIFT		10

typedef struct {
	unsigned long pgd;
} pgd_t;

typedef struct {
	unsigned long pmd;
} pmd_t;

typedef struct {
	unsigned long pte;
} pte_t;

static inline pmd_t* pgd_to_pmd(pgd_t const* pgd, unsigned long idx)
{
	return (pmd_t*)(((pgd->pgd >> PPN0_PTE_SHIFT) << PAGE_SHIFT) + \
	       idx * sizeof(*pgd));
}

static inline pte_t* pmd_to_pte(pmd_t const* pmd, unsigned long idx)
{
	return (pte_t*)(((pmd->pmd >> PPN0_PTE_SHIFT) << PAGE_SHIFT) + \
	       idx * sizeof(*pmd));
}

#endif // _SCR_MMU_H_
