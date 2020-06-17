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

static void ut_file_trunc_data_(struct voluta_ut_ctx *ut_ctx,
				loff_t off, size_t bsz)
{
	struct stat st;
	ino_t ino;
	const ino_t root_ino = ROOT_INO;
	const char *name = T_NAME;
	const loff_t bk_size = (loff_t)BK_SIZE;
	const loff_t off_bk_start = (off / bk_size) * bk_size;
	char *buf = ut_randbuf(ut_ctx, bsz);

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	voluta_ut_trunacate_file(ut_ctx, ino, off);
	voluta_ut_trunacate_file(ut_ctx, ino, 0);
	voluta_ut_getattr_file(ut_ctx, ino, &st);
	ut_assert_eq(st.st_blocks, 0);
	voluta_ut_write_read(ut_ctx, ino, buf, bsz, off);
	voluta_ut_getattr_file(ut_ctx, ino, &st);
	ut_assert_gt(st.st_blocks, 0);
	voluta_ut_trunacate_file(ut_ctx, ino, off + 1);
	voluta_ut_getattr_file(ut_ctx, ino, &st);
	ut_assert_gt(st.st_blocks, 0);
	voluta_ut_trunacate_file(ut_ctx, ino, off_bk_start);
	voluta_ut_getattr_file(ut_ctx, ino, &st);
	ut_assert_eq(st.st_blocks, 0);
	voluta_ut_trunacate_file(ut_ctx, ino, off + 1);
	voluta_ut_getattr_file(ut_ctx, ino, &st);
	ut_assert_eq(st.st_blocks, 0);
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_trunc_simple(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_trunc_data_(ut_ctx, 0, BK_SIZE);
	ut_file_trunc_data_(ut_ctx, BK_SIZE, BK_SIZE);
	ut_file_trunc_data_(ut_ctx, MEGA, BK_SIZE);
	ut_file_trunc_data_(ut_ctx, GIGA, BK_SIZE);
	ut_file_trunc_data_(ut_ctx, TERA, BK_SIZE);
}

static void ut_file_trunc_aligned(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_trunc_data_(ut_ctx, 0, UMEGA);
	ut_file_trunc_data_(ut_ctx, BK_SIZE, UMEGA);
	ut_file_trunc_data_(ut_ctx, MEGA, UMEGA);
	ut_file_trunc_data_(ut_ctx, GIGA, UMEGA);
	ut_file_trunc_data_(ut_ctx, TERA, UMEGA);
}

static void ut_file_trunc_unaligned(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_trunc_data_(ut_ctx, 1, BK_SIZE + 2);
	ut_file_trunc_data_(ut_ctx, BK_SIZE - 1, 2 * BK_SIZE + 3);
	ut_file_trunc_data_(ut_ctx, 7 * BK_SIZE - 7, 7 * BK_SIZE + 7);
	ut_file_trunc_data_(ut_ctx, 11 * MEGA - 11, 11 * BK_SIZE + 11);
	ut_file_trunc_data_(ut_ctx, 13 * GIGA - 13, 13 * BK_SIZE + 13);
	ut_file_trunc_data_(ut_ctx, TERA - 11111, BK_SIZE + 111111);
	ut_file_trunc_data_(ut_ctx, TERA - 1111111, BK_SIZE + 1111111);
	ut_file_trunc_data_(ut_ctx, FILESIZE_MAX - MEGA - 1, UMEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_trunc_mixed_(struct voluta_ut_ctx *ut_ctx,
				 loff_t off, size_t len)
{
	ino_t ino;
	const ino_t root_ino = ROOT_INO;
	const loff_t eoff = off + (loff_t)len;
	const loff_t zoff = off - (loff_t)len;
	const size_t bsz = 2 * len;
	const char *name = T_NAME;
	uint8_t *buf;

	ut_assert(len >= BK_SIZE);
	ut_assert(zoff >= 0);

	buf = ut_randbuf(ut_ctx, bsz);
	memset(buf, 0, bsz / 2);

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	voluta_ut_write_read(ut_ctx, ino, buf + len, len, off);
	voluta_ut_trunacate_file(ut_ctx, ino, eoff - 1);
	voluta_ut_read_verify(ut_ctx, ino, buf, bsz - 1, zoff);
	voluta_ut_trunacate_file(ut_ctx, ino, eoff - BK_SIZE + 1);
	voluta_ut_read_verify(ut_ctx, ino, buf, bsz - BK_SIZE + 1, zoff);
	voluta_ut_trunacate_file(ut_ctx, ino, off);
	voluta_ut_read_verify(ut_ctx, ino, buf, len, zoff);
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_trunc_mixed(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_trunc_mixed_(ut_ctx, BK_SIZE, BK_SIZE);
	ut_file_trunc_mixed_(ut_ctx, MEGA, 8 * BK_SIZE);
	ut_file_trunc_mixed_(ut_ctx, GIGA, 16 * BK_SIZE);
	ut_file_trunc_mixed_(ut_ctx, TERA, 32 * BK_SIZE);

	ut_file_trunc_mixed_(ut_ctx, MEGA - 11111, 11 * BK_SIZE);
	ut_file_trunc_mixed_(ut_ctx, MEGA + 11111, 11 * BK_SIZE);
	ut_file_trunc_mixed_(ut_ctx, GIGA - 11111, 11 * BK_SIZE);
	ut_file_trunc_mixed_(ut_ctx, GIGA + 11111, 11 * BK_SIZE);
	ut_file_trunc_mixed_(ut_ctx, TERA - 11111, 11 * BK_SIZE);
	ut_file_trunc_mixed_(ut_ctx, TERA + 11111, 11 * BK_SIZE);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_trunc_hole_(struct voluta_ut_ctx *ut_ctx,
				loff_t off1, loff_t off2, size_t len)
{
	ino_t ino;
	ino_t dino;
	loff_t hole_off2;
	size_t hole_len, nzeros;
	const char *name = T_NAME;
	const char *dname = T_NAME;
	void *buf1;
	void *buf2;
	void *zeros;

	const loff_t hole_off1 = off1 + (loff_t)len;
	hole_len = (size_t)(off2 - hole_off1);
	nzeros = (hole_len < UMEGA) ? hole_len : UMEGA;
	hole_off2 = off2 - (loff_t)nzeros;

	buf1 = ut_randbuf(ut_ctx, len);
	buf2 = ut_randbuf(ut_ctx, len);
	zeros = ut_zerobuf(ut_ctx, nzeros);

	voluta_ut_mkdir_at_root(ut_ctx, dname, &dino);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);
	voluta_ut_write_read(ut_ctx, ino, buf1, len, off1);
	voluta_ut_write_read(ut_ctx, ino, buf2, len, off2);
	voluta_ut_trunacate_file(ut_ctx, ino, off2);
	voluta_ut_read_verify(ut_ctx, ino, zeros, nzeros, hole_off1);
	voluta_ut_read_verify(ut_ctx, ino, zeros, nzeros, hole_off2);
	voluta_ut_trunacate_file(ut_ctx, ino, off1 + 1);
	voluta_ut_write_read(ut_ctx, ino, buf1, 1, off1);
	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_rmdir_at_root(ut_ctx, dname);
}

static void ut_file_trunc_hole(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_trunc_hole_(ut_ctx, 0, MEGA, BK_SIZE);
	ut_file_trunc_hole_(ut_ctx, 1, MEGA - 1, BK_SIZE);
	ut_file_trunc_hole_(ut_ctx, 2, 2 * MEGA - 2, UMEGA);
	ut_file_trunc_hole_(ut_ctx, 3, 3 * MEGA + 3, UMEGA);
	ut_file_trunc_hole_(ut_ctx, MEGA + 1, MEGA + BK_SIZE + 2, BK_SIZE);
	ut_file_trunc_hole_(ut_ctx, 0, GIGA, UMEGA);
	ut_file_trunc_hole_(ut_ctx, 1, GIGA - 1, UMEGA);
	ut_file_trunc_hole_(ut_ctx, 2, 2 * GIGA - 2, 2 * UMEGA);
	ut_file_trunc_hole_(ut_ctx, GIGA + 1, GIGA + MEGA + 2, UMEGA);
	ut_file_trunc_hole_(ut_ctx, 0, TERA, UMEGA);
	ut_file_trunc_hole_(ut_ctx, 1, TERA - 1, UMEGA);
	ut_file_trunc_hole_(ut_ctx, 2, 2 * TERA - 2, UMEGA);
	ut_file_trunc_hole_(ut_ctx, TERA + 1, TERA + MEGA + 2, UMEGA);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
ut_read_zero_byte(struct voluta_ut_ctx *ut_ctx, ino_t ino, loff_t off)
{
	const uint8_t zero[1] = { 0 };

	if (off >= 0) {
		voluta_ut_read_verify(ut_ctx, ino, zero, 1, off);
	}
}

static void ut_file_trunc_single_byte_(struct voluta_ut_ctx *ut_ctx,
				       const loff_t *off_arr, size_t cnt)
{
	ino_t ino;
	ino_t dino;
	loff_t off;
	const char *name = T_NAME;
	const char *dname = T_NAME;
	const uint8_t one[1] = { 1 };

	voluta_ut_mkdir_at_root(ut_ctx, dname, &dino);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);
	for (size_t i = 0; i < cnt; ++i) {
		off = off_arr[i];
		voluta_ut_write_read(ut_ctx, ino, one, 1, off);
	}
	for (size_t i = 0; i < cnt; ++i) {
		off = off_arr[i];
		voluta_ut_read_verify(ut_ctx, ino, one, 1, off);
		ut_read_zero_byte(ut_ctx, ino, off - 1);
	}
	for (size_t i = 0; i < cnt; ++i) {
		off = off_arr[i];
		voluta_ut_trunacate_file(ut_ctx, ino, off);
		ut_read_zero_byte(ut_ctx, ino, off - 1);
	}
	voluta_ut_trunacate_file(ut_ctx, ino, 0);
	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_rmdir_at_root(ut_ctx, dname);
}

static void ut_file_trunc_single_byte(struct voluta_ut_ctx *ut_ctx)
{
	const loff_t off1[] = {
		0, BK_SIZE, MEGA, GIGA, TERA
	};
	const loff_t off2[] = {
		1, BK_SIZE + 1, MEGA + 1, GIGA + 1, TERA + 1
	};
	const loff_t off3[] = {
		77, 777, 7777, 77777, 777777, 7777777
	};

	ut_file_trunc_single_byte_(ut_ctx, off1, ARRAY_SIZE(off1));
	ut_file_trunc_single_byte_(ut_ctx, off2, ARRAY_SIZE(off2));
	ut_file_trunc_single_byte_(ut_ctx, off3, ARRAY_SIZE(off3));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_trunc_tail_(struct voluta_ut_ctx *ut_ctx,
				loff_t off, size_t bsz)
{
	ino_t ino;
	ino_t dino;
	const ssize_t ssz = (loff_t)bsz;
	const char *dname = T_NAME;
	const char *fname = T_NAME;
	void *buf = ut_randbuf(ut_ctx, bsz);

	voluta_ut_mkdir_at_root(ut_ctx, dname, &dino);
	voluta_ut_create_file(ut_ctx, dino, fname, &ino);
	voluta_ut_write_read(ut_ctx, ino, buf, bsz, off);
	ut_read_zero_byte(ut_ctx, ino, off - 1);
	voluta_ut_trunacate_file(ut_ctx, ino, off + 1);
	voluta_ut_read_only(ut_ctx, ino, buf, 1, off);
	ut_read_zero_byte(ut_ctx, ino, off - 1);
	voluta_ut_trunacate_file(ut_ctx, ino, off + ssz);
	ut_read_zero_byte(ut_ctx, ino, off + ssz - 1);
	voluta_ut_trunacate_file(ut_ctx, ino, off + ssz + 1);
	ut_read_zero_byte(ut_ctx, ino, off + ssz);
	voluta_ut_remove_file(ut_ctx, dino, fname, ino);
	voluta_ut_rmdir_at_root(ut_ctx, dname);
}

static void ut_file_trunc_tail(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_trunc_tail_(ut_ctx, 0, UMEGA);
	ut_file_trunc_tail_(ut_ctx, 1, UMEGA + 4);
	ut_file_trunc_tail_(ut_ctx, MEGA, UMEGA);
	ut_file_trunc_tail_(ut_ctx, MEGA - 1, UMEGA + 8);
	ut_file_trunc_tail_(ut_ctx, GIGA - 1, UMEGA + 16);
	ut_file_trunc_tail_(ut_ctx, FILESIZE_MAX - MEGA - 1, UMEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_trunc_void_(struct voluta_ut_ctx *ut_ctx,
				loff_t off, size_t bsz)
{
	ino_t ino;
	ino_t dino;
	const ino_t root_ino = ROOT_INO;
	const ssize_t ssz = (loff_t)bsz;
	const loff_t end = off + ssz;
	const char *dname = T_NAME;
	const char *fname = T_NAME;
	struct stat st;
	char x = 'x';

	voluta_ut_mkdir_ok(ut_ctx, root_ino, dname, &dino);
	voluta_ut_create_file(ut_ctx, dino, fname, &ino);
	voluta_ut_trunacate_file(ut_ctx, ino, end);
	voluta_ut_read_zeros(ut_ctx, ino, off, bsz);
	voluta_ut_getattr_exists(ut_ctx, ino, &st);
	ut_assert_eq(st.st_blocks, 0);
	voluta_ut_trunacate_file(ut_ctx, ino, 0);
	voluta_ut_trunacate_file(ut_ctx, ino, end);
	voluta_ut_write_read(ut_ctx, ino, &x, 1, end);
	voluta_ut_read_zeros(ut_ctx, ino, off, bsz);
	voluta_ut_remove_file(ut_ctx, dino, fname, ino);
	voluta_ut_rmdir_ok(ut_ctx, root_ino, dname);
}

static void ut_file_trunc_void(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_trunc_void_(ut_ctx, 0, UKILO);
	ut_file_trunc_void_(ut_ctx, 1, UKILO);
	ut_file_trunc_void_(ut_ctx, 0, UMEGA);
	ut_file_trunc_void_(ut_ctx, 1, UMEGA - 11);
	ut_file_trunc_void_(ut_ctx, MEGA, UKILO);
	ut_file_trunc_void_(ut_ctx, MEGA - 11, UKILO + 1);
	ut_file_trunc_void_(ut_ctx, GIGA - 11, UMEGA + 111);
	ut_file_trunc_void_(ut_ctx, FILESIZE_MAX - MEGA - 1, MEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_trunc_zero_size_(struct voluta_ut_ctx *ut_ctx,
				     loff_t off, size_t bsz)
{
	ino_t ino;
	ino_t dino;
	const char *dname = T_NAME;
	const char *fname = T_NAME;
	struct stat st1, st2, st3;
	struct statvfs stv1, stv2, stv3;
	void *buf = ut_randbuf(ut_ctx, bsz);

	voluta_ut_mkdir_at_root(ut_ctx, dname, &dino);
	voluta_ut_create_file(ut_ctx, dino, fname, &ino);
	voluta_ut_getattr_exists(ut_ctx, ino, &st1);
	voluta_ut_statfs_ok(ut_ctx, ino, &stv1);
	voluta_ut_write_read(ut_ctx, ino, buf, bsz, off);
	voluta_ut_getattr_exists(ut_ctx, ino, &st2);
	voluta_ut_statfs_ok(ut_ctx, ino, &stv2);
	ut_assert_eq(st2.st_size, off + (loff_t)bsz);
	ut_assert_gt(st2.st_blocks, st1.st_blocks);
	ut_assert_lt(stv2.f_bfree, stv1.f_bfree);
	voluta_ut_trunacate_file(ut_ctx, ino, 0);
	voluta_ut_getattr_exists(ut_ctx, ino, &st3);
	voluta_ut_statfs_ok(ut_ctx, ino, &stv3);
	ut_assert_eq(st3.st_size, 0);
	ut_assert_eq(st3.st_blocks, 0);
	ut_assert_eq(stv3.f_bfree, stv1.f_bfree);
	voluta_ut_remove_file(ut_ctx, dino, fname, ino);
	voluta_ut_rmdir_at_root(ut_ctx, dname);
}

static void ut_file_trunc_zero_size(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_trunc_zero_size_(ut_ctx, 1, BK_SIZE);
	ut_file_trunc_zero_size_(ut_ctx, KILO, BK_SIZE);
	ut_file_trunc_zero_size_(ut_ctx, MEGA, BK_SIZE);
	ut_file_trunc_zero_size_(ut_ctx, MEGA - 1, BK_SIZE);
	ut_file_trunc_zero_size_(ut_ctx, 11 * MEGA + 11, BK_SIZE);
	ut_file_trunc_zero_size_(ut_ctx, 111 * GIGA - 111, BK_SIZE);
	ut_file_trunc_zero_size_(ut_ctx, TERA, UMEGA);
	ut_file_trunc_zero_size_(ut_ctx, TERA + 1111111, UMEGA - 1);
	ut_file_trunc_zero_size_(ut_ctx, FILESIZE_MAX - MEGA, UMEGA);
	ut_file_trunc_zero_size_(ut_ctx, FILESIZE_MAX - MEGA - 1, UMEGA + 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_file_trunc_simple),
	UT_DEFTEST(ut_file_trunc_aligned),
	UT_DEFTEST(ut_file_trunc_unaligned),
	UT_DEFTEST(ut_file_trunc_mixed),
	UT_DEFTEST(ut_file_trunc_hole),
	UT_DEFTEST(ut_file_trunc_single_byte),
	UT_DEFTEST(ut_file_trunc_tail),
	UT_DEFTEST(ut_file_trunc_void),
	UT_DEFTEST(ut_file_trunc_zero_size),
};

const struct voluta_ut_tests voluta_ut_test_file_truncate =
	UT_MKTESTS(ut_local_tests);


