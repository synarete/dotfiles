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
#include "unitest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_falloc_simple_(struct voluta_ut_ctx *ut_ctx,
				   loff_t off, size_t bsz)
{
	ino_t ino, root_ino = VOLUTA_INO_ROOT;
	const char *name = UT_NAME;
	void *buf = ut_randbuf(ut_ctx, bsz);

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	voluta_ut_fallocate_reserve(ut_ctx, ino, off, (loff_t)bsz);
	voluta_ut_write_read(ut_ctx, ino, buf, bsz, off);
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_falloc_aligned(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_falloc_simple_(ut_ctx, 0, VOLUTA_BK_SIZE);
	ut_file_falloc_simple_(ut_ctx, 0, VOLUTA_MEGA);
	ut_file_falloc_simple_(ut_ctx, VOLUTA_SMEGA, VOLUTA_MEGA);
	ut_file_falloc_simple_(ut_ctx, VOLUTA_SGIGA, VOLUTA_MEGA);
	ut_file_falloc_simple_(ut_ctx, VOLUTA_STERA, VOLUTA_MEGA);
}

static void ut_file_falloc_unaligned(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_falloc_simple_(ut_ctx, 1, 3 * VOLUTA_BK_SIZE);
	ut_file_falloc_simple_(ut_ctx, 3, VOLUTA_MEGA / 3);
	ut_file_falloc_simple_(ut_ctx, 5 * VOLUTA_SMEGA, VOLUTA_MEGA / 5);
	ut_file_falloc_simple_(ut_ctx, 7 * VOLUTA_SGIGA, VOLUTA_MEGA / 7);
	ut_file_falloc_simple_(ut_ctx, 11 * VOLUTA_STERA, VOLUTA_MEGA / 11);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_falloc_unwritten_(struct voluta_ut_ctx *ut_ctx,
				      loff_t off, size_t bsz)
{
	uint8_t b = 1;
	ino_t ino, dino, root_ino = VOLUTA_INO_ROOT;
	const loff_t len = (loff_t)bsz;
	const char *name = UT_NAME;

	voluta_ut_make_dir(ut_ctx, root_ino, name, &dino);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);
	voluta_ut_fallocate_reserve(ut_ctx, ino, off, len);
	voluta_ut_read_zeros(ut_ctx, ino, off, bsz);
	voluta_ut_write_read(ut_ctx, ino, &b, 1, off);
	voluta_ut_write_read(ut_ctx, ino, &b, 1, off + len - 1);
	voluta_ut_read_zeros(ut_ctx, ino, off + 1, bsz - 2);
	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_remove_dir(ut_ctx, root_ino, name);
}

static void ut_file_falloc_unwritten(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_falloc_unwritten_(ut_ctx, 0, VOLUTA_BK_SIZE);
	ut_file_falloc_unwritten_(ut_ctx, VOLUTA_SMEGA, 2 * VOLUTA_BK_SIZE);
	ut_file_falloc_unwritten_(ut_ctx, VOLUTA_SGIGA, 3 * VOLUTA_BK_SIZE);
	ut_file_falloc_unwritten_(ut_ctx, VOLUTA_STERA, 4 * VOLUTA_BK_SIZE);

	ut_file_falloc_unwritten_(ut_ctx, VOLUTA_SMEGA - 111,
				  VOLUTA_MEGA + 1111);
	ut_file_falloc_unwritten_(ut_ctx, VOLUTA_SGIGA - 1111,
				  VOLUTA_MEGA + 111);
	ut_file_falloc_unwritten_(ut_ctx, VOLUTA_STERA - 11111,
				  VOLUTA_MEGA + 11);
	ut_file_falloc_unwritten_(ut_ctx, VOLUTA_FILESIZE_MAX - 111111, 111111);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_falloc_punch_hole_(struct voluta_ut_ctx *ut_ctx,
				       loff_t off1, loff_t off2, size_t bsz)
{
	ino_t ino, root_ino = VOLUTA_INO_ROOT;
	const char *name = UT_NAME;
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
	ut_file_falloc_punch_hole_(ut_ctx, 0, VOLUTA_BK_SIZE, VOLUTA_BK_SIZE);
	ut_file_falloc_punch_hole_(ut_ctx, 0, VOLUTA_SMEGA, VOLUTA_BK_SIZE);
	ut_file_falloc_punch_hole_(ut_ctx, 0, VOLUTA_SGIGA, VOLUTA_MEGA);
	ut_file_falloc_punch_hole_(ut_ctx, 0, VOLUTA_STERA, VOLUTA_MEGA);

	ut_file_falloc_punch_hole_(ut_ctx, VOLUTA_SMEGA, 2 * VOLUTA_SMEGA,
				   VOLUTA_BK_SIZE);
	ut_file_falloc_punch_hole_(ut_ctx, VOLUTA_SMEGA,
				   VOLUTA_SGIGA, VOLUTA_MEGA);
	ut_file_falloc_punch_hole_(ut_ctx, VOLUTA_SMEGA,
				   VOLUTA_STERA, VOLUTA_MEGA);
	ut_file_falloc_punch_hole_(ut_ctx, VOLUTA_SGIGA,
				   VOLUTA_STERA, VOLUTA_MEGA);

	ut_file_falloc_punch_hole_(ut_ctx, 7,
				   7 * VOLUTA_BK_SIZE - 7, VOLUTA_BK_SIZE);
	ut_file_falloc_punch_hole_(ut_ctx, 77,
				   7 * VOLUTA_SMEGA, 7 * VOLUTA_BK_SIZE + 7);
	ut_file_falloc_punch_hole_(ut_ctx, 777,
				   7 * VOLUTA_SGIGA - 7, VOLUTA_MEGA + 77);
	ut_file_falloc_punch_hole_(ut_ctx, 7777,
				   VOLUTA_STERA - 7, VOLUTA_MEGA + 77);

	ut_file_falloc_punch_hole_(ut_ctx, 77 * VOLUTA_SMEGA - 7,
				   7 * VOLUTA_SGIGA - 7, VOLUTA_MEGA + 77);
	ut_file_falloc_punch_hole_(ut_ctx, 777 * VOLUTA_SGIGA + 77,
				   VOLUTA_STERA - 7, VOLUTA_MEGA + 77);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_falloc_punch_hole2_(struct voluta_ut_ctx *ut_ctx,
					loff_t off, size_t bsz)
{
	ino_t ino, dino, root_ino = VOLUTA_INO_ROOT;
	const loff_t off1 = off + (loff_t)bsz;
	const loff_t off2 = off1 + (loff_t)bsz;
	const char *name = UT_NAME;
	uint8_t *buf = ut_randbuf(ut_ctx, bsz);

	voluta_ut_make_dir(ut_ctx, root_ino, name, &dino);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);
	voluta_ut_write_read(ut_ctx, ino, buf, bsz, off);
	voluta_ut_trunacate_file(ut_ctx, ino, off2);
	voluta_ut_fallocate_punch_hole(ut_ctx, ino, off1, off2 - off1);
	voluta_ut_read_zero(ut_ctx, ino, off1);
	voluta_ut_read_zero(ut_ctx, ino, off2 - 1);
	voluta_ut_fallocate_punch_hole(ut_ctx, ino, off1 - 1, off2 - off1 - 1);
	voluta_ut_read_zero(ut_ctx, ino, off1 - 1);
	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_remove_dir(ut_ctx, root_ino, name);
}

static void ut_file_falloc_punch_hole2(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_falloc_punch_hole2_(ut_ctx, 0, VOLUTA_MEGA);
	ut_file_falloc_punch_hole2_(ut_ctx, VOLUTA_SMEGA, VOLUTA_MEGA);
	ut_file_falloc_punch_hole2_(ut_ctx, VOLUTA_SGIGA, VOLUTA_MEGA);
	ut_file_falloc_punch_hole2_(ut_ctx, VOLUTA_STERA, VOLUTA_MEGA);

	ut_file_falloc_punch_hole2_(ut_ctx, VOLUTA_SMEGA - 11,
				    VOLUTA_MEGA + 111);
	ut_file_falloc_punch_hole2_(ut_ctx, VOLUTA_SGIGA - 111,
				    VOLUTA_MEGA + 11);
	ut_file_falloc_punch_hole2_(ut_ctx, VOLUTA_STERA - 1111,
				    VOLUTA_MEGA + 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static loff_t off_to_nbk(loff_t off)
{
	return off / VOLUTA_BK_SIZE;
}

static loff_t off_to_nbk_up(loff_t off)
{
	return off_to_nbk(off + VOLUTA_BK_SIZE - 1);
}

static blkcnt_t blocks_count_of(loff_t off, loff_t len)
{
	loff_t lba_beg, lba_end, length;

	lba_beg = off_to_nbk(off);
	lba_end = off_to_nbk_up(off + len);
	length = (lba_end - lba_beg) * VOLUTA_BK_SIZE;

	return length / 512;
}

static void ut_file_falloc_stat_(struct voluta_ut_ctx *ut_ctx,
				 loff_t base_off, loff_t len, loff_t step_size)
{
	loff_t off;
	blkcnt_t nblk;
	ino_t ino, dino, root_ino = VOLUTA_INO_ROOT;
	struct stat st1, st2;
	const char *name = UT_NAME;
	const size_t cnt = 64;

	ut_assert_eq(base_off % VOLUTA_BK_SIZE, 0);
	ut_assert_eq(step_size % VOLUTA_BK_SIZE, 0);
	ut_assert_le(len, step_size);
	voluta_ut_make_dir(ut_ctx, root_ino, name, &dino);
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
	voluta_ut_remove_dir(ut_ctx, root_ino, name);
}

static void ut_file_falloc_stat(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_falloc_stat_(ut_ctx, 0, VOLUTA_BK_SIZE, VOLUTA_BK_SIZE);
	ut_file_falloc_stat_(ut_ctx, 0, VOLUTA_BK_SIZE - 1, VOLUTA_BK_SIZE);
	ut_file_falloc_stat_(ut_ctx, VOLUTA_BK_SIZE, VOLUTA_BK_SIZE - 3,
			     VOLUTA_BK_SIZE);
	ut_file_falloc_stat_(ut_ctx, 0, VOLUTA_SMEGA, VOLUTA_SMEGA);
	ut_file_falloc_stat_(ut_ctx, VOLUTA_BK_SIZE,
			     VOLUTA_BK_SIZE, VOLUTA_SMEGA);
	ut_file_falloc_stat_(ut_ctx, VOLUTA_SMEGA,
			     VOLUTA_SMEGA - 1, VOLUTA_MEGA);
	ut_file_falloc_stat_(ut_ctx, VOLUTA_SGIGA,
			     VOLUTA_SMEGA - 11, 11 * VOLUTA_MEGA);
	ut_file_falloc_stat_(ut_ctx, VOLUTA_STERA,
			     VOLUTA_SMEGA - 111, 111 * VOLUTA_MEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_falloc_sparse_(struct voluta_ut_ctx *ut_ctx,
				   loff_t base_off, loff_t step_size)
{
	loff_t off, len, zero = 0;
	blkcnt_t blocks;
	ino_t ino, dino, root_ino = VOLUTA_INO_ROOT;
	struct stat st;
	const char *name = UT_NAME;
	const long cnt = 1024;

	voluta_ut_make_dir(ut_ctx, root_ino, name, &dino);
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
	voluta_ut_remove_dir(ut_ctx, root_ino, name);
}

static void ut_file_falloc_sparse(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_falloc_sparse_(ut_ctx, 0, VOLUTA_SMEGA);
	ut_file_falloc_sparse_(ut_ctx, 1, VOLUTA_SMEGA);
	ut_file_falloc_sparse_(ut_ctx, VOLUTA_SMEGA, VOLUTA_SGIGA);
	ut_file_falloc_sparse_(ut_ctx, 11 * VOLUTA_SMEGA - 1, VOLUTA_SGIGA);
	ut_file_falloc_sparse_(ut_ctx, 111 * VOLUTA_STERA - 1, VOLUTA_SGIGA);
	ut_file_falloc_sparse_(ut_ctx, VOLUTA_FILESIZE_MAX / 2, VOLUTA_SGIGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_file_falloc_aligned),
	UT_DEFTEST(ut_file_falloc_unaligned),
	UT_DEFTEST(ut_file_falloc_unwritten),
	UT_DEFTEST(ut_file_falloc_punch_hole),
	UT_DEFTEST(ut_file_falloc_punch_hole2),
	UT_DEFTEST(ut_file_falloc_stat),
	UT_DEFTEST(ut_file_falloc_sparse),
};

const struct voluta_ut_tests voluta_ut_file_fallocate_tests =
	UT_MKTESTS(ut_local_tests);

