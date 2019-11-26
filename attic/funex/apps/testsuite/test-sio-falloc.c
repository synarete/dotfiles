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

#if !defined(FALLOC_FL_PUNCH_HOLE)
#include <linux/falloc.h>
#endif

#include "graybox.h"
#include "syscalls.h"
#include "test-hooks.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects fallocate(2) to successfully allocate space, and return EBADF if
 * fd is not opened for writing.
 */
static void test_sio_falloc_basic(gbx_env_t *gbx)
{
	int fd;
	loff_t len = FNX_BLKSIZE;
	char *path;

	path = gbx_newpath1(gbx, gbx_genname(gbx));
	gbx_expect_ok(gbx, gbx_sys_create(path, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_fallocate(fd, 0, 0, len));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_open(path, O_RDONLY, 0, &fd));
	gbx_expect_err(gbx, gbx_sys_fallocate(fd, 0, len, 2 * len), -EBADF);
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects fallocate(2) to successfully allocate space for file's sub-ranges.
 */
static void test_sio_falloc_aux(gbx_env_t *gbx, const gbx_ioargs_t *ioargs)
{
	int fd, mode = 0;
	char *path;
	const loff_t off = ioargs->off;
	const loff_t len = (loff_t)ioargs->bsz;

	path = gbx_newpath1(gbx, gbx_genname(gbx));
	gbx_expect_ok(gbx, gbx_sys_create(path, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_fallocate(fd, mode, off, len));
	gbx_expect_ok(gbx, gbx_sys_ftruncate(fd, 0));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

static void test_sio_falloc_aligned0(gbx_env_t *gbx)
{
	const gbx_ioargs_t args[] = {
		{FNX_RSEGSIZE, 1, 0},
	};
	for (size_t i = 0; i < fnx_nelems(args); ++i) {
		test_sio_falloc_aux(gbx, &args[i]);
	}
}

static void test_sio_falloc_aligned(gbx_env_t *gbx)
{
	const gbx_ioargs_t args[] = {
		{0, FNX_BLKSIZE, 0},
		{FNX_BLKSIZE, FNX_BLKSIZE, 0},

		{FNX_BLKSIZE, FNX_RSEGSIZE, 0},
		{0, FNX_RSECSIZE, 0},
	};
	for (size_t i = 0; i < fnx_nelems(args); ++i) {
		test_sio_falloc_aux(gbx, &args[i]);
	}
}

static void test_sio_falloc_unaligned1(gbx_env_t *gbx)
{
	const gbx_ioargs_t args[] = {
		{0, 1, 0},
		{0, FNX_BLKSIZE - 1, 0},
		{FNX_BLKSIZE, FNX_BLKSIZE - 1, 0},
		{0, FNX_RSEGSIZE - 1, 0},
		{FNX_BLKSIZE, FNX_RSEGSIZE - 1, 0},
		{0, FNX_RSECSIZE - 1, 0},
	};
	for (size_t i = 0; i < fnx_nelems(args); ++i) {
		test_sio_falloc_aux(gbx, &args[i]);
	}
}

static void test_sio_falloc_unaligned2(gbx_env_t *gbx)
{
	const gbx_ioargs_t args[] = {
		{1, FNX_BLKSIZE + 3, 0},
		{FNX_BLKSIZE - 1, FNX_BLKSIZE + 3, 0},
		{1, FNX_RSEGSIZE - 2, 0},
		{FNX_BLKSIZE + 1, FNX_RSEGSIZE + 1, 0},
		{1, FNX_RSECSIZE + 3, 0},
	};
	for (size_t i = 0; i < fnx_nelems(args); ++i) {
		test_sio_falloc_aux(gbx, &args[i]);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests data-consistency of write-read-trim, segments aligned
 */
static void test_sio_falloc_rdwrtrim(gbx_env_t *gbx)
{
	int fd, mode;
	loff_t pos = -1;
	size_t i, nwr, nrd;
	const size_t bsz = FNX_RSEGSIZE;
	const size_t cnt = FNX_RSECNSEG;
	void *buf1, *buf2;
	char *path;

	mode = FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE;
	path = gbx_newpath1(gbx, gbx_genname(gbx));
	buf1 = gbx_newzbuf(gbx, bsz);
	buf2 = gbx_newzbuf(gbx, bsz);

	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd));
	for (i = 0; i < cnt; ++i) {
		gbx_fillsbuf(gbx, i, buf1, bsz);
		gbx_expect_ok(gbx, gbx_sys_write(fd, buf1, bsz, &nwr));
		gbx_expect_eq(gbx, nwr, bsz);
		gbx_expect_ok(gbx, gbx_sys_lseek(fd, -((off_t)bsz), SEEK_CUR, &pos));
		gbx_expect_ok(gbx, gbx_sys_read(fd, buf2, bsz, &nrd));
		gbx_expect_eq(gbx, nrd, bsz);
		gbx_expect_eqm(gbx, buf1, buf2, bsz);
		gbx_expect_ok(gbx, gbx_sys_fallocate(fd, mode, pos, (loff_t)bsz));
	}

	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_sio_falloc(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_sio_falloc_aligned0, GBX_SIO),
		GBX_DEFTEST(test_sio_falloc_basic, GBX_SIO),
		GBX_DEFTEST(test_sio_falloc_aligned, GBX_SIO),
		GBX_DEFTEST(test_sio_falloc_unaligned1, GBX_SIO),
		GBX_DEFTEST(test_sio_falloc_unaligned2, GBX_SIO),
		GBX_DEFTEST(test_sio_falloc_rdwrtrim, GBX_SIO)
	};
	gbx_execute(gbx, tests);
}


