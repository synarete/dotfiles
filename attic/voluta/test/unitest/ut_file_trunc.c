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


static void ut_file_trunc_data_(struct ut_env *ut_env,
				loff_t off, size_t bsz)
{
	struct stat st;
	ino_t ino;
	ino_t dino;
	const char *name = UT_NAME;
	const loff_t bk_size = (loff_t)UT_BK_SIZE;
	const loff_t off_bk_start = (off / bk_size) * bk_size;
	char *buf = ut_randbuf(ut_env, bsz);

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_create_file(ut_env, dino, name, &ino);
	ut_trunacate_file(ut_env, ino, off);
	ut_trunacate_file(ut_env, ino, 0);
	ut_getattr_file(ut_env, ino, &st);
	ut_assert_eq(st.st_blocks, 0);
	ut_write_read(ut_env, ino, buf, bsz, off);
	ut_getattr_file(ut_env, ino, &st);
	ut_assert_gt(st.st_blocks, 0);
	ut_trunacate_file(ut_env, ino, off + 1);
	ut_getattr_file(ut_env, ino, &st);
	ut_assert_gt(st.st_blocks, 0);
	ut_trunacate_file(ut_env, ino, off_bk_start);
	ut_getattr_file(ut_env, ino, &st);
	ut_assert_eq(st.st_blocks, 0);
	ut_trunacate_file(ut_env, ino, off + 1);
	ut_getattr_file(ut_env, ino, &st);
	ut_assert_eq(st.st_blocks, 0);
	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_at_root(ut_env, name);
}

static void ut_file_trunc_simple(struct ut_env *ut_env)
{
	ut_file_trunc_data_(ut_env, 0, UT_BK_SIZE);
	ut_file_trunc_data_(ut_env, UT_BK_SIZE, UT_BK_SIZE);
	ut_file_trunc_data_(ut_env, UT_MEGA, UT_BK_SIZE);
	ut_file_trunc_data_(ut_env, UT_GIGA, UT_BK_SIZE);
	ut_file_trunc_data_(ut_env, UT_TERA, UT_BK_SIZE);
}

static void ut_file_trunc_aligned(struct ut_env *ut_env)
{
	ut_file_trunc_data_(ut_env, 0, UT_UMEGA);
	ut_file_trunc_data_(ut_env, UT_BK_SIZE, UT_UMEGA);
	ut_file_trunc_data_(ut_env, UT_MEGA, UT_UMEGA);
	ut_file_trunc_data_(ut_env, UT_GIGA, UT_UMEGA);
	ut_file_trunc_data_(ut_env, UT_TERA, UT_UMEGA);
}

static void ut_file_trunc_unaligned(struct ut_env *ut_env)
{
	ut_file_trunc_data_(ut_env, 1, UT_BK_SIZE + 2);
	ut_file_trunc_data_(ut_env, UT_BK_SIZE - 1, 2 * UT_BK_SIZE + 3);
	ut_file_trunc_data_(ut_env, 7 * UT_BK_SIZE - 7, 7 * UT_BK_SIZE + 7);
	ut_file_trunc_data_(ut_env, 11 * UT_MEGA - 11, 11 * UT_BK_SIZE + 11);
	ut_file_trunc_data_(ut_env, 13 * UT_GIGA - 13, 13 * UT_BK_SIZE + 13);
	ut_file_trunc_data_(ut_env, UT_TERA - 11111, UT_BK_SIZE + 111111);
	ut_file_trunc_data_(ut_env, UT_TERA - 1111111, UT_BK_SIZE + 1111111);
	ut_file_trunc_data_(ut_env, UT_FILESIZE_MAX - UT_MEGA - 1, UT_UMEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_trunc_mixed_(struct ut_env *ut_env,
				 loff_t off, size_t len)
{
	ino_t ino;
	const ino_t root_ino = UT_ROOT_INO;
	const loff_t eoff = off + (loff_t)len;
	const loff_t zoff = off - (loff_t)len;
	const size_t bsz = 2 * len;
	const char *name = UT_NAME;
	uint8_t *buf;

	ut_assert(len >= UT_BK_SIZE);
	ut_assert(zoff >= 0);

	buf = ut_randbuf(ut_env, bsz);
	memset(buf, 0, bsz / 2);

	ut_create_file(ut_env, root_ino, name, &ino);
	ut_write_read(ut_env, ino, buf + len, len, off);
	ut_trunacate_file(ut_env, ino, eoff - 1);
	ut_read_verify(ut_env, ino, buf, bsz - 1, zoff);
	ut_trunacate_file(ut_env, ino, eoff - UT_BK_SIZE + 1);
	ut_read_verify(ut_env, ino, buf, bsz - UT_BK_SIZE + 1, zoff);
	ut_trunacate_file(ut_env, ino, off);
	ut_read_verify(ut_env, ino, buf, len, zoff);
	ut_remove_file(ut_env, root_ino, name, ino);
}

static void ut_file_trunc_mixed(struct ut_env *ut_env)
{
	ut_file_trunc_mixed_(ut_env, UT_BK_SIZE, UT_BK_SIZE);
	ut_file_trunc_mixed_(ut_env, UT_MEGA, 8 * UT_BK_SIZE);
	ut_file_trunc_mixed_(ut_env, UT_GIGA, 16 * UT_BK_SIZE);
	ut_file_trunc_mixed_(ut_env, UT_TERA, 32 * UT_BK_SIZE);

	ut_file_trunc_mixed_(ut_env, UT_MEGA - 11111, 11 * UT_BK_SIZE);
	ut_file_trunc_mixed_(ut_env, UT_MEGA + 11111, 11 * UT_BK_SIZE);
	ut_file_trunc_mixed_(ut_env, UT_GIGA - 11111, 11 * UT_BK_SIZE);
	ut_file_trunc_mixed_(ut_env, UT_GIGA + 11111, 11 * UT_BK_SIZE);
	ut_file_trunc_mixed_(ut_env, UT_TERA - 11111, 11 * UT_BK_SIZE);
	ut_file_trunc_mixed_(ut_env, UT_TERA + 11111, 11 * UT_BK_SIZE);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_trunc_hole_(struct ut_env *ut_env,
				loff_t off1, loff_t off2, size_t len)
{
	ino_t ino;
	ino_t dino;
	loff_t hole_off2;
	size_t hole_len, nzeros;
	const char *name = UT_NAME;
	const char *dname = UT_NAME;
	void *buf1;
	void *buf2;
	void *zeros;

	const loff_t hole_off1 = off1 + (loff_t)len;
	hole_len = (size_t)(off2 - hole_off1);
	nzeros = (hole_len < UT_UMEGA) ? hole_len : UT_UMEGA;
	hole_off2 = off2 - (loff_t)nzeros;

	buf1 = ut_randbuf(ut_env, len);
	buf2 = ut_randbuf(ut_env, len);
	zeros = ut_zerobuf(ut_env, nzeros);

	ut_mkdir_at_root(ut_env, dname, &dino);
	ut_create_file(ut_env, dino, name, &ino);
	ut_write_read(ut_env, ino, buf1, len, off1);
	ut_write_read(ut_env, ino, buf2, len, off2);
	ut_trunacate_file(ut_env, ino, off2);
	ut_read_verify(ut_env, ino, zeros, nzeros, hole_off1);
	ut_read_verify(ut_env, ino, zeros, nzeros, hole_off2);
	ut_trunacate_file(ut_env, ino, off1 + 1);
	ut_write_read(ut_env, ino, buf1, 1, off1);
	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_at_root(ut_env, dname);
}

static void ut_file_trunc_hole(struct ut_env *ut_env)
{
	ut_file_trunc_hole_(ut_env, 0, UT_MEGA, UT_BK_SIZE);
	ut_file_trunc_hole_(ut_env, 1, UT_MEGA - 1, UT_BK_SIZE);
	ut_file_trunc_hole_(ut_env, 2, 2 * UT_MEGA - 2, UT_UMEGA);
	ut_file_trunc_hole_(ut_env, 3, 3 * UT_MEGA + 3, UT_UMEGA);
	ut_file_trunc_hole_(ut_env, UT_MEGA + 1,
			    UT_MEGA + UT_BK_SIZE + 2, UT_BK_SIZE);
	ut_file_trunc_hole_(ut_env, 0, UT_GIGA, UT_UMEGA);
	ut_file_trunc_hole_(ut_env, 1, UT_GIGA - 1, UT_UMEGA);
	ut_file_trunc_hole_(ut_env, 2, 2 * UT_GIGA - 2, 2 * UT_UMEGA);
	ut_file_trunc_hole_(ut_env, UT_GIGA + 1,
			    UT_GIGA + UT_MEGA + 2, UT_UMEGA);
	ut_file_trunc_hole_(ut_env, 0, UT_TERA, UT_UMEGA);
	ut_file_trunc_hole_(ut_env, 1, UT_TERA - 1, UT_UMEGA);
	ut_file_trunc_hole_(ut_env, 2, 2 * UT_TERA - 2, UT_UMEGA);
	ut_file_trunc_hole_(ut_env, UT_TERA + 1,
			    UT_TERA + UT_MEGA + 2, UT_UMEGA);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
ut_read_zero_byte(struct ut_env *ut_env, ino_t ino, loff_t off)
{
	const uint8_t zero[1] = { 0 };

	if (off >= 0) {
		ut_read_verify(ut_env, ino, zero, 1, off);
	}
}

static void ut_file_trunc_single_byte_(struct ut_env *ut_env,
				       const loff_t *off_arr, size_t cnt)
{
	ino_t ino;
	ino_t dino;
	loff_t off;
	const char *name = UT_NAME;
	const char *dname = UT_NAME;
	const uint8_t one[1] = { 1 };

	ut_mkdir_at_root(ut_env, dname, &dino);
	ut_create_file(ut_env, dino, name, &ino);
	for (size_t i = 0; i < cnt; ++i) {
		off = off_arr[i];
		ut_write_read(ut_env, ino, one, 1, off);
	}
	for (size_t i = 0; i < cnt; ++i) {
		off = off_arr[i];
		ut_read_verify(ut_env, ino, one, 1, off);
		ut_read_zero_byte(ut_env, ino, off - 1);
	}
	for (size_t i = 0; i < cnt; ++i) {
		off = off_arr[i];
		ut_trunacate_file(ut_env, ino, off);
		ut_read_zero_byte(ut_env, ino, off - 1);
	}
	ut_trunacate_file(ut_env, ino, 0);
	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_at_root(ut_env, dname);
}

static void ut_file_trunc_single_byte(struct ut_env *ut_env)
{
	const loff_t off1[] = {
		0, UT_BK_SIZE, UT_MEGA, UT_GIGA, UT_TERA
	};
	const loff_t off2[] = {
		1, UT_BK_SIZE + 1, UT_MEGA + 1, UT_GIGA + 1, UT_TERA + 1
	};
	const loff_t off3[] = {
		77, 777, 7777, 77777, 777777, 7777777
	};

	ut_file_trunc_single_byte_(ut_env, off1, UT_ARRAY_SIZE(off1));
	ut_file_trunc_single_byte_(ut_env, off2, UT_ARRAY_SIZE(off2));
	ut_file_trunc_single_byte_(ut_env, off3, UT_ARRAY_SIZE(off3));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_trunc_tail_(struct ut_env *ut_env,
				loff_t off, size_t bsz)
{
	ino_t ino;
	ino_t dino;
	const ssize_t ssz = (loff_t)bsz;
	const char *dname = UT_NAME;
	const char *fname = UT_NAME;
	void *buf = ut_randbuf(ut_env, bsz);

	ut_mkdir_at_root(ut_env, dname, &dino);
	ut_create_file(ut_env, dino, fname, &ino);
	ut_write_read(ut_env, ino, buf, bsz, off);
	ut_read_zero_byte(ut_env, ino, off - 1);
	ut_trunacate_file(ut_env, ino, off + 1);
	ut_read_ok(ut_env, ino, buf, 1, off);
	ut_read_zero_byte(ut_env, ino, off - 1);
	ut_trunacate_file(ut_env, ino, off + ssz);
	ut_read_zero_byte(ut_env, ino, off + ssz - 1);
	ut_trunacate_file(ut_env, ino, off + ssz + 1);
	ut_read_zero_byte(ut_env, ino, off + ssz);
	ut_remove_file(ut_env, dino, fname, ino);
	ut_rmdir_at_root(ut_env, dname);
}

static void ut_file_trunc_tail(struct ut_env *ut_env)
{
	ut_file_trunc_tail_(ut_env, 0, UT_UMEGA);
	ut_file_trunc_tail_(ut_env, 1, UT_UMEGA + 4);
	ut_file_trunc_tail_(ut_env, UT_MEGA, UT_UMEGA);
	ut_file_trunc_tail_(ut_env, UT_MEGA - 1, UT_UMEGA + 8);
	ut_file_trunc_tail_(ut_env, UT_GIGA - 1, UT_UMEGA + 16);
	ut_file_trunc_tail_(ut_env, UT_FILESIZE_MAX - UT_MEGA - 1, UT_UMEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_trunc_void_(struct ut_env *ut_env,
				loff_t off, size_t bsz)
{
	ino_t ino;
	ino_t dino;
	const ino_t root_ino = UT_ROOT_INO;
	const ssize_t ssz = (loff_t)bsz;
	const loff_t end = off + ssz;
	const char *dname = UT_NAME;
	const char *fname = UT_NAME;
	struct stat st;
	char x = 'x';

	ut_mkdir_oki(ut_env, root_ino, dname, &dino);
	ut_create_file(ut_env, dino, fname, &ino);
	ut_trunacate_file(ut_env, ino, end);
	ut_read_zeros(ut_env, ino, off, bsz);
	ut_getattr_ok(ut_env, ino, &st);
	ut_assert_eq(st.st_blocks, 0);
	ut_trunacate_file(ut_env, ino, 0);
	ut_trunacate_file(ut_env, ino, end);
	ut_write_read(ut_env, ino, &x, 1, end);
	ut_read_zeros(ut_env, ino, off, bsz);
	ut_remove_file(ut_env, dino, fname, ino);
	ut_rmdir_ok(ut_env, root_ino, dname);
}

static void ut_file_trunc_void(struct ut_env *ut_env)
{
	ut_file_trunc_void_(ut_env, 0, UT_KILO);
	ut_file_trunc_void_(ut_env, 1, UT_KILO);
	ut_file_trunc_void_(ut_env, 0, UT_UMEGA);
	ut_file_trunc_void_(ut_env, 1, UT_UMEGA - 11);
	ut_file_trunc_void_(ut_env, UT_MEGA, UT_KILO);
	ut_file_trunc_void_(ut_env, UT_MEGA - 11, UT_KILO + 1);
	ut_file_trunc_void_(ut_env, UT_GIGA - 11, UT_UMEGA + 111);
	ut_file_trunc_void_(ut_env, UT_FILESIZE_MAX - UT_MEGA - 1, UT_MEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_trunc_zero_size_(struct ut_env *ut_env,
				     loff_t off, size_t bsz)
{
	ino_t ino;
	ino_t dino;
	const char *dname = UT_NAME;
	const char *fname = UT_NAME;
	struct stat st1, st2, st3;
	struct statvfs stv1, stv2, stv3;
	void *buf = ut_randbuf(ut_env, bsz);

	ut_mkdir_at_root(ut_env, dname, &dino);
	ut_create_file(ut_env, dino, fname, &ino);
	ut_getattr_ok(ut_env, ino, &st1);
	ut_statfs_ok(ut_env, ino, &stv1);
	ut_write_read(ut_env, ino, buf, bsz, off);
	ut_getattr_ok(ut_env, ino, &st2);
	ut_statfs_ok(ut_env, ino, &stv2);
	ut_assert_eq(st2.st_size, off + (loff_t)bsz);
	ut_assert_gt(st2.st_blocks, st1.st_blocks);
	ut_assert_lt(stv2.f_bfree, stv1.f_bfree);
	ut_trunacate_file(ut_env, ino, 0);
	ut_getattr_ok(ut_env, ino, &st3);
	ut_statfs_ok(ut_env, ino, &stv3);
	ut_assert_eq(st3.st_size, 0);
	ut_assert_eq(st3.st_blocks, 0);
	ut_assert_eq(stv3.f_bfree, stv1.f_bfree);
	ut_remove_file(ut_env, dino, fname, ino);
	ut_rmdir_at_root(ut_env, dname);
}

static void ut_file_trunc_zero_size(struct ut_env *ut_env)
{
	ut_file_trunc_zero_size_(ut_env, 1, UT_BK_SIZE);
	ut_file_trunc_zero_size_(ut_env, UT_KILO, UT_BK_SIZE);
	ut_file_trunc_zero_size_(ut_env, UT_MEGA, UT_BK_SIZE);
	ut_file_trunc_zero_size_(ut_env, UT_MEGA - 1, UT_BK_SIZE);
	ut_file_trunc_zero_size_(ut_env, 11 * UT_MEGA + 11, UT_BK_SIZE);
	ut_file_trunc_zero_size_(ut_env, 111 * UT_GIGA - 111, UT_BK_SIZE);
	ut_file_trunc_zero_size_(ut_env, UT_TERA, UT_UMEGA);
	ut_file_trunc_zero_size_(ut_env, UT_TERA + 1111111, UT_UMEGA - 1);
	ut_file_trunc_zero_size_(ut_env, UT_FILESIZE_MAX - UT_MEGA, UT_UMEGA);
	ut_file_trunc_zero_size_(ut_env, UT_FILESIZE_MAX - UT_MEGA - 1,
				 UT_UMEGA + 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct ut_testdef ut_local_tests[] = {
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

const struct ut_tests ut_test_file_truncate = UT_MKTESTS(ut_local_tests);


