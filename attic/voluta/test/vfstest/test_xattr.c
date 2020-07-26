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
#include <sys/xattr.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "vfstest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful setxattr/getxattr/removexattr operations
 */
static void test_xattr_simple(struct vt_env *vt_env)
{
	int fd;
	size_t sz;
	const char *name = "user.digits";
	const char *value = "0123456789";
	const size_t valsz = strlen(value);
	char buf[80] = "";
	const char *path = vt_new_path_unique(vt_env);

	vt_open(path, O_CREAT | O_RDWR, 0700, &fd);
	vt_close(fd);
	vt_setxattr(path, name, value, valsz, 0);
	vt_getxattr(path, name, NULL, 0, &sz);
	vt_expect_eq(sz, valsz);
	vt_getxattr(path, name, buf, sizeof(buf), &sz);
	vt_expect_eq(sz, valsz);
	vt_expect_eqm(value, buf, valsz);
	vt_removexattr(path, name);
	vt_getxattr_err(path, name, -ENODATA);
	vt_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful setxattr/getxattr operations on various inode types.
 */
static void test_xattr_inode(struct vt_env *vt_env)
{
	int fd;
	size_t sz;
	const char *name = "user.digits";
	const char *value = "0123456789";
	const size_t valsz = strlen(value);
	char buf[80] = "";
	const char *path1 = vt_new_path_unique(vt_env);
	const char *path2 = vt_new_path_under(vt_env, path1);
	const char *path3 = vt_new_path_under(vt_env, path1);

	vt_mkdir(path1, 0700);
	vt_open(path2, O_CREAT | O_RDWR, 0600, &fd);
	vt_symlink(path1, path3);

	vt_setxattr(path1, name, value, valsz, 0);
	vt_getxattr(path1, name, NULL, 0, &sz);
	vt_expect_eq(sz, valsz);
	vt_getxattr(path1, name, buf, sizeof(buf), &sz);
	vt_expect_eq(sz, valsz);
	vt_expect_eqm(value, buf, valsz);

	vt_setxattr(path2, name, value, valsz, 0);
	vt_getxattr(path2, name, NULL, 0, &sz);
	vt_expect_eq(sz, valsz);
	vt_getxattr(path2, name, buf, sizeof(buf), &sz);
	vt_expect_eq(sz, valsz);
	vt_expect_eqm(value, buf, valsz);

	vt_setxattr(path3, name, value, valsz, 0);
	vt_getxattr(path3, name, NULL, 0, &sz);
	vt_expect_eq(sz, valsz);
	vt_getxattr(path3, name, buf, sizeof(buf), &sz);
	vt_expect_eq(sz, valsz);
	vt_expect_eqm(value, buf, valsz);

	vt_close(fd);
	vt_unlink(path3);
	vt_unlink(path2);
	vt_rmdir(path1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful setxattr/removexattr operations to change CTIME only
 */
static void test_xattr_ctime(struct vt_env *vt_env)
{
	int fd;
	size_t sz;
	struct stat st1, st2;
	const char *name = "user.abc";
	const char *value = "ABCDEF-ABCDEF-ABCDEF-ABCDEF-ABCDEF";
	const char *path = vt_new_path_unique(vt_env);

	vt_open(path, O_CREAT | O_RDWR, 0700, &fd);
	vt_fstat(fd, &st1);
	vt_fsetxattr(fd, name, value, strlen(value), 0);
	vt_fstat(fd, &st2);
	vt_expect_mtime_eq(&st1, &st2);
	vt_expect_ctime_ge(&st1, &st2);

	vt_fstat(fd, &st1);
	vt_fgetxattr(fd, name, NULL, 0, &sz);
	vt_fstat(fd, &st2);
	vt_expect_mtime_eq(&st1, &st2);
	vt_expect_ctime_eq(&st1, &st2);

	vt_fstat(fd, &st1);
	vt_fremovexattr(fd, name);
	vt_fstat(fd, &st2);
	vt_expect_mtime_eq(&st1, &st2);
	vt_expect_ctime_ge(&st1, &st2);

	vt_close(fd);
	vt_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful setxattr operations with implicit/explicit rename
 */
static void test_xattr_replace(struct vt_env *vt_env)
{
	int fd;
	size_t sz;
	const char *name = "user.xzy";
	const char *val1 = "0123456789";
	const char *val2 = "ABCDEFGHIJKLMNOPQRSTUVXYZ";
	const char *val3 = "abcdefghijklmnopqrstuvwxyz0123456789";
	const char *path = vt_new_path_unique(vt_env);
	char buf[256] = "";

	vt_open(path, O_CREAT | O_RDWR, 0700, &fd);
	vt_fsetxattr(fd, name, val1, strlen(val1), 0);
	vt_fgetxattr(fd, name, NULL, 0, &sz);
	vt_expect_eq(sz, strlen(val1));
	vt_fgetxattr(fd, name, buf, sizeof(buf) - 1, &sz);
	vt_expect_eq(sz, strlen(val1));
	vt_expect_eqm(buf, val1, sz);

	vt_fsetxattr(fd, name, val2, strlen(val2), 0);
	vt_fgetxattr(fd, name, NULL, 0, &sz);
	vt_expect_eq(sz, strlen(val2));
	vt_fgetxattr(fd, name, buf, sizeof(buf) - 1, &sz);
	vt_expect_eq(sz, strlen(val2));
	vt_expect_eqm(buf, val2, sz);

	vt_fsetxattr(fd, name, val3, strlen(val3), XATTR_REPLACE);
	vt_fgetxattr(fd, name, NULL, 0, &sz);
	vt_expect_eq(sz, strlen(val3));
	vt_fgetxattr(fd, name, buf, sizeof(buf) - 1, &sz);
	vt_expect_eq(sz, strlen(val3));
	vt_expect_eqm(buf, val3, sz);

	vt_fremovexattr(fd, name);
	vt_close(fd);
	vt_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTEST(test_xattr_simple),
	VT_DEFTEST(test_xattr_inode),
	VT_DEFTEST(test_xattr_ctime),
	VT_DEFTEST(test_xattr_replace),
};

const struct vt_tests vt_test_xattr = VT_DEFTESTS(vt_local_tests);

