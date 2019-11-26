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
#include <stdio.h>
#include <ctype.h>

#include "infra-compiler.h"
#include "infra-utility.h"
#include "infra-macros.h"
#include "infra-string.h"


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void chr_assign(char *c1, char c2)
{
	*c1 = c2;
}

static int chr_eq(char c1, char c2)
{
	return c1 == c2;
}
/*
static int chr_lt(char c1, char c2)
{
    return c1 < c2;
}
*/
/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_str_length(const char *s)
{
	return strlen(s);
}

int fnx_str_compare(const char *s1, const char *s2, size_t n)
{
	return memcmp(s1, s2, n);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_str_ncompare(const char *s1, size_t n1,
                     const char *s2, size_t n2)
{
	int res;
	size_t n;

	n   = fnx_min(n1, n2);
	res = fnx_str_compare(s1, s2, n);

	if (res == 0) {
		res = (n1 > n2) - (n1 < n2);
	}

	return res;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const char *fnx_str_find_chr(const char *s, size_t n, char a)
{
	return (const char *)(memchr(s, a, n));
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const char *fnx_str_find(const char *s1, size_t n1,
                         const char *s2, size_t n2)
{
	char c;
	const char *last   = NULL;
	const char *p      = NULL;


	if (n2 && (n1 >= n2)) {
		c = *s2;
		last = s1 + (n1 - n2 + 1);

		for (p = s1; p != last; ++p) {
			if (chr_eq(*p, c) && (fnx_str_compare(p, s2, n2) == 0)) {
				return p;
			}
		}
	}

	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const char *fnx_str_rfind(const char *s1, size_t n1,
                          const char *s2, size_t n2)
{
	char c;
	const char *p = NULL;

	if (n2 && (n1 >= n2)) {
		c = *s2;

		for (p = s1 + (n1 - n2); p >= s1; --p) {
			if (chr_eq(*p, c) && (fnx_str_compare(p, s2, n2) == 0)) {
				return p;
			}
		}
	}

	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const char *fnx_str_rfind_chr(const char *s, size_t n, char c)
{
	const char *p = NULL;

	for (p = s + n; p != s;) {
		if (chr_eq(*--p, c)) {
			return p;
		}
	}

	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const char *fnx_str_find_first_of(const char *s1, size_t n1,
                                  const char *s2, size_t n2)
{
	const char *p;
	const char *last;

	last = s1 + n1;
	for (p = s1; p < last; ++p) {
		if (fnx_str_find_chr(s2, n2, *p) != NULL) {
			return p;
		}
	}

	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const char *fnx_str_find_first_not_of(const char *s1, size_t n1,
                                      const char *s2, size_t n2)
{
	const char *p;
	const char *last;

	last = s1 + n1;
	for (p = s1; p < last; ++p) {
		if (fnx_str_find_chr(s2, n2, *p) == NULL) {
			return p;
		}
	}

	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const char *fnx_str_find_first_not_eq(const char *s,
                                      size_t n, char c)
{
	const char *p;
	const char *last;

	last = s + n;
	for (p = s; p < last; ++p) {
		if (!chr_eq(*p, c)) {
			return p;
		}
	}

	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const char *
fnx_str_find_last_of(const char *s1, size_t n1,
                     const char *s2, size_t n2)
{
	const char *p;
	const char *last;

	last = s1 + n1;
	for (p = last; p > s1;) {
		if (fnx_str_find_chr(s2, n2, *--p) != NULL) {
			return p;
		}
	}

	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const char *
fnx_str_find_last_not_of(const char *s1, size_t n1,
                         const char *s2, size_t n2)
{
	const char *p;
	const char *last;

	last = s1 + n1;
	for (p = last; p > s1;) {
		if (fnx_str_find_chr(s2, n2, *--p) == NULL) {
			return p;
		}
	}

	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const char *fnx_str_find_last_not_eq(const char *s, size_t n, char c)
{
	const char *p;

	for (p = s + n; p > s;) {
		if (!chr_eq(*--p, c)) {
			return p;
		}
	}

	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_str_common_prefix(const char *s1,
                             const char *s2, size_t n)
{
	const char *p  = NULL;
	const char *q  = NULL;
	size_t k = 0;

	for (p = s1, q = s2; k != n; ++p, ++q, ++k) {
		if (!chr_eq(*p, *q)) {
			break;
		}
	}

	return k;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_str_common_suffix(const char *s1,
                             const char *s2, size_t n)
{
	const char *p  = NULL;
	const char *q  = NULL;
	size_t k = 0;

	for (p = s1 + n, q = s2 + n; k != n; ++k) {
		--p;
		--q;

		if (!chr_eq(*p, *q)) {
			break;
		}
	}

	return k;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_str_overlaps(const char *s1, size_t n1,
                        const char *s2, size_t n2)
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

void fnx_str_terminate(char *s, size_t n)
{
	chr_assign(s + n, '\0');
}


void fnx_str_fill(char *s, size_t n, char c)
{
	memset(s, c, n);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void str_copy(char *s1, const char *s2, size_t n)
{
	memcpy(s1, s2, n);
}

static void str_move(char *s1, const char *s2, size_t n)
{
	memmove(s1, s2, n);
}

void fnx_str_copy(char *p, const char *s, size_t n)
{
	size_t d;

	if (n != 0) {
		d = (size_t)((p > s) ? p - s : s - p);
		if (d != 0) {
			if (fnx_likely(n < d)) {
				str_copy(p, s, n);
			} else {
				str_move(p, s, n); /* overlap */
			}
		}
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_str_reverse(char *s, size_t n)
{
	char c;
	char *p, *q;

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
str_insert_no_overlap(char *p, size_t sz, size_t n1,
                      const char *s, size_t n2)
{
	size_t k, m;

	k = fnx_min(n2, sz);
	m = fnx_min(n1, sz - k);
	fnx_str_copy(p + k, p, m);
	fnx_str_copy(p, s, k);

	return k + m;
}

/*
 * Insert where source and destination may overlap. Using local buffer for
 * safe copy -- avoid dynamic allocation, even at the price of performance
 */
static size_t
str_insert_with_overlap(char *p, size_t sz, size_t n1,
                        const char *s, size_t n2)
{

	size_t n, k, j;
	const char *end;
	char buf[512];

	end = s + fnx_min(n2, sz);
	n   = n1;
	j   = (size_t)(end - s);

	while (j > 0) {
		k = fnx_min(j, FNX_ARRAYSIZE(buf));
		fnx_str_copy(buf, end - k, k);
		n = str_insert_no_overlap(p, sz, n, buf, k);

		j -= k;
	}

	return n;
}

size_t fnx_str_insert(char *p, size_t sz, size_t n1,
                      const char *s, size_t n2)
{
	size_t k, n = 0;

	if (n2 >= sz) {
		n = sz;
		fnx_str_copy(p, s, n);
	} else {
		k = fnx_str_overlaps(p, sz, s, n2);
		if (k > 0) {
			n = str_insert_with_overlap(p, sz, n1, s, n2);
		} else {
			n = str_insert_no_overlap(p, sz, n1, s, n2);
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
size_t fnx_str_insert_chr(char *p, size_t sz, size_t n1,
                          size_t n2, char c)
{
	size_t k, m;

	k = fnx_min(n2, sz);

	m = fnx_min(n1, sz - k);
	fnx_str_copy(p + k, p, m);
	fnx_str_fill(p, k, c);

	return k + m;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_str_replace(char *p, size_t sz, size_t len, size_t n1,
                       const char *s, size_t n2)
{
	size_t k, m;

	if (n1 < n2) {
		/*
		 * Case 1: Need to extend existing string. We assume that s may overlap
		 * p and try to do our best...
		 */
		if (s < p) {
			k = n1;
			m = fnx_str_insert(p + k, sz - k, len - k, s + k, n2 - k);
			fnx_str_copy(p, s, k);
		} else {
			k = n1;
			fnx_str_copy(p, s, n1);
			m = fnx_str_insert(p + k, sz - k, len - k, s + k, n2 - k);
		}
	} else {
		/*
		 * Case 2: No need to worry about extra space; just copy s to the
		 * beginning of buffer and adjust size, then move the tail of the
		 * string backwards.
		 */
		k = n2;
		fnx_str_copy(p, s, k);

		m = len - n1;
		fnx_str_copy(p + k, p + n1, m);
	}

	return k + m;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_str_replace_chr(char *p, size_t sz, size_t len,
                           size_t n1, size_t n2, char c)
{
	size_t k, m;

	if (n1 < n2) {
		/* Case 1: First fill n1 characters, then insert the rest. */
		k = n1;
		fnx_str_fill(p, k, c);
		m = fnx_str_insert_chr(p + k, sz - k, len - k, n2 - k, c);
	} else {
		/* Case 2: No need to worry about extra space; just fill n2 characters
		    in the beginning of buffer. */
		k = n2;
		fnx_str_fill(p, k, c);

		/* Move the tail of the string backwards. */
		m = len - n1;
		fnx_str_copy(p + k, p + n1, m);
	}
	return k + m;
}

/*: : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : :*/
/*
 * Wrappers over standard ctypes functions (macros?).
 */
int fnx_chr_isalnum(char c)
{
	return isalnum(c);
}

int fnx_chr_isalpha(char c)
{
	return isalpha(c);
}

int fnx_chr_isascii(char c)
{
	return isascii(c);
}

int fnx_chr_isblank(char c)
{
	return isblank(c);
}

int fnx_chr_iscntrl(char c)
{
	return iscntrl(c);
}

int fnx_chr_isdigit(char c)
{
	return isdigit(c);
}

int fnx_chr_isgraph(char c)
{
	return isgraph(c);
}

int fnx_chr_islower(char c)
{
	return islower(c);
}

int fnx_chr_isprint(char c)
{
	return isprint(c);
}

int fnx_chr_ispunct(char c)
{
	return ispunct(c);
}

int fnx_chr_isspace(char c)
{
	return isspace(c);
}

int fnx_chr_isupper(char c)
{
	return isupper(c);
}

int fnx_chr_isxdigit(char c)
{
	return isxdigit(c);
}

int fnx_chr_toupper(char c)
{
	return toupper(c);
}

int fnx_chr_tolower(char c)
{
	return tolower(c);
}


