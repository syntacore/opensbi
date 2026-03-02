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
#include <sbi/sbi_list.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/timer/fdt_timer.h>
#include <sbi_utils/timer/scr_mtimer.h>

extern struct scr_mtimer_data scr_mt_data;

static int fdt_parse_clock_frequency(const void *fdt, int nodeoff, unsigned long *freq)
{
	const fdt32_t *clocks_map, *val;
	uint32_t clock_phandle;
	int clock_offset, len;

	clocks_map = fdt_getprop(fdt, nodeoff, "clocks", &len);
	if (!clocks_map || len < 4) {
		clock_offset = fdt_node_offset_by_compatible(fdt, -1, "syntacore,mtimer");
	} else {
		clock_phandle = fdt32_to_cpu(*clocks_map);
		clock_offset = fdt_node_offset_by_phandle(fdt, clock_phandle);
	}
	if (clock_offset < 0)
		return clock_offset;

	val = fdt_getprop(fdt, clock_offset, "clock-frequency", &len);
	if (!val || len < 4)
		return SBI_EFAIL;

	*freq = (unsigned long)fdt32_to_cpu(*val);
	return 0;
}

static int fdt_parse_frequency_divider(const void *fdt, int nodeoff, unsigned long *divider)
{
	const fdt32_t *val;
	int len;

	val = fdt_getprop(fdt, nodeoff, "scr,divider", &len);
	if (!val || len < 4) {
		*divider = 0;
		return 0;
	}
	*divider = (unsigned long)fdt32_to_cpu(*val);
	return 0;
}

static int fdt_scr_cold_timer_init(const void *fdt, int nodeoff,
				    const struct fdt_match *match)
{
	int rc;
	uint64_t reg_addr, reg_size;
	unsigned long clock_freq;

	/** sanity check */
	if (nodeoff < 0 || !fdt)
		return SBI_EINVAL;

	rc = fdt_get_node_addr_size(fdt, nodeoff, 0,
				    &reg_addr, &reg_size);
	if (rc < 0 || !reg_size)
		return SBI_ENODEV;

	scr_mt_data.reg_base = reg_addr;
	scr_mt_data.reg_size = reg_size;
	scr_mt_data.mtime_addr = reg_addr + SCR_MTIMER_TIME_OFFSET;
	scr_mt_data.mtime_size = SCR_MTIMER_TIME_SIZE;
	scr_mt_data.mtimecmp_addr = reg_addr + SCR_MTIMER_TIMECMP_OFFSET;
	scr_mt_data.mtimecmp_size = SCR_MTIMER_TIMECMP_SIZE;

	rc = fdt_parse_timebase_frequency(fdt, &scr_mt_data.mtime_freq);
	if (rc)
		return rc;

	rc = fdt_parse_frequency_divider(fdt, nodeoff, &scr_mt_data.divider);
	if (rc)
		return rc;

	rc = fdt_parse_clock_frequency(fdt, nodeoff, &clock_freq);
	if (rc)
		return rc;

	rc = scr_cold_timer_init(clock_freq);
	if (rc)
		return rc;

	return 0;
}

static const struct fdt_match scr_mtimer_match[] = {
	{ .compatible = "syntacore,mtimer" },
	{ },
};

const struct fdt_driver fdt_timer_scr_mtimer = {
	.match_table = scr_mtimer_match,
	.init = fdt_scr_cold_timer_init,
};
