/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Syntacore
 *
 */

#include "scr_plic.h"

#include <libfdt.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_error.h>
#include <sbi_utils/fdt/fdt_helper.h>

// numbering starts from 1, 0 - hardwired to zero
#define PLF_INTLINE_UART	1
#define PLF_INTLINE_ETH0_RX	2
#define PLF_INTLINE_ETH0_TX	3
#define PLF_INTLINE_PCI_MSI	4

static const struct {
	uint32_t line;
	uint32_t mode;
} irq_modes[] = {
	{PLF_INTLINE_UART, SCR_PLIC_SRC_MODE_LEVEL_HIGH},
	{PLF_INTLINE_ETH0_RX, SCR_PLIC_SRC_MODE_LEVEL_HIGH},
	{PLF_INTLINE_ETH0_TX, SCR_PLIC_SRC_MODE_LEVEL_HIGH},
	{PLF_INTLINE_PCI_MSI, SCR_PLIC_SRC_MODE_LEVEL_HIGH},
};

static inline void plic_set_mode(ulong plic_addr, u32 source, u32 val)
{
	volatile void *plic_mode = (void *)plic_addr +
		SCR_PLIC_MODE_BASE + SCR_PLIC_MODE_REG_WIDTH * source;
	writel_relaxed(val, plic_mode);
}

static int scr_plic_fixup(ulong plic_addr)
{
	int i;

	for (i = 1; i < SCR_PLIC_MAX_LINES_NUMBER; i++)
		plic_set_mode(plic_addr, i, SCR_PLIC_SRC_MODE_OFF);

	for (i = 0; i < array_size(irq_modes); i++)
		plic_set_mode(plic_addr, irq_modes[i].line, irq_modes[i].mode);

	return 0;
}

static int scr_plic_fixup_all(ulong plic_addr)
{
	int i;

	for (i = 1; i < SCR_PLIC_MAX_LINES_NUMBER; i++)
		plic_set_mode(plic_addr, i, SCR_PLIC_SRC_MODE_LEVEL_HIGH);

	return 0;
}

static const struct fdt_match plic_match[] = {
	{ .compatible = "syntacore,plic" },
	{ },
};

int src_fdt_plic_fixup(void *fdt, bool all)
{
	int nodeoff = -1;
	const struct fdt_match *match;
	uint64_t reg_addr, reg_size;
	int rc;

	nodeoff = fdt_find_match(fdt, 0, plic_match, &match);
	if (nodeoff < 0)
		return SBI_ENODEV;

	rc = fdt_get_node_addr_size(fdt, nodeoff, 0,
				    &reg_addr, &reg_size);
	if (rc < 0 || !reg_size)
		return SBI_ENODEV;

	if (all)
		return scr_plic_fixup_all((ulong)reg_addr);

	return scr_plic_fixup(reg_addr);
}
