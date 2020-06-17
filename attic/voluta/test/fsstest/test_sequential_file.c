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
#include <string.h>
#include <stdio.h>
#include "fsstest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests data-consistency of sequential writes followed by sequential reads.
 */
static void test_sequencial_(struct voluta_t_ctx *t_ctx, loff_t from,
			     size_t bsz, size_t cnt, int rewrite)
{
	int fd;
	loff_t pos;
	size_t i, j, nwr, nrd, nitr;
	void *buf1, *buf2;
	char *path;

	path = voluta_t_new_path_unique(t_ctx);
	buf2 = voluta_t_new_buf_zeros(t_ctx, bsz);
	nitr = rewrite ? 2 : 1;

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	for (i = 0; i < nitr; ++i) {
		voluta_t_llseek(fd, from, SEEK_SET, &pos);
		for (j = 0; j < cnt; ++j) {
			buf1 = voluta_t_new_buf_nums(t_ctx, j, bsz);
			voluta_t_write(fd, buf1, bsz, &nwr);
			voluta_t_expect_eq(nwr, bsz);
		}
		voluta_t_llseek(fd, from, SEEK_SET, &pos);
		for (j = 0; j < cnt; ++j) {
			buf1 = voluta_t_new_buf_nums(t_ctx, j, bsz);
			voluta_t_read(fd, buf2, bsz, &nrd);
			voluta_t_expect_eq(nrd, bsz);
			voluta_t_expect_eqm(buf1, buf2, bsz);
		}
	}
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_sequencial_io(struct voluta_t_ctx *t_ctx,
			       loff_t from, size_t bsz, size_t cnt)
{
	test_sequencial_(t_ctx, from, bsz, cnt, 0);
	test_sequencial_(t_ctx, from, bsz, cnt, 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
test_sequencial_aligned_blk(struct voluta_t_ctx *t_ctx, size_t cnt)
{
	loff_t from;
	const size_t bsz = BK_SIZE;

	from = 0;
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)BK_SIZE;
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)UMEGA;
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UMEGA - BK_SIZE);
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)UGIGA;
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UGIGA - BK_SIZE);
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UGIGA + BK_SIZE);
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UGIGA - (bsz * cnt));
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)((UGIGA * 2) / 2);
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UTERA - (bsz * cnt));
	test_sequencial_io(t_ctx, from, bsz, cnt);
}

static void test_sequencial_aligned_blk1(struct voluta_t_ctx *t_ctx)
{
	test_sequencial_aligned_blk(t_ctx, 1);
}

static void test_sequencial_aligned_blk2(struct voluta_t_ctx *t_ctx)
{
	test_sequencial_aligned_blk(t_ctx, 2);
}

static void test_sequencial_aligned_blk63(struct voluta_t_ctx *t_ctx)
{
	test_sequencial_aligned_blk(t_ctx, 63);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_sequencial_aligned_mega(struct voluta_t_ctx *t_ctx,
		size_t cnt)
{
	loff_t from;
	const size_t bsz = UMEGA;

	from = 0;
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)BK_SIZE;
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)UMEGA;
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UMEGA - BK_SIZE);
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)UGIGA;
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UGIGA - BK_SIZE);
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UGIGA + BK_SIZE);
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UGIGA - UMEGA);
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UGIGA - UMEGA);
	test_sequencial_io(t_ctx, from, bsz, 2 * cnt);
	from = (loff_t)(2 * UGIGA);
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UGIGA / 2);
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UTERA - (bsz * cnt));
	test_sequencial_io(t_ctx, from, bsz, cnt);
}

static void test_sequencial_aligned_mega1(struct voluta_t_ctx *t_ctx)
{
	test_sequencial_aligned_mega(t_ctx, 1);
}

static void test_sequencial_aligned_mega2(struct voluta_t_ctx *t_ctx)
{
	test_sequencial_aligned_mega(t_ctx, 2);
}

static void test_sequencial_aligned_mega3(struct voluta_t_ctx *t_ctx)
{
	test_sequencial_aligned_mega(t_ctx, 3);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
test_sequencial_unaligned_blk(struct voluta_t_ctx *t_ctx, size_t cnt)
{
	loff_t from;
	const size_t bsz = BK_SIZE;

	from = 1;
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)BK_SIZE - 11;
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)BK_SIZE + 11;
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)UMEGA - 11;
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UMEGA - BK_SIZE - 1);
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)UGIGA - 11;
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UGIGA - BK_SIZE - 1);
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UGIGA + BK_SIZE + 1);
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UGIGA - (bsz * cnt) + 1);
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UGIGA / 11);
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UTERA - (bsz * cnt) - 11);
	test_sequencial_io(t_ctx, from, bsz, cnt);
}

static void test_sequencial_unaligned_blk1(struct voluta_t_ctx *t_ctx)
{
	test_sequencial_unaligned_blk(t_ctx, 1);
}

static void test_sequencial_unaligned_blk2(struct voluta_t_ctx *t_ctx)
{
	test_sequencial_unaligned_blk(t_ctx, 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
test_sequencial_unaligned_mega(struct voluta_t_ctx *t_ctx, size_t cnt)
{
	loff_t from;
	const size_t bsz = UMEGA;

	from = 1;
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)BK_SIZE - 11;
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)BK_SIZE + 11;
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)UMEGA - 11;
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UMEGA - BK_SIZE - 1);
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)UGIGA - 11;
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UGIGA - BK_SIZE - 1);
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UGIGA + BK_SIZE + 1);
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UGIGA - (bsz * cnt) + 1);
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UGIGA / 11);
	test_sequencial_io(t_ctx, from, bsz, cnt);
	from = (loff_t)(UTERA - (bsz * cnt) - 11);
	test_sequencial_io(t_ctx, from, bsz, cnt);
}

static void test_sequencial_unaligned_mega1(struct voluta_t_ctx
		*t_ctx)
{
	test_sequencial_unaligned_mega(t_ctx, 1);
}

static void test_sequencial_unaligned_mega2(struct voluta_t_ctx
		*t_ctx)
{
	test_sequencial_unaligned_mega(t_ctx, 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
test_sequencial_unaligned_(struct voluta_t_ctx *t_ctx, size_t len, size_t cnt)
{
	loff_t from;

	from = 7;
	test_sequencial_io(t_ctx, from, len, cnt);
	from = (loff_t)BK_SIZE - 7;
	test_sequencial_io(t_ctx, from, len, cnt);
	from = (loff_t)BK_SIZE + 7;
	test_sequencial_io(t_ctx, from, len, cnt);
	from = (loff_t)UMEGA - 7;
	test_sequencial_io(t_ctx, from, len, cnt);
	from = (loff_t)UMEGA / 7;
	test_sequencial_io(t_ctx, from, len, cnt);
	from = (loff_t)UGIGA - 7;
	test_sequencial_io(t_ctx, from, len, cnt);
	from = (loff_t)UGIGA / 7;
	test_sequencial_io(t_ctx, from, len, cnt);
	from = (loff_t)(UGIGA + (len * cnt) - 7);
	test_sequencial_io(t_ctx, from, len, cnt);
	from = (loff_t)((UGIGA / 7) - 7);
	test_sequencial_io(t_ctx, from, len, cnt);
	from = (loff_t)(UTERA - (len * cnt) - 7);
	test_sequencial_io(t_ctx, from, len, cnt);
}

static void test_sequencial_unaligned_small(struct voluta_t_ctx
		*t_ctx)
{
	const size_t len = 7907;

	test_sequencial_unaligned_(t_ctx, len, 1);
	test_sequencial_unaligned_(t_ctx, len, 7);
	test_sequencial_unaligned_(t_ctx, len, 79);
	test_sequencial_unaligned_(t_ctx, len, 797);
}

static void test_sequencial_unaligned_large(struct voluta_t_ctx
		*t_ctx)
{
	const size_t len = 66601;

	test_sequencial_unaligned_(t_ctx, len, 1);
	test_sequencial_unaligned_(t_ctx, len, 61);
	test_sequencial_unaligned_(t_ctx, len, 661);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests data-consistency of sequential writes followed by sequential reads
 * of variable length strings
 */
static void test_sequencial_nstrings(struct voluta_t_ctx *t_ctx,
				     loff_t start_off, size_t cnt)
{
	int fd, ni, whence = SEEK_SET;
	loff_t pos = -1;
	size_t i, nu, nwr, nrd;
	char buf1[128] = "";
	char buf2[128] = "";
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_llseek(fd, start_off, whence, &pos);
	for (i = 0; i < cnt; ++i) {
		ni = snprintf(buf1, sizeof(buf1), "%lu", i);
		nu = (size_t)ni;
		voluta_t_expect_eq(nu, strlen(buf1));
		voluta_t_write(fd, buf1, nu, &nwr);
		voluta_t_expect_eq(nu, nwr);
	}
	voluta_t_llseek(fd, start_off, whence, &pos);
	for (i = 0; i < cnt; ++i) {
		ni = snprintf(buf1, sizeof(buf1), "%lu", i);
		nu = (size_t)ni;
		voluta_t_expect_eq(nu, strlen(buf1));
		voluta_t_read(fd, buf2, nu, &nrd);
		voluta_t_expect_eq(nu, nrd);
		voluta_t_expect_eq(0, strcmp(buf1, buf2));
	}
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_sequencial_nstrings_(struct voluta_t_ctx *t_ctx,
				      size_t n)
{
	test_sequencial_nstrings(t_ctx, 0, n);
	test_sequencial_nstrings(t_ctx, BK_SIZE - 1, n);
	test_sequencial_nstrings(t_ctx, BK_SIZE, n);
	test_sequencial_nstrings(t_ctx, UMEGA - 1, n);
	test_sequencial_nstrings(t_ctx, UGIGA, n);
}

static void test_sequencial_strings10(struct voluta_t_ctx *t_ctx)
{
	test_sequencial_nstrings_(t_ctx, 10);
}

static void test_sequencial_strings100(struct voluta_t_ctx *t_ctx)
{
	test_sequencial_nstrings_(t_ctx, 100);
}

static void test_sequencial_strings1000(struct voluta_t_ctx *t_ctx)
{
	test_sequencial_nstrings_(t_ctx, 1000);
}

static void test_sequencial_strings10000(struct voluta_t_ctx *t_ctx)
{
	test_sequencial_nstrings_(t_ctx, 10000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_sequencial_aligned_blk1),
	VOLUTA_T_DEFTEST(test_sequencial_aligned_blk2),
	VOLUTA_T_DEFTEST(test_sequencial_aligned_blk63),
	VOLUTA_T_DEFTEST(test_sequencial_aligned_mega1),
	VOLUTA_T_DEFTEST(test_sequencial_aligned_mega2),
	VOLUTA_T_DEFTEST(test_sequencial_aligned_mega3),
	VOLUTA_T_DEFTEST(test_sequencial_unaligned_blk1),
	VOLUTA_T_DEFTEST(test_sequencial_unaligned_blk2),
	VOLUTA_T_DEFTEST(test_sequencial_unaligned_mega1),
	VOLUTA_T_DEFTEST(test_sequencial_unaligned_mega2),
	VOLUTA_T_DEFTEST(test_sequencial_unaligned_small),
	VOLUTA_T_DEFTEST(test_sequencial_unaligned_large),
	VOLUTA_T_DEFTEST(test_sequencial_strings10),
	VOLUTA_T_DEFTEST(test_sequencial_strings100),
	VOLUTA_T_DEFTEST(test_sequencial_strings1000),
	VOLUTA_T_DEFTEST(test_sequencial_strings10000),
};

const struct voluta_t_tests
voluta_t_test_sequencial_file = VOLUTA_T_DEFTESTS(t_local_tests);

