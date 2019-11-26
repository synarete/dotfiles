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
 * Expects successful open(3p) with O_CREAT to set the file's access time
 */
static void test_posix_open_atime(gbx_env_t *gbx)
{
	int fd;
	char *path0, *path1;
	struct stat st0, st1;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath2(gbx, path0, gbx_genname(gbx));
	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0755));
	gbx_expect_ok(gbx, gbx_sys_stat(path0, &st0));
	gbx_suspend(gbx, 3, 1);
	gbx_expect_ok(gbx, gbx_sys_open(path1, O_CREAT | O_WRONLY, 0644, &fd));
	gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st1));
	gbx_expect_true(gbx, st0.st_atim.tv_sec < st1.st_atim.tv_sec);
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful open(3p) with O_CREAT to update parent's ctime and mtime
 * only if file did *not* exist.
 */
static void test_posix_open_mctime(gbx_env_t *gbx)
{
	int fd1, fd2;
	char *path0, *path1;
	struct stat st0, st1, st2, st3;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath2(gbx, path0, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0755));
	gbx_expect_ok(gbx, gbx_sys_stat(path0, &st0));
	gbx_suspend(gbx, 3, 2);
	gbx_expect_ok(gbx, gbx_sys_open(path1, O_CREAT | O_WRONLY, 0644, &fd1));
	gbx_expect_ok(gbx, gbx_sys_fstat(fd1, &st1));
	gbx_expect_lt(gbx, st0.st_mtim.tv_sec, st1.st_mtim.tv_sec);
	gbx_expect_lt(gbx, st0.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	gbx_expect_ok(gbx, gbx_sys_stat(path0, &st2));
	gbx_expect_lt(gbx, st0.st_mtim.tv_sec, st2.st_mtim.tv_sec);
	gbx_expect_lt(gbx, st0.st_ctim.tv_sec, st2.st_ctim.tv_sec);
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	gbx_expect_ok(gbx, gbx_sys_close(fd1));

	gbx_expect_ok(gbx, gbx_sys_create(path1, 0644, &fd1));
	gbx_expect_ok(gbx, gbx_sys_fstat(fd1, &st1));
	gbx_expect_ok(gbx, gbx_sys_stat(path0, &st0));
	gbx_suspend(gbx, 3, 2);
	gbx_expect_ok(gbx, gbx_sys_open(path1, O_CREAT | O_RDONLY, 0644, &fd2));
	gbx_expect_ok(gbx, gbx_sys_fstat(fd2, &st2));
	gbx_expect_ok(gbx, gbx_sys_stat(path0, &st3));
	gbx_expect_eq(gbx, st1.st_mtim.tv_sec, st2.st_mtim.tv_sec);
	gbx_expect_eq(gbx, st1.st_ctim.tv_sec, st2.st_ctim.tv_sec);
	gbx_expect_eq(gbx, st0.st_mtim.tv_sec, st3.st_mtim.tv_sec);
	gbx_expect_eq(gbx, st0.st_ctim.tv_sec, st3.st_ctim.tv_sec);

	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
	gbx_expect_ok(gbx, gbx_sys_close(fd1));
	gbx_expect_ok(gbx, gbx_sys_close(fd2));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects open(3p) to return ELOOP if too many symbolic links are encountered
 * while resolving pathname, or O_NOFOLLOW was specified but pathname was a
 * symbolic link.
 */
static void test_posix_open_loop(gbx_env_t *gbx)
{
	int fd;
	char *path0, *path1, *path2, *path3;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath1(gbx, gbx_genname(gbx));
	path2 = gbx_newpath2(gbx, path0, gbx_genname(gbx));
	path3 = gbx_newpath2(gbx, path1, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_symlink(path0, path1));
	gbx_expect_ok(gbx, gbx_sys_symlink(path1, path0));
	gbx_expect_err(gbx, gbx_sys_open(path2, O_RDONLY, 0, &fd), -ELOOP);
	gbx_expect_err(gbx, gbx_sys_open(path3, O_RDONLY, 0, &fd), -ELOOP);
	gbx_expect_ok(gbx, gbx_sys_unlink(path0));
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects open(3p) to return EISDIR if the  named file is a directory and
 * oflag includes O_WRONLY or O_RDWR.
 */
static void test_posix_open_isdir(gbx_env_t *gbx)
{
	int fd, fd2, err = -EISDIR;
	char *path;

	path = gbx_newpath1(gbx, gbx_genname(gbx));
	gbx_expect_ok(gbx, gbx_sys_mkdir(path, 0755));
	gbx_expect_ok(gbx, gbx_sys_open(path, O_RDONLY, 0, &fd));
	gbx_expect_err(gbx, gbx_sys_open(path, O_WRONLY, 0, &fd2), err);
	gbx_expect_err(gbx, gbx_sys_open(path, O_RDWR, 0, &fd2), err);
	gbx_expect_err(gbx, gbx_sys_open(path, O_RDONLY | O_TRUNC, 0, &fd2), err);
	gbx_expect_err(gbx, gbx_sys_open(path, O_WRONLY | O_TRUNC, 0, &fd2), err);
	gbx_expect_err(gbx, gbx_sys_open(path, O_RDWR | O_TRUNC, 0, &fd2), err);
	gbx_expect_ok(gbx, gbx_sys_rmdir(path));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects open(3p) to return EACCESS when O_TRUNC is specified and write
 * permission is denied.
 */
static void test_posix_open_noaccess_trunc(gbx_env_t *gbx)
{
	char *path1;

	path1 = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path1, 0755));
	gbx_expect_ok(gbx, gbx_sys_chown(path1, 65534, 65534));

	/* TODO: Writeme */

	gbx_expect_ok(gbx, gbx_sys_rmdir(path1));
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_posix_open(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_posix_open_atime, GBX_POSIX),
		GBX_DEFTEST(test_posix_open_mctime, GBX_POSIX),
		GBX_DEFTEST(test_posix_open_loop, GBX_POSIX),
		GBX_DEFTEST(test_posix_open_isdir, GBX_POSIX),
		GBX_DEFTEST(test_posix_open_noaccess_trunc, GBX_POSIX | GBX_CHOWN),
	};
	gbx_execute(gbx, tests);
}


