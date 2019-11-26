/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                             *
 *                            Funex File-System                                *
 *                                                                             *
 *  Copyright (C) 2014,2015 Synarete                                           *
 *                                                                             *
 *  Funex is a free software; you can redistribute it and/or modify it under   *
 *  the terms of the GNU General Public License as published by the Free       *
 *  Software Foundation, either version 3 of the License, or (at your option)  *
 *  any later version.                                                         *
 *                                                                             *
 *  Funex is distributed in the hope that it will be useful, but WITHOUT ANY   *
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS  *
 *  FOR A PARTICULAR PURPOSE. See the GNU General Public License for more      *
 *  details.                                                                   *
 *                                                                             *
 *  You should have received a copy of the GNU General Public License along    *
 *  with this program. If not, see <http://www.gnu.org/licenses/>              *
 *                                                                             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
#include <fnxconfig.h>

#include "graybox.h"
#include "syscalls.h"
#include "test-hooks.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects mkdir(3p) to create a directory with a mode modified by the process'
 * umask.
 */
static void test_posix_mkdir_umask(gbx_env_t *gbx)
{
	mode_t umsk, ifmt = S_IFMT;
	char *path0, *path1;
	struct stat st0, st1;

	umsk  = umask(S_IWGRP);
	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath2(gbx, path0, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0755));
	gbx_expect_ok(gbx, gbx_sys_stat(path0, &st0));
	gbx_expect_true(gbx, S_ISDIR(st0.st_mode));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path1, 0755));
	gbx_expect_ok(gbx, gbx_sys_stat(path1, &st1));
	gbx_expect_true(gbx, S_ISDIR(st1.st_mode));
	gbx_expect_eq(gbx, (st1.st_mode & ~ifmt), 0755);
	gbx_expect_ok(gbx, gbx_sys_rmdir(path1));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path1, 0153));
	gbx_expect_ok(gbx, gbx_sys_stat(path1, &st1));
	gbx_expect_true(gbx, S_ISDIR(st1.st_mode));
	gbx_expect_eq(gbx, (st1.st_mode & ~ifmt), 0153);
	gbx_expect_ok(gbx, gbx_sys_rmdir(path1));

	umask(077);
	gbx_expect_ok(gbx, gbx_sys_mkdir(path1, 0151));
	gbx_expect_ok(gbx, gbx_sys_stat(path1, &st1));
	gbx_expect_true(gbx, S_ISDIR(st1.st_mode));
	gbx_expect_eq(gbx, (st1.st_mode & ~ifmt), 0100);
	gbx_expect_ok(gbx, gbx_sys_rmdir(path1));

	umask(070);
	gbx_expect_ok(gbx, gbx_sys_mkdir(path1, 0345));
	gbx_expect_ok(gbx, gbx_sys_stat(path1, &st1));
	gbx_expect_true(gbx, S_ISDIR(st1.st_mode));
	gbx_expect_eq(gbx, (st1.st_mode & ~ifmt), 0305);
	gbx_expect_ok(gbx, gbx_sys_rmdir(path1));

	umask(0501);
	gbx_expect_ok(gbx, gbx_sys_mkdir(path1, 0345));
	gbx_expect_ok(gbx, gbx_sys_stat(path1, &st1));
	gbx_expect_true(gbx, S_ISDIR(st1.st_mode));
	gbx_expect_eq(gbx, (st1.st_mode & ~ifmt), 0244);

	gbx_expect_ok(gbx, gbx_sys_rmdir(path1));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
	gbx_expect_err(gbx, gbx_sys_stat(path0, &st0), -ENOENT);
	gbx_expect_err(gbx, gbx_sys_stat(path1, &st1), -ENOENT);
	umask(umsk);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects mkdir(3p) to return ELOOP if too many symbolic links were
 * encountered in translating of the pathname.
 */
static void test_posix_mkdir_loop(gbx_env_t *gbx)
{
	char *path0, *path1, *path2, *path3;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath1(gbx, gbx_genname(gbx));
	path2 = gbx_newpath2(gbx, path0, gbx_genname(gbx));
	path3 = gbx_newpath2(gbx, path1, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_symlink(path0, path1));
	gbx_expect_ok(gbx, gbx_sys_symlink(path1, path0));
	gbx_expect_err(gbx, gbx_sys_mkdir(path2, 0755), -ELOOP);
	gbx_expect_err(gbx, gbx_sys_mkdir(path3, 0755), -ELOOP);
	gbx_expect_ok(gbx, gbx_sys_unlink(path0));
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Verify creation & removal of many-many dir-entries.
 */
static void test_posix_mkdir_many(gbx_env_t *gbx, size_t lim)
{
	int fd;
	size_t i;
	char *path0, *path1;
	struct stat st;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0755));
	for (i = 0; i < lim; ++i) {
		path1 = gbx_newpathf(gbx, path0, "%08jx", i);
		gbx_expect_ok(gbx, gbx_sys_open(path1, O_CREAT | O_RDWR, 0644, &fd));
		gbx_expect_ok(gbx, gbx_sys_close(fd));
	}
	for (i = 0; i < lim; ++i) {
		path1 = gbx_newpathf(gbx, path0, "%08jx", i);
		gbx_expect_ok(gbx, gbx_sys_stat(path1, &st));
		gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	}
	gbx_expect_ok(gbx, gbx_sys_stat(path0, &st));
	gbx_expect_eq(gbx, st.st_nlink, 2);
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
}

static void test_posix_mkdir_big(gbx_env_t *gbx)
{
	test_posix_mkdir_many(gbx, FNX_DIRHNDENT * 2);
}

static void test_posix_mkdir_bigger(gbx_env_t *gbx)
{
	test_posix_mkdir_many(gbx, FNX_DIRHNDENT * 32);
}

static void test_posix_mkdir_large(gbx_env_t *gbx)
{
	test_posix_mkdir_many(gbx, FNX_DIRCHILD_MAX);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Create and remove deeply-nested dirs structure
 */
static void test_posix_mkdir_deep(gbx_env_t *gbx)
{
	size_t i;
	char *path0, *path1;
	char *pathi[64];

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0700));

	path1 = path0;
	for (i = 0; i < gbx_nelems(pathi); ++i) {
		path1 = gbx_newpathf(gbx, path1, "D%d", (int)i);
		gbx_expect_ok(gbx, gbx_sys_mkdir(path1, 0700));
		pathi[i] = path1;
	}
	for (i = gbx_nelems(pathi); i > 0; --i) {
		path1 = pathi[i - 1];
		gbx_expect_ok(gbx, gbx_sys_rmdir(path1));
	}
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_posix_mkdir(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_posix_mkdir_umask, GBX_POSIX),
		GBX_DEFTEST(test_posix_mkdir_loop, GBX_POSIX),
		GBX_DEFTEST(test_posix_mkdir_big, GBX_POSIX),
		GBX_DEFTEST(test_posix_mkdir_bigger, GBX_POSIX),
		GBX_DEFTEST(test_posix_mkdir_large, GBX_STRESS),
		GBX_DEFTEST(test_posix_mkdir_deep, GBX_STRESS),
	};
	gbx_execute(gbx, tests);
}

