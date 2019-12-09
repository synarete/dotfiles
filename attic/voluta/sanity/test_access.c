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
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "sanity.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects access(3p) to return 0 on root-dir.
 */
static void test_access_rootdir(struct voluta_t_ctx *t_ctx)
{
	const char *path = voluta_t_new_path_name(t_ctx, "/");

	voluta_t_access(path, R_OK | W_OK | X_OK);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects access(3p) to return ENOENT if a component of path does not name an
 * existing file or path is an empty string.
 */
static void test_access_noent(struct voluta_t_ctx *t_ctx)
{
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_under(t_ctx, path0);
	const char *path2 = voluta_t_new_path_under(t_ctx, path1);

	voluta_t_mkdir(path0, 0755);
	voluta_t_access(path0, F_OK);
	voluta_t_access(path0, X_OK);
	voluta_t_access(path0, F_OK | X_OK);

	voluta_t_access_err(path1, R_OK, -ENOENT);
	voluta_t_access_err(path1, F_OK, -ENOENT);
	voluta_t_access_err(path1, F_OK | X_OK, -ENOENT);

	voluta_t_access_err(path2, R_OK, -ENOENT);
	voluta_t_access_err(path2, F_OK, -ENOENT);
	voluta_t_access_err(path2, F_OK | X_OK, -ENOENT);
	voluta_t_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects access(3p) to return EINVAL if the value of the amode argument is
 * invalid.
 */
static void test_access_inval(struct voluta_t_ctx *t_ctx)
{
	int fd, mode = R_OK | W_OK | X_OK | F_OK;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path, 0644, &fd);
	voluta_t_access_err(path, ~mode, -EINVAL);
	voluta_t_unlink(path);
	voluta_t_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects access(3p) to return EACCES when a component of the path prefix
 * denies search permission
 */
static void test_access_prefix(struct voluta_t_ctx *t_ctx)
{
	int fd, mode = R_OK;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_under(t_ctx, path0);
	const char *path2 = voluta_t_new_path_under(t_ctx, path1);
	const char *path3 = voluta_t_new_path_under(t_ctx, path2);

	voluta_t_mkdir(path0, 0750);
	voluta_t_mkdir(path1, 0750);
	voluta_t_mkdir(path2, 0750);
	voluta_t_creat(path3, 0600, &fd);
	voluta_t_access(path3, mode);
	voluta_t_chmod(path2, 0200);
	voluta_t_suspends(t_ctx, 3);
	voluta_t_access_err(path3, mode, -EACCES);
	voluta_t_chmod(path2, 0700);
	voluta_t_suspends(t_ctx, 3);
	voluta_t_access(path3, mode);

	voluta_t_unlink(path3);
	voluta_t_close(fd);
	voluta_t_rmdir(path2);
	voluta_t_rmdir(path1);
	voluta_t_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_access_rootdir),
	VOLUTA_T_DEFTEST(test_access_noent),
	VOLUTA_T_DEFTEST(test_access_inval),
	VOLUTA_T_DEFTEST(test_access_prefix)
};
const struct voluta_t_tests
voluta_t_test_access = VOLUTA_T_DEFTESTS(t_local_tests);

