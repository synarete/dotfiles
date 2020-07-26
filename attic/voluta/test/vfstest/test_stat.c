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
#include <sys/statvfs.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "vfstest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects stat(3p) to successfully probe directory and return ENOENT if a
 * component of path does not name an existing file or path is an empty string.
 */
static void test_stat_simple(struct vt_env *vt_env)
{
	mode_t ifmt = S_IFMT;
	struct stat st;
	char *path0, *path1;

	path0 = vt_new_path_unique(vt_env);
	path1 = vt_new_path_unique(vt_env);

	vt_mkdir(path0, 0700);
	vt_stat(path0, &st);
	vt_expect_true(S_ISDIR(st.st_mode));
	vt_expect_eq((int)(st.st_mode & ~ifmt), 0700);
	vt_expect_eq((long)st.st_nlink, 2);
	vt_stat_noent(path1);

	vt_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects stat(3p) to return ENOTDIR if a component of the path prefix is not
 * a directory.
 */
static void test_stat_notdir(struct vt_env *vt_env)
{
	int fd;
	struct stat st;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_under(vt_env, path0);
	const char *path2 = vt_new_path_under(vt_env, path1);

	vt_mkdir(path0, 0700);
	vt_stat(path0, &st);
	vt_expect_true(S_ISDIR(st.st_mode));
	vt_open(path1, O_CREAT | O_RDWR, 0644, &fd);
	vt_stat(path1, &st);
	vt_expect_true(S_ISREG(st.st_mode));
	vt_expect_false(S_ISREG(st.st_size));
	vt_expect_false(S_ISREG(st.st_blocks));
	vt_stat_err(path2, -ENOTDIR);
	vt_unlink(path1);
	vt_rmdir(path0);
	vt_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects statvfs(3p) to return valid result for dir-path, reg-path or rd-open
 * file-descriptor.
 */
static void test_stat_statvfs(struct vt_env *vt_env)
{
	int fd;
	struct statvfs stv0, stv1;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_under(vt_env, path0);
	const char *path2 = vt_new_path_under(vt_env, path1);
	const char *path3 = vt_new_path_under(vt_env, path0);

	vt_mkdir(path0, 0750);
	vt_creat(path1, 0644, &fd);
	vt_statvfs(path0, &stv0);
	vt_statvfs(path1, &stv1);
	vt_expect_true((stv0.f_bavail > 0));
	vt_expect_eq(stv0.f_fsid, stv1.f_fsid);
	vt_fstatvfs(fd, &stv1);
	vt_expect_eq(stv0.f_fsid, stv1.f_fsid);
	vt_statvfs_err(path2, -ENOTDIR);
	vt_statvfs_err(path3, -ENOENT);

	vt_close(fd);
	vt_unlink(path1);
	vt_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTEST(test_stat_simple),
	VT_DEFTEST(test_stat_notdir),
	VT_DEFTEST(test_stat_statvfs),
};

const struct vt_tests vt_test_stat = VT_DEFTESTS(vt_local_tests);
