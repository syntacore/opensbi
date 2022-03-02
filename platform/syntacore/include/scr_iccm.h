/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Syntacore
 *
 * derivate: fsbl/common/ipi.h
 * Authors:
 *   Mikhail Nefedov <mikhail.nefedov@syntacore.com>
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
