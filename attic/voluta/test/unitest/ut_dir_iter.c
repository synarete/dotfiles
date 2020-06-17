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

static void ut_dir_open_release(struct voluta_ut_ctx *ut_ctx)
{
	int err;
	ino_t ino;
	ino_t parent = ROOT_INO;
	struct stat st;
	const char *name = "d1";

	err = voluta_ut_mkdir(ut_ctx, parent, name, 0700, &st);
	ut_assert_ok(err);

	err = voluta_ut_lookup(ut_ctx, parent, name, &st);
	ut_assert_ok(err);

	ino = st.st_ino;
	err = voluta_ut_opendir(ut_ctx, ino);
	ut_assert_ok(err);

	err = voluta_ut_releasedir(ut_ctx, ino);
	ut_assert_ok(err);

	err = voluta_ut_rmdir(ut_ctx, parent, name);
	ut_assert_ok(err);

	err = voluta_ut_opendir(ut_ctx, ino);
	ut_assert_err(err, -ENOENT);

	err = voluta_ut_releasedir(ut_ctx, ino);
	ut_assert_err(err, -ENOENT);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct voluta_ut_readdir_ctx *
voluta_ut_new_readdir_ctx(struct voluta_ut_ctx *ut_ctx)
{
	struct voluta_ut_readdir_ctx *readdir_ctx;

	readdir_ctx = voluta_ut_zerobuf(ut_ctx, sizeof(*readdir_ctx));
	return readdir_ctx;
}

static const char *make_name(struct voluta_ut_ctx *ut_ctx, size_t idx)
{
	char name[UT_NAME_MAX + 1] = "";

	snprintf(name, sizeof(name) - 1, "%lx", idx + 1);
	return voluta_ut_strdup(ut_ctx, name);
}

static bool isname(const struct dirent64 *dent, const char *name)
{
	return !strcmp(dent->d_name, name);
}

static bool isdot(const struct dirent64 *dent)
{
	return isname(dent, ".");
}

static bool isdotdot(const struct dirent64 *dent)
{
	return isname(dent, "..");
}

static const struct dirent64 *
find_first_not_dot(const struct dirent64 *dents, size_t n)
{
	const struct dirent64 *dent = dents;

	for (size_t i = 0; i < n; ++i) {
		if (!isdot(dent) && !isdotdot(dent)) {
			return dent;
		}
		++dent;
	}
	return NULL;
}

static const struct dirent64 *
find_any_not_dot(const struct dirent64 *dents, size_t n)
{
	size_t pos = n / 2;
	const struct dirent64 *dent;

	for (size_t i = 0; i < n; ++i) {
		if (pos >= n) {
			pos = 0;
		}
		dent = dents + pos;
		if (!isdot(dent) && !isdotdot(dent)) {
			return dent;
		}
		++pos;
	}
	return NULL;
}

static const struct dirent64 *
find_by_name(const struct dirent64 *dents, size_t n, const char *name)
{
	const struct dirent64 *dent = dents;

	while (n-- > 0) {
		if (!isname(dent, name)) {
			return dent;
		}
		++dent;
	}
	return NULL;
}

static void verify_iter_simple(struct voluta_ut_ctx *ut_ctx,
			       const struct voluta_ut_readdir_ctx *readdir_ctx)
{
	const char *name;
	const struct dirent64 *dent;
	const struct dirent64 *dents = readdir_ctx->dents;
	const size_t ndents = readdir_ctx->ndents;

	ut_assert_ge(readdir_ctx->ndents, 2);
	ut_assert(isdot(&dents[0]));
	ut_assert(isdotdot(&dents[1]));

	for (size_t i = 0; i < ndents - 2; ++i) {
		name = make_name(ut_ctx, i);
		dent = find_by_name(dents, ndents, name);
		ut_assert_notnull(dent);
	}
}

static void ut_dir_iter_simple(struct voluta_ut_ctx *ut_ctx)
{
	int err;
	ino_t dino;
	ino_t root_ino = ROOT_INO;
	const char *name;
	const char *dirname = T_NAME;
	struct stat st;
	struct voluta_ut_readdir_ctx *readdir_ctx;
	const size_t count = ARRAY_SIZE(readdir_ctx->dents) - 2;

	readdir_ctx = voluta_ut_new_readdir_ctx(ut_ctx);
	voluta_ut_mkdir_ok(ut_ctx, root_ino, dirname, &dino);
	voluta_ut_opendir_ok(ut_ctx, dino);

	for (size_t i = 0; i < count; ++i) {
		err = voluta_ut_readdir(ut_ctx, dino, 0, readdir_ctx);
		ut_assert_ok(err);
		ut_assert_eq(readdir_ctx->ndents, i + 2);

		name = make_name(ut_ctx, i);
		err = voluta_ut_mkdir(ut_ctx, dino, name, 0700, &st);
		ut_assert_ok(err);

		err = voluta_ut_readdir(ut_ctx, dino, 0, readdir_ctx);
		ut_assert_ok(err);
		ut_assert_eq(readdir_ctx->ndents, i + 3);

		err = voluta_ut_fsyncdir(ut_ctx, dino, true);
		ut_assert_ok(err);

		verify_iter_simple(ut_ctx, readdir_ctx);
	}
	for (size_t j = count; j > 0; --j) {
		name = make_name(ut_ctx, j - 1);
		err = voluta_ut_rmdir(ut_ctx, dino, name);
		ut_assert_ok(err);

		err = voluta_ut_readdir(ut_ctx, dino, 0, readdir_ctx);
		ut_assert_ok(err);

		verify_iter_simple(ut_ctx, readdir_ctx);
	}
	voluta_ut_releasedir_ok(ut_ctx, dino);
	voluta_ut_rmdir_ok(ut_ctx, root_ino, dirname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_dir_iter_links_(struct voluta_ut_ctx *ut_ctx, size_t count)
{
	int err;
	loff_t doff;
	size_t i, ndents;
	ino_t ino, dino, dino2, root_ino = ROOT_INO;
	const char *lname = NULL;
	const char *fname = T_NAME;
	const char *dname = T_NAME;
	const char *dname2 = "AAA";
	struct stat st;
	const struct dirent64 *dirent = NULL;
	struct voluta_ut_readdir_ctx *readdir_ctx;

	/* TODO: Use comp wrappers */
	err = voluta_ut_mkdir(ut_ctx, root_ino, dname, 0700, &st);
	ut_assert_ok(err);
	dino = st.st_ino;
	err = voluta_ut_opendir(ut_ctx, dino);
	ut_assert_ok(err);
	err = voluta_ut_mkdir(ut_ctx, root_ino, dname2, 0700, &st);
	ut_assert_ok(err);
	dino2 = st.st_ino;
	err = voluta_ut_opendir(ut_ctx, dino2);
	ut_assert_ok(err);
	err = voluta_ut_create(ut_ctx, dino2, fname, S_IFREG | 0600, &st);
	ut_assert_ok(err);
	ino = st.st_ino;
	err = voluta_ut_release(ut_ctx, ino);
	ut_assert_ok(err);

	readdir_ctx = voluta_ut_new_readdir_ctx(ut_ctx);
	for (i = 0; i < count; ++i) {
		lname = make_name(ut_ctx, i);
		err = voluta_ut_link(ut_ctx, ino, dino, lname, &st);
		ut_assert_ok(err);
		ut_assert_eq(ino, st.st_ino);
		ut_assert_eq(i + 2, st.st_nlink);

		err = voluta_ut_fsyncdir(ut_ctx, dino, true);
		ut_assert_ok(err);
	}

	doff = 0;
	for (i = 0; i < count; ++i) {
		err = voluta_ut_readdir(ut_ctx, dino, doff, readdir_ctx);
		ndents = readdir_ctx->ndents;
		ut_assert_ok(err);

		ut_assert_gt(ndents, 0);

		dirent = find_first_not_dot(readdir_ctx->dents, ndents);
		ut_assert_notnull(dirent);

		err = voluta_ut_lookup(ut_ctx, dino, dirent->d_name, &st);
		ut_assert_ok(err);
		ut_assert_eq(ino, st.st_ino);

		doff = dirent->d_off + 1;
	}

	doff = 0;
	for (i = 0; i < count; ++i) {
		err = voluta_ut_readdir(ut_ctx, dino, doff, readdir_ctx);
		ndents = readdir_ctx->ndents;
		ut_assert_ok(err);
		ut_assert_gt(ndents, 0);

		dirent = find_first_not_dot(readdir_ctx->dents, ndents);
		ut_assert_notnull(dirent);
		err = voluta_ut_unlink(ut_ctx, dino, dirent->d_name);
		ut_assert_ok(err);
		err = voluta_ut_lookup(ut_ctx, dino, dirent->d_name, &st);
		ut_assert_err(err, -ENOENT);

		doff = dirent->d_off;
	}

	err = voluta_ut_unlink(ut_ctx, dino2, fname);
	ut_assert_ok(err);
	err = voluta_ut_releasedir(ut_ctx, dino2);
	ut_assert_ok(err);
	err = voluta_ut_releasedir(ut_ctx, dino);
	ut_assert_ok(err);
	err = voluta_ut_rmdir(ut_ctx, root_ino, dname2);
	ut_assert_ok(err);
	err = voluta_ut_rmdir(ut_ctx, root_ino, dname);
	ut_assert_ok(err);
}

static void ut_dir_iter_links(struct voluta_ut_ctx *ut_ctx)
{
	ut_dir_iter_links_(ut_ctx, 10);
	ut_dir_iter_links_(ut_ctx, 100);
	ut_dir_iter_links_(ut_ctx, 1000);
	ut_dir_iter_links_(ut_ctx, 10000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_dir_iter_unlink_(struct voluta_ut_ctx *ut_ctx, size_t count)
{
	int err;
	loff_t doff = 0;
	size_t ndents;
	ino_t ino;
	ino_t dino;
	const ino_t root_ino = ROOT_INO;
	mode_t mode = S_IFREG | 0600;
	const char *fname = NULL;
	const char *dname = T_NAME;
	struct stat st;
	const struct dirent64 *dirent1 = NULL;
	const struct dirent64 *dirent2 = NULL;
	struct voluta_ut_readdir_ctx *readdir_ctx;

	voluta_ut_mkdir_ok(ut_ctx, root_ino, dname, &dino);
	voluta_ut_opendir_ok(ut_ctx, dino);

	for (size_t i = 0; i < count; ++i) {
		fname = make_name(ut_ctx, i);
		err = voluta_ut_create(ut_ctx, dino, fname, mode, &st);
		ut_assert_ok(err);
		ino = st.st_ino;

		err = voluta_ut_release(ut_ctx, ino);
		ut_assert_ok(err);
	}

	readdir_ctx = voluta_ut_new_readdir_ctx(ut_ctx);
	for (size_t i = 0; i < count; ++i) {
		err = voluta_ut_readdir(ut_ctx, dino, doff, readdir_ctx);
		ndents = readdir_ctx->ndents;
		ut_assert_ok(err);
		ut_assert_gt(ndents, 0);

		dirent1 = find_first_not_dot(readdir_ctx->dents, ndents);
		ut_assert_notnull(dirent1);

		dirent2 = find_any_not_dot(readdir_ctx->dents, ndents);
		ut_assert_notnull(dirent2);

		err = voluta_ut_lookup(ut_ctx, dino, dirent2->d_name, &st);
		ut_assert_ok(err);

		err = voluta_ut_unlink(ut_ctx, dino, dirent2->d_name);
		ut_assert_ok(err);

		err = voluta_ut_lookup(ut_ctx, dino, dirent2->d_name, &st);
		ut_assert_err(err, -ENOENT);

		doff = dirent1->d_off;
	}

	voluta_ut_releasedir_ok(ut_ctx, dino);
	voluta_ut_rmdir_ok(ut_ctx, root_ino, dname);
}

static void ut_dir_iter_unlink(struct voluta_ut_ctx *ut_ctx)
{
	ut_dir_iter_unlink_(ut_ctx, 10000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_dir_open_release),
	UT_DEFTEST(ut_dir_iter_simple),
	UT_DEFTEST(ut_dir_iter_links),
	UT_DEFTEST(ut_dir_iter_unlink),
};

const struct voluta_ut_tests voluta_ut_test_dir_iter =
	VOLUTA_UT_MKTESTS(ut_local_tests);
