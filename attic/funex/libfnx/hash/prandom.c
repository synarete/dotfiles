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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include "balagan.h"

#define NDAT (8)

/* Helpers */
static void swap(int *a, int *b)
{
	const int k = *a;
	*a = *b;
	*b = k;
}

/* Fill buffer with initial values, based on seed */
static void fill_data(uint64_t dat[NDAT], unsigned long seed)
{
	dat[0] = 0x6A09E667UL + seed;
	dat[1] = 0xBB67AE85UL - seed;
	dat[2] = 0x3C6EF372UL ^ seed;
	dat[3] = 0xA54FF53AUL * seed;
	dat[4] = 0x510E527FUL + ~seed;
	dat[5] = 0x9B05688CUL - ~seed;
	dat[6] = 0x1F83D9ABUL ^ ~seed;
	dat[7] = 0x5BE0CD19UL * ~seed;
}

/*
 * Pseudo-random shuffle, using hash function.
 * See: http://benpfaff.org/writings/clc/shuffle.html
 */
static void prandom_shuffle(int *arr, size_t n, unsigned long seed)
{
	size_t i, j, r;
	uint64_t rnd, dat[NDAT];

	if (n > 1) {
		for (i = 0; i < n - 1; i++) {
			fill_data(dat, seed ^ i);
			blgn_bardak(dat, sizeof(dat), &rnd);

			r = (size_t)rnd;
			j = i + (r / (ULONG_MAX / (n - i) + 1));
			swap(arr + i, arr + j);
		}
	}
}

/* Helper function for creation of sequence on integers [base..base+n) */
void blgn_create_seq(int *arr, size_t n, size_t base)
{
	for (size_t i = 0; i < n; ++i) {
		arr[i] = (int)(base++);
	}
}

/* Generates sequence of integers [base..base+n) and then random shuffle */
void blgn_prandom_seq(int *arr, size_t len, size_t base, unsigned long seed)
{
	blgn_create_seq(arr, len, base);
	prandom_shuffle(arr, len, seed);
}

void blgn_prandom_tseq(int *arr, size_t len, size_t base)
{
	blgn_prandom_seq(arr, len, base, (unsigned long)time(NULL));
}

/* Fills buffer with pseudo-random values */
void blgn_prandom_buf(void *buf, size_t bsz, unsigned long seed)
{
	unsigned itr = 0;
	uint64_t res[2], dat[NDAT];
	size_t rem, cnt;
	char *ptr = (char *)buf;
	char *end = (char *)buf + bsz;
	const size_t rsz = sizeof(res);

	fill_data(dat, seed);
	while (ptr < end) {
		blgn_murmur3(dat, sizeof(dat), itr++, res);
		rem = (size_t)(end - ptr);
		cnt = (rem < rsz) ? rem : rsz;
		memcpy(ptr, res, cnt);
		ptr += cnt;
	}
}
