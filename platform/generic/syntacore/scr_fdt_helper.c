/*
 * Copyright (C) 2025, Syntacore Ltd.
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
#include <sbi_utils/fdt/fdt_helper.h>
#include <syntacore/scr_fdt_helper.h>
#include <syntacore/scr_generic.h>


static int node_property_patch_by_compatible_u32(void *fdt, unsigned value, char *compatible, char *property)
{
	int node_offset;
	// Finding node offset
	node_offset = fdt_node_offset_by_compatible(fdt, -1, compatible);
	if (node_offset >= 0) {
		// If found, patch property
		return fdt_setprop_inplace_u32(fdt, node_offset, property, value);
	}

	return SBI_ENOENT;
}

int scr_get_node_property_val_64(const void *fdt, int offset, char *name, unsigned long *val)
{
	const u32 *val_32;
	int len;
	val_32 = fdt_getprop(fdt, offset, name, &len);
	if (!val_32 || (len < sizeof(u32))) {
		return SBI_EINVAL;
	}
	*val = (ulong) fdt32_to_cpu(val_32[0]);
#if __riscv_xlen != 32
	if (len >= sizeof(u64)) {
		*val = (*val << 32) | ((ulong) fdt32_to_cpu(val_32[1]));
	}
#else
	if (len >= sizeof(u64)) {
		*val = (ulong) fdt32_to_cpu(val_32[1]);
	}
#endif
	return SBI_OK;
}

int scr_fdt_parse_fpga_reg(void *fdt, scr_fpga_reg_data *fpga_regs)
{
	unsigned long addr_read;
	int offset, ret = SBI_ENOENT;
	char *const props[] = {"syntacore,buildid-reg", "syntacore,sysclk-reg", "syntacore,clsclk-reg"};
	unsigned long *const fpga_addrs[] = {&fpga_regs->buildid_addr, &fpga_regs->sysclk_addr, &fpga_regs->clsclk_addr};

	if(!fdt || !fpga_regs)
		return SBI_EINVAL;

	memset(fpga_regs, 0, sizeof(scr_fpga_reg_data));
	offset = fdt_node_offset_by_compatible(fdt, -1, "syntacore,fpga-reg-v1");

	if(offset < 0)
		return ret;

	for(int i = 0; i < sizeof(fpga_addrs) / sizeof(fpga_addrs[0]); i++) {
		if (scr_get_node_property_val_64(fdt, offset, props[i], &addr_read) == SBI_OK) {
			*fpga_addrs[i] = addr_read;
			ret = SBI_OK;
		}
	}

	return ret;
}

int scr_fdt_parse_hwinfo_struct(void *fdt, unsigned long *struct_addr)
{
	int offset, ret = SBI_ENOENT;
	unsigned long addr_read;

	if(!fdt || !struct_addr)
		return SBI_EINVAL;

	offset = fdt_node_offset_by_compatible(fdt, -1, "syntacore,hwinfo-struct-v1");
	if(offset < 0)
		return ret;

	if(scr_get_node_property_val_64(fdt, offset, "syntacore,struct-addr", &addr_read) == SBI_OK) {
		*struct_addr = addr_read;
		ret = SBI_OK;
	}

	return ret;
}

int scr_fdt_patch_sys_clk(void *fdt, unsigned long sys_clk)
{
	int ret = SBI_ENOENT;
	int offset = fdt_path_offset(fdt, "/clocks/clk");

	if(offset < 0)
		return ret;

	ret = fdt_setprop_inplace_u32(fdt, offset, "clock-frequency", sys_clk);

	return ret;
}

int scr_fdt_patch_axi_clk(void *fdt, unsigned long axi_clk)
{
	return node_property_patch_by_compatible_u32(fdt, axi_clk, "simple-bus", "bus-frequency");
}

int scr_fdt_patch_eth_clk(void *fdt, unsigned long eth_clk)
{
	return node_property_patch_by_compatible_u32(fdt, eth_clk, "xlnx,eth-dma", "clock-frequency");
}

int scr_fdt_patch_uart_clk(void *fdt, unsigned long uart_clk)
{
	return node_property_patch_by_compatible_u32(fdt, uart_clk, "ns16550a", "clock-frequency");
}

int scr_fdt_patch_mtimer_clk(void *fdt, unsigned long mtimer_clk)
{
	return node_property_patch_by_compatible_u32(fdt, mtimer_clk, "syntacore,mtimer", "clock-frequency");
}

int scr_fdt_patch_build_id_str(void *fdt, char *build_id)
{
	int offset = fdt_path_offset(fdt, "/firmware");

	if(offset < 0 || !fdt_getprop(fdt, offset, "syntacore,bldid", NULL))
		return SBI_ENOENT;

	return fdt_setprop_string(fdt, offset, "syntacore,bldid", build_id);
}

void scr_fdt_patch_cpu_frequency(void *fdt, int (*get_hart_clk)(unsigned long hartid, unsigned long *hart_clk))
{
	int cpu_offset, cpus_offset;
	unsigned int dtb_cpu_hartid;
	unsigned long hart_clk;

	if (!fdt || !get_hart_clk)
		return;

	cpus_offset = fdt_path_offset(fdt, "/cpus");

	if (cpus_offset < 0)
		return;

	fdt_for_each_subnode(cpu_offset, fdt, cpus_offset) {
		if (fdt_parse_hart_id(fdt, cpu_offset, &dtb_cpu_hartid))
			continue;
		if (get_hart_clk(dtb_cpu_hartid, &hart_clk))
			continue;
		fdt_setprop_inplace_u32(fdt, cpu_offset, "clock-frequency", hart_clk);
	}
}

int scr_fdt_check_mtimer_scr_external(const void *fdt)
{
	int offset = fdt_node_offset_by_compatible(fdt, -1, "syntacore,mtimer");

	if(offset < 0 || !fdt_getprop(fdt, offset, "scr,timsrc-external", NULL))
		return SBI_ENOENT;

	return SBI_OK;
}

void scr_fdt_detect_swpw(void *fdt)
{
	int cpu_offset, cpus_offset, len;
	unsigned int dtb_cpu_hartid;

	/* Find cpu node */
	cpus_offset = fdt_path_offset(fdt, "/cpus");

	if (cpus_offset < 0)
		return;

	fdt_for_each_subnode(cpu_offset, fdt, cpus_offset) {
		if(fdt_parse_hart_id(fdt, cpu_offset, &dtb_cpu_hartid))
			continue;

		if (fdt_getprop(fdt, cpu_offset, "scr,software-pagewalker", &len)) {
			/* "scr,software-pagewalker" is present in dtb cpu node */
			scr_hartid_swpw_set(dtb_cpu_hartid, 1);
		}
	}
}

static int fdt_find_match(const void *fdt, int startoff,
		   const struct fdt_match *match_table,
		   const struct fdt_match **out_match)
{
	int nodeoff;

	if (!fdt || !match_table)
		return SBI_ENODEV;

	while (match_table->compatible) {
		nodeoff = fdt_node_offset_by_compatible(fdt, startoff,
						match_table->compatible);
		if (nodeoff >= 0) {
			if (out_match)
				*out_match = match_table;
			return nodeoff;
		}
		match_table++;
	}

	return SBI_ENODEV;
}

const struct fdt_match *scr_fdt_find_cache(const void *fdt,
		const struct fdt_match *match_tbl, uint64_t *base, uint64_t *size)
{
	const struct fdt_match *match;
	int nodeoff;

	nodeoff = fdt_find_match(fdt, -1, match_tbl, &match);

	if (nodeoff < 0)
		return NULL;

	if (fdt_get_node_addr_size(fdt, nodeoff, 0, base, size))
		return NULL;

	return match;
}
