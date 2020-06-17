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

#define ut_assert_ok_or_err(ret, err_code) \
	ut_assert(!ret || (ret == err_code))

#define ut_assert_ok_or_nospc(ret) \
	ut_assert_ok_or_err(ret, -ENOSPC)

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t calc_wr_size(const struct statvfs *stv, size_t limit)
{
	size_t wr_size = DS_SIZE;
	const size_t nbytes_free = stv->f_bfree * stv->f_frsize;

	if (nbytes_free > wr_size) {
		wr_size = nbytes_free;
	}
	if (wr_size > limit) {
		wr_size = limit;
	}
	return wr_size;
}

static void ut_fillfs_simple(struct voluta_ut_ctx *ut_ctx)
{
	int err = 0;
	void *buf;
	ino_t ino;
	ino_t dino;
	loff_t off;
	size_t len;
	size_t nwr;
	fsblkcnt_t bfree;
	struct stat st;
	struct statvfs stv;
	const char *name = T_NAME;
	const size_t bsz = UMEGA;

	voluta_ut_mkdir_at_root(ut_ctx, name, &dino);
	voluta_ut_statfs_ok(ut_ctx, dino, &stv);
	bfree = stv.f_bfree;
	ut_assert_gt(bfree, 0);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);

	voluta_ut_statfs_ok(ut_ctx, dino, &stv);
	len = calc_wr_size(&stv, bsz);
	nwr = len;
	buf = ut_randbuf(ut_ctx, bsz);
	while (!err && (nwr == len)) {
		voluta_ut_getattr_file(ut_ctx, ino, &st);
		voluta_ut_statfs_ok(ut_ctx, dino, &stv);
		len = calc_wr_size(&stv, bsz);
		nwr = 0;
		off = st.st_size;
		err = voluta_ut_write(ut_ctx, ino, buf, len, off, &nwr);
		ut_assert_ok_or_nospc(err);
	}

	voluta_ut_release_file(ut_ctx, ino);
	voluta_ut_unlink_file(ut_ctx, dino, name);
	voluta_ut_statfs_ok(ut_ctx, dino, &stv);
	ut_assert_eq(bfree, stv.f_bfree);

	voluta_ut_rmdir_at_root(ut_ctx, name);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_fillfs_mixed(struct voluta_ut_ctx *ut_ctx)
{
	int err = 0;
	size_t idx;
	size_t idx_end = 0;
	loff_t off;
	ino_t ino;
	ino_t dino;
	size_t len;
	size_t nwr;
	struct statvfs stv;
	const char *name;
	const char *dname = T_NAME;
	size_t bsz = 2 * UMEGA;
	const void *buf = ut_randbuf(ut_ctx, bsz);

	voluta_ut_mkdir_at_root(ut_ctx, dname, &dino);

	voluta_ut_statfs_ok(ut_ctx, dino, &stv);
	idx = 0;
	while (!err && (stv.f_bfree > 2) && bsz) {
		name = ut_make_name(ut_ctx, dname, idx++);
		voluta_ut_mkdir_ok(ut_ctx, dino, name, &ino);
		voluta_ut_create_file(ut_ctx, ino, name, &ino);
		len = calc_wr_size(&stv, bsz--);
		off = (loff_t)idx;
		err = voluta_ut_write(ut_ctx, ino, buf, len, off, &nwr);
		ut_assert_ok_or_nospc(err);
		voluta_ut_release_file(ut_ctx, ino);
		voluta_ut_statfs_ok(ut_ctx, dino, &stv);
		idx_end = idx;
	}
	idx = 0;
	while (idx < idx_end) {
		name = ut_make_name(ut_ctx, dname, idx++);
		voluta_ut_lookup_ok(ut_ctx, dino, name, &ino);
		voluta_ut_unlink_file(ut_ctx, ino, name);
		voluta_ut_rmdir_ok(ut_ctx, dino, name);
	}
	voluta_ut_rmdir_at_root(ut_ctx, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_fillfs_simple),
	UT_DEFTEST(ut_fillfs_mixed),
};

const struct voluta_ut_tests voluta_ut_test_fillfs =
	UT_MKTESTS(ut_local_tests);

