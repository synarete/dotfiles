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

#define MKRANGE(pos_, cnt_) { .off = (pos_), .len = (cnt_) }
#define MKRANGES(a_) { .arr = (a_), .cnt = UT_ARRAY_SIZE(a_) }


struct ut_dvecs {
	struct ut_dvec *dvec[64];
	size_t count;
};


static struct ut_dvecs *new_dvecs(struct ut_env *ut_env)
{
	struct ut_dvecs *dvecs;

	dvecs = ut_zalloc(ut_env, sizeof(*dvecs));
	dvecs->count = 0;
	return dvecs;
}

static void assign(struct ut_env *ut_env,
		   struct ut_dvecs *drefs,
		   const struct ut_ranges *rngs)
{
	loff_t off;
	size_t len;
	struct ut_dvec *dvec;

	ut_assert(rngs->cnt <= UT_ARRAY_SIZE(drefs->dvec));

	for (size_t i = 0; i < rngs->cnt; ++i) {
		off = rngs->arr[i].off;
		len = rngs->arr[i].len;
		dvec = ut_new_dvec(ut_env, off, len);
		drefs->dvec[drefs->count++] = dvec;
	}
}

static struct ut_dvecs *
simple(struct ut_env *ut_env, const struct ut_ranges *ranges)
{
	struct ut_dvecs *drefs = new_dvecs(ut_env);

	assign(ut_env, drefs, ranges);
	return drefs;
}

static void swap(struct ut_dvec **pa, struct ut_dvec **pb)
{
	struct ut_dvec *c = *pa;

	*pa = *pb;
	*pb = c;
}

static struct ut_dvecs *
reverse(struct ut_env *ut_env, const struct ut_ranges *ranges)
{
	struct ut_dvecs *dvecs = simple(ut_env, ranges);

	for (size_t i = 0; i < dvecs->count / 2; ++i) {
		swap(&dvecs->dvec[i], &dvecs->dvec[dvecs->count - i - 1]);
	}
	return dvecs;
}

static struct ut_dvecs *
zigzag(struct ut_env *ut_env, const struct ut_ranges *ranges)
{
	struct ut_dvecs *dvecs = simple(ut_env, ranges);

	for (size_t i = 0; i < dvecs->count - 1; i += 2) {
		swap(&dvecs->dvec[i], &dvecs->dvec[i + 1]);
	}
	return dvecs;
}

static struct ut_dvecs *
rzigzag(struct ut_env *ut_env, const struct ut_ranges *ranges)
{
	struct ut_dvecs *dvecs = reverse(ut_env, ranges);

	for (size_t i = 0; i < dvecs->count - 1; i += 2) {
		swap(&dvecs->dvec[i], &dvecs->dvec[i + 1]);
	}
	return dvecs;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_write_read(struct ut_env *ut_env,
			    const struct ut_dvecs *dvecs, ino_t ino)
{
	void *buf;
	loff_t off;
	size_t len;
	const struct ut_dvec *dvec;

	for (size_t i = 0; i < dvecs->count; ++i) {
		dvec = dvecs->dvec[i];
		len = dvec->len;
		off = dvec->off;
		ut_write_ok(ut_env, ino, dvec->dat, len, off);
	}

	for (size_t j = 0; j < dvecs->count; ++j) {
		dvec = dvecs->dvec[j];
		len = dvec->len;
		off = dvec->off;
		buf = ut_randbuf(ut_env, len);
		ut_read_ok(ut_env, ino, buf, len, off);
		ut_assert_eqm(buf, dvec->dat, len);
	}
}


static void ut_rdwr_file1(struct ut_env *ut_env,
			  const struct ut_dvecs *drefs)
{
	ino_t ino;
	ino_t dino;
	const char *name = UT_NAME;

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_create_file(ut_env, dino, name, &ino);
	test_write_read(ut_env, drefs, ino);
	ut_release_ok(ut_env, ino);
	ut_unlink_ok(ut_env, dino, name);
	ut_rmdir_at_root(ut_env, name);
}

static void ut_rdwr_file2(struct ut_env *ut_env,
			  const struct ut_dvecs *drefs1,
			  const struct ut_dvecs *drefs2)
{
	ino_t ino;
	ino_t dino;
	const char *name = UT_NAME;

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_create_file(ut_env, dino, name, &ino);
	test_write_read(ut_env, drefs1, ino);
	test_write_read(ut_env, drefs2, ino);
	ut_release_ok(ut_env, ino);
	ut_unlink_ok(ut_env, dino, name);
	ut_rmdir_at_root(ut_env, name);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct ut_range s_ut_ranges1[] = {
	MKRANGE(1, 1),
	MKRANGE(3, 4),
	MKRANGE(8, 16),
	MKRANGE(29, 31),
	MKRANGE(127, 111),
};

static const struct ut_range s_ut_ranges2[] = {
	MKRANGE(UT_BK_SIZE, UT_BK_SIZE),
	MKRANGE(UT_MEGA, UT_BK_SIZE),
	MKRANGE(UT_GIGA, UT_BK_SIZE),
	MKRANGE(UT_TERA, UT_BK_SIZE)
};

static const struct ut_range s_ut_ranges3[] = {
	MKRANGE(UT_BK_SIZE - 1, UT_BK_SIZE + 3),
	MKRANGE(UT_MEGA - 1, UT_BK_SIZE + 3),
	MKRANGE(UT_GIGA - 1, UT_BK_SIZE + 3),
	MKRANGE(UT_TERA - 1, UT_BK_SIZE + 3)
};

static const struct ut_range s_ut_ranges4[] = {
	MKRANGE(UT_BK_SIZE, UT_UMEGA),
	MKRANGE(3 * UT_MEGA - 3, UT_UMEGA / 3),
	MKRANGE(5 * UT_MEGA + 5, UT_UMEGA / 5),
	MKRANGE(7 * UT_MEGA - 7, UT_UMEGA / 7),
	MKRANGE(11 * UT_MEGA + 11, UT_UMEGA / 11),
	MKRANGE(13 * UT_MEGA - 13, UT_UMEGA / 13),
};

static const struct ut_range s_ut_ranges5[] = {
	MKRANGE(1, 11),
	MKRANGE(23 * UT_BK_SIZE, 2 * UT_BK_SIZE),
	MKRANGE(31 * UT_MEGA - 3, 3 * UT_BK_SIZE),
	MKRANGE(677 * UT_MEGA - 3, 3 * UT_BK_SIZE),
	MKRANGE(47 * UT_GIGA - 4, 4 * UT_BK_SIZE),
	MKRANGE(977 * UT_GIGA - 4, 4 * UT_BK_SIZE),
	MKRANGE(5 * UT_TERA - 5, 5 * UT_BK_SIZE),
};

static const struct ut_range s_ut_ranges6[] = {
	MKRANGE((UT_MEGA / 23) - 23, UT_UMEGA),
	MKRANGE(23 * UT_MEGA + 23, UT_UMEGA),
	MKRANGE(113 * UT_GIGA - 113, UT_UMEGA),
	MKRANGE(223 * UT_GIGA + 223, UT_UMEGA),
};

static const struct ut_ranges s_ranges_defs[] = {
	MKRANGES(s_ut_ranges1),
	MKRANGES(s_ut_ranges2),
	MKRANGES(s_ut_ranges3),
	MKRANGES(s_ut_ranges4),
	MKRANGES(s_ut_ranges5),
	MKRANGES(s_ut_ranges6),
};


static void ut_file_ranges_(struct ut_env *ut_env,
			    const struct ut_ranges *ranges)
{
	ut_rdwr_file1(ut_env, simple(ut_env, ranges));
	ut_rdwr_file1(ut_env, reverse(ut_env, ranges));
	ut_rdwr_file1(ut_env, zigzag(ut_env, ranges));
	ut_rdwr_file1(ut_env, rzigzag(ut_env, ranges));
}

static void ut_file_ranges(struct ut_env *ut_env)
{
	const struct ut_ranges *ranges;

	for (size_t i = 0; i < UT_ARRAY_SIZE(s_ranges_defs); ++i) {
		ranges = &s_ranges_defs[i];
		ut_file_ranges_(ut_env, ranges);
	}
}

static void ut_file_xranges_(struct ut_env *ut_env,
			     const struct ut_ranges *r1,
			     const struct ut_ranges *r2)
{
	ut_rdwr_file2(ut_env, simple(ut_env, r1), simple(ut_env, r2));
	ut_rdwr_file2(ut_env, reverse(ut_env, r1), reverse(ut_env, r2));
	ut_rdwr_file2(ut_env, zigzag(ut_env, r1), zigzag(ut_env, r2));
	ut_rdwr_file2(ut_env, rzigzag(ut_env, r1), rzigzag(ut_env, r2));
	ut_rdwr_file2(ut_env, reverse(ut_env, r1), zigzag(ut_env, r2));
	ut_rdwr_file2(ut_env, rzigzag(ut_env, r1), simple(ut_env, r2));
}

static void ut_file_xranges(struct ut_env *ut_env)
{
	const struct ut_ranges *r1;
	const struct ut_ranges *r2;

	for (size_t j = 0; j < UT_ARRAY_SIZE(s_ranges_defs) - 1; ++j) {
		r1 = &s_ranges_defs[j];
		r2 = &s_ranges_defs[j + 1];
		ut_file_xranges_(ut_env, r1, r2);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_file_ranges),
	UT_DEFTEST(ut_file_xranges),
};

const struct ut_tests ut_test_file_ranges = UT_MKTESTS(ut_local_tests);
