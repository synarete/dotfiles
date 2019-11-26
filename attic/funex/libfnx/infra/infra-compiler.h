/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                             *
 *                            Funex File-System                                *
 *                                                                             *
 *  Copyright (C) 2014,2015 Synarete                                           *
 *                                                                             *
 *  Funex is a free software; you can redistribute it and/or modify it under   *
 *  the terms of the GNU General Public License as published by the Free       *
 *  Software Foundation, either version 3 of the License, or (at your option)  *
 *  any later version.                                                         *
 *                                                                             *
 *  Funex is distributed in the hope that it will be useful, but WITHOUT ANY   *
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS  *
 *  FOR A PARTICULAR PURPOSE. See the GNU General Public License for more      *
 *  details.                                                                   *
 *                                                                             *
 *  You should have received a copy of the GNU General Public License along    *
 *  with this program. If not, see <http://www.gnu.org/licenses/>              *
 *                                                                             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
#ifndef FNX_INFRA_COMPILER_H_
#define FNX_INFRA_COMPILER_H_


/* Just find out if using GCC */
#if defined(__GNUC__) || defined(__clang__)
#define FNX_GCC
#define FNX_CLANG
#elif defined(__INTEL_COMPILER)
#define FNX_ICC
#error "ICC -- Untested"
#else
#error "Unknown compiler"
#endif


#if defined(FNX_GCC)
#define FNX_GCC_VERSION \
	((__GNUC__ * 10000) + (__GNUC_MINOR__ * 100) + __GNUC_PATCHLEVEL__)
#endif

#if defined(FNX_GCC) || defined(FNX_CLANG)

#define FNX_FUNCTION                __func__ /* __FUNCTION__ */
#define FNX_WORDSIZE                __WORDSIZE

#if defined(__OPTIMIZE__)
#define FNX_OPTIMIZE                __OPTIMIZE__
#else
#define FNX_OPTIMIZE                0
#endif

#define FNX_ATTR_ALIGNED            __attribute__ ((__aligned__))
#define FNX_ATTR_UNUSED             __attribute__ ((__unused__))
#define FNX_ATTR_PACKED             __attribute__ ((__packed__))
#define FNX_ATTR_PURE               __attribute__ ((__pure__))
#define FNX_ATTR_NONNULL            __attribute__ ((__nonnull__))
#define FNX_ATTR_NONNULL1           __attribute__ ((__nonnull__ (1)))
#define FNX_ATTR_NORETURN           __attribute__ ((__noreturn__))
#define FNX_ATTR_FMTPRINTF(i, j)    __attribute__ ((format (printf, i, j)))


#define fnx_likely(x)               __builtin_expect(!!(x), 1)
#define fnx_unlikely(x)             __builtin_expect(!!(x), 0)

#define fnx_packed                  FNX_ATTR_PACKED
#define fnx_aligned                 FNX_ATTR_ALIGNED
#define fnx_noreturn                FNX_ATTR_NORETURN

#define fnx_aligned64               __attribute__((__aligned__(64)))

#endif

#endif /* FNX_INFRA_COMPILER_H_ */




