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
#include <wctype.h>
#include <wchar.h>

#include "infra-compiler.h"
#include "infra-utility.h"
#include "infra-macros.h"
#include "infra-wstring.h"


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void wchr_assign(wchar_t *c1, wchar_t c2)
{
	*c1 = c2;
}

static int wchr_eq(wchar_t c1, wchar_t c2)
{
	return c1 == c2;
}
/*
static int wchr_lt(wchar_t c1, wchar_t c2)
{
    return c1 < c2;
}
*/
/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_wstr_length(const wchar_t *s)
{
	return wcslen(s);
}

int fnx_wstr_compare(const wchar_t *s1, const wchar_t *s2, size_t n)
{
	return wmemcmp(s1, s2, n);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_wstr_ncompare(const wchar_t *s1, size_t n1,
                      const wchar_t *s2, size_t n2)
{
	int res;
	size_t n;

	n   = fnx_min(n1, n2);
	res = fnx_wstr_compare(s1, s2, n);

	if (res == 0) {
		res = (n1 > n2) - (n1 < n2);
	}

	return res;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const wchar_t *fnx_wstr_find_chr(const wchar_t *s, size_t n, wchar_t a)
{
	return (const wchar_t *)(wmemchr(s, a, n));
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const wchar_t *fnx_wstr_find(const wchar_t *s1, size_t n1,
                             const wchar_t *s2, size_t n2)
{
	wchar_t c;
	const wchar_t *last   = NULL;
	const wchar_t *p      = NULL;


	if (n2 && (n1 >= n2)) {
		c = *s2;
		last = s1 + (n1 - n2 + 1);

		for (p = s1; p != last; ++p) {
			if (wchr_eq(*p, c) && (fnx_wstr_compare(p, s2, n2) == 0)) {
				return p;
			}
		}
	}

	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const wchar_t *fnx_wstr_rfind(const wchar_t *s1, size_t n1,
                              const wchar_t *s2, size_t n2)
{
	wchar_t c;
	const wchar_t *p = NULL;

	if (n2 && (n1 >= n2)) {
		c = *s2;

		for (p = s1 + (n1 - n2); p >= s1; --p) {
			if (wchr_eq(*p, c) && (fnx_wstr_compare(p, s2, n2) == 0)) {
				return p;
			}
		}
	}

	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const wchar_t *fnx_wstr_rfind_chr(const wchar_t *s, size_t n, wchar_t c)
{
	const wchar_t *p = NULL;

	for (p = s + n; p != s;) {
		if (wchr_eq(*--p, c)) {
			return p;
		}
	}

	return NULL;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const wchar_t *fnx_wstr_find_first_of(const wchar_t *s1, size_t n1,
                                      const wchar_t *s2, size_t n2)
{
	const wchar_t *p;
	const wchar_t *last;

	last = s1 + n1;
	for (p = s1; p < last; ++p) {
		if (fnx_wstr_find_chr(s2, n2, *p) != NULL) {
			return p;
		}
	}

	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const wchar_t *fnx_wstr_find_first_not_of(const wchar_t *s1, size_t n1,
        const wchar_t *s2, size_t n2)
{
	const wchar_t *p;
	const wchar_t *last;

	last = s1 + n1;
	for (p = s1; p < last; ++p) {
		if (fnx_wstr_find_chr(s2, n2, *p) == NULL) {
			return p;
		}
	}

	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const wchar_t *fnx_wstr_find_first_not_eq(const wchar_t *s,
        size_t n, wchar_t c)
{
	const wchar_t *p;
	const wchar_t *last;

	last = s + n;
	for (p = s; p < last; ++p) {
		if (!wchr_eq(*p, c)) {
			return p;
		}
	}

	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const wchar_t *
fnx_wstr_find_last_of(const wchar_t *s1, size_t n1,
                      const wchar_t *s2, size_t n2)
{
	const wchar_t *p;
	const wchar_t *last;

	last = s1 + n1;
	for (p = last; p > s1;) {
		if (fnx_wstr_find_chr(s2, n2, *--p) != NULL) {
			return p;
		}
	}

	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const wchar_t *
fnx_wstr_find_last_not_of(const wchar_t *s1, size_t n1,
                          const wchar_t *s2, size_t n2)
{
	const wchar_t *p;
	const wchar_t *last;

	last = s1 + n1;
	for (p = last; p > s1;) {
		if (fnx_wstr_find_chr(s2, n2, *--p) == NULL) {
			return p;
		}
	}

	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const wchar_t *
fnx_wstr_find_last_not_eq(const wchar_t *s, size_t n, wchar_t c)
{
	const wchar_t *p;

	for (p = s + n; p > s;) {
		if (!wchr_eq(*--p, c)) {
			return p;
		}
	}

	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_wstr_common_prefix(const wchar_t *s1,
                              const wchar_t *s2, size_t n)
{
	const wchar_t *p  = NULL;
	const wchar_t *q  = NULL;
	size_t k = 0;

	for (p = s1, q = s2; k != n; ++p, ++q, ++k) {
		if (!wchr_eq(*p, *q)) {
			break;
		}
	}

	return k;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_wstr_common_suffix(const wchar_t *s1,
                              const wchar_t *s2, size_t n)
{
	const wchar_t *p  = NULL;
	const wchar_t *q  = NULL;
	size_t k = 0;

	for (p = s1 + n, q = s2 + n; k != n; ++k) {
		--p;
		--q;

		if (!wchr_eq(*p, *q)) {
			break;
		}
	}

	return k;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_wstr_overlaps(const wchar_t *s1, size_t n1,
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

/*: : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : :*/

void fnx_wstr_terminate(wchar_t *s, size_t n)
{
	wchr_assign(s + n, '\0');
}


void fnx_wstr_fill(wchar_t *s, size_t n, wchar_t c)
{
	wmemset(s, c, n);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void wstr_copy(wchar_t *s1, const wchar_t *s2, size_t n)
{
	wmemcpy(s1, s2, n);
}

static void wstr_move(wchar_t *s1, const wchar_t *s2, size_t n)
{
	wmemmove(s1, s2, n);
}

void fnx_wstr_copy(wchar_t *p, const wchar_t *s, size_t n)
{
	size_t d;

	if (n != 0) {
		d = (size_t)((p > s) ? p - s : s - p);
		if (d != 0) {
			if (fnx_likely(n < d)) {
				wstr_copy(p, s, n);
			} else {
				wstr_move(p, s, n); /* overlap */
			}
		}
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_wstr_reverse(wchar_t *s, size_t n)
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
wstr_insert_no_overlap(wchar_t *p, size_t sz, size_t n1,
                       const wchar_t *s, size_t n2)
{
	size_t k, m;

	k = fnx_min(n2, sz);
	m = fnx_min(n1, sz - k);
	fnx_wstr_copy(p + k, p, m);
	fnx_wstr_copy(p, s, k);

	return k + m;
}

/*
 * Insert where source and destination may overlap. Using local buffer for
 * safe copy -- avoid dynamic allocation, even at the price of performance
 */
static size_t
wstr_insert_with_overlap(wchar_t *p, size_t sz, size_t n1,
                         const wchar_t *s, size_t n2)
{

	size_t n, k, j;
	const wchar_t *end;
	wchar_t buf[256];

	end = s + fnx_min(n2, sz);
	n   = n1;
	j   = (size_t)(end - s);

	while (j > 0) {
		k = fnx_min(j, FNX_ARRAYSIZE(buf));
		fnx_wstr_copy(buf, end - k, k);
		n = wstr_insert_no_overlap(p, sz, n, buf, k);

		j -= k;
	}

	return n;
}

size_t fnx_wstr_insert(wchar_t *p, size_t sz, size_t n1,
                       const wchar_t *s, size_t n2)
{
	size_t k, n = 0;

	if (n2 >= sz) {
		n = sz;
		fnx_wstr_copy(p, s, n);
	} else {
		k = fnx_wstr_overlaps(p, sz, s, n2);
		if (k > 0) {
			n = wstr_insert_with_overlap(p, sz, n1, s, n2);
		} else {
			n = wstr_insert_no_overlap(p, sz, n1, s, n2);
		}
	}

	return n;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Inserts n2 copies of c to the front of p. Tries to insert as many characters
 * as possible, but does not insert more then available writable characters
 * in the buffer.
 *
 * Makes room at the beginning of the buffer: move the current string m steps
 * forward, then fill k c-characters into p.
 *
 * p   Target buffer
 * sz  Size of buffer: number of writable elements after p.
 * n1  Number of chars already in p (must be less or equal to sz)
 * n2  Number of copies of c to insert.
 * c   Fill character.
 *
 * Returns the number of characters in p after insertion (always less or equal
 * to sz).
 */
size_t fnx_wstr_insert_chr(wchar_t *p, size_t sz, size_t n1,
                           size_t n2, wchar_t c)
{
	size_t k, m;

	k = fnx_min(n2, sz);

	m = fnx_min(n1, sz - k);
	fnx_wstr_copy(p + k, p, m);
	fnx_wstr_fill(p, k, c);

	return k + m;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_wstr_replace(wchar_t *p, size_t sz, size_t len, size_t n1,
                        const wchar_t *s, size_t n2)
{
	size_t k, m;

	if (n1 < n2) {
		/*
		 * Case 1: Need to extend existing string. We assume that s may overlap
		 * p and try to do our best...
		 */
		if (s < p) {
			k = n1;
			m = fnx_wstr_insert(p + k, sz - k, len - k, s + k, n2 - k);
			fnx_wstr_copy(p, s, k);
		} else {
			k = n1;
			fnx_wstr_copy(p, s, n1);
			m = fnx_wstr_insert(p + k, sz - k, len - k, s + k, n2 - k);
		}
	} else {
		/*
		 * Case 2: No need to worry about extra space; just copy s to the
		 * beginning of buffer and adjust size, then move the tail of the
		 * string backwards.
		 */
		k = n2;
		fnx_wstr_copy(p, s, k);

		m = len - n1;
		fnx_wstr_copy(p + k, p + n1, m);
	}

	return k + m;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_wstr_replace_chr(wchar_t *p, size_t sz, size_t len,
                            size_t n1, size_t n2, wchar_t c)
{
	size_t k, m;

	if (n1 < n2) {
		/* Case 1: First fill n1 characters, then insert the rest. */
		k = n1;
		fnx_wstr_fill(p, k, c);
		m = fnx_wstr_insert_chr(p + k, sz - k, len - k, n2 - k, c);
	} else {
		/* Case 2: No need to worry about extra space; just fill n2 characters
		    in the beginning of buffer. */
		k = n2;
		fnx_wstr_fill(p, k, c);

		/* Move the tail of the string backwards. */
		m = len - n1;
		fnx_wstr_copy(p + k, p + n1, m);
	}
	return k + m;
}

/*: : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : :*/
/*
 * Wrappers over standard ctypes functions (macros?).
 */
int fnx_wchr_isalnum(wchar_t c)
{
	return iswalnum((wint_t)c);
}

int fnx_wchr_isalpha(wchar_t c)
{
	return iswalpha((wint_t)c);
}

int fnx_wchr_isblank(wchar_t c)
{
	return iswblank((wint_t)c);
}

int fnx_wchr_iscntrl(wchar_t c)
{
	return iswcntrl((wint_t)c);
}

int fnx_wchr_isdigit(wchar_t c)
{
	return iswdigit((wint_t)c);
}

int fnx_wchr_isgraph(wchar_t c)
{
	return iswgraph((wint_t)c);
}

int fnx_wchr_islower(wchar_t c)
{
	return iswlower((wint_t)c);
}

int fnx_wchr_isprint(wchar_t c)
{
	return iswprint((wint_t)c);
}

int fnx_wchr_ispunct(wchar_t c)
{
	return iswpunct((wint_t)c);
}

int fnx_wchr_isspace(wchar_t c)
{
	return iswspace((wint_t)c);
}

int fnx_wchr_isupper(wchar_t c)
{
	return iswupper((wint_t)c);
}

int fnx_wchr_isxdigit(wchar_t c)
{
	return iswxdigit((wint_t)c);
}


wchar_t fnx_wchr_toupper(wchar_t c)
{
	return (wchar_t)towupper((wint_t)c);
}

wchar_t fnx_wchr_tolower(wchar_t c)
{
	return (wchar_t)towlower((wint_t)c);
}

