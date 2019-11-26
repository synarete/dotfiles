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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <unistd.h>

#include "graybox.h"
#include "syscalls.h"
#include "test-hooks.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects stat(3p) to successfully probe directory and return ENOENT if a
 * component of path does not name an existing file or path is an empty string.
 */
static void test_posix_stat_simple(gbx_env_t *gbx)
{
	mode_t ifmt = S_IFMT;
	struct stat st0, st1;
	char *path0, *path1;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0700));
	gbx_expect_ok(gbx, gbx_sys_stat(path0, &st0));
	gbx_expect_true(gbx, S_ISDIR(st0.st_mode));
	gbx_expect_eq(gbx, (int)(st0.st_mode & ~ifmt), 0700);
	gbx_expect_eq(gbx, (long)st0.st_nlink, 2);
	gbx_expect_err(gbx, gbx_sys_stat(path1, &st1), -ENOENT);

	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects stat(3p) to return ENOTDIR if a component of the path prefix is not
 * a directory.
 */
static void test_posix_stat_notdir(gbx_env_t *gbx)
{
	int fd;
	struct stat st0, st1, st2;
	char *path0, *path1, *path2;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath2(gbx, path0, gbx_genname(gbx));
	path2 = gbx_newpath2(gbx, path1, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0700));
	gbx_expect_ok(gbx, gbx_sys_stat(path0, &st0));
	gbx_expect_true(gbx, S_ISDIR(st0.st_mode));
	gbx_expect_ok(gbx, gbx_sys_open(path1, O_CREAT | O_RDWR, 0644, &fd));
	gbx_expect_ok(gbx, gbx_sys_stat(path1, &st1));
	gbx_expect_true(gbx, S_ISREG(st1.st_mode));
	gbx_expect_false(gbx, S_ISREG(st1.st_size));
	gbx_expect_false(gbx, S_ISREG(st1.st_blocks));
	gbx_expect_err(gbx, gbx_sys_stat(path2, &st2), -ENOTDIR);
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects statvfs(3p) to return valid result for dir-path, reg-path or rd-open
 * file-descriptor.
 */
static void test_posix_stat_statvfs(gbx_env_t *gbx)
{
	int fd;
	struct statvfs stv0, stv1, stv2, stv3;
	char *path0, *path1, *path2, *path3;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath2(gbx, path0, gbx_genname(gbx));
	path2 = gbx_newpath2(gbx, path1, gbx_genname(gbx));
	path3 = gbx_newpath2(gbx, path0, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0750));
	gbx_expect_ok(gbx, gbx_sys_create(path1, 0644, &fd));
	gbx_expect_ok(gbx, gbx_sys_statvfs(path0, &stv0));
	gbx_expect_ok(gbx, gbx_sys_statvfs(path1, &stv1));
	gbx_expect_true(gbx, (stv0.f_bavail > 0));
	gbx_expect_eq(gbx, stv0.f_fsid, stv1.f_fsid);
	gbx_expect_ok(gbx, gbx_sys_fstatvfs(fd, &stv1));
	gbx_expect_eq(gbx, stv0.f_fsid, stv1.f_fsid);
	gbx_expect_err(gbx, gbx_sys_statvfs(path2, &stv2), -ENOTDIR);
	gbx_expect_err(gbx, gbx_sys_statvfs(path3, &stv3), -ENOENT);

	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects stat(3p) to return Funex specific values.
 */
static void test_posix_stat_custom(gbx_env_t *gbx)
{
	int fd;
	char *path;
	struct stat st;

	path = gbx_newpath1(gbx, gbx_genname(gbx));
	gbx_expect(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd), 0);
	gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st));
	gbx_expect_eq(gbx, st.st_size, 0);
	gbx_expect_eq(gbx, st.st_blocks, 0);
	gbx_expect_eq(gbx, st.st_blksize, FNX_BLKSIZE);
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_posix_stat(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_posix_stat_simple, GBX_POSIX),
		GBX_DEFTEST(test_posix_stat_notdir, GBX_POSIX),
		GBX_DEFTEST(test_posix_stat_statvfs, GBX_POSIX),
		GBX_DEFTEST(test_posix_stat_custom, GBX_CUSTOM)
	};
	gbx_execute(gbx, tests);
}
