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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include "vfstest.h"


/* Maximum hard-links per file */
static size_t get_link_max(void)
{
	long ret;
	const long lim = 2048;

	ret = sysconf(_PC_LINK_MAX);
	vt_expect_gt(ret, 0);
	vt_expect_lt(ret, VT_UGIGA);

	return (size_t)((ret < lim) ? ret : lim);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects link(3p) to return EEXIST if the path2 argument resolves to an
 * existing file or refers to a symbolic link.
 */
static void test_link_exists(struct vt_env *vt_env)
{
	int fd0, fd1;
	char *path0, *path1, *path2;

	path0 = vt_new_path_unique(vt_env);
	path1 = vt_new_path_unique(vt_env);
	path2 = vt_new_path_name(vt_env, "test-link-to-symlink-exist");

	vt_creat(path0, 0644, &fd0);
	vt_creat(path1, 0644, &fd1);

	vt_link_err(path0, path1, -EEXIST);
	vt_unlink(path1);

	vt_mkdir(path1, 0755);
	vt_link_err(path0, path1, -EEXIST);
	vt_rmdir(path1);

	vt_symlink(path1, path2);
	vt_link_err(path0, path2, -EEXIST);
	vt_unlink(path2);

	vt_mkfifo(path1, 0644);
	vt_link_err(path0, path1, -EEXIST);
	vt_unlink(path1);

	vt_unlink(path0);
	vt_close(fd0);
	vt_close(fd1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects link(3p) to return ENOENT if the source file does not exist.
 */
static void test_link_noent(struct vt_env *vt_env)
{
	int fd;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_under(vt_env, path0);
	const char *path2 = vt_new_path_under(vt_env, path0);

	vt_mkdir(path0, 0700);
	vt_creat(path1, 0640, &fd);
	vt_link(path1, path2);
	vt_unlink(path1);
	vt_unlink(path2);
	vt_link_err(path1, path2, -ENOENT);
	vt_rmdir(path0);
	vt_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects link(3p) to return EEXIST if a component of either path prefix is
 * not a directory.
 */
static void test_link_notdir(struct vt_env *vt_env)
{
	int fd1, fd2;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_under(vt_env, path0);
	const char *path2 = vt_new_path_under(vt_env, path0);
	const char *path3 = vt_new_path_under(vt_env, path1);

	vt_mkdir(path0, 0755);
	vt_creat(path1, 0644, &fd1);
	vt_link_err(path3, path2, -ENOTDIR);
	vt_creat(path2, 0644, &fd2);
	vt_link_err(path2, path3, -ENOTDIR);
	vt_unlink(path1);
	vt_unlink(path2);
	vt_rmdir(path0);
	vt_close(fd1);
	vt_close(fd2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects link(3p)/unlink(3p) sequence to succeed for renamed links.
 */
static void test_link_rename_cnt(struct vt_env *vt_env, int cnt)
{
	int i, fd, nlink1, limit;
	struct stat st;
	char *name, *path2, *path3;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_under(vt_env, path0);

	vt_mkdir(path0, 0700);
	vt_creat(path1, 0600, &fd);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_nlink, 1);
	nlink1 = (int)st.st_nlink;
	limit  = nlink1 + cnt;
	name  = vt_new_name_unique(vt_env);
	for (i = nlink1; i < limit; ++i) {
		path2 = vt_new_pathf(vt_env, path0, "%s-%d", name, i);
		path3 = vt_new_pathf(vt_env, path0, "%s-X-%d", name, i);
		vt_link(path1, path2);
		vt_rename(path2, path3);
		vt_unlink_noent(path2);
	}
	for (i = limit - 1; i >= nlink1; --i) {
		path3 = vt_new_pathf(vt_env, path0, "%s-X-%d", name, i);
		vt_unlink(path3);
	}
	vt_close(fd);
	vt_unlink(path1);
	vt_rmdir(path0);
}

static void test_link_rename(struct vt_env *vt_env)
{
	test_link_rename_cnt(vt_env, 1);
	test_link_rename_cnt(vt_env, 2);
	test_link_rename_cnt(vt_env, 300);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects link(3p) to succeed for link count less then LINK_MAX.
 */
static void test_link_max(struct vt_env *vt_env)
{
	int fd;
	nlink_t nlink;
	struct stat st;
	const char *name  = vt_new_name_unique(vt_env);
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_under(vt_env, path0);
	const char *path2 = NULL;
	const size_t link_max = get_link_max();

	vt_mkdir(path0, 0700);
	vt_creat(path1, 0600, &fd);
	vt_fstat(fd, &st);
	nlink = st.st_nlink;
	for (size_t i = nlink; i < link_max; ++i) {
		path2 = vt_new_pathf(vt_env, path0, "%s-%lu", name, i);
		vt_link(path1, path2);
	}
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_nlink, link_max);
	for (size_t j = nlink; j < link_max; ++j) {
		path2 = vt_new_pathf(vt_env, path0, "%s-%lu", name, j);
		vt_unlink(path2);
	}
	vt_close(fd);
	vt_unlink(path1);
	vt_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects link(3p) to not return EMLINK if the link count of the file is less
 * then LINK_MAX.
 */
static void test_link_limit(struct vt_env *vt_env)
{
	int fd;
	size_t i;
	nlink_t nlink;
	struct stat st;
	const char *name = vt_new_name_unique(vt_env);
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_under(vt_env, path0);
	const char *path2 = NULL;
	const size_t link_max = get_link_max();

	vt_mkdir(path0, 0750);
	vt_creat(path1, 0640, &fd);
	vt_fstat(fd, &st);
	nlink = st.st_nlink;
	for (i = nlink; i < link_max; ++i) {
		path2 = vt_new_pathf(vt_env, path0, "%d-%s", i, name);
		vt_link(path1, path2);
	}
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_nlink, link_max);
	for (i = nlink; i < link_max; i += 2) {
		path2 = vt_new_pathf(vt_env, path0, "%d-%s", i, name);
		vt_unlink(path2);
	}
	for (i = (nlink + 1); i < link_max; i += 2) {
		path2 = vt_new_pathf(vt_env, path0, "%d-%s", i, name);
		vt_unlink(path2);
	}
	vt_unlink(path1);
	vt_rmdir(path0);
	vt_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects link(3p) to succeed for names which may cause avalanche effect on
 * poor hash-based directories.
 */
static const char *make_name(struct vt_env *vt_env, char c,
			     size_t len)
{
	size_t nlen;
	char name[VOLUTA_NAME_MAX + 1] = "";

	nlen = (len < sizeof(name)) ? len : (sizeof(name) - 1);
	memset(name, c, nlen);
	return vt_strdup(vt_env, name);
}

static void test_link_similar_names(struct vt_env *vt_env)
{
	int fd;
	size_t i, j, name_max = VOLUTA_NAME_MAX;
	struct stat st;
	const char *name;
	const char *lpath;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *rpath = vt_new_path_under(vt_env, path0);
	const char *abc =
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	const size_t abc_len = strlen(abc);

	vt_mkdir(path0, 0750);
	vt_creat(rpath, 0640, &fd);
	for (i = 0; i < abc_len; ++i) {
		for (j = 1; j <= name_max; ++j) {
			name = make_name(vt_env, abc[i], j);
			lpath = vt_new_path_nested(vt_env, path0, name);
			vt_link(rpath, lpath);
		}
	}
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_nlink, (name_max * abc_len) + 1);

	for (i = 0; i < abc_len; ++i) {
		for (j = 1; j <= name_max; ++j) {
			name = make_name(vt_env, abc[i], j);
			lpath = vt_new_path_nested(vt_env, path0, name);
			vt_unlink(lpath);
		}
	}

	vt_fstat(fd, &st);
	vt_expect_eq(st.st_nlink, 1);
	vt_close(fd);
	vt_unlink(rpath);
	vt_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTEST(test_link_exists),
	VT_DEFTEST(test_link_noent),
	VT_DEFTEST(test_link_notdir),
	VT_DEFTEST(test_link_rename),
	VT_DEFTEST(test_link_max),
	VT_DEFTEST(test_link_limit),
	VT_DEFTEST(test_link_similar_names),
};

const struct vt_tests vt_test_link = VT_DEFTESTS(vt_local_tests);
