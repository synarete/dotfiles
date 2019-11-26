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
#include <string.h>

#include "infra-compiler.h"
#include "infra-macros.h"
#include "infra-utility.h"
#include "infra-panic.h"
#include "infra-wstring.h"
#include "infra-wsubstr.h"


#define wsubstr_out_of_range(ss, pos, sz)               \
	fnx_panic("out-of-range pos=%ld sz=%ld ss=%s",      \
	          (long)pos, (long)sz, ((const char*)ss->str))


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t wsubstr_max_size(void)
{
	return (size_t)(-1);
}
size_t fnx_wsubstr_max_size(void)
{
	return wsubstr_max_size();
}

static size_t wsubstr_npos(void)
{
	return wsubstr_max_size();
}
size_t fnx_wsubstr_npos(void)
{
	return wsubstr_npos();
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Immutable String Operations:
 */

/* Returns the offset of p within substr */
static size_t wsubstr_offset(const fnx_wsubstr_t *ss, const wchar_t *p)
{
	size_t off;

	off = wsubstr_npos();
	if (p != NULL) {
		if ((p >= ss->str) && (p < (ss->str + ss->len))) {
			off = (size_t)(p - ss->str);
		}
	}
	return off;
}

void fnx_wsubstr_init(fnx_wsubstr_t *ss, const wchar_t *s)
{
	fnx_wsubstr_init_rd(ss, s, fnx_wstr_length(s));
}

void fnx_wsubstr_init_rd(fnx_wsubstr_t *ss, const wchar_t *s, size_t n)
{
	fnx_wsubstr_init_rw(ss, (wchar_t *)s, n, 0UL);
}

void fnx_wsubstr_init_rwa(fnx_wsubstr_t *ss, wchar_t *s)
{
	size_t len;

	len = fnx_wstr_length(s);
	fnx_wsubstr_init_rw(ss, s, len, len);
}

void fnx_wsubstr_init_rw(fnx_wsubstr_t *ss,
                         wchar_t *s, size_t nrd, size_t nwr)
{
	ss->str  = s;
	ss->len  = nrd;
	ss->nwr  = nwr;
}

void fnx_wsubstr_inits(fnx_wsubstr_t *ss)
{
	static const wchar_t *es = L"";
	fnx_wsubstr_init(ss, es);
}

void fnx_wsubstr_clone(const fnx_wsubstr_t *ss, fnx_wsubstr_t *other)
{
	other->str = ss->str;
	other->len = ss->len;
	other->nwr = ss->nwr;
}

void fnx_wsubstr_destroy(fnx_wsubstr_t *ss)
{
	ss->str  = NULL;
	ss->len  = 0;
	ss->nwr  = 0;
}


static const wchar_t *wsubstr_data(const fnx_wsubstr_t *ss)
{
	return ss->str;
}

static wchar_t *wsubstr_mutable_data(const fnx_wsubstr_t *ss)
{
	return (wchar_t *)(ss->str);
}

static size_t wsubstr_size(const fnx_wsubstr_t *ss)
{
	return ss->len;
}

size_t fnx_wsubstr_size(const fnx_wsubstr_t *ss)
{
	return wsubstr_size(ss);
}

static size_t wsubstr_wrsize(const fnx_wsubstr_t *ss)
{
	return ss->nwr;
}

size_t fnx_wsubstr_wrsize(const fnx_wsubstr_t *ss)
{
	return wsubstr_wrsize(ss);
}

static int wsubstr_isempty(const fnx_wsubstr_t *ss)
{
	return (wsubstr_size(ss) == 0);
}

int fnx_wsubstr_isempty(const fnx_wsubstr_t *ss)
{
	return wsubstr_isempty(ss);
}

static const wchar_t *wsubstr_begin(const fnx_wsubstr_t *ss)
{
	return wsubstr_data(ss);
}

const wchar_t *fnx_wsubstr_begin(const fnx_wsubstr_t *ss)
{
	return wsubstr_begin(ss);
}

static const wchar_t *wsubstr_end(const fnx_wsubstr_t *ss)
{
	return (wsubstr_data(ss) + wsubstr_size(ss));
}

const wchar_t *fnx_wsubstr_end(const fnx_wsubstr_t *ss)
{
	return wsubstr_end(ss);
}

size_t fnx_wsubstr_offset(const fnx_wsubstr_t *ss, const wchar_t *p)
{
	return wsubstr_offset(ss, p);
}

const wchar_t *fnx_wsubstr_at(const fnx_wsubstr_t *ss, size_t n)
{
	size_t sz;

	sz = wsubstr_size(ss);
	if (!(n < sz)) {
		wsubstr_out_of_range(ss, n, sz);
	}
	return wsubstr_data(ss) + n;
}

int fnx_wsubstr_isvalid_index(const fnx_wsubstr_t *ss, size_t i)
{
	return (i < wsubstr_size(ss));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_wsubstr_copyto(const fnx_wsubstr_t *ss, wchar_t *buf, size_t n)
{
	size_t n1;

	n1 = fnx_min(n, ss->len);
	fnx_wstr_copy(buf, ss->str, n1);

	if (n1 < n) { /* If possible, terminate with EOS. */
		fnx_wstr_terminate(buf, n1);
	}

	return n1;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_wsubstr_compare(const fnx_wsubstr_t *ss, const wchar_t *s)
{
	return fnx_wsubstr_ncompare(ss, s, fnx_wstr_length(s));
}

int fnx_wsubstr_ncompare(const fnx_wsubstr_t *ss, const wchar_t *s, size_t n)
{
	int res = 0;

	if ((ss->str != s) || (ss->len != n)) {
		res = fnx_wstr_ncompare(ss->str, ss->len, s, n);
	}
	return res;
}

int fnx_wsubstr_isequal(const fnx_wsubstr_t *ss, const wchar_t *s)
{
	return fnx_wsubstr_nisequal(ss, s, fnx_wstr_length(s));
}

int fnx_wsubstr_nisequal(const fnx_wsubstr_t *ss, const wchar_t *s, size_t n)
{
	int res;
	const wchar_t *str;

	res = 0;
	if (wsubstr_size(ss) == n) {
		str = wsubstr_data(ss);
		res = ((str == s) || (fnx_wstr_compare(str, s, n) == 0));
	}
	return res;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_wsubstr_count(const fnx_wsubstr_t *ss, const wchar_t *s)
{
	return fnx_wsubstr_ncount(ss, s, fnx_wstr_length(s));
}

size_t fnx_wsubstr_ncount(const fnx_wsubstr_t *ss, const wchar_t *s, size_t n)
{
	size_t i, sz, pos, cnt;

	sz  = wsubstr_size(ss);
	pos = 0;
	cnt = 0;
	i   = fnx_wsubstr_nfind(ss, pos, s, n);
	while (i < sz) {
		++cnt;

		pos = i + n;
		i   = fnx_wsubstr_nfind(ss, pos, s, n);
	}

	return cnt;
}

size_t fnx_wsubstr_count_chr(const fnx_wsubstr_t *ss, wchar_t c)
{
	size_t i, sz, pos, cnt;

	sz  = wsubstr_size(ss);
	pos = 0;
	cnt = 0;
	i   = fnx_wsubstr_find_chr(ss, pos, c);
	while (i < sz) {
		++cnt;

		pos = i + 1;
		i   = fnx_wsubstr_find_chr(ss, pos, c);
	}

	return cnt;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_wsubstr_find(const fnx_wsubstr_t *ss, const wchar_t *s)
{
	return fnx_wsubstr_nfind(ss, 0UL, s, fnx_wstr_length(s));
}

size_t fnx_wsubstr_nfind(const fnx_wsubstr_t *ss,
                         size_t pos, const wchar_t *s, size_t n)
{
	size_t sz;
	const wchar_t *p = NULL;
	const wchar_t *dat;

	dat = wsubstr_data(ss);
	sz  = wsubstr_size(ss);

	if (pos < sz) {
		if (n > 1) {
			p = fnx_wstr_find(dat + pos, sz - pos, s, n);
		} else if (n == 1) {
			p = fnx_wstr_find_chr(dat + pos, sz - pos, s[0]);
		} else { /* n == 0 */
			/*
			 * Stay compatible with STL: empty string always matches (if inside
			 * string).
			 */
			p = dat + pos;
		}
	}
	return wsubstr_offset(ss, p);
}

size_t fnx_wsubstr_find_chr(const fnx_wsubstr_t *ss, size_t pos, wchar_t c)
{
	size_t sz;
	const wchar_t *p = NULL;
	const wchar_t *dat;

	dat = wsubstr_data(ss);
	sz  = wsubstr_size(ss);

	if (pos < sz) {
		p = fnx_wstr_find_chr(dat + pos, sz - pos, c);
	}
	return wsubstr_offset(ss, p);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_wsubstr_rfind(const fnx_wsubstr_t *ss, const wchar_t *s)
{
	size_t pos;

	pos = wsubstr_size(ss);
	return fnx_wsubstr_nrfind(ss, pos, s, fnx_wstr_length(s));
}

size_t fnx_wsubstr_nrfind(const fnx_wsubstr_t *ss,
                          size_t pos, const wchar_t *s, size_t n)
{
	size_t k, sz;
	const wchar_t *p = NULL;
	const wchar_t *q;
	const wchar_t *dat;

	q   = s;
	dat = wsubstr_data(ss);
	sz  = wsubstr_size(ss);
	k   = (pos < sz) ? pos + 1 : sz;

	if (n == 0) {
		/* STL compatible: empty string always matches */
		p = dat + k;
	} else if (n == 1) {
		p = fnx_wstr_rfind_chr(dat, k, *q);
	} else {
		p = fnx_wstr_rfind(dat, k, q, n);
	}

	return wsubstr_offset(ss, p);
}

size_t fnx_wsubstr_rfind_chr(const fnx_wsubstr_t *ss, size_t pos, wchar_t c)
{
	size_t sz, k;
	const wchar_t *p;
	const wchar_t *dat;

	dat = wsubstr_data(ss);
	sz  = wsubstr_size(ss);
	k   = (pos < sz) ? pos + 1 : sz;

	p = fnx_wstr_rfind_chr(dat, k, c);
	return wsubstr_offset(ss, p);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_wsubstr_find_first_of(const fnx_wsubstr_t *ss, const wchar_t *s)
{
	return fnx_wsubstr_nfind_first_of(ss, 0UL, s, fnx_wstr_length(s));
}

size_t fnx_wsubstr_nfind_first_of(const fnx_wsubstr_t *ss,
                                  size_t pos, const wchar_t *s, size_t n)
{
	size_t sz;
	const wchar_t *p = NULL;
	const wchar_t *q;
	const wchar_t *dat;

	q   = s;
	dat = wsubstr_data(ss);
	sz  = wsubstr_size(ss);

	if ((n != 0) && (pos < sz)) {
		if (n == 1) {
			p = fnx_wstr_find_chr(dat + pos, sz - pos, *q);
		} else {
			p = fnx_wstr_find_first_of(dat + pos, sz - pos, q, n);
		}
	}

	return wsubstr_offset(ss, p);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_wsubstr_find_last_of(const fnx_wsubstr_t *ss, const wchar_t *s)
{
	size_t pos;

	pos = wsubstr_size(ss);
	return fnx_wsubstr_nfind_last_of(ss, pos, s, fnx_wstr_length(s));
}

size_t fnx_wsubstr_nfind_last_of(const fnx_wsubstr_t *ss, size_t pos,
                                 const wchar_t *s, size_t n)
{
	size_t sz;
	const wchar_t *q, *dat, *p = NULL;

	q   = s;
	dat = wsubstr_data(ss);
	sz  = wsubstr_size(ss);
	if (n != 0) {
		const size_t k = (pos < sz) ? pos + 1 : sz;

		if (n == 1) {
			p = fnx_wstr_rfind_chr(dat, k, *q);
		} else {
			p = fnx_wstr_find_last_of(dat, k, q, n);
		}
	}
	return wsubstr_offset(ss, p);
}

size_t fnx_wsubstr_find_first_not_of(const fnx_wsubstr_t *ss,
                                     const wchar_t *s)
{
	return fnx_wsubstr_nfind_first_not_of(ss, 0UL, s, fnx_wstr_length(s));
}

size_t fnx_wsubstr_nfind_first_not_of(const fnx_wsubstr_t *ss,
                                      size_t pos, const wchar_t *s, size_t n)
{
	size_t sz;
	const wchar_t *q, *dat, *p = NULL;

	q   = s;
	dat = wsubstr_data(ss);
	sz  = wsubstr_size(ss);

	if (pos < sz) {
		if (n == 0) {
			p = dat + pos;
		} else if (n == 1) {
			p = fnx_wstr_find_first_not_eq(dat + pos, sz - pos, *q);
		} else {
			p = fnx_wstr_find_first_not_of(dat + pos, sz - pos, q, n);
		}
	}

	return wsubstr_offset(ss, p);
}

size_t fnx_wsubstr_find_first_not(const fnx_wsubstr_t *ss,
                                  size_t pos, wchar_t c)
{
	size_t sz;
	const wchar_t *dat, *p = NULL;

	dat = wsubstr_data(ss);
	sz  = wsubstr_size(ss);
	if (pos < sz) {
		p = fnx_wstr_find_first_not_eq(dat + pos, sz - pos, c);
	}
	return wsubstr_offset(ss, p);
}

size_t fnx_wsubstr_find_last_not_of(const fnx_wsubstr_t *ss, const wchar_t *s)
{
	size_t pos;

	pos = wsubstr_size(ss);
	return fnx_wsubstr_nfind_last_not_of(ss, pos, s, fnx_wstr_length(s));
}

size_t fnx_wsubstr_nfind_last_not_of(const fnx_wsubstr_t *ss,
                                     size_t pos, const wchar_t *s, size_t n)
{
	size_t k, sz;
	const wchar_t *q, *dat, *p = NULL;

	q   = s;
	dat = wsubstr_data(ss);
	sz  = wsubstr_size(ss);

	if (sz != 0) {
		k = (pos < sz) ? pos + 1 : sz;
		if (n == 0) {
			/* Stay compatible with STL. */
			p = dat + k - 1;
		} else if (n == 1) {
			p = fnx_wstr_find_last_not_eq(dat, k, *q);
		} else {
			p = fnx_wstr_find_last_not_of(dat, k, q, n);
		}
	}

	return wsubstr_offset(ss, p);
}

size_t fnx_wsubstr_find_last_not(const fnx_wsubstr_t *ss,
                                 size_t pos, wchar_t c)
{
	size_t k, sz;
	const wchar_t *p, *dat;

	dat = wsubstr_data(ss);
	sz  = wsubstr_size(ss);
	k   = (pos < sz) ? pos + 1 : sz;
	p   = fnx_wstr_find_last_not_eq(dat, k, c);

	return wsubstr_offset(ss, p);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_wsubstr_sub(const fnx_wsubstr_t *ss,
                     size_t i, size_t n, fnx_wsubstr_t *result)
{
	size_t j, k, n1, n2, sz, wr;
	wchar_t *dat;

	sz  = wsubstr_size(ss);
	j   = fnx_min(i, sz);
	n1  = fnx_min(n, sz - j);

	wr  = wsubstr_wrsize(ss);
	k   = fnx_min(i, wr);
	n2  = fnx_min(n, wr - k);
	dat = wsubstr_mutable_data(ss);

	fnx_wsubstr_init_rw(result, dat + j, n1, n2);
}

void fnx_wsubstr_rsub(const fnx_wsubstr_t *ss,
                      size_t n, fnx_wsubstr_t *result)
{
	size_t j, k, n1, n2, sz, wr;
	wchar_t *dat;

	sz  = wsubstr_size(ss);
	n1  = fnx_min(n, sz);
	j   = sz - n1;

	wr  = wsubstr_wrsize(ss);
	k   = fnx_min(j, wr);
	n2  = wr - k;
	dat = wsubstr_mutable_data(ss);

	fnx_wsubstr_init_rw(result, dat + j, n1, n2);
}

void fnx_wsubstr_intersection(const fnx_wsubstr_t *s1,
                              const fnx_wsubstr_t *s2,
                              fnx_wsubstr_t *result)
{
	size_t i, n;
	const wchar_t *s1_begin;
	const wchar_t *s1_end;
	const wchar_t *s2_begin;
	const wchar_t *s2_end;

	s1_begin = wsubstr_begin(s1);
	s2_begin = wsubstr_begin(s2);
	if (s1_begin <= s2_begin) {
		i = n = 0;

		s1_end = wsubstr_end(s1);
		s2_end = wsubstr_end(s2);

		/* Case 1:  [.s1...)  [..s2.....) -- Returns empty substring. */
		if (s1_end <= s2_begin) {
			i = wsubstr_size(s2);
		}
		/* Case 2: [.s1........)
		                [.s2..) */
		else if (s2_end <= s1_end) {
			n = wsubstr_size(s2);
		}
		/* Case 3: [.s1.....)
		               [.s2......) */
		else {
			n = (size_t)(s1_end - s2_begin);
		}
		fnx_wsubstr_sub(s2, i, n, result);
	} else {
		/* One step recursion -- its ok */
		fnx_wsubstr_intersection(s2, s1, result);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Helper function to create split-of-substrings */
static void wsubstr_make_split_pair(const fnx_wsubstr_t *ss,
                                    size_t i1, size_t n1,
                                    size_t i2, size_t n2,
                                    fnx_wsubstr_pair_t *result)
{
	fnx_wsubstr_sub(ss, i1, n1, &result->first);
	fnx_wsubstr_sub(ss, i2, n2, &result->second);
}

void fnx_wsubstr_split(const fnx_wsubstr_t *ss,
                       const wchar_t *seps, fnx_wsubstr_pair_t *result)
{

	fnx_wsubstr_nsplit(ss, seps, fnx_wstr_length(seps), result);
}

void fnx_wsubstr_nsplit(const fnx_wsubstr_t *ss,
                        const wchar_t *seps, size_t n,
                        fnx_wsubstr_pair_t *result)
{
	size_t i, j, sz;

	sz = wsubstr_size(ss);
	i  = fnx_wsubstr_nfind_first_of(ss, 0UL, seps, n);
	j  = sz;
	if (i < sz) {
		j = fnx_wsubstr_nfind_first_not_of(ss, i, seps, n);
	}

	wsubstr_make_split_pair(ss, 0UL, i, j, sz, result);
}

void fnx_wsubstr_split_chr(const fnx_wsubstr_t *ss, wchar_t sep,
                           fnx_wsubstr_pair_t *result)
{
	size_t i, j, sz;

	sz  = wsubstr_size(ss);
	i   = fnx_wsubstr_find_chr(ss, 0UL, sep);
	j   = (i < sz) ? i + 1 : sz;

	wsubstr_make_split_pair(ss, 0UL, i, j, sz, result);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_wsubstr_rsplit(const fnx_wsubstr_t *ss,
                        const wchar_t *seps, fnx_wsubstr_pair_t *result)
{

	fnx_wsubstr_nrsplit(ss, seps, fnx_wstr_length(seps), result);
}

void fnx_wsubstr_nrsplit(const fnx_wsubstr_t *ss,
                         const wchar_t *seps, size_t n,
                         fnx_wsubstr_pair_t *result)
{
	size_t i, j, sz;

	sz = wsubstr_size(ss);
	i  = fnx_wsubstr_nfind_last_of(ss, sz, seps, n);
	j  = sz;
	if (i < sz) {
		j = fnx_wsubstr_nfind_last_not_of(ss, i, seps, n);

		if (j < sz) {
			++i;
			++j;
		} else {
			i = j = sz;
		}
	}
	wsubstr_make_split_pair(ss, 0UL, j, i, sz, result);
}

void fnx_wsubstr_rsplit_chr(const fnx_wsubstr_t *ss, wchar_t sep,
                            fnx_wsubstr_pair_t *result)
{
	size_t i, j, sz;

	sz  = wsubstr_size(ss);
	i   = fnx_wsubstr_rfind_chr(ss, sz, sep);
	j   = (i < sz) ? i + 1 : sz;

	wsubstr_make_split_pair(ss, 0UL, i, j, sz, result);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_wsubstr_trim(const fnx_wsubstr_t *substr, size_t n,
                      fnx_wsubstr_t *result)
{
	size_t sz;

	sz = wsubstr_size(substr);
	fnx_wsubstr_sub(substr, n, sz, result);
}

void fnx_wsubstr_trim_any_of(const fnx_wsubstr_t *ss,
                             const wchar_t *set, fnx_wsubstr_t *result)
{
	fnx_wsubstr_ntrim_any_of(ss, set, fnx_wstr_length(set), result);
}

void fnx_wsubstr_ntrim_any_of(const fnx_wsubstr_t *ss,
                              const wchar_t *set, size_t n,
                              fnx_wsubstr_t *result)
{
	size_t i, sz;

	sz = wsubstr_size(ss);
	i  = fnx_wsubstr_nfind_first_not_of(ss, 0UL, set, n);

	fnx_wsubstr_sub(ss, i, sz, result);
}

void fnx_wsubstr_trim_chr(const fnx_wsubstr_t *ss, wchar_t c,
                          fnx_wsubstr_t *result)
{
	size_t i, sz;

	sz = wsubstr_size(ss);
	i  = fnx_wsubstr_find_first_not(ss, 0UL, c);

	fnx_wsubstr_sub(ss, i, sz, result);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_wsubstr_chop(const fnx_wsubstr_t *ss,
                      size_t n, fnx_wsubstr_t *result)
{
	size_t k, sz, wr;
	wchar_t *dat;

	sz  = wsubstr_size(ss);
	wr  = wsubstr_wrsize(ss);
	k   = fnx_min(sz, n);
	dat = wsubstr_mutable_data(ss);

	fnx_wsubstr_init_rw(result, dat, sz - k, wr);
}

void fnx_wsubstr_chop_any_of(const fnx_wsubstr_t *ss,
                             const wchar_t *set, fnx_wsubstr_t *result)
{
	fnx_wsubstr_nchop_any_of(ss, set, fnx_wstr_length(set), result);
}

void fnx_wsubstr_nchop_any_of(const fnx_wsubstr_t *ss,
                              const wchar_t *set, size_t n,
                              fnx_wsubstr_t *result)
{
	size_t j, sz;

	sz = wsubstr_size(ss);
	j  = fnx_wsubstr_nfind_last_not_of(ss, sz, set, n);

	fnx_wsubstr_sub(ss, 0UL, ((j < sz) ? j + 1 : 0), result);
}

void fnx_wsubstr_chop_chr(const fnx_wsubstr_t *substr, wchar_t c,
                          fnx_wsubstr_t *result)
{
	size_t j, sz;

	sz = wsubstr_size(substr);
	j  = fnx_wsubstr_find_last_not(substr, sz, c);

	fnx_wsubstr_sub(substr, 0UL, ((j < sz) ? j + 1 : 0), result);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_wsubstr_strip_any_of(const fnx_wsubstr_t *ss,
                              const wchar_t *set, fnx_wsubstr_t *result)
{
	fnx_wsubstr_nstrip_any_of(ss, set, fnx_wstr_length(set), result);
}

void fnx_wsubstr_nstrip_any_of(const fnx_wsubstr_t *ss,
                               const wchar_t *set, size_t n,
                               fnx_wsubstr_t *result)
{
	fnx_wsubstr_t sub;

	fnx_wsubstr_ntrim_any_of(ss, set, n, &sub);
	fnx_wsubstr_nchop_any_of(&sub, set, n, result);
}

void fnx_wsubstr_strip_chr(const fnx_wsubstr_t *ss, wchar_t c,
                           fnx_wsubstr_t *result)
{
	fnx_wsubstr_t sub;

	fnx_wsubstr_trim_chr(ss, c, &sub);
	fnx_wsubstr_chop_chr(&sub, c, result);
}

void fnx_wsubstr_strip_ws(const fnx_wsubstr_t *ss, fnx_wsubstr_t *result)
{
	const wchar_t *spaces = L" \n\t\r\v\f";
	fnx_wsubstr_strip_any_of(ss, spaces, result);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_wsubstr_find_token(const fnx_wsubstr_t *ss,
                            const wchar_t *seps, fnx_wsubstr_t *result)
{
	fnx_wsubstr_nfind_token(ss, seps, fnx_wstr_length(seps), result);
}

void fnx_wsubstr_nfind_token(const fnx_wsubstr_t *ss,
                             const wchar_t *seps, size_t n,
                             fnx_wsubstr_t *result)
{
	size_t i, j, sz;

	sz = wsubstr_size(ss);
	i  = fnx_min(fnx_wsubstr_nfind_first_not_of(ss, 0UL, seps, n), sz);
	j  = fnx_min(fnx_wsubstr_nfind_first_of(ss, i, seps, n), sz);

	fnx_wsubstr_sub(ss, i, j - i, result);
}

void fnx_wsubstr_find_token_chr(const fnx_wsubstr_t *ss, wchar_t sep,
                                fnx_wsubstr_t *result)
{
	size_t i, j, sz;

	sz = wsubstr_size(ss);
	i  = fnx_min(fnx_wsubstr_find_first_not(ss, 0UL, sep), sz);
	j  = fnx_min(fnx_wsubstr_find_chr(ss, i, sep), sz);

	fnx_wsubstr_sub(ss, i, j - i, result);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_wsubstr_find_next_token(const fnx_wsubstr_t *ss,
                                 const fnx_wsubstr_t *tok,
                                 const wchar_t *seps,
                                 fnx_wsubstr_t *result)
{
	fnx_wsubstr_nfind_next_token(ss, tok, seps, fnx_wstr_length(seps), result);
}

void fnx_wsubstr_nfind_next_token(const fnx_wsubstr_t *ss,
                                  const fnx_wsubstr_t *tok,
                                  const wchar_t *seps, size_t n,
                                  fnx_wsubstr_t *result)
{
	size_t i, sz;
	const wchar_t *p;
	fnx_wsubstr_t sub;

	sz  = wsubstr_size(ss);
	p   = wsubstr_end(tok);
	i   = wsubstr_offset(ss, p);

	fnx_wsubstr_sub(ss, i, sz, &sub);
	fnx_wsubstr_nfind_token(&sub, seps, n, result);
}

void fnx_wsubstr_find_next_token_chr(const fnx_wsubstr_t *ss,
                                     const fnx_wsubstr_t *tok, wchar_t sep,
                                     fnx_wsubstr_t *result)
{
	size_t i, sz;
	const wchar_t *p;
	fnx_wsubstr_t sub;

	sz  = wsubstr_size(ss);
	p   = wsubstr_end(tok);
	i   = wsubstr_offset(ss, p);

	fnx_wsubstr_sub(ss, i, sz, &sub);
	fnx_wsubstr_find_token_chr(&sub, sep, result);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_wsubstr_tokenize(const fnx_wsubstr_t *ss,
                         const wchar_t *seps,
                         fnx_wsubstr_t tok_list[],
                         size_t list_size, size_t *p_ntok)
{
	return fnx_wsubstr_ntokenize(ss, seps, fnx_wstr_length(seps),
	                             tok_list, list_size, p_ntok);
}

int fnx_wsubstr_ntokenize(const fnx_wsubstr_t *ss,
                          const wchar_t *seps, size_t n,
                          fnx_wsubstr_t tok_list[],
                          size_t list_size, size_t *p_ntok)
{
	size_t ntok = 0;
	fnx_wsubstr_t tok_obj;
	fnx_wsubstr_t *tok = &tok_obj;
	fnx_wsubstr_t *tgt = NULL;

	fnx_wsubstr_nfind_token(ss, seps, n, tok);
	while (!fnx_wsubstr_isempty(tok)) {
		if (ntok == list_size) {
			return -1; /* Insufficient room */
		}
		tgt = &tok_list[ntok++];
		fnx_wsubstr_clone(tok, tgt);

		fnx_wsubstr_nfind_next_token(ss, tok, seps, n, tok);
	}

	if (p_ntok != NULL) {
		*p_ntok = ntok;
	}
	return 0;
}

int fnx_wsubstr_tokenize_chr(const fnx_wsubstr_t *ss, wchar_t sep,
                             fnx_wsubstr_t tok_list[],
                             size_t list_size, size_t *p_ntok)
{
	size_t ntok = 0;
	fnx_wsubstr_t tok_obj;
	fnx_wsubstr_t *tok = &tok_obj;
	fnx_wsubstr_t *tgt = NULL;

	fnx_wsubstr_find_token_chr(ss, sep, tok);
	while (!fnx_wsubstr_isempty(tok)) {
		if (ntok == list_size) {
			return -1; /* Insufficient room */
		}
		tgt = &tok_list[ntok++];
		fnx_wsubstr_clone(tok, tgt);

		fnx_wsubstr_find_next_token_chr(ss, tok, sep, tok);
	}

	if (p_ntok != NULL) {
		*p_ntok = ntok;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_wsubstr_common_prefix(const fnx_wsubstr_t *ss, const wchar_t *s)
{
	return fnx_wsubstr_ncommon_prefix(ss, s, fnx_wstr_length(s));
}

size_t fnx_wsubstr_ncommon_prefix(const fnx_wsubstr_t *ss,
                                  const wchar_t *s, size_t n)
{
	size_t sz;
	const wchar_t *p;

	p  = wsubstr_data(ss);
	sz = wsubstr_size(ss);
	return fnx_wstr_common_prefix(p, s, fnx_min(n, sz));
}

int fnx_wsubstr_starts_with(const fnx_wsubstr_t *ss, wchar_t c)
{
	const wchar_t s[] = { c };
	return (fnx_wsubstr_ncommon_prefix(ss, s, 1) == 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_wsubstr_common_suffix(const fnx_wsubstr_t *ss, const wchar_t *s)
{
	return fnx_wsubstr_ncommon_suffix(ss, s, fnx_wstr_length(s));
}

size_t fnx_wsubstr_ncommon_suffix(const fnx_wsubstr_t *ss,
                                  const wchar_t *s, size_t n)
{
	size_t sz, k;
	const wchar_t *p;

	p  = wsubstr_data(ss);
	sz = wsubstr_size(ss);
	if (n > sz) {
		k = fnx_wstr_common_suffix(p, s + (n - sz), sz);
	} else {
		k = fnx_wstr_common_suffix(p + (sz - n), s, n);
	}
	return k;
}

int fnx_wsubstr_ends_with(const fnx_wsubstr_t *ss, wchar_t c)
{
	const wchar_t s[] = { c };
	return (fnx_wsubstr_ncommon_suffix(ss, s, 1) == 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Mutable String Opeartions:
 */
wchar_t *fnx_wsubstr_data(const fnx_wsubstr_t *ss)
{
	return wsubstr_mutable_data(ss);
}

/* Set EOS characters at the end of characters array (if possible) */
static void wsubstr_terminate(fnx_wsubstr_t *ss)
{
	wchar_t *dat;
	size_t sz, wr;

	dat = wsubstr_mutable_data(ss);
	sz  = wsubstr_size(ss);
	wr  = wsubstr_wrsize(ss);

	if (sz < wr) {
		fnx_wstr_terminate(dat, sz);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Inserts a copy of s before position pos. */
static void wsubstr_insert(fnx_wsubstr_t *ss,
                           size_t pos, const wchar_t *s, size_t n)
{
	size_t sz, wr, j, k, rem, m;
	wchar_t *dat;

	sz  = wsubstr_size(ss);
	wr  = wsubstr_wrsize(ss);
	dat = fnx_wsubstr_data(ss);

	/* Start insertion before position j. */
	j   = fnx_min(pos, sz);

	/* Number of writable elements after j. */
	rem = (j < wr) ? (wr - j) : 0;

	/* Number of elements of substr after j (to be moved fwd). */
	k   = sz - j;

	/* Insert n elements of p: try as many as possible, truncate tail in
	    case of insufficient buffer capacity. */
	m = fnx_wstr_insert(dat + j, rem, k, s, n);
	ss->len = j + m;

	wsubstr_terminate(ss);
}

/* Inserts n copies of c before position pos. */
static void wsubstr_insert_fill(fnx_wsubstr_t *ss,
                                size_t pos, size_t n, wchar_t c)
{
	size_t sz, wr, j, k, rem, m;
	wchar_t *dat;

	sz  = wsubstr_size(ss);
	wr  = wsubstr_wrsize(ss);
	dat = fnx_wsubstr_data(ss);

	/* Start insertion before position j. */
	j   = fnx_min(pos, sz);

	/* Number of writable elements after j. */
	rem = (j < wr) ? (wr - j) : 0;

	/* Number of elements of substr after j (to be moved fwd). */
	k   = sz - j;

	/* Insert n copies of c: try as many as possible; truncate tail in
	    case of insufficient buffer capacity. */
	m = fnx_wstr_insert_chr(dat + j, rem, k, n, c);
	ss->len = j + m;

	wsubstr_terminate(ss);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Replaces a substring of *this with a copy of s. */
static void wsubstr_replace(fnx_wsubstr_t *ss, size_t pos, size_t n1,
                            const wchar_t *s, size_t n)
{
	size_t sz, wr, k, m;
	wchar_t *dat;

	sz  = wsubstr_size(ss);
	wr  = wsubstr_wrsize(ss);
	dat = wsubstr_mutable_data(ss);

	/* Number of elements to replace (assuming pos <= size). */
	k = fnx_min(sz - pos, n1);

	/* Replace k elements after pos with s; truncate tail in case of
	    insufficient buffer capacity. */
	m = fnx_wstr_replace(dat + pos, wr - pos, sz - pos, k, s, n);
	ss->len = pos + m;

	wsubstr_terminate(ss);
}

/* Replaces a substring of *this with n2 copies of c. */
static void wsubstr_replace_fill(fnx_wsubstr_t *ss,
                                 size_t pos, size_t n1, size_t n2, wchar_t c)
{
	size_t sz, wr, k, m;
	wchar_t *dat;

	sz  = wsubstr_size(ss);
	wr  = wsubstr_wrsize(ss);
	dat = wsubstr_mutable_data(ss);

	/* Number of elements to replace (assuming pos <= size). */
	k = fnx_min(sz - pos, n1);

	/* Replace k elements after pos with n2 copies of c; truncate tail in
	    case of insufficient buffer capacity. */
	m = fnx_wstr_replace_chr(dat + pos, wr - pos, sz - pos, k, n2, c);
	ss->len = pos + m;

	wsubstr_terminate(ss);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_wsubstr_assign(fnx_wsubstr_t *ss, const wchar_t *s)
{
	fnx_wsubstr_nassign(ss, s, fnx_wstr_length(s));
}

void fnx_wsubstr_nassign(fnx_wsubstr_t *ss, const wchar_t *s, size_t len)
{
	size_t sz;

	sz = wsubstr_size(ss);
	fnx_wsubstr_nreplace(ss, 0, sz, s, len);
}

void fnx_wsubstr_assign_chr(fnx_wsubstr_t *ss, size_t n, wchar_t c)
{
	size_t sz;

	sz = wsubstr_size(ss);
	fnx_wsubstr_replace_chr(ss, 0, sz, n, c);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_wsubstr_push_back(fnx_wsubstr_t *ss, wchar_t c)
{
	fnx_wsubstr_append_chr(ss, 1, c);
}

void fnx_wsubstr_append(fnx_wsubstr_t *ss, const wchar_t *s)
{
	fnx_wsubstr_nappend(ss, s, fnx_wstr_length(s));
}

void fnx_wsubstr_nappend(fnx_wsubstr_t *ss, const wchar_t *s, size_t len)
{
	size_t sz;

	sz = wsubstr_size(ss);
	fnx_wsubstr_ninsert(ss, sz, s, len);
}

void fnx_wsubstr_append_chr(fnx_wsubstr_t *ss, size_t n, wchar_t c)
{
	size_t sz;

	sz = wsubstr_size(ss);
	fnx_wsubstr_insert_chr(ss, sz, n, c);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_wsubstr_insert(fnx_wsubstr_t *ss, size_t pos, const wchar_t *s)
{
	fnx_wsubstr_ninsert(ss, pos, s, fnx_wstr_length(s));
}

void fnx_wsubstr_ninsert(fnx_wsubstr_t *ss, size_t pos,
                         const wchar_t *s, size_t len)
{
	size_t sz;

	sz = wsubstr_size(ss);
	if (pos <= sz) {
		wsubstr_insert(ss, pos, s, len);
	} else {
		wsubstr_out_of_range(ss, pos, sz);
	}
}

void fnx_wsubstr_insert_chr(fnx_wsubstr_t *ss, size_t pos, size_t n, wchar_t c)
{
	size_t sz;

	sz = wsubstr_size(ss);
	if (pos <= sz) {
		wsubstr_insert_fill(ss, pos, n, c);
	} else {
		wsubstr_out_of_range(ss, pos, sz);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_wsubstr_replace(fnx_wsubstr_t *ss,
                         size_t pos, size_t n, const wchar_t *s)
{
	fnx_wsubstr_nreplace(ss, pos, n, s, fnx_wstr_length(s));
}

void fnx_wsubstr_nreplace(fnx_wsubstr_t *ss,
                          size_t pos, size_t n,  const wchar_t *s, size_t len)
{
	size_t sz;

	sz = wsubstr_size(ss);
	if (pos < sz) {
		wsubstr_replace(ss, pos, n, s, len);
	} else if (pos == sz) {
		wsubstr_insert(ss, pos, s, len);
	} else {
		wsubstr_out_of_range(ss, pos, sz);
	}
}

void fnx_wsubstr_replace_chr(fnx_wsubstr_t *ss,
                             size_t pos, size_t n1, size_t n2, wchar_t c)
{
	size_t sz;

	sz = wsubstr_size(ss);
	if (pos < sz) {
		wsubstr_replace_fill(ss, pos, n1, n2, c);
	} else if (pos == sz) {
		wsubstr_insert_fill(ss, pos, n2, c);
	} else {
		wsubstr_out_of_range(ss, pos, sz);
	}
}

void fnx_wsubstr_erase(fnx_wsubstr_t *ss, size_t pos, size_t n)
{
	wchar_t c = '\0';
	fnx_wsubstr_replace_chr(ss, pos, n, 0, c);
}

void fnx_wsubstr_reverse(fnx_wsubstr_t *ss)
{
	size_t n, sz, wr;
	wchar_t *dat;

	sz  = wsubstr_size(ss);
	wr  = wsubstr_wrsize(ss);
	n   = fnx_min(sz, wr);
	dat = fnx_wsubstr_data(ss);

	fnx_wstr_reverse(dat, n);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Generic Operations:
 */
static size_t wsubstr_find_if(const fnx_wsubstr_t *ss, fnx_wchr_pred fn, int u)
{
	int v;
	const wchar_t *s, *t, *p = NULL;

	s = wsubstr_begin(ss);
	t = wsubstr_end(ss);
	while (s < t) {
		v = fn(*s) ? 1 : 0;
		if (v == u) {
			p = s;
			break;
		}
		++s;
	}
	return wsubstr_offset(ss, p);
}

size_t fnx_wsubstr_find_if(const fnx_wsubstr_t *ss, fnx_wchr_pred fn)
{
	return wsubstr_find_if(ss, fn, 1);
}

size_t fnx_wsubstr_find_if_not(const fnx_wsubstr_t *ss, fnx_wchr_pred fn)
{
	return wsubstr_find_if(ss, fn, 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t
wsubstr_rfind_if(const fnx_wsubstr_t *ss, fnx_wchr_pred fn, int u)
{
	int v;
	const wchar_t *s, *t, *p = NULL;

	s = wsubstr_end(ss);
	t = wsubstr_begin(ss);
	while (s-- > t) {
		v = fn(*s) ? 1 : 0;
		if (v == u) {
			p = s;
			break;
		}
	}
	return wsubstr_offset(ss, p);
}

size_t fnx_wsubstr_rfind_if(const fnx_wsubstr_t *ss, fnx_wchr_pred fn)
{
	return wsubstr_rfind_if(ss, fn, 1);
}

size_t fnx_wsubstr_rfind_if_not(const fnx_wsubstr_t *ss, fnx_wchr_pred fn)
{
	return wsubstr_rfind_if(ss, fn, 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_wsubstr_count_if(const fnx_wsubstr_t *ss, fnx_wchr_pred fn)
{
	size_t cnt;
	const wchar_t *s, *t;

	cnt = 0;
	s = wsubstr_begin(ss);
	t = wsubstr_end(ss);
	while (s < t) {
		if (fn(*s++)) {
			++cnt;
		}
	}
	return cnt;
}


int fnx_wsubstr_test_if(const fnx_wsubstr_t *substr, fnx_wchr_pred fn)
{
	const wchar_t *s, *t;

	s = wsubstr_begin(substr);
	t = wsubstr_end(substr);
	while (s < t) {
		if (!fn(*s++)) {
			return 0;
		}
	}
	return 1;
}

void fnx_wsubstr_trim_if(const fnx_wsubstr_t *ss,
                         fnx_wchr_pred fn, fnx_wsubstr_t   *result)
{
	size_t i, sz;

	sz = wsubstr_size(ss);
	i  = fnx_wsubstr_find_if_not(ss, fn);

	fnx_wsubstr_sub(ss, i, sz, result);
}

void fnx_wsubstr_chop_if(const fnx_wsubstr_t *ss,
                         fnx_wchr_pred fn, fnx_wsubstr_t *result)
{
	size_t j, sz;

	sz = wsubstr_size(ss);
	j  = fnx_wsubstr_rfind_if_not(ss, fn);

	fnx_wsubstr_sub(ss, 0UL, ((j < sz) ? j + 1 : 0), result);
}

void fnx_wsubstr_strip_if(const fnx_wsubstr_t *ss,
                          fnx_wchr_pred fn, fnx_wsubstr_t *result)
{
	fnx_wsubstr_t sub;

	fnx_wsubstr_trim_if(ss, fn, &sub);
	fnx_wsubstr_chop_if(&sub, fn, result);
}

void fnx_wsubstr_foreach(fnx_wsubstr_t *ss, fnx_wchr_modify_fn fn)
{
	size_t i, sz;
	wchar_t *p;

	sz = wsubstr_wrsize(ss);
	p  = wsubstr_mutable_data(ss);
	for (i = 0; i < sz; ++i) {
		fn(p++);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Ctype Operations:
 */
int fnx_wsubstr_isalnum(const fnx_wsubstr_t *ss)
{
	return fnx_wsubstr_test_if(ss, fnx_wchr_isalnum);
}

int fnx_wsubstr_isalpha(const fnx_wsubstr_t *ss)
{
	return fnx_wsubstr_test_if(ss, fnx_wchr_isalpha);
}

int fnx_wsubstr_isblank(const fnx_wsubstr_t *ss)
{
	return fnx_wsubstr_test_if(ss, fnx_wchr_isblank);
}

int fnx_wsubstr_iscntrl(const fnx_wsubstr_t *ss)
{
	return fnx_wsubstr_test_if(ss, fnx_wchr_iscntrl);
}

int fnx_wsubstr_isdigit(const fnx_wsubstr_t *ss)
{
	return fnx_wsubstr_test_if(ss, fnx_wchr_isdigit);
}

int fnx_wsubstr_isgraph(const fnx_wsubstr_t *ss)
{
	return fnx_wsubstr_test_if(ss, fnx_wchr_isgraph);
}

int fnx_wsubstr_islower(const fnx_wsubstr_t *ss)
{
	return fnx_wsubstr_test_if(ss, fnx_wchr_islower);
}

int fnx_wsubstr_isprint(const fnx_wsubstr_t *ss)
{
	return fnx_wsubstr_test_if(ss, fnx_wchr_isprint);
}

int fnx_wsubstr_ispunct(const fnx_wsubstr_t *ss)
{
	return fnx_wsubstr_test_if(ss, fnx_wchr_ispunct);
}

int fnx_wsubstr_isspace(const fnx_wsubstr_t *ss)
{
	return fnx_wsubstr_test_if(ss, fnx_wchr_isspace);
}

int fnx_wsubstr_isupper(const fnx_wsubstr_t *ss)
{
	return fnx_wsubstr_test_if(ss, fnx_wchr_isupper);
}

int fnx_wsubstr_isxdigit(const fnx_wsubstr_t *ss)
{
	return fnx_wsubstr_test_if(ss, fnx_wchr_isxdigit);
}

static void wchr_toupper(wchar_t *c)
{
	*c = (wchar_t)fnx_wchr_toupper(*c);
}

static void wchr_tolower(wchar_t *c)
{
	*c = (wchar_t)fnx_wchr_tolower(*c);
}

void fnx_wsubstr_toupper(fnx_wsubstr_t *ss)
{
	fnx_wsubstr_foreach(ss, wchr_toupper);
}

void fnx_wsubstr_tolower(fnx_wsubstr_t *ss)
{
	fnx_wsubstr_foreach(ss, wchr_tolower);
}

void fnx_wsubstr_capitalize(fnx_wsubstr_t *ss)
{
	if (ss->len) {
		wchr_toupper(ss->str);
	}
}
