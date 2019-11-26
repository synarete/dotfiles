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
static void test_compare(fnx_substr_t *ss)
{
	int cmp, eq, emp;
	size_t sz;

	fnx_substr_init(ss, "123456789");
	sz = fnx_substr_size(ss);
	fnx_assert(sz == 9);

	emp = fnx_substr_isempty(ss);
	fnx_assert(!emp);

	cmp = fnx_substr_compare(ss, "123");
	fnx_assert(cmp > 0);

	cmp = fnx_substr_compare(ss, "9");
	fnx_assert(cmp < 0);

	cmp = fnx_substr_compare(ss, "123456789");
	fnx_assert(cmp == 0);

	eq = fnx_substr_isequal(ss, "123456789");
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_find(fnx_substr_t *ss)
{
	size_t n, pos, len;

	fnx_substr_init(ss, "ABCDEF abcdef ABCDEF");

	n = fnx_substr_find(ss, "A");
	fnx_assert(n == 0);

	n = fnx_substr_find(ss, "EF");
	fnx_assert(n == 4);

	pos = 10;
	len = 2;
	n = fnx_substr_nfind(ss, pos, "EF", len);
	fnx_assert(n == 18);

	pos = 1;
	n = fnx_substr_find_chr(ss, pos, 'A');
	fnx_assert(n == 14);

	n = fnx_substr_find(ss, "UUU");
	fnx_assert(n > fnx_substr_size(ss));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_rfind(fnx_substr_t *ss)
{
	size_t n, pos, len;

	fnx_substr_init(ss, "ABRACADABRA");

	n = fnx_substr_rfind(ss, "A");
	fnx_assert(n == 10);

	n = fnx_substr_rfind(ss, "BR");
	fnx_assert(n == 8);

	pos = fnx_substr_size(ss) / 2;
	len = 2;
	n = fnx_substr_nrfind(ss, pos, "BR", len);
	fnx_assert(n == 1);

	pos = 1;
	n = fnx_substr_rfind_chr(ss, pos, 'B');
	fnx_assert(n == 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_find_first_of(fnx_substr_t *ss)
{
	size_t n, pos;

	fnx_substr_init(ss, "012x456x89z");

	n = fnx_substr_find_first_of(ss, "xyz");
	fnx_assert(n == 3);

	pos = 5;
	n = fnx_substr_nfind_first_of(ss, pos, "x..z", 4);
	fnx_assert(n == 7);

	n = fnx_substr_find_first_of(ss, "XYZ");
	fnx_assert(n > fnx_substr_size(ss));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_find_last_of(fnx_substr_t *ss)
{
	size_t n, pos;

	fnx_substr_init(ss, "AAAAA-BBBBB");

	n = fnx_substr_find_last_of(ss, "xyzAzyx");
	fnx_assert(n == 4);

	pos = 9;
	n = fnx_substr_nfind_last_of(ss, pos, "X-Y", 3);
	fnx_assert(n == 5);

	n = fnx_substr_find_last_of(ss, "BBBBBBBBBBBBBBBBBBBBB");
	fnx_assert(n == (fnx_substr_size(ss) - 1));

	n = fnx_substr_find_last_of(ss, "...");
	fnx_assert(n > fnx_substr_size(ss));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_find_first_not_of(fnx_substr_t *ss)
{
	size_t n, pos;

	fnx_substr_init(ss, "aaa bbb ccc * ddd + eee");

	n = fnx_substr_find_first_not_of(ss, "a b c d e");
	fnx_assert(n == 12);

	pos = 14;
	n = fnx_substr_nfind_first_not_of(ss, pos, "d e", 3);
	fnx_assert(n == 18);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_find_last_not_of(fnx_substr_t *ss)
{
	size_t n, pos;

	fnx_substr_init(ss, "-..3456.--");

	n = fnx_substr_find_last_not_of(ss, ".-");
	fnx_assert(n == 6);

	pos = 1;
	n = fnx_substr_nfind_last_not_of(ss, pos, "*", 1);
	fnx_assert(n == 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_sub(fnx_substr_t *ss)
{
	int rc;
	fnx_substr_t sub;
	const char *abc;

	abc = "abcdefghijklmnopqrstuvwxyz";

	fnx_substr_init_rd(ss, abc, 10);    /* "abcdefghij" */
	fnx_substr_sub(ss, 2, 4, &sub);
	rc  = fnx_substr_isequal(&sub, "cdef");
	fnx_assert(rc == 1);

	fnx_substr_rsub(ss, 3, &sub);
	rc  = fnx_substr_isequal(&sub, "hij");
	fnx_assert(rc == 1);

	fnx_substr_chop(ss, 8, &sub);
	rc  = fnx_substr_isequal(&sub, "ab");
	fnx_assert(rc == 1);

	fnx_substr_clone(ss, &sub);
	rc  = fnx_substr_nisequal(&sub, ss->str, ss->len);
	fnx_assert(rc == 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_count(fnx_substr_t *ss)
{
	size_t n;

	fnx_substr_init(ss, "xxx-xxx-xxx-xxx");

	n = fnx_substr_count(ss, "xxx");
	fnx_assert(n == 4);

	n = fnx_substr_count_chr(ss, '-');
	fnx_assert(n == 3);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_split(fnx_substr_t *ss)
{
	int eq;
	fnx_substr_pair_t split;

	fnx_substr_init(ss, "ABC-DEF+123");
	fnx_substr_split(ss, "-", &split);

	eq = fnx_substr_isequal(&split.first, "ABC");
	fnx_assert(eq);

	eq = fnx_substr_isequal(&split.second, "DEF+123");
	fnx_assert(eq);

	fnx_substr_split(ss, " + * ", &split);
	eq = fnx_substr_isequal(&split.first, "ABC-DEF");
	fnx_assert(eq);

	eq = fnx_substr_isequal(&split.second, "123");
	fnx_assert(eq);


	fnx_substr_split_chr(ss, 'B', &split);
	eq = fnx_substr_isequal(&split.first, "A");
	fnx_assert(eq);

	eq = fnx_substr_isequal(&split.second, "C-DEF+123");
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_rsplit(fnx_substr_t *ss)
{
	int eq;
	fnx_substr_pair_t split;

	fnx_substr_init(ss, "UUU--YYY--ZZZ");
	fnx_substr_rsplit(ss, "-.", &split);

	eq = fnx_substr_isequal(&split.first, "UUU--YYY");
	fnx_assert(eq);

	eq = fnx_substr_isequal(&split.second, "ZZZ");
	fnx_assert(eq);


	fnx_substr_rsplit(ss, "+", &split);

	eq = fnx_substr_nisequal(&split.first, ss->str, ss->len);
	fnx_assert(eq);

	eq = fnx_substr_isequal(&split.second, "ZZZ");
	fnx_assert(!eq);


	fnx_substr_init(ss, "1.2.3.4.5");
	fnx_substr_rsplit_chr(ss, '.', &split);

	eq = fnx_substr_isequal(&split.first, "1.2.3.4");
	fnx_assert(eq);

	eq = fnx_substr_isequal(&split.second, "5");
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_trim(fnx_substr_t *ss)
{
	int eq;
	fnx_substr_t sub;

	fnx_substr_init(ss, ".:ABCD");

	fnx_substr_trim_any_of(ss, ":,.%^", &sub);
	eq  = fnx_substr_isequal(&sub, "ABCD");
	fnx_assert(eq);

	fnx_substr_ntrim_any_of(ss,
	                        fnx_substr_data(ss),
	                        fnx_substr_size(ss),
	                        &sub);
	eq  = fnx_substr_size(&sub) == 0;
	fnx_assert(eq);

	fnx_substr_trim_chr(ss, '.', &sub);
	eq  = fnx_substr_isequal(&sub, ":ABCD");
	fnx_assert(eq);

	fnx_substr_trim(ss, 4, &sub);
	eq  = fnx_substr_isequal(&sub, "CD");
	fnx_assert(eq);

	fnx_substr_trim_if(ss, fnx_chr_ispunct, &sub);
	eq  = fnx_substr_isequal(&sub, "ABCD");
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_chop(fnx_substr_t *ss)
{
	int eq;
	fnx_substr_t sub;

	fnx_substr_init(ss, "123....");

	fnx_substr_chop_any_of(ss, "+*&^%$.", &sub);
	eq  = fnx_substr_isequal(&sub, "123");
	fnx_assert(eq);

	fnx_substr_nchop_any_of(ss,
	                        fnx_substr_data(ss),
	                        fnx_substr_size(ss),
	                        &sub);
	eq  = fnx_substr_isequal(&sub, "");
	fnx_assert(eq);

	fnx_substr_chop(ss, 6, &sub);
	eq  = fnx_substr_isequal(&sub, "1");
	fnx_assert(eq);

	fnx_substr_chop_chr(ss, '.', &sub);
	eq  = fnx_substr_isequal(&sub, "123");
	fnx_assert(eq);

	fnx_substr_chop_if(ss, fnx_chr_ispunct, &sub);
	eq  = fnx_substr_isequal(&sub, "123");
	fnx_assert(eq);

	fnx_substr_chop_if(ss, fnx_chr_isprint, &sub);
	eq  = fnx_substr_size(&sub) == 0;
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_strip(fnx_substr_t *ss)
{
	int eq;
	size_t sz;
	const char *s;
	const char *s2 = "s ";
	fnx_substr_t sub;

	fnx_substr_init(ss, ".....#XYZ#.........");

	fnx_substr_strip_any_of(ss, "-._#", &sub);
	eq  = fnx_substr_isequal(&sub, "XYZ");
	fnx_assert(eq);

	fnx_substr_strip_chr(ss, '.', &sub);
	eq  = fnx_substr_isequal(&sub, "#XYZ#");
	fnx_assert(eq);

	fnx_substr_strip_if(ss, fnx_chr_ispunct, &sub);
	eq  = fnx_substr_isequal(&sub, "XYZ");
	fnx_assert(eq);

	s  = fnx_substr_data(ss);
	sz = fnx_substr_size(ss);
	fnx_substr_nstrip_any_of(ss, s, sz, &sub);
	eq  = fnx_substr_isequal(&sub, "");
	fnx_assert(eq);

	fnx_substr_init(ss, " \t ABC\n\r\v");
	fnx_substr_strip_ws(ss, &sub);
	eq = fnx_substr_isequal(&sub, "ABC");
	fnx_assert(eq);

	fnx_substr_init(ss, s2);
	fnx_substr_strip_if(ss, fnx_chr_isspace, &sub);
	eq  = fnx_substr_isequal(&sub, "s");
	fnx_assert(eq);

	fnx_substr_init(ss, s2 + 1);
	fnx_substr_strip_if(ss, fnx_chr_isspace, &sub);
	eq  = fnx_substr_isequal(&sub, "");
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_find_token(fnx_substr_t *ss)
{
	int eq;
	fnx_substr_t tok;
	const char *seps;

	seps   = " \t\n\v\r";
	fnx_substr_init(ss, " A BB \t  CCC    DDDD  \n");

	fnx_substr_find_token(ss, seps, &tok);
	eq  = fnx_substr_isequal(&tok, "A");
	fnx_assert(eq);

	fnx_substr_find_next_token(ss, &tok, seps, &tok);
	eq  = fnx_substr_isequal(&tok, "BB");
	fnx_assert(eq);

	fnx_substr_find_next_token(ss, &tok, seps, &tok);
	eq  = fnx_substr_isequal(&tok, "CCC");
	fnx_assert(eq);

	fnx_substr_find_next_token(ss, &tok, seps, &tok);
	eq  = fnx_substr_isequal(&tok, "DDDD");
	fnx_assert(eq);

	fnx_substr_find_next_token(ss, &tok, seps, &tok);
	eq  = fnx_substr_isequal(&tok, "");
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_tokenize(fnx_substr_t *ss)
{
	int rc, eq;
	size_t n_toks;
	fnx_substr_t toks_list[7];
	const char *seps = " /:;.| " ;
	const char *line;

	line = "    /Ant:::Bee;:Cat:Dog;...Elephant.../Frog:/Giraffe///    ";
	fnx_substr_init(ss, line);

	rc = fnx_substr_tokenize(ss, seps, toks_list, 7, &n_toks);
	fnx_assert(rc == 0);
	fnx_assert(n_toks == 7);

	eq  = fnx_substr_isequal(&toks_list[0], "Ant");
	fnx_assert(eq);

	eq  = fnx_substr_isequal(&toks_list[4], "Elephant");
	fnx_assert(eq);

	eq  = fnx_substr_isequal(&toks_list[6], "Giraffe");
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_case(fnx_substr_t *ss)
{
	int eq;
	char buf[20] = "0123456789abcdef";

	fnx_substr_init_rwa(ss, buf);

	fnx_substr_toupper(ss);
	eq  = fnx_substr_isequal(ss, "0123456789ABCDEF");
	fnx_assert(eq);

	fnx_substr_tolower(ss);
	eq  = fnx_substr_isequal(ss, "0123456789abcdef");
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_common_prefix(fnx_substr_t *ss)
{
	int eq, n;
	char buf1[] = "0123456789abcdef";

	fnx_substr_init(ss, buf1);

	n = (int)fnx_substr_common_prefix(ss, "0123456789ABCDEF");
	eq = (n == 10);
	fnx_assert(eq);

	n = (int)fnx_substr_common_prefix(ss, buf1);
	eq = (n == 16);
	fnx_assert(eq);

	n = (int)fnx_substr_common_prefix(ss, "XYZ");
	eq = (n == 0);
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_common_suffix(fnx_substr_t *ss)
{
	int eq, n;
	char buf1[] = "abcdef0123456789";

	fnx_substr_init(ss, buf1);

	n = (int)fnx_substr_common_suffix(ss, "ABCDEF0123456789");
	eq = (n == 10);
	fnx_assert(eq);

	n = (int)fnx_substr_common_suffix(ss, buf1);
	eq = (n == 16);
	fnx_assert(eq);

	n = (int)fnx_substr_common_suffix(ss, "XYZ");
	eq = (n == 0);
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_assign(fnx_substr_t *ss)
{
	int eq, n;
	fnx_substr_t sub;
	char buf1[] = "0123456789......";
	const char *s;

	fnx_substr_init_rw(ss, buf1, 10, 16);
	fnx_substr_sub(ss, 10, 6, &sub);

	n = (int)fnx_substr_size(&sub);
	eq = (n == 0);
	fnx_assert(eq);

	n = (int)fnx_substr_wrsize(&sub);
	eq = (n == 6);
	fnx_assert(eq);

	s = "ABC";
	fnx_substr_assign(ss, s);
	n = (int)fnx_substr_size(ss);
	fnx_assert(n == 3);
	n = (int)fnx_substr_wrsize(ss);
	fnx_assert(n == 16);

	eq = fnx_substr_isequal(ss, s);
	fnx_assert(eq);

	s = "ABCDEF";
	fnx_substr_assign(&sub, s);
	eq = fnx_substr_isequal(&sub, s);
	fnx_assert(eq);

	s = "ABCDEF$$$";
	fnx_substr_assign(&sub, s);
	n = (int)fnx_substr_size(&sub);
	fnx_assert(n == 6);
	n = (int)fnx_substr_wrsize(&sub);
	fnx_assert(n == 6);
	eq = fnx_substr_isequal(&sub, s);
	fnx_assert(!eq);

	fnx_substr_sub(&sub, 5, 100, &sub);
	s = "XYZ";
	fnx_substr_assign(&sub, s);
	n = (int)fnx_substr_size(&sub);
	fnx_assert(n == 1);
	n = (int)fnx_substr_wrsize(&sub);
	fnx_assert(n == 1);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_append(fnx_substr_t *ss)
{
	int eq, n;
	char buf[20];
	const char *s;

	fnx_substr_init_rw(ss, buf, 0, NELEMS(buf));
	s = "0123456789abcdef";

	fnx_substr_append(ss, s);
	n = (int)fnx_substr_size(ss);
	fnx_assert(n == 16);
	n = (int)fnx_substr_wrsize(ss);
	fnx_assert(n == NELEMS(buf));
	eq = fnx_substr_isequal(ss, s);
	fnx_assert(eq);

	fnx_substr_append(ss, s);
	n = (int)fnx_substr_size(ss);
	fnx_assert(n == NELEMS(buf));
	n = (int)fnx_substr_wrsize(ss);
	fnx_assert(n == NELEMS(buf));

	fnx_substr_init_rw(ss, buf, 0, NELEMS(buf));
	s = "01";
	fnx_substr_nappend(ss, s, 1);
	n = (int)fnx_substr_size(ss);
	fnx_assert(n == 1);
	fnx_substr_nappend(ss, s + 1, 1);
	n = (int)fnx_substr_size(ss);
	fnx_assert(n == 2);
	eq = fnx_substr_isequal(ss, s);
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_insert(fnx_substr_t *ss)
{
	int eq, n;
	const char *s;
	char buf[20];

	fnx_substr_init_rw(ss, buf, 0, NELEMS(buf));

	s = "0123456789";
	fnx_substr_insert(ss, 0, s);
	n = (int)fnx_substr_size(ss);
	fnx_assert(n == 10);
	eq = fnx_substr_isequal(ss, s);
	fnx_assert(eq);

	fnx_substr_insert(ss, 10, s);
	n = (int)fnx_substr_size(ss);
	fnx_assert(n == 20);
	eq = fnx_substr_isequal(ss, "01234567890123456789");
	fnx_assert(eq);

	fnx_substr_insert(ss, 1, "....");
	n = (int)fnx_substr_size(ss);
	fnx_assert(n == 20);
	eq = fnx_substr_isequal(ss, "0....123456789012345");
	fnx_assert(eq);

	fnx_substr_insert(ss, 16, "%%%");
	n = (int)fnx_substr_size(ss);
	fnx_assert(n == 20);
	eq = fnx_substr_isequal(ss, "0....12345678901%%%2");
	fnx_assert(eq);

	fnx_substr_insert_chr(ss, 1, 20, '$');
	n = (int)fnx_substr_size(ss);
	fnx_assert(n == 20);
	eq = fnx_substr_isequal(ss, "0$$$$$$$$$$$$$$$$$$$");
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_replace(fnx_substr_t *ss)
{
	int eq, n;
	const char *s;
	char buf[10];

	fnx_substr_init_rw(ss, buf, 0, NELEMS(buf));

	s = "ABCDEF";
	fnx_substr_replace(ss, 0, 2, s);
	n = (int) fnx_substr_size(ss);
	fnx_assert(n == 6);
	eq = fnx_substr_isequal(ss, s);
	fnx_assert(eq);

	fnx_substr_replace(ss, 1, 2, s);
	eq = fnx_substr_isequal(ss, "AABCDEFDEF");
	fnx_assert(eq);

	fnx_substr_replace(ss, 6, 3, s);
	eq = fnx_substr_isequal(ss, "AABCDEABCD");
	fnx_assert(eq);

	fnx_substr_replace_chr(ss, 0, 10, 30, '.');
	eq = fnx_substr_isequal(ss, "..........");
	fnx_assert(eq);

	fnx_substr_replace_chr(ss, 1, 8, 4, 'A');
	eq = fnx_substr_isequal(ss, ".AAAA.");
	fnx_assert(eq);

	fnx_substr_nreplace(ss, 2, 80,
	                    fnx_substr_data(ss),
	                    fnx_substr_size(ss));
	eq = fnx_substr_isequal(ss, ".A.AAAA.");
	fnx_assert(eq);

	fnx_substr_nreplace(ss, 4, 80,
	                    fnx_substr_data(ss),
	                    fnx_substr_size(ss));
	eq = fnx_substr_isequal(ss, ".A.A.A.AAA");  /* Truncated */
	fnx_assert(eq);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_erase(fnx_substr_t *ss)
{
	int n, eq;
	char buf[5];

	fnx_substr_init_rw(ss, buf, 0, NELEMS(buf));

	fnx_substr_assign(ss, "ABCDEF");
	eq = fnx_substr_isequal(ss, "ABCDE");
	fnx_assert(eq);

	fnx_substr_erase(ss, 1, 2);
	eq = fnx_substr_isequal(ss, "ADE");
	fnx_assert(eq);

	fnx_substr_erase(ss, 0, 100);
	eq = fnx_substr_isequal(ss, "");
	fnx_assert(eq);

	n = (int) fnx_substr_wrsize(ss);
	fnx_assert(n == NELEMS(buf));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_reverse(fnx_substr_t *ss)
{
	int eq;
	char buf[40];

	fnx_substr_init_rw(ss, buf, 0, NELEMS(buf));
	fnx_substr_assign(ss, "abracadabra");
	fnx_substr_reverse(ss);

	eq = fnx_substr_isequal(ss, "arbadacarba");
	fnx_assert(eq);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_copyto(fnx_substr_t *ss)
{
	int eq;
	char buf[10];
	char pad = '@';
	size_t sz;

	fnx_substr_init(ss, "123456789");
	sz = fnx_substr_size(ss);
	fnx_assert(sz == 9);

	sz = fnx_substr_copyto(ss, buf, sizeof(buf));
	fnx_assert(sz == 9);

	eq = !strcmp(buf, "123456789");
	fnx_assert(eq);
	fnx_assert(pad == '@');
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
int main(void)
{
	fnx_substr_t substr_obj;
	fnx_substr_t *ss = &substr_obj;

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

	fnx_substr_destroy(ss);

	return EXIT_SUCCESS;
}
