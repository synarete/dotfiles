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
#include <sys/xattr.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "sanity.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful setxattr/getxattr/removexattr operations
 */
static void test_xattr_simple(struct voluta_t_ctx *t_ctx)
{
	int fd;
	size_t sz;
	const char *name = "user.digits";
	const char *value = "0123456789";
	const size_t valsz = strlen(value);
	char buf[80] = "";
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0700, &fd);
	voluta_t_close(fd);
	voluta_t_setxattr(path, name, value, valsz, 0);
	voluta_t_getxattr(path, name, NULL, 0, &sz);
	voluta_t_expect_eq(sz, valsz);
	voluta_t_getxattr(path, name, buf, sizeof(buf), &sz);
	voluta_t_expect_eq(sz, valsz);
	voluta_t_expect_eqm(value, buf, valsz);
	voluta_t_removexattr(path, name);
	voluta_t_getxattr_err(path, name, -ENODATA);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful setxattr/getxattr operations on various inode types.
 */
static void test_xattr_inode(struct voluta_t_ctx *t_ctx)
{
	int fd;
	size_t sz;
	const char *name = "user.digits";
	const char *value = "0123456789";
	const size_t valsz = strlen(value);
	char buf[80] = "";
	const char *path1 = voluta_t_new_path_unique(t_ctx);
	const char *path2 = voluta_t_new_path_under(t_ctx, path1);
	const char *path3 = voluta_t_new_path_under(t_ctx, path1);

	voluta_t_mkdir(path1, 0700);
	voluta_t_open(path2, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_symlink(path1, path3);

	voluta_t_setxattr(path1, name, value, valsz, 0);
	voluta_t_getxattr(path1, name, NULL, 0, &sz);
	voluta_t_expect_eq(sz, valsz);
	voluta_t_getxattr(path1, name, buf, sizeof(buf), &sz);
	voluta_t_expect_eq(sz, valsz);
	voluta_t_expect_eqm(value, buf, valsz);

	voluta_t_setxattr(path2, name, value, valsz, 0);
	voluta_t_getxattr(path2, name, NULL, 0, &sz);
	voluta_t_expect_eq(sz, valsz);
	voluta_t_getxattr(path2, name, buf, sizeof(buf), &sz);
	voluta_t_expect_eq(sz, valsz);
	voluta_t_expect_eqm(value, buf, valsz);

	voluta_t_setxattr(path3, name, value, valsz, 0);
	voluta_t_getxattr(path3, name, NULL, 0, &sz);
	voluta_t_expect_eq(sz, valsz);
	voluta_t_getxattr(path3, name, buf, sizeof(buf), &sz);
	voluta_t_expect_eq(sz, valsz);
	voluta_t_expect_eqm(value, buf, valsz);

	voluta_t_close(fd);
	voluta_t_unlink(path3);
	voluta_t_unlink(path2);
	voluta_t_rmdir(path1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful setxattr/removexattr operations to change CTIME only
 */
static void test_xattr_ctime(struct voluta_t_ctx *t_ctx)
{
	int fd;
	size_t sz;
	struct stat st1, st2;
	const char *name = "user.abc";
	const char *value = "ABCDEF-ABCDEF-ABCDEF-ABCDEF-ABCDEF";
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0700, &fd);
	voluta_t_fstat(fd, &st1);
	voluta_t_fsetxattr(fd, name, value, strlen(value), 0);
	voluta_t_fstat(fd, &st2);
	voluta_t_expect_mtime_eq(&st1, &st2);
	voluta_t_expect_ctime_ge(&st1, &st2);

	voluta_t_fstat(fd, &st1);
	voluta_t_fgetxattr(fd, name, NULL, 0, &sz);
	voluta_t_fstat(fd, &st2);
	voluta_t_expect_mtime_eq(&st1, &st2);
	voluta_t_expect_ctime_eq(&st1, &st2);

	voluta_t_fstat(fd, &st1);
	voluta_t_fremovexattr(fd, name);
	voluta_t_fstat(fd, &st2);
	voluta_t_expect_mtime_eq(&st1, &st2);
	voluta_t_expect_ctime_ge(&st1, &st2);

	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful setxattr operations with implicit/explicit rename
 */
static void test_xattr_replace(struct voluta_t_ctx *t_ctx)
{
	int fd;
	size_t sz;
	const char *name = "user.xzy";
	const char *val1 = "0123456789";
	const char *val2 = "ABCDEFGHIJKLMNOPQRSTUVXYZ";
	const char *val3 = "abcdefghijklmnopqrstuvwxyz0123456789";
	const char *path = voluta_t_new_path_unique(t_ctx);
	char buf[256] = "";

	voluta_t_open(path, O_CREAT | O_RDWR, 0700, &fd);
	voluta_t_fsetxattr(fd, name, val1, strlen(val1), 0);
	voluta_t_fgetxattr(fd, name, NULL, 0, &sz);
	voluta_t_expect_eq(sz, strlen(val1));
	voluta_t_fgetxattr(fd, name, buf, sizeof(buf) - 1, &sz);
	voluta_t_expect_eq(sz, strlen(val1));
	voluta_t_expect_eqm(buf, val1, sz);

	voluta_t_fsetxattr(fd, name, val2, strlen(val2), 0);
	voluta_t_fgetxattr(fd, name, NULL, 0, &sz);
	voluta_t_expect_eq(sz, strlen(val2));
	voluta_t_fgetxattr(fd, name, buf, sizeof(buf) - 1, &sz);
	voluta_t_expect_eq(sz, strlen(val2));
	voluta_t_expect_eqm(buf, val2, sz);

	voluta_t_fsetxattr(fd, name, val3, strlen(val3), XATTR_REPLACE);
	voluta_t_fgetxattr(fd, name, NULL, 0, &sz);
	voluta_t_expect_eq(sz, strlen(val3));
	voluta_t_fgetxattr(fd, name, buf, sizeof(buf) - 1, &sz);
	voluta_t_expect_eq(sz, strlen(val3));
	voluta_t_expect_eqm(buf, val3, sz);

	voluta_t_fremovexattr(fd, name);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_xattr_simple),
	VOLUTA_T_DEFTEST(test_xattr_inode),
	VOLUTA_T_DEFTEST(test_xattr_ctime),
	VOLUTA_T_DEFTEST(test_xattr_replace),
};

const struct voluta_t_tests voluta_t_test_xattr =
	VOLUTA_T_DEFTESTS(t_local_tests);

