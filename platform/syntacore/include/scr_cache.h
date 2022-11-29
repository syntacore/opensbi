/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Syntacore
 *
 * derivate: fsbl/common/cache.h
 * Authors:
 *   Mikhail Nefedov <mikhail.nefedov@syntacore.com>
 */

#ifndef _SCR_CACHE_H_
#define _SCR_CACHE_H_

#include <sbi/sbi_types.h>

/* cache control CSRs */
#define SCR_CSR_CACHE_GLBL		0xbd4

/* cache info CSRs */
#define SCR_CSR_CACHE_DSCR_L1		0xfc3

#define SCR_CSR_ICACHE_INFO_MASK	0x000ffff
#define SCR_CSR_DCACHE_INFO_MASK	0xfff0000

/* global cache's control bits */
#define SCR_CACHE_GLBL_L1I_EN		(_UL(1) << 0)
#define SCR_CACHE_GLBL_L1D_EN		(_UL(1) << 1)
#define SCR_CACHE_GLBL_L1I_INV		(_UL(1) << 2)
#define SCR_CACHE_GLBL_L1D_INV		(_UL(1) << 3)
#define SCR_CACHE_GLBL_ENABLE		(SCR_CACHE_GLBL_L1I_EN | SCR_CACHE_GLBL_L1D_EN)
#define SCR_CACHE_GLBL_DISABLE		_UL(0)
#define SCR_CACHE_GLBL_INV		(SCR_CACHE_GLBL_L1I_INV | SCR_CACHE_GLBL_L1D_INV)

/* general control registers */
#define SCR_L2_VERSION			0x00
#define SCR_L2_DESCR_0			0x04
#define SCR_L2_DESCR_1			0x08
#define SCR_L2_CACHE_ENABLE		0x10
#define SCR_L2_CACHE_FLUSH		0x14
#define SCR_L2_CACHE_INVAL		0x18

/* denided status registers */
#define SCR_L2_ENABLE_DN		0x20
#define SCR_L2_FLUSH_DN			0x24
#define SCR_L2_INVAL_DN			0x28

#define SCR_L2_CACHE_CTRL		0x2c
#define SCR_L2_MCP			0x40

#define SCR_L2_ERROR_STATUS		0x80
#define SCR_L2_TAG_MEMORY_STATUS	0x88
#define SCR_L2_DATA_MEMORY_STATUS	0x8c
#define SCR_L2_ERROR_IRQ_EN		0x90

#define SCR_L2_MAU_READ_ERR_ADDR	0x100
#define SCR_L2_MAU_WRITE_ERR_ADDR	0x110
#define SCR_L2_NCAU_READ_ERR_ADDR	0x120
#define SCR_L2_NCAU_WRITE_ERR_ADDR	0x130
#define SCR_L2_MAU_READ_ERR_CODE	0x140
#define SCR_L2_MAU_WRITE_ERR_CODE	0x150
#define SCR_L2_NCAU_READ_ERR_CODE	0x160
#define SCR_L2_NCAU_WRITE_ERR_CODE	0x170

#define SCR_L2_ECC_TAG_ERR		0x200
#define SCR_L2_ECC_DATA_ERR		0x300

#define L2_CSR_DESCR_OFFS_WAYS		(0)
#define L2_CSR_DESCR_OFFS_LINESZ_LG2	(4)
#define L2_CSR_DESCR_OFFS_LINES_LG2	(8)
#define L2_CSR_DESCR_OFFS_TYPE		(13)
#define L2_CSR_DESCR_OFFS_BANKS		(16)
#define L2_CSR_DESCR_OFFS_CORES		(28)

#define L2_CSR_DESCR_MASK_BANKS		(0xf)
#define L2_CSR_DESCR_MASK_WAYS		(0x7)
#define L2_CSR_DESCR_MASK_LINESZ_LG2	(0xf)
#define L2_CSR_DESCR_MASK_LINES_LG2	(0x1f)
#define L2_CSR_DESCR_MASK_CORES		(0xf)
#define L2_CSR_DESCR_MASK_TYPE		(0x7)

bool scr_cache_l1_available(void);
bool scr_cache_l1_enabled(void);

void scr_cache_l1_enable(void);
void scr_cache_l1_disable(void);

void scr_l2cache_enable(void);
void scr_l2cache_disable(void);

bool scr_l2cache_is_enabled(void);

void scr_cache_flush(void *vaddr, unsigned long size);

void scr_print_l1cache_info(void);
void scr_print_l2cache_info(void);

unsigned int scr_l2c_get_cpunum(void);

int scr_fdt_l2_cache_init(void *fdt);

#endif // _SCR_CACHE_H_
