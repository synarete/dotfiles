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

/* http://www.isthe.com/chongo/tech/comp/fnv/ */

#include "balagan.h"

#define FNV_32_INIT     ((uint32_t)0x811c9dc5)
/*#define FNV_32_PRIME    ((uint32_t)0x01000193)*/


static uint32_t
fnv_32a(const uint8_t *bp, size_t len, uint32_t hval)
{
	const uint8_t *be;

	be = bp + len;
	while (bp < be) {
		hval ^= (uint32_t) * bp++;
		hval += ((hval << 1) + (hval << 4) +
		         (hval << 7) + (hval << 8) + (hval << 24));
	}

	return hval;
}

void blgn_fnv32a(const void *mem, size_t len, uint32_t *res)
{
	const uint32_t hval = FNV_32_INIT;
	*res = fnv_32a((const uint8_t *)mem, len, hval);
}

