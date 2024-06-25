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

#include "scr_iccm.h"

#include <libfdt.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_error.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_hartmask.h>
#include <sbi/sbi_ipi.h>
#include <sbi_utils/fdt/fdt_helper.h>

#define ICCM_MAX_NR			16
static struct scr_iccm_data iccm[ICCM_MAX_NR] = { { .idx = -1 } };

#define ICCM_MAX_WRITE_TRY_COUNT	1000

/* function temporary exposed as we need to send ipi on hsm_start */
void scr_ipi_send(u32 target_hart)
{
	int t = ICCM_MAX_WRITE_TRY_COUNT;
	unsigned long slotn = iccm[target_hart].idx;

	csr_write(SCR_CSR_ICCM_ADDR, slotn);

	while (t) {
		if (csr_read(SCR_CSR_ICCM_STATUS) == 0) {
			csr_write(SCR_CSR_ICCM_WRITE, 1);
			break;
		}
		--t;
	}
}

static void scr_ipi_clear(u32 target_hart)
{
#if __riscv_xlen == 32
	u32 msg;
#else
	u64 msg;
#endif

	do {
		msg = csr_swap(SCR_CSR_ICCM_READ, 1);
	} while (!(msg & SCR_ICCM_MAILBOX_EMPTY));
}

static struct sbi_ipi_device scr_iccm = {
	.name = "scr_iccm",
	.ipi_send = scr_ipi_send,
	.ipi_clear = scr_ipi_clear
};

int scr_iccm_warm_init(void)
{
	u32 hartid = current_hartid();

	if (iccm[hartid].idx >= 0)
		scr_ipi_clear(hartid);
	return 0;
}

static int scr_iccm_cold_init(void *fdt, int nodeoff,
			const struct fdt_match *match)
{
	int len, i, err;
	const u32 *mboxes_map;
	struct scr_iccm_data *data;
	uint32_t cpu_phandle, hartid;
	int cpu_offset;

	if (!fdt)
		return SBI_EINVAL;

	mboxes_map = fdt_getprop(fdt, nodeoff, "mbox-mapping", &len);
	if (!mboxes_map || len < 8)
		return SBI_EFAIL;

	len = len / (sizeof(u32) * 2);
	for (i = 0; i < len; i++) {
		cpu_phandle = fdt32_to_cpu(mboxes_map[2 * i]);
		cpu_offset = fdt_node_offset_by_phandle(fdt, cpu_phandle);
		err = fdt_parse_hart_id(fdt, cpu_offset, &hartid);
		if (err)
			continue;

		if (hartid > ICCM_MAX_NR)
			return SBI_EFAIL;

		data = &iccm[hartid];
		data->idx = fdt32_to_cpu(mboxes_map[2 * i + 1]);
	}

	sbi_ipi_set_device(&scr_iccm);

	return 0;
}

static const struct fdt_match iccm_match[] = {
	{ .compatible = "syntacore,iccm" },
	{ },
};

int scr_fdt_iccm_init(void *fdt)
{
	const struct fdt_match *match;
	int nodeoff = -1;

	nodeoff = fdt_find_match(fdt, 0, iccm_match, &match);
	if (nodeoff < 0)
		return SBI_ENODEV;

	return scr_iccm_cold_init(fdt, nodeoff, match);
}
