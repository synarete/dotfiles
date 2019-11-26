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
#ifndef CAC_BITS_H_
#define CAC_BITS_H_


void bits_setf16(uint16_t *flags, uint16_t mask);

void bits_unsetf16(uint16_t *flags, uint16_t mask);

int bits_testf16(uint16_t flags, uint16_t mask);


void bits_setf32(uint32_t *flags, uint32_t mask);

void bits_unsetf32(uint32_t *flags, uint32_t mask);

int bits_testf32(uint32_t flags, uint32_t mask);


void bits_setf64(uint64_t *flags, uint64_t mask);

void bits_unsetf64(uint64_t *flags, uint64_t mask);

int bits_testf64(uint64_t flags, uint64_t mask);


uint64_t bits_rotleft64(uint64_t word, unsigned shift);

uint64_t bits_rotright64(uint64_t word, unsigned shift);


unsigned bits_popcount16(uint16_t x);

unsigned bits_popcount32(uint32_t x);

#endif /* CAC_BITS_H_ */




