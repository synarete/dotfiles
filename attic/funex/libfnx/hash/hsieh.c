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

/*
 * http://www.azillionmonkeys.com/qed/hash.html
 *
 * http://burtleburtle.net/bob/hash/doobs.html
 */

#include "balagan.h"

#undef get16bits
#if (defined(__GNUC__) && defined(__i386__))
#define get16bits(d) (*((const uint16_t *) (d)))
#endif

#if !defined (get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)\
                      +(uint32_t)(((const uint8_t *)(d))[0]) )
#endif

static uint32_t
hsieh_super_fast_hash(const uint8_t *data, unsigned len)
{
	uint32_t hash = len, tmp;
	int rem;

	if (!len || !data) {
		return 0;
	}

	rem = len & 3;
	len >>= 2;

	/* Main loop */
	while (len-- > 0) {
		hash  += get16bits(data);
		tmp    = (get16bits(data + 2) << 11) ^ hash;
		hash   = (hash << 16) ^ tmp;
		data  += 2 * sizeof(uint16_t);
		hash  += hash >> 11;
	}

	/* Handle end cases */
	switch (rem) {
		case 3:
			hash += get16bits(data);
			hash ^= hash << 16;
			hash ^= ((unsigned)data[sizeof(uint16_t)]) << 18;
			hash += hash >> 11;
			break;
		case 2:
			hash += get16bits(data);
			hash ^= hash << 11;
			hash += hash >> 17;
			break;
		case 1:
			hash += (unsigned)(data[0]);
			hash ^= hash << 10;
			hash += hash >> 1;
			break;
		default:
			break;
	}

	/* Force "avalanching" of final 127 bits */
	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 4;
	hash += hash >> 17;
	hash ^= hash << 25;
	hash += hash >> 6;

	return hash;
}

void blgn_hsieh32(const void *mem, size_t len, uint32_t *res)
{
	*res = hsieh_super_fast_hash((const uint8_t *)mem, (unsigned)len);
}

