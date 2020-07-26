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
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "vfstest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects symlink(3p) to successfully create symbolic-links.
 */
static void test_symlink_simple(struct vt_env *vt_env)
{
	int fd;
	mode_t ifmt = S_IFMT;
	struct stat st0, st1;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_unique(vt_env);

	vt_creat(path0, 0600, &fd);
	vt_stat(path0, &st0);
	vt_expect_true(S_ISREG(st0.st_mode));
	vt_expect_eq((st0.st_mode & ~ifmt), 0600);

	vt_symlink(path0, path1);
	vt_stat(path1, &st1);
	vt_expect_eq(st0.st_ino, st1.st_ino);
	vt_lstat(path1, &st1);
	vt_expect_ne(st0.st_ino, st1.st_ino);
	vt_expect_true(S_ISLNK(st1.st_mode));
	vt_expect_eq((st1.st_mode & ~ifmt), 0777);
	vt_unlink(path1);
	vt_stat_noent(path1);
	vt_unlink(path0);
	vt_stat_noent(path0);
	vt_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects readlink(3p) to successfully read symbolic-links and return EINVAL
 * if the path argument names a file that is not a symbolic link.
 */
static void test_symlink_readlink(struct vt_env *vt_env)
{
	int fd;
	size_t nch, bsz = VOLUTA_PATH_MAX;
	struct stat st1;
	char *buf, buf1[2] = "";
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_unique(vt_env);
	const char *path2 = vt_new_path_unique(vt_env);

	vt_creat(path0, 0600, &fd);
	vt_symlink(path0, path1);
	vt_lstat(path1, &st1);
	vt_expect_true(S_ISLNK(st1.st_mode));
	vt_expect_eq(st1.st_size, strlen(path0));

	buf = vt_new_buf_zeros(vt_env, bsz);
	vt_readlink(path1, buf, bsz, &nch);
	vt_expect_eq(nch, strlen(path0));
	vt_expect_eq(strncmp(buf, path0, nch), 0);
	vt_readlink_err(path0, buf, bsz, -EINVAL);
	vt_readlink_err(path2, buf, bsz, -ENOENT);

	vt_readlink(path1, buf1, 1, &nch);
	vt_expect_eq(nch, 1);
	vt_expect_eq(buf1[0], path0[0]);

	vt_unlink(path1);
	vt_unlink(path0);
	vt_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects readlink(3p) to update symlink access-time.
 */
static void test_symlink_readlink_atime(struct vt_env *vt_env)
{
	size_t nch, bsz = VOLUTA_PATH_MAX;
	struct stat st;
	time_t atime1, atime2;
	char *buf;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_unique(vt_env);

	buf = vt_new_buf_zeros(vt_env, bsz);
	vt_mkdir(path0, 0700);
	vt_symlink(path0, path1);
	vt_lstat(path1, &st);
	vt_expect_true(S_ISLNK(st.st_mode));
	vt_expect_eq(st.st_size, strlen(path0));

	atime1 = st.st_atim.tv_sec;
	vt_readlink(path1, buf, bsz, &nch);
	vt_lstat(path1, &st);
	atime2 = st.st_atim.tv_sec;
	vt_expect_eqm(buf, path0, nch);
	vt_expect_le(atime1, atime2);
	vt_suspend(vt_env, 3, 2);
	vt_readlink(path1, buf, bsz, &nch);
	vt_lstat(path1, &st);
	atime2 = st.st_atim.tv_sec;
	vt_expect_eqm(buf, path0, nch);
	vt_expect_le(atime1, atime2); /* XXX _lt */

	vt_unlink(path1);
	vt_rmdir(path0);
	vt_lstat_err(path1, -ENOENT);
	vt_stat_err(path0, -ENOENT);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects symlink(3p) to successfully create symbolic-links in various length.
 */
static char *vt_new_path_dummy(struct vt_env *vt_env,
			       size_t len)
{
	size_t cnt, lim = (2 * len);
	char *path, *name;

	name = vt_new_name_unique(vt_env);
	path = vt_new_buf_zeros(vt_env, lim + 1);
	while ((cnt = strlen(path)) < len) {
		snprintf(path + cnt, lim - cnt, "/%s", name);
	}
	path[len] = '\0';
	return path;
}

static void test_symlink_anylen_(struct vt_env *vt_env, size_t len)
{
	size_t nch;
	mode_t ifmt = S_IFMT;
	struct stat st;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_dummy(vt_env, len);
	char *lnkbuf = vt_new_buf_zeros(vt_env, len + 1);

	vt_symlink(path1, path0);
	vt_lstat(path0, &st);
	vt_expect_true(S_ISLNK(st.st_mode));
	vt_expect_eq((st.st_mode & ~ifmt), 0777);
	vt_readlink(path0, lnkbuf, len, &nch);
	vt_expect_eq(len, nch);
	vt_expect_eq(strncmp(path1, lnkbuf, len), 0);
	vt_unlink(path0);
}

static void test_symlink_anylen(struct vt_env *vt_env)
{
	const size_t symval_len_max = VOLUTA_SYMLNK_MAX;

	for (size_t i = 1; i < symval_len_max; ++i) {
		test_symlink_anylen_(vt_env, i);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects symlink(3p) to successfully create symbolic-links in various length
 * while mixed with I/O operations.
 */
static void fill_name(char *name, size_t lim, size_t idx)
{
	snprintf(name, lim, "%061lx", idx);
}

static void test_symlink_with_io(struct vt_env *vt_env)
{
	int dfd, fd;
	size_t i, nch, lim = VOLUTA_SYMLNK_MAX;
	loff_t off;
	mode_t ifmt = S_IFMT;
	struct stat st;
	char name[VOLUTA_NAME_MAX + 1] = "";
	char *symval, *buf = vt_new_buf_zeros(vt_env, 2 * lim);
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_under(vt_env, path0);

	vt_mkdir(path0, 0700);
	vt_open(path0, O_DIRECTORY | O_RDONLY, 0, &dfd);
	vt_open(path1, O_CREAT | O_RDWR, 0600, &fd);
	for (i = 1; i < lim; ++i) {
		fill_name(name, sizeof(name), i);
		symval = vt_new_path_dummy(vt_env, i);
		vt_symlinkat(symval, dfd, name);
		vt_fstatat(dfd, name, &st, AT_SYMLINK_NOFOLLOW);
		vt_expect_lnk(st.st_mode);
		off = (loff_t)(i * lim);
		vt_pwriten(fd, symval, i, off);
	}
	for (i = 1; i < lim; ++i) {
		fill_name(name, sizeof(name), i);
		vt_fstatat(dfd, name, &st, AT_SYMLINK_NOFOLLOW);
		vt_expect_lnk(st.st_mode);
		symval = vt_new_path_dummy(vt_env, i + 1);
		vt_readlinkat(dfd, name, symval, i + 1, &nch);
		vt_expect_eq(nch, i);
		off = (loff_t)(i * lim);
		vt_preadn(fd, buf, i, off);
		vt_expect_eqm(buf, symval, i);
	}
	for (i = 1; i < lim; ++i) {
		fill_name(name, sizeof(name), i);
		vt_fstatat(dfd, name, &st, AT_SYMLINK_NOFOLLOW);
		vt_expect_lnk(st.st_mode);
		vt_expect_eq((st.st_mode & ~ifmt), 0777);
		vt_unlinkat(dfd, name, 0);
		vt_fstatat_err(dfd, name, 0, -ENOENT);
	}
	vt_close(fd);
	vt_unlink(path1);
	vt_close(dfd);
	vt_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTEST(test_symlink_simple),
	VT_DEFTEST(test_symlink_readlink),
	VT_DEFTEST(test_symlink_readlink_atime),
	VT_DEFTEST(test_symlink_anylen),
	VT_DEFTEST(test_symlink_with_io),
};

const struct vt_tests vt_test_symlink = VT_DEFTESTS(vt_local_tests);
