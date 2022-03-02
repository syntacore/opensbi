/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Syntacore
 *
 */
#ifndef _SCR_PLATFORM_H_
#define _SCR_PLATFORM_H_

#define SYNTACORE_PLATFORM_MAJOR_VER	0x0
#define SYNTACORE_PLATFORM_MINOR_VER	0x1

#ifdef CONFIG_PLATFORM_SYNTACORE_BUNDLED_FDT
extern const char dt_start[];
static inline void *platform_bundled_fdt(void)
{
	return (void *)dt_start;
}
#else
static inline void *platform_bundled_fdt(void)
{
	return NULL;
}
#endif

extern void *platform_fdt;

static inline void *platform_get_fdt()
{
	return platform_fdt;
}

static inline void platform_set_fdt(void *fdt)
{
	platform_fdt = fdt;
}

#endif /* _SCR_PLATFORM_H_ */
