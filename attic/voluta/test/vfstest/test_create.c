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
#include <stdlib.h>
#include <unistd.h>
#include "vfstest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects success for simple creat(3p)/unlink(3p) X N
 */
static void test_create_simple(struct vt_env *vt_env)
{
	int fd, n = 64;
	const char *path = vt_new_path_unique(vt_env);

	for (int i = 0; i < n; ++i) {
		vt_creat(path, 0600, &fd);
		vt_close(fd);
		vt_unlink(path);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects success for sequence of creat(3p)/unlink(3p) of regular files.
 */
static void test_create_unlink_(struct vt_env *vt_env, int cnt)
{
	int fd, i;
	char *path0, *path1;

	path0 = vt_new_path_unique(vt_env);
	vt_mkdir(path0, 0700);
	for (i = 0; i < cnt; ++i) {
		path1 = vt_new_pathf(vt_env, path0, "%lu", i);
		vt_creat(path1, 0600, &fd);
		vt_close(fd);
	}
	for (i = 0; i < cnt; ++i) {
		path1 = vt_new_pathf(vt_env, path0, "%lu", i);
		vt_unlink(path1);
	}
	vt_rmdir(path0);
}

static void test_create_unlink(struct vt_env *vt_env)
{
	test_create_unlink_(vt_env, 1);
	test_create_unlink_(vt_env, 2);
	test_create_unlink_(vt_env, 8);
	test_create_unlink_(vt_env, 128);
}

static void test_create_unlink_many(struct vt_env *vt_env)
{
	test_create_unlink_(vt_env, 4096);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTEST(test_create_simple),
	VT_DEFTEST(test_create_unlink),
	VT_DEFTEST(test_create_unlink_many),
};

const struct vt_tests vt_test_create = VT_DEFTESTS(vt_local_tests);
