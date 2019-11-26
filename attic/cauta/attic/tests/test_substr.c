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

static void test_substr_isequal(substr_t *ss)
{
	int eq;
	const char *s = "###";

	substr_init(ss, s);
	eq = substr_isequal(ss, s);
	cac_assert(eq);

	eq = substr_isequal(ss, "###");
	cac_assert(eq);

	eq = substr_isequal(ss, "##");
	cac_assert(!eq);

	eq = substr_isequal(ss, "#");
	cac_assert(!eq);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_substr_compare(substr_t *ss)
{
	int cmp, eq, emp;
	size_t sz;

	substr_init(ss, "123456789");
	sz = substr_size(ss);
	cac_assert(sz == 9);

	emp = substr_isempty(ss);
	cac_assert(!emp);

	cmp = substr_compare(ss, "123");
	cac_assert(cmp > 0);

	cmp = substr_compare(ss, "9");
	cac_assert(cmp < 0);

	cmp = substr_compare(ss, "123456789");
	cac_assert(cmp == 0);

	eq = substr_isequal(ss, "123456789");
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_substr_find(substr_t *ss)
{
	size_t n, pos, len;

	substr_init(ss, "ABCDEF abcdef ABCDEF");

	n = substr_find(ss, "A");
	cac_assert(n == 0);

	n = substr_find(ss, "EF");
	cac_assert(n == 4);

	pos = 10;
	len = 2;
	n = substr_nfind(ss, pos, "EF", len);
	cac_assert(n == 18);

	pos = 1;
	n = substr_find_chr(ss, pos, 'A');
	cac_assert(n == 14);

	n = substr_find(ss, "UUU");
	cac_assert(n > substr_size(ss));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_substr_rfind(substr_t *ss)
{
	size_t n, pos, len;

	substr_init(ss, "ABRACADABRA");

	n = substr_rfind(ss, "A");
	cac_assert(n == 10);

	n = substr_rfind(ss, "BR");
	cac_assert(n == 8);

	pos = substr_size(ss) / 2;
	len = 2;
	n = substr_nrfind(ss, pos, "BR", len);
	cac_assert(n == 1);

	pos = 1;
	n = substr_rfind_chr(ss, pos, 'B');
	cac_assert(n == 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_substr_find_first_of(substr_t *ss)
{
	size_t n, pos;

	substr_init(ss, "012x456x89z");

	n = substr_find_first_of(ss, "xyz");
	cac_assert(n == 3);

	pos = 5;
	n = substr_nfind_first_of(ss, pos, "x..z", 4);
	cac_assert(n == 7);

	n = substr_find_first_of(ss, "XYZ");
	cac_assert(n > substr_size(ss));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_substr_find_last_of(substr_t *ss)
{
	size_t n, pos;

	substr_init(ss, "AAAAA-BBBBB");

	n = substr_find_last_of(ss, "xyzAzyx");
	cac_assert(n == 4);

	pos = 9;
	n = substr_nfind_last_of(ss, pos, "X-Y", 3);
	cac_assert(n == 5);

	n = substr_find_last_of(ss, "BBBBBBBBBBBBBBBBBBBBB");
	cac_assert(n == (substr_size(ss) - 1));

	n = substr_find_last_of(ss, "...");
	cac_assert(n > substr_size(ss));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_substr_find_first_not_of(substr_t *ss)
{
	size_t n, pos;

	substr_init(ss, "aaa bbb ccc * ddd + eee");

	n = substr_find_first_not_of(ss, "a b c d e");
	cac_assert(n == 12);

	pos = 14;
	n = substr_nfind_first_not_of(ss, pos, "d e", 3);
	cac_assert(n == 18);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_substr_find_last_not_of(substr_t *ss)
{
	size_t n, pos;
	const char *s = "-..1234.--";

	substr_init(ss, s);

	n = substr_find_last_not_of(ss, ".-");
	cac_assert(n == 6);

	n = substr_find_last_not_of(ss, "ABC");
	cac_assert(n == 9);

	n = substr_find_last_not_of(ss, "-");
	cac_assert(n == 7);

	pos = 1;
	n = substr_nfind_last_not_of(ss, pos, "*", 1);
	cac_assert(n == 1);

	substr_init(ss, "");
	n = substr_nfind_last_not_of(ss, 0, "XX", 2);
	cac_assert(n > 0);

	n = substr_nfind_last_not_of(ss, 1, "ZZZ", 3);
	cac_assert(n > 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_substr_sub(substr_t *ss)
{
	int rc;
	substr_t sub;
	const char *abc;

	abc = "abcdefghijklmnopqrstuvwxyz";

	substr_ninit(ss, abc, 10);    /* "abcdefghij" */
	substr_sub(ss, 2, 4, &sub);
	rc  = substr_isequal(&sub, "cdef");
	cac_assert(rc == 1);

	substr_rsub(ss, 3, &sub);
	rc  = substr_isequal(&sub, "hij");
	cac_assert(rc == 1);

	substr_chop(ss, 8, &sub);
	rc  = substr_isequal(&sub, "ab");
	cac_assert(rc == 1);

	substr_clone(ss, &sub);
	rc  = substr_nisequal(&sub, ss->str, ss->len);
	cac_assert(rc == 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_substr_count(substr_t *ss)
{
	size_t n;
	const char *s = "xxx-xxx-xxx-xxx";
	const char *abc = "ABCAABBCCAAABBBCCC";

	substr_init(ss, s);

	n = substr_count(ss, s);
	cac_assert(n == 1);

	n = substr_count(ss, "xxx");
	cac_assert(n == 4);

	n = substr_count(ss, "xx");
	cac_assert(n == 4);

	n = substr_count(ss, "x");
	cac_assert(n == 12);

	n = substr_count_chr(ss, '-');
	cac_assert(n == 3);

	n = substr_count_chr(ss, 'x');
	cac_assert(n == 12);


	substr_init(ss, abc);

	n = substr_count(ss, s);
	cac_assert(n == 0);

	n = substr_count(ss, abc);
	cac_assert(n == 1);

	n = substr_count(ss, "B");
	cac_assert(n == 6);

	n = substr_count(ss, "BB");
	cac_assert(n == 2);

	n = substr_count(ss, "CC");
	cac_assert(n == 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_substr_split(substr_t *ss)
{
	int eq;
	substr_pair_t split;

	substr_init(ss, "ABC-DEF+123");
	substr_split(ss, "-", &split);

	eq = substr_isequal(&split.first, "ABC");
	cac_assert(eq);

	eq = substr_isequal(&split.second, "DEF+123");
	cac_assert(eq);

	substr_split(ss, " + * ", &split);
	eq = substr_isequal(&split.first, "ABC-DEF");
	cac_assert(eq);

	eq = substr_isequal(&split.second, "123");
	cac_assert(eq);


	substr_split_chr(ss, 'B', &split);
	eq = substr_isequal(&split.first, "A");
	cac_assert(eq);

	eq = substr_isequal(&split.second, "C-DEF+123");
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_substr_rsplit(substr_t *ss)
{
	int eq;
	substr_pair_t split;

	substr_init(ss, "UUU--YYY--ZZZ");
	substr_rsplit(ss, "-.", &split);

	eq = substr_isequal(&split.first, "UUU--YYY");
	cac_assert(eq);

	eq = substr_isequal(&split.second, "ZZZ");
	cac_assert(eq);


	substr_rsplit(ss, "+", &split);

	eq = substr_nisequal(&split.first, ss->str, ss->len);
	cac_assert(eq);

	eq = substr_isequal(&split.second, "ZZZ");
	cac_assert(!eq);


	substr_init(ss, "1.2.3.4.5");
	substr_rsplit_chr(ss, '.', &split);

	eq = substr_isequal(&split.first, "1.2.3.4");
	cac_assert(eq);

	eq = substr_isequal(&split.second, "5");
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_substr_trim(substr_t *ss)
{
	int eq;
	substr_t sub;

	substr_init(ss, ".:ABCD");

	substr_trim_any_of(ss, ":,.%^", &sub);
	eq  = substr_isequal(&sub, "ABCD");
	cac_assert(eq);

	substr_ntrim_any_of(ss,
	                    substr_data(ss),
	                    substr_size(ss),
	                    &sub);
	eq  = substr_size(&sub) == 0;
	cac_assert(eq);

	substr_trim_chr(ss, '.', &sub);
	eq  = substr_isequal(&sub, ":ABCD");
	cac_assert(eq);

	substr_trim(ss, 4, &sub);
	eq  = substr_isequal(&sub, "CD");
	cac_assert(eq);

	substr_trim_if(ss, chr_ispunct, &sub);
	eq  = substr_isequal(&sub, "ABCD");
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_substr_chop(substr_t *ss)
{
	int eq;
	substr_t sub;

	substr_init(ss, "123....");

	substr_chop_any_of(ss, "+*&^%$.", &sub);
	eq  = substr_isequal(&sub, "123");
	cac_assert(eq);

	substr_nchop_any_of(ss,
	                    substr_data(ss),
	                    substr_size(ss),
	                    &sub);
	eq  = substr_isequal(&sub, "");
	cac_assert(eq);

	substr_chop(ss, 6, &sub);
	eq  = substr_isequal(&sub, "1");
	cac_assert(eq);

	substr_chop_chr(ss, '.', &sub);
	eq  = substr_isequal(&sub, "123");
	cac_assert(eq);

	substr_chop_if(ss, chr_ispunct, &sub);
	eq  = substr_isequal(&sub, "123");
	cac_assert(eq);

	substr_chop_if(ss, chr_isprint, &sub);
	eq  = substr_size(&sub) == 0;
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_substr_strip(substr_t *ss)
{
	int eq;
	size_t sz;
	const char *s;
	const char *s2 = "s ";
	substr_t sub;

	substr_init(ss, ".....#XYZ#.........");

	substr_strip_any_of(ss, "-._#", &sub);
	eq  = substr_isequal(&sub, "XYZ");
	cac_assert(eq);

	substr_strip_chr(ss, '.', &sub);
	eq  = substr_isequal(&sub, "#XYZ#");
	cac_assert(eq);

	substr_strip_if(ss, chr_ispunct, &sub);
	eq  = substr_isequal(&sub, "XYZ");
	cac_assert(eq);

	s  = substr_data(ss);
	sz = substr_size(ss);
	substr_nstrip_any_of(ss, s, sz, &sub);
	eq  = substr_isequal(&sub, "");
	cac_assert(eq);

	substr_init(ss, " \t ABC\n\r\v");
	substr_strip_ws(ss, &sub);
	eq = substr_isequal(&sub, "ABC");
	cac_assert(eq);

	substr_init(ss, s2);
	substr_strip_if(ss, chr_isspace, &sub);
	eq  = substr_isequal(&sub, "s");
	cac_assert(eq);

	substr_init(ss, s2 + 1);
	substr_strip_if(ss, chr_isspace, &sub);
	eq  = substr_isequal(&sub, "");
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_substr_find_token(substr_t *ss)
{
	int eq;
	substr_t tok;
	const char *seps;

	seps   = " \t\n\v\r";
	substr_init(ss, " A BB \t  CCC    DDDD  \n");

	substr_find_token(ss, seps, &tok);
	eq  = substr_isequal(&tok, "A");
	cac_assert(eq);

	substr_find_next_token(ss, &tok, seps, &tok);
	eq  = substr_isequal(&tok, "BB");
	cac_assert(eq);

	substr_find_next_token(ss, &tok, seps, &tok);
	eq  = substr_isequal(&tok, "CCC");
	cac_assert(eq);

	substr_find_next_token(ss, &tok, seps, &tok);
	eq  = substr_isequal(&tok, "DDDD");
	cac_assert(eq);

	substr_find_next_token(ss, &tok, seps, &tok);
	eq  = substr_isequal(&tok, "");
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_substr_tokenize(substr_t *ss)
{
	int rc, eq;
	size_t n_tseq;
	substr_t tseq_list[7];
	const char *seps = " /:;.| " ;
	const char *line;

	line = "    /Ant:::Bee;:Cat:Dog;...Elephant.../Frog:/Giraffe///    ";
	substr_init(ss, line);

	rc = substr_tokenize(ss, seps, tseq_list, 7, &n_tseq);
	cac_assert(rc == 0);
	cac_assert(n_tseq == 7);

	eq  = substr_isequal(&tseq_list[0], "Ant");
	cac_assert(eq);

	eq  = substr_isequal(&tseq_list[4], "Elephant");
	cac_assert(eq);

	eq  = substr_isequal(&tseq_list[6], "Giraffe");
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_substr_common_prefix(substr_t *ss)
{
	int eq;
	size_t n;
	char buf1[] = "0123456789abcdef";

	substr_init(ss, buf1);

	n = substr_common_prefix(ss, "0123456789ABCDEF");
	eq = (n == 10);
	cac_assert(eq);

	n = substr_common_prefix(ss, buf1);
	eq = (n == 16);
	cac_assert(eq);

	n = substr_common_prefix(ss, "XYZ");
	eq = (n == 0);
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_substr_common_suffix(substr_t *ss)
{
	int eq;
	size_t n;
	char buf1[] = "abcdef0123456789";

	substr_init(ss, buf1);

	n = substr_common_suffix(ss, "ABCDEF0123456789");
	eq = (n == 10);
	cac_assert(eq);

	n = substr_common_suffix(ss, buf1);
	eq = (n == 16);
	cac_assert(eq);

	n = substr_common_suffix(ss, "XYZ");
	eq = (n == 0);
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_substr_copyto(substr_t *ss)
{
	int eq;
	char buf[10];
	char pad = '@';
	size_t sz;
	const char *s = "123456789";
	const char *x = "XXXXXXXXXXXX";

	substr_init(ss, s);
	sz = substr_size(ss);
	cac_assert(sz == 9);

	sz = substr_copyto(ss, buf, cac_nelems(buf));
	cac_assert(sz == 9);

	eq = (str_ncompare(buf, sz, s, sz) == 0);
	cac_assert(eq);
	cac_assert(pad == '@');

	sz = substr_copyto(ss, buf, 1);
	cac_assert(sz == 1);

	sz = substr_copyto(ss, buf, 0);
	cac_assert(sz == 0);

	substr_init(ss, x);

	sz = substr_copyto(ss, buf, 1);
	cac_assert(sz == 1);

	eq = (str_ncompare(buf, 1, x, 1) == 0);
	cac_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void cac_test_substr(void)
{
	substr_t substr;
	substr_t *ss = &substr;

	test_substr_isequal(ss);
	test_substr_compare(ss);
	test_substr_find(ss);
	test_substr_rfind(ss);
	test_substr_find_first_of(ss);
	test_substr_find_last_of(ss);
	test_substr_find_first_not_of(ss);
	test_substr_find_last_not_of(ss);
	test_substr_sub(ss);
	test_substr_count(ss);
	test_substr_split(ss);
	test_substr_rsplit(ss);
	test_substr_trim(ss);
	test_substr_chop(ss);
	test_substr_strip(ss);
	test_substr_find_token(ss);
	test_substr_tokenize(ss);
	test_substr_common_prefix(ss);
	test_substr_common_suffix(ss);
	test_substr_copyto(ss);

	substr_destroy(ss);
}
