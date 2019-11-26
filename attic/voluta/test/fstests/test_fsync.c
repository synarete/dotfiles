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
#include <string.h>
#include "fstests.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects fsync(3p) to return 0 after regular file write/read operation.
 */
static void test_fsync_reg(struct voluta_t_ctx *t_ctx, loff_t base_off,
			   size_t bsz, loff_t step, size_t cnt)
{
	int fd;
	loff_t off;
	size_t nwr;
	char *buf1, *buf2 = voluta_t_new_buf_rands(t_ctx, bsz);
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	for (size_t i = 0; i < cnt; ++i) {
		off = base_off + ((loff_t)i  * step);
		buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
		voluta_t_pwrite(fd, buf1, bsz, off, &nwr);
		voluta_t_fsync(fd);
		voluta_t_pread(fd, buf2, bsz, off, &nwr);
		voluta_t_expect_eqm(buf1, buf2, bsz);
		buf2 = buf1;
	}
	voluta_t_close(fd);
	voluta_t_unlink(path);
}


static void test_fsync_reg_aligned(struct voluta_t_ctx *t_ctx)
{
	test_fsync_reg(t_ctx, 0, VOLUTA_KILO, VOLUTA_MEGA, 64);
	test_fsync_reg(t_ctx, VOLUTA_MEGA, VOLUTA_BK_SIZE, VOLUTA_GIGA,
		       64);
}

static void test_fsync_reg_unaligned(struct voluta_t_ctx *t_ctx)
{
	test_fsync_reg(t_ctx, 1, VOLUTA_KILO - 1, VOLUTA_MEGA + 1, 64);
	test_fsync_reg(t_ctx, VOLUTA_MEGA - 1,
		       3 * VOLUTA_KILO + 1, VOLUTA_GIGA - 1, 64);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects fsync(3p) to return 0 after directory operations.
 */
static void test_fsync_dir_nent(struct voluta_t_ctx *t_ctx, size_t cnt)
{
	int fd, dfd;
	const char *path1 = voluta_t_new_path_unique(t_ctx);
	const char *path2;

	voluta_t_mkdir(path1, 0700);
	voluta_t_open(path1, O_DIRECTORY | O_RDONLY, 0, &dfd);
	for (size_t i = 0; i < cnt; ++i) {
		path2 = voluta_t_new_pathf(t_ctx, path1, "%08x", i + 1);
		voluta_t_creat(path2, 0640, &fd);
		voluta_t_fsync(dfd);
		voluta_t_close(fd);
		voluta_t_fsync(dfd);
	}
	for (size_t j = 0; j < cnt; ++j) {
		path2 = voluta_t_new_pathf(t_ctx, path1, "%08x", j + 1);
		voluta_t_unlink(path2);
		voluta_t_fsync(dfd);
	}
	voluta_t_close(dfd);
	voluta_t_rmdir(path1);
}

static void test_fsync_dir(struct voluta_t_ctx *t_ctx)
{
	test_fsync_dir_nent(t_ctx, 1024);
	test_fsync_dir_nent(t_ctx, 4096);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects syncfs(2) to return 0
 */
static void test_fsync_syncfs(struct voluta_t_ctx *t_ctx)
{
	int fd;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_syncfs(fd);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_fsync_reg_aligned),
	VOLUTA_T_DEFTEST(test_fsync_reg_unaligned),
	VOLUTA_T_DEFTEST(test_fsync_dir),
	VOLUTA_T_DEFTEST(test_fsync_syncfs),
};

const struct voluta_t_tests
voluta_t_test_fsync = VOLUTA_T_DEFTESTS(t_local_tests);
