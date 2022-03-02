/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Syntacore
 *
 * derivate: fsbl/src/mmu.h
 * Authors:
 *   Mikhail Nefedov <mikhail.nefedov@syntacore.com>
 */
#ifndef _SCR_MPU_H_
#define _SCR_MPU_H_

#include "dt-bindings/scr_mpu.h"

#include <sbi/sbi_types.h>

/* MPU CSRs */
#define SCR_CSR_MPU_BASE	0xbc4
#define SCR_CSR_MPU_SEL		(SCR_CSR_MPU_BASE + 0x00)
#define SCR_CSR_MPU_CTRL	(SCR_CSR_MPU_BASE + 0x01)
#define SCR_CSR_MPU_ADDR	(SCR_CSR_MPU_BASE + 0x02)
#define SCR_CSR_MPU_MASK	(SCR_CSR_MPU_BASE + 0x03)

/* MPU address/mask conversion macros (with sign extension) */
#define SCR_MPU_MK_ADDR32(addr) \
	((((addr) / 4) | ((addr) & 0x80000000) | \
	((addr) & 0x80000000) / 2) & 0xffffffff)
#define SCR_MPU_MK_MASK32(size)	((~((size) - 1) / 4) & 0xffffffff)

#define SCR_MPU_MK_ADDR64(addr)	((addr) >> 2)
#define SCR_MPU_MK_MASK64(size)	(~((size) - 1) >> 2)

#if __riscv_xlen == 32
#define SCR_MPU_MK_ADDR(addr)	SCR_MPU_MK_ADDR32(addr)
#define SCR_MPU_MK_MASK(size)	SCR_MPU_MK_MASK32(size)
#elif __riscv_xlen == 64
#define SCR_MPU_MK_ADDR(addr)	SCR_MPU_MK_ADDR64(addr)
#define SCR_MPU_MK_MASK(size)	SCR_MPU_MK_MASK64(size)
#else
#error MPU addr/mask conversion is not implemented
#endif

#define SCR_MPU_REGION_ALIGN	0x1000
#define SCR_MPU_MAX_REGIONS	16

void scr_hart_early_mpu_setup_mcfg(unsigned long mcfg_base, unsigned long mcfg_size);
int scr_hart_early_mpu_configure(bool cold_init, void *fdt);
void scr_hart_mpu_configure(void *fdt);

void scr_mpu_print_info(void);

#endif // _SCR_MPU_H_
