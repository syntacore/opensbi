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

#ifndef _SCR_ICCM_H_
#define _SCR_ICCM_H_

#include <sbi/sbi_types.h>

/*
 * Address	Mnemonic	Mode	Description
 * 0xBD8	ADDRESS		RW	destination address
 * 0xBD9	STATUS		RO	status of destination mailbox (0 - empty, 1 - busy)
 * 0xBDA	RDDATA		RW	status/value of incoming mailbox
 * 0xBDB	WRDATA		WO	outgoing message ("Send message" trigger)
 */

#define SCR_CSR_ICCM_BASE	0xbd8
#define SCR_CSR_ICCM_ADDR	(SCR_CSR_ICCM_BASE + 0x00)
#define SCR_CSR_ICCM_STATUS	(SCR_CSR_ICCM_BASE + 0x01)
#define SCR_CSR_ICCM_READ	(SCR_CSR_ICCM_BASE + 0x02)
#define SCR_CSR_ICCM_WRITE	(SCR_CSR_ICCM_BASE + 0x03)

#define SCR_ICCM_MAILBOX_EMPTY	(_ULL(1) << (__riscv_xlen - 1))

struct scr_iccm_data {
	/* slot index */
	unsigned long idx;
};

void scr_ipi_send(u32 target_hart);
int scr_iccm_warm_init(void);
int scr_fdt_iccm_init(void *fdt);

#endif // _SCR_ICCM_H_
