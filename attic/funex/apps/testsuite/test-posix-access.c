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
 * Expects access(3p) to return 0 on root-dir.
 */
static void test_posix_access_rootdir(gbx_env_t *gbx)
{
	const char *path;

	path = gbx_newpath1(gbx, "/");
	gbx_expect_ok(gbx, gbx_sys_access(path, R_OK | W_OK | X_OK));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects access(3p) to return ENOENT if a component of path does not name an
 * existing file or path is an empty string.
 */
static void test_posix_access_noent(gbx_env_t *gbx)
{
	char *path0, *path1, *path2;

	path0 = gbx_newpath(gbx);
	path1 = gbx_newpath2(gbx, path0, gbx_genname(gbx));
	path2 = gbx_newpath2(gbx, path1, "test");

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0755));
	gbx_expect_ok(gbx, gbx_sys_access(path0, F_OK));
	gbx_expect_ok(gbx, gbx_sys_access(path0, X_OK));
	gbx_expect_ok(gbx, gbx_sys_access(path0, F_OK | X_OK));

	gbx_expect_err(gbx, gbx_sys_access(path1, R_OK), -ENOENT);
	gbx_expect_err(gbx, gbx_sys_access(path1, F_OK), -ENOENT);
	gbx_expect_err(gbx, gbx_sys_access(path1, F_OK | X_OK), -ENOENT);

	gbx_expect_err(gbx, gbx_sys_access(path2, R_OK), -ENOENT);
	gbx_expect_err(gbx, gbx_sys_access(path2, F_OK), -ENOENT);
	gbx_expect_err(gbx, gbx_sys_access(path2, F_OK | X_OK), -ENOENT);

	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects access(3p) to return EINVAL if the value of the amode argument is
 * invalid.
 */
static void test_posix_access_inval(gbx_env_t *gbx)
{
	int fd, mode;
	const char *path;

	path = gbx_newpath(gbx);
	gbx_expect_ok(gbx, gbx_sys_create(path, 0644, &fd));

	mode = R_OK | W_OK | X_OK | F_OK;
	gbx_expect_err(gbx, gbx_sys_access(path, ~mode), -EINVAL);

	gbx_expect_ok(gbx, gbx_sys_unlink(path));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects access(3p) to return EACCES when a component of the path prefix
 * denies search permission
 */
static void test_posix_access_prefix(gbx_env_t *gbx)
{
	int fd, mode = R_OK;
	char *path0, *path1, *path2, *path3;

	/* TODO: Better logic via external flags */
	if (gbx->user.isroot || gbx->user.cap_sysadmin) {
		return;
	}
	path0 = gbx_newpath(gbx);
	path1 = gbx_newpath2(gbx, path0, gbx_genname(gbx));
	path2 = gbx_newpath2(gbx, path1, gbx_genname(gbx));
	path3 = gbx_newpath2(gbx, path2, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0750));
	gbx_expect_ok(gbx, gbx_sys_mkdir(path1, 0750));
	gbx_expect_ok(gbx, gbx_sys_mkdir(path2, 0750));
	gbx_expect_ok(gbx, gbx_sys_create(path3, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_access(path3, mode));
	gbx_expect_ok(gbx, gbx_sys_chmod(path2, 0200));
	gbx_suspend(gbx, 3, 0);
	gbx_expect_err(gbx, gbx_sys_access(path3, mode), -EACCES);
	gbx_expect_ok(gbx, gbx_sys_chmod(path2, 0700));
	gbx_suspend(gbx, 3, 0);
	gbx_expect_ok(gbx, gbx_sys_access(path3, mode));

	gbx_expect_ok(gbx, gbx_sys_unlink(path3));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path2));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path1));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_posix_access(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_posix_access_rootdir, GBX_POSIX),
		GBX_DEFTEST(test_posix_access_noent, GBX_POSIX),
		GBX_DEFTEST(test_posix_access_inval, GBX_POSIX),
		GBX_DEFTEST(test_posix_access_prefix, GBX_POSIX)
	};
	gbx_execute(gbx, tests);
}

