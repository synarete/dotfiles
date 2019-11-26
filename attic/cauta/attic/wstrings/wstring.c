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
#include <stdarg.h>
#include <stdio.h>

#include "infra.h"
#include "wstrchr.h"
#include "wsubstr.h"
#include "wsupstr.h"
#include "wstring.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static uint64_t nexthash(uint64_t c, uint64_t prevhash)
{
	return (prevhash + (c << 4) + (c >> 4)) * 11;
}

uint64_t wstring_hash(const wstring_t *s)
{
	uint64_t hkey = s->len;

	for (size_t i = 0; i < s->len; ++i) {
		hkey = nexthash((uint64_t)s->str[i], hkey);
	}
	return hkey;
}

int wstring_compare(const wstring_t *s1, const wstring_t *s2)
{
	return wsubstr_ncompare(s1, s2->str, s2->len);
}

static hashkey_t key_to_hash(const void *key)
{
	return wstring_hash((const wstring_t *)key);
}

static int key_compare(const void *k1, const void *k2)
{
	return wstring_compare((const wstring_t *)k1,
	                       (const wstring_t *)k2);
}

static const htblfns_t s_hfns = {
	.hcmp = key_compare,
	.hkey = key_to_hash
};
const htblfns_t *wstring_hfns = &s_hfns;

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static wstring_t s_null_wstring = {
	.str = L"",
	.len = 0,
};
wstring_t *wstring_nil = &s_null_wstring;


static size_t aligned_memsize(size_t bsz)
{
	const size_t align = sizeof(void *);
	return (align * ((bsz + align - 1) / align));
}

static wsupstr_t *new_wsupstr_wstring(size_t len)
{
	void  *mem;
	wchar_t  *buf;
	size_t msz;
	wsupstr_t *ss;

	msz = aligned_memsize(sizeof(*ss) + ((len + 1) * sizeof(*buf)));
	mem = gc_malloc(msz);
	ss = (wsupstr_t *)mem;
	buf = (wchar_t *)cac_memget(mem, sizeof(*ss));
	wsupstr_ninit(ss, buf, 0, len);
	return ss;
}

wstring_t *wstring_new(const wchar_t *s)
{
	size_t len;
	wstring_t *str;

	if (s != NULL) {
		len = wstr_length(s);
		str = wstring_nnew(s, len);
	} else {
		str = wstring_nnew(s_null_wstring.str, s_null_wstring.len);
	}
	return str;
}

wstring_t *wstring_nnew(const wchar_t *s, size_t n)
{
	wsupstr_t *ss;

	ss = new_wsupstr_wstring(n);
	wsupstr_nassign(ss, s, n);
	return &ss->sub;
}

wstring_t *wstring_cnew(wchar_t c)
{
	const wchar_t s[] = { c };

	return wstring_nnew(s, 1);
}

wstring_t *wstring_dup(const wstring_t *other)
{
	wsupstr_t *ss = wstring_xdup(other);
	return &ss->sub;
}

wsupstr_t *wstring_xdup(const wstring_t *other)
{
	wsupstr_t *ss;

	ss = new_wsupstr_wstring(other->len);
	wsupstr_nassign(ss, other->str, other->len);
	return ss;
}

wstring_t *wstring_join(const wstring_t *str1,
                        const wstring_t *str2)
{
	wsupstr_t *ss;

	ss = new_wsupstr_wstring(str1->len + str2->len);
	wsupstr_nassign(ss, str1->str, str1->len);
	wsupstr_nappend(ss, str2->str, str2->len);
	return &ss->sub;
}

wstring_t *wstring_joins(const wstring_t *str, const wchar_t *s)
{
	return wstring_join(str, wstring_new(s));
}

wstring_t *wstring_joinn(const wstring_t *str,
                         const wchar_t *s, size_t n)
{
	return wstring_join(str, wstring_nnew(s, n));
}

wstring_t *wstring_joinss(const wstring_t *str1,
                          const wchar_t *str2,
                          const wstring_t *str3)
{
	wsupstr_t *ss;
	const size_t len2 = wstr_length(str2);

	ss = new_wsupstr_wstring(str1->len + len2 + str3->len);
	wsupstr_nassign(ss, str1->str, str1->len);
	wsupstr_nappend(ss, str2, len2);
	wsupstr_nappend(ss, str3->str, str3->len);
	return &ss->sub;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

wstrpair_t *wstring_split(const wstring_t *str,
                          size_t pos, size_t nskip)
{
	wsubstr_t ss;
	wstrpair_t *sp;

	sp = cac_wstrpair_new();
	wsubstr_sub(str, 0, pos, &ss);
	sp->first = wstring_dup(&ss);
	wsubstr_sub(str, pos + nskip, str->len, &ss);
	sp->second = wstring_dup(&ss);
	return sp;
}

wstrpair_t *wstring_split_chr(const wstring_t *str, wchar_t chr)
{
	size_t pos;

	pos = wsubstr_find_chr(str, 0, chr);
	return wstring_split(str, pos, 1);
}

wstrpair_t *wstring_split_str(const wstring_t *str,
                              const wchar_t *s)
{
	size_t pos, len;

	len = wstr_length(s);
	pos = wsubstr_nfind(str, 0, s, len);
	return wstring_split(str, pos, len);
}


bool wstring_isequal(const wstring_t *s1, const wstring_t *s2)
{
	return wsubstr_isequals(s1, s2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

wstrpair_t *cac_wstrpair_new(void)
{
	return cac_wstrpair_new2(wstring_nil, wstring_nil);
}

wstrpair_t *cac_wstrpair_new2(wstring_t *str1, wstring_t *str2)
{
	wstrpair_t *sp;

	sp = (wstrpair_t *)gc_malloc(sizeof(*sp));
	sp->first = str1;
	sp->second = str2;
	return sp;
}


