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
#include <fnxinfra.h>

#define NELEMS(x)       FNX_NELEMS(x)


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_compare(fnx_wsubstr_t *ss)
{
	int cmp, eq, emp;
	size_t sz;

	fnx_wsubstr_init(ss, L"123456789");
	sz = fnx_wsubstr_size(ss);
	fnx_assert(sz == 9);

	emp = fnx_wsubstr_isempty(ss);
	fnx_assert(!emp);

	cmp = fnx_wsubstr_compare(ss, L"123");
	fnx_assert(cmp > 0);

	cmp = fnx_wsubstr_compare(ss, L"9");
	fnx_assert(cmp < 0);

	cmp = fnx_wsubstr_compare(ss, L"123456789");
	fnx_assert(cmp == 0);

	eq = fnx_wsubstr_isequal(ss, L"123456789");
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_find(fnx_wsubstr_t *ss)
{
	size_t n, pos, len;

	fnx_wsubstr_init(ss, L"ABCDEF abcdef ABCDEF");

	n = fnx_wsubstr_find(ss, L"A");
	fnx_assert(n == 0);

	n = fnx_wsubstr_find(ss, L"EF");
	fnx_assert(n == 4);

	pos = 10;
	len = 2;
	n = fnx_wsubstr_nfind(ss, pos, L"EF", len);
	fnx_assert(n == 18);

	pos = 1;
	n = fnx_wsubstr_find_chr(ss, pos, 'A');
	fnx_assert(n == 14);

	n = fnx_wsubstr_find(ss, L"UUU");
	fnx_assert(n > fnx_wsubstr_size(ss));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_rfind(fnx_wsubstr_t *ss)
{
	size_t n, pos, len;

	fnx_wsubstr_init(ss, L"ABRACADABRA");


	n = fnx_wsubstr_rfind(ss, L"A");
	fnx_assert(n == 10);

	n = fnx_wsubstr_rfind(ss, L"BR");
	fnx_assert(n == 8);

	pos = fnx_wsubstr_size(ss) / 2;
	len = 2;
	n = fnx_wsubstr_nrfind(ss, pos, L"BR", len);
	fnx_assert(n == 1);

	pos = 1;
	n = fnx_wsubstr_rfind_chr(ss, pos, 'B');
	fnx_assert(n == 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_find_first_of(fnx_wsubstr_t *ss)
{
	size_t n, pos;

	fnx_wsubstr_init(ss, L"012x456x89z");

	n = fnx_wsubstr_find_first_of(ss, L"xyz");
	fnx_assert(n == 3);

	pos = 5;
	n = fnx_wsubstr_nfind_first_of(ss, pos, L"x..z", 4);
	fnx_assert(n == 7);

	n = fnx_wsubstr_find_first_of(ss, L"XYZ");
	fnx_assert(n > fnx_wsubstr_size(ss));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_find_last_of(fnx_wsubstr_t *ss)
{
	size_t n, pos;

	fnx_wsubstr_init(ss, L"AAAAA-BBBBB");

	n = fnx_wsubstr_find_last_of(ss, L"xyzAzyx");
	fnx_assert(n == 4);

	pos = 9;
	n = fnx_wsubstr_nfind_last_of(ss, pos, L"X-Y", 3);
	fnx_assert(n == 5);

	n = fnx_wsubstr_find_last_of(ss, L"BBBBBBBBBBBBBBBBBBBBB");
	fnx_assert(n == (fnx_wsubstr_size(ss) - 1));

	n = fnx_wsubstr_find_last_of(ss, L"...");
	fnx_assert(n > fnx_wsubstr_size(ss));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_find_first_not_of(fnx_wsubstr_t *ss)
{
	size_t n, pos;

	fnx_wsubstr_init(ss, L"aaa bbb ccc * ddd + eee");

	n = fnx_wsubstr_find_first_not_of(ss, L"a b c d e");
	fnx_assert(n == 12);

	pos = 14;
	n = fnx_wsubstr_nfind_first_not_of(ss, pos, L"d e", 3);
	fnx_assert(n == 18);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_find_last_not_of(fnx_wsubstr_t *ss)
{
	size_t n, pos;

	fnx_wsubstr_init(ss, L"-..3456.--");

	n = fnx_wsubstr_find_last_not_of(ss, L".-");
	fnx_assert(n == 6);

	pos = 1;
	n = fnx_wsubstr_nfind_last_not_of(ss, pos, L"*", 1);
	fnx_assert(n == 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_sub(fnx_wsubstr_t *ss)
{
	int rc;
	fnx_wsubstr_t sub;
	const wchar_t *abc;

	abc = L"abcdefghijklmnopqrstuvwxyz";

	fnx_wsubstr_init_rd(ss, abc, 10);    /* L"abcdefghij" */
	fnx_wsubstr_sub(ss, 2, 4, &sub);
	rc  = fnx_wsubstr_isequal(&sub, L"cdef");
	fnx_assert(rc == 1);

	fnx_wsubstr_rsub(ss, 3, &sub);
	rc  = fnx_wsubstr_isequal(&sub, L"hij");
	fnx_assert(rc == 1);

	fnx_wsubstr_chop(ss, 8, &sub);
	rc  = fnx_wsubstr_isequal(&sub, L"ab");
	fnx_assert(rc == 1);

	fnx_wsubstr_clone(ss, &sub);
	rc  = fnx_wsubstr_nisequal(&sub, ss->str, ss->len);
	fnx_assert(rc == 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_count(fnx_wsubstr_t *ss)
{
	size_t n;

	fnx_wsubstr_init(ss, L"xxx-xxx-xxx-xxx");

	n = fnx_wsubstr_count(ss, L"xxx");
	fnx_assert(n == 4);

	n = fnx_wsubstr_count_chr(ss, '-');
	fnx_assert(n == 3);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_split(fnx_wsubstr_t *ss)
{
	int eq;
	fnx_wsubstr_pair_t split;

	fnx_wsubstr_init(ss, L"ABC-DEF+123");
	fnx_wsubstr_split(ss, L"-", &split);

	eq = fnx_wsubstr_isequal(&split.first, L"ABC");
	fnx_assert(eq);

	eq = fnx_wsubstr_isequal(&split.second, L"DEF+123");
	fnx_assert(eq);

	fnx_wsubstr_split(ss, L" + * L", &split);
	eq = fnx_wsubstr_isequal(&split.first, L"ABC-DEF");
	fnx_assert(eq);

	eq = fnx_wsubstr_isequal(&split.second, L"123");
	fnx_assert(eq);


	fnx_wsubstr_split_chr(ss, 'B', &split);
	eq = fnx_wsubstr_isequal(&split.first, L"A");
	fnx_assert(eq);

	eq = fnx_wsubstr_isequal(&split.second, L"C-DEF+123");
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_rsplit(fnx_wsubstr_t *ss)
{
	int eq;
	fnx_wsubstr_pair_t split;

	fnx_wsubstr_init(ss, L"UUU--YYY--ZZZ");
	fnx_wsubstr_rsplit(ss, L"-.", &split);

	eq = fnx_wsubstr_isequal(&split.first, L"UUU--YYY");
	fnx_assert(eq);

	eq = fnx_wsubstr_isequal(&split.second, L"ZZZ");
	fnx_assert(eq);


	fnx_wsubstr_rsplit(ss, L"+", &split);

	eq = fnx_wsubstr_nisequal(&split.first, ss->str, ss->len);
	fnx_assert(eq);

	eq = fnx_wsubstr_isequal(&split.second, L"ZZZ");
	fnx_assert(!eq);


	fnx_wsubstr_init(ss, L"1.2.3.4.5");
	fnx_wsubstr_rsplit_chr(ss, '.', &split);

	eq = fnx_wsubstr_isequal(&split.first, L"1.2.3.4");
	fnx_assert(eq);

	eq = fnx_wsubstr_isequal(&split.second, L"5");
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_trim(fnx_wsubstr_t *ss)
{
	int eq;
	fnx_wsubstr_t sub;

	fnx_wsubstr_init(ss, L".:ABCD");

	fnx_wsubstr_trim_any_of(ss, L":,.%^", &sub);
	eq  = fnx_wsubstr_isequal(&sub, L"ABCD");
	fnx_assert(eq);

	fnx_wsubstr_ntrim_any_of(ss,
	                         fnx_wsubstr_data(ss),
	                         fnx_wsubstr_size(ss),
	                         &sub);
	eq  = fnx_wsubstr_size(&sub) == 0;
	fnx_assert(eq);

	fnx_wsubstr_trim_chr(ss, '.', &sub);
	eq  = fnx_wsubstr_isequal(&sub, L":ABCD");
	fnx_assert(eq);

	fnx_wsubstr_trim(ss, 4, &sub);
	eq  = fnx_wsubstr_isequal(&sub, L"CD");
	fnx_assert(eq);

	fnx_wsubstr_trim_if(ss, fnx_wchr_ispunct, &sub);
	eq  = fnx_wsubstr_isequal(&sub, L"ABCD");
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_chop(fnx_wsubstr_t *ss)
{
	int eq;
	fnx_wsubstr_t sub;

	fnx_wsubstr_init(ss, L"123....");

	fnx_wsubstr_chop_any_of(ss, L"+*&^%$.", &sub);
	eq  = fnx_wsubstr_isequal(&sub, L"123");
	fnx_assert(eq);

	fnx_wsubstr_nchop_any_of(ss,
	                         fnx_wsubstr_data(ss),
	                         fnx_wsubstr_size(ss),
	                         &sub);
	eq  = fnx_wsubstr_isequal(&sub, L"");
	fnx_assert(eq);

	fnx_wsubstr_chop(ss, 6, &sub);
	eq  = fnx_wsubstr_isequal(&sub, L"1");
	fnx_assert(eq);

	fnx_wsubstr_chop_chr(ss, '.', &sub);
	eq  = fnx_wsubstr_isequal(&sub, L"123");
	fnx_assert(eq);

	fnx_wsubstr_chop_if(ss, fnx_wchr_ispunct, &sub);
	eq  = fnx_wsubstr_isequal(&sub, L"123");
	fnx_assert(eq);

	fnx_wsubstr_chop_if(ss, fnx_wchr_isprint, &sub);
	eq  = fnx_wsubstr_size(&sub) == 0;
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_strip(fnx_wsubstr_t *ss)
{
	int eq;
	size_t sz;
	const wchar_t *s;
	fnx_wsubstr_t sub;

	fnx_wsubstr_init(ss, L".....#XYZ#.........");

	fnx_wsubstr_strip_any_of(ss, L"-._#", &sub);
	eq  = fnx_wsubstr_isequal(&sub, L"XYZ");
	fnx_assert(eq);

	fnx_wsubstr_strip_chr(ss, '.', &sub);
	eq  = fnx_wsubstr_isequal(&sub, L"#XYZ#");
	fnx_assert(eq);

	fnx_wsubstr_strip_if(ss, fnx_wchr_ispunct, &sub);
	eq  = fnx_wsubstr_isequal(&sub, L"XYZ");
	fnx_assert(eq);

	s  = fnx_wsubstr_data(ss);
	sz = fnx_wsubstr_size(ss);
	fnx_wsubstr_nstrip_any_of(ss, s, sz, &sub);
	eq  = fnx_wsubstr_isequal(&sub, L"");
	fnx_assert(eq);

	fnx_wsubstr_init(ss, L" \t ABC\n\r\v");
	fnx_wsubstr_strip_ws(ss, &sub);
	eq = fnx_wsubstr_isequal(&sub, L"ABC");
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_find_token(fnx_wsubstr_t *ss)
{
	int eq;
	fnx_wsubstr_t tok;
	const wchar_t *seps;

	seps   = L" \t\n\v\r";
	fnx_wsubstr_init(ss, L" A BB \t  CCC    DDDD  \n");

	fnx_wsubstr_find_token(ss, seps, &tok);
	eq  = fnx_wsubstr_isequal(&tok, L"A");
	fnx_assert(eq);

	fnx_wsubstr_find_next_token(ss, &tok, seps, &tok);
	eq  = fnx_wsubstr_isequal(&tok, L"BB");
	fnx_assert(eq);

	fnx_wsubstr_find_next_token(ss, &tok, seps, &tok);
	eq  = fnx_wsubstr_isequal(&tok, L"CCC");
	fnx_assert(eq);

	fnx_wsubstr_find_next_token(ss, &tok, seps, &tok);
	eq  = fnx_wsubstr_isequal(&tok, L"DDDD");
	fnx_assert(eq);

	fnx_wsubstr_find_next_token(ss, &tok, seps, &tok);
	eq  = fnx_wsubstr_isequal(&tok, L"");
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_tokenize(fnx_wsubstr_t *ss)
{
	int rc, eq;
	size_t n_toks;
	fnx_wsubstr_t toks_list[7];
	const wchar_t *seps = L" /:;.| L" ;
	const wchar_t *line;

	line = L"    /Ant:::Bee;:Cat:Dog;...Elephant.../Frog:/Giraffe///    L";
	fnx_wsubstr_init(ss, line);

	rc = fnx_wsubstr_tokenize(ss, seps, toks_list, 7, &n_toks);
	fnx_assert(rc == 0);
	fnx_assert(n_toks == 7);

	eq  = fnx_wsubstr_isequal(&toks_list[0], L"Ant");
	fnx_assert(eq);

	eq  = fnx_wsubstr_isequal(&toks_list[4], L"Elephant");
	fnx_assert(eq);

	eq  = fnx_wsubstr_isequal(&toks_list[6], L"Giraffe");
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_case(fnx_wsubstr_t *ss)
{
	int eq;
	wchar_t buf[20] = L"0123456789abcdef";

	fnx_wsubstr_init_rwa(ss, buf);

	fnx_wsubstr_toupper(ss);
	eq  = fnx_wsubstr_isequal(ss, L"0123456789ABCDEF");
	fnx_assert(eq);

	fnx_wsubstr_tolower(ss);
	eq  = fnx_wsubstr_isequal(ss, L"0123456789abcdef");
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_common_prefix(fnx_wsubstr_t *ss)
{
	int eq, n;
	wchar_t buf1[] = L"0123456789abcdef";

	fnx_wsubstr_init(ss, buf1);

	n = (int) fnx_wsubstr_common_prefix(ss, L"0123456789ABCDEF");
	eq = (n == 10);
	fnx_assert(eq);

	n = (int) fnx_wsubstr_common_prefix(ss, buf1);
	eq = (n == 16);
	fnx_assert(eq);

	n = (int) fnx_wsubstr_common_prefix(ss, L"XYZ");
	eq = (n == 0);
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_common_suffix(fnx_wsubstr_t *ss)
{
	int eq, n;
	wchar_t buf1[] = L"abcdef0123456789";

	fnx_wsubstr_init(ss, buf1);

	n = (int) fnx_wsubstr_common_suffix(ss, L"ABCDEF0123456789");
	eq = (n == 10);
	fnx_assert(eq);

	n = (int) fnx_wsubstr_common_suffix(ss, buf1);
	eq = (n == 16);
	fnx_assert(eq);

	n = (int) fnx_wsubstr_common_suffix(ss, L"XYZ");
	eq = (n == 0);
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_assign(fnx_wsubstr_t *ss)
{
	int eq, n;
	fnx_wsubstr_t sub;
	wchar_t buf1[] = L"0123456789......";
	const wchar_t *s;

	fnx_wsubstr_init_rw(ss, buf1, 10, 16);
	fnx_wsubstr_sub(ss, 10, 6, &sub);

	n = (int)fnx_wsubstr_size(&sub);
	eq = (n == 0);
	fnx_assert(eq);

	n = (int)fnx_wsubstr_wrsize(&sub);
	eq = (n == 6);
	fnx_assert(eq);

	s = L"ABC";
	fnx_wsubstr_assign(ss, s);
	n = (int)fnx_wsubstr_size(ss);
	fnx_assert(n == 3);
	n = (int)fnx_wsubstr_wrsize(ss);
	fnx_assert(n == 16);

	eq = fnx_wsubstr_isequal(ss, s);
	fnx_assert(eq);

	s = L"ABCDEF";
	fnx_wsubstr_assign(&sub, s);
	eq = fnx_wsubstr_isequal(&sub, s);
	fnx_assert(eq);

	s = L"ABCDEF$$$";
	fnx_wsubstr_assign(&sub, s);
	n = (int)fnx_wsubstr_size(&sub);
	fnx_assert(n == 6);
	n = (int)fnx_wsubstr_wrsize(&sub);
	fnx_assert(n == 6);
	eq = fnx_wsubstr_isequal(&sub, s);
	fnx_assert(!eq);

	fnx_wsubstr_sub(&sub, 5, 100, &sub);
	s = L"XYZ";
	fnx_wsubstr_assign(&sub, s);
	n = (int)fnx_wsubstr_size(&sub);
	fnx_assert(n == 1);
	n = (int)fnx_wsubstr_wrsize(&sub);
	fnx_assert(n == 1);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_append(fnx_wsubstr_t *ss)
{
	int eq, n;
	wchar_t buf[20];
	const wchar_t *s;

	fnx_wsubstr_init_rw(ss, buf, 0, NELEMS(buf));
	s = L"0123456789abcdef";

	fnx_wsubstr_append(ss, s);
	n = (int)fnx_wsubstr_size(ss);
	fnx_assert(n == 16);
	n = (int)fnx_wsubstr_wrsize(ss);
	fnx_assert(n == NELEMS(buf));
	eq = fnx_wsubstr_isequal(ss, s);
	fnx_assert(eq);

	fnx_wsubstr_append(ss, s);
	n = (int)fnx_wsubstr_size(ss);
	fnx_assert(n == NELEMS(buf));
	n = (int)fnx_wsubstr_wrsize(ss);
	fnx_assert(n == NELEMS(buf));

	fnx_wsubstr_init_rw(ss, buf, 0, NELEMS(buf));
	s = L"01";
	fnx_wsubstr_nappend(ss, s, 1);
	n = (int)fnx_wsubstr_size(ss);
	fnx_assert(n == 1);
	fnx_wsubstr_nappend(ss, s + 1, 1);
	n = (int)fnx_wsubstr_size(ss);
	fnx_assert(n == 2);
	eq = fnx_wsubstr_isequal(ss, s);
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_insert(fnx_wsubstr_t *ss)
{
	int eq, n;
	const wchar_t *s;
	wchar_t buf[20];

	fnx_wsubstr_init_rw(ss, buf, 0, NELEMS(buf));

	s = L"0123456789";
	fnx_wsubstr_insert(ss, 0, s);
	n = (int)fnx_wsubstr_size(ss);
	fnx_assert(n == 10);
	eq = fnx_wsubstr_isequal(ss, s);
	fnx_assert(eq);

	fnx_wsubstr_insert(ss, 10, s);
	n = (int)fnx_wsubstr_size(ss);
	fnx_assert(n == 20);
	eq = fnx_wsubstr_isequal(ss, L"01234567890123456789");
	fnx_assert(eq);

	fnx_wsubstr_insert(ss, 1, L"....");
	n = (int)fnx_wsubstr_size(ss);
	fnx_assert(n == 20);
	eq = fnx_wsubstr_isequal(ss, L"0....123456789012345");
	fnx_assert(eq);

	fnx_wsubstr_insert(ss, 16, L"%%%");
	n = (int)fnx_wsubstr_size(ss);
	fnx_assert(n == 20);
	eq = fnx_wsubstr_isequal(ss, L"0....12345678901%%%2");
	fnx_assert(eq);

	fnx_wsubstr_insert_chr(ss, 1, 20, '$');
	n = (int)fnx_wsubstr_size(ss);
	fnx_assert(n == 20);
	eq = fnx_wsubstr_isequal(ss, L"0$$$$$$$$$$$$$$$$$$$");
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_replace(fnx_wsubstr_t *ss)
{
	int eq, n;
	const wchar_t *s;
	wchar_t buf[10];

	fnx_wsubstr_init_rw(ss, buf, 0, NELEMS(buf));

	s = L"ABCDEF";
	fnx_wsubstr_replace(ss, 0, 2, s);
	n = (int) fnx_wsubstr_size(ss);
	fnx_assert(n == 6);
	eq = fnx_wsubstr_isequal(ss, s);
	fnx_assert(eq);

	fnx_wsubstr_replace(ss, 1, 2, s);
	eq = fnx_wsubstr_isequal(ss, L"AABCDEFDEF");
	fnx_assert(eq);

	fnx_wsubstr_replace(ss, 6, 3, s);
	eq = fnx_wsubstr_isequal(ss, L"AABCDEABCD");
	fnx_assert(eq);

	fnx_wsubstr_replace_chr(ss, 0, 10, 30, '.');
	eq = fnx_wsubstr_isequal(ss, L"..........");
	fnx_assert(eq);

	fnx_wsubstr_replace_chr(ss, 1, 8, 4, 'A');
	eq = fnx_wsubstr_isequal(ss, L".AAAA.");
	fnx_assert(eq);

	fnx_wsubstr_nreplace(ss, 2, 80,
	                     fnx_wsubstr_data(ss),
	                     fnx_wsubstr_size(ss));
	eq = fnx_wsubstr_isequal(ss, L".A.AAAA.");
	fnx_assert(eq);

	fnx_wsubstr_nreplace(ss, 4, 80,
	                     fnx_wsubstr_data(ss),
	                     fnx_wsubstr_size(ss));
	eq = fnx_wsubstr_isequal(ss, L".A.A.A.AAA");  /* Truncated */
	fnx_assert(eq);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_erase(fnx_wsubstr_t *ss)
{
	int n, eq;
	wchar_t buf[5];

	fnx_wsubstr_init_rw(ss, buf, 0, NELEMS(buf));

	fnx_wsubstr_assign(ss, L"ABCDEF");
	eq = fnx_wsubstr_isequal(ss, L"ABCDE");
	fnx_assert(eq);

	fnx_wsubstr_erase(ss, 1, 2);
	eq = fnx_wsubstr_isequal(ss, L"ADE");
	fnx_assert(eq);

	fnx_wsubstr_erase(ss, 0, 100);
	eq = fnx_wsubstr_isequal(ss, L"");
	fnx_assert(eq);

	n = (int) fnx_wsubstr_wrsize(ss);
	fnx_assert(n == NELEMS(buf));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_reverse(fnx_wsubstr_t *ss)
{
	int eq;
	wchar_t buf[40];

	fnx_wsubstr_init_rw(ss, buf, 0, NELEMS(buf));
	fnx_wsubstr_assign(ss, L"abracadabra");
	fnx_wsubstr_reverse(ss);

	eq = fnx_wsubstr_isequal(ss, L"arbadacarba");
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_copyto(fnx_wsubstr_t *ss)
{
	int eq;
	wchar_t buf[10];
	wchar_t pad = '@';
	size_t sz;

	fnx_wsubstr_init(ss, L"123456789");
	sz = fnx_wsubstr_size(ss);
	fnx_assert(sz == 9);

	sz = fnx_wsubstr_copyto(ss, buf, sizeof(buf));
	fnx_assert(sz == 9);

	eq = !wcscmp(buf, L"123456789");
	fnx_assert(eq);
	fnx_assert(pad == '@');
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
int main(void)
{
	fnx_wsubstr_t wsubstr_obj;
	fnx_wsubstr_t *ss = &wsubstr_obj;

	test_compare(ss);
	test_find(ss);
	test_rfind(ss);
	test_find_first_of(ss);
	test_find_last_of(ss);
	test_find_first_not_of(ss);
	test_find_last_not_of(ss);
	test_sub(ss);
	test_count(ss);
	test_split(ss);
	test_rsplit(ss);
	test_trim(ss);
	test_chop(ss);
	test_strip(ss);
	test_find_token(ss);
	test_tokenize(ss);
	test_case(ss);
	test_common_prefix(ss);
	test_common_suffix(ss);

	test_assign(ss);
	test_append(ss);
	test_insert(ss);
	test_replace(ss);
	test_erase(ss);
	test_reverse(ss);
	test_copyto(ss);

	fnx_wsubstr_destroy(ss);

	return EXIT_SUCCESS;
}


