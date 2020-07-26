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

static uint8_t dvec_first_byte(const struct ut_dvec *dvec)
{
	return dvec->dat[0];
}

static uint8_t dvec_last_byte(const struct ut_dvec *dvec)
{
	return dvec->dat[dvec->len - 1];
}

static loff_t dvec_last_off(const struct ut_dvec *dvec)
{
	return dvec->off + (loff_t)dvec->len - 1;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_rw_plus_minus_1_(struct ut_env *ut_env,
				ino_t ino, loff_t off, size_t len)
{
	uint8_t byte;
	struct ut_dvec *dv1;
	struct ut_dvec *dv2;
	struct ut_dvec *dv3;
	struct ut_dvec *dv4;

	dv1 = ut_new_dvec(ut_env, off, len);
	ut_write_dvec(ut_env, ino, dv1);
	dv2 = ut_new_dvec(ut_env, off + 1, len);
	ut_write_dvec(ut_env, ino, dv2);
	byte = dvec_first_byte(dv1);
	ut_read_ok(ut_env, ino, &byte, 1, dv1->off);
	dv3 = ut_new_dvec(ut_env, off - 1, len);
	ut_write_dvec(ut_env, ino, dv3);
	byte = dvec_last_byte(dv2);
	ut_read_ok(ut_env, ino, &byte, 1, dvec_last_off(dv2));
	ut_fallocate_punch_hole(ut_env, ino, off, (loff_t)len);
	ut_read_zeros(ut_env, ino, off, len);
	byte = dvec_first_byte(dv3);
	ut_read_ok(ut_env, ino, &byte, 1, dv3->off);
	dv4 = ut_new_dvec(ut_env, off, len);
	ut_write_dvec(ut_env, ino, dv4);
	ut_fallocate_punch_hole(ut_env, ino, off - 1, (loff_t)len + 2);
	ut_read_zeros(ut_env, ino, off - 1, len + 2);
}

static void ut_file_edges_1_(struct ut_env *ut_env,
			     loff_t off, size_t len)
{
	ino_t ino;
	ino_t dino;
	const char *name = UT_NAME;

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_create_file(ut_env, dino, name, &ino);
	ut_rw_plus_minus_1_(ut_env, ino, off, len);
	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_at_root(ut_env, name);
}

static void ut_file_edges_aligned(struct ut_env *ut_env)
{
	ut_file_edges_1_(ut_env, UT_BK_SIZE, UT_BK_SIZE);
	ut_file_edges_1_(ut_env, UT_MEGA, 4 * UT_BK_SIZE);
	ut_file_edges_1_(ut_env, UT_GIGA, 8 * UT_BK_SIZE);
	ut_file_edges_1_(ut_env, UT_TERA, 16 * UT_BK_SIZE);
}

static void ut_file_edges_unaligned(struct ut_env *ut_env)
{
	ut_file_edges_1_(ut_env, UT_BK_SIZE + 11, UT_BK_SIZE - 1);
	ut_file_edges_1_(ut_env, UT_MEGA - 1111, 4 * UT_BK_SIZE + 1);
	ut_file_edges_1_(ut_env, UT_GIGA - 11111, 8 * UT_BK_SIZE + 11);
	ut_file_edges_1_(ut_env, UT_TERA - 111111, 16 * UT_BK_SIZE + 111);
}

static void ut_file_edges_special(struct ut_env *ut_env)
{
	const size_t bksz = UT_BK_SIZE;
	const loff_t bkssz = (loff_t)bksz;
	const loff_t filemap_sz = (UT_FILEMAP_NCHILD * UT_BK_SIZE);
	const loff_t filemap_sz2 = filemap_sz * UT_FILEMAP_NCHILD;
	const loff_t filesize_max = UT_FILESIZE_MAX;

	ut_file_edges_1_(ut_env, filemap_sz, bksz);
	ut_file_edges_1_(ut_env, filemap_sz, 2 * bksz);
	ut_file_edges_1_(ut_env, filemap_sz - 11, bksz + 111);
	ut_file_edges_1_(ut_env, filemap_sz - 111, 2 * bksz + 1111);
	ut_file_edges_1_(ut_env, 2 * filemap_sz, 2 * bksz);
	ut_file_edges_1_(ut_env, 2 * filemap_sz - 1, bksz + 2);
	ut_file_edges_1_(ut_env, filemap_sz + filemap_sz2, 2 * bksz);
	ut_file_edges_1_(ut_env, filemap_sz + filemap_sz2 - 2, bksz + 3);
	ut_file_edges_1_(ut_env, filemap_sz2 - 2, bksz + 3);
	ut_file_edges_1_(ut_env, filesize_max / 2, bksz);
	ut_file_edges_1_(ut_env, filesize_max - (2 * bkssz), bksz);
	ut_file_edges_1_(ut_env, filesize_max - bkssz - 1, bksz);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_file_edges_aligned),
	UT_DEFTEST(ut_file_edges_unaligned),
	UT_DEFTEST(ut_file_edges_special),
};

const struct ut_tests ut_test_file_edges = UT_MKTESTS(ut_local_tests);
