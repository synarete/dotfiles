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


static void ut_file_lseek_simple_(struct ut_env *ut_env, loff_t off)
{
	ino_t ino;
	ino_t dino;
	loff_t off_data;
	loff_t off_hole;
	char d = 'd';
	struct stat st;
	const char *name = UT_NAME;
	const loff_t step = 2 * UT_BK_SIZE;

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_create_file(ut_env, dino, name, &ino);
	ut_trunacate_file(ut_env, ino, off + step + 1);
	ut_getattr_file(ut_env, ino, &st);
	ut_lseek_data(ut_env, ino, 0, &off_data);
	ut_assert_eq(off_data, st.st_size);
	ut_write_read(ut_env, ino, &d, 1, off);
	ut_lseek_data(ut_env, ino, 0, &off_data);
	ut_assert_eq(off_data, ut_off_baligned(off));
	ut_lseek_data(ut_env, ino, off ? (off - 1) : 0, &off_data);
	ut_assert_eq(ut_off_baligned(off_data), ut_off_baligned(off));
	ut_lseek_data(ut_env, ino, off + step, &off_data);
	ut_assert_eq(off_data, st.st_size);
	ut_trunacate_file(ut_env, ino, off + 2);
	ut_lseek_hole(ut_env, ino, off, &off_hole);
	ut_assert_eq(off_hole, off + 2);
	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_at_root(ut_env, name);
}

static void ut_file_lseek_simple(struct ut_env *ut_env)
{
	ut_file_lseek_simple_(ut_env, 0);
	ut_file_lseek_simple_(ut_env, 1);
	ut_file_lseek_simple_(ut_env, UT_MEGA + 1);
	ut_file_lseek_simple_(ut_env, UT_GIGA - 3);
	ut_file_lseek_simple_(ut_env, UT_TERA + 5);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_lseek_holes_(struct ut_env *ut_env,
				 loff_t base_off, size_t cnt)
{
	ino_t ino;
	ino_t dino;
	loff_t off;
	loff_t off_data;
	loff_t off_hole;
	struct stat st;
	const char *name = UT_NAME;
	const size_t bsz = UT_BK_SIZE;
	void *buf = ut_randbuf(ut_env, bsz);

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_create_file(ut_env, dino, name, &ino);
	for (size_t i = 0; i < cnt; ++i) {
		off = base_off + (loff_t)(2 * bsz * (i + 1));
		ut_write_read(ut_env, ino, buf, bsz, off);
		ut_getattr_file(ut_env, ino, &st);
		ut_lseek_hole(ut_env, ino, off, &off_hole);
		ut_assert_eq(off_hole, st.st_size);
	}
	for (size_t i = 0; i < cnt; ++i) {
		off = base_off + (loff_t)(2 * bsz * (i + 1));
		ut_lseek_data(ut_env, ino, off, &off_data);
		ut_assert_eq(off_data, off);
		ut_lseek_hole(ut_env, ino, off, &off_hole);
		ut_assert_eq(off_hole, off + (loff_t)bsz);
	}
	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_at_root(ut_env, name);
}


static void ut_file_lseek_holes(struct ut_env *ut_env)
{
	ut_file_lseek_holes_(ut_env, 0, 10);
	ut_file_lseek_holes_(ut_env, UT_MEGA, 100);
	ut_file_lseek_holes_(ut_env, UT_GIGA, 1000);
	ut_file_lseek_holes_(ut_env, UT_TERA, 10000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_lseek_sparse_(struct ut_env *ut_env,
				  loff_t off_base, loff_t step, size_t nsteps)
{
	ino_t ino;
	ino_t dino;
	loff_t off;
	loff_t off_data;
	loff_t off_hole;
	loff_t off_next;
	const char *name = UT_NAME;

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_create_file(ut_env, dino, name, &ino);
	for (size_t i = 0; i < nsteps; ++i) {
		off = off_base + ((loff_t)i * step);
		ut_write_read1(ut_env, ino, off);
	}
	for (size_t i = 0; i < (nsteps - 1); ++i) {
		off = off_base + ((loff_t)i * step);
		off_next = off_base + ((loff_t)(i + 1) * step);
		ut_lseek_data(ut_env, ino, off, &off_data);
		ut_assert_eq(off_data, off);

		ut_lseek_hole(ut_env, ino, off, &off_hole);
		ut_assert_eq(off_hole, off_data + UT_BK_SIZE);

		off = (off + off_next) / 2;
		ut_lseek_data(ut_env, ino, off, &off_data);
		ut_assert_eq(off_data, off_next);
	}
	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_at_root(ut_env, name);
}

static void ut_file_lseek_sparse(struct ut_env *ut_env)
{
	ut_file_lseek_sparse_(ut_env, 0, 10 * UT_BK_SIZE, 10);
	ut_file_lseek_sparse_(ut_env, UT_MEGA, UT_GIGA, 100);
	ut_file_lseek_sparse_(ut_env, UT_GIGA, UT_MEGA, 1000);
	ut_file_lseek_sparse_(ut_env, UT_TERA, 11 * UT_BK_SIZE, 10000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_file_lseek_simple),
	UT_DEFTEST(ut_file_lseek_holes),
	UT_DEFTEST(ut_file_lseek_sparse),
};

const struct ut_tests ut_test_file_lseek = UT_MKTESTS(ut_local_tests);
