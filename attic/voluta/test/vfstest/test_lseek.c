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


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects valid lseek(3p) with whence as SEEK_SET, SEEK_CUR and SEEK_END
 */
static void test_lseek_simple(struct vt_env *vt_env)
{
	int fd, o_flags = O_CREAT | O_RDWR;
	loff_t pos = -1;
	size_t nrd, nwr, bsz = VT_UMEGA;
	uint8_t byte, *buf;
	const char *path = vt_new_path_unique(vt_env);

	buf = vt_new_buf_rands(vt_env, bsz);
	vt_open(path, o_flags, 0600, &fd);
	vt_write(fd, buf, bsz, &nwr);
	vt_expect_eq(bsz, nwr);

	vt_llseek(fd, 0, SEEK_SET, &pos);
	vt_expect_eq(pos, 0);
	vt_read(fd, &byte, 1, &nrd);
	vt_expect_eq(1, nrd);
	vt_expect_eq(buf[pos], byte);

	vt_llseek(fd, 2, SEEK_CUR, &pos);
	vt_expect_eq(pos, 3);
	vt_read(fd, &byte, 1, &nrd);
	vt_expect_eq(1, nrd);
	vt_expect_eq(buf[pos], byte);

	vt_llseek(fd, -1, SEEK_END, &pos);
	vt_expect_eq(pos, bsz - 1);
	vt_read(fd, &byte, 1, &nrd);
	vt_expect_eq(1, nrd);
	vt_expect_eq(buf[pos], byte);

	vt_close(fd);
	vt_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects valid lseek(2) with SEEK_DATA
 */
static void test_lseek_data_(struct vt_env *vt_env, size_t bsz)
{
	int fd;
	loff_t from, off, pos = -1;
	uint8_t byte;
	uint8_t *buf1 = vt_new_buf_rands(vt_env, bsz);
	const char *path = vt_new_path_unique(vt_env);

	vt_open(path, O_CREAT | O_RDWR, 0600, &fd);
	off = (loff_t)(bsz * 2);
	vt_pwriten(fd, buf1, bsz, off);

	from = off - 2;
	vt_llseek(fd, from, SEEK_DATA, &pos);
	vt_expect_eq(pos, off);
	vt_readn(fd, &byte, 1);
	vt_expect_eq(buf1[0], byte);
	vt_preadn(fd, &byte, 1, pos + 1);
	vt_expect_eq(buf1[1], byte);

	vt_close(fd);
	vt_unlink(path);
}

static void test_lseek_data(struct vt_env *vt_env)
{
	test_lseek_data_(vt_env, VT_BK_SIZE);
	test_lseek_data_(vt_env, 2 * VT_BK_SIZE);
	test_lseek_data_(vt_env, VT_UMEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects valid lseek(2) with SEEK_HOLE
 */
static void test_lseek_hole_(struct vt_env *vt_env, size_t bsz)
{
	int fd;
	loff_t from, off, pos = -1;
	size_t nrd;
	uint8_t byte, *buf1;
	const char *path = vt_new_path_unique(vt_env);

	vt_open(path, O_CREAT | O_RDWR, 0600, &fd);
	buf1 = vt_new_buf_rands(vt_env, bsz);
	off = (loff_t)bsz;
	vt_pwriten(fd, buf1, bsz, off);
	off = (loff_t)(bsz * 100);
	vt_pwriten(fd, buf1, bsz, off);

	off = (loff_t)bsz;
	from = off - 1;
	vt_llseek(fd, from, SEEK_HOLE, &pos);
	vt_expect_eq(pos, from);
	vt_read(fd, &byte, 1, &nrd);
	vt_expect_eq(1, nrd);
	vt_expect_eq(0, byte);

	from = (loff_t)(bsz * 2) - 2;
	vt_llseek(fd, from, SEEK_HOLE, &pos);
	vt_expect_eq(pos, (loff_t)(bsz * 2));
	vt_preadn(fd, &byte, 1, pos);
	vt_expect_eq(0, byte);

	vt_close(fd);
	vt_unlink(path);
}

static void test_lseek_hole(struct vt_env *vt_env)
{
	test_lseek_hole_(vt_env, VT_BK_SIZE);
	test_lseek_hole_(vt_env, 2 * VT_BK_SIZE);
	test_lseek_hole_(vt_env, VT_UMEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Tests lseek(2) with SEEK_DATA on sparse file
 */
static void test_lseek_data_sparse(struct vt_env *vt_env)
{
	int fd;
	loff_t off, data_off, pos, ssize;
	size_t i, nwr, step, size, nsteps = 256;
	char *path, *buf1;

	size = VT_BK_SIZE;
	ssize = (loff_t)size;
	step = VT_UMEGA;
	path = vt_new_path_unique(vt_env);
	buf1 = vt_new_buf_rands(vt_env, size);

	vt_open(path, O_CREAT | O_RDWR, 0600, &fd);
	for (i = 0; i < nsteps; ++i) {
		off = (loff_t)(step * (i + 1));
		data_off = off - ssize;
		vt_ftruncate(fd, off);
		vt_pwrite(fd, buf1, size, data_off, &nwr);
		vt_expect_eq(nwr, size);
	}

	vt_llseek(fd, 0, SEEK_SET, &pos);
	vt_expect_eq(pos, 0);
	for (i = 0; i < nsteps; ++i) {
		off = (loff_t)(step * i);
		data_off = (loff_t)(step * (i + 1)) - ssize;
		vt_llseek(fd, off, SEEK_DATA, &pos);
		vt_expect_eq(pos, data_off);
	}
	vt_close(fd);
	vt_unlink(path);
}

/*
 * Tests lseek(2) with SEEK_HOLE on sparse file
 */
static void test_lseek_hole_sparse(struct vt_env *vt_env)
{
	int fd;
	loff_t off, hole_off, pos, ssize;
	size_t i, nwr, step, size, nsteps = 256;
	char *path, *buf1;

	size = VT_BK_SIZE;
	ssize = (loff_t)size;
	step = VT_UMEGA;
	path = vt_new_path_unique(vt_env);
	buf1 = vt_new_buf_rands(vt_env, size);

	vt_open(path, O_CREAT | O_RDWR, 0600, &fd);
	for (i = 0; i < nsteps; ++i) {
		off = (loff_t)(step * i);
		vt_pwrite(fd, buf1, size, off, &nwr);
		vt_expect_eq(nwr, size);
	}

	vt_llseek(fd, 0, SEEK_SET, &pos);
	vt_expect_eq(pos, 0);
	for (i = 0; i < nsteps - 1; ++i) {
		off = (loff_t)(step * i);
		hole_off = off + ssize;
		vt_llseek(fd, off, SEEK_HOLE, &pos);
		vt_expect_eq(pos, hole_off);
	}

	vt_llseek(fd, 0, SEEK_SET, &pos);
	vt_expect_eq(pos, 0);
	for (i = 0; i < nsteps - 1; ++i) {
		off = (loff_t)(step * i) + ssize + 1;
		hole_off = off;
		vt_llseek(fd, off, SEEK_HOLE, &pos);
		vt_expect_eq(pos, hole_off);
	}

	vt_close(fd);
	vt_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTEST(test_lseek_simple),
	VT_DEFTEST(test_lseek_data),
	VT_DEFTEST(test_lseek_hole),
	VT_DEFTEST(test_lseek_data_sparse),
	VT_DEFTEST(test_lseek_hole_sparse),
};

const struct vt_tests vt_test_lseek = VT_DEFTESTS(vt_local_tests);

