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

#include <sbi/riscv_asm.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_types.h>
#include <syntacore/scr_cache_drv.h>
#include <syntacore/scr_generic.h>

/** general control registers*/
#define SCR_L2_VERSION			0x00
#define SCR_L2_DESCR_0			0x04
#define SCR_L2_DESCR_1			0x08
#define SCR_L2_CACHE_ENABLE		0x10
#define SCR_L2_CACHE_FLUSH		0x14
#define SCR_L2_CACHE_INVAL		0x18

/** defined status registers */
#define SCR_L2_ENABLE_DN		0x20
#define SCR_L2_FLUSH_DN			0x24
#define SCR_L2_INVAL_DN			0x28

#define SCR_L2_CTRL_BUSY		0x2c
#define SCR_L2_MCP				0x40

#define SCR_L2_ERROR_STATUS		0x80
#define SCR_L2_TAG_MEMORY_STATUS	0x88
#define SCR_L2_DATA_MEMORY_STATUS	0x8c
#define SCR_L2_ERROR_IRQ_EN		0x90

#define SCR_L2_SYSCO			0xa0
# define SCR_L2_SYSCO_REQ		BIT(0)
# define SCR_L2_SYSCO_ACK		BIT(8)

#define SCR_L2_MAU_READ_ERR_ADDR	0x100
#define SCR_L2_MAU_WRITE_ERR_ADDR	0x110
#define SCR_L2_NCAU_READ_ERR_ADDR	0x120
#define SCR_L2_NCAU_WRITE_ERR_ADDR	0x130
#define SCR_L2_MAU_READ_ERR_CODE	0x140
#define SCR_L2_MAU_WRITE_ERR_CODE	0x150
#define SCR_L2_NCAU_READ_ERR_CODE	0x160
#define SCR_L2_NCAU_WRITE_ERR_CODE	0x170

#define SCR_L2_ECC_TAG_ERR			0x200
#define SCR_L2_ECC_DATA_ERR			0x300

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

#define L2_CSR_DESCR_MASK_WAYS_OFF		(0x7)
#define L2_CSR_DESCR_MASK_BANKS_OFF		(0xf0000)
#define L2_CSR_DESCR_MASK_LINESZ_LG2_OFF	(0xf0)
#define L2_CSR_DESCR_MASK_LINES_LG2_OFF		(0x1f00)
#define L2_CSR_DESCR_MASK_CORES_OFF		(0xf0000000)

#define SCR_L2_DESCR_BANKS(val)		(((val >> L2_CSR_DESCR_OFFS_BANKS) & \
						L2_CSR_DESCR_MASK_BANKS) + 1)
#define SCR_L2_DESCR_WAYS(val)		(1UL << ((val >> L2_CSR_DESCR_OFFS_WAYS) & \
						L2_CSR_DESCR_MASK_WAYS))
#define SCR_L2_DESCR_LINESZ(val)	(1UL << ((val >> L2_CSR_DESCR_OFFS_LINESZ_LG2) & \
						L2_CSR_DESCR_MASK_LINESZ_LG2))
#define SCR_L2_DESCR_LINES(val)		(1UL << ((val >> L2_CSR_DESCR_OFFS_LINES_LG2) & \
						L2_CSR_DESCR_MASK_LINES_LG2))
#define SCR_L2_DESCR_CORES(val)		(((val >> L2_CSR_DESCR_OFFS_CORES) & \
						L2_CSR_DESCR_MASK_CORES) + 1)

/* Version identifier is taken from L2_VID
 * description of corresponding EAS
 */
#define SCR_L2_VID_V2			0x24062001
#define SCR_L2_CACHE_EVENTS_V1	9
#define SCR_L2_CACHE_EVENTS_V2	36

#define SCR_L2_PMU_CONTROL_BASE	0x400
#define SCR_L2_PMU_CTR_NUM		4

#define SCR_L2_CTR_CONTROL		0x00
#define SCR_L2_CTR_LOW			0x08
#define SCR_L2_CTR_HIGH			0x0c

/** SCR L2 counters control registers */
#define SCR_PMU_BANKS_LOW_BIT		16
#define SCR_PMU_BANKS_HIGH_BIT		19
#define SCR_PMU_BANKS_SEL_MASK		GENMASK(SCR_PMU_BANKS_HIGH_BIT, SCR_PMU_BANKS_LOW_BIT)

#define SCR_L2_PMU_EVENT_SELECTOR	0
#define SCR_L2_PMU_EVENT_SELECTOR_MASK	GENMASK(3, 0)

#ifdef SCR_PMU_DEBUG
#define pr_l2_debug sbi_printf
#define pr_l3_debug sbi_printf
#else
#define pr_l2_debug(...)
#define pr_l3_debug(...)
#endif

/** Features flags */
#define get_cidx_addr(offset, cidx)     (SCR_L2_PMU_CONTROL_BASE + (offset) + ((cidx) << 4))
#define get_cidx_type(x)                ((x & SBI_PMU_EVENT_IDX_TYPE_MASK) >> 16)
#define get_cidx_code(x)                (x & SBI_PMU_EVENT_IDX_CODE_MASK)

#define to_l2_pcache(drv)           container_of(drv, struct scr_l2_cache, drv)

struct scr_l2_cache {
	struct scr_cache_drv drv;
	unsigned long        base;
	unsigned long        size;
	uint32_t             l2_active_events[SCR_CPU_MAX_HARTS][SCR_L2_PMU_CTR_NUM];
};

static unsigned int scr_pmu_hartid(bool llc)
{
	return llc ? 0u : current_hartid();
}

static void l2w(struct scr_l2_cache *l2, u32 val, int reg)
{
	writel(val, (void *)l2->base + reg);
}

static u32 l2r(struct scr_l2_cache *l2, int reg)
{
	return readl((void *)l2->base + reg);
}

static void l2_wait_busy(struct scr_l2_cache *l2)
{
	while (l2r(l2, SCR_L2_CTRL_BUSY))
		;
}

static void scr_l2_enable(struct scr_l2_cache *l2)
{
	u32 l2desc = l2r(l2, SCR_L2_DESCR_0);
	u32 cbmask = (1 << SCR_L2_DESCR_BANKS(l2desc)) - 1;

	l2w(l2, cbmask, SCR_L2_CACHE_ENABLE);

	l2_wait_busy(l2);
}

static void scr_l2_sysco(struct scr_l2_cache *l2, bool enable)
{
	u32 en = enable ? SCR_L2_SYSCO_REQ : 0;

	l2w(l2, en, SCR_L2_SYSCO);

	l2_wait_busy(l2);
}

static void scr_l2_disable(struct scr_l2_cache *l2)
{
	l2w(l2, 0, SCR_L2_CACHE_ENABLE);

	l2_wait_busy(l2);
}

static void scr_l2_inval(struct scr_l2_cache *l2)
{
	l2w(l2, ~0, SCR_L2_CACHE_INVAL);
	l2_wait_busy(l2);
}

static void scr_l2_flush(struct scr_l2_cache *l2)
{
	l2w(l2, ~0, SCR_L2_CACHE_FLUSH);
	l2_wait_busy(l2);
}

static unsigned long scr_l2_version(struct scr_l2_cache *l2)
{
	return l2r(l2, SCR_L2_VERSION);
}

static bool scr_l2_is_valid(struct scr_l2_cache *l2)
{
	return scr_l2_version(l2) != 0u;
}

static void scr_l2_cache_enable(struct scr_cache_drv *drv,
		uint32_t flags)
{
	struct scr_l2_cache *l2 = to_l2_pcache(drv);

	if (!scr_l2_is_valid(l2))
		return;

	scr_l2_disable(l2);

	scr_l2_inval(l2);

	if (!drv->llc)
		scr_l2_sysco(l2, true);

	scr_l2_enable(l2);

	RISCV_FENCE_I;
}

static void scr_l2_cache_disable(struct scr_cache_drv *drv,
		uint32_t flags)
{
	struct scr_l2_cache *l2 = to_l2_pcache(drv);

	if (!scr_l2_is_valid(l2))
		return;

	scr_l2_disable(l2);

	scr_l2_flush(l2);

	if (!drv->llc)
		scr_l2_sysco(l2, false);

	RISCV_FENCE_I;
}

static void scr_l2_cache_inval(struct scr_cache_drv *drv)
{
	struct scr_l2_cache *l2 = to_l2_pcache(drv);

	if (!scr_l2_is_valid(l2))
		return;

	scr_l2_inval(l2);
}

static void scr_l2_cache_flush(struct scr_cache_drv *drv)
{
	struct scr_l2_cache *l2 = to_l2_pcache(drv);

	if (!scr_l2_is_valid(l2))
		return;

	scr_l2_flush(l2);
}

static void scr_l2_cache_print_info(struct scr_cache_drv *drv)
{
	struct scr_l2_cache *l2 = to_l2_pcache(drv);
	unsigned int l2ver, banks, ways, l2dscr;
	unsigned int linesz, lines, cores, size_kb;

	if (!scr_l2_is_valid(l2))
		return;

	l2ver = l2r(l2, SCR_L2_VERSION);
	l2dscr = l2r(l2, SCR_L2_DESCR_0);
	banks = SCR_L2_DESCR_BANKS(l2dscr);
	ways = SCR_L2_DESCR_WAYS(l2dscr);
	linesz = SCR_L2_DESCR_LINESZ(l2dscr);
	lines = SCR_L2_DESCR_LINES(l2dscr);
	cores = SCR_L2_DESCR_CORES(l2dscr);
	size_kb = (lines * linesz * ways * banks) / 1024;
	sbi_printf("L2 [%08x %08x] %uK, %u-way, %u-byte line, %s\n",
		l2ver, l2dscr, size_kb, ways, linesz,
		(cores > 1 ? "shared" : "dedicated"));
	sbi_printf("L2 %u cores, status: %x\n",
			cores, l2r(l2, SCR_L2_CACHE_ENABLE));
}

unsigned long scr_l2_cache_get_version(struct scr_cache_drv *drv)
{
	struct scr_l2_cache *l2 = to_l2_pcache(drv);

	return scr_l2_version(l2);
}

static void scr_l2_cache_get_info(struct scr_cache_drv *drv,
		struct cache_info *l2_info)
{
	struct scr_l2_cache *l2 = to_l2_pcache(drv);
	unsigned int l2dscr;

	if (!scr_l2_is_valid(l2))
		return;

	l2dscr = l2r(l2, SCR_L2_DESCR_0);

	/* According EAS, field contains (L2BANKS - 1), so add 1 here */
	unsigned int banks = EXTRACT_FIELD(l2dscr, L2_CSR_DESCR_MASK_BANKS_OFF) + 1;
	unsigned int ways = 1 << EXTRACT_FIELD(l2dscr, L2_CSR_DESCR_MASK_WAYS_OFF);
	unsigned int lines = 1 << EXTRACT_FIELD(l2dscr, L2_CSR_DESCR_MASK_LINES_LG2_OFF);

	l2_info->block_size = 1 << EXTRACT_FIELD(l2dscr, L2_CSR_DESCR_MASK_LINESZ_LG2_OFF);
	l2_info->size = (lines * l2_info->block_size * ways * banks);
	l2_info->sets = lines;
}

static int scr_l2_cache_domain_init(struct scr_cache_drv *drv)
{
	struct scr_l2_cache *l2 = to_l2_pcache(drv);

	return sbi_domain_root_add_memrange(l2->base, l2->size, PAGE_SIZE,
					    SBI_DOMAIN_MEMREGION_MMIO |
					    SBI_DOMAIN_MEMREGION_M_READABLE |
					    SBI_DOMAIN_MEMREGION_M_WRITABLE);
}

static bool scr_l2_pmu_is_event_active(uint32_t e)
{
	return e != SBI_PMU_EVENT_IDX_INVALID;
}

static unsigned int scr_l2_pmu_events(struct scr_l2_cache *l2)
{

	return scr_l2_version(l2) < SCR_L2_VID_V2 ?
			SCR_L2_CACHE_EVENTS_V1 : SCR_L2_CACHE_EVENTS_V2;
}

static inline void scr_l2_pmu_ctr_write_hw(struct scr_l2_cache *l2,
		uint32_t cidx, uint64_t ival)
{
	uint32_t low = 0, high = 0;
	int addr_low = 0, addr_high = 0;

	low = (uint32_t)(ival & 0xFFFFFFFF);
	high = (uint32_t)(ival >> 32);

	addr_low = get_cidx_addr(SCR_L2_CTR_LOW, cidx);
	addr_high = get_cidx_addr(SCR_L2_CTR_HIGH, cidx);

	l2w(l2, low, addr_low);
	l2w(l2, high, addr_high);
}

static inline void scr_l2_pmu_ctr_write_event(struct scr_l2_cache *l2,
		uint32_t cidx, uint32_t ival)
{
	int addr = get_cidx_addr(0x0, cidx);

	l2w(l2, ival, addr);
	pr_l2_debug("%s : addr=0x%p, ival=0x%lx\n", __func__, addr, ival);
}

static uint32_t scr_l2_banks_num(uint64_t event_data)
{
	return (event_data & SCR_PMU_BANKS_SEL_MASK) >> SCR_PMU_BANKS_LOW_BIT;
}

static int scr_l2_pmu_read_cntr(struct scr_cache_drv *drv,
		unsigned long cidx, uint64_t *cval)
{
	unsigned long low = 0, high_prev = 0, high = 0;
	struct scr_l2_cache *l2 = to_l2_pcache(drv);
	int addr_low, addr_high;

	addr_low  = get_cidx_addr(SCR_L2_CTR_LOW, cidx);
	addr_high = get_cidx_addr(SCR_L2_CTR_HIGH, cidx);

	high_prev = l2r(l2, addr_high);
	low = l2r(l2, addr_low);
	high = l2r(l2, addr_high);

	if (high != high_prev) {
		low = l2r(l2, addr_low);
	}

	*cval = ((uint64_t)high << 32) | low;
	pr_l2_debug("%s cidx=0x%x, addr=0x%p, val=0x%lx\n", __func__, cidx, addr_low, *cval);

	return SBI_OK;
}

static int scr_l2_pmu_get_free_cntr(struct scr_cache_drv *drv, unsigned long mask)
{
	unsigned long idx = 0;
	struct scr_l2_cache *l2 = to_l2_pcache(drv);
	unsigned int hart = scr_pmu_hartid(l2->drv.llc);
	int ret = SBI_ENOTSUPP;

	for_each_set_bit_from(idx, &mask, SCR_L2_PMU_CTR_NUM) {
		if (!scr_l2_pmu_is_event_active(l2->l2_active_events[hart][idx])) {
			ret = idx;
			break;
		}
	}

	return ret;
}

static int scr_l2_pmu_assign_cntr(struct scr_cache_drv *drv, unsigned long cnt_idx,
		unsigned long event_idx, unsigned long event_data)
{
	struct scr_l2_cache *l2 = to_l2_pcache(drv);
	unsigned int id = scr_pmu_hartid(l2->drv.llc);
	unsigned int event_type = event_idx & SCR_L2_PMU_EVENT_SELECTOR_MASK;

	if (event_type >= scr_l2_pmu_events(l2))
		return SBI_EINVAL;

	if (!scr_l2_banks_num(event_data))
		return SBI_EINVAL;

	if (scr_l2_pmu_is_event_active(l2->l2_active_events[id][cnt_idx]))
		return SBI_EINVAL;

	l2->l2_active_events[id][cnt_idx] = event_data | event_type;

	return SBI_OK;
}

static int scr_l2_pmu_free_cntr(struct scr_cache_drv *drv, unsigned long cnt_idx)
{
	struct scr_l2_cache *l2 = to_l2_pcache(drv);
	unsigned int id = scr_pmu_hartid(l2->drv.llc);

	l2->l2_active_events[id][cnt_idx] = SBI_PMU_EVENT_IDX_INVALID;

	return SBI_OK;
}

static int scr_l2_pmu_enable_cntr(struct scr_cache_drv *drv, unsigned long cnt_idx,
		bool enable, bool update, uint64_t val)
{
	struct scr_l2_cache *l2 = to_l2_pcache(drv);
	unsigned int id = scr_pmu_hartid(l2->drv.llc);

	if (!scr_l2_pmu_is_event_active(l2->l2_active_events[id][cnt_idx]))
		return SBI_EINVAL;

	if (update)
		scr_l2_pmu_ctr_write_hw(l2, cnt_idx, val);
	scr_l2_pmu_ctr_write_event(l2, cnt_idx, enable ? l2->l2_active_events[id][cnt_idx] : 0u);

	return SBI_OK;
}

static unsigned long scr_l2_pmu_get_cntrs(struct scr_cache_drv *drv)
{
	return SCR_L2_PMU_CTR_NUM;
}

asm (".macro l2c_busy base, temp			\n"
	"1:										\n"
	"fence									\n"
	"lw		\\temp, " STRINGIFY(SCR_L2_CTRL_BUSY) "(\\base)	\n"
	"bnez		\\temp, 1b					\n"
	".endm									\n"
	);

asm (
	".macro l2c_dis base, temp				\n"
	"sw		zero, " STRINGIFY(SCR_L2_CACHE_ENABLE) "(\\base)\n"
	"l2c_busy	\\base, \\temp				\n"
	"sw		zero, " STRINGIFY(SCR_L2_SYSCO) "(\\base)	\n"
	"l2c_busy	\\base, \\temp				\n"
	".endm									\n"
);

/* Global ASM function scr_l2_pcache_disable_asm
 * Use t0, t1 regs
 */
__asm__ (
	".section .data							\n"
	".align	3								\n"
	"_scr_l2_pcache_base:					\n"
	RISCV_PTR " 0							\n"
	".section .text							\n"
	".align 3								\n"
	".global scr_l2_pcache_disable_asm		\n"
	"scr_l2_pcache_disable_asm:				\n"
	"lla t0, _scr_l2_pcache_base			\n"
	REG_L " t0, 0(t0)						\n"
	"l2c_dis t0, t1							\n"
	"ret									\n"
);

extern unsigned long _scr_l2_pcache_base;

static struct scr_l2_cache l2_cache = {
	.drv = {
		.get_version  = scr_l2_cache_get_version,
		.get_info     = scr_l2_cache_get_info,
		.print_info   = scr_l2_cache_print_info,
		.domain_init  = scr_l2_cache_domain_init,
		.disable      = scr_l2_cache_disable,

		.pmu_get_cntrs     = scr_l2_pmu_get_cntrs,
		.pmu_get_free_cntr = scr_l2_pmu_get_free_cntr,
		.pmu_assign_cntr   = scr_l2_pmu_assign_cntr,
		.pmu_free_cntr     = scr_l2_pmu_free_cntr,
		.pmu_enable_cntr   = scr_l2_pmu_enable_cntr,
		.pmu_read_cntr     = scr_l2_pmu_read_cntr,

		.enable       = scr_l2_cache_enable,
		.flush        = scr_l2_cache_flush,
		.inval        = scr_l2_cache_inval,
	},
};

struct scr_cache_drv *scr_l2_pcache_probe(unsigned long reg_base, unsigned long size,
					  int level)
{
	l2_cache.base = reg_base;
	l2_cache.size = size;
	_scr_l2_pcache_base = reg_base;

	for (int i = 0; i < array_size(l2_cache.l2_active_events); i++)
		for (int j = 0; j < array_size(l2_cache.l2_active_events[0]); j++)
			l2_cache.l2_active_events[i][j] = SBI_PMU_EVENT_IDX_INVALID;

	return &l2_cache.drv;
}

/******************* L3 Cache ********************/

#if __riscv_xlen != 32

#define L3C_VERSION             0x0
#define L3C_DESC                0x8
# define L3C_DESC_CPU_NUM       GENMASK(7, 0)
# define L3C_DESC_BANK_NUM      GENMASK(15, 8)

#define L3C_DESC_BANK           0x10
# define L3C_DESC_BANK_NI       GENMASK(7, 0)
# define L3C_DESC_BANK_NW       GENMASK(15, 8)
# define L3C_DESC_BANK_LW       GENMASK(23, 16)
# define L3C_DESC_BANK_DW       GENMASK(31, 24)
# define L3C_DESC_BANK_AW       GENMASK(39, 32)

#define L3C_CMD_CTRL            0x40
# define L3C_CMD_CTRL_OP        GENMASK(7, 0)
#  define L3C_CMD_CTRL_OP_FLUSH     1
#  define L3C_CMD_CTRL_OP_INVAL     2
#  define L3C_CMD_CTRL_OP_CLEAN     3
# define L3C_CMD_CTRL_VEC       GENMASK(15, 8)

#define L3B_STS(n)              (0x100 + (n) * 0x100)
# define L3B_STS_ROB_EMPTY      BIT(0)
# define L3B_STS_VB_EMPTY       BIT(4)
# define L3B_STS_TP_EMPTY       BIT(8)
# define L3B_STS_RQ_EMPTY       BIT(12)
# define L3B_STS_EMPTY          (L3B_STS_ROB_EMPTY | \
					 L3B_STS_VB_EMPTY  | \
					 L3B_STS_TP_EMPTY  | \
					 L3B_STS_RQ_EMPTY)

#define L3B_CMD_STS(n)          (0x110 + (n) * 0x100)
# define L3B_CMD_STS_DONE       BIT(0)
# define L3B_CMD_STS_DENY       BIT(4)
# define L3B_CMD_STS_MISS       BIT(8)

#define L3B_STEP						(0x100)
#define L3B_PRIVATE_0_BANK_OFF			(L3B_STEP)
#define L3B_PRIVATE_N_BANK_OFF(N)		(L3B_PRIVATE_0_BANK_OFF + (N) * L3B_STEP)
#define L3_REGISTER_SIZE				(8)
#define L3C_DESCR_CACHE_IDX				(1)
#define L3C_DESCR_CACHE_MASK_BANK_NUM	GENMASK(15, 8)
#define L3C_PCE_CTRL_IDX				(19)
#define L3B_PCE_CTRL_IDX				(21)
#define L3B_PCE_CNTR_IDX				(22)
#define L3B_PCE_CNTR_STEP				(0x10)
#define L3B_PCE_CTRL_STEP				(0x10)
#define L3C_DESCR_CACHE_OFFS			(L3C_DESCR_CACHE_IDX * L3_REGISTER_SIZE)
#define L3C_PCE_CTRL_OFFS				(L3C_PCE_CTRL_IDX * L3_REGISTER_SIZE)
#define L3B_PCE_CTRL_OFFS				(L3B_PCE_CTRL_IDX * L3_REGISTER_SIZE)
#define L3C_PCE_CTRL_N_OFF(N)			(L3C_PCE_CTRL_OFFS + (N) * L3_REGISTER_SIZE)
#define L3B_PCE_CNTR_OFFS				(L3B_PCE_CNTR_IDX * L3_REGISTER_SIZE)
#define L3B_PCE_CNTR_0_OFF(BANK)		(L3B_PRIVATE_N_BANK_OFF(BANK) + L3B_PCE_CNTR_OFFS)
#define L3B_PCE_CTRL_0_OFF(BANK)		(L3B_PRIVATE_N_BANK_OFF(BANK) + L3B_PCE_CTRL_OFFS)
#define L3B_PCE_CNTR_N_OFF(BANK, N)		(L3B_PCE_CNTR_0_OFF(BANK) + (N) * L3B_PCE_CNTR_STEP)
#define L3B_PCE_CTRL_N_OFF(BANK, N)		(L3B_PCE_CTRL_0_OFF(BANK) + (N) * L3B_PCE_CTRL_STEP)

#define SCR_L3_PMU_CTR_NUM				4

#define SCR_L3_PMU_EVENT_SELECTOR		0
#define SCR_L3_PMU_EVENT_SELECTOR_MASK  GENMASK(7, 0)
#define SCR_L3_PMU_BANKS_LOW_BIT		8
#define SCR_L3_PMU_BANKS_HIGH_BIT		15
#define SCR_L3_PMU_BANKS_SEL_MASK		\
	GENMASK(SCR_L3_PMU_BANKS_HIGH_BIT, SCR_L3_PMU_BANKS_LOW_BIT)

#define BIT_TO_BYTE_DIVIDER             8
#define SCR_L3_VID_V1                   0x2022010101
#define SCR_L3_CACHE_EVENTS_V1          15
#define SCR_L3_CACHE_EVENTS_V2          16

/* v1 and v2 event codes starts from 1 */
#define SCR_L3_CACHE_NOP                0

#ifdef SCR_L3_PMU_DEBUG
#define pr_l3_debug sbi_printf
#else
#define pr_l3_debug(...)
#endif

#define to_l3_pcache(drv)       container_of(drv, struct scr_l3_cache, drv)

struct l3_pmu_event {
	uint64_t	ctrl;
	uint32_t	banks_mask;
	int		banks;
};

struct scr_l3_cache {
	struct scr_cache_drv drv;
	unsigned long        base;
	unsigned long        size;
	struct l3_pmu_event  l3_active_events[SCR_L3_PMU_CTR_NUM];
};

static u64 l3r(struct scr_l3_cache *l3, int reg)
{
	return readq((void *)l3->base + reg);
}

static void l3w(struct scr_l3_cache *l3, u64 val, int reg)
{
	writeq(val, (void *)l3->base + reg);
}

static int l3c_nb(struct scr_l3_cache *l3)
{
	return EXTRACT_FIELD(l3r(l3, L3C_DESC), L3C_DESC_BANK_NUM);
}

static void l3c_op(struct scr_l3_cache *l3, int op)
{
	int i, rdy;
	int nb = l3c_nb(l3);
	u64 cmd = INSERT_FIELD(0, L3C_CMD_CTRL_OP, op);

	cmd = INSERT_FIELD(cmd, L3C_CMD_CTRL_VEC, (1 << nb) - 1);
	l3w(l3, cmd, L3C_CMD_CTRL);

	do {
		rdy = 0;
		for (i = 0; i < nb; i++)
			if (l3r(l3, L3B_CMD_STS(i)) & L3B_CMD_STS_DONE)
				rdy++;

	} while (rdy < nb);

	do {
		rdy = 0;
		for (i = 0; i < nb; i++)
			if ((l3r(l3, L3B_STS(i)) & L3B_STS_EMPTY) == L3B_STS_EMPTY)
				rdy++;

	} while (rdy < nb);
}

static unsigned long scr_l3_get_version(struct scr_l3_cache *l3)
{
	return l3r(l3, L3C_VERSION);
}

static unsigned long scr_l3_cache_get_version(struct scr_cache_drv *drv)
{
	struct scr_l3_cache *l3 = to_l3_pcache(drv);

	return scr_l3_get_version(l3);
}

static void scr_l3_cache_print_info(struct scr_cache_drv *drv)
{
	u64 val;
	unsigned int nc, nb, ni, nw, ls, dw, aw, sz;
	struct scr_l3_cache *l3 = to_l3_pcache(drv);

	val = l3r(l3, L3C_DESC);
	nc = EXTRACT_FIELD(val, L3C_DESC_CPU_NUM);
	nb = EXTRACT_FIELD(val, L3C_DESC_BANK_NUM);

	val = l3r(l3, L3C_DESC_BANK);
	ni =  1 << EXTRACT_FIELD(val, L3C_DESC_BANK_NI);
	nw = 1 << EXTRACT_FIELD(val, L3C_DESC_BANK_NW);
	ls = 1 << (EXTRACT_FIELD(val, L3C_DESC_BANK_LW) - 3);
	dw = 1 << EXTRACT_FIELD(val, L3C_DESC_BANK_DW);
	aw = EXTRACT_FIELD(val, L3C_DESC_BANK_AW);
	sz = (ls * ni * nw * nb);

	sbi_printf("\n== L3 cache info ==\n");
	sbi_printf("#CPU\t\t%d\n", nc);
	sbi_printf("#bank\t\t%d\n", nb);
	sbi_printf("#index\t\t%d\n", ni);
	sbi_printf("#way\t\t%d\n", nw);
	sbi_printf("line size\t\t%dB\n", ls);
	sbi_printf("data width\t\t%d\n", dw);
	sbi_printf("addr width\t\t%d\n", aw);
	sbi_printf("L3 cache size\t\t%dK\n", sz >> 10);
	sbi_printf("== L3 cache info ==\n\n");
}

static void scr_l3_cache_get_info(struct scr_cache_drv *drv,
		struct cache_info *l3_info)
{
	struct scr_l3_cache *l3 = to_l3_pcache(drv);

	unsigned int l3dscr = l3r(l3, L3C_DESC);
	unsigned int banks = EXTRACT_FIELD(l3dscr, L3C_DESC_BANK_NUM);

	unsigned int l3dscr_bank = l3r(l3, L3C_DESC_BANK);
	unsigned int ways = 1 << EXTRACT_FIELD(l3dscr_bank, L3C_DESC_BANK_NW);
	unsigned int index =  1 << EXTRACT_FIELD(l3dscr_bank, L3C_DESC_BANK_NI);

	/* Since line size in L3 is written in bits, to get size in bytes it is divided by 8 */
	l3_info->block_size = (1 << (EXTRACT_FIELD(l3dscr_bank, L3C_DESC_BANK_LW)))/BIT_TO_BYTE_DIVIDER;
	l3_info->size = (index * l3_info->block_size * ways * banks);
	l3_info->sets = index;
}

static int scr_l3_cache_domain_init(struct scr_cache_drv *drv)
{
	struct scr_l3_cache *l3 = to_l3_pcache(drv);

	return sbi_domain_root_add_memrange(l3->base, l3->size, PAGE_SIZE,
					    SBI_DOMAIN_MEMREGION_MMIO |
					    SBI_DOMAIN_MEMREGION_M_READABLE |
					    SBI_DOMAIN_MEMREGION_M_WRITABLE);
}

static void scr_l3_cache_flush(struct scr_cache_drv *drv)
{
	struct scr_l3_cache *l3 = to_l3_pcache(drv);

	l3c_op(l3, L3C_CMD_CTRL_OP_FLUSH);
}

static void scr_l3_cache_inval(struct scr_cache_drv *drv)
{
	struct scr_l3_cache *l3 = to_l3_pcache(drv);

	l3c_op(l3, L3C_CMD_CTRL_OP_INVAL);
}

static int scr_l3_events(struct scr_l3_cache *l3)
{
	return scr_l3_get_version(l3) <= SCR_L3_VID_V1 ?
			SCR_L3_CACHE_EVENTS_V1 : SCR_L3_CACHE_EVENTS_V2;
}

static uint32_t scr_l3_event_banks_mask(uint64_t event_data)
{
	return (event_data & SCR_L3_PMU_BANKS_SEL_MASK) >> SCR_L3_PMU_BANKS_LOW_BIT;
}

static bool scr_l3_is_event_active(const struct l3_pmu_event *e)
{
	return (e->ctrl != SCR_L3_CACHE_NOP);
}

static unsigned long scr_l3_banks(struct scr_l3_cache *l3)
{
	uint64_t descr = l3r(l3, L3C_DESCR_CACHE_OFFS);

	return EXTRACT_FIELD(descr, L3C_DESCR_CACHE_MASK_BANK_NUM);
}

static inline void scr_l3_pmu_ctr_write_event(struct scr_l3_cache *l3,
		unsigned long cidx, uint64_t ival)
{
	l3w(l3, ival, L3C_PCE_CTRL_N_OFF(cidx));
	pr_l3_debug("%s addr=0x%p, ival=0x%lx\n", __func__, addr, ival);
}

static inline void scr_l3_pmu_bank_write_ctrl(struct scr_l3_cache *l3,
		unsigned long banks_mask, unsigned long cidx, uint64_t ival)
{
	int bank = 0;

	for_each_set_bit_from(bank, &banks_mask, scr_l3_banks(l3)) {
		l3w(l3, ival, L3B_PCE_CTRL_N_OFF(bank, cidx));
		pr_l3_debug("%s addr=0x%p, ival=0x%lx\n", __func__, addr, ival);
	}
}

static int scr_l3_pmu_read(struct scr_l3_cache *l3,
		unsigned long cidx, unsigned long *dval)
{
	unsigned long banks_mask;
	uint64_t val = 0U;
	int bank = 0;

	banks_mask = l3->l3_active_events[cidx].banks_mask;

	for_each_set_bit_from(bank, &banks_mask, scr_l3_banks(l3)) {
		val += l3r(l3, L3B_PCE_CNTR_N_OFF(bank, cidx));

		pr_l3_debug("%s addr=0x%p bank=0x%x, cidx=0x%x, val=%lu\n", __func__,
			    addr, bank, cidx, val);
	}
	*dval = val;

	return SBI_OK;
}

static int scr_l3_pmu_write_cntr(struct scr_l3_cache *l3,
		unsigned long banks_mask, unsigned long cidx, uint64_t val)
{
	int bank = 0;

	for_each_set_bit_from(bank, &banks_mask, scr_l3_banks(l3)) {
		pr_l3_debug("%s addr=0x%p bank=0x%x, cidx=0x%x, val=%lu\n", __func__,
			    addr, bank, cidx, val);

		l3w(l3, val, L3B_PCE_CNTR_N_OFF(bank, cidx));
	}

	return SBI_OK;
}

static void scr_l3_pmu_ctr_start_hw(struct scr_l3_cache *l3,
		uint32_t banks_mask, unsigned long cidx, uint64_t ctrl_val,
		uint64_t cntr_val, bool cntr_update)
{
	if (cntr_update)
		scr_l3_pmu_write_cntr(l3, banks_mask, cidx, cntr_val);

	scr_l3_pmu_bank_write_ctrl(l3, banks_mask, cidx, ctrl_val);

	pr_l3_debug("%s banks_mask=0x%x, cidx=0x%x, ctrl=0x%lx, val=%lu\n", __func__,
		    banks_mask, cidx, ctrl_val, cntr_val);
}

static void scr_l3_pmu_ctr_stop_hw(struct scr_l3_cache *l3,
		uint32_t banks_mask, unsigned long cidx,
		uint64_t cntr_val, bool cntr_update)
{
	scr_l3_pmu_ctr_write_event(l3, cidx, SCR_L3_CACHE_NOP);
	if (cntr_update)
		scr_l3_pmu_write_cntr(l3, banks_mask, cidx, cntr_val);
}


static int scr_l3_pmu_read_cntr(struct scr_cache_drv *drv,
		unsigned long cidx, uint64_t *cval)
{
	struct scr_l3_cache *l3 = to_l3_pcache(drv);

	return scr_l3_pmu_read(
		l3, cidx, cval
	);
}

static unsigned long scr_l3_pmu_get_cntrs(struct scr_cache_drv *drv)
{
	return SCR_L3_PMU_CTR_NUM;
}

static int scr_l3_pmu_get_free_cntr(struct scr_cache_drv *drv, unsigned long mask)
{
	unsigned long idx = 0;
	struct scr_l3_cache *l3 = to_l3_pcache(drv);
	int ret = SBI_ENOTSUPP;

	for_each_set_bit_from(idx, &mask, SCR_L3_PMU_CTR_NUM) {
		const struct l3_pmu_event *e = &l3->l3_active_events[idx];

		if (!scr_l3_is_event_active(e)) {
			ret = idx;
			break;
		}
	}

	return ret;
}

static int scr_l3_pmu_assign_cntr(struct scr_cache_drv *drv, unsigned long cnt_idx,
		unsigned long event_idx, unsigned long event_data)
{
	struct scr_l3_cache *l3 = to_l3_pcache(drv);
	unsigned int event_type = event_idx & SCR_L3_PMU_EVENT_SELECTOR_MASK;
	struct l3_pmu_event *e = &l3->l3_active_events[cnt_idx];
	unsigned long banks_mask;
	int l3_banks;
	int banks;

	l3_banks = scr_l3_banks(l3);
	if (l3_banks > sizeof(banks_mask) * 8)
		l3_banks = sizeof(banks_mask) * 8;

	banks_mask  = scr_l3_event_banks_mask(event_data);
	banks_mask &= GENMASK(l3_banks - 1, 0);
	banks       = sbi_popcount(banks_mask);

	if (event_type >= scr_l3_events(l3) || event_type == SCR_L3_CACHE_NOP || !banks)
		return SBI_EINVAL;

	if (scr_l3_is_event_active(e))
		return SBI_EINVAL;

	e->banks_mask = banks_mask;
	e->ctrl = event_type;
	e->banks = banks;

	return SBI_OK;
}

static int scr_l3_pmu_free_cntr(struct scr_cache_drv *drv, unsigned long cnt_idx)
{
	struct scr_l3_cache *l3 = to_l3_pcache(drv);

	l3->l3_active_events[cnt_idx].ctrl = SCR_L3_CACHE_NOP;
	return SBI_OK;
}

static int scr_l3_pmu_enable_cntr(struct scr_cache_drv *drv, unsigned long cnt_idx,
		bool enable, bool update, uint64_t val)
{
	struct scr_l3_cache *l3 = to_l3_pcache(drv);
	struct l3_pmu_event *event;

	event = &l3->l3_active_events[cnt_idx];

	if (!scr_l3_is_event_active(event))
		return SBI_EINVAL;

	val = val / event->banks;

	if (enable)
		scr_l3_pmu_ctr_start_hw(l3, event->banks_mask,
				cnt_idx, event->ctrl, val, update);
	else
		scr_l3_pmu_ctr_stop_hw(l3, event->banks_mask,
				cnt_idx, val, update);

	return SBI_OK;
}

static struct scr_l3_cache l3_cache = {
	.drv = {
		.get_version  = scr_l3_cache_get_version,
		.get_info     = scr_l3_cache_get_info,
		.domain_init  = scr_l3_cache_domain_init,
		.print_info   = scr_l3_cache_print_info,

		.pmu_get_cntrs     = scr_l3_pmu_get_cntrs,
		.pmu_get_free_cntr = scr_l3_pmu_get_free_cntr,
		.pmu_assign_cntr   = scr_l3_pmu_assign_cntr,
		.pmu_free_cntr     = scr_l3_pmu_free_cntr,
		.pmu_enable_cntr   = scr_l3_pmu_enable_cntr,
		.pmu_read_cntr     = scr_l3_pmu_read_cntr,

		.flush        = scr_l3_cache_flush,
		.inval        = scr_l3_cache_inval,
	},
};

struct scr_cache_drv *scr_l3_pcache_probe(unsigned long reg_base, unsigned long size,
					  int level)
{
	l3_cache.base = reg_base;
	l3_cache.size = size;
	l3_cache.drv.llc = true;

	for (int i = 0; i < SCR_L3_PMU_CTR_NUM; i++)
		l3_cache.l3_active_events[i].ctrl = SCR_L3_CACHE_NOP;

	return &l3_cache.drv;
}

#else // __riscv_xlen

struct scr_cache_drv *scr_l3_pcache_probe(unsigned long base __attribute__ ((unused)),
					  unsigned long size __attribute__ ((unused)),
					  int level __attribute__ ((unused)))
{
	return NULL;
}

#endif // __riscv_xlen
