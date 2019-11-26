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
#include <errno.h>
#include "fstests.h"


/* Fragment size (see man stat(2)) */
#define VOLUTA_T_FRGSIZE (512)

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects statvfs(3p) to successfully obtain information about the file system
 * containing base-dir, and return ENOENT if a component of path does not name
 * an existing file.
 */
static void test_statvfs_simple(struct voluta_t_ctx *t_ctx)
{
	struct statvfs stv;
	const char *name = voluta_t_new_name_unique(t_ctx);
	const char *path = voluta_t_new_path_name(t_ctx, name);

	voluta_t_statvfs(t_ctx->params.workdir, &stv);
	voluta_t_expect_gt(stv.f_bsize, 0);
	voluta_t_expect_eq((stv.f_bsize % VOLUTA_T_FRGSIZE), 0);
	voluta_t_expect_gt(stv.f_frsize, 0);
	voluta_t_expect_eq(stv.f_frsize % VOLUTA_T_FRGSIZE, 0);
	voluta_t_expect_gt(stv.f_namemax, strlen(name));
	voluta_t_statvfs_err(path, -ENOENT);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects statvfs(3p) to successfully obtain information about the file system
 * via open file-descriptor to regular-file.
 */
static void test_statvfs_reg(struct voluta_t_ctx *t_ctx)
{
	int fd;
	struct statvfs stv;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path, 0644, &fd);
	voluta_t_fstatvfs(fd, &stv);
	voluta_t_expect_gt(stv.f_bsize, 0);
	voluta_t_expect_eq((stv.f_bsize % VOLUTA_T_FRGSIZE), 0);
	voluta_t_expect_gt(stv.f_frsize, 0);
	voluta_t_expect_eq((stv.f_frsize % VOLUTA_T_FRGSIZE), 0);
	voluta_t_expect_lt(stv.f_bfree, stv.f_blocks);
	voluta_t_expect_lt(stv.f_bavail, stv.f_blocks);
	voluta_t_expect_lt(stv.f_ffree, stv.f_files);
	voluta_t_expect_lt(stv.f_favail, stv.f_files);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects statvfs(3p) to successfully obtain information about the file system
 * via open file-descriptor to directory
 */
static void test_statvfs_dir(struct voluta_t_ctx *t_ctx)
{
	int dfd;
	struct statvfs stv;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_mkdir(path, 0700);
	voluta_t_open(path, O_DIRECTORY | O_RDONLY, 0, &dfd);
	voluta_t_fstatvfs(dfd, &stv);
	voluta_t_expect_gt(stv.f_bsize, 0);
	voluta_t_expect_eq((stv.f_bsize % VOLUTA_T_FRGSIZE), 0);
	voluta_t_expect_gt(stv.f_frsize, 0);
	voluta_t_expect_eq((stv.f_frsize % VOLUTA_T_FRGSIZE), 0);
	voluta_t_expect_lt(stv.f_bfree, stv.f_blocks);
	voluta_t_expect_lt(stv.f_bavail, stv.f_blocks);
	voluta_t_expect_lt(stv.f_ffree, stv.f_files);
	voluta_t_expect_lt(stv.f_favail, stv.f_files);
	voluta_t_close(dfd);
	voluta_t_rmdir(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects statvfs(3p) to return ENOTDIR if a component of the path prefix of
 * path is not a directory.
 */
static void test_statvfs_notdir(struct voluta_t_ctx *t_ctx)
{
	int fd;
	struct statvfs stv0, stv1;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_under(t_ctx, path0);
	const char *path2 = voluta_t_new_path_under(t_ctx, path1);

	voluta_t_mkdir(path0, 0700);
	voluta_t_statvfs(path0, &stv0);
	voluta_t_creat(path1, 0644, &fd);
	voluta_t_fstatvfs(fd, &stv1);
	voluta_t_statvfs_err(path2, -ENOTDIR);
	voluta_t_unlink(path1);
	voluta_t_rmdir(path0);
	voluta_t_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects statvfs(3p) to change statvfs.f_ffree upon objects create/unlink
 */
static void test_statvfs_ffree(struct voluta_t_ctx *t_ctx)
{
	int fd;
	struct statvfs stv0, stv1;
	char *path0, *path1, *path2, *path3, *dpath;

	dpath = voluta_t_new_path_unique(t_ctx);
	path0 = voluta_t_new_path_under(t_ctx, dpath);
	path1 = voluta_t_new_path_under(t_ctx, dpath);
	path2 = voluta_t_new_path_under(t_ctx, dpath);
	path3 = voluta_t_new_path_under(t_ctx, dpath);

	voluta_t_mkdir(dpath, 0700);
	voluta_t_statvfs(dpath, &stv0);
	voluta_t_mkdir(path0, 0700);
	voluta_t_statvfs(path0, &stv1);
	voluta_t_expect_eq(stv1.f_ffree, (stv0.f_ffree - 1));
	voluta_t_rmdir(path0);
	voluta_t_statvfs(dpath, &stv1);
	voluta_t_expect_eq(stv0.f_ffree, stv1.f_ffree);

	voluta_t_statvfs(dpath, &stv0);
	voluta_t_symlink(dpath, path1);
	voluta_t_statvfs(path1, &stv1);
	voluta_t_expect_eq(stv1.f_ffree, (stv0.f_ffree - 1));
	voluta_t_unlink(path1);
	voluta_t_statvfs(dpath, &stv1);
	voluta_t_expect_eq(stv0.f_ffree, stv1.f_ffree);

	voluta_t_statvfs(dpath, &stv0);
	voluta_t_creat(path2, 0600, &fd);
	voluta_t_close(fd);
	voluta_t_statvfs(path2, &stv1);
	voluta_t_expect_eq(stv1.f_ffree, (stv0.f_ffree - 1));
	voluta_t_unlink(path2);
	voluta_t_statvfs(dpath, &stv1);
	voluta_t_expect_eq(stv1.f_ffree, stv0.f_ffree);

	voluta_t_statvfs(dpath, &stv0);
	voluta_t_creat(path3, 0600, &fd);
	voluta_t_fstatvfs(fd, &stv1);
	voluta_t_expect_eq(stv1.f_ffree, (stv0.f_ffree - 1));
	voluta_t_unlink(path3);
	voluta_t_statvfs(dpath, &stv1);
	voluta_t_expect_eq(stv1.f_ffree, (stv0.f_ffree - 1));
	voluta_t_close(fd);
	voluta_t_statvfs(dpath, &stv1);
	voluta_t_expect_eq(stv0.f_ffree, stv1.f_ffree);

	voluta_t_rmdir(dpath);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects statvfs(3p) to change statvfs.f_ffree upon sequence of creates
 * following sequence of unlinks.
 */
static void test_statvfs_ffree_nseq(struct voluta_t_ctx *t_ctx, size_t n)
{
	int fd;
	char *fpath;
	struct statvfs stv0, stv1;
	const char *dpath = voluta_t_new_path_unique(t_ctx);

	voluta_t_mkdir(dpath, 0700);
	voluta_t_statvfs(dpath, &stv0);

	for (size_t i = 0; i < n; ++i) {
		fpath = voluta_t_new_pathf(t_ctx, dpath, "%lu", i);
		voluta_t_statvfs_err(fpath, -ENOENT);
		voluta_t_creat(fpath, 0600, &fd);
		voluta_t_close(fd);
		voluta_t_statvfs(fpath, &stv1);
		voluta_t_expect_eq((stv0.f_ffree - (i + 1)), stv1.f_ffree);
	}
	for (size_t j = n; j > 0; --j) {
		fpath = voluta_t_new_pathf(t_ctx, dpath, "%lu", (j - 1));
		voluta_t_statvfs(fpath, &stv1);
		voluta_t_expect_eq((stv0.f_ffree - j), stv1.f_ffree);
		voluta_t_unlink(fpath);
	}

	voluta_t_statvfs(dpath, &stv1);
	voluta_t_expect_eq(stv0.f_ffree, stv1.f_ffree);
	voluta_t_rmdir(dpath);
}

static void test_statvfs_ffree_seq(struct voluta_t_ctx *t_ctx)
{
	test_statvfs_ffree_nseq(t_ctx, 16);
	test_statvfs_ffree_nseq(t_ctx, 4096);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects statvfs(3p) to change statvfs.f_bfree upon write/trim.
 */
static void test_statvfs_bfree(struct voluta_t_ctx *t_ctx)
{
	int fd;
	size_t nwr;
	struct statvfs stv0, stv1;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_under(t_ctx, path0);

	voluta_t_mkdir(path0, 0700);
	voluta_t_creat(path1, 0600, &fd);
	voluta_t_fstatvfs(fd, &stv0);
	voluta_t_write(fd, "1", 1, &nwr);
	voluta_t_fstatvfs(fd, &stv1);
	voluta_t_expect_gt(stv0.f_bfree, stv1.f_bfree);
	voluta_t_ftruncate(fd, 0);
	voluta_t_fstatvfs(fd, &stv1);
	voluta_t_expect_eq(stv1.f_bfree, stv0.f_bfree);
	voluta_t_close(fd);
	voluta_t_unlink(path1);
	voluta_t_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_statvfs_simple),
	VOLUTA_T_DEFTEST(test_statvfs_reg),
	VOLUTA_T_DEFTEST(test_statvfs_dir),
	VOLUTA_T_DEFTEST(test_statvfs_notdir),
	VOLUTA_T_DEFTEST(test_statvfs_ffree),
	VOLUTA_T_DEFTEST(test_statvfs_ffree_seq),
	VOLUTA_T_DEFTESTF(test_statvfs_bfree, VOLUTA_T_STAVFS),
};

const struct voluta_t_tests
voluta_t_test_statvfs = VOLUTA_T_DEFTESTS(t_local_tests);
