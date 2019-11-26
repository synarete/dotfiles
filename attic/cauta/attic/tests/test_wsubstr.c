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

static void test_wsubstr_isequal(wsubstr_t *ss)
{
	int eq;
	const wchar_t *s = L"###";

	wsubstr_init(ss, s);
	eq = wsubstr_isequal(ss, s);
	cac_assert(eq);

	eq = wsubstr_isequal(ss, L"###");
	cac_assert(eq);

	eq = wsubstr_isequal(ss, L"##");
	cac_assert(!eq);

	eq = wsubstr_isequal(ss, L"#");
	cac_assert(!eq);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsubstr_compare(wsubstr_t *ss)
{
	int cmp, eq, emp;
	size_t sz;

	wsubstr_init(ss, L"123456789");
	sz = wsubstr_size(ss);
	cac_assert(sz == 9);

	emp = wsubstr_isempty(ss);
	cac_assert(!emp);

	cmp = wsubstr_compare(ss, L"123");
	cac_assert(cmp > 0);

	cmp = wsubstr_compare(ss, L"9");
	cac_assert(cmp < 0);

	cmp = wsubstr_compare(ss, L"123456789");
	cac_assert(cmp == 0);

	eq = wsubstr_isequal(ss, L"123456789");
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsubstr_find(wsubstr_t *ss)
{
	size_t n, pos, len;

	wsubstr_init(ss, L"ABCDEF abcdef ABCDEF");

	n = wsubstr_find(ss, L"A");
	cac_assert(n == 0);

	n = wsubstr_find(ss, L"EF");
	cac_assert(n == 4);

	pos = 10;
	len = 2;
	n = wsubstr_nfind(ss, pos, L"EF", len);
	cac_assert(n == 18);

	pos = 1;
	n = wsubstr_find_chr(ss, pos, L'A');
	cac_assert(n == 14);

	n = wsubstr_find(ss, L"UUU");
	cac_assert(n > wsubstr_size(ss));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsubstr_rfind(wsubstr_t *ss)
{
	size_t n, pos, len;

	wsubstr_init(ss, L"ABRACADABRA");

	n = wsubstr_rfind(ss, L"A");
	cac_assert(n == 10);

	n = wsubstr_rfind(ss, L"BR");
	cac_assert(n == 8);

	pos = wsubstr_size(ss) / 2;
	len = 2;
	n = wsubstr_nrfind(ss, pos, L"BR", len);
	cac_assert(n == 1);

	pos = 1;
	n = wsubstr_rfind_chr(ss, pos, L'B');
	cac_assert(n == 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsubstr_find_first_of(wsubstr_t *ss)
{
	size_t n, pos;

	wsubstr_init(ss, L"012x456x89z");

	n = wsubstr_find_first_of(ss, L"xyz");
	cac_assert(n == 3);

	pos = 5;
	n = wsubstr_nfind_first_of(ss, pos, L"x..z", 4);
	cac_assert(n == 7);

	n = wsubstr_find_first_of(ss, L"XYZ");
	cac_assert(n > wsubstr_size(ss));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsubstr_find_last_of(wsubstr_t *ss)
{
	size_t n, pos;

	wsubstr_init(ss, L"AAAAA-BBBBB");

	n = wsubstr_find_last_of(ss, L"xyzAzyx");
	cac_assert(n == 4);

	pos = 9;
	n = wsubstr_nfind_last_of(ss, pos, L"X-Y", 3);
	cac_assert(n == 5);

	n = wsubstr_find_last_of(ss, L"BBBBBBBBBBBBBBBBBBBBB");
	cac_assert(n == (wsubstr_size(ss) - 1));

	n = wsubstr_find_last_of(ss, L"...");
	cac_assert(n > wsubstr_size(ss));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsubstr_find_first_not_of(wsubstr_t *ss)
{
	size_t n, pos;

	wsubstr_init(ss, L"aaa bbb ccc * ddd + eee");

	n = wsubstr_find_first_not_of(ss, L"a b c d e");
	cac_assert(n == 12);

	pos = 14;
	n = wsubstr_nfind_first_not_of(ss, pos, L"d e", 3);
	cac_assert(n == 18);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsubstr_find_last_not_of(wsubstr_t *ss)
{
	size_t n, pos;
	const wchar_t *s = L"-..1234.--";

	wsubstr_init(ss, s);

	n = wsubstr_find_last_not_of(ss, L".-");
	cac_assert(n == 6);

	n = wsubstr_find_last_not_of(ss, L"ABC");
	cac_assert(n == 9);

	n = wsubstr_find_last_not_of(ss, L"-");
	cac_assert(n == 7);

	pos = 1;
	n = wsubstr_nfind_last_not_of(ss, pos, L"*", 1);
	cac_assert(n == 1);

	wsubstr_init(ss, L"");
	n = wsubstr_nfind_last_not_of(ss, 0, L"XX", 2);
	cac_assert(n > 0);

	n = wsubstr_nfind_last_not_of(ss, 1, L"ZZZ", 3);
	cac_assert(n > 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsubstr_sub(wsubstr_t *ss)
{
	int rc;
	wsubstr_t sub;
	const wchar_t *abc;

	abc = L"abcdefghijklmnopqrstuvwxyz";

	wsubstr_ninit(ss, abc, 10);    /* L"abcdefghij" */
	wsubstr_sub(ss, 2, 4, &sub);
	rc  = wsubstr_isequal(&sub, L"cdef");
	cac_assert(rc == 1);

	wsubstr_rsub(ss, 3, &sub);
	rc  = wsubstr_isequal(&sub, L"hij");
	cac_assert(rc == 1);

	wsubstr_chop(ss, 8, &sub);
	rc  = wsubstr_isequal(&sub, L"ab");
	cac_assert(rc == 1);

	wsubstr_clone(ss, &sub);
	rc  = wsubstr_nisequal(&sub, ss->str, ss->len);
	cac_assert(rc == 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsubstr_count(wsubstr_t *ss)
{
	size_t n;
	const wchar_t *s = L"xxx-xxx-xxx-xxx";
	const wchar_t *abc = L"ABCAABBCCAAABBBCCC";

	wsubstr_init(ss, s);

	n = wsubstr_count(ss, s);
	cac_assert(n == 1);

	n = wsubstr_count(ss, L"xxx");
	cac_assert(n == 4);

	n = wsubstr_count(ss, L"xx");
	cac_assert(n == 4);

	n = wsubstr_count(ss, L"x");
	cac_assert(n == 12);

	n = wsubstr_count_chr(ss, '-');
	cac_assert(n == 3);

	n = wsubstr_count_chr(ss, 'x');
	cac_assert(n == 12);


	wsubstr_init(ss, abc);

	n = wsubstr_count(ss, s);
	cac_assert(n == 0);

	n = wsubstr_count(ss, abc);
	cac_assert(n == 1);

	n = wsubstr_count(ss, L"B");
	cac_assert(n == 6);

	n = wsubstr_count(ss, L"BB");
	cac_assert(n == 2);

	n = wsubstr_count(ss, L"CC");
	cac_assert(n == 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsubstr_split(wsubstr_t *ss)
{
	int eq;
	wsubstr_pair_t split;

	wsubstr_init(ss, L"ABC-DEF+123");
	wsubstr_split(ss, L"-", &split);

	eq = wsubstr_isequal(&split.first, L"ABC");
	cac_assert(eq);

	eq = wsubstr_isequal(&split.second, L"DEF+123");
	cac_assert(eq);

	wsubstr_split(ss, L" + * L", &split);
	eq = wsubstr_isequal(&split.first, L"ABC-DEF");
	cac_assert(eq);

	eq = wsubstr_isequal(&split.second, L"123");
	cac_assert(eq);


	wsubstr_split_chr(ss, 'B', &split);
	eq = wsubstr_isequal(&split.first, L"A");
	cac_assert(eq);

	eq = wsubstr_isequal(&split.second, L"C-DEF+123");
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsubstr_rsplit(wsubstr_t *ss)
{
	int eq;
	wsubstr_pair_t split;

	wsubstr_init(ss, L"UUU--YYY--ZZZ");
	wsubstr_rsplit(ss, L"-.", &split);

	eq = wsubstr_isequal(&split.first, L"UUU--YYY");
	cac_assert(eq);

	eq = wsubstr_isequal(&split.second, L"ZZZ");
	cac_assert(eq);


	wsubstr_rsplit(ss, L"+", &split);

	eq = wsubstr_nisequal(&split.first, ss->str, ss->len);
	cac_assert(eq);

	eq = wsubstr_isequal(&split.second, L"ZZZ");
	cac_assert(!eq);


	wsubstr_init(ss, L"1.2.3.4.5");
	wsubstr_rsplit_chr(ss, '.', &split);

	eq = wsubstr_isequal(&split.first, L"1.2.3.4");
	cac_assert(eq);

	eq = wsubstr_isequal(&split.second, L"5");
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsubstr_trim(wsubstr_t *ss)
{
	int eq;
	wsubstr_t sub;

	wsubstr_init(ss, L".:ABCD");

	wsubstr_trim_any_of(ss, L":,.%^", &sub);
	eq  = wsubstr_isequal(&sub, L"ABCD");
	cac_assert(eq);

	wsubstr_ntrim_any_of(ss,
	                     wsubstr_data(ss),
	                     wsubstr_size(ss),
	                     &sub);
	eq  = wsubstr_size(&sub) == 0;
	cac_assert(eq);

	wsubstr_trim_chr(ss, '.', &sub);
	eq  = wsubstr_isequal(&sub, L":ABCD");
	cac_assert(eq);

	wsubstr_trim(ss, 4, &sub);
	eq  = wsubstr_isequal(&sub, L"CD");
	cac_assert(eq);

	wsubstr_init(ss, L" X");
	wsubstr_trim_if(ss, wchr_isspace, &sub);
	eq  = wsubstr_isequal(&sub, L"X");
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsubstr_chop(wsubstr_t *ss)
{
	int eq;
	wsubstr_t sub;

	wsubstr_init(ss, L"123....");

	wsubstr_chop_any_of(ss, L"+*&^%$.", &sub);
	eq  = wsubstr_isequal(&sub, L"123");
	cac_assert(eq);

	wsubstr_nchop_any_of(ss,
	                     wsubstr_data(ss),
	                     wsubstr_size(ss),
	                     &sub);
	eq  = wsubstr_isequal(&sub, L"");
	cac_assert(eq);

	wsubstr_chop(ss, 6, &sub);
	eq  = wsubstr_isequal(&sub, L"1");
	cac_assert(eq);

	wsubstr_chop_chr(ss, '.', &sub);
	eq  = wsubstr_isequal(&sub, L"123");
	cac_assert(eq);

	wsubstr_chop_if(ss, wchr_ispunct, &sub);
	eq  = wsubstr_isequal(&sub, L"123");
	cac_assert(eq);

	wsubstr_chop_if(ss, wchr_isprint, &sub);
	eq  = wsubstr_size(&sub) == 0;
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsubstr_strip(wsubstr_t *ss)
{
	int eq;
	size_t sz;
	const wchar_t *s;
	const wchar_t *s2 = L"s ";
	wsubstr_t sub;

	wsubstr_init(ss, L".....#XYZ#.........");

	wsubstr_strip_any_of(ss, L"-._#", &sub);
	eq  = wsubstr_isequal(&sub, L"XYZ");
	cac_assert(eq);

	wsubstr_strip_chr(ss, '.', &sub);
	eq  = wsubstr_isequal(&sub, L"#XYZ#");
	cac_assert(eq);

	wsubstr_strip_if(ss, wchr_ispunct, &sub);
	eq  = wsubstr_isequal(&sub, L"XYZ");
	cac_assert(eq);

	s  = wsubstr_data(ss);
	sz = wsubstr_size(ss);
	wsubstr_nstrip_any_of(ss, s, sz, &sub);
	eq  = wsubstr_isequal(&sub, L"");
	cac_assert(eq);

	wsubstr_init(ss, L" \t ABC\n\r\v");
	wsubstr_strip_ws(ss, &sub);
	eq = wsubstr_isequal(&sub, L"ABC");
	cac_assert(eq);

	wsubstr_init(ss, s2);
	wsubstr_strip_if(ss, wchr_isspace, &sub);
	eq  = wsubstr_isequal(&sub, L"s");
	cac_assert(eq);

	wsubstr_init(ss, s2 + 1);
	wsubstr_strip_if(ss, wchr_isspace, &sub);
	eq  = wsubstr_isequal(&sub, L"");
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsubstr_find_token(wsubstr_t *ss)
{
	int eq;
	wsubstr_t tok;
	const wchar_t *seps;

	seps   = L" \t\n\v\r";
	wsubstr_init(ss, L" A BB \t  CCC    DDDD  \n");

	wsubstr_find_token(ss, seps, &tok);
	eq  = wsubstr_isequal(&tok, L"A");
	cac_assert(eq);

	wsubstr_find_next_token(ss, &tok, seps, &tok);
	eq  = wsubstr_isequal(&tok, L"BB");
	cac_assert(eq);

	wsubstr_find_next_token(ss, &tok, seps, &tok);
	eq  = wsubstr_isequal(&tok, L"CCC");
	cac_assert(eq);

	wsubstr_find_next_token(ss, &tok, seps, &tok);
	eq  = wsubstr_isequal(&tok, L"DDDD");
	cac_assert(eq);

	wsubstr_find_next_token(ss, &tok, seps, &tok);
	eq  = wsubstr_isequal(&tok, L"");
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsubstr_tokenize(wsubstr_t *ss)
{
	int rc, eq;
	size_t n_tseq;
	wsubstr_t tseq_list[7];
	const wchar_t *seps = L" /:;.| L" ;
	const wchar_t *line;

	line = L"    /Ant:::Bee;:Cat:Dog;...Elephant.../Frog:/Giraffe///    L";
	wsubstr_init(ss, line);

	rc = wsubstr_tokenize(ss, seps, tseq_list, 7, &n_tseq);
	cac_assert(rc == 0);
	cac_assert(n_tseq == 7);

	eq  = wsubstr_isequal(&tseq_list[0], L"Ant");
	cac_assert(eq);

	eq  = wsubstr_isequal(&tseq_list[4], L"Elephant");
	cac_assert(eq);

	eq  = wsubstr_isequal(&tseq_list[6], L"Giraffe");
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsubstr_common_prefix(wsubstr_t *ss)
{
	int eq;
	size_t n;
	wchar_t buf1[] = L"0123456789abcdef";

	wsubstr_init(ss, buf1);

	n = wsubstr_common_prefix(ss, L"0123456789ABCDEF");
	eq = (n == 10);
	cac_assert(eq);

	n = wsubstr_common_prefix(ss, buf1);
	eq = (n == 16);
	cac_assert(eq);

	n = wsubstr_common_prefix(ss, L"XYZ");
	eq = (n == 0);
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsubstr_common_suffix(wsubstr_t *ss)
{
	int eq;
	size_t n;
	wchar_t buf1[] = L"abcdef0123456789";

	wsubstr_init(ss, buf1);

	n = wsubstr_common_suffix(ss, L"ABCDEF0123456789");
	eq = (n == 10);
	cac_assert(eq);

	n = wsubstr_common_suffix(ss, buf1);
	eq = (n == 16);
	cac_assert(eq);

	n = wsubstr_common_suffix(ss, L"XYZ");
	eq = (n == 0);
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_wsubstr_copyto(wsubstr_t *ss)
{
	int eq;
	wchar_t buf[10];
	wchar_t pad = L'@';
	size_t sz;
	const wchar_t *s = L"123456789";
	const wchar_t *x = L"XXXXXXXXXXXX";

	wsubstr_init(ss, s);
	sz = wsubstr_size(ss);
	cac_assert(sz == 9);

	sz = wsubstr_copyto(ss, buf, cac_nelems(buf));
	cac_assert(sz == 9);

	eq = (wstr_ncompare(buf, sz, s, sz) == 0);
	cac_assert(eq);
	cac_assert(pad == L'@');

	sz = wsubstr_copyto(ss, buf, 1);
	cac_assert(sz == 1);

	sz = wsubstr_copyto(ss, buf, 0);
	cac_assert(sz == 0);

	wsubstr_init(ss, x);

	sz = wsubstr_copyto(ss, buf, 1);
	cac_assert(sz == 1);

	eq = (wstr_ncompare(buf, 1, x, 1) == 0);
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void cac_test_wsubstr(void)
{
	wsubstr_t wsubstr;
	wsubstr_t *ss = &wsubstr;

	test_wsubstr_isequal(ss);
	test_wsubstr_compare(ss);
	test_wsubstr_find(ss);
	test_wsubstr_rfind(ss);
	test_wsubstr_find_first_of(ss);
	test_wsubstr_find_last_of(ss);
	test_wsubstr_find_first_not_of(ss);
	test_wsubstr_find_last_not_of(ss);
	test_wsubstr_sub(ss);
	test_wsubstr_count(ss);
	test_wsubstr_split(ss);
	test_wsubstr_rsplit(ss);
	test_wsubstr_trim(ss);
	test_wsubstr_chop(ss);
	test_wsubstr_strip(ss);
	test_wsubstr_find_token(ss);
	test_wsubstr_tokenize(ss);
	test_wsubstr_common_prefix(ss);
	test_wsubstr_common_suffix(ss);
	test_wsubstr_copyto(ss);

	wsubstr_destroy(ss);
}
