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

#include <sbi/riscv_io.h>
#include <sbi_utils/timer/scr_mtimer.h>
#include <syntacore/scr_hwinfo.h>
#include <syntacore/scr_fdt_helper.h>
#include <syntacore/scr_generic.h>


typedef struct {
	int (*get_build_id)(unsigned long *build_id);
	int (*get_sys_clk)(unsigned long *sys_clk);
	int (*get_cls_clk)(unsigned long *cls_clk);
	int (*get_mtimer_clk)(unsigned long *mtimer_clk);
	int (*get_uart_clk)(unsigned long *uart_clk);
	int (*get_harts_count)(unsigned long *harts_count);
	int (*get_hart_clk)(unsigned long hartid, unsigned long *hart_clk);

} scr_hwinfo_interface;

typedef struct {
	int (*init_method)(void *fdt);
	const scr_hwinfo_interface *interface;

} scr_hwinfo_provider;

typedef struct {
	unsigned char online;
	scr_hwinfo_provider *provider;

} scr_hwinfo_source;



/**
 * Getting hwinfo based on fpga mmio registers
 */

static scr_fpga_reg_data mmio_regs;
static const scr_hwinfo_interface scr_hwinfo_mmio_interface;


static int scr_hwinfo_mmio_init(void *fdt)
{
	int ret = SCR_HWINFO_EMETHOD_DISABLED;
	unsigned long *const mmio_addrs[] = {&mmio_regs.buildid_addr, &mmio_regs.sysclk_addr, &mmio_regs.clsclk_addr};

	if(scr_fdt_parse_fpga_reg(fdt, &mmio_regs) != SBI_OK)
		return ret;

	for(int i = 0; i < sizeof(mmio_addrs) / sizeof(mmio_addrs[0]); i++) {
		if(*mmio_addrs[i] && mmio_read_allowed(*mmio_addrs[i]))
			ret = SCR_HWINFO_OK;
		else
			*mmio_addrs[i] = 0;
	}

	return ret;
}

static int scr_hwinfo_mmio_get_build_id(unsigned long *build_id)
{
	if(mmio_regs.buildid_addr) {
		*build_id = readl_relaxed((void *)mmio_regs.buildid_addr);
		return SCR_HWINFO_OK;
	}

	return SCR_HWINFO_ENO_VALUE;
}

static int scr_hwinfo_mmio_get_sys_clk(unsigned long *sys_clk)
{
	if(mmio_regs.sysclk_addr) {
		*sys_clk = readl_relaxed((void *)mmio_regs.sysclk_addr) * 1000000;
		return SCR_HWINFO_OK;
	}

	return SCR_HWINFO_ENO_VALUE;
}

static int scr_hwinfo_mmio_get_cls_clk(unsigned long *cls_clk)
{
	if(mmio_regs.clsclk_addr) {
		*cls_clk = readl_relaxed((void *)mmio_regs.clsclk_addr) * 1000000;
		return SCR_HWINFO_OK;
	}

	return SCR_HWINFO_ENO_VALUE;
}


static int scr_hwinfo_mmio_get_mtimer_clk(unsigned long *mtimer_clk)
{
	int ret = SCR_HWINFO_ENO_VALUE;
	unsigned long mtimer_clk_calc = 0, sys_clk = 0, cls_clk = 0;

	scr_hwinfo_mmio_interface.get_sys_clk(&sys_clk);
	scr_hwinfo_mmio_interface.get_cls_clk(&cls_clk);
	mtimer_clk_calc = sys_clk;

	if(scr_mtimer_get_clocksource() == SCR_CLKSRC_EXTERNAL) {
		mtimer_clk_calc /= 4;
	} else if(cls_clk) {
		mtimer_clk_calc = cls_clk;
	}

	if(mtimer_clk_calc) {
		*mtimer_clk = mtimer_clk_calc;
		ret = SCR_HWINFO_OK;
	}

	return ret;
}

static int scr_hwinfo_mmio_get_uart_clk(unsigned long *uart_clk)
{
	return scr_hwinfo_mmio_interface.get_sys_clk(uart_clk);
}

static const scr_hwinfo_interface scr_hwinfo_mmio_interface = {
	.get_build_id   = scr_hwinfo_mmio_get_build_id,
	.get_sys_clk    = scr_hwinfo_mmio_get_sys_clk,
	.get_cls_clk    = scr_hwinfo_mmio_get_cls_clk,
	.get_mtimer_clk = scr_hwinfo_mmio_get_mtimer_clk,
	.get_uart_clk   = scr_hwinfo_mmio_get_uart_clk
};

static scr_hwinfo_provider scr_hwinfo_mmio_provider = {
	.init_method = scr_hwinfo_mmio_init,
	.interface   = &scr_hwinfo_mmio_interface
};


/**
 * Getting frequencies from OCRAM structure
*/

#define SCR_HWINFO_BLOB_MAGIG_COMMON (0xE1C5DA10UL)
#define SCR_HWINFO_BLOB_VERSION_BITS (0xF)
#define SCR_HWINFO_BLOB_COMMON(addr) ((scr_hwinfo_blob_common const *)(addr))
#define SCR_HWINFO_BLOB_V1(addr)     ((scr_hwinfo_blob_v1_data const *)(addr))

typedef struct {
	unsigned long magic;          /* magic: 0xe1c5da10 */

} scr_hwinfo_blob_common;

typedef struct {
	scr_hwinfo_blob_common common;
	unsigned long build_id;       /* build id */
	unsigned long sys_clk;        /* system clk (AXI bus frequency) */
	unsigned long cluster_clk;    /* L3 cluster frequency */
	unsigned long mtimer_ext_clk; /* MTIMER external clock frequency */
	unsigned long uart_clk;       /* UART clk */
	unsigned long uart_addr;      /* UART register block start address */
	unsigned long harts_count;    /* number of harts */
	struct cpu_clk_data {
		unsigned long hartid;
		unsigned long clk;
	} cpu_clk[];

} scr_hwinfo_blob_v1_data;


static unsigned long blob_start;

static const scr_hwinfo_interface scr_hwinfo_blob_v1_interface;
static scr_hwinfo_provider scr_hwinfo_blob_provider;

static const scr_hwinfo_interface* supported_blobs[] = {
	&scr_hwinfo_blob_v1_interface
};

static int scr_hwinfo_blob_v1_init(void *fdt)
{
	unsigned long blob_addr = 0, common_magic, blob_version;
	unsigned char blobs_num = sizeof(supported_blobs) / sizeof(supported_blobs[0]);
	int ret = SCR_HWINFO_EMETHOD_DISABLED;

	if(scr_fdt_parse_hwinfo_struct(fdt, &blob_addr) != SBI_OK)
		return ret;

	if(!mmio_read_allowed(blob_addr))
		return ret;

	common_magic = SCR_HWINFO_BLOB_COMMON(blob_addr)->magic;
	blob_version = common_magic & SCR_HWINFO_BLOB_VERSION_BITS;
	common_magic &= ~SCR_HWINFO_BLOB_VERSION_BITS;

	if(common_magic == SCR_HWINFO_BLOB_MAGIG_COMMON && blob_version < blobs_num) {
		blob_start = blob_addr;
		scr_hwinfo_blob_provider.interface = supported_blobs[blob_version];
		ret = SCR_HWINFO_OK;
	}

	return ret;
}

static int scr_hwinfo_blob_v1_get_build_id(unsigned long *build_id)
{
	if(SCR_HWINFO_BLOB_V1(blob_start)->build_id) {
		*build_id = SCR_HWINFO_BLOB_V1(blob_start)->build_id;
		return SCR_HWINFO_OK;
	}

	return SCR_HWINFO_ENO_VALUE;
}

static int scr_hwinfo_blob_v1_get_sys_clk(unsigned long *sys_clk)
{
	if(SCR_HWINFO_BLOB_V1(blob_start)->sys_clk) {
		*sys_clk = SCR_HWINFO_BLOB_V1(blob_start)->sys_clk;
		return SCR_HWINFO_OK;
	}

	return SCR_HWINFO_ENO_VALUE;
}

static int scr_hwinfo_blob_v1_get_mtimer_clk(unsigned long *mtimer_clk)
{
	int ret = SCR_HWINFO_ENO_VALUE;

	if(scr_mtimer_get_clocksource() == SCR_CLKSRC_EXTERNAL && SCR_HWINFO_BLOB_V1(blob_start)->mtimer_ext_clk) {
		*mtimer_clk = SCR_HWINFO_BLOB_V1(blob_start)->mtimer_ext_clk;
		ret = SCR_HWINFO_OK;
	}
	else {
		ret = scr_hwinfo_blob_v1_interface.get_sys_clk(mtimer_clk);
	}

	return ret;
}

static int scr_hwinfo_blob_v1_get_uart_clk(unsigned long *uart_clk)
{
	if(SCR_HWINFO_BLOB_V1(blob_start)->uart_clk) {
		*uart_clk = SCR_HWINFO_BLOB_V1(blob_start)->uart_clk;
		return SCR_HWINFO_OK;
	}

	return SCR_HWINFO_ENO_VALUE;
}

static int scr_hwinfo_blob_v1_get_harts_count(unsigned long *harts_count)
{
	if (SCR_HWINFO_BLOB_V1(blob_start)->harts_count) {
		*harts_count = SCR_HWINFO_BLOB_V1(blob_start)->harts_count;
		return SCR_HWINFO_OK;
	}

	return SCR_HWINFO_ENO_VALUE;
}

static int scr_hwinfo_blob_v1_get_hart_clk(unsigned long hartid, unsigned long *hart_clk)
{
	unsigned long harts_count;

	if (scr_hwinfo_blob_v1_get_harts_count(&harts_count) != SCR_HWINFO_OK)
		return SCR_HWINFO_ENO_VALUE;

	for (unsigned i = 0; i < harts_count; i++) {
		if (SCR_HWINFO_BLOB_V1(blob_start)->cpu_clk[i].hartid == hartid) {
			*hart_clk = SCR_HWINFO_BLOB_V1(blob_start)->cpu_clk[i].clk;
			return SCR_HWINFO_OK;
		}
	}

	return SCR_HWINFO_ENO_VALUE;
}



static const scr_hwinfo_interface scr_hwinfo_blob_v1_interface = {
	.get_build_id    = scr_hwinfo_blob_v1_get_build_id,
	.get_sys_clk     = scr_hwinfo_blob_v1_get_sys_clk,
	.get_mtimer_clk  = scr_hwinfo_blob_v1_get_mtimer_clk,
	.get_uart_clk    = scr_hwinfo_blob_v1_get_uart_clk,
	.get_harts_count = scr_hwinfo_blob_v1_get_harts_count,
	.get_hart_clk    = scr_hwinfo_blob_v1_get_hart_clk
};

static scr_hwinfo_provider scr_hwinfo_blob_provider = {
	.init_method = scr_hwinfo_blob_v1_init,
	.interface = NULL
};



/**
 * Common logic to get info from specific methods
 */

/**
 * @brief List of all available hwinfo sources.
 * Newer one is considered to be better than previos ones, thus it is added at the end of list.
 * The default method is the first one available from the end of the list.
 *
 */
static scr_hwinfo_source existing_sources[] = {
	{ .provider = &scr_hwinfo_mmio_provider, .online = 0 },
	{ .provider = &scr_hwinfo_blob_provider, .online = 0 }
};

typedef int (*get_hwinfo_func)(const scr_hwinfo_interface *interface, void *data, unsigned long *hwinfo_val);

static unsigned char default_src_indx;		// Main/defualt method used to obtain hwinfo. Set during scr_hwinfo_init()
static unsigned char default_src_set = 0;	// Flag corresponding whether main/default method has been set


/**
 * @brief Generic function to retrieve piece of hwinfo.
 * This function provides a generic implementation to retrieve of a specific info.
 * It first attempts to get the info from the default hwinfo source. If the default source
 * fails, it iterates over all other available sources in reverse order, skipping any that
 * are offline, and tries to retrieve info using the provided retrieval method.
 *
 * @param get_hwinfo Function pointer to the specific info retrieval method
 * @param data       Data passed to get_hwinfo
 * @param hwinfo     Pointer to store the retrieved piece of info
 * @return int       Operation status
 */
static int scr_hwinfo_get(get_hwinfo_func get_hwinfo, void *data, unsigned long *hwinfo)
{
	int ret;
	unsigned long hwinfo_val;

	if (!default_src_set)
		return SCR_HWINFO_ENO_METHODS;

	if (hwinfo == NULL)
		return SCR_HWINFO_EBAD_ARG;

	ret = get_hwinfo(existing_sources[default_src_indx].provider->interface, data, &hwinfo_val);
	if (ret == SCR_HWINFO_OK) {
		*hwinfo = hwinfo_val;
		return ret;
	}

	// Iterate through other sources
	for (int i = default_src_indx - 1; i >= 0; i--) {
		if (!existing_sources[i].online)
			continue;

		ret = get_hwinfo(existing_sources[i].provider->interface, data, &hwinfo_val);
		if (ret == SCR_HWINFO_OK) {
			*hwinfo = hwinfo_val;
			break;
		}
	}

	return ret;
}

// Wrappers for specific hwinfo retrieval functions

static int get_build_id(const scr_hwinfo_interface *interface, void *data, unsigned long *build_id)
{
	if(interface->get_build_id == NULL)
		return SCR_HWINFO_ENO_VALUE;

	return interface->get_build_id(build_id);
}

static int get_sys_clk(const scr_hwinfo_interface *interface, void *data, unsigned long *clk_val)
{
	if(interface->get_sys_clk == NULL)
		return SCR_HWINFO_ENO_VALUE;

	return interface->get_sys_clk(clk_val);
}

static int get_mtimer_clk(const scr_hwinfo_interface *interface, void *data, unsigned long *clk_val)
{
	if(interface->get_mtimer_clk == NULL)
		return SCR_HWINFO_ENO_VALUE;

	return interface->get_mtimer_clk(clk_val);
}

static int get_uart_clk(const scr_hwinfo_interface *interface, void *data, unsigned long *clk_val)
{
	if(interface->get_uart_clk == NULL)
		return SCR_HWINFO_ENO_VALUE;

	return interface->get_uart_clk(clk_val);
}

static int get_harts_count(const scr_hwinfo_interface *interface, void *data, unsigned long *clk_val)
{
	if (!interface->get_harts_count)
		return SCR_HWINFO_ENO_VALUE;

	return interface->get_harts_count(clk_val);
}

static int get_hart_clk(const scr_hwinfo_interface *interface, void *data, unsigned long *clk_val)
{
	if (!interface->get_hart_clk)
		return SCR_HWINFO_ENO_VALUE;

	return interface->get_hart_clk(*(unsigned long *) data, clk_val);
}


// Public functions

/**
 * @brief Initialize hwinfo sources and set default hwinfo source.
 * This function initializes all available sources by calling their respective
 * initialization methods. It updates the `online` status of each hwinfo source based
 * on whether its initialization succeeds. If no hwinfo source is successfully initialized,
 * an error is returned. Otherwise, the first successfully initialized hwinfo source
 * (from the highest index) is set as the default source.
 *
 * @param fdt  A pointer to the FDT structure used to initialize the hwinfo sources
 * @return int Operation status
 */
int scr_hwinfo_init(void *fdt)
{
	int ret = SCR_HWINFO_OK;
	unsigned char hwinfo_sources_num;

	if(fdt == NULL)
		return SCR_HWINFO_EBAD_ARG;

	hwinfo_sources_num = sizeof(existing_sources) / sizeof(existing_sources[0]);
	for(int i = hwinfo_sources_num - 1; i >= 0; i--) {
		if(existing_sources[i].provider->init_method(fdt) == SCR_HWINFO_OK) {

			existing_sources[i].online = 1;

			if(!default_src_set) {
				default_src_indx = i;
				default_src_set = 1;
			}
		}
	}
	if(!default_src_set)
		ret = SCR_HWINFO_ENO_METHODS;

	return ret;
}

int scr_hwinfo_get_build_id(unsigned long *build_id)
{
	return scr_hwinfo_get(get_build_id, NULL, build_id);
}

int scr_hwinfo_get_sys_clk(unsigned long *sys_clk)
{
	return scr_hwinfo_get(get_sys_clk, NULL, sys_clk);
}

int scr_hwinfo_get_mtimer_clk(unsigned long *mtimer_clk)
{
	return scr_hwinfo_get(get_mtimer_clk, NULL, mtimer_clk);
}

int scr_hwinfo_get_uart_clk(unsigned long *uart_clk)
{
	return scr_hwinfo_get(get_uart_clk, NULL, uart_clk);
}

int scr_hwinfo_get_harts_count(unsigned long *harts_count)
{
	return scr_hwinfo_get(get_harts_count, NULL, harts_count);
}

int scr_hwinfo_get_hart_clk(unsigned long hartid, unsigned long *hart_clk)
{
	return scr_hwinfo_get(get_hart_clk, (void *)&hartid, hart_clk);
}
