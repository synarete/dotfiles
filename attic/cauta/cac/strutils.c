/*
 * CaC: Cauta-to-C Compiler
 *
 * Copyright (C) 2016,2017,2018 Shachar Sharon
 *
 * CaC is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CaC is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CaC. If not, see <https://www.gnu.org/licenses/gpl>.
 */
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "macros.h"
#include "memory.h"
#include "hashtbl.h"
#include "strutils.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static uint64_t nexthash(uint64_t c, uint64_t prevhash)
{
	return (prevhash + (c << 4) + (c >> 4)) * 11;
}

uint64_t string_hash(const string_t *s)
{
	uint64_t hkey = s->len;

	for (size_t i = 0; i < s->len; ++i) {
		hkey = nexthash((uint64_t)s->str[i], hkey);
	}
	return hkey;
}

int string_compare(const string_t *s1, const string_t *s2)
{
	int res;

	res = strncmp(s1->str, s2->str, MIN(s1->len, s2->len));
	if (res == 0) {
		res = (s1->len > s2->len) - (s1->len < s2->len);
	}
	return res;
}

static hashkey_t key_to_hash(const void *key)
{
	return string_hash((const string_t *)key);
}

static int key_compare(const void *k1, const void *k2)
{
	return string_compare((const string_t *)k1,
	                      (const string_t *)k2);
}

static const htblfns_t s_hfns = {
	.hcmp = key_compare,
	.hkey = key_to_hash
};
const htblfns_t *string_hfns = &s_hfns;

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static string_t s_null_string = {
	.str = (char *)"",
	.len = 0,
};
const string_t *string_nil = &s_null_string;


static size_t aligned_memsize(size_t bsz)
{
	const size_t align = sizeof(void *);
	return (align * ((bsz + align - 1) / align));
}

static void *memget(const void *p, size_t n)
{
	return (void *)((char *)p + n);
}

static string_t *allocate_string(size_t capacity)
{
	void  *mem;
	size_t msz;
	string_t *str;

	msz = aligned_memsize(sizeof(*str) + (capacity + 1));
	mem = gc_malloc(msz);
	str = (string_t *)mem;
	str->str = (char *)memget(mem, sizeof(*str));
	str->len = 0;
	return str;
}

string_t *string_newb(size_t bsz)
{
	return allocate_string(bsz);
}

string_t *string_new(const char *s)
{
	string_t *str;

	if (s != NULL) {
		str = string_nnew(s, strlen(s));
	} else {
		str = string_nnew(s_null_string.str, s_null_string.len);
	}
	return str;
}

string_t *string_nnew(const char *s, size_t n)
{
	string_t *str;

	str = allocate_string(n);
	strncpy(str->str, s, n);
	str->len = n;
	return str;
}

string_t *string_dup(const string_t *other)
{
	string_t *str;

	str = allocate_string(other->len);
	strncpy(str->str, other->str, other->len);
	str->len = other->len;
	return str;
}

string_t *string_join(const string_t *str1, const string_t *str2)
{
	string_t *str;

	str = allocate_string(str1->len + str2->len);
	strncpy(str->str, str1->str, str1->len);
	strncpy(str->str + str1->len, str2->str, str2->len);
	str->len = str1->len + str2->len;
	return str;
}

string_t *string_joins(const string_t *str, const char *s)
{
	return string_join(str, string_new(s));
}

string_t *string_joinn(const string_t *str, const char *s, size_t n)
{
	return string_join(str, string_nnew(s, n));
}

bool string_isequal(const string_t *s1, const string_t *s2)
{
	const size_t len = s1->len;

	return (len == s2->len) && (strncmp(s1->str, s2->str, len) == 0);
}

bool string_isequals(const string_t *s1, const char *s2)
{
	const size_t len = strlen(s2);

	return (len == s1->len) && (strncmp(s1->str, s2, len) == 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

string_t *string_fmt(const char *fmt, ...)
{
	int len;
	char buf[1];
	va_list ap;
	string_t *str;

	va_start(ap, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	str = allocate_string((size_t)len + 1);
	va_start(ap, fmt);
	len = vsnprintf(str->str, (size_t)len, fmt, ap);
	va_end(ap);
	str->len = (size_t)len;

	return str;
}



