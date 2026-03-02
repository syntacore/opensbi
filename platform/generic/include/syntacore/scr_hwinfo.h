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

#ifndef _SCR_HWINFO_H_
#define _SCR_HWINFO_H_

#define SCR_HWINFO_OK               0
#define SCR_HWINFO_EMETHOD_DISABLED -1
#define SCR_HWINFO_ENO_VALUE        -2
#define SCR_HWINFO_ENO_METHODS      -3
#define SCR_HWINFO_EBAD_ARG         -4


int scr_hwinfo_init(void *fdt);
int scr_hwinfo_get_build_id(unsigned long *build_id);
int scr_hwinfo_get_sys_clk(unsigned long *sys_clk);
int scr_hwinfo_get_mtimer_clk(unsigned long *mtimer_clk);
int scr_hwinfo_get_uart_clk(unsigned long *uart_clk);
int scr_hwinfo_get_harts_count(unsigned long *harts_count);
int scr_hwinfo_get_hart_clk(unsigned long hartid, unsigned long *hart_clk);

#endif /* _SCR_HWINFO_H_ */
