/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                             *
 *                 Balagan library -- Hash functions collection                *
 *                                                                             *
 *  Copyright (C) 2014 Synarete                                                *
 *                                                                             *
 *  Balagan is a free software; you can redistribute it and/or modify it under *
 *  the terms of the GNU Lesser General Public License as published by the     *
 *  Free Software Foundation; either version 3 of the License, or (at your     *
 *  option) any later version.                                                 *
 *                                                                             *
 *  Balagan is distributed in the hope that it will be useful, but WITHOUT ANY *
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS  *
 *  FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for    *
 *  more details.                                                              *
 *                                                                             *
 *  You should have received a copy of GNU Lesser General Public License       *
 *  along with the library. If not, see <http://www.gnu.org/licenses/>         *
 *                                                                             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <errno.h>
#include <error.h>
#include <err.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <fnxhash.h>

#define NELEMS(x) (sizeof(x) / sizeof(x[0]))


struct hkey {
	long key[4];
};
typedef struct hkey hkey_t;

typedef uint64_t hash_t;
typedef hash_t (*hash_fn)(const struct hkey *);

struct hctx {
	const char *name;
	hash_fn hook;
	size_t  nbins;
	size_t  nelem;
	long   *tbl;
	double  grade;
};
typedef struct hctx hctx_t;


static size_t s_nbins = 104729UL;
static size_t s_nitrs = 127UL;
static struct random_data s_rndd;
static char s_statebuf[2048];
static unsigned int s_seed = 0;

static long getrand(void)
{
	int32_t res1, res2;

	if (!s_seed) {
		s_seed = (unsigned)time(NULL);
		initstate_r(s_seed, s_statebuf, sizeof(s_statebuf), &s_rndd);
	}
	random_r(&s_rndd, &res1);
	random_r(&s_rndd, &res2);

	return ((long)res1 << 32) | (long)res2;
}

static void mkpattern(hkey_t *hkey, long key, long idx)
{
	hkey->key[0] = key;
	hkey->key[1] = 0x11111111;
	hkey->key[2] = idx;
	hkey->key[3] = 3;
}

static void *xnew(size_t sz)
{
	void *ptr;

	ptr = malloc(sz);
	if (ptr == NULL) {
		errx(EXIT_FAILURE, "no-malloc: sz=%lu", sz);
	}
	memset(ptr, 0, sz);
	return ptr;
}

static void hctx_setup(hctx_t *hctx, size_t nbins)
{
	hctx->tbl   = xnew(nbins * sizeof(long));
	hctx->nbins = nbins;
	hctx->nelem = 0;
	hctx->grade = 0.0f;
}

static void hctx_destroy(hctx_t *hctx)
{
	free(hctx->tbl);
	hctx->tbl   = NULL;
	hctx->nbins = 0;
	hctx->nelem = 0;
	hctx->grade = 0.0f;
}

static void hctx_apply(hctx_t *hctx, const hkey_t *hkey)
{
	hash_t hval;
	size_t entry;

	hval = hctx->hook(hkey);
	entry = hval % hctx->nbins;
	hctx->tbl[entry] += 1;
	hctx->nelem += 1;
}

static void hctx_grade(hctx_t *hctx)
{
	double binval, avarage, grade, diff;

	grade   = 0.0f;
	avarage = (double)hctx->nelem / (double)hctx->nbins;
	for (size_t i = 0; i < hctx->nbins; ++i) {
		binval = (double)hctx->tbl[i];
		diff = (binval - avarage);
		if (diff > 1.0f) {
			grade += (diff * diff);
		}
	}
	hctx->grade = grade / (double)hctx->nelem;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static hash_t hsieh32(const hkey_t *hkey)
{
	uint32_t res = 0;

	blgn_hsieh32(hkey->key, sizeof(hkey->key), &res);
	return res;
}

static hash_t crc32c(const hkey_t *hkey)
{
	uint32_t res = 0;

	blgn_crc32c(hkey->key, sizeof(hkey->key), &res);
	return res;
}

static hash_t fnv32a(const hkey_t *hkey)
{
	uint32_t res = 0;

	blgn_fnv32a(hkey->key, sizeof(hkey->key), &res);
	return res;
}

static hash_t bardak(const hkey_t *hkey)
{
	uint64_t res = 0;

	blgn_bardak(hkey->key, sizeof(hkey->key), &res);
	return res;
}

static hash_t murmur3(const hkey_t *hkey)
{
	uint64_t res[2] = { 0, 0 };

	blgn_murmur3(hkey->key, sizeof(hkey->key), 0, res);
	return (res[0] ^ res[1]);
}

static hash_t siphash(const hkey_t *hkey)
{
	uint8_t seed[16];
	uint64_t hval = 0;

	memset(seed, 0, sizeof(seed));
	blgn_siphash64(seed, hkey->key, sizeof(hkey->key), &hval);
	return hval;
}

static hash_t blake128(const hkey_t *hkey)
{
	uint64_t res[2] = { 0, 0 };

	blgn_blake128(hkey->key, sizeof(hkey->key), res);
	return (res[0] ^ res[1]);
}

static hash_t cubehash(const hkey_t *hkey)
{
	uint64_t res[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	blgn_cubehash512(hkey->key, sizeof(hkey->key), res);
	return (res[0] ^ res[1] ^ res[2] ^ res[3] ^
	        res[4] ^ res[5] ^ res[6] ^ res[7]);
}


#define HCTX(fn) { #fn, fn, 0, 0, NULL, 0.0f }

static hctx_t s_hctx_arr[] = {
	HCTX(hsieh32),
	HCTX(crc32c),
	HCTX(fnv32a),
	HCTX(bardak),
	HCTX(murmur3),
	HCTX(siphash),
	HCTX(blake128),
	HCTX(cubehash)
};

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_hquality(void)
{
	long key;
	hctx_t *hctx;
	hkey_t hkey;
	size_t i, j, k;
	const size_t nhctx = NELEMS(s_hctx_arr);

	for (i = 0; i < nhctx; ++i) {
		hctx = &s_hctx_arr[i];
		hctx_setup(hctx, s_nbins);
	}

	key = getrand();
	for (k = 0; k < s_nitrs; ++k) {
		for (j = 0; j < s_nbins; ++j) {
			mkpattern(&hkey, key + (long)k, (long)j);
			for (i = 0; i < nhctx; ++i) {
				hctx = &s_hctx_arr[i];
				hctx_apply(hctx, &hkey);
			}
		}
	}

	printf("bins:itrs %lu:%lu\n", s_nbins, s_nitrs);
	for (i = 0; i < nhctx; ++i) {
		hctx = &s_hctx_arr[i];
		hctx_grade(hctx);
		printf("  %s %lf\n", hctx->name, hctx->grade);
	}
	printf("\n");

	for (i = 0; i < nhctx; ++i) {
		hctx = &s_hctx_arr[i];
		hctx_destroy(hctx);
	}
}

int main(int argc, char *argv[])
{
	double dd;

	for (int i = 1; i < argc; ++i) {
		const int num = atoi(argv[i]);
		if (num > 7) {
			dd = sqrt((double)num);
			s_nbins = (size_t)num;
			s_nitrs = (size_t)dd;
			test_hquality();
		}
	}
	return 0;
}
