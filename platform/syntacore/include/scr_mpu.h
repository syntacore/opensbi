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

#ifndef _SCR_MPU_H_
#define _SCR_MPU_H_

#include "dt-bindings/scr_mpu.h"

#include <sbi/sbi_types.h>

/* Early MPU init regions */
#if __riscv_xlen == 32
# define MCFG_REGION_BASE 0xf0040000UL
# define MMIO_REGION_BASE 0xff000000UL
# define PLIC_REGION_BASE 0xfe000000UL
#elif __riscv_xlen == 64
# define MCFG_REGION_BASE 0xfffffff0040000UL
# define MMIO_REGION_BASE 0xffffffff000000UL
# define PLIC_REGION_BASE 0xfffffffe000000UL
#endif
#define MCFG_REGION_SIZE 8192
#define MMIO_REGION_SIZE 0x800000
#define PLIC_REGION_SIZE 0x1000000

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

void scr_hart_early_mpu_configure(void);
void scr_hart_mpu_configure(void *fdt);

void scr_mpu_print_info(void);

#endif // _SCR_MPU_H_
