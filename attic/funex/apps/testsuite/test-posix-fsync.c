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
 * Expects fsync(3p) to return 0 after regular file write/read operation.
 */
static void test_posix_fsync_reg(gbx_env_t *gbx, loff_t step)
{
	int fd;
	loff_t off, lim = (loff_t)FNX_BCKTSIZE;
	size_t nwr, bsz = FNX_BLKSIZE;
	char *buf1, *buf2;
	char *path;

	path = gbx_newpath(gbx);
	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd));
	for (off = 0; off < lim; off += step) {
		buf1 = gbx_newrbuf(gbx, bsz);
		buf2 = gbx_newrbuf(gbx, bsz);
		gbx_expect_ok(gbx, gbx_sys_pwrite(fd, buf1, bsz, off, &nwr));
		gbx_expect_ok(gbx, gbx_sys_fsync(fd));
		gbx_expect_ok(gbx, gbx_sys_pread(fd, buf2, bsz, off, &nwr));
		gbx_expect_ok(gbx, gbx_sys_fsync(fd));
		gbx_expect_eqm(gbx, buf1, buf2, bsz);
	}
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}


static void test_posix_fsync_reg_aligned(gbx_env_t *gbx)
{
	test_posix_fsync_reg(gbx, (loff_t)FNX_BLKSIZE);
}

static void test_posix_fsync_reg_unaligned(gbx_env_t *gbx)
{
	test_posix_fsync_reg(gbx, (loff_t)(FNX_BLKSIZE + 1));
	test_posix_fsync_reg(gbx, (loff_t)(FNX_BLKSIZE - 1));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects fsync(3p) to return 0 after directory operations.
 */
static void test_posix_fsync_dir_nent(gbx_env_t *gbx, int cnt)
{
	int i, fd, fd2;
	char *path;
	char *path1;

	path = gbx_newpath(gbx);
	gbx_expect_ok(gbx, gbx_sys_mkdir(path, 0700));
	gbx_expect_ok(gbx, gbx_sys_open(path, O_DIRECTORY | O_RDONLY, 0, &fd));
	for (i = 0; i < cnt; ++i) {
		path1 = gbx_newpathf(gbx, path, "%08x", i + 1);
		gbx_expect_ok(gbx, gbx_sys_create(path1, 0640, &fd2));
		gbx_expect_ok(gbx, gbx_sys_fsync(fd));
		gbx_expect_ok(gbx, gbx_sys_close(fd2));
		gbx_expect_ok(gbx, gbx_sys_fsync(fd));
	}
	for (i = 0; i < cnt; ++i) {
		path1 = gbx_newpathf(gbx, path, "%08x", i + 1);
		gbx_expect_ok(gbx, gbx_sys_unlink(path1));
		gbx_expect_ok(gbx, gbx_sys_fsync(fd));
	}
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path));
}

static void test_posix_fsync_dir(gbx_env_t *gbx)
{
	test_posix_fsync_dir_nent(gbx, FNX_DIRHNDENT);
	test_posix_fsync_dir_nent(gbx, FNX_DIRHNDENT + 3);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects syncfs(2) to return 0
 */
static void test_posix_fsync_syncfs(gbx_env_t *gbx)
{
	int fd;
	char *path;

	path = gbx_newpath(gbx);
	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_syncfs(fd));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_posix_fsync(gbx_env_t *gbx)
{
	static const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_posix_fsync_reg_aligned, GBX_POSIX),
		GBX_DEFTEST(test_posix_fsync_reg_unaligned, GBX_POSIX),
		GBX_DEFTEST(test_posix_fsync_dir, GBX_POSIX),
		GBX_DEFTEST(test_posix_fsync_syncfs, GBX_CUSTOM),
	};
	gbx_execute(gbx, tests);
}



