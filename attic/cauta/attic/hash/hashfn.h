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

#ifndef CAC_HASHFN_H_
#define CAC_HASHFN_H_

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* Non-cryptographic hash functions */
void cac_hash_adler32(const void *, size_t, uint32_t *);

void cac_hash_crc32c(const void *, size_t, uint32_t *);

void cac_hash_fnv32a(const void *, size_t, uint32_t *);

void cac_hash_hsieh32(const void *, size_t, uint32_t *);

void cac_hash_siphash64(uint8_t const [16], void const *, size_t, uint64_t *);

void cac_hash_murmur3(const void *, size_t, uint32_t, uint64_t [2]);


/* Cryptographic hash functions */
void cac_hash_blake128(const void *, size_t, uint64_t [2]);

void cac_hash_cubehash512(const void *, size_t, uint64_t [8]);


/* Re-shuffle hash functions */
uint64_t cac_hash_wang(uint64_t);


/* Pseudo-random generators */
void cac_hash_create_seq(int *, size_t, size_t);

void cac_hash_prandom_seq(int *, size_t, size_t, unsigned long);

void cac_hash_prandom_tseq(int *, size_t, size_t);

void cac_hash_prandom_buf(void *, size_t, unsigned long);

#endif /* CAC_HASHFN_H_ */

