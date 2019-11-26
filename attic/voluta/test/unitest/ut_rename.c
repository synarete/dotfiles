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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#include "unitest.h"

static void ut_rename_within_same_dir(struct voluta_ut_ctx *ut_ctx)
{
	struct stat st;
	const size_t name_max = VOLUTA_NAME_MAX;
	ino_t ino, dino, root_ino = VOLUTA_INO_ROOT;
	const char *dname = UT_NAME;
	const char *name = ut_randstr(ut_ctx, name_max);
	const char *newname = NULL;

	voluta_ut_make_dir(ut_ctx, root_ino, dname, &dino);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);
	for (size_t i = 0; i < name_max; ++i) {
		newname = ut_randstr(ut_ctx, i + 1);
		voluta_ut_rename_move(ut_ctx, dino, name, dino, newname);

		voluta_ut_getattr_exists(ut_ctx, ino, &st);
		ut_assert_eq(st.st_nlink, 1);

		name = newname;
	}
	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_remove_dir(ut_ctx, root_ino, dname);
}

static void ut_rename_toggle_between_dirs(struct voluta_ut_ctx *ut_ctx)
{
	struct stat st;
	const size_t name_max = VOLUTA_NAME_MAX;
	ino_t ino, dino1, dino2, root_ino = VOLUTA_INO_ROOT;
	const char *dname = UT_NAME;
	const char *name = ut_randstr(ut_ctx, name_max);
	const char *newname = NULL;
	ino_t src_dino, dst_dino = VOLUTA_INO_NULL;

	voluta_ut_make_dir(ut_ctx, root_ino, dname, &dino1);
	voluta_ut_make_dir(ut_ctx, dino1, dname, &dino2);
	voluta_ut_create_file(ut_ctx, dino2, name, &ino);

	for (size_t i = 0; i < name_max; ++i) {
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
	voluta_ut_remove_dir(ut_ctx, dino1, dname);
	voluta_ut_remove_dir(ut_ctx, root_ino, dname);
}

static void ut_rename_replace_without_data(struct voluta_ut_ctx *ut_ctx)
{
	struct stat st;
	const size_t name_max = VOLUTA_NAME_MAX;
	ino_t ino1, ino2, dino1, dino2, base_dino, root_ino = VOLUTA_INO_ROOT;
	const char *base_dname = UT_NAME;
	const char *dname1 = ut_randstr(ut_ctx, name_max);
	const char *dname2 = ut_randstr(ut_ctx, name_max);
	const char *name1 = NULL;
	const char *name2 = NULL;

	voluta_ut_make_dir(ut_ctx, root_ino, base_dname, &base_dino);
	voluta_ut_make_dir(ut_ctx, base_dino, dname1, &dino1);
	voluta_ut_make_dir(ut_ctx, base_dino, dname2, &dino2);

	for (size_t i = 0; i < name_max; ++i) {
		name1 = ut_randstr(ut_ctx, i + 1);
		name2 = ut_randstr(ut_ctx, name_max - i);
		voluta_ut_create_only(ut_ctx, dino1, name1, &ino1);
		voluta_ut_create_only(ut_ctx, dino2, name2, &ino2);
		voluta_ut_drop_caches(ut_ctx);
		voluta_ut_rename_replace(ut_ctx, dino1, name1, dino2, name2);
		voluta_ut_getattr_exists(ut_ctx, ino1, &st);
		ut_assert_eq(st.st_nlink, 1);
		voluta_ut_drop_caches(ut_ctx);
		voluta_ut_unlink(ut_ctx, dino2, name2);
	}
	voluta_ut_remove_dir(ut_ctx, base_dino, dname1);
	voluta_ut_remove_dir(ut_ctx, base_dino, dname2);
	voluta_ut_remove_dir(ut_ctx, root_ino, base_dname);
}

static void ut_rename_replace_with_data(struct voluta_ut_ctx *ut_ctx)
{
	void *buf1, *buf2;
	loff_t off;
	size_t bsz;
	ino_t ino1, ino2, dino1, dino2, base_dino, root_ino = VOLUTA_INO_ROOT;
	char *name1, *name2, *dname1, *dname2;
	const char *base_dname = UT_NAME;
	const size_t name_max = VOLUTA_NAME_MAX;

	dname1 = ut_randstr(ut_ctx, name_max);
	dname2 = ut_randstr(ut_ctx, name_max);
	voluta_ut_make_dir(ut_ctx, root_ino, base_dname, &base_dino);
	voluta_ut_make_dir(ut_ctx, base_dino, dname1, &dino1);
	voluta_ut_make_dir(ut_ctx, base_dino, dname2, &dino2);

	bsz = VOLUTA_BK_SIZE;
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
	voluta_ut_remove_dir(ut_ctx, base_dino, dname1);
	voluta_ut_remove_dir(ut_ctx, base_dino, dname2);
	voluta_ut_remove_dir(ut_ctx, root_ino, base_dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const char *make_name(struct voluta_ut_ctx *ut_ctx,
			     const char *prefix, size_t idx)
{
	return ut_strfmt(ut_ctx, "%s%08lx", prefix, idx);
}

static void ut_rename_move_multi(struct voluta_ut_ctx *ut_ctx)
{
	size_t i, cnt = 4096;
	const size_t name_max = VOLUTA_NAME_MAX;
	ino_t ino, dino1, dino2, base_dino, root_ino = VOLUTA_INO_ROOT;
	const char *base_dname = UT_NAME;
	const char *dname1 = ut_randstr(ut_ctx, name_max);
	const char *dname2 = ut_randstr(ut_ctx, name_max);
	const char *name1 = NULL;
	const char *name2 = NULL;

	voluta_ut_getattr_dirsize(ut_ctx, root_ino, 0);
	voluta_ut_make_dir(ut_ctx, root_ino, base_dname, &base_dino);
	voluta_ut_make_dir(ut_ctx, base_dino, dname1, &dino1);
	voluta_ut_make_dir(ut_ctx, base_dino, dname2, &dino2);

	voluta_ut_getattr_dirsize(ut_ctx, base_dino, 2);
	voluta_ut_lookup_dir(ut_ctx, base_dino, dname1, dino1);
	voluta_ut_lookup_dir(ut_ctx, base_dino, dname2, dino2);

	for (i = 0; i < cnt; ++i) {
		name1 = make_name(ut_ctx, "s", i);
		voluta_ut_create_only(ut_ctx, dino1, name1, &ino);
		voluta_ut_getattr_dirsize(ut_ctx, dino1, (loff_t)i + 1);
		voluta_ut_lookup_reg(ut_ctx, dino1, name1, ino);
	}
	for (i = 0; i < cnt; ++i) {
		name1 = make_name(ut_ctx, "s", i);
		name2 = make_name(ut_ctx, "t", i);
		voluta_ut_rename_move(ut_ctx, dino1, name1, dino2, name2);
		voluta_ut_getattr_dirsize(ut_ctx, dino2, (loff_t)i + 1);
	}
	for (i = 0; i < cnt; ++i) {
		name2 = make_name(ut_ctx, "t", i);
		voluta_ut_unlink_exists(ut_ctx, dino2, name2);
	}

	voluta_ut_remove_dir(ut_ctx, base_dino, dname1);
	voluta_ut_remove_dir(ut_ctx, base_dino, dname2);
	voluta_ut_getattr_dirsize(ut_ctx, base_dino, 0);
	voluta_ut_remove_dir(ut_ctx, root_ino, base_dname);
	voluta_ut_getattr_dirsize(ut_ctx, root_ino, 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_rename_onto_link(struct voluta_ut_ctx *ut_ctx)
{
	size_t i, j, cnt = 1000, niter = 3;
	ino_t ino, dino, root_ino = VOLUTA_INO_ROOT;
	const char *dname = UT_NAME;
	const char *fname = UT_NAME;
	const char *prefix = "dummy";
	const char *name = NULL;

	voluta_ut_make_dir(ut_ctx, root_ino, dname, &dino);
	for (j = 0; j < niter; ++j) {
		name = make_name(ut_ctx, prefix, j);
		voluta_ut_create_only(ut_ctx, dino, name, &ino);

		voluta_ut_create_only(ut_ctx, dino, fname, &ino);
		for (i = 0; i < cnt; ++i) {
			name = make_name(ut_ctx, fname, i);
			voluta_ut_link_new(ut_ctx, ino, dino, name);
		}
		for (i = 0; i < cnt; ++i) {
			name = make_name(ut_ctx, fname, i);
			voluta_ut_rename_replace(ut_ctx, dino,
						 fname, dino, name);
			voluta_ut_rename_move(ut_ctx, dino, name, dino, fname);
		}
		voluta_ut_unlink_exists(ut_ctx, dino, fname);
	}
	for (j = 0; j < niter; ++j) {
		name = make_name(ut_ctx, prefix, j);
		voluta_ut_unlink_exists(ut_ctx, dino, name);
	}
	voluta_ut_remove_dir(ut_ctx, root_ino, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_rename_exchange(struct voluta_ut_ctx *ut_ctx)
{
	const size_t cnt = 100;
	ino_t ino1, ino2, dino1, dino2, root_ino = VOLUTA_INO_ROOT;
	const char *dname1 = "dir1";
	const char *dname2 = "dir2";
	const char *name1 = NULL;
	const char *name2 = NULL;
	const char *prefix = UT_NAME;

	voluta_ut_make_dir(ut_ctx, root_ino, dname1, &dino1);
	voluta_ut_make_dir(ut_ctx, root_ino, dname2, &dino2);
	for (size_t i = 0; i < cnt; ++i) {
		name1 = make_name(ut_ctx, prefix, i + 1);
		name2 = make_name(ut_ctx, prefix, i + cnt + 1);
		voluta_ut_create_only(ut_ctx, dino1, name1, &ino1);
		voluta_ut_create_only(ut_ctx, dino2, name2, &ino2);
		voluta_ut_rename_exchange(ut_ctx, dino1, name1, dino2, name2);
	}
	for (size_t j = 0; j < cnt; ++j) {
		name1 = make_name(ut_ctx, prefix, j + 1);
		name2 = make_name(ut_ctx, prefix, j + cnt + 1);
		voluta_ut_unlink_exists(ut_ctx, dino1, name1);
		voluta_ut_unlink_exists(ut_ctx, dino2, name2);
	}
	voluta_ut_remove_dir(ut_ctx, root_ino, dname1);
	voluta_ut_remove_dir(ut_ctx, root_ino, dname2);
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
};

const struct voluta_ut_tests voluta_ut_rename_tests = UT_MKTESTS(
			ut_local_tests);
