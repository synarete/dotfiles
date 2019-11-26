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
#ifndef FNX_INFRA_BITOPS_H_
#define FNX_INFRA_BITOPS_H_


void fnx_setf(unsigned int *, unsigned int);

void fnx_unsetf(unsigned int *, unsigned int);

int fnx_testf(unsigned int, unsigned int);


void fnx_setlf(unsigned long *, unsigned long);

void fnx_unsetlf(unsigned long *, unsigned long);

int fnx_testlf(unsigned long, unsigned long);


unsigned long fnx_rotleft(unsigned long, unsigned int shift);

unsigned long fnx_rotright(unsigned long, unsigned int shift);


int fnx_popcount(unsigned int);


#endif /* FNX_INFRA_BITOPS_H_ */




