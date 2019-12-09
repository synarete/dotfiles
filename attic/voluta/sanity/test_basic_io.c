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
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "sanity.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects read-write data-consistency, sequential writes of single block.
 */
static void test_basic_simple(struct voluta_t_ctx *t_ctx)
{
	int fd, o_flags = O_CREAT | O_RDWR;
	loff_t pos = -1;
	size_t i, n, bsz = VOLUTA_BK_SIZE;
	size_t nwr, nrd;
	struct stat st;
	void *buf;
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf = voluta_t_new_buf_zeros(t_ctx, bsz);
	voluta_t_open(path, o_flags, 0644, &fd);
	for (i = 0; i < 1024; ++i) {
		n = i;
		memcpy(buf, &n, sizeof(n));
		voluta_t_write(fd, buf, bsz, &nwr);
		voluta_t_fstat(fd, &st);
		voluta_t_expect_eq(st.st_size, (i + 1) * bsz);
	}
	voluta_t_llseek(fd, 0, SEEK_SET, &pos);
	for (i = 0; i < 1024; ++i) {
		voluta_t_read(fd, buf, bsz, &nrd);
		memcpy(&n, buf, sizeof(n));
		voluta_t_expect_eq(i, n);
	}
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects read-write data-consistency for n uint64_t integers
 */
static void test_basic_seq_(struct voluta_t_ctx *t_ctx, size_t count)
{
	int fd, o_flags = O_CREAT | O_RDWR;
	size_t i, bsz, nwr, nrd;
	uint64_t num;
	loff_t off;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, o_flags, 0600, &fd);
	for (i = 0; i < count; ++i) {
		num = i;
		bsz = sizeof(num);
		off = (loff_t)(i * bsz);
		voluta_t_pwrite(fd, &num, bsz, off, &nwr);
		voluta_t_expect_eq(bsz, nwr);
	}
	for (i = 0; i < count; ++i) {
		bsz = sizeof(num);
		off = (loff_t)(i * bsz);
		voluta_t_pread(fd, &num, bsz, off, &nrd);
		voluta_t_expect_eq(bsz, nrd);
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
	test_basic_seq_(t_ctx, VOLUTA_KILO / sizeof(uint64_t));
}

static void test_basic_seq_8k(struct voluta_t_ctx *t_ctx)
{
	test_basic_seq_(t_ctx, 8 * VOLUTA_KILO / sizeof(uint64_t));
}

static void test_basic_seq_1m(struct voluta_t_ctx *t_ctx)
{
	test_basic_seq_(t_ctx, VOLUTA_MEGA / sizeof(uint64_t));
}

static void test_basic_seq_8m(struct voluta_t_ctx *t_ctx)
{
	test_basic_seq_(t_ctx, (8 * VOLUTA_MEGA) / sizeof(uint64_t));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects read-write data-consistency for buffer-size
 */
static void test_basic_rdwr(struct voluta_t_ctx *t_ctx, size_t bsz)
{
	int fd, o_flags = O_CREAT | O_RDWR;
	size_t i, nwr, nrd;
	void *buf1, *buf2;
	struct stat st;
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf2 = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_open(path, o_flags, 0600, &fd);
	for (i = 0; i < 64; ++i) {
		buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
		voluta_t_pwrite(fd, buf1, bsz, 0, &nwr);
		voluta_t_expect_eq(bsz, nwr);
		voluta_t_fsync(fd);
		voluta_t_pread(fd, buf2, bsz, 0, &nrd);
		voluta_t_expect_eq(bsz, nrd);
		voluta_t_fstat(fd, &st);
		voluta_t_expect_eq(st.st_size, bsz);
		voluta_t_expect_eqm(buf1, buf2, bsz);
	}

	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_basic_rdwr_1k(struct voluta_t_ctx *t_ctx)
{
	test_basic_rdwr(t_ctx, VOLUTA_KILO);
}

static void test_basic_rdwr_2k(struct voluta_t_ctx *t_ctx)
{
	test_basic_rdwr(t_ctx, 2 * VOLUTA_KILO);
}

static void test_basic_rdwr_4k(struct voluta_t_ctx *t_ctx)
{
	test_basic_rdwr(t_ctx, 4 * VOLUTA_KILO);
}

static void test_basic_rdwr_8k(struct voluta_t_ctx *t_ctx)
{
	test_basic_rdwr(t_ctx, 8 * VOLUTA_KILO);
}

static void test_basic_rdwr_1m(struct voluta_t_ctx *t_ctx)
{
	test_basic_rdwr(t_ctx, VOLUTA_MEGA);
}

static void test_basic_rdwr_8m(struct voluta_t_ctx *t_ctx)
{
	test_basic_rdwr(t_ctx, 8 * VOLUTA_MEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Should not get ENOSPC for sequence of write-read-unlink of large buffer.
 */
static void test_basic_space(struct voluta_t_ctx *t_ctx)
{
	int fd, o_flags = O_CREAT | O_RDWR;
	loff_t off;
	size_t cnt, bsz = VOLUTA_MEGA;
	void *buf1, *buf2;
	char *path;

	for (size_t i = 0; i < 256; ++i) {
		off  = (loff_t)i;
		path = voluta_t_new_path_unique(t_ctx);
		buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
		buf2 = voluta_t_new_buf_rands(t_ctx, bsz);
		voluta_t_open(path, o_flags, 0600, &fd);
		voluta_t_pwrite(fd, buf1, bsz, off, &cnt);
		voluta_t_pread(fd, buf2, bsz, off, &cnt);
		voluta_t_expect_eq(memcmp(buf1, buf2, bsz), 0);
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
	test_basic_reserve_at(t_ctx, 0, VOLUTA_BK_SIZE);
}

static void test_basic_reserve2(struct voluta_t_ctx *t_ctx)
{
	test_basic_reserve_at(t_ctx, 100000, 2 * VOLUTA_BK_SIZE);
}

static void test_basic_reserve3(struct voluta_t_ctx *t_ctx)
{
	test_basic_reserve_at(t_ctx, 9999999, VOLUTA_BK_SIZE - 1);
}

static void test_basic_reserve4(struct voluta_t_ctx *t_ctx)
{
	test_basic_reserve_at(t_ctx,  100003, 7 * VOLUTA_BK_SIZE);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects read-write data-consistency when I/O overlaps
 */
static void test_basic_overlap(struct voluta_t_ctx *t_ctx)
{
	int fd;
	loff_t off;
	size_t n, cnt, bsz = VOLUTA_MEGA;
	void *buf1, *buf2, *buf3;
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
	buf2 = voluta_t_new_buf_rands(t_ctx, bsz);
	buf3 = voluta_t_new_buf_zeros(t_ctx, bsz);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_pwrite(fd, buf1, bsz, 0, &n);
	voluta_t_pread(fd, buf3, bsz, 0, &n);
	voluta_t_expect_eqm(buf1, buf3, bsz);

	off = 17;
	cnt = 100;
	voluta_t_pwrite(fd, buf2, cnt, off, &n);
	voluta_t_pread(fd, buf3, cnt, off, &n);
	voluta_t_expect_eqm(buf2, buf3, cnt);

	off = 2099;
	cnt = 1000;
	voluta_t_pwrite(fd, buf2, cnt, off, &n);
	voluta_t_pread(fd, buf3, cnt, off, &n);
	voluta_t_expect_eqm(buf2, buf3, cnt);

	off = 32077;
	cnt = 10000;
	voluta_t_pwrite(fd, buf2, cnt, off, &n);
	voluta_t_pread(fd, buf3, cnt, off, &n);
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
	size_t nwr, bsz = VOLUTA_BK_SIZE;
	char *buf1, *buf2;
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
	loff_t pos, lim, step = VOLUTA_BK_SIZE;

	pos = 0;
	lim = VOLUTA_MEGA;
	test_basic_(t_ctx, pos, lim, step);
	lim = 2 * VOLUTA_MEGA;
	test_basic_(t_ctx, pos, lim, step);
	pos = VOLUTA_GIGA - VOLUTA_MEGA;
	lim = VOLUTA_MEGA;
	test_basic_(t_ctx, pos, lim, step);
}

static void test_basic_unaligned(struct voluta_t_ctx *t_ctx)
{
	loff_t pos, lim;
	const loff_t step1 = VOLUTA_BK_SIZE + 1;
	const loff_t step2 = VOLUTA_BK_SIZE - 1;

	pos = 0;
	lim = VOLUTA_MEGA;
	test_basic_(t_ctx, pos, lim, step1);
	test_basic_(t_ctx, pos, lim, step2);

	lim = 2 * VOLUTA_MEGA;
	test_basic_(t_ctx, pos, lim, step1);
	test_basic_(t_ctx, pos, lim, step2);

	pos = VOLUTA_GIGA - VOLUTA_MEGA;
	lim = VOLUTA_MEGA;
	test_basic_(t_ctx, pos, lim, step1);
	test_basic_(t_ctx, pos, lim, step2);
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
	size_t nwr = 0, nrd = 0;
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
	buf2 = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_pwrite(fd, buf1, bsz, off, &nwr);
	voluta_t_expect_eq(nwr, bsz);
	voluta_t_pread(fd, buf2, bsz, off, &nrd);
	voluta_t_expect_eq(nrd, bsz);
	voluta_t_expect_eqm(buf1, buf2, bsz);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_basic_chunk_x(struct voluta_t_ctx *t_ctx, size_t bsz)
{
	test_basic_chunk_(t_ctx, 0, bsz);
	test_basic_chunk_(t_ctx, VOLUTA_MEGA, bsz);
	test_basic_chunk_(t_ctx, 1, bsz);
	test_basic_chunk_(t_ctx, VOLUTA_MEGA - 1, bsz);
}

static void test_basic_chunk_1m(struct voluta_t_ctx *t_ctx)
{
	test_basic_chunk_x(t_ctx, VOLUTA_MEGA);
}

static void test_basic_chunk_2m(struct voluta_t_ctx *t_ctx)
{
	test_basic_chunk_x(t_ctx, 2 * VOLUTA_MEGA);
}

static void test_basic_chunk_4m(struct voluta_t_ctx *t_ctx)
{
	test_basic_chunk_x(t_ctx, 4 * VOLUTA_MEGA);
}

static void test_basic_chunk_8m(struct voluta_t_ctx *t_ctx)
{
	test_basic_chunk_x(t_ctx, 8 * VOLUTA_MEGA);
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

const struct voluta_t_tests
voluta_t_test_basic_io = VOLUTA_T_DEFTESTS(t_local_tests);
