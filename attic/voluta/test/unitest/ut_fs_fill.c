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

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t calc_wr_size(const struct statvfs *stv, size_t limit)
{
	size_t wr_size = UT_BK_SIZE;
	const size_t nbytes_free = stv->f_bfree * stv->f_frsize;

	if (nbytes_free > wr_size) {
		wr_size = nbytes_free;
	}
	if (wr_size > limit) {
		wr_size = limit;
	}
	return wr_size;
}

static void ut_fs_fill_simple(struct ut_env *ut_env)
{
	void *buf;
	ino_t ino;
	ino_t dino;
	loff_t off;
	size_t len;
	size_t nwr;
	fsblkcnt_t bfree;
	struct stat st;
	struct statvfs stv;
	const char *name = UT_NAME;
	const size_t bsz = UT_UMEGA;

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_statfs_ok(ut_env, dino, &stv);
	bfree = stv.f_bfree;
	ut_assert_gt(bfree, 0);
	ut_create_file(ut_env, dino, name, &ino);

	ut_statfs_ok(ut_env, dino, &stv);
	len = calc_wr_size(&stv, bsz);
	nwr = len;
	buf = ut_randbuf(ut_env, bsz);
	while (nwr == len) {
		ut_getattr_file(ut_env, ino, &st);
		ut_statfs_ok(ut_env, dino, &stv);
		len = calc_wr_size(&stv, bsz);
		nwr = 0;
		off = st.st_size;
		ut_write_nospc(ut_env, ino, buf, len, off, &nwr);
	}
	ut_release_file(ut_env, ino);
	ut_unlink_file(ut_env, dino, name);
	ut_statfs_ok(ut_env, dino, &stv);
	ut_assert_eq(bfree, stv.f_bfree);

	ut_rmdir_at_root(ut_env, name);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_fs_fill_mixed(struct ut_env *ut_env)
{
	size_t idx = 0;
	size_t idx_end = 0;
	loff_t off;
	ino_t ino;
	ino_t dino;
	size_t len = 0;
	size_t nwr = 0;
	struct statvfs stv;
	const char *name;
	const char *dname = UT_NAME;
	size_t bsz = 2 * UT_UMEGA;
	const void *buf = ut_randbuf(ut_env, bsz);

	ut_mkdir_at_root(ut_env, dname, &dino);
	ut_statfs_ok(ut_env, dino, &stv);
	while ((nwr == len) && (stv.f_bfree > 2) && bsz) {
		name = ut_make_name(ut_env, dname, idx++);
		ut_mkdir_oki(ut_env, dino, name, &ino);
		ut_create_file(ut_env, ino, name, &ino);
		len = calc_wr_size(&stv, bsz--);
		off = (loff_t)idx;
		nwr = 0;
		ut_write_nospc(ut_env, ino, buf, len, off, &nwr);
		ut_release_file(ut_env, ino);
		ut_statfs_ok(ut_env, dino, &stv);
		idx_end = idx;
	}
	idx = 0;
	while (idx < idx_end) {
		name = ut_make_name(ut_env, dname, idx++);
		ut_lookup_ino(ut_env, dino, name, &ino);
		ut_unlink_file(ut_env, ino, name);
		ut_rmdir_ok(ut_env, dino, name);
	}
	ut_rmdir_at_root(ut_env, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_fs_fill_append_(struct ut_env *ut_env, ino_t ino, size_t bsz)
{
	size_t nwr = bsz;
	struct stat st;
	struct statvfs stv_cur;
	const void *buf = ut_randbuf(ut_env, bsz);

	ut_statfs_ok(ut_env, ino, &stv_cur);
	while ((nwr == bsz) && stv_cur.f_bfree) {
		ut_getattr_file(ut_env, ino, &st);
		nwr = 0;
		ut_write_nospc(ut_env, ino, buf, bsz, st.st_size, &nwr);
		ut_statfs_ok(ut_env, ino, &stv_cur);
	}
	ut_statfs_ok(ut_env, ino, &stv_cur);
	ut_assert_lt(stv_cur.f_bfree, bsz);
}

static void ut_fs_fill_data_(struct ut_env *ut_env, size_t bsz)
{
	ino_t ino;
	ino_t dino;
	struct statvfs stv[2];
	const char *name = UT_NAME;

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_statfs_ok(ut_env, dino, &stv[0]);
	ut_create_file(ut_env, dino, name, &ino);
	ut_fs_fill_append_(ut_env, ino, bsz);
	ut_release_file(ut_env, ino);
	ut_unlink_file(ut_env, dino, name);
	ut_statfs_ok(ut_env, dino, &stv[1]);
	ut_assert_eq_statvfs(&stv[0], &stv[1]);
	ut_rmdir_at_root(ut_env, name);
}

static void ut_fs_fill_data(struct ut_env *ut_env)
{
	ut_fs_fill_data_(ut_env, UT_UMEGA);
	ut_fs_fill_data_(ut_env, 1111111);
}

static void ut_fs_fill_reload_(struct ut_env *ut_env, size_t bsz)
{
	ino_t ino;
	ino_t dino;
	struct statvfs stv[2];
	const char *name = UT_NAME;

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_statfs_ok(ut_env, dino, &stv[0]);
	ut_create_file(ut_env, dino, name, &ino);
	ut_fs_fill_append_(ut_env, ino, bsz);
	ut_release_file(ut_env, ino);
	ut_reload_ok(ut_env, ino);
	ut_unlink_file(ut_env, dino, name);
	ut_statfs_ok(ut_env, dino, &stv[1]);
	ut_assert_eq_statvfs(&stv[0], &stv[1]);
	ut_rmdir_at_root(ut_env, name);
}

static void ut_fs_fill_reload(struct ut_env *ut_env)
{
	ut_fs_fill_reload_(ut_env, 1234567);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_fs_fill_simple),
	UT_DEFTEST(ut_fs_fill_mixed),
	UT_DEFTEST(ut_fs_fill_data),
	UT_DEFTEST(ut_fs_fill_reload),
};

const struct ut_tests ut_test_fs_fill = UT_MKTESTS(ut_local_tests);

