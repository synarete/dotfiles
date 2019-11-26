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
#ifndef CAC_STRUTILS_H_
#define CAC_STRUTILS_H_

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct string {
	char *str;
	size_t len;
} string_t;

extern const string_t *string_nil;

extern const struct htblfns *string_hfns;


string_t *string_new(const char *);

string_t *string_newb(size_t);

string_t *string_nnew(const char *, size_t);

string_t *string_dup(const string_t *);

string_t *string_join(const string_t *, const string_t *);

string_t *string_joins(const string_t *, const char *);

string_t *string_joinn(const string_t *, const char *, size_t);

bool string_isequal(const string_t *, const string_t *);

bool string_isequals(const string_t *s1, const char *s2);

uint64_t string_hash(const string_t *);

int string_compare(const string_t *, const string_t *);

string_t *string_fmt(const char *fmt, ...);


#endif /* CAC_STRUTILS_H_ */

