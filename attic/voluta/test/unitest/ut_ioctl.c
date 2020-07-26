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

static void ut_ioctl_inquiry(struct ut_env *ut_env)
{
	ino_t ino;
	ino_t dino;
	const char *name = UT_NAME;
	struct voluta_query inq = { .version = 0 };

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_query_ok(ut_env, dino, &inq);
	ut_assert_eq(inq.version, VOLUTA_VERSION);
	ut_create_file(ut_env, dino, name, &ino);
	ut_query_ok(ut_env, ino, &inq);
	ut_assert_eq(inq.version, VOLUTA_VERSION);
	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_at_root(ut_env, name);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_ioctl_inquiry),
};

const struct ut_tests ut_test_ioctl = UT_MKTESTS(ut_local_tests);
