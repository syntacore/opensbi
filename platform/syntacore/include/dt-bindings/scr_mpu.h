/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Syntacore
 *
 * derivate: fsbl/src/mmu.h
 * Authors:
 *   Mikhail Nefedov <mikhail.nefedov@syntacore.com>
 */
#ifndef _DT_BINDINGS_MPU_SYNTACORE_SCR_H
#define _DT_BINDINGS_MPU_SYNTACORE_SCR_H

#define SCR_MPU_CTRL_VALID		(1UL << 0)
#define SCR_MPU_MMODE_READ		(1UL << 1)
#define SCR_MPU_MMODE_WRITE		(1UL << 2)
#define SCR_MPU_MMODE_EXECUTE		(1UL << 3)
#define SCR_MPU_UMODE_READ		(1UL << 4)
#define SCR_MPU_UMODE_WRITE		(1UL << 5)
#define SCR_MPU_UMODE_EXECUTE		(1UL << 6)
#define SCR_MPU_SMODE_READ		(1UL << 7)
#define SCR_MPU_SMODE_WRITE		(1UL << 8)
#define SCR_MPU_SMODE_EXECUTE		(1UL << 9)
#define SCR_MPU_CTRL_LOCK		(1UL << 31)

#define SCR_MPU_CACHE_WEAK_ORDER	(0UL << 16)
#define SCR_MPU_NOCACHE_STRONG_ORDER	(1UL << 16)
#define SCR_MPU_NOCACHE_WEAK_ORDER	(2UL << 16)
#define SCR_MPU_MMIO			(3UL << 16)

#define SCR_MPU_READ(MODE)	SCR_MPU_## MODE ##MODE_READ
#define SCR_MPU_WRITE(MODE)	SCR_MPU_## MODE ##MODE_WRITE
#define SCR_MPU_EXECUTE(MODE)	SCR_MPU_## MODE ##MODE_EXECUTE

#define SCR_MPU_RW(MODE) \
	(SCR_MPU_READ(MODE) | SCR_MPU_WRITE(MODE))

#define SCR_MPU_MMODE_RW SCR_MPU_RW(M)
#define SCR_MPU_SMODE_RW SCR_MPU_RW(S)
#define SCR_MPU_UMODE_RW SCR_MPU_RW(U)

#define SCR_MPU_ALL(M) \
	(SCR_MPU_READ(M) | SCR_MPU_WRITE(M) | SCR_MPU_EXECUTE(M))

#define SCR_MPU_MMODE_ALL SCR_MPU_ALL(M)
#define SCR_MPU_SMODE_ALL SCR_MPU_ALL(S)
#define SCR_MPU_UMODE_ALL SCR_MPU_ALL(U)

#define SCR_MPU_MODE_ALL \
	(SCR_MPU_MMODE_ALL | SCR_MPU_SMODE_ALL | SCR_MPU_UMODE_ALL)

#define SCR_MPU_READ_ALL \
	(SCR_MPU_READ(M) | SCR_MPU_READ(S) | SCR_MPU_READ(U))

#define SCR_MPU_WRITE_ALL \
	(SCR_MPU_WRITE(M) | SCR_MPU_WRITE(S) | SCR_MPU_WRITE(U))

#define SCR_MPU_EXECUTE_ALL \
	(SCR_MPU_EXECUTE(M) | SCR_MPU_EXECUTE(S) | SCR_MPU_EXECUTE(U))

/**
 * This flag is used to distinguish OpenSBI PMP
 * regions from MPU created regions.
 */
#define SCR_MPU_DEFINED_FLAGS	(1UL << 30)
#define SCR_MPU_OPENSBI_SKIP	(1UL << 29)

/**
 * Region helper macros
 */
#define SCR_MPU_MCFG_REGION	(SCR_MPU_READ(M) | SCR_MPU_WRITE(M) | SCR_MPU_MMIO)

#endif // _DT_BINDINGS_MPU_SYNTACORE_SCR_H
