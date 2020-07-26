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
#include <errno.h>
#include "vfstest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful open(3p) with O_CREAT to set the file's access time
 */
static void test_open_atime(struct vt_env *vt_env)
{
	int fd;
	struct stat st0, st1;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_under(vt_env, path0);

	vt_mkdir(path0, 0755);
	vt_stat(path0, &st0);
	vt_suspend(vt_env, 3, 1);
	vt_open(path1, O_CREAT | O_WRONLY, 0644, &fd);
	vt_fstat(fd, &st1);
	vt_expect_true(st0.st_atim.tv_sec < st1.st_atim.tv_sec);
	vt_unlink(path1);
	vt_rmdir(path0);
	vt_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful open(3p) with O_CREAT to update parent's ctime and mtime
 * only if file did *not* exist.
 */
static void test_open_mctime(struct vt_env *vt_env)
{
	int fd1, fd2;
	struct stat st0, st1, st2, st3;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_under(vt_env, path0);

	vt_mkdir(path0, 0755);
	vt_stat(path0, &st0);
	vt_suspend(vt_env, 3, 2);
	vt_open(path1, O_CREAT | O_WRONLY, 0644, &fd1);
	vt_fstat(fd1, &st1);
	vt_expect_lt(st0.st_mtim.tv_sec, st1.st_mtim.tv_sec);
	vt_expect_lt(st0.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	vt_stat(path0, &st2);
	vt_expect_lt(st0.st_mtim.tv_sec, st2.st_mtim.tv_sec);
	vt_expect_lt(st0.st_ctim.tv_sec, st2.st_ctim.tv_sec);
	vt_unlink(path1);
	vt_close(fd1);

	vt_creat(path1, 0644, &fd1);
	vt_fstat(fd1, &st1);
	vt_stat(path0, &st0);
	vt_suspend(vt_env, 3, 2);
	vt_open(path1, O_CREAT | O_RDONLY, 0644, &fd2);
	vt_fstat(fd2, &st2);
	vt_stat(path0, &st3);
	vt_expect_eq(st1.st_mtim.tv_sec, st2.st_mtim.tv_sec);
	vt_expect_eq(st1.st_ctim.tv_sec, st2.st_ctim.tv_sec);
	vt_expect_eq(st0.st_mtim.tv_sec, st3.st_mtim.tv_sec);
	vt_expect_eq(st0.st_ctim.tv_sec, st3.st_ctim.tv_sec);

	vt_unlink(path1);
	vt_rmdir(path0);
	vt_close(fd1);
	vt_close(fd2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects open(3p) to return ELOOP if too many symbolic links are encountered
 * while resolving pathname, or O_NOFOLLOW was specified but pathname was a
 * symbolic link.
 */
static void test_open_loop(struct vt_env *vt_env)
{
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_unique(vt_env);
	const char *path2 = vt_new_path_under(vt_env, path0);
	const char *path3 = vt_new_path_under(vt_env, path1);

	vt_symlink(path0, path1);
	vt_symlink(path1, path0);
	vt_open_err(path2, O_RDONLY, 0, -ELOOP);
	vt_open_err(path3, O_RDONLY, 0, -ELOOP);
	vt_unlink(path0);
	vt_unlink(path1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects open(3p) to return EISDIR if the  named file is a directory and
 * oflag includes O_WRONLY or O_RDWR.
 */
static void test_open_isdir(struct vt_env *vt_env)
{
	int fd;
	const char *path = vt_new_path_unique(vt_env);

	vt_mkdir(path, 0755);
	vt_open(path, O_RDONLY, 0, &fd);
	vt_open_err(path, O_WRONLY, 0, -EISDIR);
	vt_open_err(path, O_RDWR, 0, -EISDIR);
	vt_open_err(path, O_RDONLY | O_TRUNC, 0, -EISDIR);
	vt_open_err(path, O_WRONLY | O_TRUNC, 0, -EISDIR);
	vt_open_err(path, O_RDWR | O_TRUNC, 0, -EISDIR);
	vt_close(fd);
	vt_rmdir(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects open(2) to return valid file descriptor with O_TMPFILE, which must
 * not be associated by name with any directory.
 */
static void test_open_tmpfile(struct vt_env *vt_env)
{
	int fd, flags = O_RDWR | O_TMPFILE | O_EXCL;
	const char *path1 = vt_new_path_unique(vt_env);
	const char *path2 = vt_new_path_under(vt_env, path1);

	vt_mkdir(path1, 0700);
	vt_open_err(path2, flags, 0600, -ENOENT);
	vt_open(path1, flags, 0600, &fd);
	vt_access_err(path2, R_OK, -ENOENT);
	vt_rmdir(path1);
	vt_access_err(path1, R_OK, -ENOENT);
	vt_access_err(path2, R_OK, -ENOENT);
	vt_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTEST(test_open_atime),
	VT_DEFTEST(test_open_mctime),
	VT_DEFTEST(test_open_loop),
	VT_DEFTEST(test_open_isdir),
	VT_DEFTESTF(test_open_tmpfile, VT_POSIX_EXTRA),
};

const struct vt_tests vt_test_open = VT_DEFTESTS(vt_local_tests);
