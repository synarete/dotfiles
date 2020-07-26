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
#include <unistd.h>
#include <dirent.h>
#include "unitest.h"


struct ut_direlem {
	struct ut_direlem *next;
	struct ut_dirent_info dei;
	mode_t mode;
	int pad;
};

struct ut_dirlist {
	struct ut_env *ut_env;
	ino_t dino;
	size_t count;
	struct ut_direlem *list;
};


static struct ut_direlem *
new_direlem(struct ut_env *ut_env, const struct ut_dirent_info *dei)
{
	struct ut_direlem *de;

	de = ut_zerobuf(ut_env, sizeof(*de));
	ut_assert_not_null(de);

	memcpy(&de->dei, dei, sizeof(de->dei));
	de->mode = DTTOIF((mode_t)dei->de.d_type);

	return de;
}

static struct ut_dirlist *
new_dirlist(struct ut_env *ut_env, ino_t dino)
{
	struct ut_dirlist *dl;

	dl = ut_zerobuf(ut_env, sizeof(*dl));
	dl->ut_env = ut_env;
	dl->dino = dino;
	dl->count = 0;
	dl->list = NULL;
	return dl;
}

static void push_direlem(struct ut_dirlist *dl,
			 struct ut_direlem *de)
{
	de->next = dl->list;
	dl->list = de;
	dl->count++;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct ut_dirlist *
dir_list(struct ut_env *ut_env, ino_t dino, size_t expected_nents)
{
	int partial = 0;
	loff_t doff = 0;
	size_t ndents = 1;
	size_t dots = 0;
	const struct ut_dirent_info *dei;
	struct ut_readdir_ctx *rd_ctx;
	struct ut_dirlist *dl = new_dirlist(ut_env, dino);
	const size_t ndents_max = UT_ARRAY_SIZE(rd_ctx->dei);

	rd_ctx = ut_new_readdir_ctx(ut_env);
	ut_opendir_ok(ut_env, dino);
	while (ndents > 0) {
		ut_readdir_ok(ut_env, dino, doff, rd_ctx);
		ndents = rd_ctx->nde;
		ut_assert_le(ndents, ndents_max);

		if (ndents && (ndents < ndents_max)) {
			ut_assert_eq(partial, 0);
			partial++;
		}
		for (size_t i = 0; i < ndents; ++i) {
			dei = &rd_ctx->dei[i];
			if (!ut_dot_or_dotdot(dei->de.d_name)) {
				ut_assert_lt(dl->count, expected_nents);
				push_direlem(dl, new_direlem(ut_env, dei));
			} else {
				ut_assert_lt(dots, 2);
				dots++;
			}
			doff = dei->de.d_off + 1;
		}
	}
	if (expected_nents < UINT_MAX) {
		ut_assert_eq(dl->count, expected_nents);
	}
	ut_releasedir_ok(ut_env, dino);
	return dl;
}

static struct ut_dirlist *dir_list_all(struct ut_env *ut_env, ino_t dino)
{
	return dir_list(ut_env, dino, UINT_MAX);
}

static struct ut_dirlist *dir_list_some(struct ut_env *ut_env, ino_t dino,
					loff_t off, size_t max_nents)
{
	bool keep_iter = true;
	loff_t doff = off;
	const struct ut_dirent_info *dei;
	struct ut_dirlist *dl = new_dirlist(ut_env, dino);
	struct ut_readdir_ctx *rd_ctx =
		ut_new_readdir_ctx(ut_env);

	ut_opendir_ok(ut_env, dino);
	while (keep_iter) {
		ut_readdir_ok(ut_env, dino, doff, rd_ctx);
		for (size_t i = 0; i < rd_ctx->nde; ++i) {
			dei = &rd_ctx->dei[i];
			if (!ut_dot_or_dotdot(dei->de.d_name)) {
				push_direlem(dl, new_direlem(ut_env, dei));
				if (dl->count == max_nents) {
					keep_iter = false;
				}
			}
			doff = dei->de.d_off + 1;
		}
		if (!rd_ctx->nde) {
			keep_iter = false;
		}
	}
	ut_releasedir_ok(ut_env, dino);
	return dl;
}

static void dir_unlink_all(struct ut_dirlist *dl)
{
	size_t count = 0;
	const char *name;
	const struct ut_direlem *de;

	for (de = dl->list; de != NULL; de = de->next) {
		ut_assert_lt(count, dl->count);
		name = de->dei.de.d_name;
		if (S_ISDIR(de->mode)) {
			ut_rmdir_ok(dl->ut_env, dl->dino, name);
		} else if (S_ISLNK(de->mode)) {
			ut_remove_link(dl->ut_env, dl->dino, name);
		} else {
			ut_unlink_ok(dl->ut_env, dl->dino, name);
		}
		count += 1;
	}
	ut_assert_eq(count, dl->count);
	dl->count = 0;
	dl->list = NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_create_nfiles(struct ut_env *ut_env, ino_t dino,
			     const char *dname, size_t count)
{
	ino_t ino;
	const char *name;

	for (size_t i = 0; i < count; ++i) {
		name = ut_make_name(ut_env, dname, i);
		ut_create_only(ut_env, dino, name, &ino);
	}
}

static void ut_create_ninodes(struct ut_env *ut_env, ino_t dino,
			      const char *dname, size_t count)
{
	ino_t ino;
	const char *name;
	char s[256] = "";
	struct stat st;

	for (size_t i = 0; i < count; ++i) {
		name = ut_make_name(ut_env, dname, i);
		if ((i % 3) == 0) {
			ut_mkdir_oki(ut_env, dino, name, &ino);
		} else if ((i % 5) == 0) {
			snprintf(s, sizeof(s) - 1, "%s_%lu", dname, i);
			ut_symlink_ok(ut_env, dino, name, s, &ino);
		} else {
			ut_create_only(ut_env, dino, name, &ino);
		}
	}
	ut_getattr_ok(ut_env, dino, &st);
	ut_assert_ge(st.st_size, count);
}


static void ut_dir_list_simple_(struct ut_env *ut_env, size_t count)
{
	ino_t dino;
	const char *name = UT_NAME;
	struct ut_dirlist *dl;

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_create_nfiles(ut_env, dino, name, count);
	dl = dir_list(ut_env, dino, count);
	dir_unlink_all(dl);
	ut_create_ninodes(ut_env, dino, name, count);
	dl = dir_list(ut_env, dino, count);
	dir_unlink_all(dl);
	ut_rmdir_at_root(ut_env, name);
}

static void ut_dir_list_simple(struct ut_env *ut_env)
{
	ut_dir_list_simple_(ut_env, 1 << 4);
	ut_dir_list_simple_(ut_env, 1 << 8);
	ut_dir_list_simple_(ut_env, 1 << 10);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_dir_list_repeated_(struct ut_env *ut_env,
				  size_t count, size_t niter)
{
	ino_t dino;
	const char *prefix = NULL;
	const char *name = UT_NAME;
	struct ut_dirlist *dl;

	ut_mkdir_at_root(ut_env, name, &dino);
	while (niter-- > 0) {
		prefix = ut_randstr(ut_env, 31);
		ut_create_nfiles(ut_env, dino, prefix, count);
		dl = dir_list(ut_env, dino, count);
		dir_unlink_all(dl);

		prefix = ut_randstr(ut_env, 127);
		ut_create_ninodes(ut_env, dino, prefix, count / 2);
		dl = dir_list(ut_env, dino, count / 2);
		dir_unlink_all(dl);
	}
	ut_rmdir_at_root(ut_env, name);
}

static void ut_dir_list_repeated(struct ut_env *ut_env)
{
	ut_dir_list_repeated_(ut_env, 1 << 4, 5);
	ut_dir_list_repeated_(ut_env, 1 << 6, 4);
	ut_dir_list_repeated_(ut_env, 1 << 10, 3);
	ut_dir_list_repeated_(ut_env, 1 << 14, 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void create_nfiles_sparse(struct ut_env *ut_env, ino_t dino,
				 const char *prefix, size_t count)
{
	ino_t ino;
	const char *name;

	for (size_t i = 0; i < (2 * count); ++i) {
		name = ut_make_name(ut_env, prefix, i);
		ut_create_only(ut_env, dino, name, &ino);
	}
	for (size_t j = 1; j < (2 * count); j += 2) {
		name = ut_make_name(ut_env, prefix, j);
		ut_unlink_ok(ut_env, dino, name);
	}
}

static void ut_dir_list_sparse_(struct ut_env *ut_env, size_t count)
{
	ino_t dino;
	loff_t doff = (loff_t)count;
	const char *dname = UT_NAME;
	struct ut_dirlist *dl = NULL;

	ut_mkdir_at_root(ut_env, dname, &dino);
	create_nfiles_sparse(ut_env, dino, ut_randstr(ut_env, 71), count);
	dl = dir_list_some(ut_env, dino, (2 * doff) / 3, count);
	dir_unlink_all(dl);
	dl = dir_list_some(ut_env, dino, doff / 3, count);
	dir_unlink_all(dl);
	create_nfiles_sparse(ut_env, dino, ut_randstr(ut_env, 127), count / 2);
	dl = dir_list_some(ut_env, dino, doff / 3, count);
	dir_unlink_all(dl);
	dl = dir_list_all(ut_env, dino);
	dir_unlink_all(dl);
	ut_rmdir_at_root(ut_env, dname);
}

static void ut_dir_list_sparse(struct ut_env *ut_env)
{
	ut_dir_list_sparse_(ut_env, 11);
	ut_dir_list_sparse_(ut_env, 1111);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_dir_list_simple),
	UT_DEFTEST(ut_dir_list_repeated),
	UT_DEFTEST(ut_dir_list_sparse),
};

const struct ut_tests ut_test_dir_list = UT_MKTESTS(ut_local_tests);
