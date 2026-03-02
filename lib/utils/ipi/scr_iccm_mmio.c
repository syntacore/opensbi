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

#include <sbi_utils/ipi/scr_iccm_mmio.h>

#include <libfdt.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_error.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_hartmask.h>
#include <sbi/sbi_ipi.h>
#include <sbi/sbi_trap.h>
#include <sbi_utils/fdt/fdt_helper.h>

static unsigned long iccm_mmio_addr = -1UL;

#define ICCM_BUFSTATUS			0x0
#define ICCM_BUFREAD			0x4

#define ICCM_SNDSTAT_0			0x400
#define ICCM_SNDSTAT(x, i)		(x + ICCM_SNDSTAT_0 + 0x10 * (i))

#define ICCM_BUFWRITE_0			0xc00
#define ICCM_BUFWRITE(x, i)		(x + ICCM_BUFWRITE_0 + 0x4 * (i))

/* Version Information */
#define ICCM_VERSION			0x1000

/* Number of message receivers - harts */
#define ICCM_HARTS			0x1004

#define ICCM_CONTROL			0x1008
/*
 * External Write Enabled. Allow writing to the buffer initiated from
 * outside the cluster.
 * If set to 0, then SLVERR will be returned on such access attempt
 */
#define ICCM_CONTROL_EWE		BIT(0)
/*
 * Error on Read from Empty.
 * 1 - Return SLVERR on read request to empty buffer.
 * 0 - Return zero otherwise
 */
#define ICCM_CONTROL_ERE		BIT(1)
/*
 * Error on External Write to Full. Determines the response to an
 * attempt to write to a full mailbox initiated from outside the cluster:
 * 1 - Return SLVERR on write attempt
 * 0 - Ignore write attempt
 */
#define ICCM_CONTROL_EEWF		BIT(2)
/*
 * Error on Internal Write to Full. Determines the response to an
 * attempt to write to a full mailbox initiated from inside the cluster:
 * 1 - Return SLVERR on write attempt
 * 0 - Ignore write attempt
 */
#define ICCM_CONTROL_EIWF		BIT(3)

#define ICCM_CLEAR			0x1010

static void scr_mmio_ipi_send(u32 hart_index)
{
	u32 target_hart = sbi_hartindex_to_hartid(hart_index);
	void *addr = (void *)ICCM_BUFWRITE(iccm_mmio_addr, target_hart);

	/*
	 * Only valid if ICCM_CONTROL_EIWF not set in ICCM_CONTROL.
	 * Otherwise we should check ICCM_SNDSTAT
	 * to avoid triggering SLVERR.
	 */
	writel_relaxed(0x01, addr);
}

static void scr_mmio_ipi_clear(void)
{
	void *addr = (void *)(iccm_mmio_addr + ICCM_CLEAR);
	u32 hartid = current_hartid();

#if __riscv_xlen == 32
	writel_relaxed(BIT(hartid), addr);
#else
	writeq_relaxed(BIT_ULL(hartid), addr);
#endif
}

static struct sbi_ipi_device scr_iccm_mmio = {
	.name = "scr-iccm-mmio",
	.ipi_send = scr_mmio_ipi_send,
	.ipi_clear = scr_mmio_ipi_clear
};

int scr_iccm_mmio_cold_init(uint64_t reg_addr, uint64_t reg_size)
{
	void *addr;

	iccm_mmio_addr = reg_addr;
	addr = (void *)(iccm_mmio_addr + ICCM_CONTROL);

	/* Allow external writes and disable error generation */
	writel_relaxed(ICCM_CONTROL_EWE, addr);

	sbi_ipi_set_device(&scr_iccm_mmio);

	return 0;
}
