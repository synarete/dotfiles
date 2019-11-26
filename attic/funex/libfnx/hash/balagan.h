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
#ifndef BALAGAN_H_
#define BALAGAN_H_

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* Non-cryptographic hash functions */
void blgn_adler32(const void *, size_t, uint32_t *);

void blgn_crc32c(const void *, size_t, uint32_t *);

void blgn_fnv32a(const void *, size_t, uint32_t *);

void blgn_hsieh32(const void *, size_t, uint32_t *);

void blgn_siphash64(uint8_t const [16], void const *, size_t, uint64_t *);

void blgn_murmur3(const void *, size_t, uint32_t, uint64_t [2]);

void blgn_bardak(const void *, size_t, uint64_t *);


/* Cryptographic hash functions */
void blgn_blake128(const void *, size_t, uint64_t [2]);

void blgn_cubehash512(const void *, size_t, uint64_t [8]);


/* Re-shuffle hash functions */
uint64_t blgn_wang(uint64_t);


/* Pseudo-random generators */
void blgn_create_seq(int *, size_t, size_t);

void blgn_prandom_seq(int *, size_t, size_t, unsigned long);

void blgn_prandom_tseq(int *, size_t, size_t);

void blgn_prandom_buf(void *, size_t, unsigned long);

#endif /* BALAGAN_H_ */

