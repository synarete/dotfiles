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
#include <pthread.h>

#include "graybox.h"
#include "syscalls.h"
#include "test-hooks.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests read-write data-consistency over sparse file.
 */
static void test_sio_sparse_simple(gbx_env_t *gbx)
{
	int fd;
	char *path;
	loff_t pos = -1;
	size_t i, num, num2;
	size_t nsz, nwr, nrd;
	const size_t cnt = 7717;
	const size_t step = 524287;

	path = gbx_newpath1(gbx, gbx_genname(gbx));
	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd));
	for (i = 0; i < cnt; ++i) {
		num = (i * step);
		pos = (loff_t)num;
		nsz = sizeof(num);
		gbx_expect_ok(gbx, gbx_sys_pwrite(fd, &num, nsz, pos, &nwr));
		gbx_expect_eq(gbx, nwr, nsz);
	}
	gbx_expect_ok(gbx, gbx_sys_close(fd));

	gbx_expect_ok(gbx, gbx_sys_open(path, O_RDONLY, 0, &fd));
	for (i = 0; i < cnt; ++i) {
		num = (i * step);
		pos = (loff_t)num;
		nsz = sizeof(num2);
		gbx_expect_ok(gbx, gbx_sys_pread(fd, &num2, nsz, pos, &nrd));
		gbx_expect_eq(gbx, nrd, nsz);
		gbx_expect_eq(gbx, num, num2);
	}
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests read-write data-consistency over sparse file with syncs over same file.
 */
static void test_sio_sparse_rdwr(gbx_env_t *gbx)
{
	int fd;
	char *path;
	loff_t pos = -1;
	size_t num, num2;
	size_t nsz, nwr, nrd;
	const size_t cnt  = 127;
	const size_t step = 524287;

	path = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	for (size_t i = 0; i < 17; ++i) {
		for (size_t j = 0; j < cnt; ++j) {
			gbx_expect_ok(gbx, gbx_sys_open(path, O_RDWR, 0, &fd));
			num = i + (j * step);
			pos = (loff_t)num;
			nsz = sizeof(num);
			gbx_expect_ok(gbx, gbx_sys_pwrite(fd, &num, nsz, pos, &nwr));
			gbx_expect_eq(gbx, nwr, nsz);
			gbx_expect_ok(gbx, gbx_sys_fdatasync(fd));
			gbx_expect_ok(gbx, gbx_sys_pread(fd, &num2, nsz, pos, &nrd));
			gbx_expect_eq(gbx, nrd, nsz);
			gbx_expect_eq(gbx, num, num2);
			gbx_expect_ok(gbx, gbx_sys_close(fd));
		}
	}
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_sio_sparse(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_sio_sparse_simple, GBX_SIO),
		GBX_DEFTEST(test_sio_sparse_rdwr, GBX_SIO)
	};
	gbx_execute(gbx, tests);
}


