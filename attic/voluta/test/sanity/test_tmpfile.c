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
#define VOLUTA_TEST 1
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "sanity.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects read-write data-consistency, sequential writes of single block.
 */
static void test_tmpfile_simple(struct voluta_t_ctx *t_ctx)
{
	int fd, o_flags = O_RDWR | O_TMPFILE | O_EXCL;
	loff_t pos = -1;
	size_t i, n, bsz;
	size_t nwr, nrd;
	struct stat st;
	char *path;
	void *buf;

	bsz  = BK_SIZE;
	buf  = voluta_t_new_buf_zeros(t_ctx, bsz);
	path = voluta_t_new_path_unique(t_ctx);

	voluta_t_mkdir(path, 0700);
	voluta_t_open(path, o_flags, 0600, &fd);
	for (i = 0; i < 128; ++i) {
		n = i;
		memcpy(buf, &n, sizeof(n));
		voluta_t_write(fd, buf, bsz, &nwr);
		voluta_t_fstat(fd, &st);
		voluta_t_expect_eq((long)st.st_size, (long)((i + 1) * bsz));
	}
	voluta_t_llseek(fd, 0, SEEK_SET, &pos);
	for (i = 0; i < 128; ++i) {
		voluta_t_read(fd, buf, bsz, &nrd);
		memcpy(&n, buf, sizeof(n));
		voluta_t_expect_eq((long)i, (long)n);
	}
	voluta_t_close(fd);
	voluta_t_rmdir(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects read-write data-consistency for buffer-size of 1M.
 */
static void test_buffer(struct voluta_t_ctx *t_ctx, size_t bsz)
{
	int fd, o_flags = O_RDWR | O_TMPFILE | O_EXCL;
	size_t i, n;
	void *buf1, *buf2;
	char *path;
	struct stat st;

	path = voluta_t_new_path_unique(t_ctx);
	voluta_t_mkdir(path, 0700);
	voluta_t_open(path, o_flags, 0600, &fd);
	for (i = 0; i < 8; ++i) {
		buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
		voluta_t_pwrite(fd, buf1, bsz, 0, &n);
		voluta_t_fsync(fd);
		buf2 = voluta_t_new_buf_rands(t_ctx, bsz);
		voluta_t_pread(fd, buf2, bsz, 0, &n);
		voluta_t_fstat(fd, &st);
		voluta_t_expect_eq(st.st_size, bsz);
		voluta_t_expect_eqm(buf1, buf2, bsz);
	}
	voluta_t_close(fd);
	voluta_t_rmdir(path);
}

static void test_tmpfile_rdwr_1k(struct voluta_t_ctx *t_ctx)
{
	test_buffer(t_ctx, 1024);
}

static void test_tmpfile_rdwr_8k(struct voluta_t_ctx *t_ctx)
{
	test_buffer(t_ctx, 8 * UKILO);
}

static void test_tmpfile_rdwr_1m(struct voluta_t_ctx *t_ctx)
{
	test_buffer(t_ctx, UMEGA);
}

static void test_tmpfile_rdwr_8m(struct voluta_t_ctx *t_ctx)
{
	test_buffer(t_ctx, 8 * UMEGA);
}

static void test_tmpfile_rdwr_32m(struct voluta_t_ctx *t_ctx)
{
	test_buffer(t_ctx, 32 * UMEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTESTF(test_tmpfile_simple, VOLUTA_T_IO_TMPFILE),
	VOLUTA_T_DEFTESTF(test_tmpfile_rdwr_1k, VOLUTA_T_IO_TMPFILE),
	VOLUTA_T_DEFTESTF(test_tmpfile_rdwr_8k, VOLUTA_T_IO_TMPFILE),
	VOLUTA_T_DEFTESTF(test_tmpfile_rdwr_1m, VOLUTA_T_IO_TMPFILE),
	VOLUTA_T_DEFTESTF(test_tmpfile_rdwr_8m, VOLUTA_T_IO_TMPFILE),
	VOLUTA_T_DEFTESTF(test_tmpfile_rdwr_32m, VOLUTA_T_IO_TMPFILE),
};

const struct voluta_t_tests
voluta_t_test_tmpfile = VOLUTA_T_DEFTESTS(t_local_tests);
