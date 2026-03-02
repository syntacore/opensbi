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

#ifndef _SCR_GENERIC_H_
#define _SCR_GENERIC_H_

#include <sbi/sbi_types.h>

/* Some masks and defines for early PMP set */
#define EARLY_PMP_MCFG_REG  14
#define EARLY_PMP_DRAM_REG  15
#if __riscv_xlen == 32
#define EARLY_PMP_MCFG_OFFSET ((EARLY_PMP_MCFG_REG % 4) * 8)
#define EARLY_PMP_DRAM_OFFSET ((EARLY_PMP_DRAM_REG % 4) * 8)
#define EARLY_PMP_MCFG_MASK ((unsigned long)0xff << EARLY_PMP_MCFG_OFFSET)
#define EARLY_PMP_DRAM_MASK ((unsigned long)0xff << EARLY_PMP_DRAM_OFFSET)
#define PMP_EARLY_REG_MASK (EARLY_PMP_MCFG_MASK | EARLY_PMP_DRAM_MASK)
#else // __riscv_xlen
#define EARLY_PMP_MCFG_OFFSET ((EARLY_PMP_MCFG_REG % 8) * 8)
#define EARLY_PMP_DRAM_OFFSET ((EARLY_PMP_DRAM_REG % 8) * 8)
#define EARLY_PMP_MCFG_MASK ((unsigned long)0xff << EARLY_PMP_MCFG_OFFSET)
#define EARLY_PMP_DRAM_MASK ((unsigned long)0xff << EARLY_PMP_DRAM_OFFSET)
#define PMP_EARLY_REG_MASK (EARLY_PMP_MCFG_MASK | EARLY_PMP_DRAM_MASK)
#endif // __riscv_xlen

/* CPU defines */
#define SCR_CPU_MAX_HARTS	16

int scr_hartid_nascent_get(unsigned int hartid);
int scr_hartid_nascent_set(unsigned int hartid, u8 val);
void scr_hartid_nascent_flush(void);

int scr_hartid_swpw_set(unsigned int hartid, u8 val);
int scr_hartid_swpw_get(unsigned int hartid);

int mmio_read_allowed(uintptr_t addr);

#endif /* _SCR_GENERIC_H_ */
