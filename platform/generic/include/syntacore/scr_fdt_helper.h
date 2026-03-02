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

#ifndef _SCR_FDT_HELPER_H_
#define _SCR_FDT_HELPER_H_

#include <sbi/sbi_error.h>
#include <sbi_utils/fdt/fdt_helper.h>

typedef struct {
	unsigned long buildid_addr;
	unsigned long sysclk_addr;
	unsigned long clsclk_addr;

} scr_fpga_reg_data;


int scr_get_node_property_val_64(const void *fdt, int offset, char *name, unsigned long *val);
int scr_fdt_parse_fpga_reg(void *fdt, scr_fpga_reg_data *fpga_regs);
int scr_fdt_parse_hwinfo_struct(void *fdt, unsigned long *struct_addr);
int scr_fdt_patch_sys_clk(void *fdt, unsigned long sys_clk);
int scr_fdt_patch_axi_clk(void *fdt, unsigned long axi_clk);
int scr_fdt_patch_eth_clk(void *fdt, unsigned long eth_clk);
int scr_fdt_patch_uart_clk(void *fdt, unsigned long uart_clk);
int scr_fdt_patch_mtimer_clk(void *fdt, unsigned long mtimer_clk);
int scr_fdt_patch_build_id_str(void *fdt, char *build_id);
void scr_fdt_patch_cpu_frequency(void *fdt, int (*get_hart_clk)(unsigned long hartid, unsigned long *hart_clk));
int scr_fdt_check_mtimer_scr_external(const void *fdt);
void scr_fdt_detect_swpw(void *fdt);
const struct fdt_match *scr_fdt_find_cache(const void *fdt,
		const struct fdt_match *match_tbl, uint64_t *base, uint64_t *size);

#endif /* _SCR_FDT_HELPER_H_ */
