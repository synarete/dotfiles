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
#include <string.h>
#include <stdio.h>
#include "vfstest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests data-consistency of sequential writes followed by sequential reads.
 */
static void test_sequencial_(struct vt_env *vt_env, loff_t from,
			     size_t bsz, size_t cnt, int rewrite)
{
	int fd;
	loff_t pos;
	size_t i, j, nwr, nrd, nitr;
	void *buf1, *buf2;
	char *path;

	path = vt_new_path_unique(vt_env);
	buf2 = vt_new_buf_zeros(vt_env, bsz);
	nitr = rewrite ? 2 : 1;

	vt_open(path, O_CREAT | O_RDWR, 0600, &fd);
	for (i = 0; i < nitr; ++i) {
		vt_llseek(fd, from, SEEK_SET, &pos);
		for (j = 0; j < cnt; ++j) {
			buf1 = vt_new_buf_nums(vt_env, j, bsz);
			vt_write(fd, buf1, bsz, &nwr);
			vt_expect_eq(nwr, bsz);
		}
		vt_llseek(fd, from, SEEK_SET, &pos);
		for (j = 0; j < cnt; ++j) {
			buf1 = vt_new_buf_nums(vt_env, j, bsz);
			vt_read(fd, buf2, bsz, &nrd);
			vt_expect_eq(nrd, bsz);
			vt_expect_eqm(buf1, buf2, bsz);
		}
	}
	vt_close(fd);
	vt_unlink(path);
}

static void test_sequencial_io(struct vt_env *vt_env,
			       loff_t from, size_t bsz, size_t cnt)
{
	test_sequencial_(vt_env, from, bsz, cnt, 0);
	test_sequencial_(vt_env, from, bsz, cnt, 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
test_sequencial_aligned_blk(struct vt_env *vt_env, size_t cnt)
{
	loff_t from;
	const size_t bsz = VT_BK_SIZE;

	from = 0;
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_BK_SIZE;
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_UMEGA;
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UMEGA - VT_BK_SIZE);
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_UGIGA;
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA - VT_BK_SIZE);
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA + VT_BK_SIZE);
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA - (bsz * cnt));
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)((VT_UGIGA * 2) / 2);
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UTERA - (bsz * cnt));
	test_sequencial_io(vt_env, from, bsz, cnt);
}

static void test_sequencial_aligned_blk1(struct vt_env *vt_env)
{
	test_sequencial_aligned_blk(vt_env, 1);
}

static void test_sequencial_aligned_blk2(struct vt_env *vt_env)
{
	test_sequencial_aligned_blk(vt_env, 2);
}

static void test_sequencial_aligned_blk63(struct vt_env *vt_env)
{
	test_sequencial_aligned_blk(vt_env, 63);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
test_sequencial_aligned_mega(struct vt_env *vt_env, size_t cnt)
{
	loff_t from;
	const size_t bsz = VT_UMEGA;

	from = 0;
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_BK_SIZE;
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_UMEGA;
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UMEGA - VT_BK_SIZE);
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_UGIGA;
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA - VT_BK_SIZE);
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA + VT_BK_SIZE);
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA - VT_UMEGA);
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA - VT_UMEGA);
	test_sequencial_io(vt_env, from, bsz, 2 * cnt);
	from = (loff_t)(2 * VT_UGIGA);
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA / 2);
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UTERA - (bsz * cnt));
	test_sequencial_io(vt_env, from, bsz, cnt);
}

static void test_sequencial_aligned_mega1(struct vt_env *vt_env)
{
	test_sequencial_aligned_mega(vt_env, 1);
}

static void test_sequencial_aligned_mega2(struct vt_env *vt_env)
{
	test_sequencial_aligned_mega(vt_env, 2);
}

static void test_sequencial_aligned_mega3(struct vt_env *vt_env)
{
	test_sequencial_aligned_mega(vt_env, 3);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
test_sequencial_unaligned_blk(struct vt_env *vt_env, size_t cnt)
{
	loff_t from;
	const size_t bsz = VT_BK_SIZE;

	from = 1;
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_BK_SIZE - 11;
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_BK_SIZE + 11;
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_UMEGA - 11;
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UMEGA - VT_BK_SIZE - 1);
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_UGIGA - 11;
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA - VT_BK_SIZE - 1);
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA + VT_BK_SIZE + 1);
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA - (bsz * cnt) + 1);
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA / 11);
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UTERA - (bsz * cnt) - 11);
	test_sequencial_io(vt_env, from, bsz, cnt);
}

static void test_sequencial_unaligned_blk1(struct vt_env *vt_env)
{
	test_sequencial_unaligned_blk(vt_env, 1);
}

static void test_sequencial_unaligned_blk2(struct vt_env *vt_env)
{
	test_sequencial_unaligned_blk(vt_env, 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
test_sequencial_unaligned_mega(struct vt_env *vt_env, size_t cnt)
{
	loff_t from;
	const size_t bsz = VT_UMEGA;

	from = 1;
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_BK_SIZE - 11;
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_BK_SIZE + 11;
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_UMEGA - 11;
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UMEGA - VT_BK_SIZE - 1);
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_UGIGA - 11;
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA - VT_BK_SIZE - 1);
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA + VT_BK_SIZE + 1);
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA - (bsz * cnt) + 1);
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA / 11);
	test_sequencial_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UTERA - (bsz * cnt) - 11);
	test_sequencial_io(vt_env, from, bsz, cnt);
}

static void test_sequencial_unaligned_mega1(struct vt_env
		*vt_env)
{
	test_sequencial_unaligned_mega(vt_env, 1);
}

static void test_sequencial_unaligned_mega2(struct vt_env
		*vt_env)
{
	test_sequencial_unaligned_mega(vt_env, 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
test_sequencial_unaligned_(struct vt_env *vt_env, size_t len, size_t cnt)
{
	loff_t from;

	from = 7;
	test_sequencial_io(vt_env, from, len, cnt);
	from = (loff_t)VT_BK_SIZE - 7;
	test_sequencial_io(vt_env, from, len, cnt);
	from = (loff_t)VT_BK_SIZE + 7;
	test_sequencial_io(vt_env, from, len, cnt);
	from = (loff_t)VT_UMEGA - 7;
	test_sequencial_io(vt_env, from, len, cnt);
	from = (loff_t)VT_UMEGA / 7;
	test_sequencial_io(vt_env, from, len, cnt);
	from = (loff_t)VT_UGIGA - 7;
	test_sequencial_io(vt_env, from, len, cnt);
	from = (loff_t)VT_UGIGA / 7;
	test_sequencial_io(vt_env, from, len, cnt);
	from = (loff_t)(VT_UGIGA + (len * cnt) - 7);
	test_sequencial_io(vt_env, from, len, cnt);
	from = (loff_t)((VT_UGIGA / 7) - 7);
	test_sequencial_io(vt_env, from, len, cnt);
	from = (loff_t)(VT_UTERA - (len * cnt) - 7);
	test_sequencial_io(vt_env, from, len, cnt);
}

static void test_sequencial_unaligned_small(struct vt_env
		*vt_env)
{
	const size_t len = 7907;

	test_sequencial_unaligned_(vt_env, len, 1);
	test_sequencial_unaligned_(vt_env, len, 7);
	test_sequencial_unaligned_(vt_env, len, 79);
	test_sequencial_unaligned_(vt_env, len, 797);
}

static void test_sequencial_unaligned_large(struct vt_env
		*vt_env)
{
	const size_t len = 66601;

	test_sequencial_unaligned_(vt_env, len, 1);
	test_sequencial_unaligned_(vt_env, len, 61);
	test_sequencial_unaligned_(vt_env, len, 661);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests data-consistency of sequential writes followed by sequential reads
 * of variable length strings
 */
static void test_sequencial_nstrings(struct vt_env *vt_env,
				     loff_t start_off, size_t cnt)
{
	int fd, ni, whence = SEEK_SET;
	loff_t pos = -1;
	size_t i, nu, nwr, nrd;
	char buf1[128] = "";
	char buf2[128] = "";
	const char *path = vt_new_path_unique(vt_env);

	vt_open(path, O_CREAT | O_RDWR, 0600, &fd);
	vt_llseek(fd, start_off, whence, &pos);
	for (i = 0; i < cnt; ++i) {
		ni = snprintf(buf1, sizeof(buf1), "%lu", i);
		nu = (size_t)ni;
		vt_expect_eq(nu, strlen(buf1));
		vt_write(fd, buf1, nu, &nwr);
		vt_expect_eq(nu, nwr);
	}
	vt_llseek(fd, start_off, whence, &pos);
	for (i = 0; i < cnt; ++i) {
		ni = snprintf(buf1, sizeof(buf1), "%lu", i);
		nu = (size_t)ni;
		vt_expect_eq(nu, strlen(buf1));
		vt_read(fd, buf2, nu, &nrd);
		vt_expect_eq(nu, nrd);
		vt_expect_eq(0, strcmp(buf1, buf2));
	}
	vt_close(fd);
	vt_unlink(path);
}

static void test_sequencial_nstrings_(struct vt_env *vt_env,
				      size_t n)
{
	test_sequencial_nstrings(vt_env, 0, n);
	test_sequencial_nstrings(vt_env, VT_BK_SIZE - 1, n);
	test_sequencial_nstrings(vt_env, VT_BK_SIZE, n);
	test_sequencial_nstrings(vt_env, VT_UMEGA - 1, n);
	test_sequencial_nstrings(vt_env, VT_UGIGA, n);
}

static void test_sequencial_strings10(struct vt_env *vt_env)
{
	test_sequencial_nstrings_(vt_env, 10);
}

static void test_sequencial_strings100(struct vt_env *vt_env)
{
	test_sequencial_nstrings_(vt_env, 100);
}

static void test_sequencial_strings1000(struct vt_env *vt_env)
{
	test_sequencial_nstrings_(vt_env, 1000);
}

static void test_sequencial_strings10000(struct vt_env *vt_env)
{
	test_sequencial_nstrings_(vt_env, 10000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTEST(test_sequencial_aligned_blk1),
	VT_DEFTEST(test_sequencial_aligned_blk2),
	VT_DEFTEST(test_sequencial_aligned_blk63),
	VT_DEFTEST(test_sequencial_aligned_mega1),
	VT_DEFTEST(test_sequencial_aligned_mega2),
	VT_DEFTEST(test_sequencial_aligned_mega3),
	VT_DEFTEST(test_sequencial_unaligned_blk1),
	VT_DEFTEST(test_sequencial_unaligned_blk2),
	VT_DEFTEST(test_sequencial_unaligned_mega1),
	VT_DEFTEST(test_sequencial_unaligned_mega2),
	VT_DEFTEST(test_sequencial_unaligned_small),
	VT_DEFTEST(test_sequencial_unaligned_large),
	VT_DEFTEST(test_sequencial_strings10),
	VT_DEFTEST(test_sequencial_strings100),
	VT_DEFTEST(test_sequencial_strings1000),
	VT_DEFTEST(test_sequencial_strings10000),
};

const struct vt_tests vt_test_sequencial_file = VT_DEFTESTS(vt_local_tests);

