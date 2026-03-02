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

#include <libfdt.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_heap.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/ipi/fdt_ipi.h>
#include <sbi_utils/ipi/scr_iccm_mmio.h>

static int fdt_parse_scr_iccm_mmio_node(const void *fdt, int nodeoff, uint64_t *reg_addr, uint64_t *reg_size)
{
	int rc;

	if (nodeoff < 0 || !fdt || !reg_addr || !reg_size)
		return SBI_EINVAL;

	rc = fdt_get_node_addr_size(fdt, nodeoff, 0,
				    reg_addr, reg_size);
	if (rc < 0 || !*reg_size)
		return SBI_ENODEV;

	return 0;
}

int fdt_scr_iccm_mmio_cold_init(const void *fdt, int nodeoff,
				const struct fdt_match *match)
{
	uint64_t reg_addr, reg_size;
	int rc;

	rc = fdt_parse_scr_iccm_mmio_node(fdt, nodeoff, &reg_addr, &reg_size);
	if (rc)
		return rc;

	rc = scr_iccm_mmio_cold_init(reg_addr, reg_size);
	if (rc)
		return rc;

	return 0;
}

static const struct fdt_match ipi_iccm_mmio_match[] = {
	{ .compatible = "syntacore,iccm-mmio" },
	{ /* sentinel */ },
};

const struct fdt_driver fdt_ipi_iccm_mmio = {
	.match_table = ipi_iccm_mmio_match,
	.init = fdt_scr_iccm_mmio_cold_init,
};
