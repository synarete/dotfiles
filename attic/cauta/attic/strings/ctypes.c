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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ctypes.h"


/*
 * Wrappers over standard ctypes functions (macros?).
 */
int chr_isalnum(char c)
{
	return isalnum(c);
}

int chr_isalpha(char c)
{
	return isalpha(c);
}

int chr_isascii(char c)
{
	return isascii(c);
}

int chr_isblank(char c)
{
	return isblank(c);
}

int chr_iscntrl(char c)
{
	return iscntrl(c);
}

int chr_isdigit(char c)
{
	return isdigit(c);
}

int chr_isgraph(char c)
{
	return isgraph(c);
}

int chr_islower(char c)
{
	return islower(c);
}

int chr_isprint(char c)
{
	return isprint(c);
}

int chr_ispunct(char c)
{
	return ispunct(c);
}

int chr_isspace(char c)
{
	return isspace(c);
}

int chr_isupper(char c)
{
	return isupper(c);
}

int chr_isxdigit(char c)
{
	return isxdigit(c);
}


