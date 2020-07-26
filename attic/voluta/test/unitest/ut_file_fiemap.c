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


static struct fiemap *new_fiemap(struct ut_env *ut_env, size_t cnt)
{
	struct fiemap *fm = NULL;
	const size_t sz = sizeof(*fm) + (cnt * sizeof(fm->fm_extents[0]));

	fm = ut_zerobuf(ut_env, sz);
	fm->fm_extent_count = (uint32_t)cnt;

	return fm;
}

static struct fiemap *
ut_fiemap_of(struct ut_env *ut_env, ino_t ino, loff_t off, size_t len)
{
	loff_t pos;
	struct fiemap fm0 = {
		.fm_start = (uint64_t)off,
		.fm_length = len,
		.fm_flags = 0,
		.fm_extent_count = 0
	};
	struct fiemap *fm = NULL;
	const struct fiemap_extent *fm_ext = NULL;

	ut_fiemap_ok(ut_env, ino, &fm0);
	ut_assert_null(fm);

	fm = new_fiemap(ut_env, fm0.fm_mapped_extents);
	fm->fm_start = (uint64_t)off;
	fm->fm_length = len;
	ut_fiemap_ok(ut_env, ino, fm);

	pos = off;
	for (size_t i = 0; i < fm->fm_mapped_extents; ++i) {
		fm_ext = &fm->fm_extents[i];
		ut_assert_gt(fm_ext->fe_physical, 0);
		ut_assert_le(fm_ext->fe_length, len);

		if (i == 0) {
			ut_assert_ge(fm_ext->fe_logical, pos);
		} else {
			ut_assert_gt(fm_ext->fe_logical, pos);
		}
		pos = (loff_t)(fm_ext->fe_logical);
	}
	return fm;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_fiemap_simple_(struct ut_env *ut_env,
				   loff_t off, size_t bsz)
{
	ino_t ino;
	ino_t dino;
	const char *name = UT_NAME;
	void *buf = ut_randbuf(ut_env, bsz);
	const struct fiemap *fm = NULL;
	const struct fiemap_extent *fm_ext = NULL;

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_create_file(ut_env, dino, name, &ino);
	ut_write_read(ut_env, ino, buf, bsz, off);

	fm = ut_fiemap_of(ut_env, ino, off, bsz);
	ut_assert_ge(fm->fm_mapped_extents, 1);

	ut_trunacate_file(ut_env, ino, off + 1);
	fm = ut_fiemap_of(ut_env, ino, off, bsz);
	ut_assert_eq(fm->fm_mapped_extents, 1);
	fm_ext = &fm->fm_extents[0];
	ut_assert_eq(fm_ext->fe_logical, off);
	ut_assert_eq(fm_ext->fe_length, 1);

	ut_trunacate_file(ut_env, ino, off);
	fm = ut_fiemap_of(ut_env, ino, off, bsz);
	ut_assert_eq(fm->fm_mapped_extents, 0);

	ut_trunacate_file(ut_env, ino, off + (loff_t)bsz);
	fm = ut_fiemap_of(ut_env, ino, off, bsz);
	ut_assert_eq(fm->fm_mapped_extents, 0);

	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_at_root(ut_env, name);
}

static void ut_file_fiemap_simple(struct ut_env *ut_env)
{
	ut_file_fiemap_simple_(ut_env, 0, 100);
	ut_file_fiemap_simple_(ut_env, 0, UT_BK_SIZE);
	ut_file_fiemap_simple_(ut_env, UT_BK_SIZE, UT_MEGA);
	ut_file_fiemap_simple_(ut_env, UT_MEGA, UT_BK_SIZE);
	ut_file_fiemap_simple_(ut_env, UT_GIGA - UT_BK_SIZE, UT_MEGA);
	ut_file_fiemap_simple_(ut_env, UT_TERA - UT_BK_SIZE, 2 * UT_BK_SIZE);
	ut_file_fiemap_simple_(ut_env, UT_FILESIZE_MAX - UT_MEGA + 1,
			       UT_MEGA - 1);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_fiemap_sparse_(struct ut_env *ut_env,
				   loff_t off_base, loff_t step, size_t cnt)
{
	ino_t ino;
	ino_t dino;
	loff_t off;
	loff_t off_ext;
	loff_t boff;
	size_t len;
	char b = 'b';
	const char *name = UT_NAME;
	const struct fiemap *fm = NULL;
	const struct fiemap_extent *fm_ext = NULL;
	const loff_t bk_size = UT_BK_SIZE;
	const loff_t off_end = off_base + (step * (loff_t)cnt);

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_create_file(ut_env, dino, name, &ino);

	off = off_base;
	for (size_t i = 0; i < cnt; ++i) {
		ut_write_read(ut_env, ino, &b, 1, off);
		len = ut_off_len(off, off_end);
		fm = ut_fiemap_of(ut_env, ino, off, len);
		ut_assert_eq(fm->fm_mapped_extents, 1);
		off += step;
	}
	len = ut_off_len(off_base, off_end);
	fm = ut_fiemap_of(ut_env, ino, off_base, len);
	ut_assert_ge(fm->fm_mapped_extents, cnt);
	off = off_base;
	for (size_t i = 0; i < cnt; ++i) {
		fm_ext = &fm->fm_extents[i];
		off_ext = (loff_t)fm_ext->fe_logical;
		ut_assert_eq(ut_off_baligned(off), ut_off_baligned(off_ext));
		ut_assert_le(fm_ext->fe_length, bk_size);
		off += step;
	}

	off = off_base;
	for (size_t i = 0; i < cnt; ++i) {
		boff = ut_off_baligned(off);
		ut_fallocate_punch_hole(ut_env, ino, boff, bk_size);
		len = ut_off_len(off_base, off + 1);
		fm = ut_fiemap_of(ut_env, ino, off_base, len);
		ut_assert_eq(fm->fm_mapped_extents, 0);
		len = ut_off_len(off_base, off_end);
		fm = ut_fiemap_of(ut_env, ino, off_base, len);
		ut_assert_eq(fm->fm_mapped_extents, cnt - i - 1);
		off += step;
	}
	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_at_root(ut_env, name);
}

static void ut_file_fiemap_sparse(struct ut_env *ut_env)
{
	ut_file_fiemap_sparse_(ut_env, 0, 100, 1);
	ut_file_fiemap_sparse_(ut_env, 1, 1000, 1);
	ut_file_fiemap_sparse_(ut_env, 0, UT_BK_SIZE, 8);
	ut_file_fiemap_sparse_(ut_env, 1, UT_MEGA, 16);
	ut_file_fiemap_sparse_(ut_env, UT_GIGA, UT_MEGA, 32);
	ut_file_fiemap_sparse_(ut_env, UT_TERA - 1, UT_GIGA + 3, 64);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_file_fiemap_simple),
	UT_DEFTEST(ut_file_fiemap_sparse),
};

const struct ut_tests ut_test_file_fiemap = UT_MKTESTS(ut_local_tests);
