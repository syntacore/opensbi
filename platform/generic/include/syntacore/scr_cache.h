/*
 * Copyright (C) 2024, Syntacore Ltd.
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

#ifndef _SCR_CACHE_H_
#define _SCR_CACHE_H_

#include <syntacore/scr_generic.h>
#include <syntacore/scr_cache_drv.h>

void scr_l1c_info(void);
void scr_l1c_enable(void);
void scr_l1c_disable(void);

unsigned long scr_l2c_get_version(void);
void scr_l2c_info(void);
int  scr_l2c_domain_init(void);
void scr_l2c_set_llc(bool llc);
void scr_l2c_enable(u32 flags);
void scr_l2c_disable(u32 flags);
int  scr_l2c_probe(const void *fdt);

static inline void scr_l12c_enable(u32 flags)
{
	scr_l1c_disable();
	scr_l2c_enable(flags);
	scr_l1c_enable();
}

unsigned long scr_l3c_get_version(void);
void scr_l3c_info(void);
int  scr_l3c_domain_init(void);
void scr_l3c_flush(void);
void scr_l3c_inval(void);
int  scr_l3c_probe(const void *fdt);

void scr_cache_ops_init(void);
void scr_cache_flush(void *vaddr, unsigned long size);

void scr9_flush_suspend(unsigned long stack_addr, unsigned long stack_size);
void scr_cache_core_suspend(void);

struct caches_info {
	struct cache_info l1d[SCR_CPU_MAX_HARTS];
	struct cache_info l1i[SCR_CPU_MAX_HARTS];
	struct cache_info l2 [SCR_CPU_MAX_HARTS];
	struct cache_info l3;
};

/* L1 and L2 cache info: per hart, L3 - for all harts */
void scr_get_cache_l1d_info(struct cache_info *l1d_info);
void scr_get_cache_l1i_info(struct cache_info *l1i_info);
void scr_get_cache_l2_info (struct cache_info *l2_info);
void scr_get_cache_l3_info (struct cache_info *l3_info);

#endif /* _SCR_CACHE_H_ */
