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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "fsstest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects read-write data-consistency, sequential writes of single block.
 */
static void test_basic_simple_(struct voluta_t_ctx *t_ctx,
			       size_t bsz, size_t cnt)
{
	int fd;
	loff_t pos = -1;
	size_t num;
	struct stat st;
	void *buf = voluta_t_new_buf_zeros(t_ctx, bsz);
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0644, &fd);
	for (size_t i = 0; i < cnt; ++i) {
		num = i;
		memcpy(buf, &num, sizeof(num));
		voluta_t_writen(fd, buf, bsz);
		voluta_t_fstat(fd, &st);
		voluta_t_expect_eq(st.st_size, (i + 1) * bsz);
	}
	voluta_t_llseek(fd, 0, SEEK_SET, &pos);
	for (size_t i = 0; i < cnt; ++i) {
		voluta_t_readn(fd, buf, bsz);
		memcpy(&num, buf, sizeof(num));
		voluta_t_expect_eq(i, num);
	}
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_basic_simple(struct voluta_t_ctx *t_ctx)
{
	test_basic_simple_(t_ctx, BK_SIZE, 256);
	test_basic_simple_(t_ctx, BK_SIZE + 1, 256);
	test_basic_simple_(t_ctx, UMEGA, 16);
	test_basic_simple_(t_ctx, UMEGA - 1, 16);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects read-write data-consistency for n uint64_t integers
 */
static void test_basic_seq_(struct voluta_t_ctx *t_ctx, size_t count)
{
	int fd;
	uint64_t num;
	loff_t off;
	const size_t bsz = sizeof(num);
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	for (size_t i = 0; i < count; ++i) {
		num = i;
		off = (loff_t)(i * bsz);
		voluta_t_pwriten(fd, &num, bsz, off);
	}
	for (size_t i = 0; i < count; ++i) {
		off = (loff_t)(i * bsz);
		voluta_t_preadn(fd, &num, bsz, off);
		voluta_t_expect_eq(i, num);
	}
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_basic_seq1(struct voluta_t_ctx *t_ctx)
{
	test_basic_seq_(t_ctx, 1);
	test_basic_seq_(t_ctx, 2);
	test_basic_seq_(t_ctx, 10);
}

static void test_basic_seq_1k(struct voluta_t_ctx *t_ctx)
{
	test_basic_seq_(t_ctx, UKILO / sizeof(uint64_t));
}

static void test_basic_seq_8k(struct voluta_t_ctx *t_ctx)
{
	test_basic_seq_(t_ctx, 8 * UKILO / sizeof(uint64_t));
}

static void test_basic_seq_1m(struct voluta_t_ctx *t_ctx)
{
	test_basic_seq_(t_ctx, UMEGA / sizeof(uint64_t));
}

static void test_basic_seq_8m(struct voluta_t_ctx *t_ctx)
{
	test_basic_seq_(t_ctx, (8 * UMEGA) / sizeof(uint64_t));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects read-write data-consistency for buffer-size
 */
static void test_basic_rdwr(struct voluta_t_ctx *t_ctx, size_t bsz)
{
	int fd = -1;
	struct stat st;
	void *buf1 = NULL;
	void *buf2 = voluta_t_new_buf_rands(t_ctx, bsz);
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	for (size_t i = 0; i < 64; ++i) {
		buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
		voluta_t_pwriten(fd, buf1, bsz, 0);
		voluta_t_fsync(fd);
		voluta_t_preadn(fd, buf2, bsz, 0);
		voluta_t_fstat(fd, &st);
		voluta_t_expect_eq(st.st_size, bsz);
		voluta_t_expect_eqm(buf1, buf2, bsz);
	}

	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_basic_rdwr_1k(struct voluta_t_ctx *t_ctx)
{
	test_basic_rdwr(t_ctx, UKILO);
}

static void test_basic_rdwr_2k(struct voluta_t_ctx *t_ctx)
{
	test_basic_rdwr(t_ctx, 2 * UKILO);
}

static void test_basic_rdwr_4k(struct voluta_t_ctx *t_ctx)
{
	test_basic_rdwr(t_ctx, 4 * UKILO);
}

static void test_basic_rdwr_8k(struct voluta_t_ctx *t_ctx)
{
	test_basic_rdwr(t_ctx, 8 * UKILO);
}

static void test_basic_rdwr_1m(struct voluta_t_ctx *t_ctx)
{
	test_basic_rdwr(t_ctx, UMEGA);
}

static void test_basic_rdwr_8m(struct voluta_t_ctx *t_ctx)
{
	test_basic_rdwr(t_ctx, 8 * UMEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Must _not_ get ENOSPC for sequence of write-overwrite of large buffer.
 */
static void test_basic_space(struct voluta_t_ctx *t_ctx)
{
	int fd;
	loff_t off;
	size_t bsz = UMEGA;
	void *buf1 = NULL;
	void *buf2 = NULL;
	const char *path = voluta_t_new_path_unique(t_ctx);

	for (size_t i = 0; i < 256; ++i) {
		off  = (loff_t)i;
		buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
		buf2 = voluta_t_new_buf_rands(t_ctx, bsz);
		voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
		voluta_t_pwriten(fd, buf1, bsz, off);
		voluta_t_preadn(fd, buf2, bsz, off);
		voluta_t_expect_eqm(buf1, buf2, bsz);
		voluta_t_close(fd);
		voluta_t_unlink(path);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects read-write data-consistency, reverse writes.
 */
static void test_basic_reserve_at(struct voluta_t_ctx *t_ctx,
				  loff_t off, size_t ssz)
{
	int fd;
	loff_t pos = -1;
	size_t i, nwr, nrd;
	uint8_t buf[1];
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0644, &fd);
	for (i = 0; i < ssz; ++i) {
		buf[0] = (uint8_t)i;
		pos = off + (loff_t)(ssz - i - 1);
		voluta_t_pwrite(fd, buf, 1, pos, &nwr);
		voluta_t_expect_eq(nwr, 1);
	}
	for (i = 0; i < ssz; ++i) {
		pos = off + (loff_t)(ssz - i - 1);
		voluta_t_pread(fd, buf, 1, pos, &nrd);
		voluta_t_expect_eq(nrd, 1);
		voluta_t_expect_eq(buf[0], (uint8_t)i);
	}
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_basic_reserve1(struct voluta_t_ctx *t_ctx)
{
	test_basic_reserve_at(t_ctx, 0, BK_SIZE);
}

static void test_basic_reserve2(struct voluta_t_ctx *t_ctx)
{
	test_basic_reserve_at(t_ctx, 100000, 2 * BK_SIZE);
}

static void test_basic_reserve3(struct voluta_t_ctx *t_ctx)
{
	test_basic_reserve_at(t_ctx, 9999999, BK_SIZE - 1);
}

static void test_basic_reserve4(struct voluta_t_ctx *t_ctx)
{
	test_basic_reserve_at(t_ctx,  100003, 7 * BK_SIZE);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects read-write data-consistency when I/O overlaps
 */
static void test_basic_overlap(struct voluta_t_ctx *t_ctx)
{
	int fd;
	loff_t off;
	size_t cnt = 0;
	size_t bsz = UMEGA;
	void *buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
	void *buf2 = voluta_t_new_buf_rands(t_ctx, bsz);
	void *buf3 = voluta_t_new_buf_zeros(t_ctx, bsz);
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_pwriten(fd, buf1, bsz, 0);
	voluta_t_preadn(fd, buf3, bsz, 0);
	voluta_t_expect_eqm(buf1, buf3, bsz);

	off = 17;
	cnt = 100;
	voluta_t_pwriten(fd, buf2, cnt, off);
	voluta_t_preadn(fd, buf3, cnt, off);
	voluta_t_expect_eqm(buf2, buf3, cnt);

	off = 2099;
	cnt = 1000;
	voluta_t_pwriten(fd, buf2, cnt, off);
	voluta_t_preadn(fd, buf3, cnt, off);
	voluta_t_expect_eqm(buf2, buf3, cnt);

	off = 32077;
	cnt = 10000;
	voluta_t_pwriten(fd, buf2, cnt, off);
	voluta_t_preadn(fd, buf3, cnt, off);
	voluta_t_expect_eqm(buf2, buf3, cnt);

	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects read-write data-consistency when I/O in complex patterns
 */
static void test_basic_(struct voluta_t_ctx *t_ctx,
			loff_t pos, loff_t lim, loff_t step)
{
	int fd;
	loff_t off;
	size_t nwr;
	size_t bsz = BK_SIZE;
	void *buf1 = NULL;
	void *buf2 = NULL;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	for (off = pos; off < lim; off += step) {
		buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
		buf2 = voluta_t_new_buf_rands(t_ctx, bsz);
		voluta_t_pwrite(fd, buf1, bsz, off, &nwr);
		voluta_t_fsync(fd);
		voluta_t_pread(fd, buf2, bsz, off, &nwr);
		voluta_t_fsync(fd);
		voluta_t_expect_eqm(buf1, buf2, bsz);
	}
	voluta_t_close(fd);
	voluta_t_unlink(path);
}


static void test_basic_aligned(struct voluta_t_ctx *t_ctx)
{
	const loff_t step = BK_SIZE;

	test_basic_(t_ctx, 0, UMEGA, step);
	test_basic_(t_ctx, 0, 2 * UMEGA, step);
	test_basic_(t_ctx, UGIGA - UMEGA, UMEGA, step);
}

static void test_basic_unaligned(struct voluta_t_ctx *t_ctx)
{
	const loff_t step1 = BK_SIZE + 1;
	const loff_t step2 = BK_SIZE - 1;

	test_basic_(t_ctx, 0, UMEGA, step1);
	test_basic_(t_ctx, 0, UMEGA, step2);
	test_basic_(t_ctx, 0, 2 * UMEGA, step1);
	test_basic_(t_ctx, 0, 2 * UMEGA, step2);
	test_basic_(t_ctx, UGIGA - UMEGA, UMEGA, step1);
	test_basic_(t_ctx, UGIGA - UMEGA, UMEGA, step2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful write-read of single full large-chunk to regular file
 */
static void test_basic_chunk_(struct voluta_t_ctx *t_ctx,
			      loff_t off, size_t bsz)
{
	int fd;
	void *buf1, *buf2;
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
	buf2 = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_pwriten(fd, buf1, bsz, off);
	voluta_t_preadn(fd, buf2, bsz, off);
	voluta_t_expect_eqm(buf1, buf2, bsz);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_basic_chunk_x(struct voluta_t_ctx *t_ctx, size_t bsz)
{
	test_basic_chunk_(t_ctx, 0, bsz);
	test_basic_chunk_(t_ctx, UMEGA, bsz);
	test_basic_chunk_(t_ctx, 1, bsz);
	test_basic_chunk_(t_ctx, UMEGA - 1, bsz);
}

static void test_basic_chunk_1m(struct voluta_t_ctx *t_ctx)
{
	test_basic_chunk_x(t_ctx, UMEGA);
}

static void test_basic_chunk_2m(struct voluta_t_ctx *t_ctx)
{
	test_basic_chunk_x(t_ctx, 2 * UMEGA);
}

static void test_basic_chunk_4m(struct voluta_t_ctx *t_ctx)
{
	test_basic_chunk_x(t_ctx, 4 * UMEGA);
}

static void test_basic_chunk_8m(struct voluta_t_ctx *t_ctx)
{
	test_basic_chunk_x(t_ctx, 8 * UMEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_basic_simple),
	VOLUTA_T_DEFTEST(test_basic_rdwr_1k),
	VOLUTA_T_DEFTEST(test_basic_rdwr_2k),
	VOLUTA_T_DEFTEST(test_basic_rdwr_4k),
	VOLUTA_T_DEFTEST(test_basic_rdwr_8k),
	VOLUTA_T_DEFTEST(test_basic_rdwr_1m),
	VOLUTA_T_DEFTEST(test_basic_rdwr_8m),
	VOLUTA_T_DEFTEST(test_basic_seq1),
	VOLUTA_T_DEFTEST(test_basic_seq_1k),
	VOLUTA_T_DEFTEST(test_basic_seq_8k),
	VOLUTA_T_DEFTEST(test_basic_seq_1m),
	VOLUTA_T_DEFTEST(test_basic_seq_8m),
	VOLUTA_T_DEFTEST(test_basic_space),
	VOLUTA_T_DEFTEST(test_basic_reserve1),
	VOLUTA_T_DEFTEST(test_basic_reserve2),
	VOLUTA_T_DEFTEST(test_basic_reserve3),
	VOLUTA_T_DEFTEST(test_basic_reserve4),
	VOLUTA_T_DEFTEST(test_basic_overlap),
	VOLUTA_T_DEFTEST(test_basic_aligned),
	VOLUTA_T_DEFTEST(test_basic_unaligned),
	VOLUTA_T_DEFTEST(test_basic_chunk_1m),
	VOLUTA_T_DEFTEST(test_basic_chunk_2m),
	VOLUTA_T_DEFTEST(test_basic_chunk_4m),
	VOLUTA_T_DEFTEST(test_basic_chunk_8m),
};

const struct voluta_t_tests voluta_t_test_basic_io =
	VOLUTA_T_DEFTESTS(t_local_tests);
