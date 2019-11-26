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
 * Expects chmod(3p) to do change permissions.
 */
static void test_posix_chmod_basic(gbx_env_t *gbx)
{
	int fd;
	mode_t ifmt = S_IFMT;
	struct stat st;
	char *path0, *path1, *path2;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath1(gbx, gbx_genname(gbx));
	path2 = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_create(path0, 0644, &fd));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_stat(path0, &st));
	gbx_expect_eq(gbx, (st.st_mode & ~ifmt), 0644);
	gbx_expect_ok(gbx, gbx_sys_chmod(path0, 0111));
	gbx_expect_ok(gbx, gbx_sys_stat(path0, &st));
	gbx_expect_eq(gbx, (st.st_mode & ~ifmt), 0111);
	gbx_expect_ok(gbx, gbx_sys_unlink(path0));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path1, 0755));
	gbx_expect_ok(gbx, gbx_sys_stat(path1, &st));
	gbx_expect_eq(gbx, (st.st_mode & ~ifmt), 0755);
	gbx_expect_ok(gbx, gbx_sys_chmod(path1, 0753));
	gbx_expect_ok(gbx, gbx_sys_stat(path1, &st));
	gbx_expect_eq(gbx, (st.st_mode & ~ifmt), 0753);
	gbx_expect_ok(gbx, gbx_sys_rmdir(path1));

	gbx_expect_ok(gbx, gbx_sys_create(path0, 0644, &fd));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_symlink(path0, path2));
	gbx_expect_ok(gbx, gbx_sys_stat(path2, &st));
	gbx_expect_eq(gbx, (st.st_mode & ~ifmt), 0644);
	gbx_expect_ok(gbx, gbx_sys_chmod(path2, 0321));
	gbx_expect_ok(gbx, gbx_sys_stat(path2, &st));
	gbx_expect_eq(gbx, (st.st_mode & ~ifmt), 0321);
	gbx_expect_ok(gbx, gbx_sys_stat(path0, &st));
	gbx_expect_eq(gbx, (st.st_mode & ~ifmt), 0321);
	gbx_expect_ok(gbx, gbx_sys_unlink(path0));
	gbx_expect_ok(gbx, gbx_sys_unlink(path2));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects chmod(3p) to updates ctime if successful.
 */
static void test_posix_chmod_ctime(gbx_env_t *gbx)
{
	int fd;
	struct stat st1, st2;
	char *path0, *path1, *path2;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath1(gbx, gbx_genname(gbx));
	path2 = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_create(path0, 0644, &fd));
	gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st1));
	gbx_suspend(gbx, 3, 2);
	gbx_expect_ok(gbx, gbx_sys_chmod(path0, 0111));
	gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st2));
	gbx_expect_true(gbx, (st1.st_ctim.tv_sec < st2.st_ctim.tv_sec));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path0));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path1, 0755));
	gbx_expect_ok(gbx, gbx_sys_stat(path1, &st1));
	gbx_suspend(gbx, 3, 2);
	gbx_expect_ok(gbx, gbx_sys_chmod(path1, 0753));
	gbx_expect_ok(gbx, gbx_sys_stat(path1, &st2));
	gbx_expect_true(gbx, (st1.st_ctim.tv_sec < st2.st_ctim.tv_sec));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path1));

	gbx_expect_ok(gbx, gbx_sys_mkfifo(path2, 0640));
	gbx_expect_ok(gbx, gbx_sys_stat(path2, &st1));
	gbx_suspend(gbx, 3, 2);
	gbx_expect_ok(gbx, gbx_sys_chmod(path2, 0300));
	gbx_expect_ok(gbx, gbx_sys_stat(path2, &st2));
	gbx_expect_true(gbx, (st1.st_ctim.tv_sec < st2.st_ctim.tv_sec));
	gbx_expect_ok(gbx, gbx_sys_unlink(path2));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_posix_chmod(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_posix_chmod_basic, GBX_POSIX),
		GBX_DEFTEST(test_posix_chmod_ctime, GBX_POSIX)
	};
	gbx_execute(gbx, tests);
}


