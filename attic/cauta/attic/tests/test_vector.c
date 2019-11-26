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
#include <cac/tests.h>

struct elem {
	size_t key;
	char pad[24];
};
typedef struct elem elem_t;

static elem_t *elem_new(size_t key)
{
	elem_t *elem = NULL;

	elem = (elem_t *)gc_malloc(sizeof(*elem));
	elem->key = key;
	return elem;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t *gen_randoms(size_t cnt)
{
	size_t *buf;
	size_t  bsz;

	bsz = cnt * sizeof(*buf);
	buf = (size_t *)gc_malloc(cnt * bsz);
	cac_hash_prandom_buf(buf, bsz, (unsigned long)time(NULL));
	return buf;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_vector1(void)
{
	vector_t *vector;
	elem_t *z, *y;
	size_t sz, i, j, n = 131;

	vector = vector_nnew(1);
	cac_assert(vector_isempty(vector));
	for (i = 0; i < n; ++i) {
		z = elem_new(i);
		vector_push_back(vector, z);
		cac_assert(vector_size(vector) == (i + 1));
		y = (elem_t *)vector_at(vector, i);
		cac_assert(y != NULL);
		cac_assert(y->key == z->key);
	}
	sz = vector_size(vector);
	cac_assert(sz == n);
	for (j = n; j > 0; --j) {
		z = vector_pop_back(vector);
		cac_assert(z != NULL);
		cac_assert(z->key == (j - 1));
	}
	for (i = 0; i < n; ++i) {
		z = elem_new(i);
		vector_push_back(vector, z);
		y = (elem_t *)vector_at(vector, i);
		cac_assert(y->key == z->key);
	}
	vector_clear(vector);
	cac_assert(vector_isempty(vector));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_vector2(void)
{
	vector_t *vector;
	elem_t *z, *y;
	size_t sz, i, j, k, n = 251;

	vector = vector_new();
	for (i = 0; i < n; ++i) {
		z = elem_new(i);
		vector_push_front(vector, z);
		cac_assert(vector_size(vector) == (i + 1));
		y = (elem_t *)vector_at(vector, 0);
		cac_assert(y->key == i);
		cac_assert(y->key == z->key);
		y = (elem_t *)vector_at(vector, i);
		cac_assert(y->key == 0);

		sz = vector_size(vector);
		cac_assert(sz == (i + 1));

		for (k = 0, j = sz; j > 0; --j, ++k) {
			y = (elem_t *)vector_at(vector, j - 1);
			cac_assert(y->key == k);
		}
	}
	for (j = n; j > 0; --j) {
		z = vector_pop_front(vector);
		cac_assert(z != NULL);
		cac_assert(z->key == (j - 1));
	}
	vector_clear(vector);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_vector3(void)
{
	vector_t *vector;
	elem_t *z, *y;
	size_t sz, i, j, k, n = 443;

	vector = vector_nnew(n / 3);
	for (i = 0; i < n; ++i) {
		z = elem_new(i);
		vector_push_front(vector, z);
		vector_push_back(vector, z);
		sz = vector_size(vector);
		cac_assert(sz == (2 * (i + 1)));

		y = (elem_t *)vector_front(vector);
		cac_assert(y->key == i);
		y = (elem_t *)vector_back(vector);
		cac_assert(y->key == i);
	}
	k = n - 1;
	for (j = 0; j < n; ++j) {
		z = vector_pop_front(vector);
		cac_assert(z != NULL);
		cac_assert(z->key == k);
		y = vector_pop_back(vector);
		cac_assert(y != NULL);
		cac_assert(y->key == k);
		k--;
	}
	cac_assert(vector_isempty(vector));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_vector4(void)
{
	vector_t *vector;
	elem_t *y;
	size_t sz, i, n = 661;

	vector = vector_new();
	for (i = 0; i < n; ++i) {
		y = elem_new(i);
		sz = vector_size(vector);
		vector_insert(vector, sz / 2, y);
		sz = vector_size(vector);
		vector_insert(vector, sz / 2, y);
	}
	for (i = 0; i < n; ++i) {
		y = (elem_t *)vector_pop_front(vector);
		cac_assert(y != NULL);
		cac_assert(y->key == i);
		y = (elem_t *)vector_pop_back(vector);
		cac_assert(y != NULL);
		cac_assert(y->key == i);
	}
	cac_assert(vector_isempty(vector));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_vector5(void)
{
	vector_t *vector;
	elem_t *y;
	size_t sz, i, n = 751;
	size_t skey = 123456;

	vector = vector_new();
	vector_push_front(vector, elem_new(skey));
	vector_push_back(vector, elem_new(skey));

	for (i = 0; i < n; ++i) {
		y = elem_new(i);
		sz = vector_size(vector);
		vector_insert(vector, sz / 2, y);
	}
	sz = vector_size(vector);
	cac_asserteq(sz, (n + 2));
	vector_erasen(vector, 1, n);
	sz = vector_size(vector);
	cac_asserteq(sz, 2);

	y = (elem_t *)vector_at(vector, 0);
	cac_assert(y != NULL);
	cac_assert(y->key == skey);
	y = (elem_t *)vector_at(vector, 1);
	cac_assert(y != NULL);
	cac_assert(y->key == skey);

	vector_clear(vector);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_vector6(void)
{
	vector_t *vector;
	elem_t *y;
	size_t sz, i, n = 233;
	size_t skey = 12345678;

	vector = vector_new();

	vector_push_front(vector, elem_new(skey));
	for (i = 0; i < n; ++i) {
		vector_push_front(vector, elem_new(i));
		vector_push_back(vector, elem_new(i));
	}
	vector_push_back(vector, elem_new(skey + 1));
	for (i = 0; i < n; ++i) {
		vector_push_back(vector, elem_new(i));
	}
	vector_erasen(vector, 0, n);
	vector_erasen(vector, 1, n);
	vector_erasen(vector, 2, n);

	y = (elem_t *)vector_pop_front(vector);
	cac_assert(y != NULL);
	cac_assert(y->key == skey);
	y = (elem_t *)vector_pop_back(vector);
	cac_assert(y != NULL);
	cac_assert(y->key == (skey + 1));

	sz = vector_size(vector);
	cac_assert(sz == 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_vector7(void)
{
	size_t i, n = 127;
	elem_t *y, *z;
	vector_t *vector, *vector1, *vector2;

	vector1 = vector_new();
	vector2 = vector_new();

	for (i = 0; i < n; ++i) {
		vector_push_front(vector1, elem_new(i));
		vector_push_back(vector2, elem_new(i));
	}
	vector = vector_join(vector1, vector2);
	cac_asserteq(vector_size(vector), 2 * n);

	for (i = 0; i < n; ++i) {
		y = (elem_t *)vector_pop_front(vector);
		z = (elem_t *)vector_pop_back(vector);
		cac_assert(y != NULL);
		cac_assert(z != NULL);
		cac_assert(y->key == z->key);
		cac_assert(y->key == (n - i - 1));
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_vector8(void)
{
	vector_t *vector;
	elem_t *y;
	size_t sz, pos, i, n = 733;
	size_t *nums = gen_randoms(n);

	vector = vector_new();
	for (i = 0; i < n; ++i) {
		pos = nums[i] % n;
		y = elem_new(pos);
		vector_insert(vector, pos, y);
	}
	for (i = 0; i < n; ++i) {
		if (i % 3) {
			y = (elem_t *)vector_pop_front(vector);
		} else {
			y = (elem_t *)vector_pop_back(vector);
		}
		cac_assert(y != NULL);
		cac_assert(y->key < n);
	}
	sz = vector_size(vector);
	cac_assert(sz == 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_vector9(void)
{
	vector_t *vector;
	elem_t *y;
	size_t sz, n = 401;
	size_t *nums = gen_randoms(n);

	vector = vector_new();
	for (size_t i = 0; i < n; ++i) {
		y = elem_new(nums[i]);
		vector_insert(vector, i / 2, y);
		if (i > (n / 2)) {
			vector_erasen(vector, i / 4, 4);
		}
		sz = vector_size(vector);
		cac_assert(sz > 0);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_vector10(void)
{
	vector_t *vector;
	elem_t *y, *z;
	size_t i, sz, n = 677;
	size_t *nums = gen_randoms(n);

	vector = vector_new();
	for (i = 0; i < n; ++i) {
		y = elem_new(nums[i]);
		vector_insert(vector, i, y);
		sz = vector_size(vector);
		cac_assert(sz == (i + 1));
	}
	for (i = 0; i < n; i += 2) {
		z = elem_new(n);
		y = (elem_t *)vector_replace(vector, i, z);
		cac_assert(y != NULL);
	}
	for (i = 0; i < n; i += 2) {
		y = (elem_t *)vector_at(vector, i);
		cac_assert(y->key == n);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_vector11(void)
{
	vector_t *vector;
	elem_t *y, *z;
	size_t i, pos, n = 677;
	size_t *nums = gen_randoms(n);

	vector = vector_new();
	for (i = 0; i < n; ++i) {
		y = elem_new(nums[i]);
		vector_push_back(vector, y);
	}
	for (i = 0; i < n; ++i) {
		z = (elem_t *)vector_at(vector, i);
		cac_assert(z != NULL);
		pos = vector_find(vector, z);
		cac_assert(pos == i);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_vector12(void)
{
	vector_t *vector, *other;
	elem_t *y, *z, *w;
	size_t i, n = 1212;
	size_t *nums = gen_randoms(n);

	vector = vector_new();
	for (i = 0; i < n; ++i) {
		y = elem_new(nums[i]);
		vector_push_back(vector, y);
	}
	other = vector_dup(vector);
	cac_assert(other->len == vector->len);

	for (i = 0; i < n; ++i) {
		z = (elem_t *)vector_at(vector, i);
		cac_assert(z != NULL);
		w = (elem_t *)vector_at(other, i);
		cac_assert(w != NULL);
		cac_assert(w == z);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_vector13(void)
{
	vector_t *vector, *other;
	elem_t *y, *z, *w;
	size_t i, n = 1151;
	size_t pos, beg, end, len;
	size_t *nums = gen_randoms(n);

	vector = vector_new();
	for (i = 0; i < n; ++i) {
		y = elem_new(nums[i]);
		vector_push_back(vector, y);
	}
	beg = n / 3;
	end = n / 2;
	len = end - beg;
	other = vector_sub(vector, beg, len);
	cac_assert(other->len == len);

	pos = 0;
	for (i = beg; i < end; ++i) {
		z = (elem_t *)vector_at(vector, i);
		cac_assert(z != NULL);
		w = (elem_t *)vector_at(other, pos);
		cac_assert(w != NULL);
		cac_assert(w == z);
		pos++;
	}
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void cac_test_vector(void)
{
	test_vector1();
	test_vector2();
	test_vector3();
	test_vector4();
	test_vector5();
	test_vector6();
	test_vector7();
	test_vector8();
	test_vector9();
	test_vector10();
	test_vector11();
	test_vector12();
	test_vector13();
}
