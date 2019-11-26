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
#include "infra-string.h"
#include "infra-substr.h"


#define substr_out_of_range(ss, pos, sz)                \
	fnx_panic("out-of-range pos=%ld sz=%ld ss=%s",      \
	          (long)pos, (long)sz, ((const char*)ss->str))


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t substr_max_size(void)
{
	return (size_t)(-1);
}
size_t fnx_substr_max_size(void)
{
	return substr_max_size();
}

static size_t substr_npos(void)
{
	return substr_max_size();
}
size_t fnx_substr_npos(void)
{
	return substr_npos();
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Immutable String Operations:
 */

/* Returns the offset of p within substr */
static size_t substr_offset(const fnx_substr_t *ss, const char *p)
{
	size_t off;

	off = substr_npos();
	if (p != NULL) {
		if ((p >= ss->str) && (p < (ss->str + ss->len))) {
			off = (size_t)(p - ss->str);
		}
	}
	return off;
}

void fnx_substr_init(fnx_substr_t *ss, const char *s)
{
	fnx_substr_init_rd(ss, s, fnx_str_length(s));
}

void fnx_substr_init_rd(fnx_substr_t *ss, const char *s, size_t n)
{
	fnx_substr_init_rw(ss, (char *)s, n, 0UL);
}

void fnx_substr_init_rwa(fnx_substr_t *ss, char *s)
{
	size_t len;

	len = fnx_str_length(s);
	fnx_substr_init_rw(ss, s, len, len);
}

void fnx_substr_init_rw(fnx_substr_t *ss,
                        char *s, size_t nrd, size_t nwr)
{
	ss->str  = s;
	ss->len  = nrd;
	ss->nwr  = nwr;
}

void fnx_substr_inits(fnx_substr_t *ss)
{
	static const char *es = "";
	fnx_substr_init(ss, es);
}

void fnx_substr_clone(const fnx_substr_t *ss, fnx_substr_t *other)
{
	other->str = ss->str;
	other->len = ss->len;
	other->nwr = ss->nwr;
}

void fnx_substr_destroy(fnx_substr_t *ss)
{
	ss->str  = NULL;
	ss->len  = 0;
	ss->nwr  = 0;
}


static const char *substr_data(const fnx_substr_t *ss)
{
	return ss->str;
}

static char *substr_mutable_data(const fnx_substr_t *ss)
{
	return (char *)(ss->str);
}

static size_t substr_size(const fnx_substr_t *ss)
{
	return ss->len;
}

size_t fnx_substr_size(const fnx_substr_t *ss)
{
	return substr_size(ss);
}

static size_t substr_wrsize(const fnx_substr_t *ss)
{
	return ss->nwr;
}

size_t fnx_substr_wrsize(const fnx_substr_t *ss)
{
	return substr_wrsize(ss);
}

static int substr_isempty(const fnx_substr_t *ss)
{
	return (substr_size(ss) == 0);
}

int fnx_substr_isempty(const fnx_substr_t *ss)
{
	return substr_isempty(ss);
}

static const char *substr_begin(const fnx_substr_t *ss)
{
	return substr_data(ss);
}

const char *fnx_substr_begin(const fnx_substr_t *ss)
{
	return substr_begin(ss);
}

static const char *substr_end(const fnx_substr_t *ss)
{
	return (substr_data(ss) + substr_size(ss));
}

const char *fnx_substr_end(const fnx_substr_t *ss)
{
	return substr_end(ss);
}

size_t fnx_substr_offset(const fnx_substr_t *ss, const char *p)
{
	return substr_offset(ss, p);
}

const char *fnx_substr_at(const fnx_substr_t *ss, size_t n)
{
	size_t sz;

	sz = substr_size(ss);
	if (!(n < sz)) {
		substr_out_of_range(ss, n, sz);
	}
	return substr_data(ss) + n;
}

int fnx_substr_isvalid_index(const fnx_substr_t *ss, size_t i)
{
	return (i < substr_size(ss));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_substr_copyto(const fnx_substr_t *ss, char *buf, size_t n)
{
	size_t n1;

	n1 = fnx_min(n, ss->len);
	fnx_str_copy(buf, ss->str, n1);

	if (n1 < n) { /* If possible, terminate with EOS. */
		fnx_str_terminate(buf, n1);
	}

	return n1;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_substr_compare(const fnx_substr_t *ss, const char *s)
{
	return fnx_substr_ncompare(ss, s, fnx_str_length(s));
}

int fnx_substr_ncompare(const fnx_substr_t *ss, const char *s, size_t n)
{
	int res = 0;

	if ((ss->str != s) || (ss->len != n)) {
		res = fnx_str_ncompare(ss->str, ss->len, s, n);
	}
	return res;
}

int fnx_substr_isequal(const fnx_substr_t *ss, const char *s)
{
	return fnx_substr_nisequal(ss, s, fnx_str_length(s));
}

int fnx_substr_nisequal(const fnx_substr_t *ss, const char *s, size_t n)
{
	int res;
	const char *str;

	res = 0;
	if (substr_size(ss) == n) {
		str = substr_data(ss);
		res = ((str == s) || (fnx_str_compare(str, s, n) == 0));
	}
	return res;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_substr_count(const fnx_substr_t *ss, const char *s)
{
	return fnx_substr_ncount(ss, s, fnx_str_length(s));
}

size_t fnx_substr_ncount(const fnx_substr_t *ss, const char *s, size_t n)
{
	size_t i, sz, pos, cnt;

	sz  = substr_size(ss);
	pos = 0;
	cnt = 0;
	i   = fnx_substr_nfind(ss, pos, s, n);
	while (i < sz) {
		++cnt;

		pos = i + n;
		i   = fnx_substr_nfind(ss, pos, s, n);
	}

	return cnt;
}

size_t fnx_substr_count_chr(const fnx_substr_t *ss, char c)
{
	size_t i, sz, pos, cnt;

	sz  = substr_size(ss);
	pos = 0;
	cnt = 0;
	i   = fnx_substr_find_chr(ss, pos, c);
	while (i < sz) {
		++cnt;

		pos = i + 1;
		i   = fnx_substr_find_chr(ss, pos, c);
	}

	return cnt;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_substr_find(const fnx_substr_t *ss, const char *s)
{
	return fnx_substr_nfind(ss, 0UL, s, fnx_str_length(s));
}

size_t fnx_substr_nfind(const fnx_substr_t *ss,
                        size_t pos, const char *s, size_t n)
{
	size_t sz;
	const char *dat;
	const char *p = NULL;

	dat = substr_data(ss);
	sz  = substr_size(ss);

	if (pos < sz) {
		if (n > 1) {
			p = fnx_str_find(dat + pos, sz - pos, s, n);
		} else if (n == 1) {
			p = fnx_str_find_chr(dat + pos, sz - pos, s[0]);
		} else { /* n == 0 */
			/*
			 * Stay compatible with STL: empty string always matches (if inside
			 * string).
			 */
			p = dat + pos;
		}
	}
	return substr_offset(ss, p);
}

size_t fnx_substr_find_chr(const fnx_substr_t *ss, size_t pos, char c)
{
	size_t sz;
	const char *dat;
	const char *p = NULL;

	dat = substr_data(ss);
	sz  = substr_size(ss);

	if (pos < sz) {
		p = fnx_str_find_chr(dat + pos, sz - pos, c);
	}
	return substr_offset(ss, p);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_substr_rfind(const fnx_substr_t *ss, const char *s)
{
	size_t pos;

	pos = substr_size(ss);
	return fnx_substr_nrfind(ss, pos, s, fnx_str_length(s));
}

size_t fnx_substr_nrfind(const fnx_substr_t *ss,
                         size_t pos, const char *s, size_t n)
{
	size_t k, sz;
	const char *p;
	const char *q;
	const char *dat;

	p   = NULL;
	q   = s;
	dat = substr_data(ss);
	sz  = substr_size(ss);
	k   = (pos < sz) ? pos + 1 : sz;

	if (n == 0) {
		/* STL compatible: empty string always matches */
		p = dat + k;
	} else if (n == 1) {
		p = fnx_str_rfind_chr(dat, k, *q);
	} else {
		p = fnx_str_rfind(dat, k, q, n);
	}

	return substr_offset(ss, p);
}

size_t fnx_substr_rfind_chr(const fnx_substr_t *ss, size_t pos, char c)
{
	size_t sz, k;
	const char *p;
	const char *dat;

	dat = substr_data(ss);
	sz  = substr_size(ss);
	k   = (pos < sz) ? pos + 1 : sz;

	p = fnx_str_rfind_chr(dat, k, c);
	return substr_offset(ss, p);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_substr_find_first_of(const fnx_substr_t *ss, const char *s)
{
	return fnx_substr_nfind_first_of(ss, 0UL, s, fnx_str_length(s));
}

size_t fnx_substr_nfind_first_of(const fnx_substr_t *ss,
                                 size_t pos, const char *s, size_t n)
{
	size_t sz;
	const char *p;
	const char *q;
	const char *dat;

	p   = NULL;
	q   = s;
	dat = substr_data(ss);
	sz  = substr_size(ss);

	if ((n != 0) && (pos < sz)) {
		if (n == 1) {
			p = fnx_str_find_chr(dat + pos, sz - pos, *q);
		} else {
			p = fnx_str_find_first_of(dat + pos, sz - pos, q, n);
		}
	}

	return substr_offset(ss, p);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_substr_find_last_of(const fnx_substr_t *ss, const char *s)
{
	size_t pos;

	pos = substr_size(ss);
	return fnx_substr_nfind_last_of(ss, pos, s, fnx_str_length(s));
}

size_t fnx_substr_nfind_last_of(const fnx_substr_t *ss, size_t pos,
                                const char *s, size_t n)
{
	size_t sz;
	const char *p, *q, *dat;

	p   = NULL;
	q   = s;
	dat = substr_data(ss);
	sz  = substr_size(ss);
	if (n != 0) {
		const size_t k = (pos < sz) ? pos + 1 : sz;

		if (n == 1) {
			p = fnx_str_rfind_chr(dat, k, *q);
		} else {
			p = fnx_str_find_last_of(dat, k, q, n);
		}
	}
	return substr_offset(ss, p);
}

size_t fnx_substr_find_first_not_of(const fnx_substr_t *ss,
                                    const char *s)
{
	return fnx_substr_nfind_first_not_of(ss, 0UL, s, fnx_str_length(s));
}

size_t fnx_substr_nfind_first_not_of(const fnx_substr_t *ss,
                                     size_t pos, const char *s, size_t n)
{
	size_t sz;
	const char *p, *q, *dat;

	p   = NULL;
	q   = s;
	dat = substr_data(ss);
	sz  = substr_size(ss);

	if (pos < sz) {
		if (n == 0) {
			p = dat + pos;
		} else if (n == 1) {
			p = fnx_str_find_first_not_eq(dat + pos, sz - pos, *q);
		} else {
			p = fnx_str_find_first_not_of(dat + pos, sz - pos, q, n);
		}
	}

	return substr_offset(ss, p);
}

size_t fnx_substr_find_first_not(const fnx_substr_t *ss,
                                 size_t pos, char c)
{
	size_t sz;
	const char *p, *dat;

	p   = NULL;
	dat = substr_data(ss);
	sz  = substr_size(ss);
	if (pos < sz) {
		p = fnx_str_find_first_not_eq(dat + pos, sz - pos, c);
	}
	return substr_offset(ss, p);
}

size_t fnx_substr_find_last_not_of(const fnx_substr_t *ss, const char *s)
{
	size_t pos;

	pos = substr_size(ss);
	return fnx_substr_nfind_last_not_of(ss, pos, s, fnx_str_length(s));
}

size_t fnx_substr_nfind_last_not_of(const fnx_substr_t *ss,
                                    size_t pos, const char *s, size_t n)
{
	size_t k, sz;
	const char *p, *q, *dat;

	p   = NULL;
	q   = s;
	dat = substr_data(ss);
	sz  = substr_size(ss);

	if (sz != 0) {
		k = (pos < sz) ? pos + 1 : sz;
		if (n == 0) {
			/* Stay compatible with STL. */
			p = dat + k - 1;
		} else if (n == 1) {
			p = fnx_str_find_last_not_eq(dat, k, *q);
		} else {
			p = fnx_str_find_last_not_of(dat, k, q, n);
		}
	}

	return substr_offset(ss, p);
}

size_t fnx_substr_find_last_not(const fnx_substr_t *ss,
                                size_t pos, char c)
{
	size_t k, sz;
	const char *p, *dat;

	dat = substr_data(ss);
	sz  = substr_size(ss);
	k   = (pos < sz) ? pos + 1 : sz;
	p   = fnx_str_find_last_not_eq(dat, k, c);

	return substr_offset(ss, p);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_substr_sub(const fnx_substr_t *ss,
                    size_t i, size_t n, fnx_substr_t *result)
{
	size_t j, k, n1, n2, sz, wr;
	char *dat;

	sz  = substr_size(ss);
	j   = fnx_min(i, sz);
	n1  = fnx_min(n, sz - j);

	wr  = substr_wrsize(ss);
	k   = fnx_min(i, wr);
	n2  = fnx_min(n, wr - k);
	dat = substr_mutable_data(ss);

	fnx_substr_init_rw(result, dat + j, n1, n2);
}

void fnx_substr_rsub(const fnx_substr_t *ss,
                     size_t n, fnx_substr_t *result)
{
	size_t j, k, n1, n2, sz, wr;
	char *dat;

	sz  = substr_size(ss);
	n1  = fnx_min(n, sz);
	j   = sz - n1;

	wr  = substr_wrsize(ss);
	k   = fnx_min(j, wr);
	n2  = wr - k;
	dat = substr_mutable_data(ss);

	fnx_substr_init_rw(result, dat + j, n1, n2);
}

void fnx_substr_intersection(const fnx_substr_t *s1,
                             const fnx_substr_t *s2,
                             fnx_substr_t *result)
{
	size_t i, n;
	const char *s1_begin;
	const char *s1_end;
	const char *s2_begin;
	const char *s2_end;

	s1_begin = substr_begin(s1);
	s2_begin = substr_begin(s2);
	if (s1_begin <= s2_begin) {
		i = n = 0;

		s1_end = substr_end(s1);
		s2_end = substr_end(s2);

		/* Case 1:  [.s1...)  [..s2.....) -- Returns empty substring. */
		if (s1_end <= s2_begin) {
			i = substr_size(s2);
		}
		/* Case 2: [.s1........)
		                [.s2..) */
		else if (s2_end <= s1_end) {
			n = substr_size(s2);
		}
		/* Case 3: [.s1.....)
		               [.s2......) */
		else {
			n = (size_t)(s1_end - s2_begin);
		}
		fnx_substr_sub(s2, i, n, result);
	} else {
		/* One step recursion -- its ok */
		fnx_substr_intersection(s2, s1, result);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Helper function to create split-of-substrings */
static void substr_make_split_pair(const fnx_substr_t *ss,
                                   size_t i1, size_t n1,
                                   size_t i2, size_t n2,
                                   fnx_substr_pair_t *result)
{
	fnx_substr_sub(ss, i1, n1, &result->first);
	fnx_substr_sub(ss, i2, n2, &result->second);
}

void fnx_substr_split(const fnx_substr_t *ss,
                      const char *seps, fnx_substr_pair_t *result)
{

	fnx_substr_nsplit(ss, seps, fnx_str_length(seps), result);
}

void fnx_substr_nsplit(const fnx_substr_t *ss,
                       const char *seps, size_t n,
                       fnx_substr_pair_t *result)
{
	size_t i, j, sz;

	sz = substr_size(ss);
	i  = fnx_substr_nfind_first_of(ss, 0UL, seps, n);
	j  = sz;
	if (i < sz) {
		j = fnx_substr_nfind_first_not_of(ss, i, seps, n);
	}

	substr_make_split_pair(ss, 0UL, i, j, sz, result);
}

void fnx_substr_split_chr(const fnx_substr_t *ss, char sep,
                          fnx_substr_pair_t *result)
{
	size_t i, j, sz;

	sz  = substr_size(ss);
	i   = fnx_substr_find_chr(ss, 0UL, sep);
	j   = (i < sz) ? i + 1 : sz;

	substr_make_split_pair(ss, 0UL, i, j, sz, result);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_substr_rsplit(const fnx_substr_t *ss,
                       const char *seps, fnx_substr_pair_t *result)
{

	fnx_substr_nrsplit(ss, seps, fnx_str_length(seps), result);
}

void fnx_substr_nrsplit(const fnx_substr_t *ss,
                        const char *seps, size_t n,
                        fnx_substr_pair_t *result)
{
	size_t i, j, sz;

	sz = substr_size(ss);
	i  = fnx_substr_nfind_last_of(ss, sz, seps, n);
	j  = sz;
	if (i < sz) {
		j = fnx_substr_nfind_last_not_of(ss, i, seps, n);

		if (j < sz) {
			++i;
			++j;
		} else {
			i = j = sz;
		}
	}
	substr_make_split_pair(ss, 0UL, j, i, sz, result);
}

void fnx_substr_rsplit_chr(const fnx_substr_t *ss, char sep,
                           fnx_substr_pair_t *result)
{
	size_t i, j, sz;

	sz  = substr_size(ss);
	i   = fnx_substr_rfind_chr(ss, sz, sep);
	j   = (i < sz) ? i + 1 : sz;

	substr_make_split_pair(ss, 0UL, i, j, sz, result);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_substr_trim(const fnx_substr_t *substr, size_t n,
                     fnx_substr_t *result)
{
	size_t sz;

	sz = substr_size(substr);
	fnx_substr_sub(substr, n, sz, result);
}

void fnx_substr_trim_any_of(const fnx_substr_t *ss,
                            const char *set, fnx_substr_t *result)
{
	fnx_substr_ntrim_any_of(ss, set, fnx_str_length(set), result);
}

void fnx_substr_ntrim_any_of(const fnx_substr_t *ss,
                             const char *set, size_t n,
                             fnx_substr_t *result)
{
	size_t i, sz;

	sz = substr_size(ss);
	i  = fnx_substr_nfind_first_not_of(ss, 0UL, set, n);

	fnx_substr_sub(ss, i, sz, result);
}

void fnx_substr_trim_chr(const fnx_substr_t *ss, char c,
                         fnx_substr_t *result)
{
	size_t i, sz;

	sz = substr_size(ss);
	i  = fnx_substr_find_first_not(ss, 0UL, c);

	fnx_substr_sub(ss, i, sz, result);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_substr_chop(const fnx_substr_t *ss,
                     size_t n, fnx_substr_t *result)
{
	size_t k, sz, wr;
	char *dat;

	sz  = substr_size(ss);
	wr  = substr_wrsize(ss);
	k   = fnx_min(sz, n);
	dat = substr_mutable_data(ss);

	fnx_substr_init_rw(result, dat, sz - k, wr);
}

void fnx_substr_chop_any_of(const fnx_substr_t *ss,
                            const char *set, fnx_substr_t *result)
{
	fnx_substr_nchop_any_of(ss, set, fnx_str_length(set), result);
}

void fnx_substr_nchop_any_of(const fnx_substr_t *ss,
                             const char *set, size_t n,
                             fnx_substr_t *result)
{
	size_t j, sz;

	sz = substr_size(ss);
	j  = fnx_substr_nfind_last_not_of(ss, sz, set, n);

	fnx_substr_sub(ss, 0UL, ((j < sz) ? j + 1 : 0), result);
}

void fnx_substr_chop_chr(const fnx_substr_t *substr, char c,
                         fnx_substr_t *result)
{
	size_t j, sz;

	sz = substr_size(substr);
	j  = fnx_substr_find_last_not(substr, sz, c);

	fnx_substr_sub(substr, 0UL, ((j < sz) ? j + 1 : 0), result);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_substr_strip_any_of(const fnx_substr_t *ss,
                             const char *set, fnx_substr_t *result)
{
	fnx_substr_nstrip_any_of(ss, set, fnx_str_length(set), result);
}

void fnx_substr_nstrip_any_of(const fnx_substr_t *ss,
                              const char *set, size_t n,
                              fnx_substr_t *result)
{
	fnx_substr_t sub;

	fnx_substr_ntrim_any_of(ss, set, n, &sub);
	fnx_substr_nchop_any_of(&sub, set, n, result);
}

void fnx_substr_strip_chr(const fnx_substr_t *ss, char c,
                          fnx_substr_t *result)
{
	fnx_substr_t sub;

	fnx_substr_trim_chr(ss, c, &sub);
	fnx_substr_chop_chr(&sub, c, result);
}

void fnx_substr_strip_ws(const fnx_substr_t *ss, fnx_substr_t *result)
{
	const char *spaces = " \n\t\r\v\f";
	fnx_substr_strip_any_of(ss, spaces, result);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_substr_find_token(const fnx_substr_t *ss,
                           const char *seps, fnx_substr_t *result)
{
	fnx_substr_nfind_token(ss, seps, fnx_str_length(seps), result);
}

void fnx_substr_nfind_token(const fnx_substr_t *ss,
                            const char *seps, size_t n,
                            fnx_substr_t *result)
{
	size_t i, j, sz;

	sz = substr_size(ss);
	i  = fnx_min(fnx_substr_nfind_first_not_of(ss, 0UL, seps, n), sz);
	j  = fnx_min(fnx_substr_nfind_first_of(ss, i, seps, n), sz);

	fnx_substr_sub(ss, i, j - i, result);
}

void fnx_substr_find_token_chr(const fnx_substr_t *ss, char sep,
                               fnx_substr_t *result)
{
	size_t i, j, sz;

	sz = substr_size(ss);
	i  = fnx_min(fnx_substr_find_first_not(ss, 0UL, sep), sz);
	j  = fnx_min(fnx_substr_find_chr(ss, i, sep), sz);

	fnx_substr_sub(ss, i, j - i, result);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_substr_find_next_token(const fnx_substr_t *ss,
                                const fnx_substr_t *tok,
                                const char *seps,
                                fnx_substr_t *result)
{
	fnx_substr_nfind_next_token(ss, tok, seps, fnx_str_length(seps), result);
}

void fnx_substr_nfind_next_token(const fnx_substr_t *ss,
                                 const fnx_substr_t *tok,
                                 const char *seps, size_t n,
                                 fnx_substr_t *result)
{
	size_t i, sz;
	const char *p;
	fnx_substr_t sub;

	sz  = substr_size(ss);
	p   = substr_end(tok);
	i   = substr_offset(ss, p);

	fnx_substr_sub(ss, i, sz, &sub);
	fnx_substr_nfind_token(&sub, seps, n, result);
}

void fnx_substr_find_next_token_chr(const fnx_substr_t *ss,
                                    const fnx_substr_t *tok, char sep,
                                    fnx_substr_t *result)
{
	size_t i, sz;
	const char *p;
	fnx_substr_t sub;

	sz  = substr_size(ss);
	p   = substr_end(tok);
	i   = substr_offset(ss, p);

	fnx_substr_sub(ss, i, sz, &sub);
	fnx_substr_find_token_chr(&sub, sep, result);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_substr_tokenize(const fnx_substr_t *ss,
                        const char *seps,
                        fnx_substr_t tok_list[],
                        size_t list_size, size_t *p_ntok)
{
	return fnx_substr_ntokenize(ss, seps, fnx_str_length(seps),
	                            tok_list, list_size, p_ntok);
}

int fnx_substr_ntokenize(const fnx_substr_t *ss,
                         const char *seps, size_t n,
                         fnx_substr_t tok_list[],
                         size_t list_size, size_t *p_ntok)
{
	size_t ntok = 0;
	fnx_substr_t tok_obj;
	fnx_substr_t *tok = &tok_obj;
	fnx_substr_t *tgt = NULL;

	fnx_substr_nfind_token(ss, seps, n, tok);
	while (!fnx_substr_isempty(tok)) {
		if (ntok == list_size) {
			return -1; /* Insufficient room */
		}
		tgt = &tok_list[ntok++];
		fnx_substr_clone(tok, tgt);

		fnx_substr_nfind_next_token(ss, tok, seps, n, tok);
	}

	if (p_ntok != NULL) {
		*p_ntok = ntok;
	}
	return 0;
}

int fnx_substr_tokenize_chr(const fnx_substr_t *ss, char sep,
                            fnx_substr_t tok_list[],
                            size_t list_size, size_t *p_ntok)
{
	size_t ntok = 0;
	fnx_substr_t tok_obj;
	fnx_substr_t *tok = &tok_obj;
	fnx_substr_t *tgt = NULL;

	fnx_substr_find_token_chr(ss, sep, tok);
	while (!fnx_substr_isempty(tok)) {
		if (ntok == list_size) {
			return -1; /* Insufficient room */
		}
		tgt = &tok_list[ntok++];
		fnx_substr_clone(tok, tgt);

		fnx_substr_find_next_token_chr(ss, tok, sep, tok);
	}

	if (p_ntok != NULL) {
		*p_ntok = ntok;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_substr_common_prefix(const fnx_substr_t *ss, const char *s)
{
	return fnx_substr_ncommon_prefix(ss, s, fnx_str_length(s));
}

size_t fnx_substr_ncommon_prefix(const fnx_substr_t *ss,
                                 const char *s, size_t n)
{
	size_t sz;
	const char *p;

	p  = substr_data(ss);
	sz = substr_size(ss);
	return fnx_str_common_prefix(p, s, fnx_min(n, sz));
}

int fnx_substr_starts_with(const fnx_substr_t *ss, char c)
{
	const char s[] = { c };
	return (fnx_substr_ncommon_prefix(ss, s, 1) == 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_substr_common_suffix(const fnx_substr_t *ss, const char *s)
{
	return fnx_substr_ncommon_suffix(ss, s, fnx_str_length(s));
}

size_t fnx_substr_ncommon_suffix(const fnx_substr_t *ss,
                                 const char *s, size_t n)
{
	size_t sz, k;
	const char *p;

	p  = substr_data(ss);
	sz = substr_size(ss);
	if (n > sz) {
		k = fnx_str_common_suffix(p, s + (n - sz), sz);
	} else {
		k = fnx_str_common_suffix(p + (sz - n), s, n);
	}
	return k;
}

int fnx_substr_ends_with(const fnx_substr_t *ss, char c)
{
	const char s[] = { c };
	return (fnx_substr_ncommon_suffix(ss, s, 1) == 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Mutable String Opeartions:
 */
char *fnx_substr_data(const fnx_substr_t *ss)
{
	return substr_mutable_data(ss);
}

/* Set EOS characters at the end of characters array (if possible) */
static void substr_terminate(fnx_substr_t *ss)
{
	char *dat;
	size_t sz, wr;

	dat = substr_mutable_data(ss);
	sz  = substr_size(ss);
	wr  = substr_wrsize(ss);

	if (sz < wr) {
		fnx_str_terminate(dat, sz);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Inserts a copy of s before position pos. */
static void substr_insert(fnx_substr_t *ss,
                          size_t pos, const char *s, size_t n)
{
	size_t sz, wr, j, k, rem, m;
	char *dat;

	sz  = substr_size(ss);
	wr  = substr_wrsize(ss);
	dat = fnx_substr_data(ss);

	/* Start insertion before position j. */
	j   = fnx_min(pos, sz);

	/* Number of writable elements after j. */
	rem = (j < wr) ? (wr - j) : 0;

	/* Number of elements of substr after j (to be moved fwd). */
	k   = sz - j;

	/* Insert n elements of p: try as many as possible, truncate tail in
	    case of insufficient buffer capacity. */
	m = fnx_str_insert(dat + j, rem, k, s, n);
	ss->len = j + m;

	substr_terminate(ss);
}

/* Inserts n copies of c before position pos. */
static void substr_insert_fill(fnx_substr_t *ss,
                               size_t pos, size_t n, char c)
{
	size_t sz, wr, j, k, rem, m;
	char *dat;

	sz  = substr_size(ss);
	wr  = substr_wrsize(ss);
	dat = fnx_substr_data(ss);

	/* Start insertion before position j. */
	j   = fnx_min(pos, sz);

	/* Number of writable elements after j. */
	rem = (j < wr) ? (wr - j) : 0;

	/* Number of elements of substr after j (to be moved fwd). */
	k   = sz - j;

	/* Insert n copies of c: try as many as possible; truncate tail in
	    case of insufficient buffer capacity. */
	m = fnx_str_insert_chr(dat + j, rem, k, n, c);
	ss->len = j + m;

	substr_terminate(ss);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Replaces a substring of *this with a copy of s. */
static void substr_replace(fnx_substr_t *ss, size_t pos, size_t n1,
                           const char *s, size_t n)
{
	size_t sz, wr, k, m;
	char *dat;

	sz  = substr_size(ss);
	wr  = substr_wrsize(ss);
	dat = substr_mutable_data(ss);

	/* Number of elements to replace (assuming pos <= size). */
	k = fnx_min(sz - pos, n1);

	/* Replace k elements after pos with s; truncate tail in case of
	    insufficient buffer capacity. */
	m = fnx_str_replace(dat + pos, wr - pos, sz - pos, k, s, n);
	ss->len = pos + m;

	substr_terminate(ss);
}

/* Replaces a substring of *this with n2 copies of c. */
static void substr_replace_fill(fnx_substr_t *ss,
                                size_t pos, size_t n1, size_t n2, char c)
{
	size_t sz, wr, k, m;
	char *dat;

	sz  = substr_size(ss);
	wr  = substr_wrsize(ss);
	dat = substr_mutable_data(ss);

	/* Number of elements to replace (assuming pos <= size). */
	k = fnx_min(sz - pos, n1);

	/* Replace k elements after pos with n2 copies of c; truncate tail in
	    case of insufficient buffer capacity. */
	m = fnx_str_replace_chr(dat + pos, wr - pos, sz - pos, k, n2, c);
	ss->len = pos + m;

	substr_terminate(ss);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_substr_assign(fnx_substr_t *ss, const char *s)
{
	fnx_substr_nassign(ss, s, fnx_str_length(s));
}

void fnx_substr_nassign(fnx_substr_t *ss, const char *s, size_t len)
{
	size_t sz;

	sz = substr_size(ss);
	fnx_substr_nreplace(ss, 0, sz, s, len);
}

void fnx_substr_assign_chr(fnx_substr_t *ss, size_t n, char c)
{
	size_t sz;

	sz = substr_size(ss);
	fnx_substr_replace_chr(ss, 0, sz, n, c);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_substr_push_back(fnx_substr_t *ss, char c)
{
	fnx_substr_append_chr(ss, 1, c);
}

void fnx_substr_append(fnx_substr_t *ss, const char *s)
{
	fnx_substr_nappend(ss, s, fnx_str_length(s));
}

void fnx_substr_nappend(fnx_substr_t *ss, const char *s, size_t len)
{
	size_t sz;

	sz = substr_size(ss);
	fnx_substr_ninsert(ss, sz, s, len);
}

void fnx_substr_append_chr(fnx_substr_t *ss, size_t n, char c)
{
	size_t sz;

	sz = substr_size(ss);
	fnx_substr_insert_chr(ss, sz, n, c);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_substr_insert(fnx_substr_t *ss, size_t pos, const char *s)
{
	fnx_substr_ninsert(ss, pos, s, fnx_str_length(s));
}

void fnx_substr_ninsert(fnx_substr_t *ss, size_t pos,
                        const char *s, size_t len)
{
	size_t sz;

	sz = substr_size(ss);
	if (pos <= sz) {
		substr_insert(ss, pos, s, len);
	} else {
		substr_out_of_range(ss, pos, sz);
	}
}

void fnx_substr_insert_chr(fnx_substr_t *ss, size_t pos, size_t n, char c)
{
	size_t sz;

	sz = substr_size(ss);
	if (pos <= sz) {
		substr_insert_fill(ss, pos, n, c);
	} else {
		substr_out_of_range(ss, pos, sz);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_substr_replace(fnx_substr_t *ss,
                        size_t pos, size_t n, const char *s)
{
	fnx_substr_nreplace(ss, pos, n, s, fnx_str_length(s));
}

void fnx_substr_nreplace(fnx_substr_t *ss,
                         size_t pos, size_t n,  const char *s, size_t len)
{
	size_t sz;

	sz = substr_size(ss);
	if (pos < sz) {
		substr_replace(ss, pos, n, s, len);
	} else if (pos == sz) {
		substr_insert(ss, pos, s, len);
	} else {
		substr_out_of_range(ss, pos, sz);
	}
}

void fnx_substr_replace_chr(fnx_substr_t *ss,
                            size_t pos, size_t n1, size_t n2, char c)
{
	size_t sz;

	sz = substr_size(ss);
	if (pos < sz) {
		substr_replace_fill(ss, pos, n1, n2, c);
	} else if (pos == sz) {
		substr_insert_fill(ss, pos, n2, c);
	} else {
		substr_out_of_range(ss, pos, sz);
	}
}

void fnx_substr_erase(fnx_substr_t *ss, size_t pos, size_t n)
{
	char c = '\0';
	fnx_substr_replace_chr(ss, pos, n, 0, c);
}

void fnx_substr_reverse(fnx_substr_t *ss)
{
	size_t n, sz, wr;
	char *dat;

	sz  = substr_size(ss);
	wr  = substr_wrsize(ss);
	n   = fnx_min(sz, wr);
	dat = fnx_substr_data(ss);

	fnx_str_reverse(dat, n);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Generic Operations:
 */
static size_t substr_find_if(const fnx_substr_t *ss, fnx_chr_pred fn, int u)
{
	int v;
	const char *s, *t, *p = NULL;

	s = substr_begin(ss);
	t = substr_end(ss);
	while (s < t) {
		v = fn(*s) ? 1 : 0;
		if (v == u) {
			p = s;
			break;
		}
		++s;
	}
	return substr_offset(ss, p);
}

size_t fnx_substr_find_if(const fnx_substr_t *ss, fnx_chr_pred fn)
{
	return substr_find_if(ss, fn, 1);
}

size_t fnx_substr_find_if_not(const fnx_substr_t *ss, fnx_chr_pred fn)
{
	return substr_find_if(ss, fn, 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t
substr_rfind_if(const fnx_substr_t *ss, fnx_chr_pred fn, int u)
{
	int v;
	const char *s, *t, *p = NULL;

	s = substr_end(ss);
	t = substr_begin(ss);
	while (s-- > t) {
		v = fn(*s) ? 1 : 0;
		if (v == u) {
			p = s;
			break;
		}
	}
	return substr_offset(ss, p);
}

size_t fnx_substr_rfind_if(const fnx_substr_t *ss, fnx_chr_pred fn)
{
	return substr_rfind_if(ss, fn, 1);
}

size_t fnx_substr_rfind_if_not(const fnx_substr_t *ss, fnx_chr_pred fn)
{
	return substr_rfind_if(ss, fn, 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_substr_count_if(const fnx_substr_t *ss, fnx_chr_pred fn)
{
	size_t cnt;
	const char *s, *t;

	cnt = 0;
	s = substr_begin(ss);
	t = substr_end(ss);
	while (s < t) {
		if (fn(*s++)) {
			++cnt;
		}
	}
	return cnt;
}


int fnx_substr_test_if(const fnx_substr_t *substr, fnx_chr_pred fn)
{
	const char *s, *t;

	s = substr_begin(substr);
	t = substr_end(substr);
	while (s < t) {
		if (!fn(*s++)) {
			return 0;
		}
	}
	return 1;
}

void fnx_substr_trim_if(const fnx_substr_t *ss,
                        fnx_chr_pred fn, fnx_substr_t   *result)
{
	size_t i, sz;

	sz = substr_size(ss);
	i  = fnx_substr_find_if_not(ss, fn);

	fnx_substr_sub(ss, i, sz, result);
}

void fnx_substr_chop_if(const fnx_substr_t *ss,
                        fnx_chr_pred fn, fnx_substr_t *result)
{
	size_t j, sz;

	sz = substr_size(ss);
	j  = fnx_substr_rfind_if_not(ss, fn);

	fnx_substr_sub(ss, 0UL, ((j < sz) ? j + 1 : 0), result);
}

void fnx_substr_strip_if(const fnx_substr_t *ss,
                         fnx_chr_pred fn, fnx_substr_t *result)
{
	fnx_substr_t sub;

	fnx_substr_trim_if(ss, fn, &sub);
	fnx_substr_chop_if(&sub, fn, result);
}

void fnx_substr_foreach(fnx_substr_t *ss, fnx_chr_modify_fn fn)
{
	size_t i, sz;
	char *p;

	sz = substr_wrsize(ss);
	p  = substr_mutable_data(ss);
	for (i = 0; i < sz; ++i) {
		fn(p++);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Ctype Operations:
 */
int fnx_substr_isalnum(const fnx_substr_t *ss)
{
	return fnx_substr_test_if(ss, fnx_chr_isalnum);
}

int fnx_substr_isalpha(const fnx_substr_t *ss)
{
	return fnx_substr_test_if(ss, fnx_chr_isalpha);
}

int fnx_substr_isascii(const fnx_substr_t *ss)
{
	return fnx_substr_test_if(ss, fnx_chr_isascii);
}

int fnx_substr_isblank(const fnx_substr_t *ss)
{
	return fnx_substr_test_if(ss, fnx_chr_isblank);
}

int fnx_substr_iscntrl(const fnx_substr_t *ss)
{
	return fnx_substr_test_if(ss, fnx_chr_iscntrl);
}

int fnx_substr_isdigit(const fnx_substr_t *ss)
{
	return fnx_substr_test_if(ss, fnx_chr_isdigit);
}

int fnx_substr_isgraph(const fnx_substr_t *ss)
{
	return fnx_substr_test_if(ss, fnx_chr_isgraph);
}

int fnx_substr_islower(const fnx_substr_t *ss)
{
	return fnx_substr_test_if(ss, fnx_chr_islower);
}

int fnx_substr_isprint(const fnx_substr_t *ss)
{
	return fnx_substr_test_if(ss, fnx_chr_isprint);
}

int fnx_substr_ispunct(const fnx_substr_t *ss)
{
	return fnx_substr_test_if(ss, fnx_chr_ispunct);
}

int fnx_substr_isspace(const fnx_substr_t *ss)
{
	return fnx_substr_test_if(ss, fnx_chr_isspace);
}

int fnx_substr_isupper(const fnx_substr_t *ss)
{
	return fnx_substr_test_if(ss, fnx_chr_isupper);
}

int fnx_substr_isxdigit(const fnx_substr_t *ss)
{
	return fnx_substr_test_if(ss, fnx_chr_isxdigit);
}

static void chr_toupper(char *c)
{
	*c = (char)fnx_chr_toupper(*c);
}

static void chr_tolower(char *c)
{
	*c = (char)fnx_chr_tolower(*c);
}

void fnx_substr_toupper(fnx_substr_t *ss)
{
	fnx_substr_foreach(ss, chr_toupper);
}

void fnx_substr_tolower(fnx_substr_t *ss)
{
	fnx_substr_foreach(ss, chr_tolower);
}

void fnx_substr_capitalize(fnx_substr_t *ss)
{
	if (ss->len) {
		chr_toupper(ss->str);
	}
}


