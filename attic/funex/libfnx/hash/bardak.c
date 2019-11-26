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

void blgn_bardak(const void *dat, size_t len, uint64_t *hh)
{
	uint32_t h1, h2;

	blgn_crc32c(dat, len, &h1);
	blgn_fnv32a(dat, len, &h2);
	*hh = rehash(dissolve(h1, h2));
}


