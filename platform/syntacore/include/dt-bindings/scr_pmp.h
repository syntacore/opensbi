/*
 * Copyright (C) 2024, Syntacore Ltd.
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

#ifndef _DT_BINDINGS_PMP_SYNTACORE_SCR_H
#define _DT_BINDINGS_PMP_SYNTACORE_SCR_H

#include <sbi/riscv_encoding.h>

// PMA bits: memory type
#define SCR_PMP_PMA_SHIFT		5
#define SCR_PMP_CACHE_WEAK_ORDER	(0UL << SCR_PMP_PMA_SHIFT)
#define SCR_PMP_NOCACHE_STRONG_ORDER	(1UL << SCR_PMP_PMA_SHIFT)
#define SCR_PMP_NOCACHE_WEAK_ORDER	(2UL << SCR_PMP_PMA_SHIFT)
#define SCR_PMP_MMIO			(3UL << SCR_PMP_PMA_SHIFT)
#define SCR_PMP_PMA_MASK		(3UL << SCR_PMP_PMA_SHIFT)

#define SCR_PMP_READ			PMP_R
#define SCR_PMP_WRITE			PMP_W
#define SCR_PMP_EXECUTE			PMP_X

// Access types
#define SCR_PMP_RW                      (SCR_PMP_READ | SCR_PMP_WRITE)
#define SCR_PMP_WX                      (SCR_PMP_WRITE | SCR_PMP_EXECUTE)
#define SCR_PMP_RWX                     (SCR_PMP_READ | SCR_PMP_WRITE | SCR_PMP_EXECUTE)

/* Early init PMP */
#define EARLY_PMP_MCFG_REG		14
#define EARLY_PMP_DRAM_REG		15
#define EARLY_PMP_MCFG_OFFSET ((EARLY_PMP_MCFG_REG % 8) * 8)
#define EARLY_PMP_DRAM_OFFSET ((EARLY_PMP_DRAM_REG % 8) * 8)
#define EARLY_PMP_MCFG_MASK ((unsigned long)0xff << EARLY_PMP_MCFG_OFFSET)
#define EARLY_PMP_DRAM_MASK ((unsigned long)0xff << EARLY_PMP_DRAM_OFFSET)
#define PMP_EARLY_REG_MASK ( EARLY_PMP_MCFG_MASK | EARLY_PMP_DRAM_MASK)

#define SCR_DRAM_BASE			0x0
#define SCR_DRAM_SIZE			((CONFIG_PLATFORM_SYNTACORE_DRAM_SIZE_HIGH << 32) + CONFIG_PLATFORM_SYNTACORE_DRAM_SIZE_LOW)
#define SCR_DRAM_ORDER			CONFIG_PLATFORM_SYNTACORE_DRAM_ORDER

#define SCR_MCFG_BASE			0xfffffff0040000
#define SCR_MCFG_SIZE			0x2000
#define SCR_MCFG_ORDER			13

#endif // _DT_BINDINGS_PMP_SYNTACORE_SCR_H
