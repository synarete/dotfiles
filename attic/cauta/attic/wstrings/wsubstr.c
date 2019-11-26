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

#include "infra.h"
#include "ctypes.h"
#include "wstrchr.h"
#include "wstrbuf.h"
#include "wsubstr.h"


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t wsubstr_max_size(void)
{
	return (size_t)(-1) / 2;
}

size_t wsubstr_npos(void)
{
	return wsubstr_max_size();
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int wsubstr_isinrange(const wsubstr_t *ss, const wchar_t *p)
{
	return ((p >= ss->str) && (p < (ss->str + ss->len)));
}

size_t wsubstr_offset(const wsubstr_t *ss, const wchar_t *p)
{
	size_t off;

	if (wsubstr_isinrange(ss, p)) {
		off = (size_t)(p - ss->str);
	} else {
		off = wsubstr_npos();
	}
	return off;
}

void wsubstr_init(wsubstr_t *ss, const wchar_t *s)
{
	wsubstr_ninit(ss, s, wstr_length(s));
}

void wsubstr_ninit(wsubstr_t *ss, const wchar_t *s, size_t n)
{
	ss->str = s;
	ss->len = n;
}

void wsubstr_clone(const wsubstr_t *ss, wsubstr_t *other)
{
	other->str = ss->str;
	other->len = ss->len;
}

void wsubstr_destroy(wsubstr_t *ss)
{
	ss->str  = NULL;
	ss->len  = 0;
}

static int wsubstr_issame(const wsubstr_t *ss, const wchar_t *s, size_t n)
{
	return ((ss->str == s) && (ss->len == n));
}

const wchar_t *wsubstr_data(const wsubstr_t *ss)
{
	return ss->str;
}

size_t wsubstr_size(const wsubstr_t *ss)
{
	return ss->len;
}

int wsubstr_isempty(const wsubstr_t *ss)
{
	return (ss->len == 0);
}

const wchar_t *wsubstr_begin(const wsubstr_t *ss)
{
	return wsubstr_data(ss);
}

const wchar_t *wsubstr_end(const wsubstr_t *ss)
{
	return (ss->str + ss->len);
}

const wchar_t *wsubstr_at(const wsubstr_t *ss, size_t n)
{
	return ((n < ss->len) ? (ss->str + n) : NULL);
}

int wsubstr_isvalid_index(const wsubstr_t *ss, size_t i)
{
	return (i < ss->len);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t wsubstr_copyto(const wsubstr_t *ss, wchar_t *s, size_t n)
{
	return wstr_ncopy(s, n, ss->str, ss->len);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int wsubstr_compare(const wsubstr_t *ss, const wchar_t *s)
{
	return wsubstr_ncompare(ss, s, wstr_length(s));
}

int wsubstr_ncompare(const wsubstr_t *ss, const wchar_t *s, size_t n)
{
	return (wsubstr_issame(ss, s, n) ? 0 :
	        wstr_ncompare(ss->str, ss->len, s, n));
}

int wsubstr_isequal(const wsubstr_t *ss, const wchar_t *s)
{
	return wsubstr_nisequal(ss, s, wstr_length(s));
}

int wsubstr_isequals(const wsubstr_t *ss1, const wsubstr_t *ss2)
{
	return wsubstr_nisequal(ss1, ss2->str, ss2->len);
}

int wsubstr_nisequal(const wsubstr_t *ss, const wchar_t *s, size_t n)
{
	return (wsubstr_issame(ss, s, n) ||
	        ((ss->len == n) && !wstr_compare(ss->str, s, n)));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t wsubstr_count(const wsubstr_t *ss, const wchar_t *s)
{
	return wsubstr_ncount(ss, s, wstr_length(s));
}

size_t wsubstr_ncount(const wsubstr_t *ss, const wchar_t *s, size_t n)
{
	return wstr_count(ss->str, ss->len, s, n);
}

size_t wsubstr_count_chr(const wsubstr_t *ss, wchar_t c)
{
	return wstr_count_chr(ss->str, ss->len, c);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t wsubstr_find(const wsubstr_t *ss, const wchar_t *s)
{
	return wsubstr_nfind(ss, 0UL, s, wstr_length(s));
}

size_t wsubstr_nfind(const wsubstr_t *ss,
                     size_t pos, const wchar_t *s, size_t n)
{
	const wchar_t *p = NULL;

	if (pos < ss->len) {
		if (n > 0) {
			p = wstr_find(ss->str + pos, ss->len - pos, s, n);
		} else {
			p = ss->str + pos;
		}
	}
	return wsubstr_offset(ss, p);
}

size_t wsubstr_find_chr(const wsubstr_t *ss, size_t pos, wchar_t c)
{
	const wchar_t *p = NULL;

	if (pos < ss->len) {
		p = wstr_find_chr(ss->str + pos, ss->len - pos, c);
	}
	return wsubstr_offset(ss, p);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t wsubstr_rfind(const wsubstr_t *ss, const wchar_t *s)
{
	return wsubstr_nrfind(ss, ss->len, s, wstr_length(s));
}

size_t wsubstr_nrfind(const wsubstr_t *ss,
                      size_t pos, const wchar_t *s, size_t n)
{
	size_t k;
	const wchar_t *p;

	k = (pos < ss->len) ? pos + 1 : ss->len;
	if (n > 0) {
		p = wstr_rfind(ss->str, k, s, n);
	} else {
		p = ss->str + k;
	}
	return wsubstr_offset(ss, p);
}

size_t wsubstr_rfind_chr(const wsubstr_t *ss, size_t pos, wchar_t c)
{
	size_t k;
	const wchar_t *p;

	k = (pos < ss->len) ? pos + 1 : ss->len;
	p = wstr_rfind_chr(ss->str, k, c);
	return wsubstr_offset(ss, p);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t wsubstr_find_first_of(const wsubstr_t *ss, const wchar_t *s)
{
	return wsubstr_nfind_first_of(ss, 0UL, s, wstr_length(s));
}

size_t wsubstr_nfind_first_of(const wsubstr_t *ss,
                              size_t pos, const wchar_t *s, size_t n)
{
	const wchar_t *p = NULL;

	if ((n != 0) && (pos < ss->len)) {
		p = wstr_find_first_of(ss->str + pos, ss->len - pos, s, n);
	}
	return wsubstr_offset(ss, p);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t wsubstr_find_last_of(const wsubstr_t *ss, const wchar_t *s)
{
	return wsubstr_nfind_last_of(ss, ss->len, s, wstr_length(s));
}

size_t wsubstr_nfind_last_of(const wsubstr_t *ss, size_t pos,
                             const wchar_t *s, size_t n)
{
	size_t k;
	const wchar_t *p;

	k = (pos < ss->len) ? pos + 1 : ss->len;
	p = wstr_find_last_of(ss->str, k, s, n);
	return wsubstr_offset(ss, p);
}

size_t wsubstr_find_first_not_of(const wsubstr_t *ss,
                                 const wchar_t *s)
{
	return wsubstr_nfind_first_not_of(ss, 0UL, s, wstr_length(s));
}

size_t wsubstr_nfind_first_not_of(const wsubstr_t *ss,
                                  size_t pos, const wchar_t *s, size_t n)
{
	const wchar_t *p = NULL;

	if (pos < ss->len) {
		if (n > 0) {
			p = wstr_find_first_not_of(ss->str + pos, ss->len - pos, s, n);
		} else {
			p = ss->str + pos;
		}
	}
	return wsubstr_offset(ss, p);
}

size_t wsubstr_find_first_not(const wsubstr_t *ss,
                              size_t pos, wchar_t c)
{
	const wchar_t *p = NULL;

	if (pos < ss->len) {
		p = wstr_find_first_not_eq(ss->str + pos, ss->len - pos, c);
	}
	return wsubstr_offset(ss, p);
}

size_t wsubstr_find_last_not_of(const wsubstr_t *ss, const wchar_t *s)
{
	return wsubstr_nfind_last_not_of(ss, ss->len, s, wstr_length(s));
}

size_t wsubstr_nfind_last_not_of(const wsubstr_t *ss,
                                 size_t pos, const wchar_t *s, size_t n)
{
	size_t k;
	const wchar_t *p = NULL;

	k = (pos < ss->len) ? pos + 1 : ss->len;
	if (n > 0) {
		p = wstr_find_last_not_of(ss->str, k, s, n);
	} else {
		p = ss->str + k - 1;
	}
	return wsubstr_offset(ss, p);
}

size_t wsubstr_find_last_not(const wsubstr_t *ss,
                             size_t pos, wchar_t c)
{
	size_t k;
	const wchar_t *p;

	k = (pos < ss->len) ? pos + 1 : ss->len;
	p = wstr_find_last_not_eq(ss->str, k, c);
	return wsubstr_offset(ss, p);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void wsubstr_sub(const wsubstr_t *ss,
                 size_t i, size_t n, wsubstr_t *out)
{
	const size_t j = cac_min(i, ss->len);
	const size_t k = cac_min(n, ss->len - j);

	wsubstr_ninit(out, ss->str + j, k);
}

void wsubstr_rsub(const wsubstr_t *ss,
                  size_t n, wsubstr_t *out)
{
	const size_t k = cac_min(n, ss->len);
	const size_t j = ss->len - k;

	wsubstr_ninit(out, ss->str + j, k);
}

void wsubstr_intersection(const wsubstr_t *s1,
                          const wsubstr_t *s2,
                          wsubstr_t *out)
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

		/* Case 1:  [.s1...)  [..s2.....) -- Returns empty wsubstring. */
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
		wsubstr_sub(s2, i, n, out);
	} else {
		/* One step recursion -- its ok */
		wsubstr_intersection(s2, s1, out);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Helper function to create split-of-wsubstrings */
static void wsubstr_make_split_pair(const wsubstr_t *ss,
                                    size_t i1, size_t n1,
                                    size_t i2, size_t n2,
                                    wsubstr_pair_t *out)
{
	wsubstr_sub(ss, i1, n1, &out->first);
	wsubstr_sub(ss, i2, n2, &out->second);
}

void wsubstr_split(const wsubstr_t *ss,
                   const wchar_t *s, wsubstr_pair_t *out)
{

	wsubstr_nsplit(ss, s, wstr_length(s), out);
}

void wsubstr_nsplit(const wsubstr_t *ss,
                    const wchar_t *s, size_t n,
                    wsubstr_pair_t *out)
{
	size_t i, j;

	i = wsubstr_nfind_first_of(ss, 0UL, s, n);
	if (i < ss->len) {
		j = wsubstr_nfind_first_not_of(ss, i, s, n);
	} else {
		j = ss->len;
	}
	wsubstr_make_split_pair(ss, 0UL, i, j, ss->len, out);
}

void wsubstr_split_chr(const wsubstr_t *ss, wchar_t sep,
                       wsubstr_pair_t *out)
{
	size_t i, j;

	i = wsubstr_find_chr(ss, 0UL, sep);
	j = (i < ss->len) ? i + 1 : ss->len;
	wsubstr_make_split_pair(ss, 0UL, i, j, ss->len, out);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void wsubstr_rsplit(const wsubstr_t *ss,
                    const wchar_t *s, wsubstr_pair_t *out)
{

	wsubstr_nrsplit(ss, s, wstr_length(s), out);
}

void wsubstr_nrsplit(const wsubstr_t *ss,
                     const wchar_t *s, size_t n,
                     wsubstr_pair_t *out)
{
	size_t i, j;

	i  = wsubstr_nfind_last_of(ss, ss->len, s, n);
	if (i < ss->len) {
		j = wsubstr_nfind_last_not_of(ss, i, s, n);
		if (j < ss->len) {
			++i;
			++j;
		} else {
			i = j = ss->len;
		}
	} else {
		j = ss->len;
	}
	wsubstr_make_split_pair(ss, 0UL, j, i, ss->len, out);
}

void wsubstr_rsplit_chr(const wsubstr_t *ss, wchar_t sep,
                        wsubstr_pair_t *out)
{
	size_t i, j;

	i = wsubstr_rfind_chr(ss, ss->len, sep);
	j = (i < ss->len) ? i + 1 : ss->len;
	wsubstr_make_split_pair(ss, 0UL, i, j, ss->len, out);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void wsubstr_trim(const wsubstr_t *ss, size_t n,
                  wsubstr_t *out)
{
	wsubstr_sub(ss, n, ss->len, out);
}

void wsubstr_trim_any_of(const wsubstr_t *ss,
                         const wchar_t *set, wsubstr_t *out)
{
	wsubstr_ntrim_any_of(ss, set, wstr_length(set), out);
}

void wsubstr_ntrim_any_of(const wsubstr_t *ss,
                          const wchar_t *s, size_t n,
                          wsubstr_t *out)
{
	size_t i;

	i = wsubstr_nfind_first_not_of(ss, 0UL, s, n);
	wsubstr_sub(ss, i, ss->len, out);
}

void wsubstr_trim_chr(const wsubstr_t *ss, wchar_t c,
                      wsubstr_t *out)
{
	size_t i;

	i = wsubstr_find_first_not(ss, 0UL, c);
	wsubstr_sub(ss, i, ss->len, out);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void wsubstr_chop(const wsubstr_t *ss,
                  size_t n, wsubstr_t *out)
{
	const size_t k = cac_min(ss->len, n);
	wsubstr_ninit(out, ss->str, ss->len - k);
}

void wsubstr_chop_any_of(const wsubstr_t *ss,
                         const wchar_t *s, wsubstr_t *out)
{
	wsubstr_nchop_any_of(ss, s, wstr_length(s), out);
}

void wsubstr_nchop_any_of(const wsubstr_t *ss,
                          const wchar_t *s, size_t n,
                          wsubstr_t *out)
{
	size_t j, k;

	j = wsubstr_nfind_last_not_of(ss, ss->len, s, n);
	k = ((j < ss->len) ? j + 1 : 0);
	wsubstr_sub(ss, 0UL, k, out);
}

void wsubstr_chop_chr(const wsubstr_t *ss,
                      wchar_t c, wsubstr_t *out)
{
	size_t j, k;

	j = wsubstr_find_last_not(ss, ss->len, c);
	k = ((j < ss->len) ? j + 1 : 0);
	wsubstr_sub(ss, 0UL, k, out);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void wsubstr_strip_any_of(const wsubstr_t *ss,
                          const wchar_t *s, wsubstr_t *out)
{
	wsubstr_nstrip_any_of(ss, s, wstr_length(s), out);
}

void wsubstr_nstrip_any_of(const wsubstr_t *ss,
                           const wchar_t *set, size_t n,
                           wsubstr_t *out)
{
	wsubstr_t sub;

	wsubstr_ntrim_any_of(ss, set, n, &sub);
	wsubstr_nchop_any_of(&sub, set, n, out);
}

void wsubstr_strip_chr(const wsubstr_t *ss, wchar_t c,
                       wsubstr_t *out)
{
	wsubstr_t sub;

	wsubstr_trim_chr(ss, c, &sub);
	wsubstr_chop_chr(&sub, c, out);
}

void wsubstr_strip_ws(const wsubstr_t *ss, wsubstr_t *out)
{
	const wchar_t *spaces = L" \n\t\r\v\f";
	wsubstr_strip_any_of(ss, spaces, out);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void wsubstr_find_token(const wsubstr_t *ss,
                        const wchar_t *s, wsubstr_t *out)
{
	wsubstr_nfind_token(ss, s, wstr_length(s), out);
}

void wsubstr_nfind_token(const wsubstr_t *ss,
                         const wchar_t *s, size_t n,
                         wsubstr_t *out)
{
	size_t i, j;

	i = cac_min(wsubstr_nfind_first_not_of(ss, 0UL, s, n), ss->len);
	j = cac_min(wsubstr_nfind_first_of(ss, i, s, n), ss->len);
	wsubstr_sub(ss, i, j - i, out);
}

void wsubstr_find_token_chr(const wsubstr_t *ss,
                            wchar_t c, wsubstr_t *out)
{
	size_t i, j;

	i  = cac_min(wsubstr_find_first_not(ss, 0UL, c), ss->len);
	j  = cac_min(wsubstr_find_chr(ss, i, c), ss->len);
	wsubstr_sub(ss, i, j - i, out);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void wsubstr_find_next_token(const wsubstr_t *ss,
                             const wsubstr_t *tok,
                             const wchar_t *s,
                             wsubstr_t *out)
{
	wsubstr_nfind_next_token(ss, tok, s, wstr_length(s), out);
}

void wsubstr_nfind_next_token(const wsubstr_t *ss,
                              const wsubstr_t *tok,
                              const wchar_t *s, size_t n,
                              wsubstr_t *out)
{
	size_t i;
	const wchar_t *p;
	wsubstr_t sub;

	p = wsubstr_end(tok);
	i = wsubstr_offset(ss, p);

	wsubstr_sub(ss, i, ss->len, &sub);
	wsubstr_nfind_token(&sub, s, n, out);
}

void wsubstr_find_next_token_chr(const wsubstr_t *ss,
                                 const wsubstr_t *tok, wchar_t sep,
                                 wsubstr_t *out)
{
	size_t i;
	const wchar_t *p;
	wsubstr_t sub;

	p = wsubstr_end(tok);
	i = wsubstr_offset(ss, p);

	wsubstr_sub(ss, i, ss->len, &sub);
	wsubstr_find_token_chr(&sub, sep, out);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int wsubstr_tokenize(const wsubstr_t *ss,
                     const wchar_t *s,
                     wsubstr_t tok_list[],
                     size_t list_size, size_t *p_ntok)
{
	return wsubstr_ntokenize(ss, s, wstr_length(s),
	                         tok_list, list_size, p_ntok);
}

int wsubstr_ntokenize(const wsubstr_t *ss,
                      const wchar_t *s, size_t n,
                      wsubstr_t tok_list[],
                      size_t list_size, size_t *p_ntok)
{
	size_t ntok = 0;
	wsubstr_t tok_obj;
	wsubstr_t *tok = &tok_obj;
	wsubstr_t *tgt = NULL;

	wsubstr_nfind_token(ss, s, n, tok);
	while (!wsubstr_isempty(tok)) {
		if (ntok == list_size) {
			return -1; /* Insufficient room */
		}
		tgt = &tok_list[ntok++];
		wsubstr_clone(tok, tgt);

		wsubstr_nfind_next_token(ss, tok, s, n, tok);
	}

	if (p_ntok != NULL) {
		*p_ntok = ntok;
	}
	return 0;
}

int wsubstr_tokenize_chr(const wsubstr_t *ss, wchar_t sep,
                         wsubstr_t tok_list[],
                         size_t list_size, size_t *p_ntok)
{
	size_t ntok = 0;
	wsubstr_t tok_obj;
	wsubstr_t *tok = &tok_obj;
	wsubstr_t *tgt = NULL;

	wsubstr_find_token_chr(ss, sep, tok);
	while (!wsubstr_isempty(tok)) {
		if (ntok == list_size) {
			return -1; /* Insufficient room */
		}
		tgt = &tok_list[ntok++];
		wsubstr_clone(tok, tgt);

		wsubstr_find_next_token_chr(ss, tok, sep, tok);
	}

	if (p_ntok != NULL) {
		*p_ntok = ntok;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t wsubstr_common_prefix(const wsubstr_t *ss, const wchar_t *s)
{
	return wsubstr_ncommon_prefix(ss, s, wstr_length(s));
}

size_t wsubstr_ncommon_prefix(const wsubstr_t *ss,
                              const wchar_t *s, size_t n)
{
	return wstr_common_prefix(ss->str, ss->len, s, n);
}

int wsubstr_starts_with(const wsubstr_t *ss, wchar_t c)
{
	const wchar_t s[] = { c };
	return (wsubstr_ncommon_prefix(ss, s, 1) == 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t wsubstr_common_suffix(const wsubstr_t *ss, const wchar_t *s)
{
	return wsubstr_ncommon_suffix(ss, s, wstr_length(s));
}

size_t wsubstr_ncommon_suffix(const wsubstr_t *ss,
                              const wchar_t *s, size_t n)
{
	return wstr_common_suffix(ss->str, ss->len, s, n);
}

int wsubstr_ends_with(const wsubstr_t *ss, wchar_t c)
{
	const wchar_t s[] = { c };
	return (wsubstr_ncommon_suffix(ss, s, 1) == 1);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Generic Operations:
 */
static size_t wsubstr_find_if_u(const wsubstr_t *ss, wchr_fn fn, int u)
{
	const wchar_t *p =
	        wstr_find_if(wsubstr_begin(ss), wsubstr_end(ss), fn, u);
	return wsubstr_offset(ss, p);
}

size_t wsubstr_find_if(const wsubstr_t *ss, wchr_fn fn)
{
	return wsubstr_find_if_u(ss, fn, 1);
}

size_t wsubstr_find_if_not(const wsubstr_t *ss, wchr_fn fn)
{
	return wsubstr_find_if_u(ss, fn, 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t wsubstr_rfind_if_u(const wsubstr_t *ss, wchr_fn fn, int u)
{
	const wchar_t *p =
	        wstr_rfind_if(wsubstr_begin(ss), wsubstr_end(ss), fn, u);
	return wsubstr_offset(ss, p);
}

size_t wsubstr_rfind_if(const wsubstr_t *ss, wchr_fn fn)
{
	return wsubstr_rfind_if_u(ss, fn, 1);
}

size_t wsubstr_rfind_if_not(const wsubstr_t *ss, wchr_fn fn)
{
	return wsubstr_rfind_if_u(ss, fn, 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t wsubstr_count_if(const wsubstr_t *ss, wchr_fn fn)
{
	return wstr_count_if(wsubstr_begin(ss), wsubstr_end(ss), fn);
}

int wsubstr_test_if(const wsubstr_t *wsubstr, wchr_fn fn)
{
	return wstr_test_if(wsubstr_begin(wsubstr), wsubstr_end(wsubstr), fn);
}

void wsubstr_trim_if(const wsubstr_t *ss,
                     wchr_fn fn, wsubstr_t   *out)
{
	const size_t i = wsubstr_find_if_not(ss, fn);

	wsubstr_sub(ss, i, ss->len, out);
}

void wsubstr_chop_if(const wsubstr_t *ss,
                     wchr_fn fn, wsubstr_t *out)
{
	const size_t j = wsubstr_rfind_if_not(ss, fn);
	const size_t k = ((j < ss->len) ? j + 1 : 0);

	wsubstr_sub(ss, 0UL, k, out);
}

void wsubstr_strip_if(const wsubstr_t *ss,
                      wchr_fn fn, wsubstr_t *out)
{
	wsubstr_t sub;

	wsubstr_trim_if(ss, fn, &sub);
	wsubstr_chop_if(&sub, fn, out);
}
