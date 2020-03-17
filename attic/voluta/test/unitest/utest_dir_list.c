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
#include <unistd.h>
#include <dirent.h>
#include "unitest.h"


struct voluta_ut_direlem {
	struct voluta_ut_direlem *next;
	struct dirent64 dent;
	mode_t mode;
};

struct voluta_ut_dirlist {
	struct voluta_ut_ctx *ut_ctx;
	ino_t dino;
	size_t count;
	struct voluta_ut_direlem *list;
};


static struct voluta_ut_direlem *
new_direlem(struct voluta_ut_ctx *ut_ctx, const struct dirent64 *dent)
{
	struct voluta_ut_direlem *de;

	de = voluta_ut_zerobuf(ut_ctx, sizeof(*de));
	if (dent != NULL) {
		memcpy(&de->dent, dent, sizeof(de->dent));
		de->mode = DTTOIF((mode_t)dent->d_type);
	}
	return de;
}

static struct voluta_ut_dirlist *
new_dirlist(struct voluta_ut_ctx *ut_ctx, ino_t dino)
{
	struct voluta_ut_dirlist *dl;

	dl = voluta_ut_zerobuf(ut_ctx, sizeof(*dl));
	dl->ut_ctx = ut_ctx;
	dl->dino = dino;
	dl->count = 0;
	dl->list = NULL;
	return dl;
}

static void push_direlem(struct voluta_ut_dirlist *dl,
			 struct voluta_ut_direlem *de)
{
	de->next = dl->list;
	dl->list = de;
	dl->count++;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool is_dot_or_dotdot(const struct dirent64 *dent)
{
	return !strcmp(dent->d_name, ".") || !strcmp(dent->d_name, "..");
}

static struct voluta_ut_readdir_ctx *ut_new_readdir_ctx(
	struct voluta_ut_ctx *ut_ctx)
{
	return voluta_ut_new_readdir_ctx(ut_ctx);
}

static void ut_opendir_ok(struct voluta_ut_ctx *ut_ctx, ino_t ino)
{
	int err;

	err = voluta_ut_opendir(ut_ctx, ino);
	ut_assert_ok(err);
}

static void ut_releasedir_ok(struct voluta_ut_ctx *ut_ctx, ino_t ino)
{
	int err;

	err = voluta_ut_releasedir(ut_ctx, ino);
	ut_assert_ok(err);
}

static void ut_readdir_ok(struct voluta_ut_ctx *ut_ctx, ino_t ino, loff_t doff,
			  struct voluta_ut_readdir_ctx *readdir_ctx)
{
	int err;

	err = voluta_ut_readdir(ut_ctx, ino, doff, readdir_ctx);
	ut_assert_ok(err);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_ut_dirlist *
dir_list(struct voluta_ut_ctx *ut_ctx, ino_t dino, size_t expected_nents)
{
	int partial = 0;
	loff_t doff = 0;
	size_t ndents = 1;
	size_t dots = 0;
	const struct dirent64 *dent;
	struct voluta_ut_dirlist *dl = new_dirlist(ut_ctx, dino);
	struct voluta_ut_readdir_ctx *readdir_ctx = ut_new_readdir_ctx(ut_ctx);
	const size_t ndents_max = ARRAY_SIZE(readdir_ctx->dents);

	ut_opendir_ok(ut_ctx, dino);
	while (ndents > 0) {
		ut_readdir_ok(ut_ctx, dino, doff, readdir_ctx);
		ndents = readdir_ctx->ndents;
		ut_assert_le(ndents, ndents_max);

		if (ndents && (ndents < ndents_max)) {
			ut_assert_eq(partial, 0);
			partial++;
		}
		for (size_t i = 0; i < ndents; ++i) {
			dent = &readdir_ctx->dents[i];
			if (!is_dot_or_dotdot(dent)) {
				ut_assert_lt(dl->count, expected_nents);
				push_direlem(dl, new_direlem(ut_ctx, dent));
			} else {
				ut_assert_lt(dots, 2);
				dots++;
			}
			doff = dent->d_off + 1;
		}
	}
	if (expected_nents < UINT_MAX) {
		ut_assert_eq(dl->count, expected_nents);
	}
	ut_releasedir_ok(ut_ctx, dino);
	return dl;
}

static struct voluta_ut_dirlist *
dir_list_all(struct voluta_ut_ctx *ut_ctx, ino_t dino)
{
	return dir_list(ut_ctx, dino, UINT_MAX);
}

static struct voluta_ut_dirlist *
dir_list_some(struct voluta_ut_ctx *ut_ctx, ino_t dino, loff_t off,
	      size_t max_nents)
{
	bool keep_iter = true;
	loff_t doff = off;
	const struct dirent64 *dent;
	struct voluta_ut_dirlist *dl = new_dirlist(ut_ctx, dino);
	struct voluta_ut_readdir_ctx *readdir_ctx = ut_new_readdir_ctx(ut_ctx);

	ut_opendir_ok(ut_ctx, dino);
	while (keep_iter) {
		ut_readdir_ok(ut_ctx, dino, doff, readdir_ctx);
		for (size_t i = 0; i < readdir_ctx->ndents; ++i) {
			dent = &readdir_ctx->dents[i];
			if (!is_dot_or_dotdot(dent)) {
				push_direlem(dl, new_direlem(ut_ctx, dent));
				if (dl->count == max_nents) {
					keep_iter = false;
				}
			}
			doff = dent->d_off + 1;
		}
		if (!readdir_ctx->ndents) {
			keep_iter = false;
		}
	}
	ut_releasedir_ok(ut_ctx, dino);
	return dl;
}

static void dir_unlink_all(struct voluta_ut_dirlist *dl)
{
	size_t count = 0;
	const char *name;
	const struct voluta_ut_direlem *de;

	for (de = dl->list; de != NULL; de = de->next) {
		ut_assert_lt(count, dl->count);
		name = de->dent.d_name;
		if (S_ISDIR(de->mode)) {
			voluta_ut_rmdir_ok(dl->ut_ctx, dl->dino, name);
		} else if (S_ISLNK(de->mode)) {
			voluta_ut_remove_link(dl->ut_ctx, dl->dino, name);
		} else {
			voluta_ut_unlink_exists(dl->ut_ctx, dl->dino, name);
		}
		count += 1;
	}
	ut_assert_eq(count, dl->count);
	dl->count = 0;
	dl->list = NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const char *make_name(struct voluta_ut_ctx *ut_ctx,
			     const char *prefix, size_t idx)
{
	char name[UT_NAME_MAX + 1] = "";

	snprintf(name, sizeof(name) - 1, "%s%ld", prefix, idx + 1);
	return voluta_ut_strdup(ut_ctx, name);
}

static void create_nfiles(struct voluta_ut_ctx *ut_ctx, ino_t dino,
			  const char *dname, size_t count)
{
	ino_t ino;
	const char *name;

	for (size_t i = 0; i < count; ++i) {
		name = make_name(ut_ctx, dname, i);
		voluta_ut_create_only(ut_ctx, dino, name, &ino);
	}
}

static void create_ndirs_files(struct voluta_ut_ctx *ut_ctx, ino_t dino,
			       const char *dname, size_t count)
{
	ino_t ino;
	const char *name;
	char s[256] = "";

	for (size_t i = 0; i < count; ++i) {
		name = make_name(ut_ctx, dname, i);
		if ((i % 3) == 0) {
			voluta_ut_mkdir_ok(ut_ctx, dino, name, &ino);
		} else if ((i % 5) == 0) {
			snprintf(s, sizeof(s) - 1, "%s_%lu", dname, i);
			voluta_ut_create_symlink(ut_ctx, dino, name, s, &ino);
		} else {
			voluta_ut_create_only(ut_ctx, dino, name, &ino);
		}
	}
}


static void ut_dir_list_simple_(struct voluta_ut_ctx *ut_ctx, size_t count)
{
	ino_t dino, parent = ROOT_INO;
	const char *dname = T_NAME;
	struct voluta_ut_dirlist *dl;

	voluta_ut_mkdir_ok(ut_ctx, parent, dname, &dino);
	create_nfiles(ut_ctx, dino, dname, count);
	dl = dir_list(ut_ctx, dino, count);
	dir_unlink_all(dl);
	create_ndirs_files(ut_ctx, dino, dname, count);
	dl = dir_list(ut_ctx, dino, count);
	dir_unlink_all(dl);
	voluta_ut_releasedir(ut_ctx, dino);
	voluta_ut_rmdir_ok(ut_ctx, parent, dname);
}

static void ut_dir_list_simple(struct voluta_ut_ctx *ut_ctx)
{
	ut_dir_list_simple_(ut_ctx, 1 << 4);
	ut_dir_list_simple_(ut_ctx, 1 << 8);
	ut_dir_list_simple_(ut_ctx, 1 << 10);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_dir_list_repeated_(struct voluta_ut_ctx *ut_ctx,
				  size_t count, size_t niter)
{
	size_t count2 = count / 2;
	ino_t dino, parent = ROOT_INO;
	const char *prefix = NULL;
	const char *dname = T_NAME;
	struct voluta_ut_dirlist *dl;

	voluta_ut_mkdir_ok(ut_ctx, parent, dname, &dino);
	while (niter-- > 0) {
		prefix = ut_randstr(ut_ctx, 31);
		create_nfiles(ut_ctx, dino, prefix, count);
		dl = dir_list(ut_ctx, dino, count);
		dir_unlink_all(dl);

		prefix = ut_randstr(ut_ctx, 127);
		create_ndirs_files(ut_ctx, dino, prefix, count2);
		dl = dir_list(ut_ctx, dino, count2);
		dir_unlink_all(dl);
	}
	voluta_ut_releasedir(ut_ctx, dino);
	voluta_ut_rmdir_ok(ut_ctx, parent, dname);
}

static void ut_dir_list_repeated(struct voluta_ut_ctx *ut_ctx)
{
	ut_dir_list_repeated_(ut_ctx, 1 << 4, 5);
	ut_dir_list_repeated_(ut_ctx, 1 << 6, 4);
	ut_dir_list_repeated_(ut_ctx, 1 << 10, 3);
	ut_dir_list_repeated_(ut_ctx, 1 << 14, 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void create_nfiles_sparse(struct voluta_ut_ctx *ut_ctx, ino_t dino,
				 const char *prefix, size_t count)
{
	ino_t ino;
	const char *name;

	for (size_t i = 0; i < (2 * count); ++i) {
		name = make_name(ut_ctx, prefix, i);
		voluta_ut_create_only(ut_ctx, dino, name, &ino);
	}
	for (size_t j = 1; j < (2 * count); j += 2) {
		name = make_name(ut_ctx, prefix, j);
		voluta_ut_unlink_exists(ut_ctx, dino, name);
	}
}

static void ut_dir_list_sparse_(struct voluta_ut_ctx *ut_ctx, size_t count)
{
	loff_t doff = (loff_t)count;
	ino_t dino, parent = ROOT_INO;
	const char *dname = T_NAME;
	struct voluta_ut_dirlist *dl;

	voluta_ut_mkdir_ok(ut_ctx, parent, dname, &dino);

	create_nfiles_sparse(ut_ctx, dino, ut_randstr(ut_ctx, 71), count);
	dl = dir_list_some(ut_ctx, dino, (2 * doff) / 3, count);
	dir_unlink_all(dl);
	dl = dir_list_some(ut_ctx, dino, doff / 3, count);
	dir_unlink_all(dl);
	create_nfiles_sparse(ut_ctx, dino, ut_randstr(ut_ctx, 127), count / 2);
	dl = dir_list_some(ut_ctx, dino, doff / 3, count);
	dir_unlink_all(dl);
	dl = dir_list_all(ut_ctx, dino);
	dir_unlink_all(dl);

	voluta_ut_releasedir(ut_ctx, dino);
	voluta_ut_rmdir_ok(ut_ctx, parent, dname);
}

static void ut_dir_list_sparse(struct voluta_ut_ctx *ut_ctx)
{
	ut_dir_list_sparse_(ut_ctx, 11);
	ut_dir_list_sparse_(ut_ctx, 1111);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_dir_list_simple),
	UT_DEFTEST(ut_dir_list_repeated),
	UT_DEFTEST(ut_dir_list_sparse),
};

const struct voluta_ut_tests voluta_ut_utest_dir_list =
	UT_MKTESTS(ut_local_tests);
