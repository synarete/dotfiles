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

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_falloc_simple_(struct voluta_ut_ctx *ut_ctx,
				   loff_t off, size_t bsz)
{
	ino_t ino;
	const char *name = T_NAME;
	void *buf = ut_randbuf(ut_ctx, bsz);
	const ino_t root_ino = ROOT_INO;

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	voluta_ut_fallocate_reserve(ut_ctx, ino, off, (loff_t)bsz);
	voluta_ut_write_read(ut_ctx, ino, buf, bsz, off);
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_falloc_aligned(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_falloc_simple_(ut_ctx, 0, BK_SIZE);
	ut_file_falloc_simple_(ut_ctx, 0, UMEGA);
	ut_file_falloc_simple_(ut_ctx, MEGA, UMEGA);
	ut_file_falloc_simple_(ut_ctx, GIGA, UMEGA);
	ut_file_falloc_simple_(ut_ctx, TERA, UMEGA);
}

static void ut_file_falloc_unaligned(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_falloc_simple_(ut_ctx, 1, 3 * BK_SIZE);
	ut_file_falloc_simple_(ut_ctx, 3, UMEGA / 3);
	ut_file_falloc_simple_(ut_ctx, 5 * MEGA, UMEGA / 5);
	ut_file_falloc_simple_(ut_ctx, 7 * GIGA, UMEGA / 7);
	ut_file_falloc_simple_(ut_ctx, TERA, UMEGA / 11);
	ut_file_falloc_simple_(ut_ctx, FILESIZE_MAX / 2, UMEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_falloc_read_(struct voluta_ut_ctx *ut_ctx,
				 loff_t off, size_t cnt)
{
	uint8_t zero = 0;
	ino_t ino;
	const char *name = T_NAME;
	const ino_t root_ino = ROOT_INO;

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	voluta_ut_fallocate_reserve(ut_ctx, ino, off, (loff_t)cnt);
	voluta_ut_read_verify(ut_ctx, ino, &zero, 1, off);
	voluta_ut_read_verify(ut_ctx, ino, &zero, 1, off + (loff_t)cnt - 1);
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_falloc_read(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_falloc_read_(ut_ctx, 0, BK_SIZE);
	ut_file_falloc_read_(ut_ctx, 1, UMEGA);
	ut_file_falloc_read_(ut_ctx, 0, VOLUTA_AG_SIZE);
	ut_file_falloc_read_(ut_ctx, MEGA - 1, VOLUTA_AG_SIZE + 2);
	ut_file_falloc_read_(ut_ctx, GIGA, UMEGA);
	ut_file_falloc_read_(ut_ctx, TERA - 2, VOLUTA_AG_SIZE + 3);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_falloc_unwritten_(struct voluta_ut_ctx *ut_ctx,
				      loff_t off, size_t bsz)
{
	ino_t ino;
	ino_t dino;
	const loff_t len = (loff_t)bsz;
	const char *name = T_NAME;
	const uint8_t b = 1;

	voluta_ut_mkdir_at_root(ut_ctx, name, &dino);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);
	voluta_ut_fallocate_reserve(ut_ctx, ino, off, len);
	voluta_ut_read_zeros(ut_ctx, ino, off, bsz);
	voluta_ut_write_read(ut_ctx, ino, &b, 1, off);
	voluta_ut_write_read(ut_ctx, ino, &b, 1, off + len - 1);
	voluta_ut_read_zeros(ut_ctx, ino, off + 1, bsz - 2);
	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_rmdir_at_root(ut_ctx, name);
}

static void ut_file_falloc_unwritten(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_falloc_unwritten_(ut_ctx, 0, BK_SIZE);
	ut_file_falloc_unwritten_(ut_ctx, MEGA, 2 * BK_SIZE);
	ut_file_falloc_unwritten_(ut_ctx, GIGA, 3 * BK_SIZE);
	ut_file_falloc_unwritten_(ut_ctx, TERA, 4 * BK_SIZE);
	ut_file_falloc_unwritten_(ut_ctx, MEGA - 111, UMEGA + 1111);
	ut_file_falloc_unwritten_(ut_ctx, GIGA - 1111, UMEGA + 111);
	ut_file_falloc_unwritten_(ut_ctx, TERA - 11111, UMEGA + 11);
	ut_file_falloc_unwritten_(ut_ctx, FILESIZE_MAX - 111111, 111111);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_falloc_drop_caches_(struct voluta_ut_ctx *ut_ctx,
					loff_t off, size_t bsz)
{
	ino_t ino, root_ino = ROOT_INO;
	const char *name = T_NAME;
	void *buf = ut_randbuf(ut_ctx, bsz);

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	voluta_ut_fallocate_reserve(ut_ctx, ino, off, (loff_t)bsz);
	voluta_ut_release_file(ut_ctx, ino);
	voluta_ut_sync_drop(ut_ctx);
	voluta_ut_open_rdwr(ut_ctx, ino);
	voluta_ut_read_zero(ut_ctx, ino, off);
	voluta_ut_read_zero(ut_ctx, ino, off + (loff_t)(bsz - 1));
	voluta_ut_write_read(ut_ctx, ino, buf, bsz, off);
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_falloc_drop_caches(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_falloc_drop_caches_(ut_ctx, 0, UMEGA);
	ut_file_falloc_drop_caches_(ut_ctx, 3, UMEGA / 3);
	ut_file_falloc_drop_caches_(ut_ctx, 5 * MEGA + 5, UMEGA / 5);
	ut_file_falloc_drop_caches_(ut_ctx, TERA / 11, UMEGA / 11);
	ut_file_falloc_drop_caches_(ut_ctx, FILESIZE_MAX / 2, UMEGA);
	ut_file_falloc_drop_caches_(ut_ctx,
				    FILESIZE_MAX - UMEGA - 11, UMEGA + 11);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_falloc_punch_hole_(struct voluta_ut_ctx *ut_ctx,
				       loff_t off1, loff_t off2, size_t bsz)
{
	ino_t ino, root_ino = ROOT_INO;
	const char *name = T_NAME;
	uint8_t zero[1] = { 0 };
	uint8_t *buf = ut_randbuf(ut_ctx, bsz);

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	voluta_ut_write_read(ut_ctx, ino, buf, bsz, off1);
	voluta_ut_write_read(ut_ctx, ino, buf, bsz, off2);
	voluta_ut_fallocate_punch_hole(ut_ctx, ino, off1, off2 - off1);
	voluta_ut_read_verify(ut_ctx, ino, zero, 1, off1);
	voluta_ut_read_verify(ut_ctx, ino, zero, 1, off2 - 1);

	voluta_ut_fallocate_punch_hole(ut_ctx, ino, off1, off2 - off1 + 1);
	voluta_ut_read_verify(ut_ctx, ino, zero, 1, off2);
	voluta_ut_read_verify(ut_ctx, ino, buf + 1, 1 /* bsz - 1 */, off2 + 1);
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_falloc_punch_hole(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_falloc_punch_hole_(ut_ctx, 0, BK_SIZE, BK_SIZE);
	ut_file_falloc_punch_hole_(ut_ctx, 0, MEGA, BK_SIZE);
	ut_file_falloc_punch_hole_(ut_ctx, 0, GIGA, UMEGA);
	ut_file_falloc_punch_hole_(ut_ctx, 0, TERA, UMEGA);

	ut_file_falloc_punch_hole_(ut_ctx, MEGA, 2 * MEGA, BK_SIZE);
	ut_file_falloc_punch_hole_(ut_ctx, MEGA, GIGA, UMEGA);
	ut_file_falloc_punch_hole_(ut_ctx, MEGA, TERA, UMEGA);
	ut_file_falloc_punch_hole_(ut_ctx, GIGA, TERA, UMEGA);

	ut_file_falloc_punch_hole_(ut_ctx, 7, 7 * BK_SIZE - 7, BK_SIZE);
	ut_file_falloc_punch_hole_(ut_ctx, 77, 7 * MEGA, 7 * BK_SIZE + 7);
	ut_file_falloc_punch_hole_(ut_ctx, 777, 7 * GIGA - 7, UMEGA + 77);
	ut_file_falloc_punch_hole_(ut_ctx, 7777, TERA - 7, UMEGA + 77);

	ut_file_falloc_punch_hole_(ut_ctx, 77 * MEGA - 7,
				   7 * GIGA - 7, UMEGA + 77);
	ut_file_falloc_punch_hole_(ut_ctx, 777 * GIGA + 77,
				   TERA - 7, UMEGA + 77);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_falloc_punch_hole2_(struct voluta_ut_ctx *ut_ctx,
					loff_t off, size_t bsz)
{
	ino_t ino, dino, root_ino = ROOT_INO;
	const loff_t off1 = off + (loff_t)bsz;
	const loff_t off2 = off1 + (loff_t)bsz;
	const char *name = T_NAME;
	uint8_t *buf = ut_randbuf(ut_ctx, bsz);

	voluta_ut_mkdir_ok(ut_ctx, root_ino, name, &dino);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);
	voluta_ut_write_read(ut_ctx, ino, buf, bsz, off);
	voluta_ut_trunacate_file(ut_ctx, ino, off2);
	voluta_ut_fallocate_punch_hole(ut_ctx, ino, off1, off2 - off1);
	voluta_ut_read_zero(ut_ctx, ino, off1);
	voluta_ut_read_zero(ut_ctx, ino, off2 - 1);
	voluta_ut_fallocate_punch_hole(ut_ctx, ino, off1 - 1, off2 - off1 - 1);
	voluta_ut_read_zero(ut_ctx, ino, off1 - 1);
	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_rmdir_ok(ut_ctx, root_ino, name);
}

static void ut_file_falloc_punch_hole2(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_falloc_punch_hole2_(ut_ctx, 0, UMEGA);
	ut_file_falloc_punch_hole2_(ut_ctx, MEGA, UMEGA);
	ut_file_falloc_punch_hole2_(ut_ctx, GIGA, UMEGA);
	ut_file_falloc_punch_hole2_(ut_ctx, TERA, UMEGA);
	ut_file_falloc_punch_hole2_(ut_ctx, MEGA - 11, UMEGA + 111);
	ut_file_falloc_punch_hole2_(ut_ctx, GIGA - 111, UMEGA + 11);
	ut_file_falloc_punch_hole2_(ut_ctx, TERA - 1111, UMEGA + 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static loff_t off_to_nbk(loff_t off)
{
	return off / BK_SIZE;
}

static loff_t off_to_nbk_up(loff_t off)
{
	return off_to_nbk(off + BK_SIZE - 1);
}

static blkcnt_t blocks_count_of(loff_t off, loff_t len)
{
	loff_t lba_beg, lba_end, length;

	lba_beg = off_to_nbk(off);
	lba_end = off_to_nbk_up(off + len);
	length = (lba_end - lba_beg) * BK_SIZE;

	return length / 512;
}

static void ut_file_falloc_stat_(struct voluta_ut_ctx *ut_ctx,
				 loff_t base_off, loff_t len, loff_t step_size)
{
	loff_t off;
	blkcnt_t nblk;
	ino_t ino, dino, root_ino = ROOT_INO;
	struct stat st1, st2;
	const char *name = T_NAME;
	const size_t cnt = 64;

	ut_assert_eq(base_off % BK_SIZE, 0);
	ut_assert_eq(step_size % BK_SIZE, 0);
	ut_assert_le(len, step_size);
	voluta_ut_mkdir_ok(ut_ctx, root_ino, name, &dino);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);

	voluta_ut_getattr(ut_ctx, ino, &st1);
	ut_assert_eq(st1.st_size, 0);
	ut_assert_eq(st1.st_blocks, 0);

	off = base_off;
	for (size_t i = 0; i < cnt; ++i) {
		nblk = blocks_count_of(off, len);

		voluta_ut_getattr(ut_ctx, ino, &st1);
		voluta_ut_fallocate_reserve(ut_ctx, ino, off, len);
		voluta_ut_getattr(ut_ctx, ino, &st2);

		ut_assert_eq(off + len, st2.st_size);
		ut_assert_eq(st1.st_blocks + nblk, st2.st_blocks);
		off += step_size;
	}
	off = base_off;
	for (size_t j = 0; j < cnt; ++j) {
		nblk = blocks_count_of(off, len);

		voluta_ut_getattr(ut_ctx, ino, &st1);
		voluta_ut_fallocate_punch_hole(ut_ctx, ino, off, nblk * 512);
		voluta_ut_getattr(ut_ctx, ino, &st2);
		ut_assert_eq(st1.st_blocks - nblk, st2.st_blocks);
		off += step_size;
	}
	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_rmdir_ok(ut_ctx, root_ino, name);
}

static void ut_file_falloc_stat(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_falloc_stat_(ut_ctx, 0, BK_SIZE, BK_SIZE);
	ut_file_falloc_stat_(ut_ctx, 0, BK_SIZE - 1, BK_SIZE);
	ut_file_falloc_stat_(ut_ctx, BK_SIZE, BK_SIZE - 3, BK_SIZE);
	ut_file_falloc_stat_(ut_ctx, 0, MEGA, MEGA);
	ut_file_falloc_stat_(ut_ctx, BK_SIZE, BK_SIZE, MEGA);
	ut_file_falloc_stat_(ut_ctx, MEGA, MEGA - 1, UMEGA);
	ut_file_falloc_stat_(ut_ctx, GIGA, MEGA - 11, 11 * UMEGA);
	ut_file_falloc_stat_(ut_ctx, TERA, MEGA - 111, 111 * UMEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_falloc_sparse_(struct voluta_ut_ctx *ut_ctx,
				   loff_t base_off, loff_t step_size)
{
	loff_t off, len, zero = 0;
	blkcnt_t blocks;
	ino_t ino, dino, root_ino = ROOT_INO;
	struct stat st;
	const char *name = T_NAME;
	const long cnt = 1024;

	voluta_ut_mkdir_ok(ut_ctx, root_ino, name, &dino);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);

	voluta_ut_getattr(ut_ctx, ino, &st);
	ut_assert_eq(st.st_size, 0);
	ut_assert_eq(st.st_blocks, 0);

	blocks = 0;
	off = base_off;
	for (long i = 0; i < cnt; ++i) {
		off = base_off + (i * step_size);
		len = (int)sizeof(off);
		voluta_ut_fallocate_reserve(ut_ctx, ino, off, len);
		voluta_ut_getattr_file(ut_ctx, ino, &st);
		ut_assert_eq(st.st_size, off + len);
		ut_assert_gt(st.st_blocks, blocks);
		voluta_ut_read_verify(ut_ctx, ino, &zero, (size_t)len, off);

		blocks = st.st_blocks;
		voluta_ut_write_read(ut_ctx, ino, &off, (size_t)len, off);
		voluta_ut_getattr_file(ut_ctx, ino, &st);
		ut_assert_eq(st.st_size, off + len);
		ut_assert_eq(st.st_blocks, blocks);
	}

	voluta_ut_trunacate_file(ut_ctx, ino, 0);
	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_rmdir_ok(ut_ctx, root_ino, name);
}

static void ut_file_falloc_sparse(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_falloc_sparse_(ut_ctx, 0, MEGA);
	ut_file_falloc_sparse_(ut_ctx, 1, MEGA);
	ut_file_falloc_sparse_(ut_ctx, MEGA, GIGA);
	ut_file_falloc_sparse_(ut_ctx, 11 * MEGA - 1, GIGA);
	ut_file_falloc_sparse_(ut_ctx, TERA - 11, GIGA);
	ut_file_falloc_sparse_(ut_ctx, FILESIZE_MAX / 2, GIGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_file_falloc_aligned),
	UT_DEFTEST(ut_file_falloc_unaligned),
	UT_DEFTEST(ut_file_falloc_read),
	UT_DEFTEST(ut_file_falloc_unwritten),
	UT_DEFTEST(ut_file_falloc_drop_caches),
	UT_DEFTEST(ut_file_falloc_punch_hole),
	UT_DEFTEST(ut_file_falloc_punch_hole2),
	UT_DEFTEST(ut_file_falloc_stat),
	UT_DEFTEST(ut_file_falloc_sparse),
};

const struct voluta_ut_tests voluta_ut_test_file_fallocate =
	UT_MKTESTS(ut_local_tests);

