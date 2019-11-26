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

/* https://131002.net/siphash/ */

#include "balagan.h"

static uint64_t rotl64(uint64_t u, int s)
{
	return (u << s) | (u >> (64 - s));
}

static uint64_t get64le(void const *data, size_t ix)
{
	uint8_t const *p = (uint8_t const *)data + ix * 8;
	uint64_t ret = 0;

	for (size_t i = 0; i < 8; ++i) {
		ret |= (uint64_t)p[i] << (i * 8);
	}

	return ret;
}

static void put64le(uint64_t v, void *out)
{
	uint8_t *p = (uint8_t *)out;

	for (size_t i = 0; i < 8; ++i) {
		p[i] = (uint8_t)(v >> (i * 8));
	}
}

static void sipround(uint64_t *v0, uint64_t *v1, uint64_t *v2, uint64_t *v3)
{
	*v0 += *v1;
	*v1 = rotl64(*v1, 13);
	*v1 ^= *v0;
	*v0 = rotl64(*v0, 32);

	*v2 += *v3;
	*v3 = rotl64(*v3, 16);
	*v3 ^= *v2;

	*v2 += *v1;
	*v1 = rotl64(*v1, 17);
	*v1 ^= *v2;
	*v2 = rotl64(*v2, 32);

	*v0 += *v3;
	*v3 = rotl64(*v3, 21);
	*v3 ^= *v0;
}

static void sipcompress2(uint64_t *v0, uint64_t *v1,
                         uint64_t *v2, uint64_t *v3, uint64_t m)
{
	*v3 ^= m;
	sipround(v0, v1, v2, v3);
	sipround(v0, v1, v2, v3);
	*v0 ^= m;
}

static uint64_t siplast(const void *dat, size_t len)
{
	size_t i, cnt, off;
	uint64_t last;
	const uint8_t *ptr;

	last = 0;
	cnt  = len % 8;
	off  = (len / 8) * 8;
	ptr  = (const uint8_t *)dat + off;
	for (i = 0; i < cnt; ++i) {
		last |= (uint64_t)(*ptr++) << (i * 8);
	}
	last |= (uint64_t)(len % 0xff) << (7 * 8);

	return last;
}

static void
siphash_24(uint8_t const *key, const void *dat, size_t len, uint8_t *out)
{
	size_t i;
	uint64_t m;
	uint64_t v0, v1, v2, v3;
	uint64_t key0, key1;

	key0 = get64le(key, 0);
	key1 = get64le(key, 1);

	v0   = key0 ^ 0x736f6d6570736575ull;
	v1   = key1 ^ 0x646f72616e646f6dull;
	v2   = key0 ^ 0x6c7967656e657261ull;
	v3   = key1 ^ 0x7465646279746573ull;

	for (i = 0; i < (len / 8); ++i) {
		m = get64le(dat, i);
		sipcompress2(&v0, &v1, &v2, &v3, m);
	}
	m = siplast(dat, len);
	sipcompress2(&v0, &v1, &v2, &v3, m);

	v2 ^= 0xff;

	for (i = 0; i < 4; ++i) {
		sipround(&v0, &v1, &v2, &v3);
	}

	put64le(v0 ^ v1 ^ v2 ^ v3, out);
}

void blgn_siphash64(uint8_t const seed[16],
                    void const *dat, size_t len, uint64_t *res)
{
	uint8_t out[8];

	siphash_24(seed, dat, len, out);
	*res = get64le(out, 0);
}

