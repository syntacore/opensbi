/*
 * Copyright (C) 2022, Syntacore Ltd.
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

/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Atish Patra <atish.patra@wdc.com>
 */

#ifndef __SBI_HSM_H__
#define __SBI_HSM_H__

#include <sbi/sbi_types.h>
#include <sbi/riscv_atomic.h>

/** Hart state managment device */
struct sbi_hsm_device {
	/** Name of the hart state managment device */
	char name[32];

	/** Start (or power-up) the given hart */
	int (*hart_start)(u32 hartid, ulong saddr);

	/**
	 * Stop (or power-down) the current hart from running. This call
	 * doesn't expect to return if success.
	 */
	int (*hart_stop)(void);

	/**
	 * Put the current hart in platform specific suspend (or low-power)
	 * state.
	 *
	 * For successful retentive suspend, the call will return 0 when
	 * the hart resumes normal execution.
	 *
	 * For successful non-retentive suspend, the hart will resume from
	 * the warm boot entry point.
	 */
	int (*hart_suspend)(u32 suspend_type);

	/**
	 * Perform platform-specific actions to resume from a suspended state.
	 *
	 * This includes restoring any platform state that was lost during
	 * non-retentive suspend.
	 */
	void (*hart_resume)(void);
};

struct sbi_domain;
struct sbi_scratch;

const struct sbi_hsm_device *sbi_hsm_get_device(void);

void sbi_hsm_set_device(const struct sbi_hsm_device *dev);

int sbi_hsm_init(struct sbi_scratch *scratch, u32 hartid, bool cold_boot);
void __noreturn sbi_hsm_exit(struct sbi_scratch *scratch);

int sbi_hsm_hart_start(struct sbi_scratch *scratch,
		       const struct sbi_domain *dom,
		       u32 hartid, ulong saddr, ulong smode, ulong priv);
int sbi_hsm_hart_stop(struct sbi_scratch *scratch, bool exitnow);
void sbi_hsm_hart_resume_start(struct sbi_scratch *scratch);
void sbi_hsm_hart_resume_finish(struct sbi_scratch *scratch);
int sbi_hsm_hart_suspend(struct sbi_scratch *scratch, u32 suspend_type,
			 ulong raddr, ulong rmode, ulong priv);
int sbi_hsm_hart_get_state(const struct sbi_domain *dom, u32 hartid);
int sbi_hsm_hart_interruptible_mask(const struct sbi_domain *dom,
				    ulong hbase, ulong *out_hmask);
void sbi_hsm_prepare_next_jump(struct sbi_scratch *scratch, u32 hartid);

atomic_t *sbi_hsm_get_state_ptr(u32 hartid);

#endif
