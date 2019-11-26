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
#define _GNU_SOURCE 1
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#include "wctypes.h"

#define wint(c) ((wint_t)c)


/*
 * Wrappers over standard ctypes functions (macros?).
 */
int wchr_isalnum(wchar_t c)
{
	return iswalnum(wint(c));
}

int wchr_isalpha(wchar_t c)
{
	return iswalpha(wint(c));
}

int wchr_isblank(wchar_t c)
{
	return iswblank(wint(c));
}

int wchr_iscntrl(wchar_t c)
{
	return iswcntrl(wint(c));
}

int wchr_isdigit(wchar_t c)
{
	return iswdigit(wint(c));
}

int wchr_isgraph(wchar_t c)
{
	return iswgraph(wint(c));
}

int wchr_islower(wchar_t c)
{
	return iswlower(wint(c));
}

int wchr_isprint(wchar_t c)
{
	return iswprint(wint(c));
}

int wchr_ispunct(wchar_t c)
{
	return iswpunct(wint(c));
}

int wchr_isspace(wchar_t c)
{
	return iswspace(wint(c));
}

int wchr_isupper(wchar_t c)
{
	return iswupper(wint(c));
}

int wchr_isxdigit(wchar_t c)
{
	return iswxdigit(wint(c));
}


