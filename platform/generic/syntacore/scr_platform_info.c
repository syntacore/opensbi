/*
 * Copyright (C) 2025, Syntacore Ltd.
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

#include <sbi/sbi_bitops.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_csr_detect.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_pmu.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi_utils/fdt/fdt_helper.h>

#include <syntacore/sbi_vendor.h>
#include <syntacore/scr_cache.h>
#include <syntacore/scr_encoding.h>
#include <syntacore/scr_platform_info.h>

/* When MISELECT most significant bit is set, access to custom registers
 * through the alias CSRs is provided. So, in order to access SCR_CSR_VCR_SCHEMA,
 * SCR_CSR_CORE_TIMESTAMP_ID, SCR_CSR_CORE_BUILD_TARGET_ID, SCR_CSR_CORE_CFG_ID
 * this bit must be set first.
 */
#define MISEL_ACCESS		BIT(__riscv_xlen - 1)

int scr_get_platform_info (struct sbi_trap_regs *regs,
			struct sbi_ecall_return *out, int l3_platform)
{
	int ret = SBI_OK;
	struct sbi_trap_info trap = {0};
	unsigned long val = 0;

	switch (regs->a0) {
	case SCR_PLF_GET_FEAT_EN:
		val = csr_read_allowed(SCR_CSR_FEAT_EN, (ulong)&trap);
		break;
	case SCR_PLF_GET_L1D_PF_CTRL0:
		val = csr_read_allowed(SCR_CSR_L1D_PF_CTRL0, (ulong)&trap);
		break;
	default:
		ret = SBI_ENOTSUPP;
		break;
	}
	out->value = val;
	if (trap.cause)
		ret = SBI_ENOTSUPP;

	return ret;
}
