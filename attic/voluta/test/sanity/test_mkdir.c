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
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include "sanity.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects mkdir(3p) to create nested directories structure and allow rmdir(3p)
 * only to apply on last. Expects all other non-empty directories to return
 * -ENOTEMPTY upon rmdir(3p).
 */
static void test_rmdir_notempty(struct voluta_t_ctx *t_ctx,
				char const **pathi, size_t count)
{
	for (size_t i = 0; i < count; ++i) {
		voluta_t_rmdir_err(pathi[i], -ENOTEMPTY);
	}
	for (size_t j = count; j > 0; --j) {
		voluta_t_rmdir_err(pathi[j - 1], -ENOTEMPTY);
	}
	voluta_unused(t_ctx);
}

static void test_mkdir_rmdir(struct voluta_t_ctx *t_ctx)
{
	const char *pathi[32];
	const size_t nelems = ARRAY_SIZE(pathi);
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = path0;

	voluta_t_mkdir(path0, 0700);
	for (size_t i = 0; i < nelems; ++i) {
		path1 = voluta_t_new_pathf(t_ctx, path1, "D%d", (int)i);
		voluta_t_mkdir(path1, 0700);
		pathi[i] = path1;
	}
	for (size_t j = nelems; j > 0; --j) {
		test_rmdir_notempty(t_ctx, pathi, j - 1);
		path1 = pathi[j - 1];
		voluta_t_rmdir(path1);
	}
	voluta_t_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects mkdir(3p) to create a directory with a mode modified by the process'
 * umask.
 */
static void test_mkdir_umask(struct voluta_t_ctx *t_ctx)
{
	mode_t umsk;
	const mode_t ifmt = S_IFMT;
	struct stat st0;
	struct stat st1;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_under(t_ctx, path0);

	umsk  = umask(0020);
	voluta_t_mkdir(path0, 0755);
	voluta_t_stat(path0, &st0);
	voluta_t_expect_dir(st0.st_mode);

	voluta_t_mkdir(path1, 0755);
	voluta_t_stat(path1, &st1);
	voluta_t_expect_dir(st1.st_mode);
	voluta_t_expect_eq((st1.st_mode & ~ifmt), 0755);
	voluta_t_rmdir(path1);

	voluta_t_mkdir(path1, 0153);
	voluta_t_stat(path1, &st1);
	voluta_t_expect_dir(st1.st_mode);
	voluta_t_expect_eq((st1.st_mode & ~ifmt), 0153);
	voluta_t_rmdir(path1);

	umask(077);
	voluta_t_mkdir(path1, 0151);
	voluta_t_stat(path1, &st1);
	voluta_t_expect_dir(st1.st_mode);
	voluta_t_expect_eq((st1.st_mode & ~ifmt), 0100);
	voluta_t_rmdir(path1);

	umask(070);
	voluta_t_mkdir(path1, 0345);
	voluta_t_stat(path1, &st1);
	voluta_t_expect_dir(st1.st_mode);
	voluta_t_expect_eq((st1.st_mode & ~ifmt), 0305);
	voluta_t_rmdir(path1);

	umask(0501);
	voluta_t_mkdir(path1, 0345);
	voluta_t_stat(path1, &st1);
	voluta_t_expect_dir(st1.st_mode);
	voluta_t_expect_eq((st1.st_mode & ~ifmt), 0244);

	voluta_t_rmdir(path1);
	voluta_t_rmdir(path0);
	voluta_t_stat_noent(path0);
	voluta_t_stat_noent(path1);
	umask(umsk);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects mkdir(3p) to create a nested directory when parent directory is
 * writable or not.
 */
static void test_mkdir_chmod(struct voluta_t_ctx *t_ctx)
{
	struct stat st;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_under(t_ctx, path0);
	const char *path2 = voluta_t_new_path_under(t_ctx, path1);

	voluta_t_mkdir(path0, 0700);
	voluta_t_stat(path0, &st);
	voluta_t_expect_true(st.st_mode & S_IWUSR);
	voluta_t_expect_true(st.st_mode & S_IXUSR);
	voluta_t_mkdir(path1, st.st_mode);
	voluta_t_stat(path1, &st);
	voluta_t_expect_true(st.st_mode & S_IWUSR);
	voluta_t_expect_true(st.st_mode & S_IXUSR);
	voluta_t_chmod(path1, st.st_mode & ~((mode_t)S_IRUSR));
	voluta_t_stat(path1, &st);
	voluta_t_expect_false(st.st_mode & S_IRUSR);
	voluta_t_mkdir(path2, st.st_mode);
	voluta_t_stat(path2, &st);
	voluta_t_expect_true(st.st_mode & S_IWUSR);
	voluta_t_expect_true(st.st_mode & S_IXUSR);
	voluta_t_expect_false(st.st_mode & S_IRUSR);
	voluta_t_chmod(path2, st.st_mode & ~((mode_t)S_IXUSR));
	voluta_t_stat(path2, &st);
	voluta_t_expect_true(st.st_mode & S_IWUSR);
	voluta_t_expect_false(st.st_mode & S_IXUSR);
	voluta_t_rmdir(path2);
	voluta_t_stat(path1, &st);
	voluta_t_expect_true(st.st_mode & S_IWUSR);
	voluta_t_chmod(path1, st.st_mode & ~((mode_t)S_IWUSR));
	voluta_t_mkdir_err(path2, 0700, -EACCES);
	voluta_t_rmdir(path1);
	voluta_t_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects mkdir(3p) to return ELOOP if too many symbolic links were
 * encountered in translating of the pathname.
 */
static void test_mkdir_loop(struct voluta_t_ctx *t_ctx)
{
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_unique(t_ctx);
	const char *path2 = voluta_t_new_path_under(t_ctx, path0);
	const char *path3 = voluta_t_new_path_under(t_ctx, path1);

	voluta_t_symlink(path0, path1);
	voluta_t_symlink(path1, path0);
	voluta_t_mkdir_err(path2, 0755, -ELOOP);
	voluta_t_mkdir_err(path3, 0750, -ELOOP);
	voluta_t_unlink(path0);
	voluta_t_unlink(path1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Verify creation & removal of many-many dir-entries.
 */
static void test_mkdir_many(struct voluta_t_ctx *t_ctx, size_t lim)
{
	int fd;
	int dfd;
	const char *name;
	struct stat st;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_mkdir(path, 0755);
	voluta_t_open(path, O_DIRECTORY | O_RDONLY, 0, &dfd);

	for (size_t i = 0; i < lim; ++i) {
		name = voluta_t_strfmt(t_ctx, "%08jx", i);
		voluta_t_openat(dfd, name, O_CREAT | O_RDWR, 0644, &fd);
		voluta_t_close(fd);
	}
	for (size_t j = 0; j < lim; ++j) {
		name = voluta_t_strfmt(t_ctx, "%08jx", j);
		voluta_t_fstatat(dfd, name, &st, 0);
		voluta_t_unlinkat(dfd, name, 0);
	}
	voluta_t_stat(path, &st);
	voluta_t_expect_eq(st.st_nlink, 2);
	voluta_t_close(dfd);
	voluta_t_rmdir(path);
}

static void test_mkdir_big(struct voluta_t_ctx *t_ctx)
{
	test_mkdir_many(t_ctx, 1000);
}

static void test_mkdir_bigger(struct voluta_t_ctx *t_ctx)
{
	test_mkdir_many(t_ctx, 30000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Create and remove deeply-nested directories structure
 */
static void test_mkdir_nested(struct voluta_t_ctx *t_ctx)
{
	size_t i;
	char *path0, *path1;
	char *pathi[64];

	path1 = path0 = voluta_t_new_path_unique(t_ctx);
	voluta_t_mkdir(path0, 0700);
	for (i = 0; i < ARRAY_SIZE(pathi); ++i) {
		path1 = voluta_t_new_pathf(t_ctx, path1, "D%d", (int)i);
		voluta_t_mkdir(path1, 0700);
		pathi[i] = path1;
	}
	for (i = ARRAY_SIZE(pathi); i > 0; --i) {
		path1 = pathi[i - 1];
		voluta_t_rmdir(path1);
	}
	voluta_t_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Create recursively directory tree structure
 */
static const char *makename(struct voluta_t_ctx *t_ctx,
			    const char *prefix, size_t depth, size_t id)
{
	return voluta_t_strfmt(t_ctx, "%s%03x-%03x",
			       prefix, (int)depth, (int)id);
}

static void test_walktree_recursive(struct voluta_t_ctx *t_ctx,
				    const char *base)
{
	int fd;
	loff_t pos, off = 0;
	struct dirent64 dent = { .d_ino = 0 };
	const char *path;

	voluta_t_open(base, O_DIRECTORY | O_RDONLY, 0, &fd);
	while (1) {
		voluta_t_llseek(fd, off, SEEK_SET, &pos);
		voluta_t_getdent(fd, &dent);
		off = dent.d_off;
		if (off <= 0) {
			break;
		}
		if (!strcmp(dent.d_name, ".") || !strcmp(dent.d_name, "..")) {
			continue;
		}
		if (dent.d_type == DT_DIR) {
			continue;
		}
		path = voluta_t_new_path_nested(t_ctx, base, dent.d_name);
		test_walktree_recursive(t_ctx, path);
	}
	voluta_t_close(fd);
}


static void test_mktree_recursive(struct voluta_t_ctx *t_ctx,
				  const char *parent,
				  size_t id, size_t nchilds,
				  size_t depth, size_t depth_max)
{
	int fd;
	const char *path;
	const char *name;

	if (depth < depth_max) {
		name = makename(t_ctx, "d", depth, id);
		path = voluta_t_new_path_nested(t_ctx, parent, name);
		voluta_t_mkdir(path, 0700);
		for (size_t i = 0; i < nchilds; ++i) {
			test_mktree_recursive(t_ctx, path, i + 1, nchilds,
					      depth + 1, depth_max);
		}
	} else {
		name = makename(t_ctx, "f", depth, id);
		path = voluta_t_new_path_nested(t_ctx, parent, name);
		voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
		voluta_t_close(fd);
	}
}

static void test_rmtree_recursive(struct voluta_t_ctx *t_ctx,
				  const char *parent,
				  size_t id, size_t nchilds,
				  size_t depth, size_t depth_max)
{
	const char *path;
	const char *name;

	if (depth < depth_max) {
		name = makename(t_ctx, "d", depth, id);
		path = voluta_t_new_path_nested(t_ctx, parent, name);
		for (size_t i = 0; i < nchilds; ++i) {
			test_rmtree_recursive(t_ctx, path, i + 1, nchilds,
					      depth + 1, depth_max);
		}
		voluta_t_rmdir(path);
	} else {
		name = makename(t_ctx, "f", depth, id);
		path = voluta_t_new_path_nested(t_ctx, parent, name);
		voluta_t_unlink(path);
	}
}

static void test_mkdir_tree_(struct voluta_t_ctx *t_ctx,
			     size_t nchilds, size_t depth_max)
{
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_mkdir(path, 0700);
	for (size_t i = 0; i < nchilds; ++i) {
		test_mktree_recursive(t_ctx, path, i + 1,
				      nchilds, 1, depth_max);
	}
	test_walktree_recursive(t_ctx, path);
	for (size_t j = 0; j < nchilds; ++j) {
		test_rmtree_recursive(t_ctx, path, j + 1,
				      nchilds, 1, depth_max);
	}
	voluta_t_rmdir(path);
}

static void test_mkdir_tree_wide(struct voluta_t_ctx *t_ctx)
{
	test_mkdir_tree_(t_ctx, 32, 2);
}

static void test_mkdir_tree_deep(struct voluta_t_ctx *t_ctx)
{
	test_mkdir_tree_(t_ctx, 2, 8);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_mkdir_rmdir),
	VOLUTA_T_DEFTEST(test_mkdir_umask),
	VOLUTA_T_DEFTEST(test_mkdir_chmod),
	VOLUTA_T_DEFTEST(test_mkdir_loop),
	VOLUTA_T_DEFTEST(test_mkdir_nested),
	VOLUTA_T_DEFTEST(test_mkdir_big),
	VOLUTA_T_DEFTEST(test_mkdir_bigger),
	VOLUTA_T_DEFTEST(test_mkdir_tree_wide),
	VOLUTA_T_DEFTEST(test_mkdir_tree_deep),
};

const struct voluta_t_tests
voluta_t_test_mkdir = VOLUTA_T_DEFTESTS(t_local_tests);
