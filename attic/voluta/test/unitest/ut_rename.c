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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include "unitest.h"

static void ut_rename_within_same_dir(struct ut_env *ut_env)
{
	ino_t ino;
	ino_t dino;
	struct stat st;
	const char *dname = UT_NAME;
	const char *newname = NULL;
	const size_t name_max = UT_NAME_MAX;
	const char *name = ut_randstr(ut_env, name_max);

	ut_mkdir_at_root(ut_env, dname, &dino);
	ut_create_file(ut_env, dino, name, &ino);
	for (size_t i = 0; i < name_max; ++i) {
		newname = ut_randstr(ut_env, i + 1);
		ut_rename_move(ut_env, dino, name, dino, newname);

		ut_getattr_ok(ut_env, ino, &st);
		ut_assert_eq(st.st_nlink, 1);

		name = newname;
	}
	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_at_root(ut_env, dname);
}

static void ut_rename_toggle_between_dirs(struct ut_env *ut_env)
{
	struct stat st;
	ino_t ino, dino1, dino2;
	const char *dname = UT_NAME;
	const char *name = ut_randstr(ut_env, UT_NAME_MAX);
	const char *newname = NULL;
	ino_t src_dino, dst_dino;

	ut_mkdir_at_root(ut_env, dname, &dino1);
	ut_mkdir_oki(ut_env, dino1, dname, &dino2);
	ut_create_file(ut_env, dino2, name, &ino);
	for (size_t i = 0; i < UT_NAME_MAX; ++i) {
		newname = ut_randstr(ut_env, i + 1);
		src_dino = (i & 1) ? dino1 : dino2;
		dst_dino = (i & 1) ? dino2 : dino1;
		ut_rename_move(ut_env, src_dino,
			       name, dst_dino, newname);
		ut_getattr_ok(ut_env, ino, &st);
		ut_assert_eq(st.st_nlink, 1);

		name = newname;
	}
	ut_remove_file(ut_env, dst_dino, name, ino);
	ut_rmdir_ok(ut_env, dino1, dname);
	ut_rmdir_at_root(ut_env, dname);
}

static void ut_rename_replace_without_data(struct ut_env *ut_env)
{
	ino_t ino1;
	ino_t ino2;
	ino_t dino1;
	ino_t dino2;
	ino_t base_dino;
	struct stat st;
	const size_t name_max = UT_NAME_MAX;
	const char *base_dname = UT_NAME;
	const char *dname1 = ut_randstr(ut_env, name_max);
	const char *dname2 = ut_randstr(ut_env, name_max);
	const char *name1 = NULL;
	const char *name2 = NULL;

	ut_mkdir_at_root(ut_env, base_dname, &base_dino);
	ut_mkdir_oki(ut_env, base_dino, dname1, &dino1);
	ut_mkdir_oki(ut_env, base_dino, dname2, &dino2);
	for (size_t i = 0; i < name_max; ++i) {
		name1 = ut_randstr(ut_env, i + 1);
		name2 = ut_randstr(ut_env, name_max - i);
		ut_create_only(ut_env, dino1, name1, &ino1);
		ut_create_only(ut_env, dino2, name2, &ino2);
		ut_drop_caches_fully(ut_env);
		ut_rename_replace(ut_env, dino1, name1, dino2, name2);
		ut_getattr_ok(ut_env, ino1, &st);
		ut_assert_eq(st.st_nlink, 1);
		ut_drop_caches_fully(ut_env);
		ut_unlink_ok(ut_env, dino2, name2);
	}
	ut_rmdir_ok(ut_env, base_dino, dname1);
	ut_rmdir_ok(ut_env, base_dino, dname2);
	ut_rmdir_at_root(ut_env, base_dname);
}

static void ut_rename_replace_with_data(struct ut_env *ut_env)
{
	void *buf1, *buf2;
	loff_t off;
	size_t bsz;
	ino_t ino1, ino2, dino1, dino2, base_dino, root_ino = UT_ROOT_INO;
	char *name1, *name2, *dname1, *dname2;
	const char *base_dname = UT_NAME;
	const size_t name_max = UT_NAME_MAX;

	dname1 = ut_randstr(ut_env, name_max);
	dname2 = ut_randstr(ut_env, name_max);
	ut_mkdir_oki(ut_env, root_ino, base_dname, &base_dino);
	ut_mkdir_oki(ut_env, base_dino, dname1, &dino1);
	ut_mkdir_oki(ut_env, base_dino, dname2, &dino2);

	bsz = UT_BK_SIZE;
	for (size_t i = 0; i < name_max; ++i) {
		off = (loff_t)((i * bsz) + i);
		buf1 = ut_randbuf(ut_env, bsz);
		buf2 = ut_randbuf(ut_env, bsz);
		name1 = ut_randstr(ut_env, i + 1);
		name2 = ut_randstr(ut_env, name_max - i);

		ut_create_file(ut_env, dino1, name1, &ino1);
		ut_create_file(ut_env, dino2, name2, &ino2);
		ut_write_read(ut_env, ino1, buf1, bsz, off);
		ut_write_read(ut_env, ino2, buf2, bsz, off);
		ut_release_file(ut_env, ino2);
		ut_rename_replace(ut_env, dino1, name1, dino2, name2);
		ut_read_ok(ut_env, ino1, buf1, bsz, off);
		ut_release_file(ut_env, ino1);
		ut_unlink_ok(ut_env, dino2, name2);
	}
	ut_rmdir_ok(ut_env, base_dino, dname1);
	ut_rmdir_ok(ut_env, base_dino, dname2);
	ut_rmdir_ok(ut_env, root_ino, base_dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_rename_move_multi_(struct ut_env *ut_env, size_t cnt)
{
	const size_t name_max = UT_NAME_MAX;
	ino_t ino;
	ino_t dino1;
	ino_t dino2;
	ino_t base_dino;
	const char *base_dname = UT_NAME;
	const char *dname1 = ut_randstr(ut_env, name_max);
	const char *dname2 = ut_randstr(ut_env, name_max);
	const char *name1 = NULL;
	const char *name2 = NULL;
	const ino_t root_ino = UT_ROOT_INO;

	ut_getattr_dirsize(ut_env, root_ino, 0);
	ut_mkdir_oki(ut_env, root_ino, base_dname, &base_dino);
	ut_mkdir_oki(ut_env, base_dino, dname1, &dino1);
	ut_mkdir_oki(ut_env, base_dino, dname2, &dino2);

	ut_getattr_dirsize(ut_env, base_dino, 2);
	ut_lookup_dir(ut_env, base_dino, dname1, dino1);
	ut_lookup_dir(ut_env, base_dino, dname2, dino2);

	for (size_t i = 0; i < cnt; ++i) {
		name1 = ut_make_name(ut_env, "s", i);
		ut_create_only(ut_env, dino1, name1, &ino);
		ut_getattr_dirsize(ut_env, dino1, (loff_t)i + 1);
		ut_lookup_file(ut_env, dino1, name1, ino);
	}
	for (size_t i = 0; i < cnt; ++i) {
		name1 = ut_make_name(ut_env, "s", i);
		name2 = ut_make_name(ut_env, "t", i);
		ut_rename_move(ut_env, dino1, name1, dino2, name2);
		ut_getattr_dirsize(ut_env, dino2, (loff_t)i + 1);
	}
	for (size_t i = 0; i < cnt; ++i) {
		name2 = ut_make_name(ut_env, "t", i);
		ut_unlink_ok(ut_env, dino2, name2);
	}

	ut_rmdir_ok(ut_env, base_dino, dname1);
	ut_rmdir_ok(ut_env, base_dino, dname2);
	ut_getattr_dirsize(ut_env, base_dino, 0);
	ut_rmdir_ok(ut_env, root_ino, base_dname);
	ut_getattr_dirsize(ut_env, root_ino, 0);
}

static void ut_rename_move_multi(struct ut_env *ut_env)
{
	ut_rename_move_multi_(ut_env, 100);
	ut_rename_move_multi_(ut_env, 2000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_rename_onto_link_(struct ut_env *ut_env,
				 size_t niter, size_t cnt)
{
	ino_t ino;
	ino_t dino;
	struct stat st;
	const char *tname = UT_NAME;
	const char *prefix = "dummy";
	const char *name = NULL;

	ut_mkdir_at_root(ut_env, tname, &dino);
	for (size_t i = 0; i < niter; ++i) {
		name = ut_make_name(ut_env, prefix, i);
		ut_create_only(ut_env, dino, name, &ino);

		ut_create_only(ut_env, dino, tname, &ino);
		for (size_t j = 0; j < cnt; ++j) {
			name = ut_make_name(ut_env, tname, j);
			ut_link_ok(ut_env, ino, dino, name, &st);
		}
		for (size_t j = 0; j < cnt; ++j) {
			name = ut_make_name(ut_env, tname, j);
			ut_rename_replace(ut_env, dino,
					  tname, dino, name);
			ut_rename_move(ut_env, dino, name, dino, tname);
		}
		ut_unlink_ok(ut_env, dino, tname);
	}
	for (size_t i = 0; i < niter; ++i) {
		name = ut_make_name(ut_env, prefix, i);
		ut_unlink_ok(ut_env, dino, name);
	}
	ut_rmdir_at_root(ut_env, tname);
}

static void ut_rename_onto_link(struct ut_env *ut_env)
{
	ut_rename_onto_link_(ut_env, 10, 10);
	ut_rename_onto_link_(ut_env, 3, 1000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_rename_exchange_aux_(struct ut_env *ut_env,
				    ino_t dino1, ino_t dino2, size_t cnt)
{
	ino_t ino1;
	ino_t ino2;
	const char *name1;
	const char *name2;
	const char *prefix = UT_NAME;

	for (size_t i = 0; i < cnt; ++i) {
		name1 = ut_make_name(ut_env, prefix, i + 1);
		name2 = ut_make_name(ut_env, prefix, i + cnt + 1);
		ut_create_only(ut_env, dino1, name1, &ino1);
		ut_symlink_ok(ut_env, dino2, name2, name1, &ino2);
		ut_rename_exchange(ut_env, dino1, name1, dino2, name2);
	}
	for (size_t i = 0; i < cnt; ++i) {
		name1 = ut_make_name(ut_env, prefix, i + 1);
		name2 = ut_make_name(ut_env, prefix, i + cnt + 1);
		ut_rename_exchange(ut_env, dino1, name1, dino2, name2);
	}
	for (size_t i = 0; i < cnt; ++i) {
		name1 = ut_make_name(ut_env, prefix, i + 1);
		name2 = ut_make_name(ut_env, prefix, i + cnt + 1);
		ut_unlink_ok(ut_env, dino1, name1);
		ut_unlink_ok(ut_env, dino2, name2);
	}
}

static void ut_rename_exchange_(struct ut_env *ut_env, size_t cnt)
{
	ino_t dino1;
	ino_t dino2;
	const char *dname1 = "dir1";
	const char *dname2 = "dir2";

	ut_mkdir_at_root(ut_env, dname1, &dino1);
	ut_mkdir_at_root(ut_env, dname2, &dino2);
	ut_rename_exchange_aux_(ut_env, dino1, dino2, cnt);
	ut_rmdir_at_root(ut_env, dname1);
	ut_rmdir_at_root(ut_env, dname2);
}

static void ut_rename_exchange_simple(struct ut_env *ut_env)
{
	ut_rename_exchange_(ut_env, 10);
	ut_rename_exchange_(ut_env, 1000);
}

static void ut_rename_exchange_same_(struct ut_env *ut_env, size_t cnt)
{
	ino_t dino;
	const char *dname = UT_NAME;

	ut_mkdir_at_root(ut_env, dname, &dino);
	ut_rename_exchange_aux_(ut_env, dino, dino, cnt);
	ut_rmdir_at_root(ut_env, dname);
}

static void ut_rename_exchange_same(struct ut_env *ut_env)
{
	ut_rename_exchange_same_(ut_env, 10);
	ut_rename_exchange_same_(ut_env, 1000);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_rename_within_same_dir),
	UT_DEFTEST(ut_rename_toggle_between_dirs),
	UT_DEFTEST(ut_rename_replace_without_data),
	UT_DEFTEST(ut_rename_replace_with_data),
	UT_DEFTEST(ut_rename_move_multi),
	UT_DEFTEST(ut_rename_onto_link),
	UT_DEFTEST(ut_rename_exchange_simple),
	UT_DEFTEST(ut_rename_exchange_same),
};

const struct ut_tests ut_test_rename = UT_MKTESTS(ut_local_tests);
