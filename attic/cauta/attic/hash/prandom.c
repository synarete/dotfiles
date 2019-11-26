/*
 * This file is part of CaC
 *
 * Copyright (C) 2016,2017 Shachar Sharon
 *
 * CaC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CaC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CaC.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include "hashfn.h"

#define NDAT (8)


static uint64_t dissolve(uint32_t x, uint32_t y)
{
	const uint64_t x1 = (uint64_t)(x & 0x0000FFFF);
	const uint64_t y1 = (uint64_t)(y & 0x0000FFFF) << 16;
	const uint64_t x2 = (uint64_t)(x & 0xFFFF0000) << 32;
	const uint64_t y2 = (uint64_t)(y & 0xFFFF0000) << 48;

	return (x1 | y1 | x2 | y2);
}

/* Numerical Recipes' rehash */
static uint64_t rehash(uint64_t u)
{
	uint64_t v = u * 3935559000370003845ULL + 2691343689449507681ULL;

	v ^= v >> 21;
	v ^= v << 37;
	v ^= v >>  4;
	v *= 4768777513237032717ULL;
	v ^= v << 20;
	v ^= v >> 41;
	v ^= v <<  5;

	return v;
}

static uint64_t hash(const void *dat, size_t len)
{
	uint32_t h1, h2;

	cac_hash_crc32c(dat, len, &h1);
	cac_hash_fnv32a(dat, len, &h2);
	return rehash(dissolve(h1, h2));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

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
		for (i = 0; i < (n - 1); i++) {
			fill_data(dat, seed ^ i);
			rnd = hash(dat, sizeof(dat));

			r = (size_t)rnd;
			j = i + (r / (ULONG_MAX / (n - i) + 1));
			swap(arr + i, arr + j);
		}
	}
}

/* Helper function for creation of sequence on integers [base..base+n) */
void cac_hash_create_seq(int *arr, size_t n, size_t base)
{
	for (size_t i = 0; i < n; ++i) {
		arr[i] = (int)(base++);
	}
}

/* Generates sequence of integers [base..base+n) and then random shuffle */
void cac_hash_prandom_seq(int *arr, size_t len, size_t base, unsigned long seed)
{
	cac_hash_create_seq(arr, len, base);
	prandom_shuffle(arr, len, seed);
}

void cac_hash_prandom_tseq(int *arr, size_t len, size_t base)
{
	cac_hash_prandom_seq(arr, len, base, (unsigned long)time(NULL));
}

/* Fills buffer with pseudo-random values */
void cac_hash_prandom_buf(void *buf, size_t bsz, unsigned long seed)
{
	unsigned itr = 0;
	uint64_t res[2], dat[NDAT];
	size_t rem, cnt;
	char *ptr = (char *)buf;
	char *end = (char *)buf + bsz;
	const size_t rsz = sizeof(res);

	fill_data(dat, seed);
	while (ptr < end) {
		cac_hash_murmur3(dat, sizeof(dat), itr++, res);
		rem = (size_t)(end - ptr);
		cnt = (rem < rsz) ? rem : rsz;
		memcpy(ptr, res, cnt);
		ptr += cnt;
	}
}
