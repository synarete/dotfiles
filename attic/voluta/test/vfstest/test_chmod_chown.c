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
#include "vfstest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects chmod(3p) to do change permissions.
 */
static void test_chmod_basic(struct vt_env *vt_env)
{
	int fd;
	mode_t ifmt = S_IFMT;
	struct stat st;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_unique(vt_env);
	const char *path2 = vt_new_path_unique(vt_env);

	vt_creat(path0, 0644, &fd);
	vt_close(fd);
	vt_stat(path0, &st);
	vt_expect_eq((st.st_mode & ~ifmt), 0644);
	vt_chmod(path0, 0111);
	vt_stat(path0, &st);
	vt_expect_eq((st.st_mode & ~ifmt), 0111);
	vt_unlink(path0);

	vt_mkdir(path1, 0755);
	vt_stat(path1, &st);
	vt_expect_eq((st.st_mode & ~ifmt), 0755);
	vt_chmod(path1, 0753);
	vt_stat(path1, &st);
	vt_expect_eq((st.st_mode & ~ifmt), 0753);
	vt_rmdir(path1);

	vt_creat(path0, 0644, &fd);
	vt_close(fd);
	vt_symlink(path0, path2);
	vt_stat(path2, &st);
	vt_expect_eq((st.st_mode & ~ifmt), 0644);
	vt_chmod(path2, 0321);
	vt_stat(path2, &st);
	vt_expect_eq((st.st_mode & ~ifmt), 0321);
	vt_stat(path0, &st);
	vt_expect_eq((st.st_mode & ~ifmt), 0321);
	vt_unlink(path0);
	vt_unlink(path2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects chmod(3p) to updates ctime if successful.
 */
static void test_chmod_ctime(struct vt_env *vt_env)
{
	int fd;
	struct stat st1, st2;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_unique(vt_env);
	const char *path2 = vt_new_path_unique(vt_env);

	vt_creat(path0, 0644, &fd);
	vt_fstat(fd, &st1);
	vt_suspend(vt_env, 3, 2);
	vt_chmod(path0, 0111);
	vt_fstat(fd, &st2);
	vt_expect_ctime_gt(&st1, &st2);
	vt_close(fd);
	vt_unlink(path0);

	vt_mkdir(path1, 0755);
	vt_stat(path1, &st1);
	vt_suspend(vt_env, 3, 2);
	vt_chmod(path1, 0753);
	vt_stat(path1, &st2);
	vt_expect_ctime_gt(&st1, &st2);
	vt_rmdir(path1);

	vt_mkfifo(path2, 0640);
	vt_stat(path2, &st1);
	vt_suspend(vt_env, 3, 2);
	vt_chmod(path2, 0300);
	vt_stat(path2, &st2);
	vt_expect_ctime_gt(&st1, &st2);
	vt_unlink(path2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects fchmod(3p) to operate on regular files
 */
static void test_chmod_fchmod(struct vt_env *vt_env)
{
	int fd;
	mode_t ifmt = S_IFMT;
	struct stat st;
	const char *path = vt_new_path_unique(vt_env);

	vt_creat(path, 0600, &fd);
	vt_fstat(fd, &st);
	vt_expect_eq((st.st_mode & ~ifmt), 0600);
	vt_fchmod(fd, 0755);
	vt_fstat(fd, &st);
	vt_expect_eq((st.st_mode & ~ifmt), 0755);
	vt_fchmod(fd, 0100);
	vt_fstat(fd, &st);
	vt_expect_eq((st.st_mode & ~ifmt), 0100);
	vt_fchmod(fd, 0600);
	vt_close(fd);
	vt_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects fchmod(3p) to operate on on unlinked regular files
 */
static void test_chmod_unlinked(struct vt_env *vt_env)
{
	int fd;
	mode_t ifmt = S_IFMT;
	struct stat st;
	const char *path = vt_new_path_unique(vt_env);

	vt_creat(path, 0600, &fd);
	vt_unlink(path);
	vt_fstat(fd, &st);
	vt_expect_eq((st.st_mode & ~ifmt), 0600);
	vt_fchmod(fd, 0755);
	vt_fstat(fd, &st);
	vt_expect_eq((st.st_mode & ~ifmt), 0755);
	vt_fchmod(fd, 0100);
	vt_fstat(fd, &st);
	vt_expect_eq((st.st_mode & ~ifmt), 0100);
	vt_fchmod(fd, 0600);
	vt_fstat(fd, &st);
	vt_expect_eq((st.st_mode & ~ifmt), 0600);
	vt_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects fchmod(3p) to properly set/clear SUID/SGID
 */
static void test_chmod_suid_sgid(struct vt_env *vt_env)
{
	int fd;
	mode_t ifmt = S_IFMT, isuid = S_ISUID, isgid = S_ISGID;
	struct stat st;
	const char *path = vt_new_path_unique(vt_env);

	vt_creat(path, 0755, &fd);
	vt_fstat(fd, &st);
	vt_expect_eq((st.st_mode & ~ifmt), 0755);
	vt_fchmod(fd, st.st_mode | S_ISUID);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & S_ISUID, S_ISUID);
	vt_fchmod(fd, st.st_mode & ~isuid);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & S_ISUID, 0);
	vt_fchmod(fd, st.st_mode | S_ISGID);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & S_ISGID, S_ISGID);
	vt_fchmod(fd, st.st_mode & ~isgid);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & S_ISGID, 0);
	vt_unlink(path);

	vt_fchmod(fd, st.st_mode | S_ISUID);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & S_ISUID, S_ISUID);
	vt_fchmod(fd, st.st_mode & ~isuid);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & S_ISUID, 0);
	vt_fchmod(fd, st.st_mode | S_ISGID);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & S_ISGID, S_ISGID);
	vt_fchmod(fd, st.st_mode & ~isgid);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & S_ISGID, 0);
	vt_close(fd);
}

/* TODO: Check fchmodat */

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects chown(3p) to update CTIME
 */
static void test_chown_ctime(struct vt_env *vt_env)
{
	int fd;
	struct stat st1, st2;
	const char *path = vt_new_path_unique(vt_env);

	vt_creat(path, 0600, &fd);
	vt_stat(path, &st1);
	vt_suspend(vt_env, 2, 5);
	vt_chown(path, st1.st_uid, st1.st_gid);
	vt_stat(path, &st2);
	vt_expect_gt(st2.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	vt_fstat(fd, &st1);
	vt_suspend(vt_env, 2, 5);
	vt_fchown(fd, st1.st_uid, st1.st_gid);
	vt_fstat(fd, &st2);
	vt_expect_gt(st2.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	vt_close(fd);
	vt_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects chown(3p) to work properly on unlinked file
 */
static void test_chown_unlinked(struct vt_env *vt_env)
{
	int fd;
	struct stat st1, st2;
	const char *path = vt_new_path_unique(vt_env);

	vt_creat(path, 0600, &fd);
	vt_unlink(path);
	vt_stat_noent(path);
	vt_fstat(fd, &st1);
	vt_suspend(vt_env, 2, 5);
	vt_fchown(fd, st1.st_uid, st1.st_gid);
	vt_fstat(fd, &st2);
	vt_expect_gt(st2.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	vt_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects fchown(3p) to properly clear SUID/SGID
 */
static void test_chown_suid_sgid(struct vt_env *vt_env)
{
	int fd;
	struct stat st;
	const char *path = vt_new_path_unique(vt_env);

	vt_creat(path, 0600, &fd);
	vt_fstat(fd, &st);
	vt_fchmod(fd, st.st_mode | S_ISUID);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & S_ISUID, S_ISUID);
	vt_fchown(fd, st.st_uid, st.st_gid);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & S_ISUID, 0);
	vt_fchmod(fd, st.st_mode | S_ISGID | S_IXGRP);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & S_ISGID, S_ISGID);
	vt_fchown(fd, st.st_uid, st.st_gid);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & S_ISGID, 0);
	vt_close(fd);
	vt_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTEST(test_chmod_basic),
	VT_DEFTEST(test_chmod_ctime),
	VT_DEFTEST(test_chmod_fchmod),
	VT_DEFTEST(test_chmod_unlinked),
	VT_DEFTEST(test_chmod_suid_sgid),
	VT_DEFTEST(test_chown_ctime),
	VT_DEFTEST(test_chown_unlinked),
	VT_DEFTEST(test_chown_suid_sgid),
};

const struct vt_tests vt_test_chmod = VT_DEFTESTS(vt_local_tests);

