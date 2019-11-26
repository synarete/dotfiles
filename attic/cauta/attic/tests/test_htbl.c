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
#include <cacconfig.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include <cac/infra.h>
#include <cac/hash.h>
#include "tests.h"

#define MAGIC 0xDEADBEEF

struct elem {
	char key[40];
	size_t val;
	size_t magic;
};
typedef struct elem elem_t;


static elem_t *elem_new(size_t val)
{
	elem_t *elem = NULL;

	elem = (elem_t *)gc_malloc(sizeof(*elem));
	snprintf(elem->key, sizeof(elem->key), "%lu", val);
	elem->val = val;
	elem->magic = MAGIC;
	return elem;
}

static hashkey_t key_to_hash(const void *key)
{
	hashkey_t hkey = 0;
	const char *str = (const char *)key;
	uint8_t const seed[16] =
	{ 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7 };

	cac_hash_siphash64(seed, str, strlen(str), &hkey);
	return hkey;
}

static int key_compare(const void *key1, const void *key2)
{
	const char *str1 = (const char *)key1;
	const char *str2 = (const char *)key2;
	return strcmp(str1, str2);
}

static const htblfns_t s_hfns = {
	.hcmp = key_compare,
	.hkey = key_to_hash
};

static int *new_prandom_seq(size_t cnt)
{
	int *arr;

	arr = gc_malloc(cnt * sizeof(*arr));
	cac_hash_prandom_tseq(arr, cnt, 0);
	return arr;
}
/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_htbl1(void)
{
	elem_t *x, *y;
	htbl_t *htbl;
	const size_t n = 2207;

	htbl = htbl_new(&s_hfns);
	cac_assert(htbl != NULL);
	for (size_t i = 0; i < n; ++i) {
		x = elem_new(i);
		htbl_insert(htbl, x->key, x);
		y = htbl_find(htbl, x->key);
		cac_assert(y != NULL);
		cac_assert(y->magic == MAGIC);
		cac_assert(y->val == x->val);
		cac_assert(y == x);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_htbl2(void)
{
	elem_t *x, *y, *z;
	htbl_t *htbl;
	const size_t n = 2677;

	htbl = htbl_new(&s_hfns);
	for (size_t i = 0; i < n; ++i) {
		x = elem_new(i);
		htbl_insert(htbl, x->key, x);
	}
	for (size_t j = 0; j < n; ++j) {
		y = elem_new(j);
		z = htbl_find(htbl, y->key);
		cac_assert(z->val == y->val);
	}
	htbl_clear(htbl);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_htbl3(void)
{
	elem_t *x, *y, *z, *w;
	htbl_t *htbl;
	const size_t n = 2333;

	htbl = htbl_new(&s_hfns);
	for (size_t i = 0; i < n; ++i) {
		x = elem_new(i);
		htbl_insert(htbl, x->key, x);
	}
	for (size_t j = 0; j < n; ++j) {
		y = elem_new(j);
		z = htbl_find(htbl, y->key);
		cac_assert(z->val == y->val);
		w = htbl_erase(htbl, y->key);
		cac_assert(w != NULL);
		cac_assert(!strcmp(w->key, y->key));
		z = htbl_find(htbl, y->key);
		cac_assert(z == NULL);
	}
	cac_assert(htbl_isempty(htbl));
	htbl_clear(htbl);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_htbl4(void)
{
	int *arr;
	size_t sz, sz2;
	elem_t *x, *y, *z, *w;
	htbl_t *htbl;
	const size_t n = 7727;

	arr = new_prandom_seq(n);
	htbl = htbl_new(&s_hfns);
	for (size_t i = 0; i < n; ++i) {
		x = elem_new((size_t)arr[i]);
		htbl_insert(htbl, x->key, x);
		sz = htbl_size(htbl);
		cac_assert(sz == (i + 1));
	}
	for (size_t j = 0; j < n; ++j) {
		y = elem_new((size_t)arr[j]);
		z = htbl_find(htbl, y->key);
		cac_assert(z->val == y->val);
		w = htbl_erase(htbl, y->key);
		cac_assert(!strcmp(w->key, y->key));
		z = htbl_find(htbl, y->key);
		cac_assert(z == NULL);

		sz--;
		sz2 = htbl_size(htbl);
		cac_assert(sz == sz2);
	}
	cac_assert(htbl_isempty(htbl));
	htbl_clear(htbl);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_htbl5(void)
{
	int *arr;
	size_t i;
	elem_t *x, *y, *z, *w;
	htbl_t *htbl;
	const size_t n = 7877;

	arr = new_prandom_seq(n);
	htbl = htbl_new(&s_hfns);
	for (i = 0; i < n; ++i) {
		x = elem_new((size_t)arr[i]);
		htbl_insert(htbl, x->key, x);
	}
	for (i = 0; i < n; i += 2) {
		y = elem_new((size_t)arr[i]);
		z = htbl_find(htbl, y->key);
		cac_assert(z->val == y->val);
		w = htbl_erase(htbl, y->key);
		cac_assert(!strcmp(w->key, y->key));
		z = htbl_find(htbl, y->key);
		cac_assert(z == NULL);
		htbl_insert(htbl, y->key, y);
	}

	for (i = 0; i < n; ++i) {
		y = elem_new((size_t)arr[i]);
		z = htbl_find(htbl, y->key);
		cac_assert(z->val == y->val);
		w = htbl_erase(htbl, y->key);
		cac_assert(!strcmp(w->key, y->key));
		z = htbl_find(htbl, y->key);
		cac_assert(z == NULL);
	}

	cac_assert(htbl_isempty(htbl));
	htbl_clear(htbl);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void cac_test_htbl(void)
{
	test_htbl1();
	test_htbl2();
	test_htbl3();
	test_htbl4();
	test_htbl5();
}
