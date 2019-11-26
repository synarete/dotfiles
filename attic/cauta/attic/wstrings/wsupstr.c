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
#include "wstrchr.h"
#include "wsubstr.h"
#include "wstrbuf.h"
#include "wsupstr.h"



/* Set EOS characters at the end of characters array (if possible) */
static void wsupstr_terminate(wsupstr_t *ss)
{
	if (ss->sub.len < ss->bsz) {
		wstr_terminate(ss->buf, ss->sub.len);
	}
}


void wsupstr_init(wsupstr_t *ss, wchar_t *buf, size_t nwr)
{
	wsupstr_ninit(ss, buf, 0, nwr);
}

void wsupstr_ninit(wsupstr_t *ss, wchar_t *buf, size_t nrd, size_t nwr)
{
	wsubstr_ninit(&ss->sub, buf, nrd);
	ss->buf  = buf;
	ss->bsz  = nwr;
	wsupstr_terminate(ss);
}

void wsupstr_destroy(wsupstr_t *ss)
{
	wsubstr_destroy(&ss->sub);
	ss->buf  = NULL;
	ss->bsz  = 0;
}

/*
 * Returns the string's read-length. Synonym to ss->sub.len
 */
size_t wsupstr_size(const wsupstr_t *ss)
{
	return wsubstr_size(&ss->sub);
}

size_t wsupstr_bufsize(const wsupstr_t *ss)
{
	return ss->bsz;
}


wchar_t *wsupstr_data(const wsupstr_t *ss)
{
	return ss->buf;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Inserts a copy of s before position pos. */
static void wsupstr_insert_ns(wsupstr_t *ss,
                              size_t pos, const wchar_t *s, size_t n)
{
	ss->sub.len =
	        wstr_insert_sub(ss->buf, ss->sub.len, ss->bsz, pos, s, n);
	wsupstr_terminate(ss);
}

/* Inserts n copies of c before position pos. */
static void wsupstr_insert_nc(wsupstr_t *ss,
                              size_t pos, size_t n, wchar_t c)
{
	ss->sub.len =
	        wstr_insert_fill(ss->buf, ss->sub.len, ss->bsz, pos, n, c);
	wsupstr_terminate(ss);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Replaces a wsupstring of *this with a copy of s. */
static void wsupstr_replace_ns(wsupstr_t *ss, size_t pos, size_t n1,
                               const wchar_t *s, size_t n)
{
	ss->sub.len =
	        wstr_replace_sub(ss->buf, ss->sub.len, ss->bsz, pos, n1, s, n);
	wsupstr_terminate(ss);
}

/* Replaces a wsupstring of *this with n2 copies of c. */
static void wsupstr_replace_nc(wsupstr_t *ss,
                               size_t pos, size_t n1, size_t n2, wchar_t c)
{
	ss->sub.len =
	        wstr_replace_fill(ss->buf, ss->sub.len, ss->bsz, pos, n1, n2, c);
	wsupstr_terminate(ss);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void wsupstr_assign(wsupstr_t *ss, const wchar_t *s)
{
	wsupstr_nassign(ss, s, wstr_length(s));
}

void wsupstr_nassign(wsupstr_t *ss, const wchar_t *s, size_t len)
{
	wsupstr_nreplace(ss, 0, ss->sub.len, s, len);
}

void wsupstr_assign_chr(wsupstr_t *ss, size_t n, wchar_t c)
{
	wsupstr_replace_chr(ss, 0, ss->sub.len, n, c);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void wsupstr_push_back(wsupstr_t *ss, wchar_t c)
{
	wsupstr_append_chr(ss, 1, c);
}

void wsupstr_append(wsupstr_t *ss, const wchar_t *s)
{
	wsupstr_nappend(ss, s, wstr_length(s));
}

void wsupstr_nappend(wsupstr_t *ss, const wchar_t *s, size_t len)
{
	wsupstr_ninsert(ss, ss->sub.len, s, len);
}

void wsupstr_append_chr(wsupstr_t *ss, size_t n, wchar_t c)
{
	wsupstr_insert_chr(ss, ss->sub.len, n, c);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void wsupstr_insert(wsupstr_t *ss, size_t pos, const wchar_t *s)
{
	wsupstr_ninsert(ss, pos, s, wstr_length(s));
}

void wsupstr_ninsert(wsupstr_t *ss, size_t pos,
                     const wchar_t *s, size_t len)
{
	const size_t at = cac_min(ss->sub.len, pos);
	wsupstr_insert_ns(ss, at, s, len);
}

void wsupstr_insert_chr(wsupstr_t *ss, size_t pos, size_t n, wchar_t c)
{
	const size_t at = cac_min(ss->sub.len, pos);
	wsupstr_insert_nc(ss, at, n, c);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void wsupstr_replace(wsupstr_t *ss,
                     size_t pos, size_t n, const wchar_t *s)
{
	wsupstr_nreplace(ss, pos, n, s, wstr_length(s));
}

void wsupstr_nreplace(wsupstr_t *ss,
                      size_t pos, size_t n,  const wchar_t *s, size_t len)
{
	if (pos < ss->sub.len) {
		wsupstr_replace_ns(ss, pos, n, s, len);
	} else {
		wsupstr_insert_ns(ss, ss->sub.len, s, len);
	}
}

void wsupstr_replace_chr(wsupstr_t *ss,
                         size_t pos, size_t n1, size_t n2, wchar_t c)
{
	if (pos < ss->sub.len) {
		wsupstr_replace_nc(ss, pos, n1, n2, c);
	} else {
		wsupstr_insert_nc(ss, ss->sub.len, n2, c);
	}
}

void wsupstr_erase(wsupstr_t *ss, size_t pos, size_t n)
{
	wchar_t c = '\0';
	wsupstr_replace_chr(ss, pos, n, 0, c);
}

void wsupstr_reverse(wsupstr_t *ss)
{
	wstr_reverse(ss->buf, ss->sub.len);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/


void wsupstr_foreach(wsupstr_t *ss, wchr_modify_fn fn)
{
	for (size_t i = 0; i < ss->bsz; ++i) {
		fn(&ss->buf[i]);
	}
}

