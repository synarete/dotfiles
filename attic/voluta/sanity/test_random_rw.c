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
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include "sanity.h"


static void do_pwrite(int fd, const void *buf, size_t cnt, loff_t off)
{
	size_t nwr = 0;

	voluta_t_pwrite(fd, buf, cnt, off, &nwr);
	voluta_t_expect_eq(nwr, cnt);
}

static void do_pread(int fd, void *buf, size_t cnt,
		     loff_t off, const void *expected_data)
{
	size_t nrd = 0;

	voluta_t_pread(fd, buf, cnt, off, &nrd);
	voluta_t_expect_eq(nrd, cnt);
	if (expected_data != NULL) {
		voluta_t_expect_eqm(buf, expected_data, cnt);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests read-write data-consistency for a sequence of IOs at pseudo random
 * offsets.
 */
static void test_random_(struct voluta_t_ctx *t_ctx, loff_t from,
			 size_t len, size_t cnt, int rewrite)
{
	int fd;
	const long *pseq;
	loff_t pos, idx, slen = (loff_t)len;
	size_t i, j, nitr, seed;
	void *buf1, *buf2;
	const char *path = voluta_t_new_path_unique(t_ctx);

	nitr = rewrite ? 2 : 1;
	buf2 = voluta_t_new_buf_zeros(t_ctx, len);
	pseq = voluta_t_new_randseq(t_ctx, cnt, 0);

	voluta_t_open(path, O_CREAT | O_RDWR, 0640, &fd);
	for (i = 0; i < nitr; ++i) {
		for (j = 0; j < cnt; ++j) {
			idx = pseq[j];
			voluta_assert_lt(idx, cnt);
			voluta_assert_ge(idx, 0);
			pos = from + (slen * idx);
			seed = i + j + (size_t)pos;
			buf1 = voluta_t_new_buf_nums(t_ctx, seed, len);
			do_pwrite(fd, buf1, len, pos);
		}
		for (j = 0; j < cnt; ++j) {
			idx = pseq[j];
			voluta_assert_lt(idx, cnt);
			voluta_assert_ge(idx, 0);
			pos = from + (slen * idx);
			seed = i + j + (size_t)pos;
			buf1 = voluta_t_new_buf_nums(t_ctx, seed, len);
			do_pread(fd, buf2, len, pos, buf1);
		}
	}
	voluta_t_close(fd);
	voluta_t_unlink(path);
}


static void test_random_io(struct voluta_t_ctx *t_ctx, loff_t from,
			   size_t len, size_t cnt)
{
	test_random_(t_ctx, from, len, cnt, 0);
	test_random_(t_ctx, from, len, cnt, 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_random_aligned_blk(struct voluta_t_ctx *t_ctx,
				    size_t cnt)
{
	loff_t from;
	const size_t len = VOLUTA_BK_SIZE;

	from = 0;
	test_random_io(t_ctx, from, len, cnt);
	from = (loff_t)VOLUTA_BK_SIZE;
	test_random_io(t_ctx, from, len, cnt);
	from = (loff_t)VOLUTA_MEGA;
	test_random_io(t_ctx, from, len, cnt);
	from = (loff_t)(VOLUTA_MEGA - VOLUTA_BK_SIZE);
	test_random_io(t_ctx, from, len, cnt);
	from = (loff_t)VOLUTA_GIGA;
	test_random_io(t_ctx, from, len, cnt);
	from = (loff_t)(VOLUTA_GIGA - VOLUTA_BK_SIZE);
	test_random_io(t_ctx, from, len, cnt);
	from = (loff_t)(VOLUTA_GIGA + VOLUTA_BK_SIZE);
	test_random_io(t_ctx, from, len, cnt);
	from = (loff_t)(VOLUTA_GIGA - (len * cnt));
	test_random_io(t_ctx, from, len, cnt);
	from = (loff_t)((VOLUTA_GIGA) / 2);
	test_random_io(t_ctx, from, len, cnt);
	from = (loff_t)(VOLUTA_TERA - (len * cnt));
	test_random_io(t_ctx, from, len, cnt);
}


static void test_random_aligned_blk1(struct voluta_t_ctx *t_ctx)
{
	test_random_aligned_blk(t_ctx, 1);
}

static void test_random_aligned_blk2(struct voluta_t_ctx *t_ctx)
{
	test_random_aligned_blk(t_ctx, 2);
}

static void test_random_aligned_blk63(struct voluta_t_ctx *t_ctx)
{
	test_random_aligned_blk(t_ctx, 63);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_random_aligned_mega(struct voluta_t_ctx *t_ctx,
				     size_t cnt)
{
	loff_t from;
	const size_t bsz = VOLUTA_MEGA;

	from = 0;
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)VOLUTA_BK_SIZE;
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)VOLUTA_MEGA;
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(VOLUTA_MEGA - VOLUTA_BK_SIZE);
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)VOLUTA_GIGA;
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(VOLUTA_GIGA - VOLUTA_BK_SIZE);
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(VOLUTA_GIGA + VOLUTA_BK_SIZE);
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(VOLUTA_GIGA - VOLUTA_MEGA);
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(VOLUTA_GIGA - VOLUTA_MEGA);
	test_random_io(t_ctx, from, bsz, 2 * cnt);
	from = (loff_t)(2 * VOLUTA_GIGA);
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)((VOLUTA_GIGA) / 2);
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(VOLUTA_TERA - (bsz * cnt));
	test_random_io(t_ctx, from, bsz, cnt);
}

static void test_random_aligned_mega1(struct voluta_t_ctx *t_ctx)
{
	test_random_aligned_mega(t_ctx, 1);
}

static void test_random_aligned_mega2(struct voluta_t_ctx *t_ctx)
{
	test_random_aligned_mega(t_ctx, 2);
}

static void test_random_aligned_mega3(struct voluta_t_ctx *t_ctx)
{
	test_random_aligned_mega(t_ctx, 3);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_random_unaligned_blk(struct voluta_t_ctx *t_ctx,
				      size_t cnt)
{
	loff_t from;
	const size_t bsz = VOLUTA_BK_SIZE;

	from = 1;
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)VOLUTA_BK_SIZE - 11;
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)VOLUTA_BK_SIZE + 11;
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)VOLUTA_MEGA - 11;
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(VOLUTA_MEGA - VOLUTA_BK_SIZE - 1);
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)VOLUTA_GIGA - 11;
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(VOLUTA_GIGA - VOLUTA_BK_SIZE - 1);
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(VOLUTA_GIGA + VOLUTA_BK_SIZE + 1);
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(VOLUTA_GIGA - (bsz * cnt) + 1);
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)((VOLUTA_GIGA * 13) / 11);
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(VOLUTA_TERA - (bsz * cnt) - 11);
	test_random_io(t_ctx, from, bsz, cnt);
}

static void test_random_unaligned_blk1(struct voluta_t_ctx *t_ctx)
{
	test_random_unaligned_blk(t_ctx, 1);
}

static void test_random_unaligned_blk2(struct voluta_t_ctx *t_ctx)
{
	test_random_unaligned_blk(t_ctx, 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_random_unaligned_mega(struct voluta_t_ctx *t_ctx,
				       size_t cnt)
{
	loff_t from;
	const size_t bsz = VOLUTA_MEGA;

	from = 1;
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)VOLUTA_BK_SIZE - 11;
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)VOLUTA_BK_SIZE + 11;
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)VOLUTA_MEGA - 11;
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(VOLUTA_MEGA - VOLUTA_BK_SIZE - 1);
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)VOLUTA_GIGA - 11;
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(VOLUTA_GIGA - VOLUTA_BK_SIZE - 1);
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(VOLUTA_GIGA + VOLUTA_BK_SIZE + 1);
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(VOLUTA_GIGA - (bsz * cnt) + 1);
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)((VOLUTA_GIGA * 13) / 11);
	test_random_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(VOLUTA_TERA - (bsz * cnt) - 11);
	test_random_io(t_ctx, from, bsz, cnt);
}

static void test_random_unaligned_mega1(struct voluta_t_ctx *t_ctx)
{
	test_random_unaligned_mega(t_ctx, 1);
}

static void test_random_unaligned_mega2(struct voluta_t_ctx *t_ctx)
{
	test_random_unaligned_mega(t_ctx, 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
test_random_unaligned_(struct voluta_t_ctx *t_ctx, size_t len,
		       size_t cnt)
{
	loff_t from;

	from = 7;
	test_random_io(t_ctx, from, len, cnt);
	from = (loff_t)VOLUTA_BK_SIZE - 7;
	test_random_io(t_ctx, from, len, cnt);
	from = (loff_t)VOLUTA_BK_SIZE + 7;
	test_random_io(t_ctx, from, len, cnt);
	from = (loff_t)VOLUTA_MEGA - 7;
	test_random_io(t_ctx, from, len, cnt);
	from = (loff_t)VOLUTA_MEGA / 7;
	test_random_io(t_ctx, from, len, cnt);
	from = (loff_t)VOLUTA_GIGA - 7;
	test_random_io(t_ctx, from, len, cnt);
	from = (loff_t)VOLUTA_GIGA / 7;
	test_random_io(t_ctx, from, len, cnt);
	from = (loff_t)(VOLUTA_GIGA + (len * cnt) - 7);
	test_random_io(t_ctx, from, len, cnt);
	from = (loff_t)((VOLUTA_GIGA / 7) - 7);
	test_random_io(t_ctx, from, len, cnt);
	from = (loff_t)(VOLUTA_TERA - (len * cnt) - 7);
	test_random_io(t_ctx, from, len, cnt);
}

static void test_random_unaligned_small(struct voluta_t_ctx *t_ctx)
{
	const size_t len = 7907;

	test_random_unaligned_(t_ctx, len, 1);
	test_random_unaligned_(t_ctx, len, 7);
	test_random_unaligned_(t_ctx, len, 79);
	test_random_unaligned_(t_ctx, len, 797);
}

static void test_random_unaligned_large(struct voluta_t_ctx *t_ctx)
{
	const size_t len = 66601;

	test_random_unaligned_(t_ctx, len, 1);
	test_random_unaligned_(t_ctx, len, 61);
	test_random_unaligned_(t_ctx, len, 661);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_random_aligned_blk1),
	VOLUTA_T_DEFTEST(test_random_aligned_blk2),
	VOLUTA_T_DEFTEST(test_random_aligned_blk63),
	VOLUTA_T_DEFTEST(test_random_aligned_mega1),
	VOLUTA_T_DEFTEST(test_random_aligned_mega2),
	VOLUTA_T_DEFTEST(test_random_aligned_mega3),
	VOLUTA_T_DEFTEST(test_random_unaligned_blk1),
	VOLUTA_T_DEFTEST(test_random_unaligned_blk2),
	VOLUTA_T_DEFTEST(test_random_unaligned_mega1),
	VOLUTA_T_DEFTEST(test_random_unaligned_mega2),
	VOLUTA_T_DEFTEST(test_random_unaligned_small),
	VOLUTA_T_DEFTEST(test_random_unaligned_large),
};

const struct voluta_t_tests
voluta_t_test_random = VOLUTA_T_DEFTESTS(t_local_tests);
