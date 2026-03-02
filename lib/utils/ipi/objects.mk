#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Western Digital Corporation or its affiliates.
#
# Authors:
#   Anup Patel <anup.patel@wdc.com>
#

libsbiutils-objs-$(CONFIG_IPI_MSWI) += ipi/aclint_mswi.o
libsbiutils-objs-$(CONFIG_IPI_PLICSW) += ipi/andes_plicsw.o
libsbiutils-objs-$(CONFIG_IPI_ICCM_MMIO) += ipi/scr_iccm_mmio.o
libsbiutils-objs-$(CONFIG_IPI_ICCM) += ipi/scr_iccm.o

libsbiutils-objs-$(CONFIG_FDT_IPI) += ipi/fdt_ipi.o
libsbiutils-objs-$(CONFIG_FDT_IPI) += ipi/fdt_ipi_drivers.carray.o

carray-fdt_ipi_drivers-$(CONFIG_FDT_IPI_MSWI) += fdt_ipi_mswi
libsbiutils-objs-$(CONFIG_FDT_IPI_MSWI) += ipi/fdt_ipi_mswi.o

carray-fdt_ipi_drivers-$(CONFIG_FDT_IPI_PLICSW) += fdt_ipi_plicsw
libsbiutils-objs-$(CONFIG_FDT_IPI_PLICSW) += ipi/fdt_ipi_plicsw.o

carray-fdt_ipi_drivers-$(CONFIG_FDT_IPI_ICCM_MMIO) += fdt_ipi_iccm_mmio
libsbiutils-objs-$(CONFIG_FDT_IPI_ICCM_MMIO) += ipi/fdt_ipi_iccm_mmio.o

carray-fdt_ipi_drivers-$(CONFIG_FDT_IPI_ICCM) += fdt_ipi_iccm
libsbiutils-objs-$(CONFIG_FDT_IPI_ICCM) += ipi/fdt_ipi_iccm.o
