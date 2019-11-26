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
 * Expects successful unlink(3p) of directory entry.
 */
static void test_posix_unlink_reg(gbx_env_t *gbx)
{
	int fd;
	char *path;
	struct stat st;

	path = gbx_newpath(gbx);
	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0700, &fd));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_lstat(path, &st));
	gbx_expect_true(gbx, S_ISREG(st.st_mode));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
	gbx_expect_err(gbx, gbx_sys_unlink(path), -ENOENT);
	gbx_expect_err(gbx, gbx_sys_lstat(path, &st), -ENOENT);
}

static void test_posix_unlink_symlink(gbx_env_t *gbx)
{
	int fd;
	struct stat st;
	char *path0, *path1;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_create(path0, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_close(fd));

	gbx_expect_ok(gbx, gbx_sys_symlink(path0, path1));
	gbx_expect_ok(gbx, gbx_sys_lstat(path1, &st));
	gbx_expect_true(gbx, S_ISLNK(st.st_mode));
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	gbx_expect_err(gbx, gbx_sys_unlink(path1), -ENOENT);
	gbx_expect_err(gbx, gbx_sys_stat(path1, &st), -ENOENT);

	gbx_expect_ok(gbx, gbx_sys_unlink(path0));
	gbx_expect_err(gbx, gbx_sys_unlink(path0), -ENOENT);
}

static void test_posix_unlink_fifo(gbx_env_t *gbx)
{
	char *path;
	struct stat st;

	path = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkfifo(path, 0644));
	gbx_expect_ok(gbx, gbx_sys_lstat(path, &st));
	gbx_expect_true(gbx, S_ISFIFO(st.st_mode));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
	gbx_expect_err(gbx, gbx_sys_unlink(path), -ENOENT);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful unlink(3p) to return -ENOTDIR if a component of the
 * path prefix is not a directory.
 */
static void test_posix_unlink_notdir(gbx_env_t *gbx)
{
	int fd;
	struct stat st;
	char *path0, *path1, *path2;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath2(gbx, path0, gbx_genname(gbx));
	path2 = gbx_newpath2(gbx, path1, "no-entry");

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0755));
	gbx_expect_ok(gbx, gbx_sys_stat(path0, &st));
	gbx_expect_ok(gbx, gbx_sys_open(path1, O_CREAT | O_RDWR, 0700, &fd));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_err(gbx, gbx_sys_unlink(path2), -ENOTDIR);
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_posix_unlink(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_posix_unlink_reg, GBX_POSIX),
		GBX_DEFTEST(test_posix_unlink_symlink, GBX_POSIX),
		GBX_DEFTEST(test_posix_unlink_fifo, GBX_POSIX),
		GBX_DEFTEST(test_posix_unlink_notdir, GBX_POSIX),
	};
	gbx_execute(gbx, tests);
}


