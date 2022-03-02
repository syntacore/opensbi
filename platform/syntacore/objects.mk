#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2022 Syntacore
#

# Compiler flags
platform-cppflags-y =
platform-cflags-y =
#platform-cflags-y += -g3 -ggdb
platform-asflags-y =
platform-ldflags-y =

platform-objs-y += platform.o scr_mtimer.o scr_mpu.o scr_cache.o scr_iccm.o scr_plic.o scr_l2_pmu.o
ifeq ($(CONFIG_PLATFORM_SYNTACORE_BUNDLED_FDT), y)
platform-objs-y += dts/$(subst ",,$(CONFIG_PLATFORM_SYNTACORE_BUNDLED_FDT_NAME)).o
endif

ifeq ($(PLATFORM_RISCV_XLEN), 32)
PLATFORM_RISCV_ABI = ilp32
PLATFORM_RISCV_ISA = rv32imafdc_zicsr_zifencei
PLATFORM_RISCV_CODE_MODEL = medany
else
PLATFORM_RISCV_ABI = lp64
PLATFORM_RISCV_ISA = rv64imafdc_zicsr_zifencei
PLATFORM_RISCV_CODE_MODEL = medany
endif

FW_TEXT_START=$(CONFIG_PLATFORM_SYNTACORE_TEXT_START)

FW_JUMP=y
FW_JUMP_ADDR=$(CONFIG_PLATFORM_SYNTACORE_JUMP_ADDR)
# fdt relocation address
FW_JUMP_FDT_ADDR=$(CONFIG_PLATFORM_SYNTACORE_JUMP_FDT_ADDR)

FW_PAYLOAD=y
ifeq ($(PLATFORM_RISCV_XLEN), 32)
FW_PAYLOAD_ALIGN=0x40000
else
FW_PAYLOAD_ALIGN=0x20000
endif

# TODO: add support for dynamic mode in future
#       for instance: spl->opensbi(fw_dynamic)->u-boot boot flow
FW_DYNAMIC=n

#
# QEMU fast invocation:
#  make PLATFORM=syntacore PLATFORM_DEFCONFIG=vcu118_scr7 run
#
ifeq ($(CONFIG_PLATFORM_SYNTACORE_SCR7), y)
qemu_machine = syntacore_scr7
else
qemu_machine = syntacore_scr5
endif

platform-runcmd = qemu-system-riscv$(PLATFORM_RISCV_XLEN) \
		  -M $(qemu_machine) -smp 4 -nographic \
		  -kernel $(build_dir)/platform/syntacore/firmware/fw_payload.elf
