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
#include <errno.h>
#include "sanity.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful open(3p) with O_CREAT to set the file's access time
 */
static void test_open_atime(struct voluta_t_ctx *t_ctx)
{
	int fd;
	struct stat st0, st1;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_under(t_ctx, path0);

	voluta_t_mkdir(path0, 0755);
	voluta_t_stat(path0, &st0);
	voluta_t_suspend(t_ctx, 3, 1);
	voluta_t_open(path1, O_CREAT | O_WRONLY, 0644, &fd);
	voluta_t_fstat(fd, &st1);
	voluta_t_expect_true(st0.st_atim.tv_sec < st1.st_atim.tv_sec);
	voluta_t_unlink(path1);
	voluta_t_rmdir(path0);
	voluta_t_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful open(3p) with O_CREAT to update parent's ctime and mtime
 * only if file did *not* exist.
 */
static void test_open_mctime(struct voluta_t_ctx *t_ctx)
{
	int fd1, fd2;
	struct stat st0, st1, st2, st3;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_under(t_ctx, path0);

	voluta_t_mkdir(path0, 0755);
	voluta_t_stat(path0, &st0);
	voluta_t_suspend(t_ctx, 3, 2);
	voluta_t_open(path1, O_CREAT | O_WRONLY, 0644, &fd1);
	voluta_t_fstat(fd1, &st1);
	voluta_t_expect_lt(st0.st_mtim.tv_sec, st1.st_mtim.tv_sec);
	voluta_t_expect_lt(st0.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	voluta_t_stat(path0, &st2);
	voluta_t_expect_lt(st0.st_mtim.tv_sec, st2.st_mtim.tv_sec);
	voluta_t_expect_lt(st0.st_ctim.tv_sec, st2.st_ctim.tv_sec);
	voluta_t_unlink(path1);
	voluta_t_close(fd1);

	voluta_t_creat(path1, 0644, &fd1);
	voluta_t_fstat(fd1, &st1);
	voluta_t_stat(path0, &st0);
	voluta_t_suspend(t_ctx, 3, 2);
	voluta_t_open(path1, O_CREAT | O_RDONLY, 0644, &fd2);
	voluta_t_fstat(fd2, &st2);
	voluta_t_stat(path0, &st3);
	voluta_t_expect_eq(st1.st_mtim.tv_sec, st2.st_mtim.tv_sec);
	voluta_t_expect_eq(st1.st_ctim.tv_sec, st2.st_ctim.tv_sec);
	voluta_t_expect_eq(st0.st_mtim.tv_sec, st3.st_mtim.tv_sec);
	voluta_t_expect_eq(st0.st_ctim.tv_sec, st3.st_ctim.tv_sec);

	voluta_t_unlink(path1);
	voluta_t_rmdir(path0);
	voluta_t_close(fd1);
	voluta_t_close(fd2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects open(3p) to return ELOOP if too many symbolic links are encountered
 * while resolving pathname, or O_NOFOLLOW was specified but pathname was a
 * symbolic link.
 */
static void test_open_loop(struct voluta_t_ctx *t_ctx)
{
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_unique(t_ctx);
	const char *path2 = voluta_t_new_path_under(t_ctx, path0);
	const char *path3 = voluta_t_new_path_under(t_ctx, path1);

	voluta_t_symlink(path0, path1);
	voluta_t_symlink(path1, path0);
	voluta_t_open_err(path2, O_RDONLY, 0, -ELOOP);
	voluta_t_open_err(path3, O_RDONLY, 0, -ELOOP);
	voluta_t_unlink(path0);
	voluta_t_unlink(path1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects open(3p) to return EISDIR if the  named file is a directory and
 * oflag includes O_WRONLY or O_RDWR.
 */
static void test_open_isdir(struct voluta_t_ctx *t_ctx)
{
	int fd;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_mkdir(path, 0755);
	voluta_t_open(path, O_RDONLY, 0, &fd);
	voluta_t_open_err(path, O_WRONLY, 0, -EISDIR);
	voluta_t_open_err(path, O_RDWR, 0, -EISDIR);
	voluta_t_open_err(path, O_RDONLY | O_TRUNC, 0, -EISDIR);
	voluta_t_open_err(path, O_WRONLY | O_TRUNC, 0, -EISDIR);
	voluta_t_open_err(path, O_RDWR | O_TRUNC, 0, -EISDIR);
	voluta_t_close(fd);
	voluta_t_rmdir(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects open(2) to return valid file descriptor with O_TMPFILE, which must
 * not be associated by name with any directory.
 */
static void test_open_tmpfile(struct voluta_t_ctx *t_ctx)
{
	int fd, flags = O_RDWR | O_TMPFILE | O_EXCL;
	const char *path1 = voluta_t_new_path_unique(t_ctx);
	const char *path2 = voluta_t_new_path_under(t_ctx, path1);

	voluta_t_mkdir(path1, 0700);
	voluta_t_open_err(path2, flags, 0600, -ENOENT);
	voluta_t_open(path1, flags, 0600, &fd);
	voluta_t_access_err(path2, R_OK, -ENOENT);
	voluta_t_rmdir(path1);
	voluta_t_access_err(path1, R_OK, -ENOENT);
	voluta_t_access_err(path2, R_OK, -ENOENT);
	voluta_t_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_open_atime),
	VOLUTA_T_DEFTEST(test_open_mctime),
	VOLUTA_T_DEFTEST(test_open_loop),
	VOLUTA_T_DEFTEST(test_open_isdir),
	VOLUTA_T_DEFTESTF(test_open_tmpfile, VOLUTA_T_POSIX_EXTRA),
};

const struct voluta_t_tests
voluta_t_test_open = VOLUTA_T_DEFTESTS(t_local_tests);
