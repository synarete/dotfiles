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
#include <fcntl.h>
#include <errno.h>
#include "fstests.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful unlink(3p) of directory entry.
 */
static void test_unlink_reg(struct voluta_t_ctx *t_ctx)
{
	int fd;
	struct stat st;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0700, &fd);
	voluta_t_close(fd);
	voluta_t_lstat(path, &st);
	voluta_t_expect_true(S_ISREG(st.st_mode));
	voluta_t_unlink(path);
	voluta_t_unlink_noent(path);
	voluta_t_lstat_err(path, -ENOENT);
}

static void test_unlink_symlink(struct voluta_t_ctx *t_ctx)
{
	int fd;
	struct stat st;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path0, 0600, &fd);
	voluta_t_close(fd);
	voluta_t_symlink(path0, path1);
	voluta_t_lstat(path1, &st);
	voluta_t_expect_true(S_ISLNK(st.st_mode));
	voluta_t_unlink(path1);
	voluta_t_unlink_noent(path1);
	voluta_t_unlink(path0);
	voluta_t_unlink_noent(path0);
}

static void test_unlink_fifo(struct voluta_t_ctx *t_ctx)
{
	struct stat st;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_mkfifo(path, 0644);
	voluta_t_lstat(path, &st);
	voluta_t_expect_true(S_ISFIFO(st.st_mode));
	voluta_t_unlink(path);
	voluta_t_unlink_noent(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects unlink(3p) to return -ENOTDIR if a component of the path prefix
 * is not a directory.
 */
static void test_unlink_notdir(struct voluta_t_ctx *t_ctx)
{
	int fd;
	struct stat st;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_under(t_ctx, path0);
	const char *path2 = voluta_t_new_path_under(t_ctx, path1);

	voluta_t_mkdir(path0, 0755);
	voluta_t_stat(path0, &st);
	voluta_t_open(path1, O_CREAT | O_RDWR, 0700, &fd);
	voluta_t_close(fd);
	voluta_t_unlink_err(path2, -ENOTDIR);
	voluta_t_unlink(path1);
	voluta_t_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects unlink(3p) to return -EISDIR if target is a directory
 */
static void test_unlink_isdir(struct voluta_t_ctx *t_ctx)
{
	struct stat st;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_mkdir(path, 0700);
	voluta_t_stat(path, &st);
	voluta_t_expect_true(S_ISDIR(st.st_mode));
	voluta_t_unlink_err(path, -EISDIR);
	voluta_t_rmdir(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects unlinkat(3p) to recreate files with same name when previous one with
 * same-name has been unlinked but still open.
 */
static void test_unlinkat_same_name(struct voluta_t_ctx *t_ctx)
{
	size_t nwr, nrd, nfds = 0;
	int dfd, fd, fds[64];
	struct stat st;
	const char *path = voluta_t_new_path_unique(t_ctx);
	const char *name = voluta_t_new_name_unique(t_ctx);

	voluta_t_mkdir(path, 0700);
	voluta_t_open(path, O_DIRECTORY | O_RDONLY, 0, &dfd);
	for (size_t i = 0; i < VOLUTA_ARRAY_SIZE(fds); ++i) {
		voluta_t_openat(dfd, name, O_CREAT | O_RDWR, 0600, &fd);
		voluta_t_unlinkat(dfd, name, 0);
		voluta_t_pwrite(fd, &fd, sizeof(fd), fd, &nwr);
		voluta_t_fstat(dfd, &st);
		voluta_t_expect_eq(st.st_nlink, 2);
		fds[nfds++] = fd;
	}
	for (size_t j = 0; j < VOLUTA_ARRAY_SIZE(fds); ++j) {
		fd = fds[j];
		voluta_t_pread(fd, &fd, sizeof(fd), fd, &nrd);
		voluta_t_expect_eq(fd, fds[j]);
		voluta_t_fstat(fd, &st);
		voluta_t_close(fd);
	}
	voluta_t_close(dfd);
	voluta_t_rmdir(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_unlink_reg),
	VOLUTA_T_DEFTEST(test_unlink_symlink),
	VOLUTA_T_DEFTEST(test_unlink_fifo),
	VOLUTA_T_DEFTEST(test_unlink_notdir),
	VOLUTA_T_DEFTEST(test_unlink_isdir),
	VOLUTA_T_DEFTEST(test_unlinkat_same_name),
};

const struct voluta_t_tests
voluta_t_test_unlink = VOLUTA_T_DEFTESTS(t_local_tests);


