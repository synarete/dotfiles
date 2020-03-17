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
#define VOLUTA_TEST 1
#include "unitest.h"

#define MAGIC   0xDEADBEEF

#define STATICASSERT_EQ(x_, y_) \
	VOLUTA_STATICASSERT_EQ(x_, y_)

#define STATICASSERT_SIZEOF(t_, s_) \
	STATICASSERT_EQ(sizeof(t_), s_)


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct voluta_ut_alloc_info {
	long    magic;
	struct voluta_qalloc *qal;
	struct voluta_list_head link;
	void  *mem;
	size_t len;
	size_t dat_len;
	char   dat[8];
};


static void alloc_info_setup(struct voluta_ut_alloc_info *ai,
			     void *mem, size_t len)
{
	STATICASSERT_SIZEOF(*ai, 64);

	ut_assert_ge(len, sizeof(*ai));
	voluta_list_head_init(&ai->link);
	ai->magic = MAGIC;
	ai->mem = mem;
	ai->len = len;
	ai->dat_len = len - offsetof(struct voluta_ut_alloc_info, dat);
}

static struct voluta_ut_alloc_info *alloc_info_of(void *mem, size_t len)
{
	struct voluta_ut_alloc_info *ai = mem;

	alloc_info_setup(ai, mem, len);
	return ai;
}

static void alloc_info_check(const struct voluta_ut_alloc_info *ai)
{
	ut_assert_eq(ai->magic, MAGIC);
	ut_assert_ge(ai->len, sizeof(*ai));
	ut_assert_notnull(ai->mem);
}

static struct voluta_ut_alloc_info *
link_to_alloc_info(const struct voluta_list_head *link)
{
	const struct voluta_ut_alloc_info *ai =
		voluta_container_of(link, struct voluta_ut_alloc_info, link);

	alloc_info_check(ai);
	return (struct voluta_ut_alloc_info *)ai;
}

static struct voluta_ut_alloc_info *alloc_info_new(struct voluta_qalloc *qal,
		size_t msz)
{
	int err;
	void *mem = NULL;
	struct voluta_ut_alloc_info *ai;

	err = voluta_malloc(qal, msz, &mem);
	ut_assert_ok(err);
	ut_assert_notnull(mem);

	ai = alloc_info_of(mem, msz);
	ai->qal = qal;

	return ai;
}

static void alloc_info_del(struct voluta_ut_alloc_info *ai)
{
	int err;
	struct voluta_qalloc *qal = ai->qal;

	alloc_info_check(ai);
	err = voluta_free(qal, ai->mem, ai->len);
	ut_assert_ok(err);
}

static void link_alloc_info_del(struct voluta_list_head *link)
{
	struct voluta_ut_alloc_info *ai;

	ai = link_to_alloc_info(link);
	voluta_list_head_remove(link);
	alloc_info_del(ai);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_qalloc *
ut_new_qalloc(struct voluta_ut_ctx *ut_ctx, size_t sz)
{
	int err;
	struct voluta_qalloc *qal = NULL;

	voluta_unused(ut_ctx);
	err = voluta_qalloc_new(sz, &qal);
	ut_assert_ok(err);

	return qal;
}

static void ut_del_qalloc(struct voluta_qalloc *qal)
{
	int err;

	err = voluta_qalloc_del(qal);
	ut_assert_ok(err);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_alloc_nbks_simple(struct voluta_ut_ctx *ut_ctx)
{
	struct voluta_qalloc *qal;
	struct voluta_ut_alloc_info *ai;
	struct voluta_list_head lst, *lnk;
	size_t sizes[] = {
		BK_SIZE - 1, BK_SIZE, BK_SIZE + 1,
		2 * BK_SIZE, 8 * BK_SIZE - 1
	};

	voluta_list_init(&lst);
	qal = ut_new_qalloc(ut_ctx, 32 * UMEGA);
	for (size_t i = 0; i < ARRAY_SIZE(sizes); ++i) {
		ai = alloc_info_new(qal, sizes[i]);
		memset(ai->dat, (int)i, ai->dat_len);
		voluta_list_push_back(&lst, &ai->link);
	}
	lnk = lst.next;
	while (lnk != &lst) {
		link_alloc_info_del(lnk);
		lnk = lst.next;
	}
	ut_del_qalloc(qal);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t align_up(size_t nn, size_t align)
{
	return ((nn + align - 1) / align) * align;
}

static void ut_alloc_free_nbks(struct voluta_ut_ctx *ut_ctx)
{
	size_t msz, rem, total = 0;
	struct voluta_qalloc *qal;
	struct voluta_ut_alloc_info *ai;
	struct voluta_list_head lst, *lnk;
	const size_t bk_size =  BK_SIZE;
	struct voluta_qmstat qmst;

	voluta_list_init(&lst);
	qal = ut_new_qalloc(ut_ctx, 32 * UMEGA);
	voluta_qmstat(qal, &qmst);
	while (total < qmst.memsz_data) {
		rem = qmst.memsz_data - total;
		msz = sizeof(*ai) + (bk_size / 2) + (total % 10000);
		msz = voluta_clamp(msz, (bk_size / 2) + 1, rem);

		ai = alloc_info_new(qal, msz);
		voluta_list_push_back(&lst, &ai->link);

		total += align_up(msz, bk_size);
		voluta_qmstat(qal, &qmst);
	}
	lnk = lst.next;
	while (lnk != &lst) {
		link_alloc_info_del(lnk);
		lnk = lst.next;
	}
	ut_del_qalloc(qal);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_alloc_slab_elems(struct voluta_ut_ctx *ut_ctx)
{
	size_t val, msz;
	struct voluta_qalloc *qal;
	struct voluta_ut_alloc_info *ai;
	struct voluta_list_head lst, *lnk;
	const size_t pg_size = VOLUTA_PAGE_SIZE_MIN;

	voluta_list_init(&lst);
	qal = ut_new_qalloc(ut_ctx, 64 * UMEGA);
	for (size_t i = 0; i < 10000; ++i) {
		val = (pg_size + i) % (pg_size / 2);
		msz = voluta_clamp(val, sizeof(*ai), (pg_size / 2));
		ai = alloc_info_new(qal, msz);
		memset(ai->dat, (int)i, ai->dat_len);
		voluta_list_push_back(&lst, &ai->link);

		if ((i % 7) == 1) {
			link_alloc_info_del(lst.next);
		}
	}
	lnk = lst.next;
	while (lnk != &lst) {
		link_alloc_info_del(lnk);
		lnk = lst.next->next;
	}
	ut_del_qalloc(qal);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_alloc_mixed(struct voluta_ut_ctx *ut_ctx)
{
	size_t msz, val, val2, val_max = 100000;
	struct voluta_qalloc *qal;
	const size_t bk_size = BK_SIZE;
	struct voluta_ut_alloc_info *ai;
	struct voluta_list_head lst, *lnk;

	voluta_list_init(&lst);
	qal = ut_new_qalloc(ut_ctx, 256 * UMEGA);
	for (val = 0; val < val_max; val += 100) {
		msz = voluta_clamp(val, sizeof(*ai), 11 * bk_size);
		ai = alloc_info_new(qal, msz);
		voluta_list_push_back(&lst, &ai->link);

		if ((val % 11) == 1) {
			link_alloc_info_del(lst.next);
		}

		val2 = (val_max - val) / 2;
		msz = voluta_clamp(val2, sizeof(*ai), 11 * bk_size);
		ai = alloc_info_new(qal, msz);
		voluta_list_push_back(&lst, &ai->link);
	}
	lnk = lst.next;
	while (lnk != &lst) {
		link_alloc_info_del(lnk);
		lnk = lst.next->next;
	}
	ut_del_qalloc(qal);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

#define NALLOC_SMALL 128

static size_t small_alloc_size(size_t i)
{
	return (i * 17) + 1;
}

static void ut_alloc_small_sizes(struct voluta_ut_ctx *ut_ctx)
{
	int err;
	size_t i, j, idx, msz;
	struct voluta_qalloc *qal;
	void *mem, *ptr[NALLOC_SMALL];
	long idx_arr[NALLOC_SMALL];

	qal = ut_new_qalloc(ut_ctx, 32 * UMEGA);
	for (i = 0; i < ARRAY_SIZE(ptr); ++i) {
		msz = small_alloc_size(i);
		mem = NULL;
		err = voluta_malloc(qal, msz, &mem);
		ut_assert_ok(err);
		ut_assert_notnull(mem);
		memset(mem, (int)i, msz);
		ptr[i] = mem;
	}
	for (j = 0; j < ARRAY_SIZE(ptr); ++j) {
		msz = small_alloc_size(j);
		mem = ptr[j];
		voluta_free(qal, mem, msz);
		ptr[j] = NULL;
	}

	voluta_ut_prandom_seq(idx_arr, ARRAY_SIZE(idx_arr), 0);
	for (i = 0; i < ARRAY_SIZE(ptr); ++i) {
		idx = (size_t)idx_arr[i];
		msz = small_alloc_size(idx);
		mem = NULL;
		err = voluta_malloc(qal, msz, &mem);
		ut_assert_ok(err);
		ut_assert_notnull(mem);
		memset(mem, (int)i, msz);
		ptr[idx] = mem;
	}
	voluta_ut_prandom_seq(idx_arr, ARRAY_SIZE(idx_arr), 0);
	for (j = 0; j < ARRAY_SIZE(ptr); ++j) {
		idx = (size_t)idx_arr[j];
		msz = small_alloc_size(idx);
		mem = ptr[idx];
		voluta_free(qal, mem, msz);
		ptr[idx] = NULL;
	}
	ut_del_qalloc(qal);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_alloc_nbks_simple),
	UT_DEFTEST(ut_alloc_free_nbks),
	UT_DEFTEST(ut_alloc_slab_elems),
	UT_DEFTEST(ut_alloc_mixed),
	UT_DEFTEST(ut_alloc_small_sizes),
};

const struct voluta_ut_tests voluta_ut_utest_alloc =
	UT_MKTESTS(ut_local_tests);


