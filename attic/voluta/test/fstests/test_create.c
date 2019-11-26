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
#include "fstests.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects success for simple creat(3p)/unlink(3p) X N
 */
static void test_create_simple(struct voluta_t_ctx *t_ctx)
{
	int fd, n = 64;
	const char *path = voluta_t_new_path_unique(t_ctx);

	for (int i = 0; i < n; ++i) {
		voluta_t_creat(path, 0600, &fd);
		voluta_t_close(fd);
		voluta_t_unlink(path);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects success for sequence of creat(3p)/unlink(3p) of regular files.
 */
static void test_create_unlink_(struct voluta_t_ctx *t_ctx, int cnt)
{
	int fd, i;
	char *path0, *path1;

	path0 = voluta_t_new_path_unique(t_ctx);
	voluta_t_mkdir(path0, 0700);
	for (i = 0; i < cnt; ++i) {
		path1 = voluta_t_new_pathf(t_ctx, path0, "%lu", i);
		voluta_t_creat(path1, 0600, &fd);
		voluta_t_close(fd);
	}
	for (i = 0; i < cnt; ++i) {
		path1 = voluta_t_new_pathf(t_ctx, path0, "%lu", i);
		voluta_t_unlink(path1);
	}
	voluta_t_rmdir(path0);
}

static void test_create_unlink(struct voluta_t_ctx *t_ctx)
{
	test_create_unlink_(t_ctx, 1);
	test_create_unlink_(t_ctx, 2);
	test_create_unlink_(t_ctx, 8);
	test_create_unlink_(t_ctx, 128);
}

static void test_create_unlink_many(struct voluta_t_ctx *t_ctx)
{
	test_create_unlink_(t_ctx, 4096);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_create_simple),
	VOLUTA_T_DEFTEST(test_create_unlink),
	VOLUTA_T_DEFTEST(test_create_unlink_many),
};

const struct voluta_t_tests
voluta_t_test_create = VOLUTA_T_DEFTESTS(t_local_tests);
