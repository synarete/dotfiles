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


static void ut_statfs_simple(struct voluta_ut_ctx *ut_ctx)
{
	int err;
	ino_t ino = VOLUTA_INO_ROOT;
	struct statvfs stv;

	err = voluta_ut_statfs(ut_ctx, ino, &stv);
	ut_assert_ok(err);
}

static void ut_getattr_root(struct voluta_ut_ctx *ut_ctx)
{
	int err;
	ino_t ino = VOLUTA_INO_ROOT;
	struct stat st;

	err = voluta_ut_getattr(ut_ctx, ino, &st);
	ut_assert_ok(err);

	ut_assert_eq(ino, st.st_ino);
	ut_assert(S_ISDIR(st.st_mode));
	ut_assert_eq(st.st_size, VOLUTA_DIREMPTY_SIZE);
	ut_assert_eq(st.st_nlink, 2);
}

static void ut_access_root(struct voluta_ut_ctx *ut_ctx)
{
	int err;
	ino_t ino = VOLUTA_INO_ROOT;

	err = voluta_ut_access(ut_ctx, ino, R_OK);
	ut_assert_ok(err);

	err = voluta_ut_access(ut_ctx, ino, X_OK);
	ut_assert_ok(err);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_statfs_simple),
	UT_DEFTEST(ut_getattr_root),
	UT_DEFTEST(ut_access_root),
};

const struct voluta_ut_tests voluta_ut_super_tests = UT_MKTESTS(ut_local_tests);

