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
#include "unitest.h"


static uint8_t iobuf_first_byte(const struct voluta_ut_iobuf *iobuf)
{
	return iobuf->buf[0];
}

static uint8_t iobuf_last_byte(const struct voluta_ut_iobuf *iobuf)
{
	return iobuf->buf[iobuf->len - 1];
}

static loff_t iobuf_last_off(const struct voluta_ut_iobuf *iobuf)
{
	return iobuf->off + (loff_t)iobuf->len - 1;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_rw_plus_minus_1_(struct voluta_ut_ctx *ut_ctx,
				ino_t ino, loff_t off, size_t len)
{
	uint8_t iob1_first;
	uint8_t iob2_last;
	uint8_t iob3_first;
	struct voluta_ut_iobuf *iob1, *iob2, *iob3, *iob4;

	iob1 = voluta_ut_new_iobuf(ut_ctx, off, len);
	voluta_ut_write_iobuf(ut_ctx, ino, iob1);
	iob2 = voluta_ut_new_iobuf(ut_ctx, off + 1, len);
	voluta_ut_write_iobuf(ut_ctx, ino, iob2);
	iob1_first = iobuf_first_byte(iob1);
	voluta_ut_read_only(ut_ctx, ino, &iob1_first, 1, iob1->off);
	iob3 = voluta_ut_new_iobuf(ut_ctx, off - 1, len);
	voluta_ut_write_iobuf(ut_ctx, ino, iob3);
	iob2_last = iobuf_last_byte(iob2);
	voluta_ut_read_only(ut_ctx, ino, &iob2_last, 1, iobuf_last_off(iob2));
	voluta_ut_fallocate_punch_hole(ut_ctx, ino, off, (loff_t)len);
	voluta_ut_read_zeros(ut_ctx, ino, off, len);
	iob3_first = iobuf_first_byte(iob3);
	voluta_ut_read_only(ut_ctx, ino, &iob3_first, 1, iob3->off);
	iob4 = voluta_ut_new_iobuf(ut_ctx, off, len);
	voluta_ut_write_iobuf(ut_ctx, ino, iob4);
	voluta_ut_fallocate_punch_hole(ut_ctx, ino, off - 1, (loff_t)len + 2);
	voluta_ut_read_zeros(ut_ctx, ino, off - 1, len + 2);
}

static void ut_file_edges_1_(struct voluta_ut_ctx *ut_ctx,
			     loff_t off, size_t len)
{
	ino_t ino;
	ino_t dino;
	const char *name = T_NAME;

	voluta_ut_mkdir_at_root(ut_ctx, name, &dino);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);
	ut_rw_plus_minus_1_(ut_ctx, ino, off, len);
	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_rmdir_at_root(ut_ctx, name);
}

static void ut_file_edges_aligned(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_edges_1_(ut_ctx, BK_SIZE, BK_SIZE);
	ut_file_edges_1_(ut_ctx, MEGA, 4 * BK_SIZE);
	ut_file_edges_1_(ut_ctx, GIGA, 8 * BK_SIZE);
	ut_file_edges_1_(ut_ctx, TERA, 16 * BK_SIZE);
}

static void ut_file_edges_unaligned(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_edges_1_(ut_ctx, BK_SIZE + 11, BK_SIZE - 1);
	ut_file_edges_1_(ut_ctx, MEGA - 1111, 4 * BK_SIZE + 1);
	ut_file_edges_1_(ut_ctx, GIGA - 11111, 8 * BK_SIZE + 11);
	ut_file_edges_1_(ut_ctx, TERA - 111111, 16 * BK_SIZE + 111);
}

static void ut_file_edges_special(struct voluta_ut_ctx *ut_ctx)
{
	const size_t bksz = BK_SIZE;
	const loff_t bkssz = (loff_t)bksz;
	const loff_t filemap_sz = (FILEMAP_NCHILD * BK_SIZE);
	const loff_t filemap_sz2 = filemap_sz * FILEMAP_NCHILD;
	const loff_t filesize_max = FILESIZE_MAX;

	ut_file_edges_1_(ut_ctx, filemap_sz, bksz);
	ut_file_edges_1_(ut_ctx, filemap_sz, 2 * bksz);
	ut_file_edges_1_(ut_ctx, filemap_sz - 11, bksz + 111);
	ut_file_edges_1_(ut_ctx, filemap_sz - 111, 2 * bksz + 1111);
	ut_file_edges_1_(ut_ctx, 2 * filemap_sz, 2 * bksz);
	ut_file_edges_1_(ut_ctx, 2 * filemap_sz - 1, bksz + 2);
	ut_file_edges_1_(ut_ctx, filemap_sz + filemap_sz2, 2 * bksz);
	ut_file_edges_1_(ut_ctx, filemap_sz + filemap_sz2 - 2, bksz + 3);
	ut_file_edges_1_(ut_ctx, filemap_sz2 - 2, bksz + 3);
	ut_file_edges_1_(ut_ctx, filesize_max / 2, bksz);
	ut_file_edges_1_(ut_ctx, filesize_max - (2 * bkssz), bksz);
	ut_file_edges_1_(ut_ctx, filesize_max - bkssz - 1, bksz);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_file_edges_aligned),
	UT_DEFTEST(ut_file_edges_unaligned),
	UT_DEFTEST(ut_file_edges_special),
};

const struct voluta_ut_tests voluta_ut_test_file_edges =
	UT_MKTESTS(ut_local_tests);
