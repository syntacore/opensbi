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

#ifndef _SCR_CACHE_DRV_H_
#define _SCR_CACHE_DRV_H_

#include <sbi/sbi_bitops.h>
#include <sbi/sbi_types.h>

#define SCR_PMU_DEDICATED_FLAG	0x2

#define L2_CACHE_LEVEL 			2
#define L3_CACHE_LEVEL 			3

/* This masks are required for pcache
 * but use them for compatibility with Linux
 */
#define SCR_L2_PMU_BANKS_LOW_BIT		16
#define SCR_L2_PMU_BANKS_HIGH_BIT		19
#define SCR_L2_PMU_BANKS_SEL_MASK		\
	GENMASK(SCR_L2_PMU_BANKS_HIGH_BIT, SCR_L2_PMU_BANKS_LOW_BIT)

#define SCR_L3_PMU_BANKS_LOW_BIT		8
#define SCR_L3_PMU_BANKS_HIGH_BIT		15
#define SCR_L3_PMU_BANKS_SEL_MASK		\
	GENMASK(SCR_L3_PMU_BANKS_HIGH_BIT, SCR_L3_PMU_BANKS_LOW_BIT)

struct cache_info {
	unsigned int block_size;
	unsigned int sets;
	unsigned int size;
};

enum scr_cache_ops_type {
	XSYNTACORE_CACHE_OPS = 0,
	CBO_CACHE_OPS,
};

/* Cache */
struct cache_ops {
	size_t cbom_block_size;
	enum scr_cache_ops_type type;
	void (*flush_block)(uintptr_t vaddr);
	void (*inv_block)(uintptr_t vaddr);
};

/*
 * Cache description
 */
struct scr_cache_drv {
	bool llc;

	unsigned long (*get_version)(struct scr_cache_drv *drv);
	void (*get_info)(struct scr_cache_drv *drv, struct cache_info *info);
	void (*print_info)(struct scr_cache_drv *drv);
	int (*domain_init)(struct scr_cache_drv *drv);

	unsigned long (*pmu_get_cntrs)(struct scr_cache_drv *drv);
	int (*pmu_get_free_cntr)(struct scr_cache_drv *drv, unsigned long mask);
	int (*pmu_assign_cntr)(struct scr_cache_drv *drv,
			unsigned long cnt_idx, unsigned long event_idx, unsigned long event_data);
	int (*pmu_free_cntr)(struct scr_cache_drv *drv, unsigned long cnt_idx);
	int (*pmu_enable_cntr)(struct scr_cache_drv *drv,
			unsigned long cnt_idx, bool enable, bool update, uint64_t val);
	int (*pmu_read_cntr)(struct scr_cache_drv *drv, unsigned long cnt_idx, uint64_t *cval);

	/* Currently disable is require for
	 * HSM support as miniboot is not called on stop
	 */
	void (*disable)(struct scr_cache_drv *drv, uint32_t flags);

	/*
	 * Deprecated. This functionality will
	 * be moved to miniboot
	 */
	void (*enable)(struct scr_cache_drv *drv, u32 flags);
	void (*flush)(struct scr_cache_drv *drv);
	void (*inval)(struct scr_cache_drv *drv);
};

typedef struct scr_cache_drv *(*scr_cache_probe_t)(unsigned long reg_base, unsigned long size,
						   int level);

struct scr_cache_drv *scr_l2_pcache_probe(unsigned long reg_base, unsigned long size,
					  int level);
struct scr_cache_drv *scr_l3_pcache_probe(unsigned long reg_base, unsigned long size,
					  int level);
struct scr_cache_drv *scr_gcache_probe(unsigned long reg_base, unsigned long size,
				       int level);

#endif /* _SCR_CACHE_DRV_H_ */
