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
 * Expects successful write(3p) of single block to regular file
 */
static void test_posix_write_reg(gbx_env_t *gbx)
{
	int fd;
	char *path;
	void *buf1;
	size_t nwr = 0;
	size_t bsz = FNX_BLKSIZE;

	path = gbx_newpath(gbx);
	buf1 = gbx_newrbuf(gbx, bsz);
	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_WRONLY, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_write(fd, buf1, bsz, &nwr));
	gbx_expect_eq(gbx, bsz, nwr);

	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful write(3p) to unlinked file
 */
static void test_posix_write_unlinked(gbx_env_t *gbx)
{
	int fd;
	char *path;
	void *buf1;
	size_t nwr = 0;
	size_t bsz = FNX_BLKSIZE;

	path = gbx_newpath(gbx);
	buf1 = gbx_newrbuf(gbx, bsz);
	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_WRONLY, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));

	gbx_expect_ok(gbx, gbx_sys_write(fd, buf1, bsz, &nwr));
	gbx_expect_eq(gbx, bsz, nwr);
	gbx_expect_ok(gbx, gbx_sys_write(fd, buf1, bsz, &nwr));
	gbx_expect_eq(gbx, bsz, nwr);
	gbx_expect_ok(gbx, gbx_sys_close(fd));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects pwrite(3p) to return -EFBIG if an attempt was made to write a
 * file that exceeds the implementation-defined maximum file size.
 */
static void test_posix_write_efbig(gbx_env_t *gbx)
{
	int fd;
	char *path;
	void *buf1;
	size_t nwr = 0;
	size_t bsz = FNX_BLKSIZE;
	const loff_t off = (loff_t)FNX_REGSIZE_MAX;

	path = gbx_newpath(gbx);
	buf1 = gbx_newrbuf(gbx, bsz);
	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_pwrite(fd, buf1, bsz, 0, &nwr));
	gbx_expect_ok(gbx, gbx_sys_pwrite(fd, buf1, bsz, off - (loff_t)bsz, &nwr));
	gbx_expect_err(gbx, gbx_sys_pwrite(fd, buf1, bsz, off, &nwr), -EFBIG);

	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects pwrite(3p) to return -ESPIPE if an attempt was made to write to a
 * file-descriptor which is associated with a pipe or FIFO.
 */
static void test_posix_write_espipe(gbx_env_t *gbx)
{
	int fd;
	char *path;
	void *buf1;
	size_t nwr = 0;
	size_t bsz = FNX_BLKSIZE;
	const loff_t off = (loff_t)FNX_REGSIZE_MAX;

	path = gbx_newpath(gbx);
	buf1 = gbx_newrbuf(gbx, bsz);
	gbx_expect_ok(gbx, gbx_sys_mkfifo(path, 0777));
	gbx_expect_ok(gbx, gbx_sys_open(path, O_RDWR, 0, &fd));
	gbx_expect_err(gbx, gbx_sys_pwrite(fd, buf1, bsz, off, &nwr), -ESPIPE);

	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_posix_write(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_posix_write_reg, GBX_POSIX),
		GBX_DEFTEST(test_posix_write_unlinked, GBX_POSIX),
		GBX_DEFTEST(test_posix_write_efbig, GBX_POSIX),
		GBX_DEFTEST(test_posix_write_espipe, GBX_POSIX),
	};
	gbx_execute(gbx, tests);
}


