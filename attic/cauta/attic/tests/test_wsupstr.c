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

static void test_wsupstr_assign(wsupstr_t *ss)
{
	int eq;
	size_t n;
	wchar_t buf[] = L"0123456789......";
	const wchar_t *s;
	const size_t bsz = cac_nelems(buf) - 1;

	wsupstr_ninit(ss, buf, 10, bsz);
	cac_asserteq(ss->bsz, bsz);

	s = L"ABC";
	wsupstr_assign(ss, s);
	n = wsupstr_size(ss);
	cac_asserteq(n, 3);
	n = wsupstr_bufsize(ss);
	cac_asserteq(n, bsz);
	eq = wsubstr_isequal(&ss->sub, s);
	cac_assert(eq);
	eq = wsubstr_isequal(&ss->sub, ss->sub.str);
	cac_assert(eq);

	s = L"ZZZZ";
	wsupstr_assign(ss, s);
	n = wsupstr_size(ss);
	cac_asserteq(n, 4);
	n = wsupstr_bufsize(ss);
	cac_asserteq(n, bsz);
	eq = wsubstr_isequal(&ss->sub, s);
	cac_assert(eq);

	s = L"0123456789ABCDEF";
	wsupstr_assign(ss, s);
	n = wsupstr_size(ss);
	cac_asserteq(n, 16);
	n = wsupstr_bufsize(ss);
	cac_asserteq(n, bsz);
	eq = wsubstr_isequal(&ss->sub, s);
	cac_assert(eq);
	eq = wsubstr_isequal(&ss->sub, ss->sub.str);
	cac_assert(eq);

	s = L"0123456789ABCDEF++++++";
	wsupstr_assign(ss, s);
	n = wsupstr_size(ss);
	cac_asserteq(n, bsz);
	n = wsupstr_bufsize(ss);
	cac_asserteq(n, bsz);
	eq = wsubstr_isequal(&ss->sub, s);
	cac_assert(!eq);
	eq = wsubstr_isequal(&ss->sub, ss->sub.str);
	cac_assert(eq);

	wsupstr_destroy(ss);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsupstr_append1(wsupstr_t *ss)
{
	int eq;
	size_t n;
	wchar_t buf[20];
	const wchar_t *s;
	const size_t bsz = cac_nelems(buf);

	wsupstr_ninit(ss, buf, 0, bsz);
	s = L"0123456789abcdef";
	wsupstr_append(ss, s);
	n = wsupstr_size(ss);
	cac_asserteq(n, 16);
	n = wsupstr_bufsize(ss);
	cac_asserteq(n, bsz);
	eq = wsubstr_isequal(&ss->sub, s);
	cac_assert(eq);

	wsupstr_append(ss, s);
	n = wsupstr_size(ss);
	cac_asserteq(n, bsz);

	wsupstr_ninit(ss, buf, 0, bsz);
	s = L"01";
	wsupstr_nappend(ss, s, 1);
	n = wsupstr_size(ss);
	cac_asserteq(n, 1);
	wsupstr_nappend(ss, s + 1, 1);
	n = wsupstr_size(ss);
	cac_asserteq(n, 2);
	eq = wsubstr_isequal(&ss->sub, s);
	cac_assert(eq);

	wsupstr_destroy(ss);
}

static void test_wsupstr_append2(wsupstr_t *ss)
{
	size_t n, k;
	wchar_t buf[100];
	const wchar_t *s;
	const size_t bsz = cac_nelems(buf);

	wsupstr_ninit(ss, buf, 0, bsz);
	s = L"X";
	for (size_t i = 0; i < bsz; ++i) {
		n = wsupstr_size(ss);
		cac_asserteq(n, i);

		wsupstr_append(ss, s);
		n = wsupstr_size(ss);
		cac_asserteq(n, (i + 1));

		k = wsubstr_find_last_of(&ss->sub, s);
		cac_asserteq(k, (n - 1));
	}
	n = wsupstr_size(ss);
	cac_asserteq(n, bsz);

	wsupstr_append(ss, s);
	n = wsupstr_size(ss);
	cac_asserteq(n, bsz);

	wsupstr_destroy(ss);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsupstr_insert(wsupstr_t *ss)
{
	int eq;
	size_t n;
	const wchar_t *s;
	wchar_t buf[20];
	const size_t bsz = cac_nelems(buf);

	wsupstr_ninit(ss, buf, 0, bsz);
	s = L"0123456789";
	wsupstr_insert(ss, 0, s);
	n = wsupstr_size(ss);
	cac_asserteq(n, 10);
	eq = wsubstr_isequal(&ss->sub, s);
	cac_assert(eq);

	wsupstr_insert(ss, 10, s);
	n = wsupstr_size(ss);
	cac_asserteq(n, 20);
	eq = wsubstr_isequal(&ss->sub, L"01234567890123456789");
	cac_assert(eq);

	wsupstr_insert(ss, 1, L"....");
	n = wsupstr_size(ss);
	cac_asserteq(n, 20);
	eq = wsubstr_isequal(&ss->sub, L"0....123456789012345");
	cac_assert(eq);

	wsupstr_insert(ss, 16, L"%%%");
	n = wsupstr_size(ss);
	cac_asserteq(n, 20);
	eq = wsubstr_isequal(&ss->sub, L"0....12345678901%%%2");
	cac_assert(eq);

	wsupstr_insert_chr(ss, 1, 20, '$');
	n = wsupstr_size(ss);
	cac_asserteq(n, 20);
	eq = wsubstr_isequal(&ss->sub, L"0$$$$$$$$$$$$$$$$$$$");
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsupstr_replace(wsupstr_t *ss)
{
	int eq;
	size_t n;
	const wchar_t *s;
	wchar_t buf[10];
	const size_t bsz = cac_nelems(buf);

	wsupstr_init(ss, buf, bsz);
	s = L"ABCDEF";
	wsupstr_replace(ss, 0, 2, s);
	n = wsupstr_size(ss);
	cac_asserteq(n, 6);
	eq = wsubstr_isequal(&ss->sub, s);
	cac_assert(eq);

	wsupstr_replace(ss, 1, 2, s);
	eq = wsubstr_isequal(&ss->sub, L"AABCDEFDEF");
	cac_assert(eq);

	wsupstr_replace(ss, 6, 3, s);
	eq = wsubstr_isequal(&ss->sub, L"AABCDEABCD");
	cac_assert(eq);

	wsupstr_replace_chr(ss, 0, 10, 30, '.');
	eq = wsubstr_isequal(&ss->sub, L"..........");
	cac_assert(eq);

	wsupstr_replace_chr(ss, 1, 8, 4, 'A');
	eq = wsubstr_isequal(&ss->sub, L".AAAA.");
	cac_assert(eq);

	wsupstr_nreplace(ss, 2, 80,
	                 wsubstr_data(&ss->sub),
	                 wsubstr_size(&ss->sub));
	eq = wsubstr_isequal(&ss->sub, L".A.AAAA.");
	cac_assert(eq);

	wsupstr_nreplace(ss, 4, 80,
	                 wsubstr_data(&ss->sub),
	                 wsubstr_size(&ss->sub));
	eq = wsubstr_isequal(&ss->sub, L".A.A.A.AAA");  /* Truncated */
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsupstr_erase(wsupstr_t *ss)
{
	int eq;
	size_t n;
	wchar_t buf[5];
	const size_t bsz = cac_nelems(buf);

	wsupstr_ninit(ss, buf, 0, bsz);

	wsupstr_assign(ss, L"ABCDEF");
	eq = wsubstr_isequal(&ss->sub, L"ABCDE");
	cac_assert(eq);

	wsupstr_erase(ss, 1, 2);
	eq = wsubstr_isequal(&ss->sub, L"ADE");
	cac_assert(eq);

	wsupstr_erase(ss, 0, 100);
	eq = wsubstr_isequal(&ss->sub, L"");
	cac_assert(eq);

	n = wsupstr_bufsize(ss);
	cac_asserteq(n, bsz);

	wsupstr_destroy(ss);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsupstr_reverse(wsupstr_t *ss)
{
	int eq;
	wchar_t buf[40];
	const size_t bsz = cac_nelems(buf);

	wsupstr_ninit(ss, buf, 0, bsz);
	wsupstr_assign(ss, L"abracadabra");
	wsupstr_reverse(ss);

	eq = wsubstr_isequal(&ss->sub, L"arbadacarba");
	cac_assert(eq);

	wsupstr_destroy(ss);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void cac_test_wsupstr(void)
{
	wsupstr_t wsupstr;
	wsupstr_t *ss = &wsupstr;

	test_wsupstr_assign(ss);
	test_wsupstr_append1(ss);
	test_wsupstr_append2(ss);
	test_wsupstr_insert(ss);
	test_wsupstr_replace(ss);
	test_wsupstr_erase(ss);
	test_wsupstr_reverse(ss);
}


