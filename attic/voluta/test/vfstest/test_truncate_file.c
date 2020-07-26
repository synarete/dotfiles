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
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "vfstest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects truncate(3p) a regular file named by path to have a size which
 * shall be equal to length bytes.
 */
static void test_truncate_basic(struct vt_env *vt_env)
{
	int fd;
	size_t i, nwr, cnt = 100;
	loff_t off;
	struct stat st;
	const char *path = vt_new_path_unique(vt_env);

	vt_creat(path, 0600, &fd);
	for (i = 0; i < cnt; ++i) {
		vt_write(fd, path, strlen(path), &nwr);
	}
	for (i = cnt; i > 0; i--) {
		off = (loff_t)(19 * i);
		vt_ftruncate(fd, off);
		vt_fstat(fd, &st);
		vt_expect_eq(st.st_size, off);
	}
	for (i = 0; i < cnt; i++) {
		off = (loff_t)(1811 * i);
		vt_ftruncate(fd, off);
		vt_fstat(fd, &st);
		vt_expect_eq(st.st_size, off);
	}
	vt_close(fd);
	vt_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects truncate(3p) to create zeros at the truncated tail-range.
 */
static void test_truncate_tail_(struct vt_env *vt_env, loff_t base_off,
				size_t data_sz, size_t tail_sz)
{
	int fd;
	loff_t off, off2;
	struct stat st;
	void *buf1 = vt_new_buf_rands(vt_env, data_sz);
	void *buf2 = vt_new_buf_zeros(vt_env, data_sz);
	const char *path = vt_new_path_unique(vt_env);

	vt_open(path, O_CREAT | O_RDWR, 0600, &fd);
	vt_pwriten(fd, buf1, data_sz, base_off);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_size, base_off + (loff_t)data_sz);
	off = base_off + (loff_t)(data_sz - tail_sz);
	vt_ftruncate(fd, off);
	off2 = off + (loff_t)data_sz;
	vt_ftruncate(fd, off2);
	vt_preadn(fd, buf1, data_sz, off);
	vt_expect_eqm(buf1, buf2, data_sz);

	vt_close(fd);
	vt_unlink(path);
}

static void test_truncate_tail(struct vt_env *vt_env)
{
	test_truncate_tail_(vt_env, 0, VT_UMEGA, VT_BK_SIZE);
	test_truncate_tail_(vt_env, 0, VT_UMEGA, 1);
	test_truncate_tail_(vt_env, 0, VT_UMEGA, 11);
	test_truncate_tail_(vt_env, 1, VT_UMEGA + 111, (7 * VT_BK_SIZE) - 7);
	test_truncate_tail_(vt_env, VT_MEGA - 1, VT_UMEGA + 2, VT_UMEGA / 2);
	test_truncate_tail_(vt_env, VT_GIGA - 11,
			    VT_UMEGA + 111, VT_UMEGA / 3);
	test_truncate_tail_(vt_env, VT_FILESIZE_MAX / 3,
			    VT_UMEGA + 3, VT_UMEGA / 3);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects truncate(3p) to create zeros at extended area without written data
 */
static void test_truncate_extend_(struct vt_env *vt_env,
				  loff_t base_off, size_t data_sz)
{
	int fd;
	loff_t off, off2;
	struct stat st;
	void *buf1 = vt_new_buf_rands(vt_env, data_sz);
	void *buf2 = vt_new_buf_zeros(vt_env, data_sz);
	const char *path = vt_new_path_unique(vt_env);

	vt_open(path, O_CREAT | O_RDWR, 0600, &fd);
	vt_pwriten(fd, buf1, data_sz, base_off);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_size, base_off + (loff_t)data_sz);
	off = base_off + (loff_t)(2 * data_sz);
	vt_ftruncate(fd, off);
	off2 = base_off + (loff_t)(data_sz);
	vt_preadn(fd, buf1, data_sz, off2);
	vt_expect_eqm(buf1, buf2, data_sz);
	vt_close(fd);
	vt_unlink(path);
}

static void test_truncate_extend(struct vt_env *vt_env)
{
	test_truncate_extend_(vt_env, 0, VT_BK_SIZE);
	test_truncate_extend_(vt_env, 1, VT_BK_SIZE);
	test_truncate_extend_(vt_env, VT_BK_SIZE - 11, 11 * VT_BK_SIZE);
	test_truncate_extend_(vt_env, 0, VT_UMEGA);
	test_truncate_extend_(vt_env, 1, VT_UMEGA);
	test_truncate_extend_(vt_env, VT_UMEGA - 1, VT_UMEGA + 2);
	test_truncate_extend_(vt_env, VT_UGIGA - 11, VT_UMEGA + 111);
	test_truncate_extend_(vt_env, (11 * VT_UGIGA) - 111, VT_UMEGA + 1111);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful truncate(3p) to yield zero bytes
 */
static void test_truncate_zeros_(struct vt_env *vt_env, loff_t off,
				 size_t len)
{
	int fd;
	size_t nrd;
	uint8_t byte = 1;
	loff_t end = off + (loff_t)len;
	struct stat st;
	const char *path = vt_new_path_unique(vt_env);

	vt_open(path, O_CREAT | O_RDWR, 0700, &fd);
	vt_ftruncate(fd, end);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_size, end);
	vt_pread(fd, &byte, 1, off, &nrd);
	vt_expect_eq(nrd, 1);
	vt_expect_eq(byte, 0);
	vt_pread(fd, &byte, 1, end - 1, &nrd);
	vt_expect_eq(nrd, 1);
	vt_expect_eq(byte, 0);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_size, end);
	vt_expect_eq(st.st_blocks, 0);
	vt_close(fd);
	vt_unlink(path);
}

static void test_truncate_zeros(struct vt_env *vt_env)
{
	test_truncate_zeros_(vt_env, 0, 2);
	test_truncate_zeros_(vt_env, 0, VT_BK_SIZE);
	test_truncate_zeros_(vt_env, 1, VT_BK_SIZE);
	test_truncate_zeros_(vt_env, 11, VT_BK_SIZE / 11);
	test_truncate_zeros_(vt_env, VT_UMEGA, VT_UMEGA);
	test_truncate_zeros_(vt_env, VT_UMEGA - 1, VT_UMEGA + 3);
	test_truncate_zeros_(vt_env, VT_UGIGA - 11, VT_UMEGA + 111);
	test_truncate_zeros_(vt_env, VT_UTERA - 111, VT_UGIGA + 1111);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTEST(test_truncate_basic),
	VT_DEFTEST(test_truncate_tail),
	VT_DEFTEST(test_truncate_extend),
	VT_DEFTEST(test_truncate_zeros),
};

const struct vt_tests vt_test_truncate_io =
	VT_DEFTESTS(vt_local_tests);
