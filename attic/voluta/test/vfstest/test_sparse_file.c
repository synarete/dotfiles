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
#include <stdio.h>
#include <string.h>
#include "vfstest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests read-write data-consistency over sparse file.
 */
static void test_sparse_simple(struct vt_env *vt_env)
{
	int fd;
	loff_t pos = -1;
	size_t nsz, num, num2;
	const size_t cnt = 7717;
	const size_t step = 524287;
	const char *path = vt_new_path_unique(vt_env);

	vt_open(path, O_CREAT | O_RDWR, 0600, &fd);
	for (size_t i = 0; i < cnt; ++i) {
		num = (i * step);
		pos = (loff_t)num;
		nsz = sizeof(num);
		vt_pwriten(fd, &num, nsz, pos);
	}
	vt_close(fd);

	vt_open(path, O_RDONLY, 0, &fd);
	for (size_t j = 0; j < cnt; ++j) {
		num = (j * step);
		pos = (loff_t)num;
		nsz = sizeof(num2);
		vt_preadn(fd, &num2, nsz, pos);
		vt_expect_eq(num, num2);
	}
	vt_close(fd);
	vt_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests read-write data-consistency over sparse file with syncs over same file
 */
static void test_sparse_rdwr(struct vt_env *vt_env)
{
	int fd = -1;
	loff_t pos = -1;
	size_t nsz, num, num2;
	const size_t cnt  = 127;
	const size_t step = 524287;
	const char *path = vt_new_path_unique(vt_env);

	vt_open(path, O_CREAT | O_RDWR, 0600, &fd);
	vt_close(fd);
	for (size_t i = 0; i < 17; ++i) {
		for (size_t j = 0; j < cnt; ++j) {
			vt_open(path, O_RDWR, 0, &fd);
			num = i + (j * step);
			pos = (loff_t)num;
			nsz = sizeof(num);
			vt_pwriten(fd, &num, nsz, pos);
			vt_fdatasync(fd);
			vt_preadn(fd, &num2, nsz, pos);
			vt_expect_eq(num, num2);
			vt_close(fd);
		}
	}
	vt_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests read-write data-consistency over sparse file with overwrites.
 */
static void
test_sparse_overwrite_(struct vt_env *vt_env, loff_t base_off)
{
	int fd;
	size_t i;
	loff_t off;
	char *path;
	uint8_t byte, *buf1, *buf2, *buf3;
	loff_t offs[] = {
		737717, 280411, 10007, 31033, 42043, 53113, 161881, 375533,
		86767, 97171, 75353, 611999, 1108007, 64601, 1272211, 20323
	};
	const size_t len1 = 10037;
	const size_t len2 = 10039;
	const size_t noffs = VT_ARRAY_SIZE(offs);

	buf1 = vt_new_buf_rands(vt_env, len1);
	buf2 = vt_new_buf_rands(vt_env, len2);
	buf3 = vt_new_buf_rands(vt_env, len1 + len2);
	path = vt_new_path_unique(vt_env);

	vt_open(path, O_CREAT | O_RDWR, 0600, &fd);
	for (i = 0; i < noffs; ++i) {
		off = base_off + offs[i];
		vt_pwriten(fd, buf1, len1, off);
	}
	for (i = 0; i < noffs; ++i) {
		off = base_off + offs[i] + 1;
		vt_pwriten(fd, buf2, len2, off);
	}
	for (i = 0; i < noffs; ++i) {
		off = base_off + offs[i];
		vt_preadn(fd, &byte, 1, off);
		vt_expect_eq(buf1[0], byte);
		vt_preadn(fd, buf3, len2, off + 1);
		vt_expect_eqm(buf2, buf3, len2);
	}
	vt_close(fd);
	vt_unlink(path);
}

static void test_sparse_overwrite(struct vt_env *vt_env)
{
	test_sparse_overwrite_(vt_env, 0);
	test_sparse_overwrite_(vt_env, 1);
	test_sparse_overwrite_(vt_env, VT_UMEGA - 2);
	test_sparse_overwrite_(vt_env, VT_UGIGA - 3);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTEST(test_sparse_simple),
	VT_DEFTEST(test_sparse_rdwr),
	VT_DEFTEST(test_sparse_overwrite),
};

const struct vt_tests vt_test_sparse_file = VT_DEFTESTS(vt_local_tests);
