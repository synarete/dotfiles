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
#include <fnxconfig.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#include "infra-atomic.h"


/*
 * Atomic operations wrappers:
 */
/*
 * Using GCC's atomic extention (Atomicity wrapped with functions):
 * http://gcc.gnu.org/onlinedocs/gcc-4.5.0/gcc/Atomic-Builtins.html
 */
size_t fnx_atomic_size_read(volatile const size_t *p)
{
	return __sync_or_and_fetch((volatile size_t *)p, 0);
}

size_t fnx_atomic_size_test_and_set(volatile size_t *p, size_t n)
{
	return __sync_lock_test_and_set(p, n);
}

size_t fnx_atomic_size_add_and_fetch(volatile size_t *p, size_t n)
{
	return __sync_add_and_fetch(p, n);
}

size_t fnx_atomic_size_sub_and_fetch(volatile size_t *p, size_t n)
{
	return __sync_sub_and_fetch(p, n);
}

void fnx_atomic_size_add(volatile size_t *p, size_t n)
{
	fnx_atomic_size_add_and_fetch(p, n);
}

void fnx_atomic_size_inc(volatile size_t *p)
{
	fnx_atomic_size_add(p, 1);
}



/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int32_t fnx_atomic_i32_read(volatile const int32_t *p)
{
	return __sync_or_and_fetch((volatile int32_t *)p, 0);
}

int32_t fnx_atomic_i32_test_and_set(volatile int32_t *p, int32_t i)
{
	return __sync_lock_test_and_set(p, i);
}

int32_t fnx_atomic_i32_add_and_fetch(volatile int32_t *p, int32_t i)
{
	return __sync_add_and_fetch(p, i);
}

int32_t fnx_atomic_i32_sub_and_fetch(volatile int32_t *p, int32_t i)
{
	return __sync_sub_and_fetch(p, i);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

uint32_t fnx_atomic_u32_read(volatile const uint32_t *p)
{
	return __sync_or_and_fetch((volatile uint32_t *)p, 0);
}

uint32_t fnx_atomic_u32_test_and_set(volatile uint32_t *p, uint32_t u)
{
	return __sync_lock_test_and_set(p, u);
}

uint32_t fnx_atomic_u32_add_and_fetch(volatile uint32_t *p, uint32_t u)
{
	return __sync_add_and_fetch(p, u);
}

uint32_t fnx_atomic_u32_sub_and_fetch(volatile uint32_t *p, uint32_t u)
{
	return __sync_sub_and_fetch(p, u);
}


uint32_t fnx_atomic_get(volatile const uint32_t *p)
{
	return fnx_atomic_u32_read(p);
}

void fnx_atomic_set(volatile uint32_t *p, uint32_t u)
{
	fnx_atomic_u32_test_and_set(p, u);
}

void fnx_atomic_add(volatile uint32_t *p, uint32_t u)
{
	fnx_atomic_u32_add_and_fetch(p, u);
}

void fnx_atomic_sub(volatile uint32_t *p, uint32_t u)
{
	fnx_atomic_u32_sub_and_fetch(p, u);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

#if defined(FNX_HAVE_ATOMIC64)

int64_t fnx_atomic_i64_read(volatile const int64_t *p)
{
	return __sync_or_and_fetch((volatile int64_t *)p, 0ULL);
}

int64_t fnx_atomic_i64_test_and_set(volatile int64_t *p, int64_t i)
{
	return __sync_lock_test_and_set(p, i);
}

int64_t fnx_atomic_i64_add_and_fetch(volatile int64_t *p, int64_t i)
{
	return __sync_add_and_fetch(p, i);
}

int64_t fnx_atomic_i64_sub_and_fetch(volatile int64_t *p, int64_t i)
{
	return __sync_sub_and_fetch(p, i);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

uint64_t fnx_atomic_u64_read(volatile const uint64_t *p)
{
	return __sync_or_and_fetch((volatile uint64_t *)p, 0ULL);
}

uint64_t fnx_atomic_u64_test_and_set(volatile uint64_t *p, uint64_t u)
{
	return __sync_lock_test_and_set(p, u);
}

uint64_t fnx_atomic_u64_add_and_fetch(volatile uint64_t *p, uint64_t u)
{
	return __sync_add_and_fetch(p, u);
}

uint64_t fnx_atomic_u64_sub_and_fetch(volatile uint64_t *p, uint64_t u)
{
	return __sync_sub_and_fetch(p, u);
}


uint64_t fnx_atomic_get64(volatile const uint64_t *p)
{
	return fnx_atomic_u64_read(p);
}

void fnx_atomic_set64(volatile uint64_t *p, uint64_t u)
{
	fnx_atomic_u64_test_and_set(p, u);
}

void fnx_atomic_add64(volatile uint64_t *p, uint64_t u)
{
	fnx_atomic_u64_add_and_fetch(p, u);
}

void fnx_atomic_sub64(volatile uint64_t *p, uint64_t u)
{
	fnx_atomic_u64_sub_and_fetch(p, u);
}

#endif /* FNX_HAVE_ATOMIC64 */



