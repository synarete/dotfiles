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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "vfstest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects read-write data-consistency, sequential writes of single block.
 */
static void test_tmpfile_simple(struct vt_env *vt_env)
{
	int fd, o_flags = O_RDWR | O_TMPFILE | O_EXCL;
	loff_t pos = -1;
	size_t i, n, bsz;
	size_t nwr, nrd;
	struct stat st;
	char *path;
	void *buf;

	bsz  = VT_BK_SIZE;
	buf  = vt_new_buf_zeros(vt_env, bsz);
	path = vt_new_path_unique(vt_env);

	vt_mkdir(path, 0700);
	vt_open(path, o_flags, 0600, &fd);
	for (i = 0; i < 128; ++i) {
		n = i;
		memcpy(buf, &n, sizeof(n));
		vt_write(fd, buf, bsz, &nwr);
		vt_fstat(fd, &st);
		vt_expect_eq((long)st.st_size, (long)((i + 1) * bsz));
	}
	vt_llseek(fd, 0, SEEK_SET, &pos);
	for (i = 0; i < 128; ++i) {
		vt_read(fd, buf, bsz, &nrd);
		memcpy(&n, buf, sizeof(n));
		vt_expect_eq((long)i, (long)n);
	}
	vt_close(fd);
	vt_rmdir(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects read-write data-consistency for buffer-size of 1M.
 */
static void test_buffer(struct vt_env *vt_env, size_t bsz)
{
	int fd, o_flags = O_RDWR | O_TMPFILE | O_EXCL;
	size_t i, n;
	void *buf1, *buf2;
	char *path;
	struct stat st;

	path = vt_new_path_unique(vt_env);
	vt_mkdir(path, 0700);
	vt_open(path, o_flags, 0600, &fd);
	for (i = 0; i < 8; ++i) {
		buf1 = vt_new_buf_rands(vt_env, bsz);
		vt_pwrite(fd, buf1, bsz, 0, &n);
		vt_fsync(fd);
		buf2 = vt_new_buf_rands(vt_env, bsz);
		vt_pread(fd, buf2, bsz, 0, &n);
		vt_fstat(fd, &st);
		vt_expect_eq(st.st_size, bsz);
		vt_expect_eqm(buf1, buf2, bsz);
	}
	vt_close(fd);
	vt_rmdir(path);
}

static void test_tmpfile_rdwr_1k(struct vt_env *vt_env)
{
	test_buffer(vt_env, 1024);
}

static void test_tmpfile_rdwr_8k(struct vt_env *vt_env)
{
	test_buffer(vt_env, 8 * VT_UKILO);
}

static void test_tmpfile_rdwr_1m(struct vt_env *vt_env)
{
	test_buffer(vt_env, VT_UMEGA);
}

static void test_tmpfile_rdwr_8m(struct vt_env *vt_env)
{
	test_buffer(vt_env, 8 * VT_UMEGA);
}

static void test_tmpfile_rdwr_32m(struct vt_env *vt_env)
{
	test_buffer(vt_env, 32 * VT_UMEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTESTF(test_tmpfile_simple, VT_IO_TMPFILE),
	VT_DEFTESTF(test_tmpfile_rdwr_1k, VT_IO_TMPFILE),
	VT_DEFTESTF(test_tmpfile_rdwr_8k, VT_IO_TMPFILE),
	VT_DEFTESTF(test_tmpfile_rdwr_1m, VT_IO_TMPFILE),
	VT_DEFTESTF(test_tmpfile_rdwr_8m, VT_IO_TMPFILE),
	VT_DEFTESTF(test_tmpfile_rdwr_32m, VT_IO_TMPFILE),
};

const struct vt_tests vt_test_tmpfile = VT_DEFTESTS(vt_local_tests);
