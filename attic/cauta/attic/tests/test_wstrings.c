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
#include <cacconfig.h>
#include <cac/infra.h>
#include <cac/strings.h>
#include "tests.h"


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wstring_join(void)
{
	int cmp;
	wstring_t *str, *hello, *world;

	hello = wstring_new(L"hello ");
	world = wstring_new(L"world!");
	str = wstring_join(hello, world);
	cmp = wsubstr_compare(str, L"hello world!");
	cac_asserteq(cmp, 0);

	str = wstring_joinss(str, L" ", str);
	cmp = wsubstr_compare(str, L"hello world! hello world!");
	cac_asserteq(cmp, 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wstring_split(void)
{
	int cmp;
	wstring_t *str;
	wstrpair_t *sp;
	const wchar_t *dec = L"0123456789";
	const wchar_t *hex = L"ABCDEF";

	str = wstring_joinss(wstring_new(dec), L"--", wstring_new(hex));
	sp = wstring_split(str, 10, 2);

	cmp = wsubstr_compare(sp->first, dec);
	cac_asserteq(cmp, 0);

	cmp = wsubstr_compare(sp->second, hex);
	cac_asserteq(cmp, 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void cac_test_wstrings(void)
{
	test_wstring_join();
	test_wstring_split();
}


