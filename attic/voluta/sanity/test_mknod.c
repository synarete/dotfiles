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
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include "sanity.h"


static void fill_name(char *name, size_t lim, size_t idx)
{
	snprintf(name, lim, "%061lx", idx);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful mkfifo(3p)/mkfifoat(3p)
 */
static void test_mkfifo(struct voluta_t_ctx *t_ctx)
{
	struct stat st;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_mkfifo(path, S_IFIFO | 0600);
	voluta_t_stat(path, &st);
	voluta_t_expect_true(S_ISFIFO(st.st_mode));
	voluta_t_expect_eq(st.st_nlink, 1);
	voluta_t_expect_eq(st.st_size, 0);
	voluta_t_unlink(path);
}

static void test_mkfifoat(struct voluta_t_ctx *t_ctx)
{
	int dfd;
	size_t i, count = 1024;
	struct stat st;
	char name[VOLUTA_NAME_MAX + 1];
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_mkdir(path, 0700);
	voluta_t_open(path, O_DIRECTORY | O_RDONLY, 0, &dfd);
	for (i = 0; i < count; ++i) {
		fill_name(name, sizeof(name), i);
		voluta_t_mkfifoat(dfd, name, S_IFIFO | 0600);
		voluta_t_fstatat(dfd, name, &st, 0);
		voluta_t_expect_true(S_ISFIFO(st.st_mode));
		voluta_t_expect_eq(st.st_nlink, 1);
		voluta_t_expect_eq(st.st_size, 0);
	}
	for (i = 0; i < count; ++i) {
		fill_name(name, sizeof(name), i);
		voluta_t_unlinkat(dfd, name, 0);
	}
	voluta_t_close(dfd);
	voluta_t_rmdir(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_mkfifo),
	VOLUTA_T_DEFTEST(test_mkfifoat),
};

const struct voluta_t_tests
voluta_t_test_mknod = VOLUTA_T_DEFTESTS(t_local_tests);

