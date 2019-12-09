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
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "sanity.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects truncate(3p) a regular file named by path to have a size which
 * shall be equal to length bytes.
 */
static void test_truncate_basic(struct voluta_t_ctx *t_ctx)
{
	int fd;
	size_t i, nwr, cnt = 100;
	loff_t off;
	struct stat st;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path, 0600, &fd);
	for (i = 0; i < cnt; ++i) {
		voluta_t_write(fd, path, strlen(path), &nwr);
	}
	for (i = cnt; i > 0; i--) {
		off = (loff_t)(19 * i);
		voluta_t_ftruncate(fd, off);
		voluta_t_fstat(fd, &st);
		voluta_t_expect_eq(st.st_size, off);
	}
	for (i = 0; i < cnt; i++) {
		off = (loff_t)(1811 * i);
		voluta_t_ftruncate(fd, off);
		voluta_t_fstat(fd, &st);
		voluta_t_expect_eq(st.st_size, off);
	}
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects truncate(3p) to create zeros at the truncated tail-range.
 */
static void test_truncate_tail_(struct voluta_t_ctx *t_ctx,
				loff_t base_off,
				size_t data_sz, size_t tail_sz)
{
	int fd;
	size_t nwr, nrd;
	loff_t off, off2;
	void *buf1, *buf2;
	struct stat st;
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf1 = voluta_t_new_buf_rands(t_ctx, data_sz);
	buf2 = voluta_t_new_buf_zeros(t_ctx, data_sz);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_pwrite(fd, buf1, data_sz, base_off, &nwr);
	voluta_t_expect_eq(data_sz, nwr);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, base_off + (loff_t)data_sz);
	off = base_off + (loff_t)(data_sz - tail_sz);
	voluta_t_ftruncate(fd, off);
	off2 = off + (loff_t)data_sz;
	voluta_t_ftruncate(fd, off2);
	voluta_t_pread(fd, buf1, data_sz, off, &nrd);
	voluta_t_expect_eq(data_sz, nrd);
	voluta_t_expect_eqm(buf1, buf2, data_sz);

	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_truncate_tail(struct voluta_t_ctx *t_ctx)
{
	test_truncate_tail_(t_ctx, 0, VOLUTA_MEGA, VOLUTA_BK_SIZE);
	test_truncate_tail_(t_ctx, 0, VOLUTA_MEGA, 1);
	test_truncate_tail_(t_ctx, 0, VOLUTA_MEGA, 11);
	test_truncate_tail_(t_ctx, 1, VOLUTA_MEGA + 111,
			    (7 * VOLUTA_BK_SIZE) - 7);
	test_truncate_tail_(t_ctx, VOLUTA_MEGA - 1,
			    VOLUTA_MEGA + 2, VOLUTA_MEGA / 2);
	test_truncate_tail_(t_ctx, VOLUTA_GIGA - 11,
			    VOLUTA_MEGA + 111, VOLUTA_MEGA / 3);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects truncate(3p) to create zeros at extended area without written data
 */
static void test_truncate_extend_(struct voluta_t_ctx *t_ctx,
				  loff_t base_off, size_t data_sz)
{
	int fd;
	size_t nwr, nrd;
	loff_t off, off2;
	void *buf1, *buf2;
	const char *path;
	struct stat st;

	path = voluta_t_new_path_unique(t_ctx);
	buf1 = voluta_t_new_buf_rands(t_ctx, data_sz);
	buf2 = voluta_t_new_buf_zeros(t_ctx, data_sz);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_pwrite(fd, buf1, data_sz, base_off, &nwr);
	voluta_t_expect_eq(data_sz, nwr);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, base_off + (loff_t)data_sz);
	off = base_off + (loff_t)(2 * data_sz);
	voluta_t_ftruncate(fd, off);
	off2 = base_off + (loff_t)(data_sz);
	voluta_t_pread(fd, buf1, data_sz, off2, &nrd);
	voluta_t_expect_eq(data_sz, nrd);
	voluta_t_expect_eqm(buf1, buf2, data_sz);

	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_truncate_extend(struct voluta_t_ctx *t_ctx)
{
	test_truncate_extend_(t_ctx, 0, VOLUTA_BK_SIZE);
	test_truncate_extend_(t_ctx, 1, VOLUTA_BK_SIZE);
	test_truncate_extend_(t_ctx, VOLUTA_BK_SIZE - 11,
			      11 * VOLUTA_BK_SIZE);
	test_truncate_extend_(t_ctx, 0, VOLUTA_MEGA);
	test_truncate_extend_(t_ctx, 1, VOLUTA_MEGA);
	test_truncate_extend_(t_ctx, VOLUTA_MEGA - 1, VOLUTA_MEGA + 2);
	test_truncate_extend_(t_ctx, VOLUTA_GIGA - 11, VOLUTA_MEGA + 111);
	test_truncate_extend_(t_ctx, (11 * VOLUTA_GIGA) - 111,
			      VOLUTA_MEGA + 1111);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful truncate(3p) to yield zero bytes
 */
static void test_truncate_zeros_(struct voluta_t_ctx *t_ctx, loff_t off,
				 size_t len)
{
	int fd;
	size_t nrd;
	uint8_t byte = 1;
	loff_t end = off + (loff_t)len;
	struct stat st;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0700, &fd);
	voluta_t_ftruncate(fd, end);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, end);
	voluta_t_pread(fd, &byte, 1, off, &nrd);
	voluta_t_expect_eq(nrd, 1);
	voluta_t_expect_eq(byte, 0);
	voluta_t_pread(fd, &byte, 1, end - 1, &nrd);
	voluta_t_expect_eq(nrd, 1);
	voluta_t_expect_eq(byte, 0);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, end);
	voluta_t_expect_eq(st.st_blocks, 0);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_truncate_zeros(struct voluta_t_ctx *t_ctx)
{
	test_truncate_zeros_(t_ctx, 0, 2);
	test_truncate_zeros_(t_ctx, 0, VOLUTA_BK_SIZE);
	test_truncate_zeros_(t_ctx, 1, VOLUTA_BK_SIZE);
	test_truncate_zeros_(t_ctx, 11, VOLUTA_BK_SIZE / 11);
	test_truncate_zeros_(t_ctx, VOLUTA_MEGA, VOLUTA_MEGA);
	test_truncate_zeros_(t_ctx, VOLUTA_MEGA - 1, VOLUTA_MEGA + 3);
	test_truncate_zeros_(t_ctx, VOLUTA_GIGA - 11, VOLUTA_MEGA + 111);
	test_truncate_zeros_(t_ctx, VOLUTA_TERA - 111, VOLUTA_GIGA + 1111);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_truncate_basic),
	VOLUTA_T_DEFTEST(test_truncate_tail),
	VOLUTA_T_DEFTEST(test_truncate_extend),
	VOLUTA_T_DEFTEST(test_truncate_zeros),
};

const struct voluta_t_tests
voluta_t_test_truncate_io = VOLUTA_T_DEFTESTS(t_local_tests);
