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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include "unitest.h"

static void ut_rename_within_same_dir(struct voluta_ut_ctx *ut_ctx)
{
	ino_t ino;
	ino_t dino;
	struct stat st;
	const char *dname = T_NAME;
	const char *newname = NULL;
	const size_t name_max = VOLUTA_NAME_MAX;
	const char *name = ut_randstr(ut_ctx, name_max);

	voluta_ut_mkdir_at_root(ut_ctx, dname, &dino);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);
	for (size_t i = 0; i < name_max; ++i) {
		newname = ut_randstr(ut_ctx, i + 1);
		voluta_ut_rename_move(ut_ctx, dino, name, dino, newname);

		voluta_ut_getattr_exists(ut_ctx, ino, &st);
		ut_assert_eq(st.st_nlink, 1);

		name = newname;
	}
	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_rmdir_at_root(ut_ctx, dname);
}

static void ut_rename_toggle_between_dirs(struct voluta_ut_ctx *ut_ctx)
{
	struct stat st;
	ino_t ino, dino1, dino2;
	const char *dname = T_NAME;
	const char *name = ut_randstr(ut_ctx, UT_NAME_MAX);
	const char *newname = NULL;
	ino_t src_dino, dst_dino;

	voluta_ut_mkdir_at_root(ut_ctx, dname, &dino1);
	voluta_ut_mkdir_ok(ut_ctx, dino1, dname, &dino2);
	voluta_ut_create_file(ut_ctx, dino2, name, &ino);
	for (size_t i = 0; i < UT_NAME_MAX; ++i) {
		newname = ut_randstr(ut_ctx, i + 1);
		src_dino = (i & 1) ? dino1 : dino2;
		dst_dino = (i & 1) ? dino2 : dino1;
		voluta_ut_rename_move(ut_ctx, src_dino,
				      name, dst_dino, newname);
		voluta_ut_getattr_exists(ut_ctx, ino, &st);
		ut_assert_eq(st.st_nlink, 1);

		name = newname;
	}
	voluta_ut_remove_file(ut_ctx, dst_dino, name, ino);
	voluta_ut_rmdir_ok(ut_ctx, dino1, dname);
	voluta_ut_rmdir_at_root(ut_ctx, dname);
}

static void ut_rename_replace_without_data(struct voluta_ut_ctx *ut_ctx)
{
	struct stat st;
	const size_t name_max = VOLUTA_NAME_MAX;
	ino_t ino1, ino2, dino1, dino2, base_dino, root_ino = ROOT_INO;
	const char *base_dname = T_NAME;
	const char *dname1 = ut_randstr(ut_ctx, name_max);
	const char *dname2 = ut_randstr(ut_ctx, name_max);
	const char *name1 = NULL;
	const char *name2 = NULL;

	voluta_ut_mkdir_ok(ut_ctx, root_ino, base_dname, &base_dino);
	voluta_ut_mkdir_ok(ut_ctx, base_dino, dname1, &dino1);
	voluta_ut_mkdir_ok(ut_ctx, base_dino, dname2, &dino2);

	for (size_t i = 0; i < name_max; ++i) {
		name1 = ut_randstr(ut_ctx, i + 1);
		name2 = ut_randstr(ut_ctx, name_max - i);
		voluta_ut_create_only(ut_ctx, dino1, name1, &ino1);
		voluta_ut_create_only(ut_ctx, dino2, name2, &ino2);
		voluta_ut_drop_caches_fully(ut_ctx);
		voluta_ut_rename_replace(ut_ctx, dino1, name1, dino2, name2);
		voluta_ut_getattr_exists(ut_ctx, ino1, &st);
		ut_assert_eq(st.st_nlink, 1);
		voluta_ut_drop_caches_fully(ut_ctx);
		voluta_ut_unlink(ut_ctx, dino2, name2);
	}
	voluta_ut_rmdir_ok(ut_ctx, base_dino, dname1);
	voluta_ut_rmdir_ok(ut_ctx, base_dino, dname2);
	voluta_ut_rmdir_ok(ut_ctx, root_ino, base_dname);
}

static void ut_rename_replace_with_data(struct voluta_ut_ctx *ut_ctx)
{
	void *buf1, *buf2;
	loff_t off;
	size_t bsz;
	ino_t ino1, ino2, dino1, dino2, base_dino, root_ino = ROOT_INO;
	char *name1, *name2, *dname1, *dname2;
	const char *base_dname = T_NAME;
	const size_t name_max = VOLUTA_NAME_MAX;

	dname1 = ut_randstr(ut_ctx, name_max);
	dname2 = ut_randstr(ut_ctx, name_max);
	voluta_ut_mkdir_ok(ut_ctx, root_ino, base_dname, &base_dino);
	voluta_ut_mkdir_ok(ut_ctx, base_dino, dname1, &dino1);
	voluta_ut_mkdir_ok(ut_ctx, base_dino, dname2, &dino2);

	bsz = BK_SIZE;
	for (size_t i = 0; i < name_max; ++i) {
		off = (loff_t)((i * bsz) + i);
		buf1 = ut_randbuf(ut_ctx, bsz);
		buf2 = ut_randbuf(ut_ctx, bsz);
		name1 = ut_randstr(ut_ctx, i + 1);
		name2 = ut_randstr(ut_ctx, name_max - i);

		voluta_ut_create_file(ut_ctx, dino1, name1, &ino1);
		voluta_ut_create_file(ut_ctx, dino2, name2, &ino2);
		voluta_ut_write_read(ut_ctx, ino1, buf1, bsz, off);
		voluta_ut_write_read(ut_ctx, ino2, buf2, bsz, off);
		voluta_ut_release_file(ut_ctx, ino2);
		voluta_ut_rename_replace(ut_ctx, dino1, name1, dino2, name2);
		voluta_ut_read_only(ut_ctx, ino1, buf1, bsz, off);
		voluta_ut_release_file(ut_ctx, ino1);
		voluta_ut_unlink(ut_ctx, dino2, name2);
	}
	voluta_ut_rmdir_ok(ut_ctx, base_dino, dname1);
	voluta_ut_rmdir_ok(ut_ctx, base_dino, dname2);
	voluta_ut_rmdir_ok(ut_ctx, root_ino, base_dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const char *make_name(struct voluta_ut_ctx *ut_ctx,
			     const char *prefix, size_t idx)
{
	return ut_strfmt(ut_ctx, "%s%08lx", prefix, idx);
}

static void ut_rename_move_multi_(struct voluta_ut_ctx *ut_ctx, size_t cnt)
{
	const size_t name_max = VOLUTA_NAME_MAX;
	ino_t ino;
	ino_t dino1;
	ino_t dino2;
	ino_t base_dino;
	const char *base_dname = T_NAME;
	const char *dname1 = ut_randstr(ut_ctx, name_max);
	const char *dname2 = ut_randstr(ut_ctx, name_max);
	const char *name1 = NULL;
	const char *name2 = NULL;
	const ino_t root_ino = ROOT_INO;

	voluta_ut_getattr_dirsize(ut_ctx, root_ino, 0);
	voluta_ut_mkdir_ok(ut_ctx, root_ino, base_dname, &base_dino);
	voluta_ut_mkdir_ok(ut_ctx, base_dino, dname1, &dino1);
	voluta_ut_mkdir_ok(ut_ctx, base_dino, dname2, &dino2);

	voluta_ut_getattr_dirsize(ut_ctx, base_dino, 2);
	voluta_ut_lookup_dir(ut_ctx, base_dino, dname1, dino1);
	voluta_ut_lookup_dir(ut_ctx, base_dino, dname2, dino2);

	for (size_t i = 0; i < cnt; ++i) {
		name1 = make_name(ut_ctx, "s", i);
		voluta_ut_create_only(ut_ctx, dino1, name1, &ino);
		voluta_ut_getattr_dirsize(ut_ctx, dino1, (loff_t)i + 1);
		voluta_ut_lookup_file(ut_ctx, dino1, name1, ino);
	}
	for (size_t i = 0; i < cnt; ++i) {
		name1 = make_name(ut_ctx, "s", i);
		name2 = make_name(ut_ctx, "t", i);
		voluta_ut_rename_move(ut_ctx, dino1, name1, dino2, name2);
		voluta_ut_getattr_dirsize(ut_ctx, dino2, (loff_t)i + 1);
	}
	for (size_t i = 0; i < cnt; ++i) {
		name2 = make_name(ut_ctx, "t", i);
		voluta_ut_unlink_exists(ut_ctx, dino2, name2);
	}

	voluta_ut_rmdir_ok(ut_ctx, base_dino, dname1);
	voluta_ut_rmdir_ok(ut_ctx, base_dino, dname2);
	voluta_ut_getattr_dirsize(ut_ctx, base_dino, 0);
	voluta_ut_rmdir_ok(ut_ctx, root_ino, base_dname);
	voluta_ut_getattr_dirsize(ut_ctx, root_ino, 0);
}

static void ut_rename_move_multi(struct voluta_ut_ctx *ut_ctx)
{
	ut_rename_move_multi_(ut_ctx, 100);
	ut_rename_move_multi_(ut_ctx, 2000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_rename_onto_link_(struct voluta_ut_ctx *ut_ctx,
				 size_t niter, size_t cnt)
{
	ino_t ino;
	ino_t dino;
	const char *dname = T_NAME;
	const char *fname = T_NAME;
	const char *prefix = "dummy";
	const char *name = NULL;

	voluta_ut_mkdir_at_root(ut_ctx, dname, &dino);
	for (size_t i = 0; i < niter; ++i) {
		name = make_name(ut_ctx, prefix, i);
		voluta_ut_create_only(ut_ctx, dino, name, &ino);

		voluta_ut_create_only(ut_ctx, dino, fname, &ino);
		for (size_t j = 0; j < cnt; ++j) {
			name = make_name(ut_ctx, fname, j);
			voluta_ut_link_new(ut_ctx, ino, dino, name);
		}
		for (size_t j = 0; j < cnt; ++j) {
			name = make_name(ut_ctx, fname, j);
			voluta_ut_rename_replace(ut_ctx, dino,
						 fname, dino, name);
			voluta_ut_rename_move(ut_ctx, dino, name, dino, fname);
		}
		voluta_ut_unlink_exists(ut_ctx, dino, fname);
	}
	for (size_t i = 0; i < niter; ++i) {
		name = make_name(ut_ctx, prefix, i);
		voluta_ut_unlink_exists(ut_ctx, dino, name);
	}
	voluta_ut_rmdir_at_root(ut_ctx, dname);
}

static void ut_rename_onto_link(struct voluta_ut_ctx *ut_ctx)
{
	ut_rename_onto_link_(ut_ctx, 10, 10);
	ut_rename_onto_link_(ut_ctx, 3, 1000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_rename_exchange_aux_(struct voluta_ut_ctx *ut_ctx,
				    ino_t dino1, ino_t dino2, size_t cnt)
{
	ino_t ino1;
	ino_t ino2;
	const char *name1;
	const char *name2;
	const char *prefix = T_NAME;

	for (size_t i = 0; i < cnt; ++i) {
		name1 = make_name(ut_ctx, prefix, i + 1);
		name2 = make_name(ut_ctx, prefix, i + cnt + 1);
		voluta_ut_create_only(ut_ctx, dino1, name1, &ino1);
		voluta_ut_create_symlink(ut_ctx, dino2, name2, name1, &ino2);
		voluta_ut_rename_exchange(ut_ctx, dino1, name1, dino2, name2);
	}
	for (size_t i = 0; i < cnt; ++i) {
		name1 = make_name(ut_ctx, prefix, i + 1);
		name2 = make_name(ut_ctx, prefix, i + cnt + 1);
		voluta_ut_rename_exchange(ut_ctx, dino1, name1, dino2, name2);
	}
	for (size_t i = 0; i < cnt; ++i) {
		name1 = make_name(ut_ctx, prefix, i + 1);
		name2 = make_name(ut_ctx, prefix, i + cnt + 1);
		voluta_ut_unlink_exists(ut_ctx, dino1, name1);
		voluta_ut_unlink_exists(ut_ctx, dino2, name2);
	}
}

static void ut_rename_exchange_(struct voluta_ut_ctx *ut_ctx, size_t cnt)
{
	ino_t dino1;
	ino_t dino2;
	const char *dname1 = "dir1";
	const char *dname2 = "dir2";

	voluta_ut_mkdir_at_root(ut_ctx, dname1, &dino1);
	voluta_ut_mkdir_at_root(ut_ctx, dname2, &dino2);
	ut_rename_exchange_aux_(ut_ctx, dino1, dino2, cnt);
	voluta_ut_rmdir_at_root(ut_ctx, dname1);
	voluta_ut_rmdir_at_root(ut_ctx, dname2);
}

static void ut_rename_exchange(struct voluta_ut_ctx *ut_ctx)
{
	ut_rename_exchange_(ut_ctx, 10);
	ut_rename_exchange_(ut_ctx, 1000);
}

static void ut_rename_exchange_same_(struct voluta_ut_ctx *ut_ctx, size_t cnt)
{
	ino_t dino;
	const char *dname = T_NAME;

	voluta_ut_mkdir_at_root(ut_ctx, dname, &dino);
	ut_rename_exchange_aux_(ut_ctx, dino, dino, cnt);
	voluta_ut_rmdir_at_root(ut_ctx, dname);
}

static void ut_rename_exchange_same(struct voluta_ut_ctx *ut_ctx)
{
	ut_rename_exchange_same_(ut_ctx, 10);
	ut_rename_exchange_same_(ut_ctx, 1000);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_rename_within_same_dir),
	UT_DEFTEST(ut_rename_toggle_between_dirs),
	UT_DEFTEST(ut_rename_replace_without_data),
	UT_DEFTEST(ut_rename_replace_with_data),
	UT_DEFTEST(ut_rename_move_multi),
	UT_DEFTEST(ut_rename_onto_link),
	UT_DEFTEST(ut_rename_exchange),
	UT_DEFTEST(ut_rename_exchange_same),
};

const struct voluta_ut_tests voluta_ut_test_rename =
	UT_MKTESTS(ut_local_tests);
