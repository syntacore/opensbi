/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Syntacore
 *
 * Authors:
 *   Sergey Matyukevich <sergey.matyukevich@syntacore.com>
 *
 */

#ifndef __RISCV_DBTR_H__
#define __RISCV_DBTR_H__

#define RV_MAX_TRIGGERS	32

enum {
	RISCV_DBTR_TRIG_NONE = 0,
	RISCV_DBTR_TRIG_LEGACY,
	RISCV_DBTR_TRIG_MCONTROL,
	RISCV_DBTR_TRIG_ICOUNT,
	RISCV_DBTR_TRIG_ITRIGGER,
	RISCV_DBTR_TRIG_ETRIGGER,
	RISCV_DBTR_TRIG_MCONTROL6,
};

union riscv_dbtr_tdata1 {
	unsigned long value;
	struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		unsigned long type:4;
		unsigned long dmode:1;
#if __riscv_xlen == 64
		unsigned long data:59;
#elif __riscv_xlen == 32
		unsigned long data:27;
#else
#error "Unexpected __riscv_xlen"
#endif
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#if __riscv_xlen == 64
		unsigned long data:59;
#elif __riscv_xlen == 32
		unsigned long data:27;
#else
#error "Unexpected __riscv_xlen"
#endif
		unsigned long dmode:1;
		unsigned long type:4;
#else
#error "Unexpected endianness"
#endif
	};
};

union riscv_dbtr_tdata1_mcontrol {
	unsigned long value;
	struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		unsigned long type:4;
		unsigned long dmode:1;
		unsigned long maskmax:6;
#if __riscv_xlen >= 64
		unsigned long _res1:30;
		unsigned long sizehi:2;
#endif
		unsigned long hit:1;
		unsigned long select:1;
		unsigned long timing:1;
		unsigned long sizelo:2;
		unsigned long action:4;
		unsigned long chain:1;
		unsigned long match:4;
		unsigned long m:1;
		unsigned long _res2:1;
		unsigned long s:1;
		unsigned long u:1;
		unsigned long execute:1;
		unsigned long store:1;
		unsigned long load:1;
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		unsigned long load:1;
		unsigned long store:1;
		unsigned long execute:1;
		unsigned long u:1;
		unsigned long s:1;
		unsigned long _res2:1;
		unsigned long m:1;
		unsigned long match:4;
		unsigned long chain:1;
		unsigned long action:4;
		unsigned long sizelo:2;
		unsigned long timing:1;
		unsigned long select:1;
		unsigned long hit:1;
#if __riscv_xlen >= 64
		unsigned long sizehi:2;
		unsigned long _res1:30;
#endif
		unsigned long maskmax:6;
		unsigned long dmode:1;
		unsigned long type:4;
#else
#error "Unexpected endianness"
#endif
	};
};

#define MCONTROL_U_OFFSET	3
#define MCONTROL_S_OFFSET	4
#define MCONTROL_M_OFFSET	6

union riscv_dbtr_tdata1_mcontrol6 {
	unsigned long value;
	struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		unsigned long type:4;
		unsigned long dmode:1;
#if __riscv_xlen == 64
		unsigned long _res1:34;
#elif __riscv_xlen == 32
		unsigned long _res1:2;
#else
#error "Unexpected __riscv_xlen"
#endif
		unsigned long vs:1;
		unsigned long vu:1;
		unsigned long hit:1;
		unsigned long select:1;
		unsigned long timing:1;
		unsigned long size:4;
		unsigned long action:4;
		unsigned long chain:1;
		unsigned long match:4;
		unsigned long m:1;
		unsigned long _res2:1;
		unsigned long s:1;
		unsigned long u:1;
		unsigned long execute:1;
		unsigned long store:1;
		unsigned long load:1;
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		unsigned long load:1;
		unsigned long store:1;
		unsigned long execute:1;
		unsigned long u:1;
		unsigned long s:1;
		unsigned long _res2:1;
		unsigned long m:1;
		unsigned long match:4;
		unsigned long chain:1;
		unsigned long action:4;
		unsigned long size:4;
		unsigned long timing:1;
		unsigned long select:1;
		unsigned long hit:1;
		unsigned long vu:1;
		unsigned long vs:1;
#if __riscv_xlen == 64
		unsigned long _res1:34;
#elif __riscv_xlen == 32
		unsigned long _res1:2;
#else
#error "Unexpected __riscv_xlen"
#endif
		unsigned long dmode:1;
		unsigned long type:4;
#else
#error "Unexpected endianness"
#endif
	};
};

#define MCONTROL6_U_OFFSET	3
#define MCONTROL6_S_OFFSET	4
#define MCONTROL6_M_OFFSET	6
#define MCONTROL6_VU_OFFSET	23
#define MCONTROL6_VS_OFFSET	24

#endif /* __RISCV_DBTR_H__ */
