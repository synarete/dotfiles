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

static void ut_mkdir_simple(struct voluta_ut_ctx *ut_ctx)
{
	int err;
	ino_t ino, parent = VOLUTA_INO_ROOT;
	struct stat st;
	const char *name = "ABC";

	err = voluta_ut_mkdir(ut_ctx, parent, name, 0700, &st);
	ino = st.st_ino;
	ut_assert_ok(err);
	ut_assert(S_ISDIR(st.st_mode));
	ut_assert_eq(st.st_nlink, 2);
	ut_assert_ne(ino, parent);

	err = voluta_ut_lookup(ut_ctx, parent, name, &st);
	ut_assert_ok(err);
	ut_assert(S_ISDIR(st.st_mode));
	ut_assert_eq(ino, st.st_ino);

	err = voluta_ut_lookup(ut_ctx, parent, "abc", &st);
	ut_assert_err(err, -ENOENT);

	err = voluta_ut_mkdir(ut_ctx, parent, name, 0700, &st);
	ut_assert_err(err, -EEXIST);

	err = voluta_ut_rmdir(ut_ctx, parent, name);
	ut_assert_ok(err);

	err = voluta_ut_lookup(ut_ctx, parent, name, &st);
	ut_assert_err(err, -ENOENT);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_mkdir_reloaded(struct voluta_ut_ctx *ut_ctx)
{
	int err;
	ino_t ino, parent_ino = VOLUTA_INO_ROOT;
	struct stat st;
	const char *name = UT_NAME;

	voluta_ut_drop_caches(ut_ctx);
	voluta_ut_getattr_exists(ut_ctx, parent_ino, &st);
	ut_assert(S_ISDIR(st.st_mode));
	ut_assert_eq(st.st_nlink, 2);

	voluta_ut_drop_caches(ut_ctx);
	voluta_ut_make_dir(ut_ctx, parent_ino, name, &ino);
	ut_assert_ne(ino, parent_ino);
	voluta_ut_getattr_exists(ut_ctx, ino, &st);

	voluta_ut_drop_caches(ut_ctx);
	voluta_ut_getattr_exists(ut_ctx, ino, &st);
	ut_assert(S_ISDIR(st.st_mode));
	ut_assert_eq(st.st_nlink, 2);

	voluta_ut_drop_caches(ut_ctx);
	voluta_ut_lookup_dir(ut_ctx, parent_ino, name, ino);

	voluta_ut_drop_caches(ut_ctx);
	err = voluta_ut_mkdir(ut_ctx, parent_ino, name, 0700, &st);
	ut_assert_err(err, -EEXIST);

	voluta_ut_drop_caches(ut_ctx);
	voluta_ut_remove_dir(ut_ctx, parent_ino, name);

	voluta_ut_drop_caches(ut_ctx);
	voluta_ut_lookup_noent(ut_ctx, parent_ino, name);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const char *make_iname(struct voluta_ut_ctx *ut_ctx, size_t idx)
{
	return ut_strfmt(ut_ctx, "%lx", idx);
}

static void ut_mkdir_multi_(struct voluta_ut_ctx *ut_ctx, size_t subd_max)
{
	ino_t child_ino, dino, root_ino = VOLUTA_INO_ROOT;
	struct stat st;
	struct statvfs stv;
	const char *iname;
	const char *dname = UT_NAME;

	voluta_ut_make_dir(ut_ctx, root_ino, dname, &dino);
	voluta_ut_statfs_ok(ut_ctx, dino, &stv);
	ut_assert_gt(stv.f_favail, subd_max);

	for (size_t i = 0; i < subd_max; ++i) {
		iname = make_iname(ut_ctx, i + 1);
		voluta_ut_make_dir(ut_ctx, dino, iname, &child_ino);
		iname = make_iname(ut_ctx, i + 2);
		voluta_ut_lookup_noent(ut_ctx, dino, iname);
	}
	voluta_ut_drop_caches(ut_ctx);

	voluta_ut_getattr_exists(ut_ctx, dino, &st);
	ut_assert_gt(st.st_size, 0);
	if (st.st_size > VOLUTA_DIREMPTY_SIZE) {
		ut_assert_gt(st.st_blocks, 0);
	}

	for (size_t j = 0; j < subd_max; ++j) {
		iname = make_iname(ut_ctx, j + 1);
		voluta_ut_remove_dir(ut_ctx, dino, iname);
	}
	voluta_ut_remove_dir(ut_ctx, root_ino, dname);
}

static void ut_mkdir_multi32(struct voluta_ut_ctx *ut_ctx)
{
	ut_mkdir_multi_(ut_ctx, 32);
}

static void ut_mkdir_multi4k(struct voluta_ut_ctx *ut_ctx)
{
	ut_mkdir_multi_(ut_ctx, 4096);
}

static void ut_mkdir_multi16k(struct voluta_ut_ctx *ut_ctx)
{
	ut_mkdir_multi_(ut_ctx, 16384);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_mkdir_link_max(struct voluta_ut_ctx *ut_ctx)
{
	int err;
	ino_t child_ino, dino, root_ino = VOLUTA_INO_ROOT;
	struct stat st;
	struct statvfs stv;
	const char *iname;
	const char *dname = UT_NAME;
	const size_t nlink_max = VOLUTA_LINK_MAX;

	voluta_ut_make_dir(ut_ctx, root_ino, dname, &dino);
	voluta_ut_statfs_ok(ut_ctx, dino, &stv);
	ut_assert_gt(stv.f_favail, nlink_max);

	voluta_ut_getattr_exists(ut_ctx, dino, &st);
	ut_assert_eq(st.st_nlink, 2);

	for (size_t i = 2; i < nlink_max; ++i) {
		iname = make_iname(ut_ctx, i);
		voluta_ut_make_dir(ut_ctx, dino, iname, &child_ino);
		voluta_ut_getattr_exists(ut_ctx, dino, &st);
		ut_assert_eq(st.st_nlink, i + 1);
	}
	voluta_ut_drop_caches(ut_ctx);

	iname = make_iname(ut_ctx, 1000 * nlink_max);
	err = voluta_ut_mkdir(ut_ctx, dino, iname, 0700, &st);
	ut_assert_err(err, -EMLINK);

	for (size_t j = 2; j < nlink_max; ++j) {
		iname = make_iname(ut_ctx, j);
		voluta_ut_remove_dir(ut_ctx, dino, iname);
	}
	voluta_ut_remove_dir(ut_ctx, root_ino, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const char *make_long_name(struct voluta_ut_ctx *ut_ctx, size_t idx)
{
	int n;
	char prefix[64] = "";
	char name[VOLUTA_NAME_MAX + 1] = "";
	const size_t name_max = sizeof(name) - 1;

	n = snprintf(prefix, sizeof(prefix) - 1, "%lu", idx);
	memset(name, '.', name_max - 1);
	memcpy(name, prefix, (size_t)n);
	name[name_max - 1] = '\0';

	voluta_ut_assert(ut_ctx != NULL);
	return voluta_ut_strdup(ut_ctx, name);
}

static void ut_link_long_names(struct voluta_ut_ctx *ut_ctx)
{
	int err;
	size_t i, nfiles = 1024;
	mode_t fmode = S_IFREG | S_IRWXU;
	ino_t ino, dino, root_ino = VOLUTA_INO_ROOT;
	struct stat st;
	const char *dname = UT_NAME;
	const char *fname = NULL;

	err = voluta_ut_mkdir(ut_ctx, root_ino, dname, 0700, &st);
	ut_assert_ok(err);
	dino = st.st_ino;

	for (i = 0; i < nfiles; ++i) {
		fname = make_long_name(ut_ctx, i + 1);
		err = voluta_ut_create(ut_ctx, dino, fname, fmode, &st);
		ut_assert_ok(err);
		ino = st.st_ino;

		err = voluta_ut_lookup(ut_ctx, dino, fname, &st);
		ut_assert_ok(err);
		ut_assert_eq(ino, st.st_ino);

		err = voluta_ut_release(ut_ctx, ino);
		ut_assert_ok(err);
	}

	for (i = 0; i < nfiles; ++i) {
		fname = make_long_name(ut_ctx, i + 1);
		err = voluta_ut_lookup(ut_ctx, dino, fname, &st);
		ut_assert_ok(err);
		ino = st.st_ino;

		err = voluta_ut_getattr(ut_ctx, ino, &st);
		ut_assert_ok(err);

		err = voluta_ut_unlink(ut_ctx, dino, fname);
		ut_assert_ok(err);

		err = voluta_ut_lookup(ut_ctx, dino, fname, &st);
		ut_assert_err(err, -ENOENT);

		err = voluta_ut_getattr(ut_ctx, ino, &st);
		ut_assert_err(err, -ENOENT);
	}

	err = voluta_ut_rmdir(ut_ctx, root_ino, dname);
	ut_assert_ok(err);
}

static char *make_kname(struct voluta_ut_ctx *ut_ctx, size_t k, int tag)
{
	char name[VOLUTA_NAME_MAX + 1] = "";
	const size_t name_max = sizeof(name) - 1;
	const char ch = tag ? 'A' : 'B';

	ut_assert_le(k, name_max);
	memset(name, ch, k);
	name[k] = '\0';

	return voluta_ut_strdup(ut_ctx, name);
}

static void ut_link_unlink_mixed(struct voluta_ut_ctx *ut_ctx)
{
	int err;
	size_t i, k, nfiles = VOLUTA_NAME_MAX;
	ino_t ino, dino, root_ino = VOLUTA_INO_ROOT;
	struct stat st;
	const char *dname = UT_NAME;
	const char *fname = UT_NAME;
	const char *lname = NULL;

	err = voluta_ut_mkdir(ut_ctx, root_ino, dname, 0700, &st);
	ut_assert_ok(err);
	dino = st.st_ino;

	err = voluta_ut_create(ut_ctx, dino, fname, S_IFREG | S_IRWXU, &st);
	ut_assert_ok(err);
	ino = st.st_ino;

	err = voluta_ut_release(ut_ctx, ino);
	ut_assert_ok(err);

	for (i = 0; i < nfiles; i += 2) {
		k = i + 1;
		lname = make_kname(ut_ctx, k, 0);
		err = voluta_ut_link(ut_ctx, ino, dino, lname, &st);
		ut_assert_ok(err);
		ut_assert_eq(ino, st.st_ino);

		lname = make_kname(ut_ctx, k, 1);
		err = voluta_ut_link(ut_ctx, ino, dino, lname, &st);
		ut_assert_ok(err);
		ut_assert_eq(ino, st.st_ino);

		lname = make_kname(ut_ctx, k, 0);
		err = voluta_ut_unlink(ut_ctx, dino, lname);
		ut_assert_ok(err);
	}

	for (i = 1; i < nfiles; i += 2) {
		k = i + 1;
		lname = make_kname(ut_ctx, k, 0);
		err = voluta_ut_link(ut_ctx, ino, dino, lname, &st);
		ut_assert_ok(err);
		ut_assert_eq(ino, st.st_ino);

		lname = make_kname(ut_ctx, k, 1);
		err = voluta_ut_link(ut_ctx, ino, dino, lname, &st);
		ut_assert_ok(err);
		ut_assert_eq(ino, st.st_ino);

		lname = make_kname(ut_ctx, k, 0);
		err = voluta_ut_unlink(ut_ctx, dino, lname);
		ut_assert_ok(err);
	}

	for (i = 0; i < nfiles; ++i) {
		k = i + 1;
		lname = make_kname(ut_ctx, k, 0);
		err = voluta_ut_unlink(ut_ctx, dino, lname);
		ut_assert_err(err, -ENOENT);

		lname = make_kname(ut_ctx, k, 1);
		err = voluta_ut_unlink(ut_ctx, dino, lname);
		ut_assert_ok(err);
	}

	err = voluta_ut_unlink(ut_ctx, dino, fname);
	ut_assert_ok(err);

	err = voluta_ut_rmdir(ut_ctx, root_ino, dname);
	ut_assert_ok(err);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const char *make_xname(struct voluta_ut_ctx *ut_ctx, size_t x)
{
	return ut_strfmt(ut_ctx, "%lx", x);
}

static void ut_dir_stat_(struct voluta_ut_ctx *ut_ctx, size_t cnt)
{
	size_t i;
	blkcnt_t blocks = 0, nfrg = VOLUTA_BK_SIZE / 512;
	ino_t dino, ino, root_ino = VOLUTA_INO_ROOT;
	const char *dname = UT_NAME;
	const char *xname;
	struct stat st;

	voluta_ut_make_dir(ut_ctx, root_ino, dname, &dino);
	voluta_ut_getattr_exists(ut_ctx, dino, &st);
	voluta_ut_assert_eq(st.st_size, VOLUTA_DIREMPTY_SIZE);

	for (i = 0; i < cnt; ++i) {
		xname = make_xname(ut_ctx, i);
		voluta_ut_create_only(ut_ctx, dino, xname, &ino);
		voluta_ut_getattr_exists(ut_ctx, dino, &st);
		voluta_ut_assert_ge(st.st_size, (loff_t)i + 1);
		voluta_ut_assert_ge(st.st_blocks, blocks);
		voluta_ut_assert_le(st.st_blocks, blocks + nfrg);
		blocks = st.st_blocks;
	}
	voluta_ut_drop_caches(ut_ctx);

	voluta_ut_getattr_exists(ut_ctx, dino, &st);
	voluta_ut_assert_ge(st.st_size, cnt);
	if (cnt > VOLUTA_DIRHEAD_NENTS) {
		voluta_ut_assert_gt(st.st_blocks, 0);
	}
	for (i = 0; i < cnt; ++i) {
		xname = make_xname(ut_ctx, i);
		voluta_ut_remove_link(ut_ctx, dino, xname);
		voluta_ut_getattr_exists(ut_ctx, dino, &st);
		voluta_ut_assert_ge(st.st_size, (loff_t)(cnt - i) - 1);
	}

	voluta_ut_getattr_exists(ut_ctx, dino, &st);
	voluta_ut_assert_eq(st.st_size, VOLUTA_DIREMPTY_SIZE);
	voluta_ut_assert_eq(st.st_blocks, 0);
	voluta_ut_remove_dir(ut_ctx, root_ino, dname);
}

static void ut_dir_stat_simple(struct voluta_ut_ctx *ut_ctx)
{
	ut_dir_stat_(ut_ctx, VOLUTA_DIRHEAD_NENTS);
}

static void ut_dir_stat_large(struct voluta_ut_ctx *ut_ctx)
{
	ut_dir_stat_(ut_ctx, VOLUTA_LINK_MAX); /* TODO: VOLUTA_DIROFF_MAX */
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_mkdir_simple),
	UT_DEFTEST(ut_mkdir_reloaded),
	UT_DEFTEST(ut_mkdir_multi32),
	UT_DEFTEST(ut_mkdir_multi4k),
	UT_DEFTEST(ut_mkdir_multi16k),
	UT_DEFTEST(ut_mkdir_link_max),
	UT_DEFTEST(ut_link_long_names),
	UT_DEFTEST(ut_link_unlink_mixed),
	UT_DEFTEST(ut_dir_stat_simple),
	UT_DEFTEST(ut_dir_stat_large),
};

const struct voluta_ut_tests voluta_ut_dir_tests = UT_MKTESTS(ut_local_tests);

