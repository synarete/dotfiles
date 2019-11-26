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
#include <string.h>
#include <pthread.h>

#include <fnxinfra.h>


#define NTHREAD 39
#define NITER   10000

struct atomic_counters {
	size_t   cnt_sz;
	int32_t  cnt_i32;
	uint32_t cnt_u32;
	int64_t  cnt_i64;
	uint64_t cnt_u64;
};
typedef struct atomic_counters  atomic_counters_t;

static void *start_atomic_ops(void *p)
{
	size_t i;
	atomic_counters_t *cnt;

	cnt = (atomic_counters_t *)p;

	for (i = 0; i < NITER; ++i) {
		fnx_atomic_size_read(&cnt->cnt_sz);
		fnx_atomic_size_add_and_fetch(&cnt->cnt_sz, 1);
		fnx_atomic_size_add_and_fetch(&cnt->cnt_sz, 1);
		fnx_atomic_size_sub_and_fetch(&cnt->cnt_sz, 1);

		fnx_atomic_i32_read(&cnt->cnt_i32);
		fnx_atomic_i32_sub_and_fetch(&cnt->cnt_i32, 3);
		fnx_atomic_i32_add_and_fetch(&cnt->cnt_i32, 1);
		fnx_atomic_i32_add_and_fetch(&cnt->cnt_i32, 3);

		fnx_atomic_get(&cnt->cnt_u32);
		fnx_atomic_add(&cnt->cnt_u32, 2);
		fnx_atomic_sub(&cnt->cnt_u32, 1);

#if defined(FNX_HAVE_ATOMIC64)
		fnx_atomic_i64_read(&cnt->cnt_i64);
		fnx_atomic_i64_sub_and_fetch(&cnt->cnt_i64, 7);
		fnx_atomic_i64_add_and_fetch(&cnt->cnt_i64, 1);
		fnx_atomic_i64_add_and_fetch(&cnt->cnt_i64, 7);

		fnx_atomic_get64(&cnt->cnt_u64);
		fnx_atomic_add64(&cnt->cnt_u64, 5);
		fnx_atomic_sub64(&cnt->cnt_u64, 4);
#endif
	}

	return NULL;
}


static void fnx_test_atomic(void)
{
	int rc;
	size_t i, nth, eval;
	pthread_t th_arr[NTHREAD];
	pthread_t *th = NULL;
	atomic_counters_t cnt;


	fnx_atomic_size_test_and_set(&cnt.cnt_sz, 0);
	fnx_atomic_i32_test_and_set(&cnt.cnt_i32, 0);
	fnx_atomic_u32_test_and_set(&cnt.cnt_u32, 0);
#if defined(FNX_HAVE_ATOMIC64)
	fnx_atomic_i64_test_and_set(&cnt.cnt_i64, 0);
	fnx_atomic_u64_test_and_set(&cnt.cnt_u64, 0);
#endif

	nth = FNX_NELEMS(th_arr);
	for (i = 0; i < nth; ++i) {
		th = &th_arr[i];
		rc = pthread_create(th, NULL, start_atomic_ops, &cnt);
		fnx_assert(rc == 0);
	}

	for (i = 0; i < nth; ++i) {
		th = &th_arr[i];
		rc = pthread_join(*th, NULL);
		fnx_assert(rc == 0);
	}

	eval = nth * NITER;
	fnx_assert(cnt.cnt_sz  == (size_t)eval);
	fnx_assert(cnt.cnt_i32 == (int32_t)eval);
	fnx_assert(cnt.cnt_u32 == (uint32_t)eval);
#if defined(FNX_HAVE_ATOMIC64)
	fnx_assert(cnt.cnt_i64 == (int64_t)eval);
	fnx_assert(cnt.cnt_u64 == (uint64_t)eval);
#endif
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int main(void)
{
	fnx_test_atomic();
	return EXIT_SUCCESS;
}



