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
#define _GNU_SOURCE 1
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "infra.h"
#include "wstrchr.h"


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int eq(wchar_t c1, wchar_t c2)
{
	return (c1 == c2);
}

static size_t distance(const wchar_t *beg, const wchar_t *end)
{
	return ((end > beg) ? (size_t)(end - beg) : 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int str_memcmp(const wchar_t *s1, const wchar_t *s2, size_t n)
{
	return wmemcmp(s1, s2, n);
}

static const wchar_t *str_memchr(const wchar_t *s, size_t n, wchar_t c)
{
	return (const wchar_t *)(wmemchr(s, c, n));
}

static const wchar_t *str_memmem(const wchar_t *s1, size_t n1,
                                 const wchar_t *s2, size_t n2)
{
	const size_t csz = sizeof(*s1);
	return (const wchar_t *)memmem(s1, n1 * csz, s2, n2 * csz);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t wstr_length(const wchar_t *s)
{
	return (s != NULL) ? wcslen(s) : 0;
}

int wstr_compare(const wchar_t *s1, const wchar_t *s2, size_t n)
{
	return str_memcmp(s1, s2, n);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int wstr_ncompare(const wchar_t *s1, size_t n1,
                  const wchar_t *s2, size_t n2)
{
	int res;

	res = wstr_compare(s1, s2, cac_min(n1, n2));
	if (res == 0) {
		res = (n1 > n2) - (n1 < n2);
	}
	return res;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const wchar_t *wstr_find_chr(const wchar_t *s, size_t n, wchar_t c)
{
	return str_memchr(s, n, c);
}

size_t wstr_count_chr(const wchar_t *s, size_t n, wchar_t c)
{
	const wchar_t *p;
	size_t k, cnt = 0;

	p = wstr_find_chr(s, n, c);
	while (p != NULL) {
		++cnt;
		k = distance(s, p);
		if (k < n) {
			p = wstr_find_chr(s + k + 1, (n - k) - 1, c);
		} else {
			p = NULL;
		}
	}
	return cnt;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const wchar_t *wstr_find(const wchar_t *s1, size_t n1,
                         const wchar_t *s2, size_t n2)
{
	const wchar_t *p = NULL;

	if ((n2 > 0) && (n1 >= n2)) {
		p = str_memmem(s1, n1, s2, n2);
	}
	return p;
}

size_t wstr_count(const wchar_t *s1, size_t n1, const wchar_t *s2,
                  size_t n2)
{
	const wchar_t *p;
	size_t k, r, cnt = 0;

	p = wstr_find(s1, n1, s2, n2);
	while (p != NULL) {
		++cnt;
		k = distance(s1, p);
		r = n1 - k;
		if ((k < n1) && (r >= n2)) {
			p = wstr_find(s1 + k + n2, r - n2, s2, n2);
		} else {
			p = NULL;
		}
	}
	return cnt;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const wchar_t *wstr_rfind(const wchar_t *s1, size_t n1,
                          const wchar_t *s2, size_t n2)
{
	const wchar_t *p;

	if (n2 && (n1 >= n2)) {
		for (p = s1 + (n1 - n2); p >= s1; --p) {
			if (wstr_compare(p, s2, n2) == 0) {
				return p;
			}
		}
	}
	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const wchar_t *wstr_rfind_chr(const wchar_t *s, size_t n, wchar_t c)
{
	const wchar_t *p = s + n;

	while (p-- > s) {
		if (eq(*p, c)) {
			return p;
		}
	}
	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const wchar_t *wstr_find_first_of(const wchar_t *s1, size_t n1,
                                  const wchar_t *s2, size_t n2)
{
	const wchar_t *p;
	const wchar_t *last;

	last = s1 + n1;
	for (p = s1; p < last; ++p) {
		if (wstr_find_chr(s2, n2, *p) != NULL) {
			return p;
		}
	}
	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const wchar_t *wstr_find_first_not_of(const wchar_t *s1, size_t n1,
                                      const wchar_t *s2, size_t n2)
{
	const wchar_t *p;
	const wchar_t *last;

	last = s1 + n1;
	for (p = s1; p < last; ++p) {
		if (wstr_find_chr(s2, n2, *p) == NULL) {
			return p;
		}
	}
	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const wchar_t *wstr_find_first_not_eq(const wchar_t *s,
                                      size_t n, wchar_t c)
{
	const wchar_t *p;
	const wchar_t *last;

	last = s + n;
	for (p = s; p < last; ++p) {
		if (!eq(*p, c)) {
			return p;
		}
	}
	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const wchar_t *wstr_find_last_of(const wchar_t *s1, size_t n1,
                                 const wchar_t *s2, size_t n2)
{
	const wchar_t *p;

	if (n2 > 0) {
		p = s1 + n1;
		while (p > s1) {
			if (wstr_find_chr(s2, n2, *--p) != NULL) {
				return p;
			}
		}
	}
	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const wchar_t *wstr_find_last_not_of(const wchar_t *s1, size_t n1,
                                     const wchar_t *s2, size_t n2)
{
	const wchar_t *p;
	const wchar_t *last;

	last = s1 + n1;
	for (p = last; p > s1;) {
		if (wstr_find_chr(s2, n2, *--p) == NULL) {
			return p;
		}
	}
	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const wchar_t *wstr_find_last_not_eq(const wchar_t *s, size_t n, wchar_t c)
{
	const wchar_t *p;

	for (p = s + n; p > s;) {
		if (!eq(*--p, c)) {
			return p;
		}
	}
	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t wstr_common_prefix(const wchar_t *s1, size_t n1,
                          const wchar_t *s2, size_t n2)
{
	size_t k = 0;
	const wchar_t *p = s1;
	const wchar_t *q = s2;
	const size_t n = cac_min(n1, n2);

	while ((k < n) && eq(*p++, *q++)) {
		++k;
	}
	return k;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t wstr_common_suffix(const wchar_t *s1, size_t n1,
                          const wchar_t *s2, size_t n2)
{
	size_t k = 0;
	const wchar_t *p = s1 + n1;
	const wchar_t *q = s2 + n2;
	const size_t n = cac_min(n1, n2);

	while ((k < n) && eq(*--p, *--q)) {
		++k;
	}
	return k;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t wstr_overlaps(const wchar_t *s1, size_t n1,
                     const wchar_t *s2, size_t n2)
{
	size_t d, k;

	if (s1 < s2) {
		d = (size_t)(s2 - s1);
		k = (d < n1) ? (n1 - d) : 0;
	} else {
		d = (size_t)(s1 - s2);
		k = (d < n2) ? (n2 - d) : 0;
	}
	return k;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Generic Operations:
 */
const wchar_t *wstr_find_if(const wchar_t *beg, const wchar_t *end,
                            wchr_fn fn, int u)
{
	int v;
	const wchar_t *itr = beg;

	while (itr < end) {
		v = fn(*itr) ? 1 : 0;
		if (v == u) {
			return itr;
		}
		++itr;
	}
	return NULL;
}

const wchar_t *wstr_rfind_if(const wchar_t *beg, const wchar_t *end,
                             wchr_fn fn, int u)
{
	int v;
	const wchar_t *itr = end;

	while (itr-- > beg) {
		v = fn(*itr) ? 1 : 0;
		if (v == u) {
			return itr;
		}
	}
	return NULL;
}

int wstr_test_if(const wchar_t *beg, const wchar_t *end, wchr_fn fn)
{
	const wchar_t *itr = beg;

	while (itr < end) {
		if (!fn(*itr++)) {
			return 0;
		}
	}
	return 1;
}

size_t wstr_count_if(const wchar_t *beg, const wchar_t *end, wchr_fn fn)
{
	size_t cnt = 0;
	const wchar_t *itr = beg;

	while (itr < end) {
		if (fn(*itr++)) {
			++cnt;
		}
	}
	return cnt;
}
