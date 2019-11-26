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

static void ut_file_trunc_data_(struct voluta_ut_ctx *ut_ctx, loff_t off,
				size_t bsz)
{
	ino_t ino, root_ino = VOLUTA_INO_ROOT;
	const char *name = UT_NAME;
	char *buf = ut_randbuf(ut_ctx, bsz);

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	voluta_ut_trunacate_file(ut_ctx, ino, off);
	voluta_ut_trunacate_file(ut_ctx, ino, 0);
	voluta_ut_write_read(ut_ctx, ino, buf, bsz, off);
	voluta_ut_trunacate_file(ut_ctx, ino, off);
	voluta_ut_trunacate_file(ut_ctx, ino, 2 * off);
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_trunc_simple(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_trunc_data_(ut_ctx, 0, VOLUTA_BK_SIZE);
	ut_file_trunc_data_(ut_ctx, VOLUTA_BK_SIZE, VOLUTA_BK_SIZE);
	ut_file_trunc_data_(ut_ctx, VOLUTA_SMEGA, VOLUTA_BK_SIZE);
	ut_file_trunc_data_(ut_ctx, VOLUTA_SGIGA, VOLUTA_BK_SIZE);
	ut_file_trunc_data_(ut_ctx, VOLUTA_STERA, VOLUTA_BK_SIZE);
}

static void ut_file_trunc_aligned_big(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_trunc_data_(ut_ctx, 0, VOLUTA_MEGA);
	ut_file_trunc_data_(ut_ctx, VOLUTA_BK_SIZE, VOLUTA_MEGA);
	ut_file_trunc_data_(ut_ctx, VOLUTA_SMEGA, VOLUTA_MEGA);
	ut_file_trunc_data_(ut_ctx, VOLUTA_SGIGA, VOLUTA_MEGA);
	ut_file_trunc_data_(ut_ctx, VOLUTA_STERA, VOLUTA_MEGA);
}

static void ut_file_trunc_aligned_large(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_trunc_data_(ut_ctx, 0, 2 * VOLUTA_MEGA);
	ut_file_trunc_data_(ut_ctx, VOLUTA_BK_SIZE, 2 * VOLUTA_MEGA);
	ut_file_trunc_data_(ut_ctx, VOLUTA_SMEGA, 2 * VOLUTA_MEGA);
	ut_file_trunc_data_(ut_ctx, VOLUTA_SGIGA, 2 * VOLUTA_MEGA);
	ut_file_trunc_data_(ut_ctx, VOLUTA_STERA, 2 * VOLUTA_MEGA);
}

static void ut_file_trunc_unaligned(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_trunc_data_(ut_ctx, 1, VOLUTA_BK_SIZE + 2);
	ut_file_trunc_data_(ut_ctx, VOLUTA_BK_SIZE - 1,
			    2 * VOLUTA_BK_SIZE + 3);
	ut_file_trunc_data_(ut_ctx, 7 * VOLUTA_BK_SIZE - 7,
			    7 * VOLUTA_BK_SIZE + 7);
	ut_file_trunc_data_(ut_ctx, 11 * VOLUTA_SMEGA - 11,
			    11 * VOLUTA_BK_SIZE + 11);
	ut_file_trunc_data_(ut_ctx, 13 * VOLUTA_SGIGA - 13,
			    13 * VOLUTA_BK_SIZE + 13);
	ut_file_trunc_data_(ut_ctx, VOLUTA_STERA - 11111,
			    VOLUTA_BK_SIZE + 111111);
	ut_file_trunc_data_(ut_ctx, VOLUTA_STERA - 1111111,
			    VOLUTA_BK_SIZE + 1111111);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_trunc_mixed_(struct voluta_ut_ctx *ut_ctx, loff_t off,
				 size_t len)
{
	ino_t ino, root_ino = VOLUTA_INO_ROOT;
	uint8_t *buf;
	const loff_t eoff = off + (loff_t)len;
	const loff_t zoff = off - (loff_t)len;
	const size_t bsz = 2 * len;
	const char *name = UT_NAME;

	ut_assert(len >= VOLUTA_BK_SIZE);
	ut_assert(zoff >= 0);

	buf = ut_randbuf(ut_ctx, bsz);
	memset(buf, 0, bsz / 2);

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	voluta_ut_write_read(ut_ctx, ino, buf + len, len, off);
	voluta_ut_trunacate_file(ut_ctx, ino, eoff - 1);
	voluta_ut_read_verify(ut_ctx, ino, buf, bsz - 1, zoff);
	voluta_ut_trunacate_file(ut_ctx, ino, eoff - VOLUTA_BK_SIZE + 1);
	voluta_ut_read_verify(ut_ctx, ino, buf, bsz - VOLUTA_BK_SIZE + 1, zoff);
	voluta_ut_trunacate_file(ut_ctx, ino, off);
	voluta_ut_read_verify(ut_ctx, ino, buf, len, zoff);
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_trunc_mixed(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_trunc_mixed_(ut_ctx, VOLUTA_BK_SIZE, VOLUTA_BK_SIZE);
	ut_file_trunc_mixed_(ut_ctx, VOLUTA_SMEGA, 8 * VOLUTA_BK_SIZE);
	ut_file_trunc_mixed_(ut_ctx, VOLUTA_SGIGA, 16 * VOLUTA_BK_SIZE);
	ut_file_trunc_mixed_(ut_ctx, VOLUTA_STERA, 32 * VOLUTA_BK_SIZE);

	ut_file_trunc_mixed_(ut_ctx, VOLUTA_SMEGA - 11111, 11 * VOLUTA_BK_SIZE);
	ut_file_trunc_mixed_(ut_ctx, VOLUTA_SMEGA + 11111, 11 * VOLUTA_BK_SIZE);
	ut_file_trunc_mixed_(ut_ctx, VOLUTA_SGIGA - 11111, 11 * VOLUTA_BK_SIZE);
	ut_file_trunc_mixed_(ut_ctx, VOLUTA_SGIGA + 11111, 11 * VOLUTA_BK_SIZE);
	ut_file_trunc_mixed_(ut_ctx, VOLUTA_STERA - 11111, 11 * VOLUTA_BK_SIZE);
	ut_file_trunc_mixed_(ut_ctx, VOLUTA_STERA + 11111, 11 * VOLUTA_BK_SIZE);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_trunc_hole_(struct voluta_ut_ctx *ut_ctx,
				loff_t off1, loff_t off2, size_t len)
{
	ino_t ino, root_ino = VOLUTA_INO_ROOT;
	loff_t hole_off1, hole_off2;
	size_t hole_len, nzeros;
	const char *name = UT_NAME;
	uint8_t *buf1, *buf2, *zeros;

	hole_off1 = off1 + (loff_t)len;
	hole_len = (size_t)(off2 - hole_off1);
	nzeros = (hole_len < VOLUTA_MEGA) ? hole_len : VOLUTA_MEGA;
	hole_off2 = off2 - (loff_t)nzeros;

	ut_assert_gt(off2, off1);
	ut_assert_gt((off2 - off1), (loff_t)len);
	ut_assert_gt(off2, hole_off1);

	buf1 = ut_randbuf(ut_ctx, len);
	buf2 = ut_randbuf(ut_ctx, len);
	zeros = ut_zerobuf(ut_ctx, nzeros);

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	voluta_ut_write_read(ut_ctx, ino, buf1, len, off1);
	voluta_ut_write_read(ut_ctx, ino, buf2, len, off2);
	voluta_ut_trunacate_file(ut_ctx, ino, off2);
	voluta_ut_read_verify(ut_ctx, ino, zeros, nzeros, hole_off1);
	voluta_ut_read_verify(ut_ctx, ino, zeros, nzeros, hole_off2);
	voluta_ut_trunacate_file(ut_ctx, ino, off1 + 1);
	voluta_ut_write_read(ut_ctx, ino, buf1, 1, off1);
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_trunc_hole(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_trunc_hole_(ut_ctx, 0, VOLUTA_SMEGA, VOLUTA_BK_SIZE);
	ut_file_trunc_hole_(ut_ctx, 1, VOLUTA_SMEGA - 1, VOLUTA_BK_SIZE);
	ut_file_trunc_hole_(ut_ctx, 2, 2 * VOLUTA_SMEGA - 2, VOLUTA_MEGA);
	ut_file_trunc_hole_(ut_ctx, 3, 3 * VOLUTA_SMEGA + 3, VOLUTA_MEGA);
	ut_file_trunc_hole_(ut_ctx, VOLUTA_SMEGA + 1,
			    VOLUTA_SMEGA + VOLUTA_BK_SIZE + 2, VOLUTA_BK_SIZE);
	ut_file_trunc_hole_(ut_ctx, 0, VOLUTA_SGIGA, VOLUTA_MEGA);
	ut_file_trunc_hole_(ut_ctx, 1, VOLUTA_SGIGA - 1, VOLUTA_MEGA);
	ut_file_trunc_hole_(ut_ctx, 2, 2 * VOLUTA_SGIGA - 2, 2 * VOLUTA_MEGA);
	ut_file_trunc_hole_(ut_ctx, VOLUTA_SGIGA + 1,
			    VOLUTA_SGIGA + VOLUTA_SMEGA + 2, VOLUTA_MEGA);
	ut_file_trunc_hole_(ut_ctx, 0, VOLUTA_STERA, VOLUTA_MEGA);
	ut_file_trunc_hole_(ut_ctx, 1, VOLUTA_STERA - 1, VOLUTA_MEGA);
	ut_file_trunc_hole_(ut_ctx, 2, 2 * VOLUTA_STERA - 2, VOLUTA_MEGA);
	ut_file_trunc_hole_(ut_ctx, VOLUTA_STERA + 1,
			    VOLUTA_STERA + VOLUTA_SMEGA + 2, VOLUTA_MEGA);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_read_zero_byte(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			      loff_t off)
{
	const uint8_t nil[1] = { 0 };

	if (off > 0) {
		voluta_ut_read_verify(ut_ctx, ino, nil, 1, off);
	}
}

static void ut_file_trunc_single_byte_(struct voluta_ut_ctx *ut_ctx,
				       const loff_t *off_arr, size_t cnt)
{
	ino_t ino, root_ino = VOLUTA_INO_ROOT;
	loff_t off;
	size_t i;
	const char *name = UT_NAME;
	const uint8_t one[1] = { 1 };

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	for (i = 0; i < cnt; ++i) {
		off = off_arr[i];
		voluta_ut_write_read(ut_ctx, ino, one, 1, off);
	}
	for (i = 0; i < cnt; ++i) {
		off = off_arr[i];
		voluta_ut_read_verify(ut_ctx, ino, one, 1, off);
		ut_read_zero_byte(ut_ctx, ino, off - 1);
	}
	for (i = 0; i < cnt; ++i) {
		off = off_arr[i];
		voluta_ut_trunacate_file(ut_ctx, ino, off);
		ut_read_zero_byte(ut_ctx, ino, off - 1);
	}
	voluta_ut_trunacate_file(ut_ctx, ino, 0);
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_trunc_single_byte(struct voluta_ut_ctx *ut_ctx)
{
	const loff_t off1[] = {
		0, VOLUTA_BK_SIZE, VOLUTA_SMEGA,
		VOLUTA_SGIGA, VOLUTA_STERA
	};
	const loff_t off2[] = {
		1, VOLUTA_BK_SIZE + 1, VOLUTA_SMEGA + 1,
		VOLUTA_SGIGA + 1, VOLUTA_STERA + 1
	};
	const loff_t off3[] = {
		77, 777, 7777, 77777, 777777, 7777777
	};

	ut_file_trunc_single_byte_(ut_ctx, off1, VOLUTA_ARRAY_SIZE(off1));
	ut_file_trunc_single_byte_(ut_ctx, off2, VOLUTA_ARRAY_SIZE(off2));
	ut_file_trunc_single_byte_(ut_ctx, off3, VOLUTA_ARRAY_SIZE(off3));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_trunc_tail_(struct voluta_ut_ctx *ut_ctx, loff_t off,
				size_t bsz)
{
	ino_t ino, dino, root_ino = VOLUTA_INO_ROOT;
	const ssize_t ssz = (loff_t)bsz;
	const char *dname = UT_NAME;
	const char *fname = UT_NAME;
	void *buf = ut_randbuf(ut_ctx, bsz);

	voluta_ut_make_dir(ut_ctx, root_ino, dname, &dino);
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
	voluta_ut_remove_dir(ut_ctx, root_ino, dname);
}

static void ut_file_trunc_tail(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_trunc_tail_(ut_ctx, 0, VOLUTA_MEGA);
	ut_file_trunc_tail_(ut_ctx, 1, VOLUTA_MEGA + 4);
	ut_file_trunc_tail_(ut_ctx, VOLUTA_SMEGA, VOLUTA_MEGA);
	ut_file_trunc_tail_(ut_ctx, VOLUTA_SMEGA - 1, VOLUTA_MEGA + 8);
	ut_file_trunc_tail_(ut_ctx, VOLUTA_SGIGA - 1, VOLUTA_MEGA + 16);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_trunc_zeros_(struct voluta_ut_ctx *ut_ctx,
				 loff_t off, size_t bsz)
{
	ino_t ino, dino, root_ino = VOLUTA_INO_ROOT;
	const ssize_t ssz = (loff_t)bsz;
	const loff_t end = off + ssz;
	const char *dname = UT_NAME;
	const char *fname = UT_NAME;
	struct stat st;
	char b = 1;

	voluta_ut_make_dir(ut_ctx, root_ino, dname, &dino);
	voluta_ut_create_file(ut_ctx, dino, fname, &ino);
	voluta_ut_trunacate_file(ut_ctx, ino, end);
	voluta_ut_read_zeros(ut_ctx, ino, off, bsz);
	voluta_ut_getattr_exists(ut_ctx, ino, &st);
	ut_assert_eq(st.st_blocks, 0);
	voluta_ut_trunacate_file(ut_ctx, ino, 0);
	voluta_ut_trunacate_file(ut_ctx, ino, end);
	voluta_ut_write_read(ut_ctx, ino, &b, 1, end);
	voluta_ut_read_zeros(ut_ctx, ino, off, bsz);
	voluta_ut_remove_file(ut_ctx, dino, fname, ino);
	voluta_ut_remove_dir(ut_ctx, root_ino, dname);
}

static void ut_file_trunc_zeros(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_trunc_zeros_(ut_ctx, 0, VOLUTA_KILO);
	ut_file_trunc_zeros_(ut_ctx, 1, VOLUTA_KILO);
	ut_file_trunc_zeros_(ut_ctx, 0, VOLUTA_MEGA);
	ut_file_trunc_zeros_(ut_ctx, 1, VOLUTA_MEGA - 11);
	ut_file_trunc_zeros_(ut_ctx, VOLUTA_SMEGA, VOLUTA_KILO);
	ut_file_trunc_zeros_(ut_ctx, VOLUTA_SMEGA - 11, VOLUTA_KILO + 1);
	ut_file_trunc_zeros_(ut_ctx, VOLUTA_SGIGA - 11, VOLUTA_MEGA + 111);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_file_trunc_simple),
	UT_DEFTEST(ut_file_trunc_aligned_big),
	UT_DEFTEST(ut_file_trunc_aligned_large),
	UT_DEFTEST(ut_file_trunc_unaligned),
	UT_DEFTEST(ut_file_trunc_mixed),
	UT_DEFTEST(ut_file_trunc_hole),
	UT_DEFTEST(ut_file_trunc_single_byte),
	UT_DEFTEST(ut_file_trunc_tail),
	UT_DEFTEST(ut_file_trunc_zeros),
};

const struct voluta_ut_tests voluta_ut_file_truncate_tests =
	UT_MKTESTS(ut_local_tests);


