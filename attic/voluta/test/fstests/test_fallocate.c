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
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include "fstests.h"


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects fallocate(2) to successfully allocate space, and return EBADF if
 * fd is not opened for writing.
 */
static void test_fallocate_basic(struct voluta_t_ctx *t_ctx)
{
	int fd;
	loff_t len = VOLUTA_BK_SIZE;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path, 0600, &fd);
	voluta_t_fallocate(fd, 0, 0, len);
	voluta_t_close(fd);
	voluta_t_open(path, O_RDONLY, 0, &fd);
	voluta_t_fallocate_err(fd, 0, len, 2 * len, -EBADF);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects fallocate(2) to successfully allocate space for file's sub-ranges.
 */
static void test_fallocate_(struct voluta_t_ctx *t_ctx, loff_t off, loff_t len)
{
	int fd, mode = 0;
	struct stat st;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path, 0600, &fd);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, 0);
	voluta_t_expect_eq(st.st_blocks, 0);
	voluta_t_fallocate(fd, mode, off, len);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, off + len);
	voluta_t_expect_gt(st.st_blocks, 0);
	voluta_t_ftruncate(fd, 0);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_fallocate_aligned(struct voluta_t_ctx *t_ctx)
{
	test_fallocate_(t_ctx, 0, VOLUTA_BK_SIZE);
	test_fallocate_(t_ctx, 0, VOLUTA_MEGA);
	test_fallocate_(t_ctx, VOLUTA_MEGA, VOLUTA_BK_SIZE);
	test_fallocate_(t_ctx, VOLUTA_GIGA, VOLUTA_MEGA);
	test_fallocate_(t_ctx, VOLUTA_TERA, VOLUTA_MEGA);
}

static void test_fallocate_unaligned(struct voluta_t_ctx *t_ctx)
{
	test_fallocate_(t_ctx, 0, 1);
	test_fallocate_(t_ctx, 0, VOLUTA_BK_SIZE - 1);
	test_fallocate_(t_ctx, 0, VOLUTA_MEGA - 1);
	test_fallocate_(t_ctx, VOLUTA_BK_SIZE, VOLUTA_BK_SIZE - 1);
	test_fallocate_(t_ctx, 1, VOLUTA_BK_SIZE + 3);
	test_fallocate_(t_ctx, VOLUTA_BK_SIZE - 1, VOLUTA_BK_SIZE + 3);
	test_fallocate_(t_ctx, 1, VOLUTA_MEGA + 3);
	test_fallocate_(t_ctx, VOLUTA_MEGA - 1, VOLUTA_MEGA + 3);
	test_fallocate_(t_ctx, VOLUTA_GIGA - 11, VOLUTA_MEGA + 11);
	test_fallocate_(t_ctx, VOLUTA_TERA - 111, VOLUTA_MEGA + 111);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects fallocate(2) to report allocated space as zero
 */
static void test_fallocate_zeros_(struct voluta_t_ctx *t_ctx, loff_t off,
				  loff_t len)
{
	int fd, mode = 0;
	uint8_t byte = 1, zero = 0, ab = 0xAB;
	size_t nrd, nwr;
	struct stat st;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_fallocate(fd, mode, off, len);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, off + len);
	voluta_t_pread(fd, &byte, 1, off, &nrd);
	voluta_t_expect_eq(byte, zero);
	voluta_t_pread(fd, &byte, 1, off + len - 1, &nrd);
	voluta_t_expect_eq(byte, zero);
	voluta_t_pwrite(fd, &ab, 1, off, &nwr);
	voluta_t_pread(fd, &byte, 1, off + 1, &nrd);
	voluta_t_expect_eq(byte, zero);
	voluta_t_pwrite(fd, &ab, 1, off + len - 1, &nwr);
	voluta_t_pread(fd, &byte, 1, off + len - 2, &nrd);
	voluta_t_expect_eq(byte, zero);
	voluta_t_unlink(path);
	voluta_t_close(fd);
}

static void test_fallocate_zeros(struct voluta_t_ctx *t_ctx)
{
	test_fallocate_zeros_(t_ctx, 0, VOLUTA_BK_SIZE);
	test_fallocate_zeros_(t_ctx, 0, VOLUTA_MEGA);
	test_fallocate_zeros_(t_ctx, VOLUTA_MEGA, VOLUTA_BK_SIZE);
	test_fallocate_zeros_(t_ctx, VOLUTA_GIGA, VOLUTA_MEGA);
	test_fallocate_zeros_(t_ctx, VOLUTA_TERA, VOLUTA_MEGA);
	test_fallocate_zeros_(t_ctx, 1, VOLUTA_MEGA + 3);
	test_fallocate_zeros_(t_ctx, VOLUTA_MEGA - 1, VOLUTA_MEGA + 11);
	test_fallocate_zeros_(t_ctx, VOLUTA_GIGA - 11, VOLUTA_MEGA + 111);
	test_fallocate_zeros_(t_ctx, VOLUTA_TERA - 111, VOLUTA_MEGA + 1111);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects fallocate(2) with FALLOC_FL_PUNCH_HOLE to return zeros on hole
 */
static void test_fallocate_punch_hole_(struct voluta_t_ctx *t_ctx,
				       loff_t data_off, size_t data_len,
				       loff_t hole_off, size_t hole_len)
{
	int fd;
	loff_t pos;
	size_t nwr, nrd;
	void *buf;
	uint8_t byte;
	int mode = FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	buf = voluta_t_new_buf_rands(t_ctx, data_len);
	voluta_t_pwrite(fd, buf, data_len, data_off, &nwr);
	voluta_t_expect_eq(nwr, data_len);
	voluta_t_fallocate(fd, mode, hole_off, (loff_t)hole_len);
	voluta_t_pread(fd, &byte, 1, hole_off, &nrd);
	voluta_t_expect_eq(nrd, 1);
	voluta_t_expect_eq(byte, 0);
	pos = hole_off + (loff_t)(hole_len - 1);
	voluta_t_pread(fd, &byte, 1, pos, &nrd);
	voluta_t_expect_eq(nrd, 1);
	voluta_t_expect_eq(byte, 0);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_fallocate_punch_hole(struct voluta_t_ctx *t_ctx)
{
	test_fallocate_punch_hole_(t_ctx, 0, 1024, 0, 512);
	test_fallocate_punch_hole_(t_ctx, 0, VOLUTA_BK_SIZE, 0, 32);
	test_fallocate_punch_hole_(t_ctx, 0, VOLUTA_BK_SIZE, 1, 17);
	test_fallocate_punch_hole_(t_ctx, VOLUTA_BK_SIZE, VOLUTA_BK_SIZE,
				   VOLUTA_BK_SIZE + 1, VOLUTA_BK_SIZE - 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests fallocate(2) with FALLOC_FL_PUNCH_HOLE on various corner cases
 */
static void
test_fallocate_punch_into_hole_(struct voluta_t_ctx *t_ctx, loff_t base_off)
{
	int fd;
	loff_t off, off_end;
	size_t nrd;
	void *buf1, *buf2, *buf3;
	struct stat st, st2;
	int mode = FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE;
	const size_t size = VOLUTA_MEGA;
	const loff_t zlen = VOLUTA_MEGA / 4;
	const char *path = voluta_t_new_path_unique(t_ctx);

	off = base_off;
	off_end = base_off + (loff_t)size;
	buf1 = voluta_t_new_buf_rands(t_ctx, size);
	buf2 = voluta_t_new_buf_zeros(t_ctx, size);
	buf3 = voluta_t_new_buf_rands(t_ctx, size);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_ftruncate(fd, off_end);
	voluta_t_fallocate(fd, mode, off, zlen);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_blocks, 0);
	voluta_t_pread(fd, buf1, size, off, &nrd);
	voluta_t_expect_eq(nrd, size);
	voluta_t_expect_eqm(buf1, buf2, size);
	voluta_t_fallocate(fd, mode, off_end - zlen, zlen);
	voluta_t_pread(fd, buf1, size, off_end - zlen, &nrd);
	voluta_t_expect_eq(nrd, zlen);
	voluta_t_expect_eqm(buf1, buf2, (size_t)zlen);
	voluta_t_pwrite(fd, buf3, (size_t)zlen, off, &nrd);
	voluta_t_expect_eq(nrd, zlen);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_gt(st.st_blocks, 0);
	voluta_t_fallocate(fd, mode, off + zlen, zlen);
	voluta_t_fstat(fd, &st2);
	voluta_t_expect_eq(st.st_blocks, st2.st_blocks);
	voluta_t_fallocate(fd, mode, off, zlen);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_lt(st.st_blocks, st2.st_blocks);
	voluta_t_ftruncate(fd, 0);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_fallocate_punch_into_hole(struct voluta_t_ctx *t_ctx)
{
	test_fallocate_punch_into_hole_(t_ctx, 0);
	test_fallocate_punch_into_hole_(t_ctx, VOLUTA_MEGA);
	test_fallocate_punch_into_hole_(t_ctx, VOLUTA_MEGA - 1);
	test_fallocate_punch_into_hole_(t_ctx, VOLUTA_GIGA);
	test_fallocate_punch_into_hole_(t_ctx, VOLUTA_GIGA + 1);
	test_fallocate_punch_into_hole_(t_ctx, VOLUTA_TERA);
	test_fallocate_punch_into_hole_(t_ctx, VOLUTA_TERA - 1);
	test_fallocate_punch_into_hole_(t_ctx, VOLUTA_FILESIZE_MAX / 2);
	test_fallocate_punch_into_hole_(t_ctx, (VOLUTA_FILESIZE_MAX / 2) + 1);
}


static void test_fallocate_punch_into_allocated(struct voluta_t_ctx *t_ctx)
{
	int fd;
	loff_t pos;
	size_t nwr, nrd;
	char *buf1, *buf2;
	int mode = FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE;
	const size_t size = 20 * VOLUTA_KILO;
	const size_t nzeros = 4 * VOLUTA_KILO;
	const loff_t off = (loff_t)nzeros;
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf1 = voluta_t_new_buf_rands(t_ctx, size);
	buf2 = voluta_t_new_buf_zeros(t_ctx, size);
	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_ftruncate(fd, (loff_t)size);
	voluta_t_pwrite(fd, buf2, nzeros, off, &nwr);
	voluta_t_expect_eq(nwr, nzeros);
	voluta_t_pread(fd, buf1, size, 0, &nrd);
	voluta_t_expect_eq(nrd, size);
	voluta_t_expect_eqm(buf1, buf2, size);
	voluta_t_llseek(fd, 0, SEEK_SET, &pos);
	voluta_t_expect_eq(pos, 0);
	voluta_t_llseek(fd, 0, SEEK_DATA, &pos);
	voluta_t_expect_eq(pos, off);
	voluta_t_fallocate(fd, mode, off, off);
	voluta_t_pread(fd, buf1, size, 0, &nrd);
	voluta_t_expect_eq(nrd, size);
	voluta_t_expect_eqm(buf1, buf2, size);

	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests fallocate(2) with FALLOC_FL_COLLAPSE_RANGE on continues range.
 */
static void
test_collpase_range_continues(struct voluta_t_ctx *t_ctx, loff_t base)
{
	int fd, mode = FALLOC_FL_COLLAPSE_RANGE;
	loff_t off1, off2, off3;
	loff_t slen1, slen2, slen3;
	size_t nwr, nrd, len1, len2, len3, len4;
	char *path, *buf1, *buf2, *buf3, *buf4;
	struct stat st;

	off1 = base;
	len1 = 32 * VOLUTA_BK_SIZE;
	slen1 = (loff_t)len1;
	buf1 = voluta_t_new_buf_rands(t_ctx, len1);
	off2 = off1 + (loff_t)(len1);
	len2 = 16 * VOLUTA_BK_SIZE;
	slen2 = (loff_t)len2;
	buf2 = voluta_t_new_buf_rands(t_ctx, len2);
	off3 = off2 + (loff_t)(len2);
	len3 = 8 * VOLUTA_BK_SIZE;
	slen3 = (loff_t)len3;
	buf3 = voluta_t_new_buf_rands(t_ctx, len3);
	len4 = len1 + len2 + len3;
	buf4 = voluta_t_new_buf_rands(t_ctx, len4);

	path = voluta_t_new_path_unique(t_ctx);
	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_pwrite(fd, buf1, len1, off1, &nwr);
	voluta_t_expect_eq(nwr, len1);
	voluta_t_pwrite(fd, buf2, len2, off2, &nwr);
	voluta_t_expect_eq(nwr, len2);
	voluta_t_pwrite(fd, buf3, len3, off3, &nwr);
	voluta_t_expect_eq(nwr, len3);

	voluta_t_fallocate(fd, mode, off2, slen2);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, off1 + slen1 + slen3);
	voluta_t_pread(fd, buf4, len1 + len3, off1, &nrd);
	voluta_t_expect_eq(nrd, len1 + len3);
	voluta_t_expect_eqm(buf1, buf4, len1);

	voluta_t_fallocate(fd, mode, off1, slen1);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, off1 + slen3);
	voluta_t_pread(fd, buf4, len3, off1, &nrd);
	voluta_t_expect_eq(nrd, len3);
	voluta_t_expect_eqm(buf3, buf4, len3);

	voluta_t_ftruncate(fd, 0);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_fallocate_collpase_range_simple(struct voluta_t_ctx *t_ctx)
{
	test_collpase_range_continues(t_ctx, 0);
	test_collpase_range_continues(t_ctx, VOLUTA_BK_SIZE);
	test_collpase_range_continues(t_ctx, VOLUTA_MEGA);
	test_collpase_range_continues(t_ctx, VOLUTA_GIGA);
}

/*
 * Tests fallocate(2) with FALLOC_FL_COLLAPSE_RANGE on range with holes.
 */
static void test_collpase_range_with_holes(struct voluta_t_ctx *t_ctx,
		loff_t base, loff_t holesz)
{
	int fd, mode = FALLOC_FL_COLLAPSE_RANGE;
	loff_t off1, off2, off3;
	loff_t slen1, slen2, slen3;
	size_t nwr, nrd, len1, len2, len3, len4;
	char *buf1, *buf2, *buf3, *buf4;
	struct stat st;
	const char *path = voluta_t_new_path_unique(t_ctx);

	off1 = base;
	len1 = 32 * VOLUTA_BK_SIZE;
	slen1 = (loff_t)len1;
	buf1 = voluta_t_new_buf_rands(t_ctx, len1);
	off2 = off1 + holesz + slen1;
	len2 = 16 * VOLUTA_BK_SIZE;
	slen2 = (loff_t)len2;
	buf2 = voluta_t_new_buf_rands(t_ctx, len2);
	off3 = off2 + holesz + slen2;
	len3 = 8 * VOLUTA_BK_SIZE;
	slen3 = (loff_t)len3;
	buf3 = voluta_t_new_buf_rands(t_ctx, len3);
	len4 = len1 + len2 + len3;
	buf4 = voluta_t_new_buf_rands(t_ctx, len4);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_pwrite(fd, buf1, len1, off1, &nwr);
	voluta_t_expect_eq(nwr, len1);
	voluta_t_pwrite(fd, buf2, len2, off2, &nwr);
	voluta_t_expect_eq(nwr, len2);
	voluta_t_pwrite(fd, buf3, len3, off3, &nwr);
	voluta_t_expect_eq(nwr, len3);

	voluta_t_fallocate(fd, mode, off3 - holesz, holesz);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, off3 + slen3 - holesz);
	voluta_t_fallocate(fd, mode, off2 - holesz, holesz);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, off1 + slen1 + slen2 + slen3);
	voluta_t_pread(fd, buf4, len4, off1, &nrd);
	voluta_t_expect_eq(nrd, len4);
	voluta_t_expect_eqm(buf1, buf4, len1);
	voluta_t_expect_eqm(buf2, &buf4[len1], len2);
	voluta_t_expect_eqm(buf3, &buf4[len1 + len2], len3);

	voluta_t_fallocate(fd, mode, off1, slen1);
	voluta_t_fallocate(fd, mode, off1, slen2);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, off1 + slen3);
	voluta_t_pread(fd, buf4, len3, off1, &nrd);
	voluta_t_expect_eq(nrd, len3);
	voluta_t_expect_eqm(buf3, buf4, len3);

	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_fallocate_collpase_range_holes(struct voluta_t_ctx *t_ctx)
{
	test_collpase_range_with_holes(t_ctx, 0, VOLUTA_BK_SIZE);
	test_collpase_range_with_holes(t_ctx, VOLUTA_BK_SIZE, VOLUTA_BK_SIZE);
	test_collpase_range_with_holes(t_ctx, VOLUTA_MEGA, VOLUTA_BK_SIZE);
	test_collpase_range_with_holes(t_ctx, VOLUTA_GIGA, VOLUTA_BK_SIZE);
	test_collpase_range_with_holes(t_ctx, 0, VOLUTA_MEGA);
	test_collpase_range_with_holes(t_ctx, VOLUTA_BK_SIZE, VOLUTA_MEGA);
	test_collpase_range_with_holes(t_ctx, VOLUTA_MEGA, VOLUTA_MEGA);
	test_collpase_range_with_holes(t_ctx, VOLUTA_GIGA, VOLUTA_MEGA);
}


/*
 * Tests fallocate(2) with FALLOC_FL_ZERO_RANGE on range with data
 */
struct voluta_t_zero_range_info {
	loff_t data_off;
	size_t data_sz;
	loff_t zero_off;
	size_t zero_sz;
};

static void test_zero_range_data_(struct voluta_t_ctx *t_ctx,
				  const struct voluta_t_zero_range_info *zri)
{
	int fd, mode = FALLOC_FL_ZERO_RANGE;
	size_t nwr, nrd, len1, len2, pos0, pos1, pos2;
	const loff_t data_off = zri->data_off;
	const size_t data_sz = zri->data_sz;
	const loff_t zero_off = zri->zero_off;
	const size_t zero_sz = zri->zero_sz;
	const char *path = voluta_t_new_path_unique(t_ctx);
	uint8_t *data_buf1 = voluta_t_new_buf_rands(t_ctx, data_sz);
	uint8_t *data_buf2 = voluta_t_new_buf_rands(t_ctx, data_sz);
	uint8_t *zeros_buf = voluta_t_new_buf_zeros(t_ctx, zero_sz);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_pwrite(fd, data_buf1, data_sz, data_off, &nwr);
	voluta_t_fallocate(fd, mode, zero_off, (loff_t)zero_sz);
	voluta_t_pread(fd, data_buf2, data_sz, data_off, &nrd);

	pos0 = 0;
	len1 = (size_t)(zero_off - data_off);
	pos1 = len1;
	pos2 = pos1 + zero_sz;
	len2 = (size_t)(data_sz - pos2);
	voluta_t_expect_eqm(data_buf2 + pos0, data_buf1 + pos0, len1);
	voluta_t_expect_eqm(data_buf2 + pos1, zeros_buf, zero_sz);
	voluta_t_expect_eqm(data_buf1 + pos2, data_buf2 + pos2, len2);

	voluta_t_ftruncate(fd, 0);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static const struct voluta_t_zero_range_info s_zero_ranges[] = {
	{ 0, VOLUTA_BK_SIZE, 1, VOLUTA_BK_SIZE - 1 },
	{ 0, 2 * VOLUTA_BK_SIZE, VOLUTA_BK_SIZE - 1, 2 },
	{
		(2 * VOLUTA_BK_SIZE) - 3, 3 * VOLUTA_BK_SIZE,
		(3 * VOLUTA_BK_SIZE) - 1, VOLUTA_BK_SIZE + 3
	},
	{ 0, VOLUTA_MEGA, 0, VOLUTA_BK_SIZE },
	{ 1, VOLUTA_MEGA, 3, 3 * VOLUTA_BK_SIZE },
	{ VOLUTA_MEGA - 13, VOLUTA_MEGA + 29, VOLUTA_MEGA - 11, VOLUTA_MEGA },
	{ VOLUTA_GIGA - 11, VOLUTA_MEGA + 11, VOLUTA_GIGA - 3, VOLUTA_MEGA + 3 }
};

static void test_fallocate_zero_range_data(struct voluta_t_ctx *t_ctx)
{
	for (size_t i = 0; i < VOLUTA_ARRAY_SIZE(s_zero_ranges); ++i) {
		test_zero_range_data_(t_ctx, &s_zero_ranges[i]);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects fallocate(2) on sparse file change size and blocks count. Expects
 * wrtie-on-fallocated to change none.
 */
static void test_fallocate_sparse_(struct voluta_t_ctx *t_ctx,
				   loff_t base_off, loff_t step_size)
{
	int fd;
	size_t nwr, nrd;
	loff_t off, len, tmp;
	blkcnt_t blocks;
	struct stat st;
	const char *path = voluta_t_new_path_unique(t_ctx);
	const long cnt = 1024;

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, 0);
	voluta_t_expect_eq(st.st_blocks, 0);

	blocks = 0;
	off = base_off;
	for (long i = 0; i < cnt; ++i) {
		off = base_off + (i * step_size);
		len = (int)sizeof(off);
		voluta_t_fallocate(fd, 0, off, len);
		voluta_t_fstat(fd, &st);
		voluta_t_expect_eq(st.st_size, off + len);
		voluta_t_expect_gt(st.st_blocks, blocks);
		voluta_t_pread(fd, &tmp, (size_t)len, off, &nrd);
		voluta_t_expect_eq(nrd, len);
		voluta_t_expect_eq(tmp, 0);
		blocks = st.st_blocks;
		voluta_t_pwrite(fd, &off, (size_t)len, off, &nwr);
		voluta_t_expect_eq(nwr, len);
		voluta_t_fstat(fd, &st);
		voluta_t_expect_eq(st.st_size, off + len);
		voluta_t_expect_eq(st.st_blocks, blocks);
		voluta_t_pread(fd, &tmp, (size_t)len, off, &nrd);
		voluta_t_expect_eq(nrd, len);
		voluta_t_expect_eq(tmp, off);
	}
	voluta_t_ftruncate(fd, 0);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_fallocate_sparse(struct voluta_t_ctx *t_ctx)
{
	test_fallocate_sparse_(t_ctx, 0, VOLUTA_MEGA);
	test_fallocate_sparse_(t_ctx, 1, VOLUTA_MEGA);
	test_fallocate_sparse_(t_ctx, VOLUTA_MEGA, VOLUTA_GIGA);
	test_fallocate_sparse_(t_ctx, 11 * VOLUTA_MEGA + 1, VOLUTA_GIGA);
	test_fallocate_sparse_(t_ctx, 111 * VOLUTA_TERA + 11, VOLUTA_GIGA);
	test_fallocate_sparse_(t_ctx, VOLUTA_FILESIZE_MAX / 2, VOLUTA_GIGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_fallocate_basic),
	VOLUTA_T_DEFTEST(test_fallocate_aligned),
	VOLUTA_T_DEFTEST(test_fallocate_unaligned),
	VOLUTA_T_DEFTEST(test_fallocate_zeros),
	VOLUTA_T_DEFTEST(test_fallocate_punch_hole),
	VOLUTA_T_DEFTEST(test_fallocate_sparse),
	VOLUTA_T_DEFTEST(test_fallocate_punch_into_hole),

	/* Unsupported, yet */
	VOLUTA_T_DEFTESTF(test_fallocate_punch_into_allocated,
			  VOLUTA_T_IO_EXTRA),
	VOLUTA_T_DEFTESTF(test_fallocate_collpase_range_simple,
			  VOLUTA_T_IO_EXTRA),
	VOLUTA_T_DEFTESTF(test_fallocate_collpase_range_holes,
			  VOLUTA_T_IO_EXTRA),
	VOLUTA_T_DEFTESTF(test_fallocate_zero_range_data,
			  VOLUTA_T_IO_EXTRA),
};

const struct voluta_t_tests
voluta_t_test_fallocate = VOLUTA_T_DEFTESTS(t_local_tests);
