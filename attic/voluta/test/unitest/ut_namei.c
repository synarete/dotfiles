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


static void ut_create_open_release(struct voluta_ut_ctx *ut_ctx)
{
	int err;
	ino_t ino;
	ino_t dino;
	ino_t parent = ROOT_INO;
	struct stat st;
	const char *dname = T_NAME;
	const char *fname = "bbb";

	err = voluta_ut_mkdir(ut_ctx, parent, dname, 0700, &st);
	ut_assert_ok(err);
	dino = st.st_ino;

	err = voluta_ut_create(ut_ctx, dino, fname, S_IFREG | 0600, &st);
	ut_assert_ok(err);
	ino = st.st_ino;

	err = voluta_ut_release(ut_ctx, ino);
	ut_assert_ok(err);

	voluta_ut_drop_caches_fully(ut_ctx);

	err = voluta_ut_lookup(ut_ctx, dino, fname, &st);
	ut_assert_ok(err);
	ut_assert(S_ISREG(st.st_mode));
	ut_assert_eq(ino, st.st_ino);

	voluta_ut_drop_caches_fully(ut_ctx);

	err = voluta_ut_open(ut_ctx, ino, O_RDONLY);
	ut_assert_ok(err);
	ut_assert_eq(ino, st.st_ino);

	err = voluta_ut_release(ut_ctx, ino);
	ut_assert_ok(err);

	err = voluta_ut_unlink(ut_ctx, dino, fname);
	ut_assert_ok(err);

	err = voluta_ut_rmdir(ut_ctx, parent, dname);
	ut_assert_ok(err);
}

static void ut_create_unlink_simple(struct voluta_ut_ctx *ut_ctx)
{
	int err;
	ino_t ino, root_ino = ROOT_INO;
	struct stat st;
	const char *name = "123";
	const mode_t mode = S_IFREG | 0600;

	err = voluta_ut_create(ut_ctx, root_ino, name, mode, &st);
	ino = st.st_ino;
	ut_assert_ok(err);
	ut_assert(S_ISREG(st.st_mode));
	ut_assert_eq(st.st_nlink, 1);
	ut_assert_ne(ino, root_ino);

	err = voluta_ut_lookup(ut_ctx, root_ino, name, &st);
	ut_assert_ok(err);
	ut_assert(S_ISREG(st.st_mode));
	ut_assert_eq(ino, st.st_ino);

	err = voluta_ut_release(ut_ctx, ino);
	ut_assert_ok(err);

	voluta_ut_drop_caches_fully(ut_ctx);

	err = voluta_ut_lookup(ut_ctx, root_ino, name, &st);
	ut_assert_ok(err);

	voluta_ut_drop_caches_fully(ut_ctx);

	err = voluta_ut_unlink(ut_ctx, root_ino, name);
	ut_assert_ok(err);

	err = voluta_ut_lookup(ut_ctx, root_ino, name, &st);
	ut_assert_err(err, -ENOENT);
}

static const char *link_name(struct voluta_ut_ctx *ut_ctx,
			     const char *prefix, unsigned long i)
{
	return ut_strfmt(ut_ctx, "%s%lu", prefix, i);
}

static void ut_link_unlink_many(struct voluta_ut_ctx *ut_ctx)
{
	ino_t ino;
	ino_t dino;
	struct stat st;
	const char *lname;
	const char *dname = T_NAME;
	const char *fname = T_NAME;
	const size_t nlinks_max = 10000;

	ut_assert_le(nlinks_max, VOLUTA_LINK_MAX);
	voluta_ut_mkdir_at_root(ut_ctx, dname, &dino);
	voluta_ut_create_file(ut_ctx, dino, fname, &ino);
	for (size_t i = 0; i < nlinks_max; ++i) {
		voluta_ut_getattr_exists(ut_ctx, ino, &st);
		ut_assert_eq(st.st_nlink, i + 1);

		lname = link_name(ut_ctx, fname, i + 1);
		voluta_ut_link_new(ut_ctx, ino, dino, lname);

		voluta_ut_getattr_exists(ut_ctx, ino, &st);
		ut_assert_eq(st.st_nlink, i + 2);
	}
	for (size_t i = 0; i < nlinks_max; i += 2) {
		lname = link_name(ut_ctx, fname, i + 1);
		voluta_ut_unlink_exists(ut_ctx, dino, lname);
	}
	for (size_t i = 1; i < nlinks_max; i += 2) {
		lname = link_name(ut_ctx, fname, i + 1);
		voluta_ut_unlink_exists(ut_ctx, dino, lname);
	}

	voluta_ut_remove_file(ut_ctx, dino, fname, ino);
	voluta_ut_rmdir_at_root(ut_ctx, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_link_max(struct voluta_ut_ctx *ut_ctx)
{
	int err;
	ino_t ino, dino, root_ino = ROOT_INO;
	struct stat st;
	struct statvfs stv;
	const char *lname;
	const char *dname = T_NAME;
	const char *fname = T_NAME;
	const size_t nlink_max = VOLUTA_LINK_MAX;

	voluta_ut_mkdir_ok(ut_ctx, root_ino, dname, &dino);
	voluta_ut_statfs_ok(ut_ctx, dino, &stv);
	ut_assert_gt(stv.f_favail, nlink_max);

	voluta_ut_create_file(ut_ctx, dino, fname, &ino);
	voluta_ut_getattr_exists(ut_ctx, ino, &st);
	ut_assert_eq(st.st_nlink, 1);

	for (size_t i = 1; i < nlink_max; ++i) {
		lname = link_name(ut_ctx, fname, i);
		voluta_ut_link_new(ut_ctx, ino, dino, lname);
	}

	lname = link_name(ut_ctx, fname, 1000 * nlink_max);
	err = voluta_ut_link(ut_ctx, ino, dino, lname, &st);
	ut_assert_err(err, -EMLINK);

	for (size_t j = 1; j < nlink_max; ++j) {
		lname = link_name(ut_ctx, fname, j);
		voluta_ut_unlink_exists(ut_ctx, dino, lname);
	}

	voluta_ut_getattr_exists(ut_ctx, ino, &st);
	ut_assert_eq(st.st_nlink, 1);
	voluta_ut_remove_file(ut_ctx, dino, fname, ino);

	voluta_ut_rmdir_ok(ut_ctx, root_ino, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const char *
make_repeated_name(struct voluta_ut_ctx *ut_ctx, char c, size_t len)
{
	size_t nlen;
	char name[VOLUTA_NAME_MAX + 1] = "";

	nlen = (len < sizeof(name)) ? len : (sizeof(name) - 1);
	memset(name, c, nlen);
	return voluta_ut_strdup(ut_ctx, name);
}

static void ut_link_similar_names(struct voluta_ut_ctx *ut_ctx)
{
	ino_t ino, dino, root_ino = ROOT_INO;
	size_t i, j, abc_len, name_max;
	const char *dname = "dir";
	const char *fname = T_NAME;
	const char *lname;
	const char *abc =
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

	voluta_ut_mkdir_ok(ut_ctx, root_ino, dname, &dino);
	voluta_ut_create_file(ut_ctx, dino, fname, &ino);

	abc_len = strlen(abc);
	name_max = VOLUTA_NAME_MAX;
	for (i = 0; i < abc_len; ++i) {
		for (j = 1; j <= name_max; ++j) {
			lname = make_repeated_name(ut_ctx, abc[i], j);
			voluta_ut_link_new(ut_ctx, ino, dino, lname);
		}
	}
	for (i = 0; i < abc_len; ++i) {
		for (j = 1; j <= name_max; ++j) {
			lname = make_repeated_name(ut_ctx, abc[i], j);
			voluta_ut_unlink_exists(ut_ctx, dino, lname);
		}
	}
	voluta_ut_remove_file(ut_ctx, dino, fname, ino);
	voluta_ut_rmdir_ok(ut_ctx, root_ino, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_link_rand_names(struct voluta_ut_ctx *ut_ctx)
{
	ino_t ino, dino, root_ino = ROOT_INO;
	size_t i, name_len;
	const char *dname = T_NAME;
	const char *fname = T_NAME;
	const size_t nlinks = 8 * 1024; /* XXX check with large */
	const size_t name_max = VOLUTA_NAME_MAX;
	char *lname, **links;

	voluta_ut_mkdir_ok(ut_ctx, root_ino, dname, &dino);
	voluta_ut_create_only(ut_ctx, dino, fname, &ino);
	links = ut_zerobuf(ut_ctx, nlinks * sizeof(*links));
	for (i = 0; i < nlinks; ++i) {
		name_len = (i % name_max) | 0xA1;
		lname = ut_randstr(ut_ctx, name_len);
		voluta_ut_link_new(ut_ctx, ino, dino, lname);
		links[i] = lname;
	}
	voluta_ut_drop_caches_fully(ut_ctx);
	for (i = 0; i < nlinks; ++i) {
		lname = links[i];
		voluta_ut_unlink_exists(ut_ctx, dino, lname);
	}
	voluta_ut_remove_link(ut_ctx, dino, fname);
	voluta_ut_rmdir_ok(ut_ctx, root_ino, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_inode_utimes(struct voluta_ut_ctx *ut_ctx)
{
	ino_t ino, dino, root_ino = ROOT_INO;
	const char *dname = T_NAME;
	const char *fname = T_NAME;
	struct timespec mtime = { 111, 2222 };
	struct timespec atime = { 33333, 444444 };

	voluta_ut_mkdir_ok(ut_ctx, root_ino, dname, &dino);
	voluta_ut_create_file(ut_ctx, dino, fname, &ino);
	voluta_ut_utimens_atime(ut_ctx, ino, &atime);
	voluta_ut_utimens_mtime(ut_ctx, ino, &mtime);
	voluta_ut_remove_file(ut_ctx, dino, fname, ino);
	voluta_ut_rmdir_ok(ut_ctx, root_ino, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_inode_special(struct voluta_ut_ctx *ut_ctx)
{
	ino_t ino, dino, root_ino = ROOT_INO;
	const char *dname = T_NAME;
	const char *fname = T_NAME;
	const mode_t rmode = S_IRUSR | S_IRGRP;

	voluta_ut_mkdir_ok(ut_ctx, root_ino, dname, &dino);
	voluta_ut_create_special(ut_ctx, dino, fname, S_IFIFO | rmode, &ino);
	voluta_ut_remove_file(ut_ctx, dino, fname, ino);
	voluta_ut_create_special(ut_ctx, dino, fname, S_IFSOCK | rmode, &ino);
	voluta_ut_remove_file(ut_ctx, dino, fname, ino);
	voluta_ut_rmdir_ok(ut_ctx, root_ino, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_assert_eq_ts(const struct timespec *st_ts,
			    const struct statx_timestamp *stx_ts)

{
	ut_assert_eq(stx_ts->tv_sec, st_ts->tv_sec);
	ut_assert_eq(stx_ts->tv_nsec, st_ts->tv_nsec);
}

static void ut_assert_eq_stat(const struct stat *st, const struct statx *stx)
{
	ut_assert_eq(st->st_nlink, stx->stx_nlink);
	ut_assert_eq(st->st_uid, stx->stx_uid);
	ut_assert_eq(st->st_gid, stx->stx_gid);
	ut_assert_eq(st->st_mode, stx->stx_mode);
	ut_assert_eq(st->st_ino, stx->stx_ino);
	ut_assert_eq(st->st_size, stx->stx_size);
	ut_assert_eq(st->st_blocks, stx->stx_blocks);
	ut_assert_eq(st->st_blksize, stx->stx_blksize);
	ut_assert_eq_ts(&st->st_mtim, &stx->stx_mtime);
	ut_assert_eq_ts(&st->st_ctim, &stx->stx_ctime);
}

static void ut_inode_statx(struct voluta_ut_ctx *ut_ctx)
{
	ino_t ino, dino, root_ino = ROOT_INO;
	const char *dname = T_NAME;
	const char *fname = T_NAME;
	struct stat st;
	struct statx stx;

	voluta_ut_mkdir_ok(ut_ctx, root_ino, dname, &dino);
	voluta_ut_create_file(ut_ctx, dino, fname, &ino);
	voluta_ut_getattr_exists(ut_ctx, ino, &st);
	voluta_ut_statx_exists(ut_ctx, ino, &stx);
	ut_assert_eq_stat(&st, &stx);
	voluta_ut_remove_file(ut_ctx, dino, fname, ino);
	voluta_ut_rmdir_ok(ut_ctx, root_ino, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_create_open_release),
	UT_DEFTEST(ut_create_unlink_simple),
	UT_DEFTEST(ut_link_unlink_many),
	UT_DEFTEST(ut_link_similar_names),
	UT_DEFTEST(ut_link_rand_names),
	UT_DEFTEST(ut_link_max),
	UT_DEFTEST(ut_inode_utimes),
	UT_DEFTEST(ut_inode_special),
	UT_DEFTEST(ut_inode_statx),
};

const struct voluta_ut_tests voluta_ut_test_namei =
	UT_MKTESTS(ut_local_tests);

