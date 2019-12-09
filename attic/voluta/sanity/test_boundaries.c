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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "sanity.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects read-write data-consistency when I/O at block boundaries
 */
static void test_boundaries_(struct voluta_t_ctx *t_ctx, size_t bsz)
{
	int fd;
	size_t nwr, nrd;
	uint8_t byte1, byte2, mark;
	void *buf1, *buf2;
	const loff_t off = (loff_t)bsz;
	const loff_t off_p1 = off + 1;
	const loff_t off_p2 = off + 2;
	const loff_t off_m1 = off - 1;
	const loff_t off_t2 = off * 2;
	const loff_t off_d2 = off / 2;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);

	buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
	buf2 = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_pwrite(fd, buf1, bsz, 0, &nwr);
	voluta_t_pwrite(fd, buf2, bsz, off, &nwr);

	mark = 1;
	voluta_t_pwrite(fd, &mark, 1, 0, &nwr);
	voluta_t_pread(fd, &byte1, 1, 0, &nrd);
	voluta_t_pread(fd, &byte2, 1, off_t2, &nrd);
	voluta_t_expect_eq(byte1, mark);
	voluta_t_expect_eq(nrd, 0);

	mark = 2;
	voluta_t_pwrite(fd, &mark, 1, off_m1, &nwr);
	voluta_t_pread(fd, &byte1, 1, off_m1, &nrd);
	voluta_t_pread(fd, &byte2, 1, off_t2, &nrd);
	voluta_t_expect_eq(byte1, mark);
	voluta_t_expect_eq(nrd, 0);

	mark = 3;
	voluta_t_pwrite(fd, &mark, 1, off_p1, &nwr);
	voluta_t_pread(fd, &byte1, 1, off_p1, &nrd);
	voluta_t_pread(fd, &byte2, 1, off_t2, &nrd);
	voluta_t_expect_eq(byte1, mark);
	voluta_t_expect_eq(nrd, 0);

	mark = 4;
	voluta_t_pwrite(fd, &mark, 1, off_p2, &nwr);
	voluta_t_pread(fd, &byte1, 1, off_p2, &nrd);
	voluta_t_pread(fd, &byte2, 1, off_t2, &nrd);
	voluta_t_expect_eq(byte1, mark);
	voluta_t_expect_eq(nrd, 0);

	mark = 5;
	voluta_t_pwrite(fd, &mark, 1, off_d2, &nwr);
	voluta_t_pread(fd, &byte1, 1, off_d2, &nrd);
	voluta_t_pread(fd, &byte2, 1, off_t2, &nrd);
	voluta_t_expect_eq(byte1, mark);
	voluta_t_expect_eq(nrd, 0);

	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_boundaries_aligned(struct voluta_t_ctx *t_ctx)
{
	test_boundaries_(t_ctx, VOLUTA_KILO);
	test_boundaries_(t_ctx, VOLUTA_BK_SIZE);
	test_boundaries_(t_ctx, 64 * VOLUTA_KILO);
	test_boundaries_(t_ctx, VOLUTA_MEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_boundaries_aligned),
};

const struct voluta_t_tests
voluta_t_test_boundaries = VOLUTA_T_DEFTESTS(t_local_tests);
