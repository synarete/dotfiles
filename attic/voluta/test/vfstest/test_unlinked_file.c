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
 * Tests data-consistency of I/O via fd where file's path is unlinked from
 * filesyatem's namespace. Data truncated to zero explicitly before close.
 */
static void test_unlinked_simple_(struct vt_env *vt_env,
				  size_t bsz,
				  size_t cnt)
{
	int fd;
	loff_t pos = -1;
	size_t nwr;
	size_t nrd;
	void *buf1 = vt_new_buf_rands(vt_env, bsz);
	void *buf2 = vt_new_buf_rands(vt_env, bsz);
	const char *path = vt_new_path_unique(vt_env);

	vt_open(path, O_CREAT | O_RDWR, 0600, &fd);
	vt_unlink(path);

	for (size_t i = 0; i < cnt; ++i) {
		vt_unlink_noent(path);
		vt_write(fd, buf1, bsz, &nwr);
		vt_expect_eq(nwr, bsz);
	}
	vt_llseek(fd, 0, SEEK_SET, &pos);
	for (size_t i = 0; i < cnt; ++i) {
		vt_unlink_noent(path);
		vt_read(fd, buf2, bsz, &nrd);
		vt_expect_eq(nrd, bsz);
		vt_expect_eqm(buf1, buf2, bsz);
	}

	vt_ftruncate(fd, 0);
	vt_close(fd);
	vt_unlink_noent(path);
}


static void test_unlinked_simple1(struct vt_env *vt_env)
{
	test_unlinked_simple_(vt_env, 1, 1);
	test_unlinked_simple_(vt_env, VT_BK_SIZE, 2);
	test_unlinked_simple_(vt_env, VT_BK_SIZE - 3, 3);
}

static void test_unlinked_simple2(struct vt_env *vt_env)
{
	test_unlinked_simple_(vt_env, VT_BK_SIZE, VT_UKILO);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests data-consistency of I/O via fd where file's path is unlinked from
 * filesyatem's namespace and data is truncated implicitly upon close.
 */
static void test_unlinked_complex_(struct vt_env *vt_env,
				   loff_t base, size_t bsz, size_t cnt)
{
	int fd;
	loff_t pos;
	void *buf1 = vt_new_buf_rands(vt_env, bsz);
	void *buf2 = vt_new_buf_rands(vt_env, bsz);
	const char *path = vt_new_path_unique(vt_env);

	vt_open(path, O_CREAT | O_RDWR, 0600, &fd);
	vt_unlink(path);
	for (size_t i = 0; i < cnt; ++i) {
		pos = base + (loff_t)(i * bsz);
		vt_pwriten(fd, buf1, bsz, pos);
	}
	for (size_t j = 0; j < cnt; ++j) {
		pos = base + (loff_t)(j * bsz);
		vt_preadn(fd, buf2, bsz, pos);
		vt_expect_eqm(buf1, buf2, bsz);
	}
	vt_close(fd);
	vt_unlink_noent(path);
}


static void test_unlinked_complex1(struct vt_env *vt_env)
{
	test_unlinked_complex_(vt_env, 0, 1, 1);
	test_unlinked_complex_(vt_env, 0, VT_BK_SIZE, 2);
	test_unlinked_complex_(vt_env, 0, VT_BK_SIZE - 3, 3);
}

static void test_unlinked_complex2(struct vt_env *vt_env)
{
	test_unlinked_complex_(vt_env, 0, VT_BK_SIZE, VT_UKILO);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests data-consistency of I/O via multiple fds where file's path is
 * unlinked from filesyatem's namespace.
 */
static void test_unlinked_multi(struct vt_env *vt_env)
{
	int fd1, fd2;
	loff_t pos;
	const size_t bsz = VT_BK_SIZE;
	const size_t cnt = VT_UKILO;
	void *buf1 = vt_new_buf_rands(vt_env, bsz);
	void *buf2 = vt_new_buf_rands(vt_env, bsz);
	const char *path = vt_new_path_unique(vt_env);

	vt_open(path, O_CREAT | O_RDWR, 0600, &fd1);
	vt_open(path, O_RDONLY, 0, &fd2);
	vt_unlink(path);

	for (size_t i = 0; i < cnt; ++i) {
		pos = (loff_t)(cnt * VT_UMEGA);
		vt_pwriten(fd1, buf1, bsz, pos);
	}
	for (size_t j = 0; j < cnt; ++j) {
		pos = (loff_t)(cnt * VT_UMEGA);
		vt_preadn(fd2, buf2, bsz, pos);
		vt_expect_eqm(buf1, buf2, bsz);
	}

	vt_unlink_noent(path);
	vt_close(fd1);
	vt_close(fd2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests data-consistency of I/O after rename operations (with possible
 * implicit unlink).
 */
static void test_unlinked_rename(struct vt_env *vt_env)
{
	int fd, fd2;
	loff_t pos;
	size_t i, k;
	const size_t cnt = 111;
	const size_t isz = sizeof(i);
	const char *path1 = vt_new_path_unique(vt_env);
	const char *path2 = vt_new_path_unique(vt_env);

	vt_open(path1, O_CREAT | O_RDWR, 0600, &fd);
	for (i = cnt; i > 0; --i) {
		pos = (loff_t)(i * cnt);
		vt_pwriten(fd, &i, isz, pos);
	}
	vt_rename(path1, path2);
	for (i = cnt; i > 0; --i) {
		pos = (loff_t)(i * cnt);
		vt_preadn(fd, &k, isz, pos);
		vt_expect_eq(i, k);
	}
	vt_open(path2, O_RDONLY, 0, &fd2);
	for (i = cnt; i > 0; --i) {
		pos = (loff_t)(i * cnt);
		vt_preadn(fd2, &k, isz, pos);
		vt_expect_eq(i, k);
	}
	vt_unlink_noent(path1);
	vt_unlink(path2);
	vt_close(fd);
	vt_unlink_noent(path2);
	vt_close(fd2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTEST(test_unlinked_simple1),
	VT_DEFTEST(test_unlinked_simple2),
	VT_DEFTEST(test_unlinked_complex1),
	VT_DEFTEST(test_unlinked_complex2),
	VT_DEFTEST(test_unlinked_multi),
	VT_DEFTEST(test_unlinked_rename),
};

const struct vt_tests vt_test_unlinked_file = VT_DEFTESTS(vt_local_tests);
