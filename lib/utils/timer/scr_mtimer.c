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

#include <sbi/riscv_io.h>
#include <sbi/sbi_timer.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_console.h>
#include <sbi_utils/timer/scr_mtimer.h>

static int scr_clock_source;
struct scr_mtimer_data scr_mt_data;

static u64 mtimer_time_rd64(void *addr)
{
#if __riscv_xlen == 32
	volatile u32 mtime_h;
	u32 mtime_l;

	do {
		mtime_h = readl_relaxed(addr + 4);
		mtime_l = readl_relaxed(addr);
	} while (mtime_h != readl_relaxed(addr + 4));

	return (((u64)mtime_h) << 32 | mtime_l);

#else
	return readq_relaxed(addr);
#endif
}

static void mtimer_time_wr64(u64 value, void *addr)
{
#if __riscv_xlen == 32
	writel_relaxed(0x0, addr);
	writel_relaxed((u32)(value >> 32), (addr + 4));
	writel_relaxed((u32)(value & 0xFFFFFFFF), addr);
#else
	writeq_relaxed(value, addr);
#endif
}

static u64 scr_mtimer_value(void)
{
	/* Read MTIMER Time Value */
	return mtimer_time_rd64((void *)scr_mt_data.mtime_addr);
}

static void scr_mtimer_event_start(u64 next_event)
{
	/* Program MTIMER Time Compare */
	mtimer_time_wr64(next_event, (void *)scr_mt_data.mtimecmp_addr);
}

static void scr_mtimer_event_stop(void)
{
	/* Clear MTIMER Time Compare */
	mtimer_time_wr64(-1ULL, (void *)scr_mt_data.mtimecmp_addr);
}

static struct sbi_timer_device scr_mtimer = {
	.name = "syntacore-mtimer",
	.timer_value = scr_mtimer_value,
	.timer_event_start = scr_mtimer_event_start,
	.timer_event_stop = scr_mtimer_event_stop
};

void scr_mtimer_set_clocksource(int clock_source)
{
	scr_clock_source = clock_source;
}

int scr_mtimer_get_clocksource(void)
{
	return scr_clock_source;
}

void scr_mtimer_print_info(void)
{
	struct scr_mtimer_data *mt = &scr_mt_data;
	u32 val;

	val = readl_relaxed((void *)mt->reg_base + SCR_MTIMER_CONTROL_OFFSET);
	sbi_printf("scr mtimer: SCR_MTIMER_CONTROL : 0x%x\n", val);
	val = readl_relaxed((void *)mt->reg_base + SCR_MTIMER_DIVIDER_OFFSET);
	sbi_printf("scr mtimer: SCR_MTIMER_DIVIDER : %u\n", val);
	sbi_printf("scr mtimer: source clock frequency: %lu\r\n", mt->source_freq);
	sbi_printf("scr mtimer: timebase frequency: %lu\r\n", mt->mtime_freq);
}

static int scr_div_init(const struct scr_mtimer_data *mt)
{
	uint32_t mtimer_clock_div;
	unsigned long div = mt->divider ? mt->divider : 1;

	mtimer_clock_div = mt->source_freq / mt->mtime_freq * div - 1;
	if (mtimer_clock_div >= (1 << 10)) {
		sbi_panic("%s: SCR_MTIMER_DIVIDER value exceeds 10 bit: %u",
				__func__, mtimer_clock_div);
	}

	mtimer_clock_div &= (1 << 10) - 1;

	/** disable MTIMER, set Core Clock source */
	writel_relaxed(0, (void *)mt->reg_base + SCR_MTIMER_CONTROL_OFFSET);
	/* workaround for mtime != 0 on reset */
#if __riscv_xlen == 32
	writel(0UL, (void *)mt->mtime_addr);
	writel(0UL, (void *)(mt->mtime_addr + 4));
#else
	writeq(0ULL, (void *)mt->mtime_addr);
#endif
	/** setup divider */
	writel(mtimer_clock_div, (void *)mt->reg_base + SCR_MTIMER_DIVIDER_OFFSET);
	/** enable MTIMER */
	writel_relaxed(SCR_MTIMER_ENABLE | scr_clock_source, (void *)mt->reg_base + SCR_MTIMER_CONTROL_OFFSET);

	return 0;
}

int scr_cold_timer_init(unsigned long clock_freq)
{
	int rc;

	scr_mt_data.source_freq = clock_freq;

	/** calc divider for freq */
	rc = scr_div_init(&scr_mt_data);
	if (rc)
		return rc;

	scr_mtimer.timer_freq = scr_mt_data.mtime_freq;
	sbi_timer_set_device(&scr_mtimer);

	return 0;
}
