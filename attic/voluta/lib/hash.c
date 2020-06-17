/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * This file is part of libvoluta
 *
 * Copyright (C) 2020 Shachar Sharon
 *
 * Libvoluta is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Libvoluta is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 */
#define _GNU_SOURCE 1
#include <stdlib.h>
#include <stdint.h>
#include "libvoluta.h"


#define MOD_ADLER   (65521)

uint32_t voluta_adler32(const void *dat, size_t len)
{
	uint32_t a;
	uint32_t b;
	const uint8_t *p = dat;

	a = 1;
	b = 0;
	for (size_t i = 0; i < len; ++i) {
		a = (a + p[i]) % MOD_ADLER;
		b = (b + a) % MOD_ADLER;
	}
	return (b << 16) | a;
}

uint64_t voluta_fnv1a(const void *buf, size_t len, uint64_t hval_base)
{
	uint64_t hval;
	const uint8_t *itr = (const uint8_t *)buf;
	const uint8_t *end = itr + len;
	const uint64_t fnv_prime = 0x100000001b3UL;

	hval = hval_base;
	while (itr < end) {
		hval *= fnv_prime;

		hval ^= (uint64_t)(*itr++);
	}
	return hval;
}

uint64_t voluta_twang_mix64(uint64_t key)
{
	key = ~key + (key << 21);
	key = key ^ (key >> 24);
	key = key + (key << 3) + (key << 8);
	key = key ^ (key >> 14);
	key = key + (key << 2) + (key << 4);
	key = key ^ (key >> 28);
	key = key + (key << 31);

	return key;
}

