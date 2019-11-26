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
#include <stdlib.h>
#include <ctype.h>

#include <fnxinfra.h>
#include "version.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const char
s_funex_license[] =
    "The Funex file-system is a free software: you can redistribute it    \n" \
    "and/or modify it under the terms of the GNU General Public License   \n" \
    "as published by the Free Software Foundation, either version 3 of    \n" \
    "the License, or (at your option) any later version.                  \n" \
    "                                                                     \n" \
    "Funex is distributed in the hope that it will be useful, but WITHOUT \n" \
    "ANY WARRANTY; without even the implied warranty of MERCHANTABILITY   \n" \
    "or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public      \n" \
    "License for more details.                                            \n" \
    "                                                                     \n" \
    "You should have received a copy of the GNU General Public License    \n" \
    "along with this program. If not, see <http://www.gnu.org/licenses/>  \n" \
    ;

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const char *funex_license(void)
{
	return s_funex_license;
}

const char *funex_version(void)
{
	const char *str;
#if defined(VERSION) && defined(RELEASE) && defined(REVISION)
	str = VERSION "-" RELEASE "." REVISION;
#elif defined(FUNEX_VERSION)
	str = FNX_MAKESTR(FUNEX_VERSION);
#else
	str = "0.0.0";
#endif
	return str;
}

const char *funex_buildtime(void)
{
#if defined(BUILDDATE)
	return BUILDDATE;
#else
	return __DATE__ " " __TIME__;
#endif
}


