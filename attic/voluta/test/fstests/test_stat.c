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
#include <sys/statvfs.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "fstests.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects stat(3p) to successfully probe directory and return ENOENT if a
 * component of path does not name an existing file or path is an empty string.
 */
static void test_stat_simple(struct voluta_t_ctx *t_ctx)
{
	mode_t ifmt = S_IFMT;
	struct stat st;
	char *path0, *path1;

	path0 = voluta_t_new_path_unique(t_ctx);
	path1 = voluta_t_new_path_unique(t_ctx);

	voluta_t_mkdir(path0, 0700);
	voluta_t_stat(path0, &st);
	voluta_t_expect_true(S_ISDIR(st.st_mode));
	voluta_t_expect_eq((int)(st.st_mode & ~ifmt), 0700);
	voluta_t_expect_eq((long)st.st_nlink, 2);
	voluta_t_stat_noent(path1);

	voluta_t_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects stat(3p) to return ENOTDIR if a component of the path prefix is not
 * a directory.
 */
static void test_stat_notdir(struct voluta_t_ctx *t_ctx)
{
	int fd;
	struct stat st;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_under(t_ctx, path0);
	const char *path2 = voluta_t_new_path_under(t_ctx, path1);

	voluta_t_mkdir(path0, 0700);
	voluta_t_stat(path0, &st);
	voluta_t_expect_true(S_ISDIR(st.st_mode));
	voluta_t_open(path1, O_CREAT | O_RDWR, 0644, &fd);
	voluta_t_stat(path1, &st);
	voluta_t_expect_true(S_ISREG(st.st_mode));
	voluta_t_expect_false(S_ISREG(st.st_size));
	voluta_t_expect_false(S_ISREG(st.st_blocks));
	voluta_t_stat_err(path2, -ENOTDIR);
	voluta_t_unlink(path1);
	voluta_t_rmdir(path0);
	voluta_t_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects statvfs(3p) to return valid result for dir-path, reg-path or rd-open
 * file-descriptor.
 */
static void test_stat_statvfs(struct voluta_t_ctx *t_ctx)
{
	int fd;
	struct statvfs stv0, stv1;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_under(t_ctx, path0);
	const char *path2 = voluta_t_new_path_under(t_ctx, path1);
	const char *path3 = voluta_t_new_path_under(t_ctx, path0);

	voluta_t_mkdir(path0, 0750);
	voluta_t_creat(path1, 0644, &fd);
	voluta_t_statvfs(path0, &stv0);
	voluta_t_statvfs(path1, &stv1);
	voluta_t_expect_true((stv0.f_bavail > 0));
	voluta_t_expect_eq(stv0.f_fsid, stv1.f_fsid);
	voluta_t_fstatvfs(fd, &stv1);
	voluta_t_expect_eq(stv0.f_fsid, stv1.f_fsid);
	voluta_t_statvfs_err(path2, -ENOTDIR);
	voluta_t_statvfs_err(path3, -ENOENT);

	voluta_t_close(fd);
	voluta_t_unlink(path1);
	voluta_t_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_stat_simple),
	VOLUTA_T_DEFTEST(test_stat_notdir),
	VOLUTA_T_DEFTEST(test_stat_statvfs),
};

const struct voluta_t_tests
voluta_t_test_stat = VOLUTA_T_DEFTESTS(t_local_tests);
