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
#ifndef __SCR_ENCODING_H__
#define __SCR_ENCODING_H__

#include <sbi/riscv_encoding.h>

/** Syntacore custom CSRs **/
#define SCR_CSR_L1_CTRL			0xBD4
#define SCR_CSR_FEAT_EN			0xBE0
#define SCR_CSR_L1D_PF_CTRL0		0xBF0
#define SCR_CSR_L1D_PF_CTRL1		0xBF1

/** Register bits and masks **/
/* Feature enable register lock bit (MSB of FEAT_EN) */
#define FEAT_EN_LOCK_BIT (BIT(__riscv_xlen - 1))
/* Feature enable register hardware pagewalker bit */
#define FEAT_EN_HPW_BIT (BIT(0))
/* Mask for Core ID in MARCHID csr */
#define MARCHID_CORE_MASK 0xF000
/* Value for SCR5 Core ID in MARCHID csr */
#define MARCHID_CORE_SCR5 0x5000

#endif // __SCR_ENCODING_H__
