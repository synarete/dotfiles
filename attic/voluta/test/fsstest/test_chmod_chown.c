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
#include <unistd.h>
#include "fsstest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects chmod(3p) to do change permissions.
 */
static void test_chmod_basic(struct voluta_t_ctx *t_ctx)
{
	int fd;
	mode_t ifmt = S_IFMT;
	struct stat st;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_unique(t_ctx);
	const char *path2 = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path0, 0644, &fd);
	voluta_t_close(fd);
	voluta_t_stat(path0, &st);
	voluta_t_expect_eq((st.st_mode & ~ifmt), 0644);
	voluta_t_chmod(path0, 0111);
	voluta_t_stat(path0, &st);
	voluta_t_expect_eq((st.st_mode & ~ifmt), 0111);
	voluta_t_unlink(path0);

	voluta_t_mkdir(path1, 0755);
	voluta_t_stat(path1, &st);
	voluta_t_expect_eq((st.st_mode & ~ifmt), 0755);
	voluta_t_chmod(path1, 0753);
	voluta_t_stat(path1, &st);
	voluta_t_expect_eq((st.st_mode & ~ifmt), 0753);
	voluta_t_rmdir(path1);

	voluta_t_creat(path0, 0644, &fd);
	voluta_t_close(fd);
	voluta_t_symlink(path0, path2);
	voluta_t_stat(path2, &st);
	voluta_t_expect_eq((st.st_mode & ~ifmt), 0644);
	voluta_t_chmod(path2, 0321);
	voluta_t_stat(path2, &st);
	voluta_t_expect_eq((st.st_mode & ~ifmt), 0321);
	voluta_t_stat(path0, &st);
	voluta_t_expect_eq((st.st_mode & ~ifmt), 0321);
	voluta_t_unlink(path0);
	voluta_t_unlink(path2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects chmod(3p) to updates ctime if successful.
 */
static void test_chmod_ctime(struct voluta_t_ctx *t_ctx)
{
	int fd;
	struct stat st1, st2;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_unique(t_ctx);
	const char *path2 = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path0, 0644, &fd);
	voluta_t_fstat(fd, &st1);
	voluta_t_suspend(t_ctx, 3, 2);
	voluta_t_chmod(path0, 0111);
	voluta_t_fstat(fd, &st2);
	voluta_t_expect_ctime_gt(&st1, &st2);
	voluta_t_close(fd);
	voluta_t_unlink(path0);

	voluta_t_mkdir(path1, 0755);
	voluta_t_stat(path1, &st1);
	voluta_t_suspend(t_ctx, 3, 2);
	voluta_t_chmod(path1, 0753);
	voluta_t_stat(path1, &st2);
	voluta_t_expect_ctime_gt(&st1, &st2);
	voluta_t_rmdir(path1);

	voluta_t_mkfifo(path2, 0640);
	voluta_t_stat(path2, &st1);
	voluta_t_suspend(t_ctx, 3, 2);
	voluta_t_chmod(path2, 0300);
	voluta_t_stat(path2, &st2);
	voluta_t_expect_ctime_gt(&st1, &st2);
	voluta_t_unlink(path2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects fchmod(3p) to operate on regular files
 */
static void test_chmod_fchmod(struct voluta_t_ctx *t_ctx)
{
	int fd;
	mode_t ifmt = S_IFMT;
	struct stat st;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path, 0600, &fd);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq((st.st_mode & ~ifmt), 0600);
	voluta_t_fchmod(fd, 0755);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq((st.st_mode & ~ifmt), 0755);
	voluta_t_fchmod(fd, 0100);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq((st.st_mode & ~ifmt), 0100);
	voluta_t_fchmod(fd, 0600);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects fchmod(3p) to operate on on unlinked regular files
 */
static void test_chmod_unlinked(struct voluta_t_ctx *t_ctx)
{
	int fd;
	mode_t ifmt = S_IFMT;
	struct stat st;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path, 0600, &fd);
	voluta_t_unlink(path);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq((st.st_mode & ~ifmt), 0600);
	voluta_t_fchmod(fd, 0755);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq((st.st_mode & ~ifmt), 0755);
	voluta_t_fchmod(fd, 0100);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq((st.st_mode & ~ifmt), 0100);
	voluta_t_fchmod(fd, 0600);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq((st.st_mode & ~ifmt), 0600);
	voluta_t_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects fchmod(3p) to properly set/clear SUID/SGID
 */
static void test_chmod_suid_sgid(struct voluta_t_ctx *t_ctx)
{
	int fd;
	mode_t ifmt = S_IFMT, isuid = S_ISUID, isgid = S_ISGID;
	struct stat st;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path, 0755, &fd);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq((st.st_mode & ~ifmt), 0755);
	voluta_t_fchmod(fd, st.st_mode | S_ISUID);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISUID, S_ISUID);
	voluta_t_fchmod(fd, st.st_mode & ~isuid);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISUID, 0);
	voluta_t_fchmod(fd, st.st_mode | S_ISGID);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISGID, S_ISGID);
	voluta_t_fchmod(fd, st.st_mode & ~isgid);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISGID, 0);
	voluta_t_unlink(path);

	voluta_t_fchmod(fd, st.st_mode | S_ISUID);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISUID, S_ISUID);
	voluta_t_fchmod(fd, st.st_mode & ~isuid);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISUID, 0);
	voluta_t_fchmod(fd, st.st_mode | S_ISGID);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISGID, S_ISGID);
	voluta_t_fchmod(fd, st.st_mode & ~isgid);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISGID, 0);
	voluta_t_close(fd);
}

/* TODO: Check fchmodat */

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects chown(3p) to update CTIME
 */
static void test_chown_ctime(struct voluta_t_ctx *t_ctx)
{
	int fd;
	struct stat st1, st2;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path, 0600, &fd);
	voluta_t_stat(path, &st1);
	voluta_t_suspend(t_ctx, 2, 5);
	voluta_t_chown(path, st1.st_uid, st1.st_gid);
	voluta_t_stat(path, &st2);
	voluta_t_expect_gt(st2.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	voluta_t_fstat(fd, &st1);
	voluta_t_suspend(t_ctx, 2, 5);
	voluta_t_fchown(fd, st1.st_uid, st1.st_gid);
	voluta_t_fstat(fd, &st2);
	voluta_t_expect_gt(st2.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects chown(3p) to work properly on unlinked file
 */
static void test_chown_unlinked(struct voluta_t_ctx *t_ctx)
{
	int fd;
	struct stat st1, st2;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path, 0600, &fd);
	voluta_t_unlink(path);
	voluta_t_stat_noent(path);
	voluta_t_fstat(fd, &st1);
	voluta_t_suspend(t_ctx, 2, 5);
	voluta_t_fchown(fd, st1.st_uid, st1.st_gid);
	voluta_t_fstat(fd, &st2);
	voluta_t_expect_gt(st2.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	voluta_t_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects fchown(3p) to properly clear SUID/SGID
 */
static void test_chown_suid_sgid(struct voluta_t_ctx *t_ctx)
{
	int fd;
	struct stat st;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path, 0600, &fd);
	voluta_t_fstat(fd, &st);
	voluta_t_fchmod(fd, st.st_mode | S_ISUID);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISUID, S_ISUID);
	voluta_t_fchown(fd, st.st_uid, st.st_gid);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISUID, 0);
	voluta_t_fchmod(fd, st.st_mode | S_ISGID | S_IXGRP);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISGID, S_ISGID);
	voluta_t_fchown(fd, st.st_uid, st.st_gid);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISGID, 0);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_chmod_basic),
	VOLUTA_T_DEFTEST(test_chmod_ctime),
	VOLUTA_T_DEFTEST(test_chmod_fchmod),
	VOLUTA_T_DEFTEST(test_chmod_unlinked),
	VOLUTA_T_DEFTEST(test_chmod_suid_sgid),
	VOLUTA_T_DEFTEST(test_chown_ctime),
	VOLUTA_T_DEFTEST(test_chown_unlinked),
	VOLUTA_T_DEFTEST(test_chown_suid_sgid),
};

const struct voluta_t_tests voluta_t_test_chmod =
	VOLUTA_T_DEFTESTS(t_local_tests);

