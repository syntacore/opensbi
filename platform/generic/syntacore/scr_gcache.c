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
#include <sbi/sbi_domain.h>
#include <sbi/sbi_error.h>
#include <syntacore/scr_cache_drv.h>
#include <syntacore/scr_generic.h>

/* Currently only 64 bit access is supported */
#if __riscv_xlen != 32

#define GC_VERSION_ID                           0x8
#define GC_VERSION_ID_YEAR_MASK                 GENMASK(63, 48)
#define GC_VERSION_ID_MONTH_MASK                GENMASK(47, 40)
#define GC_VERSION_ID_DAY_MASK                  GENMASK(39, 32)
#define GC_VERSION_ID_RELEASE_MASK              GENMASK(31, 0)

#define SCR_GC_RELEASE(x, y, z)                 ((x) * 0x100 + (y) * 0x10 + (z))

#define GC_CLUSTER_INFO                         0x10
#define GC_CLUSTER_INFO_NODE_ID_MASK            GENMASK(63, 48)
#define GC_CLUSTER_INFO_SHARER_NUM_MASK         GENMASK(47, 32)
#define GC_CLUSTER_INFO_BANK_NUM_MASK           GENMASK(31, 16)
#define GC_CLUSTER_INFO_CLUSTER_NUM_MASK        GENMASK(15, 0)

#define GC_CACHE_INFO                           0x18
#define GC_CACHE_INFO_PERF_CNTR_NUM_MASK        GENMASK(55, 48)
#define GC_CACHE_INFO_DRAM_LATENCY_MASK         GENMASK(47, 40)
#define GC_CACHE_INFO_OFFSET_WIDTH_MASK         GENMASK(39, 32)
#define GC_CACHE_INFO_DATA_WAY_NUM_MASK         GENMASK(31, 24)
#define GC_CACHE_INFO_TAGC_WAY_NUM_MASK         GENMASK(23, 16)
#define GC_CACHE_INFO_SCREW_GRP_NUM_MASK        GENMASK(15,  8)
#define GC_CACHE_INFO_INDEX_WIDTH_MASK          GENMASK(7,  0)

#define GC_CACHE_CTRL                           0x20
#define GC_CACHE_CTRL_CACHING_ENABLE            BIT(32)
#define GC_CACHE_CTRL_COHERENCY_ENABLE          BIT(24)
#define GC_CACHE_CTRL_REP_FREQ_PRIO             BIT(16)
#define GC_CACHE_CTRL_REP_INS_DISTANT           BIT(8)
#define GC_CACHE_CTRL_NO_SILENT_EVICT           BIT(0)

#define GC_CACHE_STATUS                         0x28

#define GC_FEAT_INFO                            0x30
#define GC_FEAT_INFO_DIRECT_CACHE_TRANSFER      BIT(8)
#define GC_FEAT_INFO_DIRECT_MEM_TRANSFER        BIT(0)

#define GC_CMD_CTRL                             0x48
#define GC_CMD_CTRL_WAY_OFFSET                  24
#define GC_CMD_CTRL_INDEX_OFFSET                8

#define GC_CMD_STATUS                           0x98
#define GC_CMD_STATUS_CODE                      GENMASK(15, 8)
#define GC_CMD_STATUS_DONE                      BIT(0)

#define GC_PCE_CTRL                             0xC0
#define GC_PCE_CTRL_OPCODE_MASK                 GENMASK(7, 0)

#define GC_PCE_CNTR                             0xC8

#define GC_CNTR_STEP                            0x10
#define GC_BANK_STEP                            0x200

#define SCR_GC_PMU_CTR_NUM                      4
#define SCR_GC_PMU_EVENTS_V2_RELEASE            SCR_GC_RELEASE(1, 4, 0)
#define SCR_GC_PMU_EVENTS_V1                    16
#define SCR_GC_PMU_EVENTS_V2                    18

#define SCR_GC_EVENT_NOP                        0

#define SCR_GC_PMU_EVENT_SELECTOR_MASK          GENMASK(7, 0)

#define QUIRK_ONE_BANK                          BIT(0)

#define to_gcache(drv)                          container_of(drv, struct scr_gcache, drv)

enum cmd_ctrl_opcodes {
	CMD_CTRL_OPCODE_NOP = 0,
	CMD_CTRL_OPCODE_FLUSH_ALL,
	CMD_CTRL_OPCODE_INVALID_ALL,
	CMD_CTRL_OPCODE_CLEAN_ALL,
	CMD_CTRL_OPCODE_LOAD_BY_ADDR,
	CMD_CTRL_OPCODE_LOAD_BY_SET_WAY,
	CMD_CTRL_OPCODE_DMB,
	CMD_CTRL_OPCODE_DSB
};

enum status_codes {
	STATUS_CODE_SUCCESS = 0,
	STATUS_CODE_BAD_INDEX,
	STATUS_CODE_BAD_SKEW_GRP,
	STATUS_CODE_BAD_WAY,
	STATUS_CODE_BAD_ADDR,
	STATUS_CODE_CACHE_MISS,
};

struct gc_version {
	unsigned int release;
	unsigned int day;
	unsigned int month;
	unsigned int year;
};

struct gc_pmu_event {
	uint64_t      ctrl;
	unsigned long banks_mask;
};

struct scr_gcache {
	struct scr_cache_drv drv;

	unsigned long        base;
	unsigned long        size;
	uint64_t             quirks;
	int                  level;

	struct gc_pmu_event  active_events[SCR_CPU_MAX_HARTS][SCR_GC_PMU_CTR_NUM];

	unsigned long        event_bank_mask;
};

static uint64_t gc_r(struct scr_gcache *gc, unsigned long reg)
{
	return readq((void *)(gc->base + reg));
}

static unsigned int gc_banks_num(struct scr_gcache *gc)
{
	uint64_t cluster_info = gc_r(gc, GC_CLUSTER_INFO);

	return EXTRACT_FIELD(cluster_info, GC_CLUSTER_INFO_BANK_NUM_MASK);
}

static uint64_t gc_bank_r(struct scr_gcache *gc,
		unsigned long bidx, unsigned long reg)
{
	if (!!(gc->quirks & QUIRK_ONE_BANK) && (gc_banks_num(gc) == 1))
		return gc_r(gc, reg);
	return gc_r(gc, GC_BANK_STEP * (bidx + 1) + reg);
}

static uint64_t gc_cntr_r(struct scr_gcache *gc,
		unsigned long bidx, unsigned long cidx, unsigned long reg)
{
	return gc_bank_r(gc, bidx, GC_CNTR_STEP * cidx + reg);
}

static void gc_w(struct scr_gcache *gc, unsigned long reg, uint64_t val)
{
	writeq(val, (void *)(gc->base + reg));
}

static void gc_bank_w(struct scr_gcache *gc,
		unsigned long bidx, unsigned long reg, uint64_t val)
{
	if (!!(gc->quirks & QUIRK_ONE_BANK) && (gc_banks_num(gc) == 1))
		gc_w(gc, reg, val);
	else
		gc_w(gc, (bidx + 1) * GC_BANK_STEP + reg, val);
}

static void gc_cntr_w(struct scr_gcache *gc, unsigned long bidx,
		unsigned long cidx, unsigned long reg, uint64_t val)
{
	gc_bank_w(gc, bidx, cidx * GC_CNTR_STEP + reg, val);
}

static unsigned int scr_pmu_hartid(bool llc)
{
	return llc ? 0u : current_hartid();
}

static unsigned int gc_clusters_num(struct scr_gcache *gc)
{
	uint64_t cluster_info = gc_r(gc, GC_CLUSTER_INFO);

	return EXTRACT_FIELD(cluster_info, GC_CLUSTER_INFO_CLUSTER_NUM_MASK);
}

static unsigned int gc_sharer_num(struct scr_gcache *gc)
{
	uint64_t cluster_info = gc_r(gc, GC_CLUSTER_INFO);

	return EXTRACT_FIELD(cluster_info, GC_CLUSTER_INFO_SHARER_NUM_MASK);
}

static unsigned int gc_node_id(struct scr_gcache *gc)
{
	uint64_t cluster_info = gc_r(gc, GC_CLUSTER_INFO);

	return EXTRACT_FIELD(cluster_info, GC_CLUSTER_INFO_NODE_ID_MASK);
}

static unsigned int gc_index_width(struct scr_gcache *gc)
{
	uint64_t cache_info = gc_r(gc, GC_CACHE_INFO);

	return EXTRACT_FIELD(cache_info, GC_CACHE_INFO_INDEX_WIDTH_MASK);
}

static unsigned int gc_screw_grp_num(struct scr_gcache *gc)
{
	uint64_t cache_info = gc_r(gc, GC_CACHE_INFO);

	return EXTRACT_FIELD(cache_info, GC_CACHE_INFO_SCREW_GRP_NUM_MASK);
}

static unsigned int gc_tag_way_num(struct scr_gcache *gc)
{
	uint64_t cache_info = gc_r(gc, GC_CACHE_INFO);

	return EXTRACT_FIELD(cache_info, GC_CACHE_INFO_TAGC_WAY_NUM_MASK);
}

static unsigned int gc_data_way_num(struct scr_gcache *gc)
{
	uint64_t cache_info = gc_r(gc, GC_CACHE_INFO);

	return EXTRACT_FIELD(cache_info, GC_CACHE_INFO_DATA_WAY_NUM_MASK);
}

static unsigned int gc_offset_width(struct scr_gcache *gc)
{
	uint64_t cache_info = gc_r(gc, GC_CACHE_INFO);

	return EXTRACT_FIELD(cache_info, GC_CACHE_INFO_OFFSET_WIDTH_MASK);
}

static unsigned int gc_data_ram_latency(struct scr_gcache *gc)
{
	uint64_t cache_info = gc_r(gc, GC_CACHE_INFO);

	return EXTRACT_FIELD(cache_info, GC_CACHE_INFO_DRAM_LATENCY_MASK);
}

static unsigned int gc_perf_cntr_num(struct scr_gcache *gc)
{
	uint64_t cache_info = gc_r(gc, GC_CACHE_INFO);
	unsigned int perf_cnt =
			EXTRACT_FIELD(cache_info, GC_CACHE_INFO_PERF_CNTR_NUM_MASK);

	/* Event table supports only SCR_GC_PMU_CTR_NUM counters */
	if (perf_cnt > SCR_GC_PMU_CTR_NUM)
		perf_cnt = SCR_GC_PMU_CTR_NUM;

	return perf_cnt;
}

static bool gc_direct_mem_is_supported(struct scr_gcache *gc)
{
	uint64_t feat_info = gc_r(gc, GC_FEAT_INFO);

	return !!(feat_info & GC_FEAT_INFO_DIRECT_MEM_TRANSFER);
}


static bool gc_direct_cache_is_supported(struct scr_gcache *gc)
{
	uint64_t feat_info = gc_r(gc, GC_FEAT_INFO);

	return !!(feat_info & GC_FEAT_INFO_DIRECT_CACHE_TRANSFER);
}

static int gc2error_code(unsigned int gc_err)
{
	switch (gc_err)	{
	case STATUS_CODE_SUCCESS:
		return SBI_OK;
	case STATUS_CODE_BAD_INDEX:
	case STATUS_CODE_BAD_SKEW_GRP:
	case STATUS_CODE_BAD_WAY:
	case STATUS_CODE_BAD_ADDR:
	case STATUS_CODE_CACHE_MISS:
		return SBI_EINVALID_ADDR;
	default:
		return SBI_EINVAL;
	}
}

static int gc_cmd_run(struct scr_gcache *gc,
				enum cmd_ctrl_opcodes opcode,
				uint64_t index, uint64_t way)
{
	uint64_t cmd;
	uint64_t status;
	bool done;

	cmd = opcode | (index << GC_CMD_CTRL_INDEX_OFFSET) |
			(way << GC_CMD_CTRL_WAY_OFFSET);
	gc_w(gc, GC_CMD_CTRL, cmd);

	for (int bank = 0; bank < gc_banks_num(gc); bank++) {
		uint64_t status_code;

		do {
			status = gc_bank_r(gc, bank, GC_CMD_STATUS);
			done = !!(status & GC_CMD_STATUS_DONE);
		} while (!done);

		status_code = EXTRACT_FIELD(status, GC_CMD_STATUS_CODE);

		if (status_code	!= STATUS_CODE_SUCCESS)
			return gc2error_code(status_code);
	}

	return SBI_OK;
}

static int gc_flush_all(struct scr_gcache *gc)
{
	return gc_cmd_run(gc, CMD_CTRL_OPCODE_FLUSH_ALL, 0, 0);
}

static int gc_inval_all(struct scr_gcache *gc)
{
	return gc_cmd_run(gc, CMD_CTRL_OPCODE_INVALID_ALL, 0, 0);
}

static void gc_enable(struct scr_gcache *gc, uint32_t flags)
{
	uint64_t ctrl = gc_r(gc, GC_CACHE_CTRL);
	uint64_t status;
	bool     done;

	ctrl |= GC_CACHE_CTRL_CACHING_ENABLE;

	if (!gc->drv.llc)
		ctrl |= GC_CACHE_CTRL_COHERENCY_ENABLE;

	gc_w(gc, GC_CACHE_CTRL, ctrl);

	for (int bank = 0; bank < gc_banks_num(gc); bank++) {
		do {
			status = gc_bank_r(gc, bank, GC_CACHE_STATUS);
			done = (status & ctrl) == ctrl;
		} while (!done);
	}
}

static void gc_disable(struct scr_gcache *gc, uint32_t flags)
{
	uint64_t ctrl = gc_r(gc, GC_CACHE_CTRL);
	uint64_t status;
	bool     done;

	ctrl &= ~GC_CACHE_CTRL_CACHING_ENABLE;

	if (!gc->drv.llc)
		ctrl &= ~GC_CACHE_CTRL_COHERENCY_ENABLE;

	gc_w(gc, GC_CACHE_CTRL, ctrl);

	for (int bank = 0; bank < gc_banks_num(gc); bank++) {
		do {
			status = gc_bank_r(gc, bank, GC_CACHE_STATUS);
			done = (status & ctrl) == ctrl;
		} while (!done);
	}
}

static void gc_version(struct scr_gcache *gc, struct gc_version *ver)
{
	uint64_t version_id = gc_r(gc, GC_VERSION_ID);

	ver->release = EXTRACT_FIELD(version_id, GC_VERSION_ID_RELEASE_MASK);
	ver->day     = EXTRACT_FIELD(version_id, GC_VERSION_ID_DAY_MASK);
	ver->month   = EXTRACT_FIELD(version_id, GC_VERSION_ID_MONTH_MASK);
	ver->year    = EXTRACT_FIELD(version_id, GC_VERSION_ID_YEAR_MASK);
}

static bool gc_is_event_active(const struct gc_pmu_event *e)
{
	return (e->ctrl != SCR_GC_EVENT_NOP);
}

static uint32_t scr_event_banks_mask(struct scr_gcache *gc, uint64_t event_data)
{
	return EXTRACT_FIELD(event_data, gc->event_bank_mask);
}

static void gc_pmu_cntr_setval(struct scr_gcache *gc,
		unsigned long bank_mask, unsigned long cidx, uint64_t val)
{
	unsigned long bank = 0u;

	for_each_set_bit_from(bank, &bank_mask, gc_banks_num(gc))
		gc_cntr_w(gc, bank, cidx, GC_PCE_CNTR, val);
}

static void gc_pmu_ctrl_setval(struct scr_gcache *gc,
		unsigned long bank_mask, unsigned long cidx, uint64_t val)
{
	unsigned long bank = 0u;

	for_each_set_bit_from(bank, &bank_mask, gc_banks_num(gc))
		gc_cntr_w(gc, bank, cidx, GC_PCE_CTRL, val);
}

static void gc_pmu_ctr_start(struct scr_gcache *gc,
		unsigned long bank_mask, unsigned long cidx, uint64_t ctrl_val,
		uint64_t cntr_val, bool cntr_update)
{
	if (cntr_update)
		gc_pmu_cntr_setval(gc, bank_mask, cidx, cntr_val);

	gc_pmu_ctrl_setval(gc, bank_mask, cidx, ctrl_val);
}

static void gc_pmu_ctr_stop(struct scr_gcache *gc,
		unsigned long bank_mask, unsigned long cidx,
		uint64_t cntr_val, bool cntr_update)
{
	gc_pmu_ctrl_setval(gc, bank_mask, cidx, SCR_GC_EVENT_NOP);
	if (cntr_update)
		gc_pmu_cntr_setval(gc, bank_mask, cidx, cntr_val);
}

static void scr_gcache_inval(struct scr_cache_drv *drv)
{
	struct scr_gcache *gc = to_gcache(drv);

	gc_inval_all(gc);
}

static void scr_gcache_flush(struct scr_cache_drv *drv)
{
	struct scr_gcache *gc = to_gcache(drv);

	gc_flush_all(gc);
}

static void scr_gcache_enable(struct scr_cache_drv *drv,
		uint32_t flags)
{
	struct scr_gcache *gc = to_gcache(drv);

	gc_enable(gc, flags);

	RISCV_FENCE_I;
}

static void scr_gcache_disable(struct scr_cache_drv *drv,
		uint32_t flags)
{
	struct scr_gcache *gc = to_gcache(drv);

	gc_disable(gc, flags);

	RISCV_FENCE_I;
}

static unsigned long scr_gcache_get_version( struct scr_cache_drv *drv)
{
	struct scr_gcache *gc = to_gcache(drv);

	/* GC version is in another format then
	 * l2 pcache, but still allow comparison with l2 pcache
	 */
	return gc_r(gc, GC_VERSION_ID);
}

static void scr_gcache_print_info(struct scr_cache_drv *drv)
{
	struct scr_gcache *gc = to_gcache(drv);
	struct gc_version ver;

	gc_version(gc, &ver);

	sbi_printf("Generic Cache L%d ver %x %x.%x.%x\n"
			"\t banks number: %u\n"
			"\t cluster number: %u\n"
			"\t sharer number: %u\n"
			"\t node_id: %u\n"
			"\t index width: %u\n"
			"\t screw group number: %u\n"
			"\t tag way number: %u\n"
			"\t data way number: %u\n"
			"\t offset width: %u\n"
			"\t data ram latency: %u\n"
			"\t performance counters number: %u\n"
			"\t direct memory transfer supporting: %s\n"
			"\t direct cache transfer supporting: %s\n",
			gc->level,
			ver.release, ver.day, ver.month, ver.year,
			gc_banks_num(gc),
			gc_clusters_num(gc),
			gc_sharer_num(gc),
			gc_node_id(gc),
			gc_index_width(gc),
			gc_screw_grp_num(gc),
			gc_tag_way_num(gc),
			gc_data_way_num(gc),
			gc_offset_width(gc),
			gc_data_ram_latency(gc),
			gc_perf_cntr_num(gc),
			gc_direct_mem_is_supported(gc) ? "true" : "false",
			gc_direct_cache_is_supported(gc) ? "true" : "false");
}

static void scr_gcache_get_info(struct scr_cache_drv *drv,
		struct cache_info *c_info)
{
	struct scr_gcache *gc = to_gcache(drv);

	unsigned int banks = gc_banks_num(gc);
	unsigned int ways = gc_data_way_num(gc);

	c_info->block_size = (1 << gc_offset_width(gc)) / 8 /* bits -> bytes */;
	c_info->sets = 1 << gc_index_width(gc);
	c_info->size = (c_info->sets * c_info->block_size * ways * banks);
}

static int scr_gcache_domain_init(struct scr_cache_drv *drv)
{
	struct scr_gcache *gc = to_gcache(drv);

	return sbi_domain_root_add_memrange(gc->base, gc->size, PAGE_SIZE,
					    SBI_DOMAIN_MEMREGION_MMIO |
					    SBI_DOMAIN_MEMREGION_M_READABLE |
					    SBI_DOMAIN_MEMREGION_M_WRITABLE);
}

static unsigned long scr_gcache_pmu_get_cntrs(struct scr_cache_drv *drv)
{
	struct scr_gcache *gc = to_gcache(drv);

	return gc_perf_cntr_num(gc);
}

static int scr_gcache_pmu_read_cntr(struct scr_cache_drv *drv,
		unsigned long cidx, uint64_t *cval)
{
	struct scr_gcache *gc = to_gcache(drv);
	const struct gc_pmu_event *e;
	int hart = scr_pmu_hartid(gc->drv.llc);
	unsigned long bank = 0u;
	uint64_t val = 0u;

	e = &gc->active_events[hart][cidx];

	for_each_set_bit_from(bank, &e->banks_mask, gc_banks_num(gc))
		val += gc_cntr_r(gc, bank, cidx, GC_PCE_CNTR);

	*cval = val;
	return SBI_OK;
}

static int scr_gcache_pmu_get_free_cntr(struct scr_cache_drv *drv, unsigned long mask)
{
	unsigned long idx = 0;
	struct scr_gcache *gc = to_gcache(drv);
	int hart = scr_pmu_hartid(gc->drv.llc);
	int ret = SBI_ENOTSUPP;

	for_each_set_bit_from(idx, &mask, gc_perf_cntr_num(gc)) {
		const struct gc_pmu_event *e = &gc->active_events[hart][idx];

		if (!gc_is_event_active(e)) {
			ret = idx;
			break;
		}
	}

	return ret;
}

static unsigned int scr_gcache_pmu_events(struct scr_gcache *gc)
{
	struct gc_version ver;

	gc_version(gc, &ver);

	return ver.release < SCR_GC_PMU_EVENTS_V2_RELEASE ?
			SCR_GC_PMU_EVENTS_V1 : SCR_GC_PMU_EVENTS_V2;
}

static int scr_gcache_pmu_assign_cntr(struct scr_cache_drv *drv, unsigned long cnt_idx,
		unsigned long event_idx, unsigned long event_data)
{
	struct scr_gcache *gc = to_gcache(drv);
	int hart = scr_pmu_hartid(gc->drv.llc);
	struct gc_pmu_event *e = &gc->active_events[hart][cnt_idx];
	unsigned int event_type = event_idx & SCR_GC_PMU_EVENT_SELECTOR_MASK;
	unsigned long banks_mask;
	int gc_banks;

	if (event_idx >= scr_gcache_pmu_events(gc) || event_idx == SCR_GC_EVENT_NOP)
		return SBI_EINVAL;

	if (gc_is_event_active(e))
		return SBI_EINVAL;

	gc_banks = gc_banks_num(gc);
	if (gc_banks > sizeof(banks_mask) * 8)
		gc_banks = sizeof(banks_mask) * 8;

	banks_mask  = scr_event_banks_mask(gc, event_data);
	banks_mask &= GENMASK(gc_banks - 1, 0);

	e->banks_mask = banks_mask;
	e->ctrl = event_type;

	return SBI_OK;
}

static int scr_gcache_pmu_free_cntr(struct scr_cache_drv *drv, unsigned long cnt_idx)
{
	struct scr_gcache *gc = to_gcache(drv);
	int hart = scr_pmu_hartid(gc->drv.llc);

	gc->active_events[hart][cnt_idx].ctrl = SCR_GC_EVENT_NOP;

	return SBI_OK;
}

static int scr_gcache_pmu_enable_cntr(struct scr_cache_drv *drv, unsigned long cnt_idx,
		bool enable, bool update, uint64_t val)
{
	struct scr_gcache *gc = to_gcache(drv);
	int hart = scr_pmu_hartid(gc->drv.llc);
	struct gc_pmu_event *e = &gc->active_events[hart][cnt_idx];

	if (!gc_is_event_active(e))
		return SBI_EINVAL;

	if (enable)
		gc_pmu_ctr_start(gc, e->banks_mask, cnt_idx, e->ctrl, val, update);
	else
		gc_pmu_ctr_stop(gc, e->banks_mask, cnt_idx, val, update);

	return SBI_OK;
}

#define GC_CACHE_CTRL_DISABLE               \
	((GC_CACHE_CTRL_CACHING_ENABLE | GC_CACHE_CTRL_COHERENCY_ENABLE))
#define GC_CLUSTER_INFO_BANK_NUM_OFF        16
#define GC_CLUSTER_INFO_BANK_NUM_OFF_MASK   0xffff

asm (".macro l2gc_wait_bank gcreg, r0, mask        \n"
	"1:                                            \n"
	"fence                                         \n"
	"ld     \\r0, \\gcreg                          \n"
	"and	\\r0, \\r0, \\mask                     \n"
	"bne	\\r0, \\mask, 1b                       \n"
	".endm                                         \n"
	);

asm (
	".macro l2gc_dis_feature gcreg, r0, mask       \n"
	"ld     \\r0, \\gcreg                          \n"
	"and    \\r0, \\r0, \\mask                     \n"
	"sd     \\r0, \\gcreg 	                       \n"
	".endm                                         \n"
);

/* Global ASM function scr_l2_gcache_disable_asm
 * Use t0, t1, t2, t3 regs
 */
__asm__ (
	".section .data                         \n"
	".align	3                               \n"
	"_scr_l2_gcache_base:                   \n"
	RISCV_PTR " 0                           \n"
	"_scr_l2_gcache_quirks:                 \n"
	RISCV_INT " 0                           \n"
	".section .text                         \n"
	".align 3                               \n"
	".global scr_l2_gcache_disable_asm      \n"
	"scr_l2_gcache_disable_asm:             \n"
	"lla  t0, _scr_l2_gcache_base           \n"
	"ld   t0, 0(t0)                         \n"
	"li   t1, " STRINGIFY(~GC_CACHE_CTRL_DISABLE) "\n"
	"l2gc_dis_feature "
		STRINGIFY(GC_CACHE_CTRL) "(t0),"
		 "t2,"
		 "t1                                 \n"
	"not  t1, t1                             \n"
/* Get banks number */
	"ld   t3, " STRINGIFY(GC_CLUSTER_INFO) "(t0) \n"
	"srli t3, t3, " STRINGIFY(GC_CLUSTER_INFO_BANK_NUM_OFF)  "\n"
	"li   t2, " STRINGIFY(GC_CLUSTER_INFO_BANK_NUM_OFF_MASK) "\n"
	"and  t3, t3, t2                         \n"
/* Check for QUIRK_ONE_BANK */
	"lla  t2, _scr_l2_gcache_quirks          \n"
	"lw   t2, 0(t2)                          \n"
	"andi t2, t2, "STRINGIFY(QUIRK_ONE_BANK)"\n"
	"beqz t2, 2f                             \n"
	"li   t2, 1                              \n"
	"beq  t2, t3, 2f                         \n"
	"addi t0, t0," STRINGIFY(GC_BANK_STEP)  "\n"
/* Iterate banks */
	"2:                                      \n"
	"l2gc_wait_bank "
		STRINGIFY(GC_CACHE_STATUS) "(t0),"
		"t2,"
		"t1                                  \n"
	"addi t0, t0," STRINGIFY(GC_BANK_STEP)  "\n"
	"li   t1, 1                              \n"
	"sub  t3, t3, t1                         \n"
	"beqz t3, 2b                             \n"
	"ret                                     \n"
);

extern unsigned long _scr_l2_gcache_base;
extern unsigned int  _scr_l2_gcache_quirks;

static struct scr_gcache gcache_l2 = {
	.quirks = QUIRK_ONE_BANK,
	.drv = {
		.get_version   = scr_gcache_get_version,
		.get_info      = scr_gcache_get_info,
		.print_info    = scr_gcache_print_info,
		.domain_init   = scr_gcache_domain_init,
		.disable       = scr_gcache_disable,

		.pmu_get_cntrs     = scr_gcache_pmu_get_cntrs,
		.pmu_get_free_cntr = scr_gcache_pmu_get_free_cntr,
		.pmu_assign_cntr   = scr_gcache_pmu_assign_cntr,
		.pmu_free_cntr     = scr_gcache_pmu_free_cntr,
		.pmu_enable_cntr   = scr_gcache_pmu_enable_cntr,
		.pmu_read_cntr     = scr_gcache_pmu_read_cntr,

		.enable       = scr_gcache_enable,
		.flush        = scr_gcache_flush,
		.inval        = scr_gcache_inval,
	},
};

static struct scr_gcache gcache_l3 = {
	.drv = {
		.get_version   = scr_gcache_get_version,
		.get_info      = scr_gcache_get_info,
		.print_info    = scr_gcache_print_info,
		.domain_init   = scr_gcache_domain_init,
		.disable       = scr_gcache_disable,

		.pmu_get_cntrs     = scr_gcache_pmu_get_cntrs,
		.pmu_get_free_cntr = scr_gcache_pmu_get_free_cntr,
		.pmu_assign_cntr   = scr_gcache_pmu_assign_cntr,
		.pmu_free_cntr     = scr_gcache_pmu_free_cntr,
		.pmu_enable_cntr   = scr_gcache_pmu_enable_cntr,
		.pmu_read_cntr     = scr_gcache_pmu_read_cntr,

		.enable       = scr_gcache_enable,
		.flush        = scr_gcache_flush,
		.inval        = scr_gcache_inval,
	},
};

struct scr_cache_drv *scr_gcache_probe(unsigned long reg_base, unsigned long size,
				       int level)
{
	struct scr_gcache *gc;
	unsigned long event_bank_mask;

	if (level == L2_CACHE_LEVEL) {
		gc = &gcache_l2;
		event_bank_mask = SCR_L2_PMU_BANKS_SEL_MASK;
	} else if (level == L3_CACHE_LEVEL) {
		gc = &gcache_l3;
		event_bank_mask = SCR_L3_PMU_BANKS_SEL_MASK;
	} else
		return NULL;

	gc->base    = reg_base;
	gc->size    = size;
	gc->level   = level;
	gc->drv.llc = level == L3_CACHE_LEVEL;

	gc->event_bank_mask = event_bank_mask;

	if (level == L2_CACHE_LEVEL) {
		_scr_l2_gcache_base   = reg_base;
		_scr_l2_gcache_quirks = gc->quirks;
	}

	return &gc->drv;
}

#else

struct scr_cache_drv *scr_gcache_probe(unsigned long base __attribute__ ((unused)),
				      unsigned long size __attribute__ ((unused)),
				      int level __attribute__ ((unused)))
{
	return NULL;
}

#endif
