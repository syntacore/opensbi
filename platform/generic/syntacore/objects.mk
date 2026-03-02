#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (C) 2024, Syntacore Ltd.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     1. Redistributions of source code must retain the above copyright notice,
#        this list of conditions and the following disclaimer.
#     2. Redistributions in binary form must reproduce the above copyright
#        notice, this list of conditions and the following disclaimer in the
#        documentation and/or other materials provided with the distribution.
#     3. Neither the name of the copyright holder nor the names of its
#        contributors may be used to endorse or promote products derived from
#        this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

platform-objs-$(CONFIG_PLATFORM_SYNTACORE) += syntacore/scr_cache.o
platform-objs-$(CONFIG_PLATFORM_SYNTACORE) += syntacore/scr_mpu.o
platform-objs-$(CONFIG_PLATFORM_SYNTACORE) += syntacore/scr_platform_info.o
platform-objs-$(CONFIG_PLATFORM_SYNTACORE) += syntacore/scr_swpw.o
platform-objs-$(CONFIG_PLATFORM_SYNTACORE) += syntacore/scr_fdt_helper.o
platform-objs-$(CONFIG_PLATFORM_SYNTACORE) += syntacore/scr_hwinfo.o
platform-objs-$(CONFIG_PLATFORM_SYNTACORE) += syntacore/scr_pcache.o
platform-objs-$(CONFIG_PLATFORM_SYNTACORE) += syntacore/scr_gcache.o

carray-platform_override_modules-$(CONFIG_PLATFORM_SYNTACORE) += syntacore_generic
platform-objs-$(CONFIG_PLATFORM_SYNTACORE) += syntacore/syntacore_generic.o
