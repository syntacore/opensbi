/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef __RISCV_ENDIAN_H__
#define __RISCV_ENDIAN_H__

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
# define cpu_to_le16(x)		(x)
# define cpu_to_le32(x)		(x)
# define cpu_to_le64(x)		(x)
# define le16_to_cpu(x)		(x)
# define le32_to_cpu(x)		(x)
# define le64_to_cpu(x)		(x)
# define cpu_to_be16(x)		__builtin_bswap16(x)
# define cpu_to_be32(x)		__builtin_bswap32(x)
# define cpu_to_be64(x)		__builtin_bswap64(x)
# define be16_to_cpu(x)		__builtin_bswap16(x)
# define be32_to_cpu(x)		__builtin_bswap32(x)
# define be64_to_cpu(x)		__builtin_bswap64(x)
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
# define cpu_to_le16(x)		__builtin_bswap16(x)
# define cpu_to_le32(x)		__builtin_bswap32(x)
# define cpu_to_le64(x)		__builtin_bswap64(x)
# define le16_to_cpu(x)		__builtin_bswap16(x)
# define le32_to_cpu(x)		__builtin_bswap32(x)
# define le64_to_cpu(x)		__builtin_bswap64(x)
# define cpu_to_be16(x)		(x)
# define cpu_to_be32(x)		(x)
# define cpu_to_be64(x)		(x)
# define be16_to_cpu(x)		(x)
# define be32_to_cpu(x)		(x)
# define be64_to_cpu(x)		(x)
#else
#error "Unexpected endianness"
#endif

#if __riscv_xlen == 64
#define cpu_to_lle cpu_to_le64
#define lle_to_cpu le64_to_cpu
#elif __riscv_xlen == 32
#define cpu_to_lle cpu_to_le32
#define lle_to_cpu le32_to_cpu
#else
#error "Unexpected __riscv_xlen"
#endif

#endif
