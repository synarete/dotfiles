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


static void ut_rootd_getattr(struct ut_env *ut_env)
{
	struct stat st;

	ut_getattr_ok(ut_env, UT_ROOT_INO, &st);
	ut_assert(S_ISDIR(st.st_mode));
	ut_assert_eq(st.st_size, VOLUTA_DIR_EMPTY_SIZE);
	ut_assert_eq(st.st_nlink, 2);
}

static void ut_rootd_access(struct ut_env *ut_env)
{
	ut_access_ok(ut_env, UT_ROOT_INO, R_OK);
	ut_access_ok(ut_env, UT_ROOT_INO, X_OK);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_statfs_empty(struct ut_env *ut_env)
{
	size_t fs_size;
	size_t used_bytes;
	size_t used_files;
	const size_t vol_size = (size_t)(ut_env->volume_size);
	const size_t ag_size = VOLUTA_AG_SIZE;
	struct statvfs stv;

	ut_statfs_ok(ut_env, UT_ROOT_INO, &stv);
	ut_assert_eq(stv.f_frsize, UT_BK_SIZE);
	ut_assert_gt(stv.f_blocks, 0);
	ut_assert_gt(stv.f_blocks, stv.f_bfree);
	ut_assert_gt(stv.f_files, stv.f_ffree);

	fs_size = stv.f_frsize * stv.f_blocks;
	ut_assert_eq(fs_size, vol_size);

	used_bytes = (stv.f_blocks - stv.f_bfree) * stv.f_frsize;
	ut_assert_gt(used_bytes, 2 * ag_size);
	ut_assert_lt(used_bytes, vol_size);

	used_files = stv.f_files - stv.f_ffree;
	ut_assert_eq(used_files, 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_statfs_files_(struct ut_env *ut_env, size_t cnt)
{
	ino_t ino;
	ino_t dino;
	fsfilcnt_t ffree;
	const char *dname = UT_NAME;
	const char *fname = NULL;
	struct statvfs stv;

	ut_mkdir_at_root(ut_env, dname, &dino);
	ut_statfs_ok(ut_env, dino, &stv);
	ffree = stv.f_ffree;
	ut_assert_gt(ffree, cnt);
	for (size_t i = 0; i < cnt; ++i) {
		fname = ut_make_name(ut_env, dname, i);
		ut_create_only(ut_env, dino, fname, &ino);
		ut_statfs_ok(ut_env, dino, &stv);
		ut_assert_eq(ffree, stv.f_ffree + 1);
		ffree = stv.f_ffree;
	}
	ut_statfs_ok(ut_env, dino, &stv);
	ffree = stv.f_ffree;
	ut_assert_gt(ffree, 0);
	for (size_t i = 0; i < cnt; ++i) {
		fname = ut_make_name(ut_env, dname, i);
		ut_unlink_file(ut_env, dino, fname);
		ut_statfs_ok(ut_env, dino, &stv);
		ut_assert_eq(ffree + 1, stv.f_ffree);
		ffree = stv.f_ffree;
	}
	ut_rmdir_at_root(ut_env, dname);
}

static void ut_statfs_files(struct ut_env *ut_env)
{
	ut_statfs_files_(ut_env, 100);
	ut_statfs_files_(ut_env, 1000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_rootd_getattr),
	UT_DEFTEST(ut_rootd_access),
	UT_DEFTEST(ut_statfs_empty),
	UT_DEFTEST(ut_statfs_files),
};

const struct ut_tests ut_test_super = UT_MKTESTS(ut_local_tests);

