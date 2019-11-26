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

/*
 * http://cubehash.cr.yp.to/
 *
 * https://code.google.com/p/shacrypt/
 */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "hashfn.h"

#define CUBEHASH_ROUNDS 8
#define CUBEHASH_BLOCKBYTES 1


struct cubehash_state {
	int hashbitlen, pos;
	uint32_t x[32];
};
typedef struct cubehash_state cubehash_state_t;


static uint32_t rotate(uint32_t v, unsigned n)
{
	return ((v << n) | (v >> (32 - n)));
}

static void cubehash_transform(cubehash_state_t *state)
{
	int i, r;
	uint32_t y[16];

	for (r = 0; r < CUBEHASH_ROUNDS; ++r) {
		for (i = 0; i < 16; ++i) {
			state->x[i + 16] += state->x[i];
		}
		for (i = 0; i < 16; ++i) {
			y[i ^ 8] = state->x[i];
		}
		for (i = 0; i < 16; ++i) {
			state->x[i] = rotate(y[i], 7);
		}
		for (i = 0; i < 16; ++i) {
			state->x[i] ^= state->x[i + 16];
		}
		for (i = 0; i < 16; ++i) {
			y[i ^ 2] = state->x[i + 16];
		}
		for (i = 0; i < 16; ++i) {
			state->x[i + 16] = y[i];
		}
		for (i = 0; i < 16; ++i) {
			state->x[i + 16] += state->x[i];
		}
		for (i = 0; i < 16; ++i) {
			y[i ^ 4] = state->x[i];
		}
		for (i = 0; i < 16; ++i) {
			state->x[i] = rotate(y[i], 11);
		}
		for (i = 0; i < 16; ++i) {
			state->x[i] ^= state->x[i + 16];
		}
		for (i = 0; i < 16; ++i) {
			y[i ^ 1] = state->x[i + 16];
		}
		for (i = 0; i < 16; ++i) {
			state->x[i + 16] = y[i];
		}
	}
}

static void cubehash_init(cubehash_state_t *state, int hashbitlen)
{
	int i;

	state->hashbitlen = hashbitlen;
	for (i = 0; i < 32; ++i) {
		state->x[i] = 0;
	}
	state->x[0] = (uint32_t)(hashbitlen / 8);
	state->x[1] = CUBEHASH_BLOCKBYTES;
	state->x[2] = CUBEHASH_ROUNDS;
	for (i = 0; i < 10; ++i) {
		cubehash_transform(state);
	}
	state->pos = 0;
}

static int
cubehash_update(cubehash_state_t *state, const uint8_t *data, size_t databitlen)
{
	/* caller promises us that previous data had integral number of bytes */
	/* so state->pos is a multiple of 8 */
	while (databitlen >= 8) {
		uint32_t u = *data;
		u <<= 8 * ((state->pos / 8) % 4);
		state->x[state->pos / 32] ^= u;
		data += 1;
		databitlen -= 8;
		state->pos += 8;
		if (state->pos == 8 * CUBEHASH_BLOCKBYTES) {
			cubehash_transform(state);
			state->pos = 0;
		}
	}
	if (databitlen > 0) {
		uint32_t u = *data;
		u <<= 8 * ((state->pos / 8) % 4);
		state->x[state->pos / 32] ^= u;
		state->pos += (int)databitlen;
	}
	return 0;
}

static void cubehash_final(cubehash_state_t *state, uint8_t *hashval)
{
	int i;
	uint32_t u;

	u = (128u >> (state->pos % 8));
	u <<= 8 * ((state->pos / 8) % 4);
	state->x[state->pos / 32] ^= u;
	cubehash_transform(state);
	state->x[31] ^= 1;
	for (i = 0; i < 10; ++i) {
		cubehash_transform(state);
	}
	for (i = 0; i < state->hashbitlen / 8; ++i) {
		hashval[i] = (uint8_t)(state->x[i / 4] >> (8 * (i % 4)));
	}
}

static void cubehash_hash(int hashbitlen, const uint8_t *data,
                          size_t databitlen, uint8_t hashval[64])
{
	cubehash_state_t state;

	cubehash_init(&state, hashbitlen);
	cubehash_update(&state, data, databitlen);
	cubehash_final(&state, hashval);
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

void cac_hash_cubehash512(const void *in, size_t len, uint64_t res[8])
{
	uint8_t hval[64];

	cubehash_hash(512, (const uint8_t *)in, 8 * len, hval);
	for (size_t i = 0; i < 8; ++i) {
		res[i] = octets_to_uint64(&hval[8 * i]);
	}
}



