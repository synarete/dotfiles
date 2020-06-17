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


static struct fiemap *new_fiemap(struct voluta_ut_ctx *ut_ctx, size_t cnt)
{
	struct fiemap *fm = NULL;
	const size_t sz = sizeof(*fm) + (cnt * sizeof(fm->fm_extents[0]));

	fm = ut_zerobuf(ut_ctx, sz);
	fm->fm_extent_count = (uint32_t)cnt;

	return fm;
}

static struct fiemap *
ut_fiemap_of(struct voluta_ut_ctx *ut_ctx, ino_t ino, loff_t off, size_t len)
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

	voluta_ut_fiemap_ok(ut_ctx, ino, &fm0);
	ut_assert_null(fm);

	fm = new_fiemap(ut_ctx, fm0.fm_mapped_extents);
	fm->fm_start = (uint64_t)off;
	fm->fm_length = len;
	voluta_ut_fiemap_ok(ut_ctx, ino, fm);

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

static void ut_file_fiemap_simple_(struct voluta_ut_ctx *ut_ctx,
				   loff_t off, size_t bsz)
{
	ino_t ino;
	ino_t dino;
	const char *name = T_NAME;
	void *buf = ut_randbuf(ut_ctx, bsz);
	const struct fiemap *fm = NULL;
	const struct fiemap_extent *fm_ext = NULL;

	voluta_ut_mkdir_at_root(ut_ctx, name, &dino);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);
	voluta_ut_write_read(ut_ctx, ino, buf, bsz, off);

	fm = ut_fiemap_of(ut_ctx, ino, off, bsz);
	ut_assert_ge(fm->fm_mapped_extents, 1);

	voluta_ut_trunacate_file(ut_ctx, ino, off + 1);
	fm = ut_fiemap_of(ut_ctx, ino, off, bsz);
	ut_assert_eq(fm->fm_mapped_extents, 1);
	fm_ext = &fm->fm_extents[0];
	ut_assert_eq(fm_ext->fe_logical, off);
	ut_assert_eq(fm_ext->fe_length, 1);

	voluta_ut_trunacate_file(ut_ctx, ino, off);
	fm = ut_fiemap_of(ut_ctx, ino, off, bsz);
	ut_assert_eq(fm->fm_mapped_extents, 0);

	voluta_ut_trunacate_file(ut_ctx, ino, off + (loff_t)bsz);
	fm = ut_fiemap_of(ut_ctx, ino, off, bsz);
	ut_assert_eq(fm->fm_mapped_extents, 0);

	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_rmdir_at_root(ut_ctx, name);
}

static void ut_file_fiemap_simple(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_fiemap_simple_(ut_ctx, 0, 100);
	ut_file_fiemap_simple_(ut_ctx, 0, BK_SIZE);
	ut_file_fiemap_simple_(ut_ctx, BK_SIZE, MEGA);
	ut_file_fiemap_simple_(ut_ctx, MEGA, BK_SIZE);
	ut_file_fiemap_simple_(ut_ctx, GIGA - BK_SIZE, MEGA);
	ut_file_fiemap_simple_(ut_ctx, TERA - BK_SIZE, 2 * BK_SIZE);
	ut_file_fiemap_simple_(ut_ctx, FILESIZE_MAX - MEGA + 1, MEGA - 1);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static loff_t off_aligned(loff_t off, loff_t align)
{
	return (off / align) * align;
}

static loff_t off_ds(loff_t off)
{
	return off_aligned(off, VOLUTA_DS_SIZE);
}

static size_t off_len(loff_t beg, loff_t end)
{
	return (size_t)(end - beg);
}

static void ut_file_fiemap_sparse_(struct voluta_ut_ctx *ut_ctx,
				   loff_t off_base, loff_t step, size_t cnt)
{
	ino_t ino;
	ino_t dino;
	loff_t off;
	loff_t off_ext;
	size_t len;
	char b = 'b';
	const char *name = T_NAME;
	const struct fiemap *fm = NULL;
	const struct fiemap_extent *fm_ext = NULL;
	const loff_t dssz = VOLUTA_DS_SIZE;
	const loff_t off_end = off_base + (step * (loff_t)cnt);

	voluta_ut_mkdir_at_root(ut_ctx, name, &dino);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);

	off = off_base;
	for (size_t i = 0; i < cnt; ++i) {
		voluta_ut_write_read(ut_ctx, ino, &b, 1, off);
		len = off_len(off, off_end);
		fm = ut_fiemap_of(ut_ctx, ino, off, len);
		ut_assert_eq(fm->fm_mapped_extents, 1);
		off += step;
	}
	len = off_len(off_base, off_end);
	fm = ut_fiemap_of(ut_ctx, ino, off_base, len);
	ut_assert_ge(fm->fm_mapped_extents, cnt);
	off = off_base;
	for (size_t i = 0; i < cnt; ++i) {
		fm_ext = &fm->fm_extents[i];
		off_ext = (loff_t)fm_ext->fe_logical;
		ut_assert_eq(off_ds(off), off_ds(off_ext));
		ut_assert_le(fm_ext->fe_length, dssz);
		off += step;
	}

	off = off_base;
	for (size_t i = 0; i < cnt; ++i) {
		voluta_ut_fallocate_punch_hole(ut_ctx, ino, off_ds(off), dssz);
		len = off_len(off_base, off + 1);
		fm = ut_fiemap_of(ut_ctx, ino, off_base, len);
		ut_assert_eq(fm->fm_mapped_extents, 0);
		len = off_len(off_base, off_end);
		fm = ut_fiemap_of(ut_ctx, ino, off_base, len);
		ut_assert_eq(fm->fm_mapped_extents, cnt - i - 1);
		off += step;
	}
	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_rmdir_at_root(ut_ctx, name);
}

static void ut_file_fiemap_sparse(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_fiemap_sparse_(ut_ctx, 0, 100, 1);
	ut_file_fiemap_sparse_(ut_ctx, 1, 1000, 1);
	ut_file_fiemap_sparse_(ut_ctx, 0, BK_SIZE, 8);
	ut_file_fiemap_sparse_(ut_ctx, 1, MEGA, 16);
	ut_file_fiemap_sparse_(ut_ctx, GIGA, MEGA, 32);
	ut_file_fiemap_sparse_(ut_ctx, TERA - 1, GIGA + 3, 64);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_lseek_simple_(struct voluta_ut_ctx *ut_ctx, loff_t off)
{
	ino_t ino;
	ino_t dino;
	loff_t x_off;
	char x = 'x';
	struct stat st;
	const char *name = T_NAME;
	const loff_t step = 2 * VOLUTA_DS_SIZE;

	voluta_ut_mkdir_at_root(ut_ctx, name, &dino);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);
	voluta_ut_trunacate_file(ut_ctx, ino, off + step + 1);
	voluta_ut_getattr_file(ut_ctx, ino, &st);
	voluta_ut_lseek_data(ut_ctx, ino, 0, &x_off);
	ut_assert_eq(x_off, st.st_size);
	voluta_ut_write_read(ut_ctx, ino, &x, 1, off);
	voluta_ut_lseek_data(ut_ctx, ino, 0, &x_off);
	ut_assert_eq(x_off, off_ds(off));
	voluta_ut_lseek_data(ut_ctx, ino, off ? (off - 1) : 0, &x_off);
	ut_assert_eq(off_ds(x_off), off_ds(off));
	voluta_ut_lseek_data(ut_ctx, ino, off + step, &x_off);
	ut_assert_eq(x_off, st.st_size);
	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_rmdir_at_root(ut_ctx, name);
}

static void ut_file_lseek_simple(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_lseek_simple_(ut_ctx, 0);
	ut_file_lseek_simple_(ut_ctx, 1);
	ut_file_lseek_simple_(ut_ctx, MEGA + 1);
	ut_file_lseek_simple_(ut_ctx, GIGA - 3);
	ut_file_lseek_simple_(ut_ctx, TERA + 5);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_lseek_sparse_(struct voluta_ut_ctx *ut_ctx,
				  loff_t off_base, loff_t step, size_t nsteps)
{
	ino_t ino;
	ino_t dino;
	loff_t off;
	loff_t off_data;
	loff_t off_next;
	const char *name = T_NAME;

	voluta_ut_mkdir_at_root(ut_ctx, name, &dino);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);
	for (size_t i = 0; i < nsteps; ++i) {
		off = off_base + ((loff_t)i * step);
		voluta_ut_write_read1(ut_ctx, ino, off);
	}
	for (size_t i = 0; i < (nsteps - 1); ++i) {
		off = off_base + ((loff_t)i * step);
		off_next = off_base + ((loff_t)(i + 1) * step);
		voluta_ut_lseek_data(ut_ctx, ino, off, &off_data);
		ut_assert_eq(off_data, off);

		off = (off + off_next) / 2;
		voluta_ut_lseek_data(ut_ctx, ino, off, &off_data);
		ut_assert_eq(off_data, off_next);
	}
	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_rmdir_at_root(ut_ctx, name);
}

static void ut_file_lseek_sparse(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_lseek_sparse_(ut_ctx, 0, 10 * DS_SIZE, 100);
	ut_file_lseek_sparse_(ut_ctx, MEGA, GIGA, 100);
	ut_file_lseek_sparse_(ut_ctx, GIGA, MEGA, 1000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_file_fiemap_simple),
	UT_DEFTEST(ut_file_fiemap_sparse),
	UT_DEFTEST(ut_file_lseek_simple),
	UT_DEFTEST(ut_file_lseek_sparse),
};

const struct voluta_ut_tests voluta_ut_test_file_fiemap =
	UT_MKTESTS(ut_local_tests);
