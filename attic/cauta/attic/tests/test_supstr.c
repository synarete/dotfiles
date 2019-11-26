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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <cac/infra.h>
#include <cac/strings.h>
#include "tests.h"


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_supstr_assign(supstr_t *ss)
{
	int eq;
	size_t n;
	char buf[] = "0123456789......";
	const char *s;
	const size_t bsz = cac_nelems(buf) - 1;

	supstr_ninit(ss, buf, 10, bsz);
	cac_asserteq(ss->bsz, bsz);

	s = "ABC";
	supstr_assign(ss, s);
	n = supstr_size(ss);
	cac_asserteq(n, 3);
	n = supstr_bufsize(ss);
	cac_asserteq(n, bsz);
	eq = substr_isequal(&ss->sub, s);
	cac_assert(eq);
	eq = substr_isequal(&ss->sub, ss->sub.str);
	cac_assert(eq);

	s = "ZZZZ";
	supstr_assign(ss, s);
	n = supstr_size(ss);
	cac_asserteq(n, 4);
	n = supstr_bufsize(ss);
	cac_asserteq(n, bsz);
	eq = substr_isequal(&ss->sub, s);
	cac_assert(eq);

	s = "0123456789ABCDEF";
	supstr_assign(ss, s);
	n = supstr_size(ss);
	cac_asserteq(n, 16);
	n = supstr_bufsize(ss);
	cac_asserteq(n, bsz);
	eq = substr_isequal(&ss->sub, s);
	cac_assert(eq);
	eq = substr_isequal(&ss->sub, ss->sub.str);
	cac_assert(eq);

	s = "0123456789ABCDEF++++++";
	supstr_assign(ss, s);
	n = supstr_size(ss);
	cac_asserteq(n, bsz);
	n = supstr_bufsize(ss);
	cac_asserteq(n, bsz);
	eq = substr_isequal(&ss->sub, s);
	cac_assert(!eq);
	eq = substr_isequal(&ss->sub, ss->sub.str);
	cac_assert(eq);

	supstr_destroy(ss);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_supstr_append1(supstr_t *ss)
{
	int eq;
	size_t n;
	char buf[20];
	const char *s;
	const size_t bsz = cac_nelems(buf);

	supstr_ninit(ss, buf, 0, bsz);
	s = "0123456789abcdef";
	supstr_append(ss, s);
	n = supstr_size(ss);
	cac_asserteq(n, 16);
	n = supstr_bufsize(ss);
	cac_asserteq(n, bsz);
	eq = substr_isequal(&ss->sub, s);
	cac_assert(eq);

	supstr_append(ss, s);
	n = supstr_size(ss);
	cac_asserteq(n, bsz);

	supstr_ninit(ss, buf, 0, bsz);
	s = "01";
	supstr_nappend(ss, s, 1);
	n = supstr_size(ss);
	cac_asserteq(n, 1);
	supstr_nappend(ss, s + 1, 1);
	n = supstr_size(ss);
	cac_asserteq(n, 2);
	eq = substr_isequal(&ss->sub, s);
	cac_assert(eq);

	supstr_destroy(ss);
}

static void test_supstr_append2(supstr_t *ss)
{
	size_t n, k;
	char buf[100];
	const char *s;
	const size_t bsz = cac_nelems(buf);

	supstr_ninit(ss, buf, 0, bsz);
	s = "X";
	for (size_t i = 0; i < bsz; ++i) {
		n = supstr_size(ss);
		cac_asserteq(n, i);

		supstr_append(ss, s);
		n = supstr_size(ss);
		cac_asserteq(n, (i + 1));

		k = substr_find_last_of(&ss->sub, s);
		cac_asserteq(k, (n - 1));
	}
	n = supstr_size(ss);
	cac_asserteq(n, bsz);

	supstr_append(ss, s);
	n = supstr_size(ss);
	cac_asserteq(n, bsz);

	supstr_destroy(ss);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_supstr_insert(supstr_t *ss)
{
	int eq;
	size_t n;
	const char *s;
	char buf[20];
	const size_t bsz = cac_nelems(buf);

	supstr_ninit(ss, buf, 0, bsz);
	s = "0123456789";
	supstr_insert(ss, 0, s);
	n = supstr_size(ss);
	cac_asserteq(n, 10);
	eq = substr_isequal(&ss->sub, s);
	cac_assert(eq);

	supstr_insert(ss, 10, s);
	n = supstr_size(ss);
	cac_asserteq(n, 20);
	eq = substr_isequal(&ss->sub, "01234567890123456789");
	cac_assert(eq);

	supstr_insert(ss, 1, "....");
	n = supstr_size(ss);
	cac_asserteq(n, 20);
	eq = substr_isequal(&ss->sub, "0....123456789012345");
	cac_assert(eq);

	supstr_insert(ss, 16, "%%%");
	n = supstr_size(ss);
	cac_asserteq(n, 20);
	eq = substr_isequal(&ss->sub, "0....12345678901%%%2");
	cac_assert(eq);

	supstr_insert_chr(ss, 1, 20, '$');
	n = supstr_size(ss);
	cac_asserteq(n, 20);
	eq = substr_isequal(&ss->sub, "0$$$$$$$$$$$$$$$$$$$");
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_supstr_replace(supstr_t *ss)
{
	int eq;
	size_t n;
	const char *s;
	char buf[10];
	const size_t bsz = cac_nelems(buf);

	supstr_init(ss, buf, bsz);
	s = "ABCDEF";
	supstr_replace(ss, 0, 2, s);
	n = supstr_size(ss);
	cac_asserteq(n, 6);
	eq = substr_isequal(&ss->sub, s);
	cac_assert(eq);

	supstr_replace(ss, 1, 2, s);
	eq = substr_isequal(&ss->sub, "AABCDEFDEF");
	cac_assert(eq);

	supstr_replace(ss, 6, 3, s);
	eq = substr_isequal(&ss->sub, "AABCDEABCD");
	cac_assert(eq);

	supstr_replace_chr(ss, 0, 10, 30, '.');
	eq = substr_isequal(&ss->sub, "..........");
	cac_assert(eq);

	supstr_replace_chr(ss, 1, 8, 4, 'A');
	eq = substr_isequal(&ss->sub, ".AAAA.");
	cac_assert(eq);

	supstr_nreplace(ss, 2, 80,
	                substr_data(&ss->sub),
	                substr_size(&ss->sub));
	eq = substr_isequal(&ss->sub, ".A.AAAA.");
	cac_assert(eq);

	supstr_nreplace(ss, 4, 80,
	                substr_data(&ss->sub),
	                substr_size(&ss->sub));
	eq = substr_isequal(&ss->sub, ".A.A.A.AAA");  /* Truncated */
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_supstr_erase(supstr_t *ss)
{
	int eq;
	size_t n;
	char buf[5];
	const size_t bsz = cac_nelems(buf);

	supstr_ninit(ss, buf, 0, bsz);

	supstr_assign(ss, "ABCDEF");
	eq = substr_isequal(&ss->sub, "ABCDE");
	cac_assert(eq);

	supstr_erase(ss, 1, 2);
	eq = substr_isequal(&ss->sub, "ADE");
	cac_assert(eq);

	supstr_erase(ss, 0, 100);
	eq = substr_isequal(&ss->sub, "");
	cac_assert(eq);

	n = supstr_bufsize(ss);
	cac_asserteq(n, bsz);

	supstr_destroy(ss);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_supstr_reverse(supstr_t *ss)
{
	int eq;
	char buf[40];
	const size_t bsz = cac_nelems(buf);

	supstr_ninit(ss, buf, 0, bsz);
	supstr_assign(ss, "abracadabra");
	supstr_reverse(ss);

	eq = substr_isequal(&ss->sub, "arbadacarba");
	cac_assert(eq);

	supstr_destroy(ss);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void cac_test_supstr(void)
{
	supstr_t supstr;
	supstr_t *ss = &supstr;

	test_supstr_assign(ss);
	test_supstr_append1(ss);
	test_supstr_append2(ss);
	test_supstr_insert(ss);
	test_supstr_replace(ss);
	test_supstr_erase(ss);
	test_supstr_reverse(ss);
}


