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

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "infra.h"
#include "wstrchr.h"
#include "wstrbuf.h"

static void wchr_assign(wchar_t *c1, wchar_t c2)
{
	*c1 = c2;
}

void wstr_terminate(wchar_t *s, size_t n)
{
	wchr_assign(s + n, L'\0');
}


void wstr_fill(wchar_t *s, size_t n, wchar_t c)
{
	wmemset(s, c, n);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void wstr_memcopy(wchar_t *s1, const wchar_t *s2, size_t n)
{
	wmemcpy(s1, s2, n);
}

static void wstr_memmove(wchar_t *s1, const wchar_t *s2, size_t n)
{
	wmemmove(s1, s2, n);
}

void wstr_copy(wchar_t *p, const wchar_t *s, size_t n)
{
	size_t d;

	if (n != 0) {
		d = (size_t)((p > s) ? p - s : s - p);
		if (d != 0) {
			if (cac_likely(n < d)) {
				wstr_memcopy(p, s, n);
			} else {
				wstr_memmove(p, s, n); /* overlap */
			}
		}
	}
}

size_t wstr_ncopy(wchar_t *s1, size_t n1, const wchar_t *s2, size_t n2)
{
	const size_t n = cac_min(n2, n1);

	wstr_copy(s1, s2, n);
	if (n < n1) {
		wstr_terminate(s1, n);
	}
	return n;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void wstr_reverse(wchar_t *s, size_t n)
{
	wchar_t c;
	wchar_t *p, *q;

	for (p = s, q = s + n - 1; p < q; ++p, --q) {
		c  = *p;
		*p = *q;
		*q = c;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Insert where there is no overlap between source and destination. Tries to
 * insert as many characters as possible, but without overflow.
 *
 * Makes room at the beginning of the buffer: move the current string m steps
 * forward, and then inserts s to the beginning of buffer.
 */
static size_t
str_insert_no_overlap(wchar_t *buf, size_t sz, size_t n1,
                      const wchar_t *s, size_t n2)
{
	size_t k, m;

	k = cac_min(n2, sz);
	m = cac_min(n1, sz - k);
	wstr_copy(buf + k, buf, m);
	wstr_copy(buf, s, k);

	return k + m;
}

/*
 * Insert where source and destination may overlap. Using local buffer for
 * safe copy -- avoid dynamic allocation, even at the price of performance
 */
static size_t
str_insert_with_overlap(wchar_t *buf, size_t sz, size_t n1,
                        const wchar_t *s, size_t n2)
{

	size_t n, k, j;
	const wchar_t *end;
	wchar_t sbuf[512];

	end = s + cac_min(n2, sz);
	n   = n1;
	j   = (size_t)(end - s);

	while (j > 0) {
		k = cac_min(j, CAC_ARRAYSIZE(sbuf));
		wstr_copy(sbuf, end - k, k);
		n = str_insert_no_overlap(buf, sz, n, sbuf, k);

		j -= k;
	}

	return n;
}

size_t wstr_insert(wchar_t *buf, size_t sz, size_t n1,
                   const wchar_t *s, size_t n2)
{
	size_t k, n = 0;

	if (n2 >= sz) {
		n = sz;
		wstr_copy(buf, s, n);
	} else {
		k = wstr_overlaps(buf, sz, s, n2);
		if (k > 0) {
			n = str_insert_with_overlap(buf, sz, n1, s, n2);
		} else {
			n = str_insert_no_overlap(buf, sz, n1, s, n2);
		}
	}

	return n;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t wstr_insert_sub(wchar_t *buf, size_t sz, size_t wr,
                       size_t pos, const wchar_t *s, size_t n)
{
	size_t j, k, rem, m;

	/* Trivial protection */
	if (wr == 0) {
		return 0;
	}

	/* Start insertion before position j */
	j   = cac_min(pos, sz);

	/* Number of writable elements after j. */
	rem = (j < wr) ? (wr - j) : 0;

	/* Number of elements of p after j to be moved forward */
	k   = sz - j;

	/*
	 * Insert n elements of s into p: try as many as possible, truncate tail in
	 * case of insufficient buffer capacity.
	 */
	m = wstr_insert(buf + j, rem, k, s, n);
	return j + m;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t wstr_insert_chr(wchar_t *buf, size_t sz, size_t n1,
                       size_t n2, wchar_t c)
{
	size_t k, m;

	k = cac_min(n2, sz);

	m = cac_min(n1, sz - k);
	wstr_copy(buf + k, buf, m);
	wstr_fill(buf, k, c);

	return k + m;
}

size_t wstr_insert_fill(wchar_t *buf, size_t sz, size_t wr,
                        size_t pos, size_t n, wchar_t c)
{
	size_t j, k, rem, m;

	/* Trivial protection */
	if (wr == 0) {
		return 0;
	}

	/* Start insertion before position j. */
	j   = cac_min(pos, sz);

	/* Number of writable elements after j. */
	rem = (j < wr) ? (wr - j) : 0;

	/* Number of elements of p after j to be moved forward */
	k   = sz - j;

	/*
	 * Insert n copies of c: try as many as possible; truncate tail in case of
	 * insufficient buffer capacity
	 */
	m = wstr_insert_chr(buf + j, rem, k, n, c);
	return j + m;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t wstr_replace(wchar_t *buf, size_t sz, size_t len, size_t n1,
                    const wchar_t *s, size_t n2)
{
	size_t k, m;

	if (n1 < n2) {
		/*
		 * Case 1: Need to extend existing string. We assume that s may overlap
		 * p and try to do our best...
		 */
		if (s < buf) {
			k = n1;
			m = wstr_insert(buf + k, sz - k, len - k, s + k, n2 - k);
			wstr_copy(buf, s, k);
		} else {
			k = n1;
			wstr_copy(buf, s, n1);
			m = wstr_insert(buf + k, sz - k, len - k, s + k, n2 - k);
		}
	} else {
		/*
		 * Case 2: No need to worry about extra space; just copy s to the
		 * beginning of buffer and adjust size, then move the tail of the
		 * string backwards.
		 */
		k = n2;
		wstr_copy(buf, s, k);

		m = len - n1;
		wstr_copy(buf + k, buf + n1, m);
	}

	return k + m;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t wstr_replace_sub(wchar_t *buf, size_t sz, size_t wr,
                        size_t pos, size_t cnt, const wchar_t *s, size_t n)
{
	size_t r, m;

	/* Trivial protection */
	if (wr == 0) {
		return 0;
	}

	/* Number of elements to replace */
	r = ((pos < sz) ? cac_min(sz - pos, cnt) : 0);

	/*
	 * Replace r elements after pos with s. Truncates tail in case of
	 * insufficient buffer capacity.
	 */
	m = wstr_replace(buf + pos, wr - pos, sz - pos, r, s, n);

	return pos + m;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t wstr_replace_chr(wchar_t *buf, size_t sz, size_t len,
                        size_t n1, size_t n2, wchar_t c)
{
	size_t k, m;

	if (n1 < n2) {
		/* Case 1: First fill n1 characters, then insert the rest. */
		k = n1;
		wstr_fill(buf, k, c);
		m = wstr_insert_chr(buf + k, sz - k, len - k, n2 - k, c);
	} else {
		/*
		 * Case 2: No need to worry about extra space; just fill n2 characters
		 * in the beginning of buffer
		 */
		k = n2;
		wstr_fill(buf, k, c);

		/* Move the tail of the string backwards. */
		m = len - n1;
		wstr_copy(buf + k, buf + n1, m);
	}
	return k + m;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t wstr_replace_fill(wchar_t *buf, size_t sz, size_t wr,
                         size_t pos, size_t cnt, size_t n, wchar_t c)
{
	size_t r, m;

	/* Trivial protection */
	if (wr == 0) {
		return 0;
	}

	/* Number of elements to replace */
	r = ((pos < sz) ? cac_min(sz - pos, cnt) : 0);

	/*
	 * Replace r elements after pos with n copies of c. Truncates tail in case
	 * of insufficient buffer capacity
	 */
	m = wstr_replace_chr(buf + pos, wr - pos, sz - pos, r, n, c);

	return pos + m;
}
