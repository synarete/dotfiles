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
 * Tests data-consistency of I/O via fd where file's path is unlinked from
 * filesyatem's namespace. Data truncated to zero explicitly before close.
 */
static void test_unlinked_simple_(struct voluta_t_ctx *t_ctx,
				  size_t bsz,
				  size_t cnt)
{
	int fd;
	loff_t pos = -1;
	size_t i, nwr, nrd;
	void *buf1, *buf2;
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
	buf2 = voluta_t_new_buf_rands(t_ctx, bsz);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_unlink(path);

	for (i = 0; i < cnt; ++i) {
		voluta_t_unlink_noent(path);
		voluta_t_write(fd, buf1, bsz, &nwr);
		voluta_t_expect_eq(nwr, bsz);
	}
	voluta_t_llseek(fd, 0, SEEK_SET, &pos);
	for (i = 0; i < cnt; ++i) {
		voluta_t_unlink_noent(path);
		voluta_t_read(fd, buf2, bsz, &nrd);
		voluta_t_expect_eq(nrd, bsz);
		voluta_t_expect_eqm(buf1, buf2, bsz);
	}

	voluta_t_ftruncate(fd, 0);
	voluta_t_close(fd);
	voluta_t_unlink_noent(path);
}


static void test_unlinked_simple1(struct voluta_t_ctx *t_ctx)
{
	test_unlinked_simple_(t_ctx, 1, 1);
	test_unlinked_simple_(t_ctx, VOLUTA_BK_SIZE, 2);
	test_unlinked_simple_(t_ctx, VOLUTA_BK_SIZE - 3, 3);
}

static void test_unlinked_simple2(struct voluta_t_ctx *t_ctx)
{
	test_unlinked_simple_(t_ctx, VOLUTA_BK_SIZE, VOLUTA_KILO);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests data-consistency of I/O via fd where file's path is unlinked from
 * filesyatem's namespace and data is truncated implicitly upon close.
 */
static void test_unlinked_complex_(struct voluta_t_ctx *t_ctx,
				   loff_t base, size_t bsz, size_t cnt)
{
	int fd;
	loff_t pos = -1;
	size_t i, nwr, nrd;
	void *buf1, *buf2;
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
	buf2 = voluta_t_new_buf_rands(t_ctx, bsz);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_unlink(path);

	for (i = 0; i < cnt; ++i) {
		pos = base + (loff_t)(i * bsz);
		voluta_t_pwrite(fd, buf1, bsz, pos, &nwr);
		voluta_t_expect_eq(nwr, bsz);
	}
	for (i = 0; i < cnt; ++i) {
		pos = base + (loff_t)(i * bsz);
		voluta_t_pread(fd, buf2, bsz, pos, &nrd);
		voluta_t_expect_eq(nrd, bsz);
		voluta_t_expect_eqm(buf1, buf2, bsz);
	}
	voluta_t_close(fd);
	voluta_t_unlink_noent(path);
}


static void test_unlinked_complex1(struct voluta_t_ctx *t_ctx)
{
	test_unlinked_complex_(t_ctx, 0, 1, 1);
	test_unlinked_complex_(t_ctx, 0, VOLUTA_BK_SIZE, 2);
	test_unlinked_complex_(t_ctx, 0, VOLUTA_BK_SIZE - 3, 3);
}

static void test_unlinked_complex2(struct voluta_t_ctx *t_ctx)
{
	test_unlinked_complex_(t_ctx, 0, VOLUTA_BK_SIZE, VOLUTA_KILO);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests data-consistency of I/O via multiple fds where file's path is
 * unlinked from filesyatem's namespace.
 */
static void test_unlinked_multi(struct voluta_t_ctx *t_ctx)
{
	int fd1, fd2;
	loff_t pos;
	size_t i, nwr, nrd;
	const size_t bsz = VOLUTA_BK_SIZE;
	const size_t cnt = VOLUTA_KILO;
	void *buf1, *buf2;
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
	buf2 = voluta_t_new_buf_rands(t_ctx, bsz);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd1);
	voluta_t_open(path, O_RDONLY, 0, &fd2);
	voluta_t_unlink(path);

	for (i = 0; i < cnt; ++i) {
		pos = (loff_t)(cnt * VOLUTA_MEGA);
		voluta_t_pwrite(fd1, buf1, bsz, pos, &nwr);
		voluta_t_expect_eq(nwr, bsz);
	}
	for (i = 0; i < cnt; ++i) {
		pos = (loff_t)(cnt * VOLUTA_MEGA);
		voluta_t_pread(fd2, buf2, bsz, pos, &nrd);
		voluta_t_expect_eq(nrd, bsz);
		voluta_t_expect_eqm(buf1, buf2, bsz);
	}

	voluta_t_unlink_noent(path);
	voluta_t_close(fd1);
	voluta_t_close(fd2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests data-consistency of I/O after rename operations (with possible
 * implicit unlink).
 */
static void test_unlinked_rename(struct voluta_t_ctx *t_ctx)
{
	int fd, fd2;
	loff_t pos;
	size_t i, k, nwr, nrd;
	const size_t cnt = 111;
	const size_t isz = sizeof(i);
	const char *path1 = voluta_t_new_path_unique(t_ctx);
	const char *path2 = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path1, O_CREAT | O_RDWR, 0600, &fd);
	for (i = cnt; i > 0; --i) {
		pos = (loff_t)(i * cnt);
		voluta_t_pwrite(fd, &i, isz, pos, &nwr);
		voluta_t_expect_eq(nwr, isz);
	}
	voluta_t_rename(path1, path2);
	for (i = cnt; i > 0; --i) {
		pos = (loff_t)(i * cnt);
		voluta_t_pread(fd, &k, isz, pos, &nrd);
		voluta_t_expect_eq(nrd, isz);
		voluta_t_expect_eq(i, k);
	}
	voluta_t_open(path2, O_RDONLY, 0, &fd2);
	for (i = cnt; i > 0; --i) {
		pos = (loff_t)(i * cnt);
		voluta_t_pread(fd2, &k, isz, pos, &nrd);
		voluta_t_expect_eq(nrd, isz);
		voluta_t_expect_eq(i, k);
	}
	voluta_t_unlink_noent(path1);
	voluta_t_unlink(path2);
	voluta_t_close(fd);
	voluta_t_unlink_noent(path2);
	voluta_t_close(fd2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_unlinked_simple1),
	VOLUTA_T_DEFTEST(test_unlinked_simple2),
	VOLUTA_T_DEFTEST(test_unlinked_complex1),
	VOLUTA_T_DEFTEST(test_unlinked_complex2),
	VOLUTA_T_DEFTEST(test_unlinked_multi),
	VOLUTA_T_DEFTEST(test_unlinked_rename),
};

const struct voluta_t_tests
voluta_t_test_unlinked_file = VOLUTA_T_DEFTESTS(t_local_tests);
