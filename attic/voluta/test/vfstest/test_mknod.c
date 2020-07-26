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
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include "vfstest.h"


static void fill_name(char *name, size_t lim, size_t idx)
{
	snprintf(name, lim, "%061lx", idx);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful mkfifo(3p)/mkfifoat(3p)
 */
static void test_mkfifo(struct vt_env *vt_env)
{
	struct stat st;
	const char *path = vt_new_path_unique(vt_env);

	vt_mkfifo(path, S_IFIFO | 0600);
	vt_stat(path, &st);
	vt_expect_true(S_ISFIFO(st.st_mode));
	vt_expect_eq(st.st_nlink, 1);
	vt_expect_eq(st.st_size, 0);
	vt_unlink(path);
}

static void test_mkfifoat(struct vt_env *vt_env)
{
	int dfd;
	size_t i, count = 1024;
	struct stat st;
	char name[VOLUTA_NAME_MAX + 1];
	const char *path = vt_new_path_unique(vt_env);

	vt_mkdir(path, 0700);
	vt_open(path, O_DIRECTORY | O_RDONLY, 0, &dfd);
	for (i = 0; i < count; ++i) {
		fill_name(name, sizeof(name), i);
		vt_mkfifoat(dfd, name, S_IFIFO | 0600);
		vt_fstatat(dfd, name, &st, 0);
		vt_expect_true(S_ISFIFO(st.st_mode));
		vt_expect_eq(st.st_nlink, 1);
		vt_expect_eq(st.st_size, 0);
	}
	for (i = 0; i < count; ++i) {
		fill_name(name, sizeof(name), i);
		vt_unlinkat(dfd, name, 0);
	}
	vt_close(dfd);
	vt_rmdir(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTEST(test_mkfifo),
	VT_DEFTEST(test_mkfifoat),
};

const struct vt_tests vt_test_mknod = VT_DEFTESTS(vt_local_tests);

