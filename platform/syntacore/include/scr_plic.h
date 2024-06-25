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

#ifndef _SCR_PLIC_H_
#define _SCR_PLIC_H_

#include <sbi/sbi_types.h>

#define SCR_PLIC_MAX_LINES_NUMBER	1024
#define SCR_PLIC_MODE_BASE		0x1f0000
#define SCR_PLIC_MODE_REG_WIDTH		0x04

enum scr_plic_mode_enum {
	SCR_PLIC_SRC_MODE_OFF		= 0,
	SCR_PLIC_SRC_MODE_LEVEL_HIGH	= 1,
	SCR_PLIC_SRC_MODE_LEVEL_LOW	= 2,
	SCR_PLIC_SRC_MODE_EDGE_RISING	= 3,
	SCR_PLIC_SRC_MODE_EDGE_FALLING	= 4,
	SCR_PLIC_SRC_MODE_EDGE_BOTH	= 5,
	SCR_PLIC_SRC_MODE_MAX		= SCR_PLIC_SRC_MODE_EDGE_BOTH,
};

int src_fdt_plic_fixup(void *fdt, bool all);

#endif // _SCR_PLIC_H_
