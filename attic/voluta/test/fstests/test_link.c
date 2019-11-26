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
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include "fstests.h"


/* Maximum hard-links per file */
static size_t get_link_max(void)
{
	long ret;

	ret = sysconf(_PC_LINK_MAX);
	voluta_t_expect_gt(ret, 0);
	voluta_t_expect_lt(ret, VOLUTA_GIGA);

	return voluta_min((size_t)ret, 2048);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects link(3p) to return EEXIST if the path2 argument resolves to an
 * existing file or refers to a symbolic link.
 */
static void test_link_exists(struct voluta_t_ctx *t_ctx)
{
	int fd0, fd1;
	char *path0, *path1, *path2;

	path0 = voluta_t_new_path_unique(t_ctx);
	path1 = voluta_t_new_path_unique(t_ctx);
	path2 = voluta_t_new_path_name(t_ctx, "test-link-to-symlink-exist");

	voluta_t_creat(path0, 0644, &fd0);
	voluta_t_creat(path1, 0644, &fd1);

	voluta_t_link_err(path0, path1, -EEXIST);
	voluta_t_unlink(path1);

	voluta_t_mkdir(path1, 0755);
	voluta_t_link_err(path0, path1, -EEXIST);
	voluta_t_rmdir(path1);

	voluta_t_symlink(path1, path2);
	voluta_t_link_err(path0, path2, -EEXIST);
	voluta_t_unlink(path2);

	voluta_t_mkfifo(path1, 0644);
	voluta_t_link_err(path0, path1, -EEXIST);
	voluta_t_unlink(path1);

	voluta_t_unlink(path0);
	voluta_t_close(fd0);
	voluta_t_close(fd1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects link(3p) to return ENOENT if the source file does not exist.
 */
static void test_link_noent(struct voluta_t_ctx *t_ctx)
{
	int fd;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_under(t_ctx, path0);
	const char *path2 = voluta_t_new_path_under(t_ctx, path0);

	voluta_t_mkdir(path0, 0700);
	voluta_t_creat(path1, 0640, &fd);
	voluta_t_link(path1, path2);
	voluta_t_unlink(path1);
	voluta_t_unlink(path2);
	voluta_t_link_err(path1, path2, -ENOENT);
	voluta_t_rmdir(path0);
	voluta_t_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects link(3p) to return EEXIST if a component of either path prefix is
 * not a directory.
 */
static void test_link_notdir(struct voluta_t_ctx *t_ctx)
{
	int fd1, fd2;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_under(t_ctx, path0);
	const char *path2 = voluta_t_new_path_under(t_ctx, path0);
	const char *path3 = voluta_t_new_path_under(t_ctx, path1);

	voluta_t_mkdir(path0, 0755);
	voluta_t_creat(path1, 0644, &fd1);
	voluta_t_link_err(path3, path2, -ENOTDIR);
	voluta_t_creat(path2, 0644, &fd2);
	voluta_t_link_err(path2, path3, -ENOTDIR);
	voluta_t_unlink(path1);
	voluta_t_unlink(path2);
	voluta_t_rmdir(path0);
	voluta_t_close(fd1);
	voluta_t_close(fd2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects link(3p)/unlink(3p) sequence to succeed for renamed links.
 */
static void test_link_rename_cnt(struct voluta_t_ctx *t_ctx, int cnt)
{
	int i, fd, nlink1, limit;
	struct stat st;
	char *name, *path2, *path3;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_under(t_ctx, path0);

	voluta_t_mkdir(path0, 0700);
	voluta_t_creat(path1, 0600, &fd);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_nlink, 1);
	nlink1 = (int)st.st_nlink;
	limit  = nlink1 + cnt;
	name  = voluta_t_new_name_unique(t_ctx);
	for (i = nlink1; i < limit; ++i) {
		path2 = voluta_t_new_pathf(t_ctx, path0, "%s-%d", name, i);
		path3 = voluta_t_new_pathf(t_ctx, path0, "%s-X-%d", name, i);
		voluta_t_link(path1, path2);
		voluta_t_rename(path2, path3);
		voluta_t_unlink_noent(path2);
	}
	for (i = limit - 1; i >= nlink1; --i) {
		path3 = voluta_t_new_pathf(t_ctx, path0, "%s-X-%d", name, i);
		voluta_t_unlink(path3);
	}
	voluta_t_close(fd);
	voluta_t_unlink(path1);
	voluta_t_rmdir(path0);
}

static void test_link_rename(struct voluta_t_ctx *t_ctx)
{
	test_link_rename_cnt(t_ctx, 1);
	test_link_rename_cnt(t_ctx, 2);
	test_link_rename_cnt(t_ctx, 300);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects link(3p) to succeed for link count less then LINK_MAX.
 */
static void test_link_max(struct voluta_t_ctx *t_ctx)
{
	int fd;
	nlink_t nlink;
	struct stat st;
	const char *name  = voluta_t_new_name_unique(t_ctx);
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_under(t_ctx, path0);
	const char *path2 = NULL;
	const size_t link_max = get_link_max();

	voluta_t_mkdir(path0, 0700);
	voluta_t_creat(path1, 0600, &fd);
	voluta_t_fstat(fd, &st);
	nlink = st.st_nlink;
	for (size_t i = nlink; i < link_max; ++i) {
		path2 = voluta_t_new_pathf(t_ctx, path0, "%s-%lu", name, i);
		voluta_t_link(path1, path2);
	}
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_nlink, link_max);
	for (size_t j = nlink; j < link_max; ++j) {
		path2 = voluta_t_new_pathf(t_ctx, path0, "%s-%lu", name, j);
		voluta_t_unlink(path2);
	}
	voluta_t_close(fd);
	voluta_t_unlink(path1);
	voluta_t_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects link(3p) to not return EMLINK if the link count of the file is less
 * then LINK_MAX.
 */
static void test_link_limit(struct voluta_t_ctx *t_ctx)
{
	int fd;
	size_t i;
	nlink_t nlink;
	struct stat st;
	const char *name = voluta_t_new_name_unique(t_ctx);
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_under(t_ctx, path0);
	const char *path2 = NULL;
	const size_t link_max = get_link_max();

	voluta_t_mkdir(path0, 0750);
	voluta_t_creat(path1, 0640, &fd);
	voluta_t_fstat(fd, &st);
	nlink = st.st_nlink;
	for (i = nlink; i < link_max; ++i) {
		path2 = voluta_t_new_pathf(t_ctx, path0, "%d-%s", i, name);
		voluta_t_link(path1, path2);
	}
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_nlink, link_max);
	for (i = nlink; i < link_max; i += 2) {
		path2 = voluta_t_new_pathf(t_ctx, path0, "%d-%s", i, name);
		voluta_t_unlink(path2);
	}
	for (i = (nlink + 1); i < link_max; i += 2) {
		path2 = voluta_t_new_pathf(t_ctx, path0, "%d-%s", i, name);
		voluta_t_unlink(path2);
	}
	voluta_t_unlink(path1);
	voluta_t_rmdir(path0);
	voluta_t_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects link(3p) to succeed for names which may cause avalanche effect on
 * poor hash-based directories.
 */
static const char *make_name(struct voluta_t_ctx *t_ctx, char c,
			     size_t len)
{
	size_t nlen;
	char name[VOLUTA_NAME_MAX + 1] = "";

	nlen = (len < sizeof(name)) ? len : (sizeof(name) - 1);
	memset(name, c, nlen);
	return voluta_t_strdup(t_ctx, name);
}

static void test_link_similar_names(struct voluta_t_ctx *t_ctx)
{
	int fd;
	size_t i, j, name_max = VOLUTA_NAME_MAX;
	struct stat st;
	const char *name;
	const char *lpath;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *rpath = voluta_t_new_path_under(t_ctx, path0);
	const char *abc =
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	const size_t abc_len = strlen(abc);

	voluta_t_mkdir(path0, 0750);
	voluta_t_creat(rpath, 0640, &fd);
	for (i = 0; i < abc_len; ++i) {
		for (j = 1; j <= name_max; ++j) {
			name = make_name(t_ctx, abc[i], j);
			lpath = voluta_t_new_path_nested(t_ctx, path0, name);
			voluta_t_link(rpath, lpath);
		}
	}
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_nlink, (name_max * abc_len) + 1);

	for (i = 0; i < abc_len; ++i) {
		for (j = 1; j <= name_max; ++j) {
			name = make_name(t_ctx, abc[i], j);
			lpath = voluta_t_new_path_nested(t_ctx, path0, name);
			voluta_t_unlink(lpath);
		}
	}

	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_nlink, 1);
	voluta_t_close(fd);
	voluta_t_unlink(rpath);
	voluta_t_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_link_exists),
	VOLUTA_T_DEFTEST(test_link_noent),
	VOLUTA_T_DEFTEST(test_link_notdir),
	VOLUTA_T_DEFTEST(test_link_rename),
	VOLUTA_T_DEFTEST(test_link_max),
	VOLUTA_T_DEFTEST(test_link_limit),
	VOLUTA_T_DEFTEST(test_link_similar_names),
};

const struct voluta_t_tests
voluta_t_test_link = VOLUTA_T_DEFTESTS(t_local_tests);
