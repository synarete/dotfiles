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
#include <errno.h>
#include "vfstest.h"



/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects statvfs(3p) to successfully obtain information about the file system
 * containing base-dir, and return ENOENT if a component of path does not name
 * an existing file.
 */
static void test_statvfs_simple(struct vt_env *vt_env)
{
	struct statvfs stv;
	const char *name = vt_new_name_unique(vt_env);
	const char *path = vt_new_path_name(vt_env, name);

	vt_statvfs(vt_env->params.workdir, &stv);
	vt_expect_gt(stv.f_bsize, 0);
	vt_expect_eq((stv.f_bsize % VT_FRGSIZE), 0);
	vt_expect_gt(stv.f_frsize, 0);
	vt_expect_eq(stv.f_frsize % VT_FRGSIZE, 0);
	vt_expect_gt(stv.f_namemax, strlen(name));
	vt_statvfs_err(path, -ENOENT);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects statvfs(3p) to successfully obtain information about the file system
 * via open file-descriptor to regular-file.
 */
static void test_statvfs_reg(struct vt_env *vt_env)
{
	int fd;
	struct statvfs stv;
	const char *path = vt_new_path_unique(vt_env);

	vt_creat(path, 0644, &fd);
	vt_fstatvfs(fd, &stv);
	vt_expect_gt(stv.f_bsize, 0);
	vt_expect_eq((stv.f_bsize % VT_FRGSIZE), 0);
	vt_expect_gt(stv.f_frsize, 0);
	vt_expect_eq((stv.f_frsize % VT_FRGSIZE), 0);
	vt_expect_lt(stv.f_bfree, stv.f_blocks);
	vt_expect_lt(stv.f_bavail, stv.f_blocks);
	vt_expect_lt(stv.f_ffree, stv.f_files);
	vt_expect_lt(stv.f_favail, stv.f_files);
	vt_close(fd);
	vt_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects statvfs(3p) to successfully obtain information about the file system
 * via open file-descriptor to directory
 */
static void test_statvfs_dir(struct vt_env *vt_env)
{
	int dfd;
	struct statvfs stv;
	const char *path = vt_new_path_unique(vt_env);

	vt_mkdir(path, 0700);
	vt_open(path, O_DIRECTORY | O_RDONLY, 0, &dfd);
	vt_fstatvfs(dfd, &stv);
	vt_expect_gt(stv.f_bsize, 0);
	vt_expect_eq((stv.f_bsize % VT_FRGSIZE), 0);
	vt_expect_gt(stv.f_frsize, 0);
	vt_expect_eq((stv.f_frsize % VT_FRGSIZE), 0);
	vt_expect_lt(stv.f_bfree, stv.f_blocks);
	vt_expect_lt(stv.f_bavail, stv.f_blocks);
	vt_expect_lt(stv.f_ffree, stv.f_files);
	vt_expect_lt(stv.f_favail, stv.f_files);
	vt_close(dfd);
	vt_rmdir(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects statvfs(3p) to return ENOTDIR if a component of the path prefix of
 * path is not a directory.
 */
static void test_statvfs_notdir(struct vt_env *vt_env)
{
	int fd;
	struct statvfs stv0, stv1;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_under(vt_env, path0);
	const char *path2 = vt_new_path_under(vt_env, path1);

	vt_mkdir(path0, 0700);
	vt_statvfs(path0, &stv0);
	vt_creat(path1, 0644, &fd);
	vt_fstatvfs(fd, &stv1);
	vt_statvfs_err(path2, -ENOTDIR);
	vt_unlink(path1);
	vt_rmdir(path0);
	vt_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects statvfs(3p) to change statvfs.f_ffree upon objects create/unlink
 */
static void test_statvfs_ffree(struct vt_env *vt_env)
{
	int fd;
	struct statvfs stv0, stv1;
	char *path0, *path1, *path2, *path3, *dpath;

	dpath = vt_new_path_unique(vt_env);
	path0 = vt_new_path_under(vt_env, dpath);
	path1 = vt_new_path_under(vt_env, dpath);
	path2 = vt_new_path_under(vt_env, dpath);
	path3 = vt_new_path_under(vt_env, dpath);

	vt_mkdir(dpath, 0700);
	vt_statvfs(dpath, &stv0);
	vt_mkdir(path0, 0700);
	vt_statvfs(path0, &stv1);
	vt_expect_eq(stv1.f_ffree, (stv0.f_ffree - 1));
	vt_rmdir(path0);
	vt_statvfs(dpath, &stv1);
	vt_expect_eq(stv0.f_ffree, stv1.f_ffree);

	vt_statvfs(dpath, &stv0);
	vt_symlink(dpath, path1);
	vt_statvfs(path1, &stv1);
	vt_expect_eq(stv1.f_ffree, (stv0.f_ffree - 1));
	vt_unlink(path1);
	vt_statvfs(dpath, &stv1);
	vt_expect_eq(stv0.f_ffree, stv1.f_ffree);

	vt_statvfs(dpath, &stv0);
	vt_creat(path2, 0600, &fd);
	vt_close(fd);
	vt_statvfs(path2, &stv1);
	vt_expect_eq(stv1.f_ffree, (stv0.f_ffree - 1));
	vt_unlink(path2);
	vt_statvfs(dpath, &stv1);
	vt_expect_eq(stv1.f_ffree, stv0.f_ffree);

	vt_statvfs(dpath, &stv0);
	vt_creat(path3, 0600, &fd);
	vt_fstatvfs(fd, &stv1);
	vt_expect_eq(stv1.f_ffree, (stv0.f_ffree - 1));
	vt_unlink(path3);
	vt_statvfs(dpath, &stv1);
	vt_expect_eq(stv1.f_ffree, (stv0.f_ffree - 1));
	vt_close(fd);
	vt_statvfs(dpath, &stv1);
	vt_expect_eq(stv0.f_ffree, stv1.f_ffree);

	vt_rmdir(dpath);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects statvfs(3p) to change statvfs.f_ffree upon sequence of creates
 * following sequence of unlinks.
 */
static void test_statvfs_ffree_nseq(struct vt_env *vt_env, size_t n)
{
	int fd;
	char *fpath;
	struct statvfs stv0, stv1;
	const char *dpath = vt_new_path_unique(vt_env);

	vt_mkdir(dpath, 0700);
	vt_statvfs(dpath, &stv0);

	for (size_t i = 0; i < n; ++i) {
		fpath = vt_new_pathf(vt_env, dpath, "%lu", i);
		vt_statvfs_err(fpath, -ENOENT);
		vt_creat(fpath, 0600, &fd);
		vt_close(fd);
		vt_statvfs(fpath, &stv1);
		vt_expect_eq((stv0.f_ffree - (i + 1)), stv1.f_ffree);
	}
	for (size_t j = n; j > 0; --j) {
		fpath = vt_new_pathf(vt_env, dpath, "%lu", (j - 1));
		vt_statvfs(fpath, &stv1);
		vt_expect_eq((stv0.f_ffree - j), stv1.f_ffree);
		vt_unlink(fpath);
	}

	vt_statvfs(dpath, &stv1);
	vt_expect_eq(stv0.f_ffree, stv1.f_ffree);
	vt_rmdir(dpath);
}

static void test_statvfs_ffree_seq(struct vt_env *vt_env)
{
	test_statvfs_ffree_nseq(vt_env, 16);
	test_statvfs_ffree_nseq(vt_env, 4096);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects statvfs(3p) to change statvfs.f_bfree upon write/trim.
 */
static void test_statvfs_bfree_(struct vt_env *vt_env, loff_t off)
{
	int fd;
	size_t bsz = VT_UMEGA;
	struct stat st1, st2;
	struct statvfs stv1, stv2;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_under(vt_env, path0);
	void *buf1 = vt_new_buf_rands(vt_env, bsz);
	void *buf2 = vt_new_buf_rands(vt_env, bsz);

	vt_mkdir(path0, 0700);
	vt_open(path1, O_CREAT | O_RDWR, 0600, &fd);
	vt_fstat(fd, &st1);
	vt_fstatvfs(fd, &stv1);
	vt_pwriten(fd, buf1, bsz, off);
	vt_preadn(fd, buf2, bsz, off);
	vt_expect_eqm(buf1, buf2, bsz);
	vt_fstat(fd, &st2);
	vt_fstatvfs(fd, &stv2);
	vt_expect_gt(st2.st_blocks, st1.st_blocks);
	vt_expect_gt(stv1.f_bfree, stv2.f_bfree);
	vt_ftruncate(fd, 0);
	vt_fstat(fd, &st2);
	vt_fstatvfs(fd, &stv2);
	vt_expect_eq(st2.st_blocks, st1.st_blocks);
	vt_expect_eq(stv2.f_bfree, stv1.f_bfree);
	vt_close(fd);
	vt_unlink(path1);
	vt_rmdir(path0);
}

static void test_statvfs_bfree(struct vt_env *vt_env)
{
	test_statvfs_bfree_(vt_env, 0);
	test_statvfs_bfree_(vt_env, VT_MEGA);
	test_statvfs_bfree_(vt_env, VT_MEGA + 1);
	test_statvfs_bfree_(vt_env, VT_TERA - 11);
	test_statvfs_bfree_(vt_env, VT_FILESIZE_MAX - VT_UMEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTEST(test_statvfs_simple),
	VT_DEFTEST(test_statvfs_reg),
	VT_DEFTEST(test_statvfs_dir),
	VT_DEFTEST(test_statvfs_notdir),
	VT_DEFTEST(test_statvfs_ffree),
	VT_DEFTEST(test_statvfs_ffree_seq),
	VT_DEFTEST(test_statvfs_bfree),
};

const struct vt_tests vt_test_statvfs = VT_DEFTESTS(vt_local_tests);
