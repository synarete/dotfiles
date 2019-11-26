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

#include <cacdefs.h>
#include "hashfn.h"

#if defined(CAC_GCC_VERSION) && (CAC_GCC_VERSION >= 70100)
#define FALLTHROUGH __attribute__ ((fallthrough))
#else
#define FALLTHROUGH
#endif


/*
 * https://code.google.com/p/smhasher
 */

#define MURMUR3_C1 (0x87c37b91114253d5ULL)
#define MURMUR3_C2 (0x4cf5ad432745937fULL)


static uint64_t rotl64(uint64_t x, int8_t r)
{
	return (x << r) | (x >> (64 - r));
}

/*
 * Block read - if your platform needs to do endian-swapping or can only handle
 * aligned reads, do the conversion here
 */
#define getblock(p, i) (p[i])


/* Finalization mix - force all bits of a hash block to avalanche */
static uint64_t fmix64(uint64_t k)
{
	k ^= k >> 33;
	k *= 0xff51afd7ed558ccdULL;
	k ^= k >> 33;
	k *= 0xc4ceb9fe1a85ec53ULL;
	k ^= k >> 33;

	return k;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
murmur3_body(const uint64_t *blocks, size_t nblocks, uint64_t *v1, uint64_t *v2)
{
	uint64_t h1, h2, k1, k2;
	const uint64_t c1 = MURMUR3_C1;
	const uint64_t c2 = MURMUR3_C2;

	h1 = *v1;
	h2 = *v2;
	for (size_t i = 0; i < nblocks; i++) {
		k1 = getblock(blocks, i * 2 + 0);
		k2 = getblock(blocks, i * 2 + 1);

		k1 *= c1;
		k1  = rotl64(k1, 31);
		k1 *= c2;
		h1 ^= k1;

		h1 = rotl64(h1, 27);
		h1 += h2;
		h1 = h1 * 5 + 0x52dce729;

		k2 *= c2;
		k2  = rotl64(k2, 33);
		k2 *= c1;
		h2 ^= k2;

		h2 = rotl64(h2, 31);
		h2 += h1;
		h2 = h2 * 5 + 0x38495ab5;
	}
	*v1 = h1;
	*v2 = h2;
}

static void
murmur3_tail(const uint8_t *tail, size_t len, uint64_t *v1, uint64_t *v2)
{
	uint64_t h1, h2, k1, k2;
	const uint64_t c1 = MURMUR3_C1;
	const uint64_t c2 = MURMUR3_C2;

	h1 = *v1;
	h2 = *v2;
	k1 = 0;
	k2 = 0;

	switch (len & 15) {
		case 15:
			k2 ^= (uint64_t)(tail[14]) << 48;
			FALLTHROUGH;
		case 14:
			k2 ^= (uint64_t)(tail[13]) << 40;
			FALLTHROUGH;
		case 13:
			k2 ^= (uint64_t)(tail[12]) << 32;
			FALLTHROUGH;
		case 12:
			k2 ^= (uint64_t)(tail[11]) << 24;
			FALLTHROUGH;
		case 11:
			k2 ^= (uint64_t)(tail[10]) << 16;
			FALLTHROUGH;
		case 10:
			k2 ^= (uint64_t)(tail[ 9]) << 8;
			FALLTHROUGH;
		case  9:
			k2 ^= (uint64_t)(tail[ 8]) << 0;
			k2 *= c2;
			k2  = rotl64(k2, 33);
			k2 *= c1;
			h2 ^= k2;
			FALLTHROUGH;
		case  8:
			k1 ^= (uint64_t)(tail[ 7]) << 56;
			FALLTHROUGH;
		case  7:
			k1 ^= (uint64_t)(tail[ 6]) << 48;
			FALLTHROUGH;
		case  6:
			k1 ^= (uint64_t)(tail[ 5]) << 40;
			FALLTHROUGH;
		case  5:
			k1 ^= (uint64_t)(tail[ 4]) << 32;
			FALLTHROUGH;
		case  4:
			k1 ^= (uint64_t)(tail[ 3]) << 24;
			FALLTHROUGH;
		case  3:
			k1 ^= (uint64_t)(tail[ 2]) << 16;
			FALLTHROUGH;
		case  2:
			k1 ^= (uint64_t)(tail[ 1]) << 8;
			FALLTHROUGH;
		case  1:
			k1 ^= (uint64_t)(tail[ 0]) << 0;
			k1 *= c1;
			k1  = rotl64(k1, 31);
			k1 *= c2;
			h1 ^= k1;
			FALLTHROUGH;
		default:
			break;
	}
	*v1 = h1;
	*v2 = h2;
}

static void
murmur3_finalize(size_t len, uint64_t *v1, uint64_t *v2)
{
	uint64_t h1, h2;

	h1 = *v1;
	h2 = *v2;

	h1 ^= (uint64_t)len;
	h2 ^= (uint64_t)len;

	h1 += h2;
	h2 += h1;

	h1 = fmix64(h1);
	h2 = fmix64(h2);

	h1 += h2;
	h2 += h1;

	*v1 = h1;
	*v2 = h2;
}

static void
murmurhash3_x64_128(const void *key, size_t len, uint32_t seed, void *out)
{
	uint64_t h1, h2;
	size_t nblocks;
	const uint8_t  *data;
	const uint8_t  *tail;
	const uint64_t *blocks;

	nblocks = len / 16;
	data    = (const uint8_t *)key;
	blocks  = (const uint64_t *)key;
	tail    = (const uint8_t *)(data + (nblocks * 16));

	h1 = h2 = seed;
	murmur3_body(blocks, (size_t)nblocks, &h1, &h2);
	murmur3_tail(tail, (size_t)len, &h1, &h2);
	murmur3_finalize((size_t)len, &h1, &h2);

	((uint64_t *)out)[0] = h1;
	((uint64_t *)out)[1] = h2;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static uint64_t octets_to_uint64(const uint8_t dat[8])
{
	uint64_t n = 0;

	n |= (uint64_t)(dat[0]) << 56;
	n |= (uint64_t)(dat[1]) << 48;
	n |= (uint64_t)(dat[2]) << 40;
	n |= (uint64_t)(dat[3]) << 32;
	n |= (uint64_t)(dat[4]) << 24;
	n |= (uint64_t)(dat[5]) << 16;
	n |= (uint64_t)(dat[6]) << 8;
	n |= (uint64_t)(dat[7]);
	return n;
}

void cac_hash_murmur3(const void *dat, size_t len, uint32_t seed,
                      uint64_t res[2])
{
	uint8_t out[16];

	memset(out, 0, sizeof(out));
	murmurhash3_x64_128(dat, len, seed, out);
	res[0] = octets_to_uint64(out);
	res[1] = octets_to_uint64(out + 8);
}

