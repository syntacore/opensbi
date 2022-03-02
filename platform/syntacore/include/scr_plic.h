/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Syntacore
 *
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
