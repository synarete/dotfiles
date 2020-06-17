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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "fsstest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects symlink(3p) to successfully create symbolic-links.
 */
static void test_symlink_simple(struct voluta_t_ctx *t_ctx)
{
	int fd;
	mode_t ifmt = S_IFMT;
	struct stat st0, st1;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path0, 0600, &fd);
	voluta_t_stat(path0, &st0);
	voluta_t_expect_true(S_ISREG(st0.st_mode));
	voluta_t_expect_eq((st0.st_mode & ~ifmt), 0600);

	voluta_t_symlink(path0, path1);
	voluta_t_stat(path1, &st1);
	voluta_t_expect_eq(st0.st_ino, st1.st_ino);
	voluta_t_lstat(path1, &st1);
	voluta_t_expect_ne(st0.st_ino, st1.st_ino);
	voluta_t_expect_true(S_ISLNK(st1.st_mode));
	voluta_t_expect_eq((st1.st_mode & ~ifmt), 0777);
	voluta_t_unlink(path1);
	voluta_t_stat_noent(path1);
	voluta_t_unlink(path0);
	voluta_t_stat_noent(path0);
	voluta_t_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects readlink(3p) to successfully read symbolic-links and return EINVAL
 * if the path argument names a file that is not a symbolic link.
 */
static void test_symlink_readlink(struct voluta_t_ctx *t_ctx)
{
	int fd;
	size_t nch, bsz = VOLUTA_PATH_MAX;
	struct stat st1;
	char *buf, buf1[2] = "";
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_unique(t_ctx);
	const char *path2 = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path0, 0600, &fd);
	voluta_t_symlink(path0, path1);
	voluta_t_lstat(path1, &st1);
	voluta_t_expect_true(S_ISLNK(st1.st_mode));
	voluta_t_expect_eq(st1.st_size, strlen(path0));

	buf = voluta_t_new_buf_zeros(t_ctx, bsz);
	voluta_t_readlink(path1, buf, bsz, &nch);
	voluta_t_expect_eq(nch, strlen(path0));
	voluta_t_expect_eq(strncmp(buf, path0, nch), 0);
	voluta_t_readlink_err(path0, buf, bsz, -EINVAL);
	voluta_t_readlink_err(path2, buf, bsz, -ENOENT);

	voluta_t_readlink(path1, buf1, 1, &nch);
	voluta_t_expect_eq(nch, 1);
	voluta_t_expect_eq(buf1[0], path0[0]);

	voluta_t_unlink(path1);
	voluta_t_unlink(path0);
	voluta_t_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects readlink(3p) to update symlink access-time.
 */
static void test_symlink_readlink_atime(struct voluta_t_ctx *t_ctx)
{
	size_t nch, bsz = VOLUTA_PATH_MAX;
	struct stat st;
	time_t atime1, atime2;
	char *buf;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_unique(t_ctx);

	buf = voluta_t_new_buf_zeros(t_ctx, bsz);
	voluta_t_mkdir(path0, 0700);
	voluta_t_symlink(path0, path1);
	voluta_t_lstat(path1, &st);
	voluta_t_expect_true(S_ISLNK(st.st_mode));
	voluta_t_expect_eq(st.st_size, strlen(path0));

	atime1 = st.st_atim.tv_sec;
	voluta_t_readlink(path1, buf, bsz, &nch);
	voluta_t_lstat(path1, &st);
	atime2 = st.st_atim.tv_sec;
	voluta_t_expect_eqm(buf, path0, nch);
	voluta_t_expect_le(atime1, atime2);
	voluta_t_suspend(t_ctx, 3, 2);
	voluta_t_readlink(path1, buf, bsz, &nch);
	voluta_t_lstat(path1, &st);
	atime2 = st.st_atim.tv_sec;
	voluta_t_expect_eqm(buf, path0, nch);
	voluta_t_expect_le(atime1, atime2); /* XXX _lt */

	voluta_t_unlink(path1);
	voluta_t_rmdir(path0);
	voluta_t_lstat_err(path1, -ENOENT);
	voluta_t_stat_err(path0, -ENOENT);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects symlink(3p) to successfully create symbolic-links in various length.
 */
static char *voluta_t_new_path_dummy(struct voluta_t_ctx *t_ctx,
				     size_t len)
{
	size_t cnt, lim = (2 * len);
	char *path, *name;

	name = voluta_t_new_name_unique(t_ctx);
	path = voluta_t_new_buf_zeros(t_ctx, lim + 1);
	while ((cnt = strlen(path)) < len) {
		snprintf(path + cnt, lim - cnt, "/%s", name);
	}
	path[len] = '\0';
	return path;
}

static void test_symlink_anylen_(struct voluta_t_ctx *t_ctx, size_t len)
{
	size_t nch;
	mode_t ifmt = S_IFMT;
	struct stat st;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_dummy(t_ctx, len);
	char *lnkbuf = voluta_t_new_buf_zeros(t_ctx, len + 1);

	voluta_t_symlink(path1, path0);
	voluta_t_lstat(path0, &st);
	voluta_t_expect_true(S_ISLNK(st.st_mode));
	voluta_t_expect_eq((st.st_mode & ~ifmt), 0777);
	voluta_t_readlink(path0, lnkbuf, len, &nch);
	voluta_t_expect_eq(len, nch);
	voluta_t_expect_eq(strncmp(path1, lnkbuf, len), 0);
	voluta_t_unlink(path0);
}

static void test_symlink_anylen(struct voluta_t_ctx *t_ctx)
{
	const size_t symval_len_max = VOLUTA_SYMLNK_MAX;

	for (size_t i = 1; i < symval_len_max; ++i) {
		test_symlink_anylen_(t_ctx, i);
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

static void test_symlink_with_io(struct voluta_t_ctx *t_ctx)
{
	int dfd, fd;
	size_t i, nch, lim = VOLUTA_SYMLNK_MAX;
	loff_t off;
	mode_t ifmt = S_IFMT;
	struct stat st;
	char name[VOLUTA_NAME_MAX + 1] = "";
	char *symval, *buf = voluta_t_new_buf_zeros(t_ctx, 2 * lim);
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_under(t_ctx, path0);

	voluta_t_mkdir(path0, 0700);
	voluta_t_open(path0, O_DIRECTORY | O_RDONLY, 0, &dfd);
	voluta_t_open(path1, O_CREAT | O_RDWR, 0600, &fd);
	for (i = 1; i < lim; ++i) {
		fill_name(name, sizeof(name), i);
		symval = voluta_t_new_path_dummy(t_ctx, i);
		voluta_t_symlinkat(symval, dfd, name);
		voluta_t_fstatat(dfd, name, &st, AT_SYMLINK_NOFOLLOW);
		voluta_t_expect_lnk(st.st_mode);
		off = (loff_t)(i * lim);
		voluta_t_pwriten(fd, symval, i, off);
	}
	for (i = 1; i < lim; ++i) {
		fill_name(name, sizeof(name), i);
		voluta_t_fstatat(dfd, name, &st, AT_SYMLINK_NOFOLLOW);
		voluta_t_expect_lnk(st.st_mode);
		symval = voluta_t_new_path_dummy(t_ctx, i + 1);
		voluta_t_readlinkat(dfd, name, symval, i + 1, &nch);
		voluta_t_expect_eq(nch, i);
		off = (loff_t)(i * lim);
		voluta_t_preadn(fd, buf, i, off);
		voluta_t_expect_eqm(buf, symval, i);
	}
	for (i = 1; i < lim; ++i) {
		fill_name(name, sizeof(name), i);
		voluta_t_fstatat(dfd, name, &st, AT_SYMLINK_NOFOLLOW);
		voluta_t_expect_lnk(st.st_mode);
		voluta_t_expect_eq((st.st_mode & ~ifmt), 0777);
		voluta_t_unlinkat(dfd, name, 0);
		voluta_t_fstatat_err(dfd, name, 0, -ENOENT);
	}
	voluta_t_close(fd);
	voluta_t_unlink(path1);
	voluta_t_close(dfd);
	voluta_t_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_symlink_simple),
	VOLUTA_T_DEFTEST(test_symlink_readlink),
	VOLUTA_T_DEFTEST(test_symlink_readlink_atime),
	VOLUTA_T_DEFTEST(test_symlink_anylen),
	VOLUTA_T_DEFTEST(test_symlink_with_io),
};

const struct voluta_t_tests voluta_t_test_symlink =
	VOLUTA_T_DEFTESTS(t_local_tests);
