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

#include "balagan.h"

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

void blgn_adler32(const void *dat, size_t len, uint32_t *res)
{
	*res = adler32(dat, len);
}
