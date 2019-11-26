/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * This file is part of voluta.
 *
 * Copyright (C) 2019 Shachar Sharon
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

#define BK_SIZE VOLUTA_BK_SIZE
#define MAGIC   0xDEADBEEF

#define STATICASSERT_EQ(x_, y_) \
	VOLUTA_STATICASSERT_EQ(x_, y_)

#define STATICASSERT_SIZEOF(t_, s_) \
	STATICASSERT_EQ(sizeof(t_), s_)


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct voluta_ut_alloc_info {
	long    magic;
	struct voluta_qmalloc *qmal;
	struct voluta_list_head link;
	void  *mem;
	size_t len;
	size_t dat_len;
	char   dat[8];
};


static void alloc_info_setup(struct voluta_ut_alloc_info *ai, void *mem,
			     size_t len)
{
	STATICASSERT_SIZEOF(*ai, VOLUTA_CACHELINE_SIZE_MIN);

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

static struct voluta_ut_alloc_info *link_to_alloc_info(const struct
		voluta_list_head *link)
{
	const struct voluta_ut_alloc_info *ai =
		voluta_container_of(link, struct voluta_ut_alloc_info, link);

	alloc_info_check(ai);
	return (struct voluta_ut_alloc_info *)ai;
}

static struct voluta_ut_alloc_info *alloc_info_new(struct voluta_qmalloc *qmal,
		size_t msz)
{
	int err;
	void *mem = NULL;
	struct voluta_ut_alloc_info *ai;

	err = voluta_malloc(qmal, msz, &mem);
	ut_assert_ok(err);
	ut_assert_notnull(mem);

	ai = alloc_info_of(mem, msz);
	ai->qmal = qmal;

	return ai;
}

static void alloc_info_del(struct voluta_ut_alloc_info *ai)
{
	int err;
	struct voluta_qmalloc *qmal = ai->qmal;

	alloc_info_check(ai);
	err = voluta_free(qmal, ai->mem, ai->len);
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

static struct voluta_qmalloc *
ut_new_qmalloc(struct voluta_ut_ctx *ut_ctx, size_t sz)
{
	int err;
	struct voluta_qmalloc *qmal = NULL;

	voluta_unused(ut_ctx);
	err = voluta_qmalloc_new(sz, &qmal);
	ut_assert_ok(err);

	return qmal;
}

static void ut_del_qmalloc(struct voluta_qmalloc *qmal)
{
	int err;

	err = voluta_qmalloc_del(qmal);
	ut_assert_ok(err);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_alloc_nbks_simple(struct voluta_ut_ctx *ut_ctx)
{
	struct voluta_qmalloc *qmal;
	struct voluta_ut_alloc_info *ai;
	struct voluta_list_head lst, *lnk;
	size_t sizes[] = {
		BK_SIZE - 1, BK_SIZE, BK_SIZE + 1,
		2 * BK_SIZE, 8 * BK_SIZE - 1
	};

	voluta_list_init(&lst);
	qmal = ut_new_qmalloc(ut_ctx, 32 * VOLUTA_MEGA);
	for (size_t i = 0; i < VOLUTA_ARRAY_SIZE(sizes); ++i) {
		ai = alloc_info_new(qmal, sizes[i]);
		memset(ai->dat, (int)i, ai->dat_len);
		voluta_list_push_back(&lst, &ai->link);
	}
	lnk = lst.next;
	while (lnk != &lst) {
		link_alloc_info_del(lnk);
		lnk = lst.next;
	}
	ut_del_qmalloc(qmal);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t align_up(size_t nn, size_t align)
{
	return ((nn + align - 1) / align) * align;
}

static void ut_alloc_free_nbks(struct voluta_ut_ctx *ut_ctx)
{
	size_t msz, rem, total = 0;
	struct voluta_qmalloc *qmal;
	struct voluta_ut_alloc_info *ai;
	struct voluta_list_head lst, *lnk;
	const size_t bk_size =  VOLUTA_BK_SIZE;
	struct voluta_qmstat qmst;

	voluta_list_init(&lst);
	qmal = ut_new_qmalloc(ut_ctx, 32 * VOLUTA_MEGA);
	voluta_qmstat(qmal, &qmst);
	while (total < qmst.memsz_data) {
		rem = qmst.memsz_data - total;
		msz = sizeof(*ai) + (bk_size / 2) + (total % 10000);
		msz = voluta_clamp(msz, (bk_size / 2) + 1, rem);

		ai = alloc_info_new(qmal, msz);
		voluta_list_push_back(&lst, &ai->link);

		total += align_up(msz, bk_size);
		voluta_qmstat(qmal, &qmst);
	}
	lnk = lst.next;
	while (lnk != &lst) {
		link_alloc_info_del(lnk);
		lnk = lst.next;
	}
	ut_del_qmalloc(qmal);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_alloc_slab_elems(struct voluta_ut_ctx *ut_ctx)
{
	size_t val, msz;
	struct voluta_qmalloc *qmal;
	struct voluta_ut_alloc_info *ai;
	struct voluta_list_head lst, *lnk;
	const size_t pg_size = VOLUTA_PAGE_SIZE_MIN;

	voluta_list_init(&lst);
	qmal = ut_new_qmalloc(ut_ctx, 64 * VOLUTA_MEGA);
	for (size_t i = 0; i < 10000; ++i) {
		val = (pg_size + i) % (pg_size / 2);
		msz = voluta_clamp(val, sizeof(*ai), (pg_size / 2));

		ai = alloc_info_new(qmal, msz);
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
	ut_del_qmalloc(qmal);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_alloc_mixed(struct voluta_ut_ctx *ut_ctx)
{
	size_t msz, val, val2, val_max = 100000;
	struct voluta_qmalloc *qmal;
	const size_t bk_size = VOLUTA_BK_SIZE;
	struct voluta_ut_alloc_info *ai;
	struct voluta_list_head lst, *lnk;

	voluta_list_init(&lst);
	qmal = ut_new_qmalloc(ut_ctx, 256 * VOLUTA_MEGA);
	for (val = 0; val < val_max; val += 100) {
		msz = voluta_clamp(val, sizeof(*ai), 11 * bk_size);
		ai = alloc_info_new(qmal, msz);
		voluta_list_push_back(&lst, &ai->link);

		if ((val % 11) == 1) {
			link_alloc_info_del(lst.next);
		}

		val2 = (val_max - val) / 2;
		msz = voluta_clamp(val2, sizeof(*ai), 11 * bk_size);
		ai = alloc_info_new(qmal, msz);
		voluta_list_push_back(&lst, &ai->link);
	}
	lnk = lst.next;
	while (lnk != &lst) {
		link_alloc_info_del(lnk);
		lnk = lst.next->next;
	}
	ut_del_qmalloc(qmal);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_alloc_nbks_simple),
	UT_DEFTEST(ut_alloc_free_nbks),
	UT_DEFTEST(ut_alloc_slab_elems),
	UT_DEFTEST(ut_alloc_mixed),
};

const struct voluta_ut_tests voluta_ut_alloc_tests =
	UT_MKTESTS(ut_local_tests);


