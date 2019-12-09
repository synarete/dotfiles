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
#include <stdio.h>
#include <string.h>
#include "sanity.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests read-write data-consistency over sparse file.
 */
static void test_sparse_simple(struct voluta_t_ctx *t_ctx)
{
	int fd;
	loff_t pos = -1;
	size_t num, num2;
	size_t nsz, nwr, nrd;
	const size_t cnt = 7717;
	const size_t step = 524287;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	for (size_t i = 0; i < cnt; ++i) {
		num = (i * step);
		pos = (loff_t)num;
		nsz = sizeof(num);
		voluta_t_pwrite(fd, &num, nsz, pos, &nwr);
		voluta_t_expect_eq(nwr, nsz);
	}
	voluta_t_close(fd);

	voluta_t_open(path, O_RDONLY, 0, &fd);
	for (size_t j = 0; j < cnt; ++j) {
		num = (j * step);
		pos = (loff_t)num;
		nsz = sizeof(num2);
		voluta_t_pread(fd, &num2, nsz, pos, &nrd);
		voluta_t_expect_eq(nrd, nsz);
		voluta_t_expect_eq(num, num2);
	}
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests read-write data-consistency over sparse file with syncs over same file.
 */
static void test_sparse_rdwr(struct voluta_t_ctx *t_ctx)
{
	int fd = -1;
	loff_t pos = -1;
	size_t num, num2;
	size_t nsz, nwr, nrd;
	const size_t cnt  = 127;
	const size_t step = 524287;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_close(fd);
	for (size_t i = 0; i < 17; ++i) {
		for (size_t j = 0; j < cnt; ++j) {
			voluta_t_open(path, O_RDWR, 0, &fd);
			num = i + (j * step);
			pos = (loff_t)num;
			nsz = sizeof(num);
			voluta_t_pwrite(fd, &num, nsz, pos, &nwr);
			voluta_t_expect_eq(nwr, nsz);
			voluta_t_fdatasync(fd);
			voluta_t_pread(fd, &num2, nsz, pos, &nrd);
			voluta_t_expect_eq(nrd, nsz);
			voluta_t_expect_eq(num, num2);
			voluta_t_close(fd);
		}
	}
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests read-write data-consistency over sparse file with overwrites.
 */
static void
test_sparse_overwrite_(struct voluta_t_ctx *t_ctx, loff_t base_off)
{
	int fd;
	loff_t off;
	char *path;
	uint8_t byte, *buf1, *buf2, *buf3;
	size_t i, nwr, nrd;
	loff_t offs[] = {
		737717, 280411, 10007, 31033, 42043, 53113, 161881, 375533,
		86767, 97171, 75353, 611999, 1108007, 64601, 1272211, 20323
	};
	const size_t len1 = 10037;
	const size_t len2 = 10039;
	const size_t noffs = VOLUTA_ARRAY_SIZE(offs);

	buf1 = voluta_t_new_buf_rands(t_ctx, len1);
	buf2 = voluta_t_new_buf_rands(t_ctx, len2);
	buf3 = voluta_t_new_buf_rands(t_ctx, len1 + len2);
	path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	for (i = 0; i < noffs; ++i) {
		off = base_off + offs[i];
		voluta_t_pwrite(fd, buf1, len1, off, &nwr);
		voluta_t_expect_eq(len1, nwr);
	}
	for (i = 0; i < noffs; ++i) {
		off = base_off + offs[i] + 1;
		voluta_t_pwrite(fd, buf2, len2, off, &nwr);
		voluta_t_expect_eq(len2, nwr);
	}
	for (i = 0; i < noffs; ++i) {
		off = base_off + offs[i];
		voluta_t_pread(fd, &byte, 1, off, &nrd);
		voluta_t_expect_eq(buf1[0], byte);
		voluta_t_pread(fd, buf3, len2, off + 1, &nrd);
		voluta_t_expect_eqm(buf2, buf3, len2);
	}
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_sparse_overwrite(struct voluta_t_ctx *t_ctx)
{
	test_sparse_overwrite_(t_ctx, 0);
	test_sparse_overwrite_(t_ctx, 1);
	test_sparse_overwrite_(t_ctx, VOLUTA_MEGA - 2);
	test_sparse_overwrite_(t_ctx, VOLUTA_GIGA - 3);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_sparse_simple),
	VOLUTA_T_DEFTEST(test_sparse_rdwr),
	VOLUTA_T_DEFTEST(test_sparse_overwrite),
};

const struct voluta_t_tests
voluta_t_test_sparse_file = VOLUTA_T_DEFTESTS(t_local_tests);
