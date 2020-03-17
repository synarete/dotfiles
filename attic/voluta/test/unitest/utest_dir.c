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


struct ut_namesarr {
	size_t cnt;
	const char *arr[1];
};

static const char *make_name(struct voluta_ut_ctx *ut_ctx, long idx, size_t len)
{
	char name[UT_NAME_MAX + 1] = "";
	const size_t name_max = sizeof(name) - 1;

	ut_assert_lt(len, sizeof(name));
	memset(name, 'x', name_max);
	snprintf(name, name_max, "%lu", idx);
	if (len) {
		name[strlen(name)] = '_';
		name[len] = '\0';
	}
	return voluta_ut_strdup(ut_ctx, name);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct ut_namesarr *
new_namesarr(struct voluta_ut_ctx *ut_ctx, size_t cnt)
{
	size_t sz;
	struct ut_namesarr *na;

	sz = sizeof(*na) + ((cnt - 1) * sizeof(na->arr));
	na = voluta_ut_zalloc(ut_ctx, sz);
	na->cnt = cnt;

	return na;
}

static struct ut_namesarr *
make_names_(struct voluta_ut_ctx *ut_ctx, size_t cnt, size_t len)
{
	struct ut_namesarr *na;

	na = new_namesarr(ut_ctx, cnt);
	for (size_t i = 0; i < na->cnt; ++i) {
		na->arr[i] = make_name(ut_ctx, (long)i + 1, len);
	}
	return na;
}

static struct ut_namesarr *
make_names(struct voluta_ut_ctx *ut_ctx, size_t cnt)
{
	return make_names_(ut_ctx, cnt, 0);
}

static struct ut_namesarr *
make_long_names(struct voluta_ut_ctx *ut_ctx, size_t cnt)
{
	return make_names_(ut_ctx, cnt, UT_NAME_MAX);
}

static struct ut_namesarr *
make_names_any_len(struct voluta_ut_ctx *ut_ctx, size_t cnt)
{
	struct ut_namesarr *na = new_namesarr(ut_ctx, cnt);

	for (size_t i = 0; i < na->cnt; ++i) {
		size_t len = voluta_clamp(i % UT_NAME_MAX, 17, UT_NAME_MAX);

		na->arr[i] = make_name(ut_ctx, (long)i + 1, len);
	}
	return na;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_mkdir_simple(struct voluta_ut_ctx *ut_ctx)
{
	int err;
	ino_t ino;
	ino_t parent = ROOT_INO;
	const char *name = T_NAME;
	struct stat st;

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

static void ut_mkdir_subdirs_(struct voluta_ut_ctx *ut_ctx, size_t cnt)
{
	ino_t sino;
	ino_t dino;
	const ino_t root_ino = ROOT_INO;
	const char *dname = T_NAME;
	struct ut_namesarr *na = make_names(ut_ctx, cnt);

	voluta_ut_mkdir_ok(ut_ctx, root_ino, dname, &dino);
	for (size_t i = 0; i < cnt; ++i) {
		voluta_ut_mkdir_ok(ut_ctx, dino, na->arr[i], &sino);
		voluta_ut_mkdir_fail(ut_ctx, dino, na->arr[i]);
	}
	for (size_t j = 0; j < cnt; ++j) {
		voluta_ut_mkdir_fail(ut_ctx, dino, na->arr[j]);
		voluta_ut_rmdir_ok(ut_ctx, dino, na->arr[j]);
	}
	voluta_ut_rmdir_ok(ut_ctx, root_ino, dname);
}

static void ut_mkdir_subdirs(struct voluta_ut_ctx *ut_ctx)
{
	ut_mkdir_subdirs_(ut_ctx, 1);
	ut_mkdir_subdirs_(ut_ctx, 10);
	ut_mkdir_subdirs_(ut_ctx, 1000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_mkdir_reloaded(struct voluta_ut_ctx *ut_ctx)
{
	int err;
	ino_t ino, parent_ino = ROOT_INO;
	struct stat st;
	const char *name = T_NAME;

	voluta_ut_drop_caches_fully(ut_ctx);
	voluta_ut_getattr_exists(ut_ctx, parent_ino, &st);
	ut_assert(S_ISDIR(st.st_mode));
	ut_assert_eq(st.st_nlink, 2);

	voluta_ut_drop_caches_fully(ut_ctx);
	voluta_ut_mkdir_ok(ut_ctx, parent_ino, name, &ino);
	ut_assert_ne(ino, parent_ino);
	voluta_ut_getattr_exists(ut_ctx, ino, &st);

	voluta_ut_drop_caches_fully(ut_ctx);
	voluta_ut_getattr_exists(ut_ctx, ino, &st);
	ut_assert(S_ISDIR(st.st_mode));
	ut_assert_eq(st.st_nlink, 2);

	voluta_ut_drop_caches_fully(ut_ctx);
	voluta_ut_lookup_dir(ut_ctx, parent_ino, name, ino);

	voluta_ut_drop_caches_fully(ut_ctx);
	err = voluta_ut_mkdir(ut_ctx, parent_ino, name, 0700, &st);
	ut_assert_err(err, -EEXIST);

	voluta_ut_drop_caches_fully(ut_ctx);
	voluta_ut_rmdir_ok(ut_ctx, parent_ino, name);

	voluta_ut_drop_caches_fully(ut_ctx);
	voluta_ut_lookup_noent(ut_ctx, parent_ino, name);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_mkdir_multi_(struct voluta_ut_ctx *ut_ctx, size_t cnt)
{
	ino_t dino;
	ino_t child_ino;
	loff_t size = 0;
	blkcnt_t blkcnt = 0;
	struct stat st;
	struct statvfs stv;
	const char *dname = T_NAME;
	const ino_t root_ino = ROOT_INO;
	struct ut_namesarr *na = make_names(ut_ctx, cnt + 1);

	voluta_ut_mkdir_ok(ut_ctx, root_ino, dname, &dino);
	voluta_ut_statfs_ok(ut_ctx, dino, &stv);
	ut_assert_gt(stv.f_favail, cnt);

	for (size_t i = 0; i < cnt; ++i) {
		voluta_ut_mkdir_ok(ut_ctx, dino, na->arr[i], &child_ino);
		voluta_ut_lookup_noent(ut_ctx, dino, na->arr[i + 1]);
		voluta_ut_getattr_exists(ut_ctx, dino, &st);

		ut_assert_gt(st.st_size, 0);
		ut_assert_ge(st.st_size, size);
		size = st.st_size;
		ut_assert_gt(st.st_blocks, 0);
		ut_assert_ge(st.st_blocks, blkcnt);
		blkcnt = st.st_blocks;
	}
	voluta_ut_drop_caches_fully(ut_ctx);
	voluta_ut_getattr_exists(ut_ctx, dino, &st);
	ut_assert_eq(st.st_size, size);
	ut_assert_eq(st.st_blocks, blkcnt);

	for (size_t j = 0; j < cnt; ++j) {
		voluta_ut_getattr_exists(ut_ctx, dino, &st);
		ut_assert_gt(st.st_size, 0);
		ut_assert_gt(st.st_blocks, 0);
		voluta_ut_rmdir_ok(ut_ctx, dino, na->arr[j]);
	}
	voluta_ut_rmdir_ok(ut_ctx, root_ino, dname);
}

static void ut_mkdir_multi(struct voluta_ut_ctx *ut_ctx)
{
	ut_mkdir_multi_(ut_ctx, 10);
	ut_mkdir_multi_(ut_ctx, 1000);
}

static void ut_mkdir_multi_link_max(struct voluta_ut_ctx *ut_ctx)
{
	ut_mkdir_multi_(ut_ctx, VOLUTA_LINK_MAX - 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_mkdir_link_max(struct voluta_ut_ctx *ut_ctx)
{
	int err;
	ino_t child_ino;
	ino_t dino;
	struct stat st;
	struct statvfs stv;
	const char *dname = T_NAME;
	const ino_t root_ino = ROOT_INO;
	const size_t nlink_max = VOLUTA_LINK_MAX;
	struct ut_namesarr *na = make_names(ut_ctx, nlink_max);

	voluta_ut_mkdir_ok(ut_ctx, root_ino, dname, &dino);
	voluta_ut_statfs_ok(ut_ctx, dino, &stv);
	ut_assert_gt(stv.f_favail, nlink_max);

	voluta_ut_getattr_exists(ut_ctx, dino, &st);
	ut_assert_eq(st.st_nlink, 2);

	for (size_t i = 2; i < nlink_max; ++i) {
		voluta_ut_mkdir_ok(ut_ctx, dino, na->arr[i], &child_ino);
		voluta_ut_getattr_exists(ut_ctx, dino, &st);
		ut_assert_eq(st.st_nlink, i + 1);
	}
	voluta_ut_drop_caches_fully(ut_ctx);

	err = voluta_ut_mkdir(ut_ctx, dino, dname, 0700, &st);
	ut_assert_err(err, -EMLINK);

	for (size_t j = 2; j < nlink_max; ++j) {
		voluta_ut_rmdir_ok(ut_ctx, dino, na->arr[j]);
	}
	voluta_ut_rmdir_ok(ut_ctx, root_ino, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_dir_link_any_names_(struct voluta_ut_ctx *ut_ctx, size_t cnt)
{
	ino_t ino;
	ino_t dino;
	const char *dname = T_NAME;
	const ino_t root_ino = ROOT_INO;
	const long *idx = voluta_ut_randseq(ut_ctx, cnt, 0);
	struct ut_namesarr *na = make_names_any_len(ut_ctx, cnt);

	voluta_ut_mkdir_ok(ut_ctx, root_ino, dname, &dino);
	for (size_t i = 0; i < cnt; ++i) {
		voluta_ut_create_only(ut_ctx, dino, na->arr[idx[i]], &ino);
	}
	for (size_t j = 0; j < cnt; ++j) {
		voluta_ut_unlink_exists(ut_ctx, dino, na->arr[idx[j]]);
	}
	voluta_ut_rmdir_ok(ut_ctx, root_ino, dname);
}

static void ut_dir_link_any_names(struct voluta_ut_ctx *ut_ctx)
{
	ut_dir_link_any_names_(ut_ctx, 10);
	ut_dir_link_any_names_(ut_ctx, 100);
	ut_dir_link_any_names_(ut_ctx, 1000);
	ut_dir_link_any_names_(ut_ctx, 50000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_dir_link_long_names_(struct voluta_ut_ctx *ut_ctx, size_t cnt)
{
	ino_t ino;
	ino_t dino;
	long *idx;
	const char *dname = T_NAME;
	const ino_t root_ino = ROOT_INO;
	struct ut_namesarr *na = make_long_names(ut_ctx, cnt);

	voluta_ut_mkdir_ok(ut_ctx, root_ino, dname, &dino);
	idx = voluta_ut_randseq(ut_ctx, cnt, 0);
	for (size_t i = 0; i < cnt; ++i) {
		voluta_ut_create_only(ut_ctx, dino, na->arr[idx[i]], &ino);
	}
	idx = voluta_ut_randseq(ut_ctx, cnt, 0);
	for (size_t j = 0; j < cnt; ++j) {
		voluta_ut_unlink_exists(ut_ctx, dino, na->arr[idx[j]]);
	}
	voluta_ut_rmdir_ok(ut_ctx, root_ino, dname);
}

static void ut_dir_link_long_names(struct voluta_ut_ctx *ut_ctx)
{
	ut_dir_link_long_names_(ut_ctx, 10);
	ut_dir_link_long_names_(ut_ctx, 100);
	ut_dir_link_long_names_(ut_ctx, 1000);
	ut_dir_link_long_names_(ut_ctx, 50000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

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

static void ut_dir_link_unlink_mixed(struct voluta_ut_ctx *ut_ctx)
{
	int err;
	size_t i, k, nfiles = VOLUTA_NAME_MAX;
	ino_t ino, dino, root_ino = ROOT_INO;
	struct stat st;
	const char *dname = T_NAME;
	const char *fname = T_NAME;
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
	blkcnt_t blocks = 0, nfrg = BK_SIZE / 512;
	ino_t dino, ino, root_ino = ROOT_INO;
	const char *dname = T_NAME;
	const char *xname;
	struct stat st;

	voluta_ut_mkdir_ok(ut_ctx, root_ino, dname, &dino);
	voluta_ut_getattr_exists(ut_ctx, dino, &st);
	ut_assert_eq(st.st_size, VOLUTA_DIR_EMPTY_SIZE);

	for (i = 0; i < cnt; ++i) {
		xname = make_xname(ut_ctx, i);
		voluta_ut_create_only(ut_ctx, dino, xname, &ino);
		voluta_ut_getattr_exists(ut_ctx, dino, &st);
		ut_assert_ge(st.st_size, (loff_t)i + 1);
		ut_assert_ge(st.st_blocks, blocks);
		ut_assert_le(st.st_blocks, blocks + nfrg);
		blocks = st.st_blocks;
	}
	voluta_ut_drop_caches_fully(ut_ctx);

	voluta_ut_getattr_exists(ut_ctx, dino, &st);
	ut_assert_ge(st.st_size, cnt);
	ut_assert_gt(st.st_blocks, 0);
	for (i = 0; i < cnt; ++i) {
		xname = make_xname(ut_ctx, i);
		voluta_ut_remove_link(ut_ctx, dino, xname);
		voluta_ut_getattr_exists(ut_ctx, dino, &st);
		ut_assert_ge(st.st_size, (loff_t)(cnt - i) - 1);
	}

	voluta_ut_getattr_exists(ut_ctx, dino, &st);
	ut_assert_eq(st.st_size, VOLUTA_DIR_EMPTY_SIZE);
	ut_assert_eq(st.st_blocks, 0);
	voluta_ut_rmdir_ok(ut_ctx, root_ino, dname);
}

static void ut_dir_stat_simple(struct voluta_ut_ctx *ut_ctx)
{
	ut_dir_stat_(ut_ctx, 16);
	ut_dir_stat_(ut_ctx, 256);
}

static void ut_dir_stat_large(struct voluta_ut_ctx *ut_ctx)
{
	ut_dir_stat_(ut_ctx, VOLUTA_LINK_MAX); /* TODO: VOLUTA_DIROFF_MAX */
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_mkdir_simple),
	UT_DEFTEST(ut_mkdir_subdirs),
	UT_DEFTEST(ut_mkdir_reloaded),
	UT_DEFTEST(ut_mkdir_multi),
	UT_DEFTEST(ut_mkdir_multi_link_max),
	UT_DEFTEST(ut_mkdir_link_max),
	UT_DEFTEST(ut_dir_link_any_names),
	UT_DEFTEST(ut_dir_link_long_names),
	UT_DEFTEST(ut_dir_link_unlink_mixed),
	UT_DEFTEST(ut_dir_stat_simple),
	UT_DEFTEST(ut_dir_stat_large),
};

const struct voluta_ut_tests voluta_ut_utest_dir =
	UT_MKTESTS(ut_local_tests);

