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
#include "fstests.h"


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects valid lseek(3p) with whence as SEEK_SET, SEEK_CUR and SEEK_END
 */
static void test_lseek_simple(struct voluta_t_ctx *t_ctx)
{
	int fd, o_flags = O_CREAT | O_RDWR;
	loff_t pos = -1;
	size_t nrd, nwr, bsz = VOLUTA_MEGA;
	uint8_t byte, *buf;
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_open(path, o_flags, 0600, &fd);
	voluta_t_write(fd, buf, bsz, &nwr);
	voluta_t_expect_eq(bsz, nwr);

	voluta_t_llseek(fd, 0, SEEK_SET, &pos);
	voluta_t_expect_eq(pos, 0);
	voluta_t_read(fd, &byte, 1, &nrd);
	voluta_t_expect_eq(1, nrd);
	voluta_t_expect_eq(buf[pos], byte);

	voluta_t_llseek(fd, 2, SEEK_CUR, &pos);
	voluta_t_expect_eq(pos, 3);
	voluta_t_read(fd, &byte, 1, &nrd);
	voluta_t_expect_eq(1, nrd);
	voluta_t_expect_eq(buf[pos], byte);

	voluta_t_llseek(fd, -1, SEEK_END, &pos);
	voluta_t_expect_eq(pos, bsz - 1);
	voluta_t_read(fd, &byte, 1, &nrd);
	voluta_t_expect_eq(1, nrd);
	voluta_t_expect_eq(buf[pos], byte);

	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects valid lseek(2) with SEEK_DATA
 */
static void test_lseek_data_(struct voluta_t_ctx *t_ctx, size_t bsz)
{
	int fd;
	loff_t from, off, pos = -1;
	size_t nrd, nwr;
	uint8_t byte, *buf1;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
	off = (loff_t)(bsz * 2);
	voluta_t_pwrite(fd, buf1, bsz, off, &nwr);
	voluta_t_expect_eq(bsz, nwr);

	from = off - 2;
	voluta_t_llseek(fd, from, SEEK_DATA, &pos);
	voluta_t_expect_eq(pos, off);
	voluta_t_read(fd, &byte, 1, &nrd);
	voluta_t_expect_eq(1, nrd);
	voluta_t_expect_eq(buf1[0], byte);
	voluta_t_pread(fd, &byte, 1, pos + 1, &nrd);
	voluta_t_expect_eq(1, nrd);
	voluta_t_expect_eq(buf1[1], byte);

	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_lseek_data(struct voluta_t_ctx *t_ctx)
{
	test_lseek_data_(t_ctx, VOLUTA_BK_SIZE);
	test_lseek_data_(t_ctx, 2 * VOLUTA_BK_SIZE);
	test_lseek_data_(t_ctx, VOLUTA_MEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects valid lseek(2) with SEEK_HOLE
 */
static void test_lseek_hole_(struct voluta_t_ctx *t_ctx, size_t bsz)
{
	int fd;
	loff_t from, off, pos = -1;
	size_t nrd, nwr;
	uint8_t byte, *buf1;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
	off = (loff_t)bsz;
	voluta_t_pwrite(fd, buf1, bsz, off, &nwr);
	voluta_t_expect_eq(bsz, nwr);
	off = (loff_t)(bsz * 100);
	voluta_t_pwrite(fd, buf1, bsz, off, &nwr);
	voluta_t_expect_eq(bsz, nwr);

	off = (loff_t)bsz;
	from = off - 1;
	voluta_t_llseek(fd, from, SEEK_HOLE, &pos);
	voluta_t_expect_eq(pos, from);
	voluta_t_read(fd, &byte, 1, &nrd);
	voluta_t_expect_eq(1, nrd);
	voluta_t_expect_eq(0, byte);

	from = (loff_t)(bsz * 2) - 2;
	voluta_t_llseek(fd, from, SEEK_HOLE, &pos);
	voluta_t_expect_eq(pos, (loff_t)(bsz * 2));
	voluta_t_pread(fd, &byte, 1, pos, &nrd);
	voluta_t_expect_eq(1, nrd);
	voluta_t_expect_eq(0, byte);

	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_lseek_hole(struct voluta_t_ctx *t_ctx)
{
	test_lseek_hole_(t_ctx, VOLUTA_BK_SIZE);
	test_lseek_hole_(t_ctx, 2 * VOLUTA_BK_SIZE);
	test_lseek_hole_(t_ctx, VOLUTA_MEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_lseek_simple),
	VOLUTA_T_DEFTESTF(test_lseek_data, VOLUTA_T_POSIX_EXTRA),
	VOLUTA_T_DEFTESTF(test_lseek_hole, VOLUTA_T_POSIX_EXTRA),
};

const struct voluta_t_tests
voluta_t_test_lseek = VOLUTA_T_DEFTESTS(t_local_tests);

