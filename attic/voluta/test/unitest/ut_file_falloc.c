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
#include "unitest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_falloc_simple_(struct ut_env *ut_env,
				   loff_t off, size_t bsz)
{
	ino_t ino;
	const char *name = UT_NAME;
	void *buf = ut_randbuf(ut_env, bsz);
	const ino_t root_ino = UT_ROOT_INO;

	ut_create_file(ut_env, root_ino, name, &ino);
	ut_fallocate_reserve(ut_env, ino, off, (loff_t)bsz);
	ut_write_read(ut_env, ino, buf, bsz, off);
	ut_remove_file(ut_env, root_ino, name, ino);
}

static void ut_file_falloc_aligned(struct ut_env *ut_env)
{
	ut_file_falloc_simple_(ut_env, 0, UT_BK_SIZE);
	ut_file_falloc_simple_(ut_env, 0, UT_UMEGA);
	ut_file_falloc_simple_(ut_env, UT_MEGA, UT_UMEGA);
	ut_file_falloc_simple_(ut_env, UT_GIGA, UT_UMEGA);
	ut_file_falloc_simple_(ut_env, UT_TERA, UT_UMEGA);
}

static void ut_file_falloc_unaligned(struct ut_env *ut_env)
{
	ut_file_falloc_simple_(ut_env, 1, 3 * UT_BK_SIZE);
	ut_file_falloc_simple_(ut_env, 3, UT_UMEGA / 3);
	ut_file_falloc_simple_(ut_env, 5 * UT_MEGA, UT_UMEGA / 5);
	ut_file_falloc_simple_(ut_env, 7 * UT_GIGA, UT_UMEGA / 7);
	ut_file_falloc_simple_(ut_env, UT_TERA, UT_UMEGA / 11);
	ut_file_falloc_simple_(ut_env, UT_FILESIZE_MAX / 2, UT_UMEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_falloc_read_(struct ut_env *ut_env,
				 loff_t off, size_t cnt)
{
	uint8_t zero = 0;
	ino_t ino;
	const char *name = UT_NAME;
	const ino_t root_ino = UT_ROOT_INO;

	ut_create_file(ut_env, root_ino, name, &ino);
	ut_fallocate_reserve(ut_env, ino, off, (loff_t)cnt);
	ut_read_verify(ut_env, ino, &zero, 1, off);
	ut_read_verify(ut_env, ino, &zero, 1, off + (loff_t)cnt - 1);
	ut_remove_file(ut_env, root_ino, name, ino);
}

static void ut_file_falloc_read(struct ut_env *ut_env)
{
	ut_file_falloc_read_(ut_env, 0, UT_BK_SIZE);
	ut_file_falloc_read_(ut_env, 1, UT_UMEGA);
	ut_file_falloc_read_(ut_env, 0, VOLUTA_AG_SIZE);
	ut_file_falloc_read_(ut_env, UT_MEGA - 1, VOLUTA_AG_SIZE + 2);
	ut_file_falloc_read_(ut_env, UT_GIGA, UT_UMEGA);
	ut_file_falloc_read_(ut_env, UT_TERA - 2, VOLUTA_AG_SIZE + 3);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_falloc_unwritten_(struct ut_env *ut_env,
				      loff_t off, size_t bsz)
{
	ino_t ino;
	ino_t dino;
	const loff_t len = (loff_t)bsz;
	const char *name = UT_NAME;
	const uint8_t b = 1;

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_create_file(ut_env, dino, name, &ino);
	ut_fallocate_reserve(ut_env, ino, off, len);
	ut_read_zeros(ut_env, ino, off, bsz);
	ut_write_read(ut_env, ino, &b, 1, off);
	ut_write_read(ut_env, ino, &b, 1, off + len - 1);
	ut_read_zeros(ut_env, ino, off + 1, bsz - 2);
	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_at_root(ut_env, name);
}

static void ut_file_falloc_unwritten(struct ut_env *ut_env)
{
	ut_file_falloc_unwritten_(ut_env, 0, UT_BK_SIZE);
	ut_file_falloc_unwritten_(ut_env, UT_MEGA, 2 * UT_BK_SIZE);
	ut_file_falloc_unwritten_(ut_env, UT_GIGA, 3 * UT_BK_SIZE);
	ut_file_falloc_unwritten_(ut_env, UT_TERA, 4 * UT_BK_SIZE);
	ut_file_falloc_unwritten_(ut_env, UT_MEGA - 111, UT_UMEGA + 1111);
	ut_file_falloc_unwritten_(ut_env, UT_GIGA - 1111, UT_UMEGA + 111);
	ut_file_falloc_unwritten_(ut_env, UT_TERA - 11111, UT_UMEGA + 11);
	ut_file_falloc_unwritten_(ut_env, UT_FILESIZE_MAX - 111111, 111111);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_falloc_drop_caches_(struct ut_env *ut_env,
					loff_t off, size_t bsz)
{
	ino_t ino, root_ino = UT_ROOT_INO;
	const char *name = UT_NAME;
	void *buf = ut_randbuf(ut_env, bsz);

	ut_create_file(ut_env, root_ino, name, &ino);
	ut_fallocate_reserve(ut_env, ino, off, (loff_t)bsz);
	ut_release_file(ut_env, ino);
	ut_sync_drop(ut_env);
	ut_open_rdwr(ut_env, ino);
	ut_read_zero(ut_env, ino, off);
	ut_read_zero(ut_env, ino, off + (loff_t)(bsz - 1));
	ut_write_read(ut_env, ino, buf, bsz, off);
	ut_remove_file(ut_env, root_ino, name, ino);
}

static void ut_file_falloc_drop_caches(struct ut_env *ut_env)
{
	ut_file_falloc_drop_caches_(ut_env, 0, UT_UMEGA);
	ut_file_falloc_drop_caches_(ut_env, 3, UT_UMEGA / 3);
	ut_file_falloc_drop_caches_(ut_env, 5 * UT_MEGA + 5, UT_UMEGA / 5);
	ut_file_falloc_drop_caches_(ut_env, UT_TERA / 11, UT_UMEGA / 11);
	ut_file_falloc_drop_caches_(ut_env, UT_FILESIZE_MAX / 2, UT_UMEGA);
	ut_file_falloc_drop_caches_(ut_env, UT_FILESIZE_MAX - UT_UMEGA - 11,
				    UT_UMEGA + 11);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_falloc_punch_hole_(struct ut_env *ut_env,
				       loff_t off1, loff_t off2, size_t bsz)
{
	ino_t ino, root_ino = UT_ROOT_INO;
	const char *name = UT_NAME;
	uint8_t zero[1] = { 0 };
	uint8_t *buf = ut_randbuf(ut_env, bsz);

	ut_create_file(ut_env, root_ino, name, &ino);
	ut_write_read(ut_env, ino, buf, bsz, off1);
	ut_write_read(ut_env, ino, buf, bsz, off2);
	ut_fallocate_punch_hole(ut_env, ino, off1, off2 - off1);
	ut_read_verify(ut_env, ino, zero, 1, off1);
	ut_read_verify(ut_env, ino, zero, 1, off2 - 1);

	ut_fallocate_punch_hole(ut_env, ino, off1, off2 - off1 + 1);
	ut_read_verify(ut_env, ino, zero, 1, off2);
	ut_read_verify(ut_env, ino, buf + 1, 1 /* bsz - 1 */, off2 + 1);
	ut_remove_file(ut_env, root_ino, name, ino);
}

static void ut_file_falloc_punch_hole(struct ut_env *ut_env)
{
	ut_file_falloc_punch_hole_(ut_env, 0, UT_BK_SIZE, UT_BK_SIZE);
	ut_file_falloc_punch_hole_(ut_env, 0, UT_MEGA, UT_BK_SIZE);
	ut_file_falloc_punch_hole_(ut_env, 0, UT_GIGA, UT_UMEGA);
	ut_file_falloc_punch_hole_(ut_env, 0, UT_TERA, UT_UMEGA);

	ut_file_falloc_punch_hole_(ut_env, UT_MEGA, 2 * UT_MEGA, UT_BK_SIZE);
	ut_file_falloc_punch_hole_(ut_env, UT_MEGA, UT_GIGA, UT_UMEGA);
	ut_file_falloc_punch_hole_(ut_env, UT_MEGA, UT_TERA, UT_UMEGA);
	ut_file_falloc_punch_hole_(ut_env, UT_GIGA, UT_TERA, UT_UMEGA);

	ut_file_falloc_punch_hole_(ut_env, 7, 7 * UT_BK_SIZE - 7, UT_BK_SIZE);
	ut_file_falloc_punch_hole_(ut_env, 77, 7 * UT_MEGA,
				   7 * UT_BK_SIZE + 7);
	ut_file_falloc_punch_hole_(ut_env, 777, 7 * UT_GIGA - 7,
				   UT_UMEGA + 77);
	ut_file_falloc_punch_hole_(ut_env, 7777, UT_TERA - 7, UT_UMEGA + 77);

	ut_file_falloc_punch_hole_(ut_env, 77 * UT_MEGA - 7,
				   7 * UT_GIGA - 7, UT_UMEGA + 77);
	ut_file_falloc_punch_hole_(ut_env, 777 * UT_GIGA + 77,
				   UT_TERA - 7, UT_UMEGA + 77);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_falloc_punch_hole2_(struct ut_env *ut_env,
					loff_t off, size_t bsz)
{
	ino_t ino, dino, root_ino = UT_ROOT_INO;
	const loff_t off1 = off + (loff_t)bsz;
	const loff_t off2 = off1 + (loff_t)bsz;
	const char *name = UT_NAME;
	uint8_t *buf = ut_randbuf(ut_env, bsz);

	ut_mkdir_oki(ut_env, root_ino, name, &dino);
	ut_create_file(ut_env, dino, name, &ino);
	ut_write_read(ut_env, ino, buf, bsz, off);
	ut_trunacate_file(ut_env, ino, off2);
	ut_fallocate_punch_hole(ut_env, ino, off1, off2 - off1);
	ut_read_zero(ut_env, ino, off1);
	ut_read_zero(ut_env, ino, off2 - 1);
	ut_fallocate_punch_hole(ut_env, ino, off1 - 1, off2 - off1 - 1);
	ut_read_zero(ut_env, ino, off1 - 1);
	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_ok(ut_env, root_ino, name);
}

static void ut_file_falloc_punch_hole2(struct ut_env *ut_env)
{
	ut_file_falloc_punch_hole2_(ut_env, 0, UT_UMEGA);
	ut_file_falloc_punch_hole2_(ut_env, UT_MEGA, UT_UMEGA);
	ut_file_falloc_punch_hole2_(ut_env, UT_GIGA, UT_UMEGA);
	ut_file_falloc_punch_hole2_(ut_env, UT_TERA, UT_UMEGA);
	ut_file_falloc_punch_hole2_(ut_env, UT_MEGA - 11, UT_UMEGA + 111);
	ut_file_falloc_punch_hole2_(ut_env, UT_GIGA - 111, UT_UMEGA + 11);
	ut_file_falloc_punch_hole2_(ut_env, UT_TERA - 1111, UT_UMEGA + 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static loff_t off_to_nbk(loff_t off)
{
	return off / UT_BK_SIZE;
}

static loff_t off_to_nbk_up(loff_t off)
{
	return off_to_nbk(off + UT_BK_SIZE - 1);
}

static blkcnt_t blocks_count_of(loff_t off, loff_t len)
{
	loff_t lba_beg, lba_end, length;

	lba_beg = off_to_nbk(off);
	lba_end = off_to_nbk_up(off + len);
	length = (lba_end - lba_beg) * UT_BK_SIZE;

	return length / 512;
}

static void ut_file_falloc_stat_(struct ut_env *ut_env,
				 loff_t base_off, loff_t len, loff_t step_size)
{
	loff_t off;
	blkcnt_t nblk;
	ino_t ino, dino, root_ino = UT_ROOT_INO;
	struct stat st1, st2;
	const char *name = UT_NAME;
	const size_t cnt = 64;

	ut_assert_eq(base_off % UT_BK_SIZE, 0);
	ut_assert_eq(step_size % UT_BK_SIZE, 0);
	ut_assert_le(len, step_size);
	ut_mkdir_oki(ut_env, root_ino, name, &dino);
	ut_create_file(ut_env, dino, name, &ino);

	ut_getattr_ok(ut_env, ino, &st1);
	ut_assert_eq(st1.st_size, 0);
	ut_assert_eq(st1.st_blocks, 0);

	off = base_off;
	for (size_t i = 0; i < cnt; ++i) {
		nblk = blocks_count_of(off, len);

		ut_getattr_ok(ut_env, ino, &st1);
		ut_fallocate_reserve(ut_env, ino, off, len);
		ut_getattr_ok(ut_env, ino, &st2);

		ut_assert_eq(off + len, st2.st_size);
		ut_assert_eq(st1.st_blocks + nblk, st2.st_blocks);
		off += step_size;
	}
	off = base_off;
	for (size_t j = 0; j < cnt; ++j) {
		nblk = blocks_count_of(off, len);

		ut_getattr_ok(ut_env, ino, &st1);
		ut_fallocate_punch_hole(ut_env, ino, off, nblk * 512);
		ut_getattr_ok(ut_env, ino, &st2);
		ut_assert_eq(st1.st_blocks - nblk, st2.st_blocks);
		off += step_size;
	}
	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_ok(ut_env, root_ino, name);
}

static void ut_file_falloc_stat(struct ut_env *ut_env)
{
	ut_file_falloc_stat_(ut_env, 0, UT_BK_SIZE, UT_BK_SIZE);
	ut_file_falloc_stat_(ut_env, 0, UT_BK_SIZE - 1, UT_BK_SIZE);
	ut_file_falloc_stat_(ut_env, UT_BK_SIZE, UT_BK_SIZE - 3, UT_BK_SIZE);
	ut_file_falloc_stat_(ut_env, 0, UT_MEGA, UT_MEGA);
	ut_file_falloc_stat_(ut_env, UT_BK_SIZE, UT_BK_SIZE, UT_MEGA);
	ut_file_falloc_stat_(ut_env, UT_MEGA, UT_MEGA - 1, UT_UMEGA);
	ut_file_falloc_stat_(ut_env, UT_GIGA, UT_MEGA - 11, 11 * UT_UMEGA);
	ut_file_falloc_stat_(ut_env, UT_TERA, UT_MEGA - 111, 111 * UT_UMEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_falloc_sparse_(struct ut_env *ut_env,
				   loff_t base_off, loff_t step_size)
{
	loff_t off, len, zero = 0;
	blkcnt_t blocks;
	ino_t ino, dino, root_ino = UT_ROOT_INO;
	struct stat st;
	const char *name = UT_NAME;
	const long cnt = 1024;

	ut_mkdir_oki(ut_env, root_ino, name, &dino);
	ut_create_file(ut_env, dino, name, &ino);

	ut_getattr_ok(ut_env, ino, &st);
	ut_assert_eq(st.st_size, 0);
	ut_assert_eq(st.st_blocks, 0);

	blocks = 0;
	off = base_off;
	for (long i = 0; i < cnt; ++i) {
		off = base_off + (i * step_size);
		len = (int)sizeof(off);
		ut_fallocate_reserve(ut_env, ino, off, len);
		ut_getattr_file(ut_env, ino, &st);
		ut_assert_eq(st.st_size, off + len);
		ut_assert_gt(st.st_blocks, blocks);
		ut_read_verify(ut_env, ino, &zero, (size_t)len, off);

		blocks = st.st_blocks;
		ut_write_read(ut_env, ino, &off, (size_t)len, off);
		ut_getattr_file(ut_env, ino, &st);
		ut_assert_eq(st.st_size, off + len);
		ut_assert_eq(st.st_blocks, blocks);
	}

	ut_trunacate_file(ut_env, ino, 0);
	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_ok(ut_env, root_ino, name);
}

static void ut_file_falloc_sparse(struct ut_env *ut_env)
{
	ut_file_falloc_sparse_(ut_env, 0, UT_MEGA);
	ut_file_falloc_sparse_(ut_env, 1, UT_MEGA);
	ut_file_falloc_sparse_(ut_env, UT_MEGA, UT_GIGA);
	ut_file_falloc_sparse_(ut_env, 11 * UT_MEGA - 1, UT_GIGA);
	ut_file_falloc_sparse_(ut_env, UT_TERA - 11, UT_GIGA);
	ut_file_falloc_sparse_(ut_env, UT_FILESIZE_MAX / 2, UT_GIGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct ut_testdef ut_local_tests[] = {
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

const struct ut_tests ut_test_file_fallocate = UT_MKTESTS(ut_local_tests);

