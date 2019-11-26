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

static void test_string_join(void)
{
	int cmp;
	string_t *str, *hello, *world;

	hello = string_new("hello ");
	world = string_new("world!");
	str = string_join(hello, world);
	cmp = substr_compare(str, "hello world!");
	cac_asserteq(cmp, 0);

	str = string_joinss(str, " ", str);
	cmp = substr_compare(str, "hello world! hello world!");
	cac_asserteq(cmp, 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_string_split(void)
{
	int cmp;
	string_t *str;
	strpair_t *sp;
	const char *dec = "0123456789";
	const char *hex = "ABCDEF";

	str = string_joinss(string_new(dec), "--", string_new(hex));
	sp = string_split(str, 10, 2);

	cmp = substr_compare(sp->first, dec);
	cac_asserteq(cmp, 0);

	cmp = substr_compare(sp->second, hex);
	cac_asserteq(cmp, 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_string_fmt(void)
{
	int cmp;
	string_t *str;

	str = string_fmt("%u", 123456789);
	cmp = substr_compare(str, "123456789");
	cac_asserteq(cmp, 0);

	str = string_fmt("%s", str->str);
	cmp = substr_compare(str, "123456789");
	cac_asserteq(cmp, 0);

	str = string_fmt("0x%X", 0xABCDEF);
	cmp = substr_compare(str, "0xABCDEF");
	cac_asserteq(cmp, 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void cac_test_strings(void)
{
	test_string_join();
	test_string_split();
	test_string_fmt();
}


