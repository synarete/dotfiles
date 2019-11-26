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
 * Expects success for simple creat(3p)/unlink(3p) X 2
 */
static void test_posix_create_simple(gbx_env_t *gbx)
{
	int fd;
	char *path;

	path = gbx_newpath(gbx);
	gbx_expect_ok(gbx, gbx_sys_create(path, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));

	gbx_expect_ok(gbx, gbx_sys_create(path, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects success for sequence of creat(3p)/unlink(3p) of regular files.
 */
static void test_posix_create_unlink_aux(gbx_env_t *gbx, int cnt)
{
	int i;
	char *path0, *path1;

	path0 = gbx_newpath(gbx);
	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0700));
	for (i = 0; i < cnt; ++i) {
		path1 = gbx_newpathf(gbx, path0, "%lu", i);
		gbx_expect_ok(gbx, gbx_sys_create2(path1, 0600));
	}
	for (i = 0; i < cnt; ++i) {
		path1 = gbx_newpathf(gbx, path0, "%lu", i);
		gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	}
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
}

static void test_posix_create_unlink(gbx_env_t *gbx)
{
	/* dirtop */
	test_posix_create_unlink_aux(gbx, 1);
	test_posix_create_unlink_aux(gbx, 2);
	test_posix_create_unlink_aux(gbx, FNX_DIRHNDENT - 2);

	/* dirseg */
	test_posix_create_unlink_aux(gbx, FNX_DIRHNDENT - 1);
	test_posix_create_unlink_aux(gbx, FNX_DIRHNDENT + 1);
	test_posix_create_unlink_aux(gbx, FNX_DIRNSEGS);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_posix_create(gbx_env_t *gbx)
{
	const gbx_execdef_t test[] = {
		GBX_DEFTEST(test_posix_create_simple, GBX_POSIX),
		GBX_DEFTEST(test_posix_create_unlink, GBX_POSIX),
	};
	gbx_execute(gbx, test);
}


