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
#include "hashfn.h"

#define MOD_ADLER   (65521)

static uint32_t adler32(const void *dat, size_t len)
{
	uint32_t a, b, i;
	const uint8_t *p;

	a = 1;
	b = 0;
	p = (const uint8_t *)dat;
	for (i = 0; i < len; ++i) {
		a = (a + p[i]) % MOD_ADLER;
		b = (b + a) % MOD_ADLER;
	}

	return (b << 16) | a;
}

void cac_hash_adler32(const void *dat, size_t len, uint32_t *res)
{
	*res = adler32(dat, len);
}
