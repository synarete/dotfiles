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

#define MKRANGE(pos_, cnt_) { .off = (pos_), .len = (cnt_) }
#define MKRANGES(a_) { .arr = (a_), .cnt = ARRAY_SIZE(a_) }


struct voluta_ut_iobufs {
	struct voluta_ut_ctx *ut_ctx;
	size_t count;
	struct voluta_ut_iobuf *iobuf[64];
};


static struct voluta_ut_iobufs *new_iobufs(struct voluta_ut_ctx *ut_ctx)
{
	struct voluta_ut_iobufs *iobufs;

	iobufs = voluta_ut_zalloc(ut_ctx, sizeof(*iobufs));
	iobufs->ut_ctx = ut_ctx;
	iobufs->count = 0;
	return iobufs;
}

static void assign(struct voluta_ut_iobufs *drefs,
		   const struct voluta_ut_ranges *rngs)
{
	loff_t off;
	size_t len;
	struct voluta_ut_iobuf *iobuf;

	ut_assert(rngs->cnt <= ARRAY_SIZE(drefs->iobuf));

	for (size_t i = 0; i < rngs->cnt; ++i) {
		off = rngs->arr[i].off;
		len = rngs->arr[i].len;
		iobuf = voluta_ut_new_iobuf(drefs->ut_ctx, off, len);
		drefs->iobuf[drefs->count++] = iobuf;
	}
}

static struct voluta_ut_iobufs *
simple(struct voluta_ut_ctx *ut_ctx, const struct voluta_ut_ranges *ranges)
{
	struct voluta_ut_iobufs *drefs = new_iobufs(ut_ctx);

	assign(drefs, ranges);
	return drefs;
}

static void swap(struct voluta_ut_iobuf **pa, struct voluta_ut_iobuf **pb)
{
	struct voluta_ut_iobuf *c = *pa;

	*pa = *pb;
	*pb = c;
}

static struct voluta_ut_iobufs *
reverse(struct voluta_ut_ctx *ut_ctx, const struct voluta_ut_ranges *ranges)
{
	struct voluta_ut_iobufs *iobufs = simple(ut_ctx, ranges);

	for (size_t i = 0; i < iobufs->count / 2; ++i) {
		swap(&iobufs->iobuf[i], &iobufs->iobuf[iobufs->count - i - 1]);
	}
	return iobufs;
}

static struct voluta_ut_iobufs *
zigzag(struct voluta_ut_ctx *ut_ctx, const struct voluta_ut_ranges *ranges)
{
	struct voluta_ut_iobufs *iobufs = simple(ut_ctx, ranges);

	for (size_t i = 0; i < iobufs->count - 1; i += 2) {
		swap(&iobufs->iobuf[i], &iobufs->iobuf[i + 1]);
	}
	return iobufs;
}

static struct voluta_ut_iobufs *
rzigzag(struct voluta_ut_ctx *ut_ctx, const struct voluta_ut_ranges *ranges)
{
	struct voluta_ut_iobufs *iobufs = reverse(ut_ctx, ranges);

	for (size_t i = 0; i < iobufs->count - 1; i += 2) {
		swap(&iobufs->iobuf[i], &iobufs->iobuf[i + 1]);
	}
	return iobufs;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static ino_t create_file(struct voluta_ut_ctx *ut_ctx, const char *name)
{
	int err;
	struct stat st;
	ino_t root_ino = ROOT_INO;

	err = voluta_ut_create(ut_ctx, root_ino, name, S_IFREG | 0600, &st);
	ut_assert_ok(err);

	return st.st_ino;
}

static void remove_file(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			const char *name)
{
	int err;
	struct stat st;
	ino_t root_ino = ROOT_INO;

	err = voluta_ut_release(ut_ctx, ino);
	ut_assert_ok(err);

	err = voluta_ut_unlink(ut_ctx, root_ino, name);
	ut_assert_ok(err);

	err = voluta_ut_lookup(ut_ctx, root_ino, name, &st);
	ut_assert_err(err, -ENOENT);
}

static void test_write_read(struct voluta_ut_ctx *ut_ctx,
			    const struct voluta_ut_iobufs *iobufs, ino_t ino)
{
	int err;
	void *buf;
	loff_t off;
	size_t nrd, nwr, len;
	const struct voluta_ut_iobuf *iobuf;

	for (size_t i = 0; i < iobufs->count; ++i) {
		iobuf = iobufs->iobuf[i];
		len = iobuf->len;
		off = iobuf->off;
		err = voluta_ut_write(ut_ctx, ino, iobuf->buf, len, off, &nwr);
		ut_assert_ok(err);
		ut_assert_eq(nwr, len);
	}

	for (size_t j = 0; j < iobufs->count; ++j) {
		iobuf = iobufs->iobuf[j];
		len = iobuf->len;
		off = iobuf->off;
		buf = ut_randbuf(ut_ctx, len);
		err = voluta_ut_read(ut_ctx, ino, buf, len, off, &nrd);
		ut_assert_ok(err);
		ut_assert_eq(nrd, len);
		ut_assert_eqm(buf, iobuf->buf, len);
	}
}


static void ut_file_1(struct voluta_ut_ctx *ut_ctx,
		      const struct voluta_ut_iobufs *drefs)
{
	ino_t ino;
	const char *name = T_NAME;

	ino = create_file(ut_ctx, name);
	test_write_read(ut_ctx, drefs, ino);
	remove_file(ut_ctx, ino, name);
}

static void ut_file_2(struct voluta_ut_ctx *ut_ctx,
		      const struct voluta_ut_iobufs *drefs1,
		      const struct voluta_ut_iobufs *drefs2)
{
	ino_t ino;
	const char *name = T_NAME;

	ino = create_file(ut_ctx, name);
	test_write_read(ut_ctx, drefs1, ino);
	test_write_read(ut_ctx, drefs2, ino);
	remove_file(ut_ctx, ino, name);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_range s_ut_ranges1[] = {
	MKRANGE(1, 1),
	MKRANGE(3, 4),
	MKRANGE(8, 16),
	MKRANGE(29, 31),
	MKRANGE(127, 111),
};

static const struct voluta_ut_range s_ut_ranges2[] = {
	MKRANGE(BK_SIZE, BK_SIZE),
	MKRANGE(MEGA, BK_SIZE),
	MKRANGE(GIGA, BK_SIZE),
	MKRANGE(TERA, BK_SIZE)
};

static const struct voluta_ut_range s_ut_ranges3[] = {
	MKRANGE(BK_SIZE - 1, BK_SIZE + 3),
	MKRANGE(MEGA - 1, BK_SIZE + 3),
	MKRANGE(GIGA - 1, BK_SIZE + 3),
	MKRANGE(TERA - 1, BK_SIZE + 3)
};

static const struct voluta_ut_range s_ut_ranges4[] = {
	MKRANGE(BK_SIZE, UMEGA),
	MKRANGE(3 * MEGA - 3, UMEGA / 3),
	MKRANGE(5 * MEGA + 5, UMEGA / 5),
	MKRANGE(7 * MEGA - 7, UMEGA / 7),
	MKRANGE(11 * MEGA + 11, UMEGA / 11),
	MKRANGE(13 * MEGA - 13, UMEGA / 13),
};

static const struct voluta_ut_range s_ut_ranges5[] = {
	MKRANGE(1, 11),
	MKRANGE(23 * BK_SIZE, 2 * BK_SIZE),
	MKRANGE(31 * MEGA - 3, 3 * BK_SIZE),
	MKRANGE(677 * MEGA - 3, 3 * BK_SIZE),
	MKRANGE(47 * GIGA - 4, 4 * BK_SIZE),
	MKRANGE(977 * GIGA - 4, 4 * BK_SIZE),
	MKRANGE(5 * TERA - 5, 5 * BK_SIZE),
};

static const struct voluta_ut_range s_ut_ranges6[] = {
	MKRANGE((MEGA / 23) - 23, UMEGA),
	MKRANGE(23 * MEGA + 23, UMEGA),
	MKRANGE(113 * GIGA - 113, UMEGA),
	MKRANGE(223 * GIGA + 223, UMEGA),
};

static const struct voluta_ut_ranges s_ranges_defs[] = {
	MKRANGES(s_ut_ranges1),
	MKRANGES(s_ut_ranges2),
	MKRANGES(s_ut_ranges3),
	MKRANGES(s_ut_ranges4),
	MKRANGES(s_ut_ranges5),
	MKRANGES(s_ut_ranges6),
};


static void ut_file_ranges_(struct voluta_ut_ctx *ut_ctx,
			    const struct voluta_ut_ranges *ranges)
{
	ut_file_1(ut_ctx, simple(ut_ctx, ranges));
	ut_file_1(ut_ctx, reverse(ut_ctx, ranges));
	ut_file_1(ut_ctx, zigzag(ut_ctx, ranges));
	ut_file_1(ut_ctx, rzigzag(ut_ctx, ranges));
}

static void ut_file_ranges(struct voluta_ut_ctx *ut_ctx)
{
	const struct voluta_ut_ranges *ranges;

	for (size_t i = 0; i < ARRAY_SIZE(s_ranges_defs); ++i) {
		ranges = &s_ranges_defs[i];
		ut_file_ranges_(ut_ctx, ranges);
	}
}

static void ut_file_xranges_(struct voluta_ut_ctx *ut_ctx,
			     const struct voluta_ut_ranges *ranges1,
			     const struct voluta_ut_ranges *ranges2)
{
	ut_file_2(ut_ctx, simple(ut_ctx, ranges1), simple(ut_ctx, ranges2));
	ut_file_2(ut_ctx, reverse(ut_ctx, ranges1), reverse(ut_ctx, ranges2));
	ut_file_2(ut_ctx, zigzag(ut_ctx, ranges1), zigzag(ut_ctx, ranges2));
	ut_file_2(ut_ctx, rzigzag(ut_ctx, ranges1), rzigzag(ut_ctx, ranges2));
	ut_file_2(ut_ctx, reverse(ut_ctx, ranges1), zigzag(ut_ctx, ranges2));
	ut_file_2(ut_ctx, rzigzag(ut_ctx, ranges1), simple(ut_ctx, ranges2));
}

static void ut_file_xranges(struct voluta_ut_ctx *ut_ctx)
{
	const struct voluta_ut_ranges *ranges1;
	const struct voluta_ut_ranges *ranges2;

	for (size_t j = 0; j < ARRAY_SIZE(s_ranges_defs) - 1; ++j) {
		ranges1 = &s_ranges_defs[j];
		ranges2 = &s_ranges_defs[j + 1];
		ut_file_xranges_(ut_ctx, ranges1, ranges2);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_file_ranges),
	UT_DEFTEST(ut_file_xranges),
};

const struct voluta_ut_tests voluta_ut_utest_file_ranges =
	UT_MKTESTS(ut_local_tests);
