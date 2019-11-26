/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                             *
 *                            Funex File-System                                *
 *                                                                             *
 *  Copyright (C) 2014,2015 Synarete                                           *
 *                                                                             *
 *  Funex is a free software; you can redistribute it and/or modify it under   *
 *  the terms of the GNU General Public License as published by the Free       *
 *  Software Foundation, either version 3 of the License, or (at your option)  *
 *  any later version.                                                         *
 *                                                                             *
 *  Funex is distributed in the hope that it will be useful, but WITHOUT ANY   *
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS  *
 *  FOR A PARTICULAR PURPOSE. See the GNU General Public License for more      *
 *  details.                                                                   *
 *                                                                             *
 *  You should have received a copy of the GNU General Public License along    *
 *  with this program. If not, see <http://www.gnu.org/licenses/>              *
 *                                                                             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
#include <fnxconfig.h>

#include "infra-compiler.h"
#include "infra-macros.h"
#include "infra-bitops.h"


void fnx_setf(unsigned int *flags, unsigned int mask)
{
	*flags |= mask;
}

void fnx_unsetf(unsigned int *flags, unsigned int mask)
{
	*flags &= ~mask;
}

int fnx_testf(unsigned int flags, unsigned int mask)
{
	return ((flags & mask) == mask);
}


void fnx_setlf(unsigned long *flags, unsigned long mask)
{
	*flags |= mask;
}

void fnx_unsetlf(unsigned long *flags, unsigned long mask)
{
	*flags &= ~mask;
}

int fnx_testlf(unsigned long flags, unsigned long mask)
{
	return ((flags & mask) == mask);
}


unsigned long fnx_rotleft(unsigned long word, unsigned int shift)
{
	return (word << shift) | (word >> (64 - shift));
}

unsigned long fnx_rotright(unsigned long word, unsigned int shift)
{
	return (word >> shift) | (word << (64 - shift));
}

/* Returns the number of 1-bits in x */
int fnx_popcount(unsigned int x)
{
	return x ? __builtin_popcount(x) : 0;
}


