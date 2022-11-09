/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) Syntacore 2022. All rights reserved.
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

#define PAGE_VALID		(1 << 0)
#define PAGE_READ		(1 << 1)
#define PAGE_WRITE		(1 << 2)
#define PAGE_EXEC		(1 << 3)
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
