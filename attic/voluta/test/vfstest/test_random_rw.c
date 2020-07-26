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
 * Tests read-write data-consistency for a sequence of IOs at pseudo random
 * offsets.
 */
static void test_random_(struct vt_env *vt_env, loff_t from,
			 size_t len, size_t cnt, int rewrite)
{
	int fd;
	const long *pseq;
	loff_t pos, idx, slen = (loff_t)len;
	size_t i, j, nitr, seed;
	void *buf1, *buf2;
	const char *path = vt_new_path_unique(vt_env);

	nitr = rewrite ? 2 : 1;
	buf2 = vt_new_buf_zeros(vt_env, len);
	pseq = vt_new_randseq(vt_env, cnt, 0);

	vt_open(path, O_CREAT | O_RDWR, 0640, &fd);
	for (i = 0; i < nitr; ++i) {
		for (j = 0; j < cnt; ++j) {
			idx = pseq[j];
			voluta_assert_lt(idx, cnt);
			voluta_assert_ge(idx, 0);
			pos = from + (slen * idx);
			seed = i + j + (size_t)pos;
			buf1 = vt_new_buf_nums(vt_env, seed, len);
			vt_pwriten(fd, buf1, len, pos);
		}
		for (j = 0; j < cnt; ++j) {
			idx = pseq[j];
			voluta_assert_lt(idx, cnt);
			voluta_assert_ge(idx, 0);
			pos = from + (slen * idx);
			seed = i + j + (size_t)pos;
			buf1 = vt_new_buf_nums(vt_env, seed, len);
			vt_preadn(fd, buf2, len, pos);
			vt_expect_eqm(buf1, buf2, len);
		}
	}
	vt_close(fd);
	vt_unlink(path);
}


static void test_random_io(struct vt_env *vt_env, loff_t from,
			   size_t len, size_t cnt)
{
	test_random_(vt_env, from, len, cnt, 0);
	test_random_(vt_env, from, len, cnt, 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_random_aligned_blk(struct vt_env *vt_env, size_t cnt)
{
	loff_t from;
	const size_t len = VT_BK_SIZE;

	from = 0;
	test_random_io(vt_env, from, len, cnt);
	from = (loff_t)VT_BK_SIZE;
	test_random_io(vt_env, from, len, cnt);
	from = (loff_t)VT_UMEGA;
	test_random_io(vt_env, from, len, cnt);
	from = (loff_t)(VT_UMEGA - VT_BK_SIZE);
	test_random_io(vt_env, from, len, cnt);
	from = (loff_t)VT_UGIGA;
	test_random_io(vt_env, from, len, cnt);
	from = (loff_t)(VT_UGIGA - VT_BK_SIZE);
	test_random_io(vt_env, from, len, cnt);
	from = (loff_t)(VT_UGIGA + VT_BK_SIZE);
	test_random_io(vt_env, from, len, cnt);
	from = (loff_t)(VT_UGIGA - (len * cnt));
	test_random_io(vt_env, from, len, cnt);
	from = (loff_t)((VT_UGIGA) / 2);
	test_random_io(vt_env, from, len, cnt);
	from = (loff_t)(VT_UTERA - (len * cnt));
	test_random_io(vt_env, from, len, cnt);
}

static void test_random_aligned_blk1(struct vt_env *vt_env)
{
	test_random_aligned_blk(vt_env, 1);
}

static void test_random_aligned_blk2(struct vt_env *vt_env)
{
	test_random_aligned_blk(vt_env, 2);
}

static void test_random_aligned_blk63(struct vt_env *vt_env)
{
	test_random_aligned_blk(vt_env, 63);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_random_aligned_mega(struct vt_env *vt_env,
				     size_t cnt)
{
	loff_t from;
	const size_t bsz = VT_UMEGA;

	from = 0;
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_BK_SIZE;
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_UMEGA;
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UMEGA - VT_BK_SIZE);
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_UGIGA;
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA - VT_BK_SIZE);
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA + VT_BK_SIZE);
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA - VT_UMEGA);
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA - VT_UMEGA);
	test_random_io(vt_env, from, bsz, 2 * cnt);
	from = (loff_t)(2 * VT_UGIGA);
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)((VT_UGIGA) / 2);
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UTERA - (bsz * cnt));
	test_random_io(vt_env, from, bsz, cnt);
}

static void test_random_aligned_mega1(struct vt_env *vt_env)
{
	test_random_aligned_mega(vt_env, 1);
}

static void test_random_aligned_mega2(struct vt_env *vt_env)
{
	test_random_aligned_mega(vt_env, 2);
}

static void test_random_aligned_mega3(struct vt_env *vt_env)
{
	test_random_aligned_mega(vt_env, 3);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_random_unaligned_blk(struct vt_env *vt_env,
				      size_t cnt)
{
	loff_t from;
	const size_t bsz = VT_BK_SIZE;

	from = 1;
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_BK_SIZE - 11;
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_BK_SIZE + 11;
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_UMEGA - 11;
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UMEGA - VT_BK_SIZE - 1);
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_UGIGA - 11;
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA - VT_BK_SIZE - 1);
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA + VT_BK_SIZE + 1);
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA - (bsz * cnt) + 1);
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)((VT_UGIGA * 13) / 11);
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UTERA - (bsz * cnt) - 11);
	test_random_io(vt_env, from, bsz, cnt);
}

static void test_random_unaligned_blk1(struct vt_env *vt_env)
{
	test_random_unaligned_blk(vt_env, 1);
}

static void test_random_unaligned_blk2(struct vt_env *vt_env)
{
	test_random_unaligned_blk(vt_env, 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_random_unaligned_mega(struct vt_env *vt_env,
				       size_t cnt)
{
	loff_t from;
	const size_t bsz = VT_UMEGA;

	from = 1;
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_BK_SIZE - 11;
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_BK_SIZE + 11;
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_UMEGA - 11;
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UMEGA - VT_BK_SIZE - 1);
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)VT_UGIGA - 11;
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA - VT_BK_SIZE - 1);
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA + VT_BK_SIZE + 1);
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UGIGA - (bsz * cnt) + 1);
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)((VT_UGIGA * 13) / 11);
	test_random_io(vt_env, from, bsz, cnt);
	from = (loff_t)(VT_UTERA - (bsz * cnt) - 11);
	test_random_io(vt_env, from, bsz, cnt);
}

static void test_random_unaligned_mega1(struct vt_env *vt_env)
{
	test_random_unaligned_mega(vt_env, 1);
}

static void test_random_unaligned_mega2(struct vt_env *vt_env)
{
	test_random_unaligned_mega(vt_env, 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
test_random_unaligned_(struct vt_env *vt_env, size_t len,
		       size_t cnt)
{
	loff_t from;

	from = 7;
	test_random_io(vt_env, from, len, cnt);
	from = (loff_t)VT_BK_SIZE - 7;
	test_random_io(vt_env, from, len, cnt);
	from = (loff_t)VT_BK_SIZE + 7;
	test_random_io(vt_env, from, len, cnt);
	from = (loff_t)VT_UMEGA - 7;
	test_random_io(vt_env, from, len, cnt);
	from = (loff_t)VT_UMEGA / 7;
	test_random_io(vt_env, from, len, cnt);
	from = (loff_t)VT_UGIGA - 7;
	test_random_io(vt_env, from, len, cnt);
	from = (loff_t)VT_UGIGA / 7;
	test_random_io(vt_env, from, len, cnt);
	from = (loff_t)(VT_UGIGA + (len * cnt) - 7);
	test_random_io(vt_env, from, len, cnt);
	from = (loff_t)((VT_UGIGA / 7) - 7);
	test_random_io(vt_env, from, len, cnt);
	from = (loff_t)(VT_UTERA - (len * cnt) - 7);
	test_random_io(vt_env, from, len, cnt);
}

static void test_random_unaligned_small(struct vt_env *vt_env)
{
	const size_t len = 7907;

	test_random_unaligned_(vt_env, len, 1);
	test_random_unaligned_(vt_env, len, 7);
	test_random_unaligned_(vt_env, len, 79);
	test_random_unaligned_(vt_env, len, 797);
}

static void test_random_unaligned_large(struct vt_env *vt_env)
{
	const size_t len = 66601;

	test_random_unaligned_(vt_env, len, 1);
	test_random_unaligned_(vt_env, len, 61);
	test_random_unaligned_(vt_env, len, 661);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTEST(test_random_aligned_blk1),
	VT_DEFTEST(test_random_aligned_blk2),
	VT_DEFTEST(test_random_aligned_blk63),
	VT_DEFTEST(test_random_aligned_mega1),
	VT_DEFTEST(test_random_aligned_mega2),
	VT_DEFTEST(test_random_aligned_mega3),
	VT_DEFTEST(test_random_unaligned_blk1),
	VT_DEFTEST(test_random_unaligned_blk2),
	VT_DEFTEST(test_random_unaligned_mega1),
	VT_DEFTEST(test_random_unaligned_mega2),
	VT_DEFTEST(test_random_unaligned_small),
	VT_DEFTEST(test_random_unaligned_large),
};

const struct vt_tests vt_test_random = VT_DEFTESTS(vt_local_tests);
