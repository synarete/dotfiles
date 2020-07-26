/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * This file is part of voluta.
 *
 * Copyright (C) 2020 Shachar Sharon
 *
 * Voluta is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Voluta is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#define _GNU_SOURCE 1
#include "unitest.h"

#define ZMAGIC  0xA334CDE661L

struct ut_zrecord {
	struct ut_env *ut_env;
	struct voluta_avl_node avl_node;
	long key;
	long magic;
};

static struct ut_zrecord *
avl_node_to_zrecord(const struct voluta_avl_node *an)
{
	const struct ut_zrecord *zr;

	ut_assert_not_null(an);
	zr = ut_container_of2(an, struct ut_zrecord, avl_node);
	ut_assert_eq(zr->magic, ZMAGIC);

	return voluta_unconst(zr);
}

static const void *zrecord_getkey(const struct voluta_avl_node *an)
{
	const struct ut_zrecord *zr = avl_node_to_zrecord(an);

	return &zr->key;
}

static long zrecord_keycmp(const void *x, const void *y)
{
	const long znum_x = *((const long *)x);
	const long znum_y = *((const long *)y);

	return znum_y - znum_x;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct ut_zrecord *
zrecord_new(struct ut_env *ut_env, long num)
{
	struct ut_zrecord *zr;

	zr = ut_malloc(ut_env, sizeof(*zr));
	zr->ut_env = ut_env;
	zr->key = num;
	zr->magic = ZMAGIC;

	return zr;
}

static struct voluta_avl_node *avl_node_of(struct ut_zrecord *zr)
{
	return &zr->avl_node;
}

static struct voluta_avl_node *new_node(struct ut_env *ut_env, long num)
{
	struct ut_zrecord *zr = zrecord_new(ut_env, num);

	return avl_node_of(zr);
}

static void check_node(const struct voluta_avl_node *x, long num)
{
	const struct ut_zrecord *zr = avl_node_to_zrecord(x);

	ut_assert_eq(zr->key, num);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool avl_isempty(const struct voluta_avl *avl)
{
	return voluta_avl_isempty(avl);
}

static struct voluta_avl *avl_new(struct ut_env *ut_env)
{
	struct voluta_avl *avl;

	avl = ut_malloc(ut_env, sizeof(*avl));
	voluta_avl_init(avl, ut_env, zrecord_getkey, zrecord_keycmp);
	ut_assert(avl_isempty(avl));
	return avl;
}

static void avl_done(struct voluta_avl *avl)
{
	ut_assert_eq(avl->size, 0);
	voluta_avl_fini(avl);
}

static struct ut_env *avl_ut_env(const struct voluta_avl *avl)
{
	return avl->userp;
}

static void avl_insert_unique_(struct voluta_avl *avl,
			       struct voluta_avl_node *an)
{
	int ret;

	ret = voluta_avl_insert_unique(avl, an);
	ut_assert_ok(ret);
	ret = voluta_avl_insert_unique(avl, an);
	ut_assert_err(ret, -1);
}

static void avl_insert_unique(struct voluta_avl *avl, long key)
{
	avl_insert_unique_(avl, new_node(avl_ut_env(avl), key));
}

static void avl_insert_replace(struct voluta_avl *avl, long key)
{
	struct voluta_avl_node *an;
	struct voluta_avl_node *an2;

	an = voluta_avl_find(avl, &key);
	check_node(an, key);

	an2 = voluta_avl_insert_replace(avl, new_node(avl_ut_env(avl), key));
	ut_assert_eq(an, an2);
}

static void avl_find_exists(const struct voluta_avl *avl, long key)
{
	const struct ut_zrecord *zr;
	const struct voluta_avl_node *an;

	an = voluta_avl_find(avl, &key);
	ut_assert_not_null(an);

	zr = avl_node_to_zrecord(an);
	ut_assert_eq(zr->key, key);
}

static void avl_find_non_exists(const struct voluta_avl *avl, long key)
{
	const struct voluta_avl_node *an;

	an = voluta_avl_find(avl, &key);
	ut_assert_null(an);
}

static void avl_find_unique(const struct voluta_avl *avl, long key)
{
	size_t cnt;
	const struct voluta_avl_node *an;

	an = voluta_avl_find_first(avl, &key);
	check_node(an, key);

	cnt = voluta_avl_count(avl, &key);
	ut_assert_eq(cnt, 1);
}

static void avl_remove_exists(struct voluta_avl *avl, long key)
{
	struct voluta_avl_node *an;

	an = voluta_avl_find(avl, &key);
	check_node(an, key);

	voluta_avl_remove(avl, an);

	an = voluta_avl_find(avl, &key);
	ut_assert_null(an);
}

static size_t avl_size(const struct voluta_avl *avl)
{
	return voluta_avl_size(avl);
}

static struct voluta_avl_node *avl_begin(const struct voluta_avl *avl)
{
	return voluta_avl_begin(avl);
}

static struct voluta_avl_node *avl_end(const struct voluta_avl *avl)
{
	return voluta_avl_end(avl);
}

static struct voluta_avl_node *
avl_next(const struct voluta_avl *avl, const struct voluta_avl_node *x)
{
	return voluta_avl_next(avl, x);
}

static struct voluta_avl_node *
avl_prev(const struct voluta_avl *avl, const struct voluta_avl_node *x)
{
	return voluta_avl_prev(avl, x);
}

static long avl_min_key(const struct voluta_avl *avl)
{
	const struct ut_zrecord *zr;
	const struct voluta_avl_node *beg;

	ut_assert(avl->size > 0);

	beg = avl_begin(avl);
	ut_assert(beg != avl_end(avl));
	zr = avl_node_to_zrecord(beg);

	return zr->key;
}

static void avl_iterate_range(const struct voluta_avl *avl,
			      struct voluta_avl_node *beg,
			      const struct voluta_avl_node *end,
			      size_t expected_cnt, long key_beg, long step)
{
	size_t cnt;
	long key = key_beg;
	struct voluta_avl_node *itr = beg;

	cnt = 0;
	while (itr != end) {
		check_node(itr, key);

		key += step;
		cnt++;
		itr = avl_next(avl, itr);
	}
	ut_assert_eq(cnt, expected_cnt);

	while (itr != beg) {
		key -= step;
		cnt--;
		itr = avl_prev(avl, itr);
		check_node(itr, key);
	}
	ut_assert_eq(cnt, 0);
}

static void avl_iterate_all(const struct voluta_avl *avl,
			    long key_beg, long step)
{
	avl_iterate_range(avl, avl_begin(avl), avl_end(avl),
			  avl_size(avl), key_beg, step);
}

static void avl_iterate_seq(const struct voluta_avl *avl)
{
	avl_iterate_all(avl, avl_min_key(avl), 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_avl_simple_(struct ut_env *ut_env,
			   size_t cnt, long key_base, long step)
{
	long key;
	struct voluta_avl *avl;

	avl = avl_new(ut_env);

	key = key_base;
	for (size_t i = 0; i < cnt; ++i) {
		avl_insert_unique(avl, key);
		avl_iterate_all(avl, key_base, step);
		avl_find_unique(avl, key);
		key += step;
	}
	key = key_base;
	for (size_t i = 0; i < cnt; ++i) {
		avl_insert_replace(avl, key);
		key += step;
	}
	key = key_base;
	for (size_t i = 0; i < cnt; ++i) {
		avl_find_exists(avl, key);
		key += step;
	}
	key = key_base;
	for (size_t i = 0; i < cnt; ++i) {
		avl_find_unique(avl, key);
		avl_remove_exists(avl, key);
		key += step;
	}
	avl_done(avl);
}

static void ut_avl_simple(struct ut_env *ut_env)
{
	ut_avl_simple_(ut_env, 10, 1, 1);
	ut_avl_simple_(ut_env, 1111, 111, 11);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_avl_mixed_(struct ut_env *ut_env,
			  size_t cnt, long key_base, long step)
{
	long key;
	struct voluta_avl *avl;

	avl = avl_new(ut_env);
	key = key_base;
	for (size_t i = 0; i < cnt; ++i) {
		avl_insert_unique(avl, key);
		key += (2 * step);
	}
	key = key_base;
	for (size_t i = 0; i < cnt; ++i) {
		avl_remove_exists(avl, key);
		key += step;
		avl_insert_unique(avl, key);
		key += step;
	}
	key = key_base;
	for (size_t i = 0; i < cnt; ++i) {
		key += step;
		avl_remove_exists(avl, key);
		key += step;
		avl_find_non_exists(avl, key);
	}
	avl_done(avl);
}

static void ut_avl_mixed(struct ut_env *ut_env)
{
	ut_avl_mixed_(ut_env, 8, 1, 2);
	ut_avl_mixed_(ut_env, 1111, 111, 11);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static long *random_keys(struct ut_env *ut_env, size_t cnt, long base)
{
	return ut_randseq(ut_env, cnt, base);
}

static void ut_avl_random_(struct ut_env *ut_env, size_t cnt)
{
	long key;
	struct voluta_avl *avl;
	const long base = 100000;
	const long *keys = random_keys(ut_env, cnt, base);

	avl = avl_new(ut_env);
	for (size_t i = 0; i < cnt; ++i) {
		key = keys[i];
		avl_insert_unique(avl, key);
	}
	for (size_t i = 0; i < cnt; ++i) {
		key = base + (long)i;
		avl_find_exists(avl, key);
	}
	avl_iterate_seq(avl);

	for (size_t i = 0; i < cnt; i += 2) {
		key = keys[i];
		avl_remove_exists(avl, key);
	}
	for (size_t i = 1; i < cnt; i += 2) {
		key = keys[i];
		avl_remove_exists(avl, key);
	}
	avl_done(avl);
}

static void ut_avl_random(struct ut_env *ut_env)
{
	ut_avl_random_(ut_env, 10);
	ut_avl_random_(ut_env, 1000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_avl_simple),
	UT_DEFTEST(ut_avl_mixed),
	UT_DEFTEST(ut_avl_random),
};

const struct ut_tests ut_test_avl = UT_MKTESTS(ut_local_tests);


