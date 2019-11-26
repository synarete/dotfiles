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
#include <string.h>
#include "fstests.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects ioctl(FICLONE) to successfully clone entire file range between two
 * files.
 */
static void test_clone_file_range_(struct voluta_t_ctx *t_ctx,
				   size_t bsz)
{
	int fd1, fd2;
	size_t nwr, nrd;
	void *data1, *data2;
	struct stat st1, st2;
	const char *path1 = voluta_t_new_path_unique(t_ctx);
	const char *path2 = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path1, O_CREAT | O_RDWR, 0600, &fd1);
	voluta_t_open(path2, O_CREAT | O_RDWR, 0600, &fd2);
	data1 = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_pwrite(fd1, data1, bsz, 0, &nwr);
	voluta_t_expect_eq(bsz, nwr);
	voluta_t_fstat(fd1, &st1);
	voluta_t_expect_eq(bsz, st1.st_size);
	voluta_t_ioctl_ficlone(fd2, fd1);
	voluta_t_fstat(fd2, &st2);
	voluta_t_expect_eq(bsz, st2.st_size);
	data2 = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_pread(fd1, data2, bsz, 0, &nrd);
	voluta_t_expect_eq(bsz, nrd);
	voluta_t_expect_eqm(data1, data2, bsz);
	voluta_t_expect_eq(st1.st_blocks, st2.st_blocks);
	voluta_t_close(fd1);
	voluta_t_unlink(path1);
	voluta_t_close(fd2);
	voluta_t_unlink(path2);
}

static void test_clone_file_range_small(struct voluta_t_ctx *t_ctx)
{
	test_clone_file_range_(t_ctx, VOLUTA_BK_SIZE);
	test_clone_file_range_(t_ctx, 8 * VOLUTA_BK_SIZE);
}

static void test_clone_file_range_large(struct voluta_t_ctx *t_ctx)
{
	test_clone_file_range_(t_ctx, VOLUTA_MEGA);
	test_clone_file_range_(t_ctx, 8 * VOLUTA_MEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTESTF(test_clone_file_range_small, VOLUTA_T_IO_CLONE),
	VOLUTA_T_DEFTESTF(test_clone_file_range_large, VOLUTA_T_IO_CLONE),
};

const struct voluta_t_tests
voluta_t_test_clone = VOLUTA_T_DEFTESTS(t_local_tests);
