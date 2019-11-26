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
#include <unistd.h>
#include <fcntl.h>

#include "graybox.h"
#include "syscalls.h"
#include "test-hooks.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects truncate(3p) a regular file named by path to have a size which
 * shall be equal to length bytes.
 */
static void test_sio_truncate_basic(gbx_env_t *gbx)
{
	int fd;
	size_t i, nwr, cnt = 100;
	loff_t off;
	struct stat st;
	const char *path;

	path = gbx_newpath1(gbx, gbx_genname(gbx));
	gbx_expect_ok(gbx, gbx_sys_create(path, 0600, &fd));
	for (i = 0; i < cnt; ++i) {
		gbx_expect_ok(gbx, gbx_sys_write(fd, path, strlen(path), &nwr));
	}
	for (i = cnt; i > 0; i--) {
		off = (loff_t)(19 * i);
		gbx_expect_ok(gbx, gbx_sys_ftruncate(fd, off));
		gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st));
		gbx_expect_eq(gbx, st.st_size, off);
	}
	for (i = 0; i < cnt; i++) {
		off = (loff_t)(1811 * i);
		gbx_expect_ok(gbx, gbx_sys_ftruncate(fd, off));
		gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st));
		gbx_expect_eq(gbx, st.st_size, off);
	}
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_sio_truncate(gbx_env_t *gboxctx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_sio_truncate_basic, GBX_SIO)
	};
	gbx_execute(gboxctx, tests);
}


