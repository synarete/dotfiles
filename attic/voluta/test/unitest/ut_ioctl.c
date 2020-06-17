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

static void ut_ioctl_inquiry(struct voluta_ut_ctx *ut_ctx)
{
	ino_t ino;
	ino_t dino;
	const char *name = T_NAME;
	struct voluta_inquiry inq = { .version = 0 };

	voluta_ut_mkdir_at_root(ut_ctx, name, &dino);
	voluta_ut_inquiry_ok(ut_ctx, dino, &inq);
	ut_assert_eq(inq.version, VOLUTA_VERSION);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);
	voluta_ut_inquiry_ok(ut_ctx, ino, &inq);
	ut_assert_eq(inq.version, VOLUTA_VERSION);
	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_rmdir_at_root(ut_ctx, name);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_ioctl_inquiry),
};

const struct voluta_ut_tests voluta_ut_test_ioctl =
	UT_MKTESTS(ut_local_tests);
