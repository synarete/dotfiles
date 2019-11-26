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
 * Expects read-write data-consistency, sequential writes of single block.
 */
static void test_sio_basic_simple(gbx_env_t *gbx)
{
	int fd;
	loff_t pos = -1;
	size_t i, n, bsz;
	size_t nwr, nrd;
	struct stat st;
	char *path;
	void *buf;

	bsz  = FNX_BLKSIZE;
	buf  = gbx_newzbuf(gbx, bsz);
	path = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_expect(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0644, &fd), 0);
	for (i = 0; i < FNX_RSEGNBK; ++i) {
		n = i;
		memcpy(buf, &n, sizeof(n));
		gbx_expect_ok(gbx, gbx_sys_write(fd, buf, bsz, &nwr));
		gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st));
		gbx_expect(gbx, (long)st.st_size, (long)((i + 1) * bsz));
	}
	gbx_expect_ok(gbx, gbx_sys_lseek(fd, 0, SEEK_SET, &pos));
	for (i = 0; i < FNX_RSEGNBK; ++i) {
		gbx_expect_ok(gbx, gbx_sys_read(fd, buf, bsz, &nrd));
		memcpy(&n, buf, sizeof(n));
		gbx_expect(gbx, (long)i, (long)n);
	}
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects read-write data-consistency for buffer-size of 1M.
 */
static void test_sio_buffer(gbx_env_t *gbx, size_t bsz)
{
	int fd;
	size_t i, n;
	void *buf1, *buf2;
	char *path;
	struct stat st;

	buf1 = gbx_newzbuf(gbx, bsz);
	buf2 = gbx_newzbuf(gbx, bsz);
	path = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd));
	for (i = 0; i < 5; ++i) {
		gbx_fillrbuf(gbx, buf1, bsz);
		gbx_expect_ok(gbx, gbx_sys_pwrite(fd, buf1, bsz, 0, &n));
		gbx_expect_ok(gbx, gbx_sys_fsync(fd));
		gbx_expect_ok(gbx, gbx_sys_pread(fd, buf2, bsz, 0, &n));
		gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st));
		gbx_expect_eq(gbx, st.st_size, bsz);
		gbx_expect_eqm(gbx, buf1, buf2, bsz);
	}

	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

static void test_sio_basic_rdwr_1k(gbx_env_t *gbx)
{
	test_sio_buffer(gbx, 1024);
}

static void test_sio_basic_rdwr_8k(gbx_env_t *gbx)
{
	test_sio_buffer(gbx, 8 * 1024);
}

static void test_sio_basic_rdwr_1m(gbx_env_t *gbx)
{
	test_sio_buffer(gbx, 1024 * 1024);
}

static void test_sio_basic_rdwr_8m(gbx_env_t *gbx)
{
	test_sio_buffer(gbx, 8 * 1024 * 1024);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Should not get ENOSPC for sequence of write-read-unlink of large buffer.
 */
static void test_sio_basic_space(gbx_env_t *gbx)
{
	int fd;
	loff_t off;
	size_t cnt, bsz = FNX_RSEGSIZE;
	void *buf1, *buf2;
	char *path;

	buf1  = gbx_newzbuf(gbx, bsz);
	buf2  = gbx_newzbuf(gbx, bsz);
	for (size_t i = 0; i < 256; ++i) {
		off  = (loff_t)i;
		path = gbx_newpath1(gbx, gbx_genname(gbx));
		gbx_fillrbuf(gbx, buf1, bsz);
		gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd));
		gbx_expect_ok(gbx, gbx_sys_pwrite(fd, buf1, bsz, off, &cnt));
		gbx_expect_ok(gbx, gbx_sys_pread(fd, buf2, bsz, off, &cnt));
		gbx_expect_eq(gbx, memcmp(buf1, buf2, bsz), 0);
		gbx_expect_ok(gbx, gbx_sys_close(fd));
		gbx_expect_ok(gbx, gbx_sys_unlink(path));
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects read-write data-consistency, reverse writes.
 */
static void test_sio_basic_reserve_at(gbx_env_t *gbx, loff_t off, size_t ssz)
{
	int fd;
	loff_t pos = -1;
	size_t i, nwr, nrd;
	char *path;
	uint8_t buf[1];

	path = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0644, &fd));
	for (i = 0; i < ssz; ++i) {
		buf[0] = (uint8_t)i;
		pos = off + (loff_t)(ssz - i - 1);
		gbx_expect_ok(gbx, gbx_sys_pwrite(fd, buf, 1, pos, &nwr));
		gbx_expect_eq(gbx, nwr, 1);
	}
	for (i = 0; i < ssz; ++i) {
		pos = off + (loff_t)(ssz - i - 1);
		gbx_expect_ok(gbx, gbx_sys_pread(fd, buf, 1, pos, &nrd));
		gbx_expect_eq(gbx, nrd, 1);
		gbx_expect_eq(gbx, buf[0], (uint8_t)i);
	}
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

static void test_sio_basic_reserve_blk(gbx_env_t *gbx)
{
	test_sio_basic_reserve_at(gbx, 0, FNX_BLKSIZE);
}

static void test_sio_basic_reserve_blk2(gbx_env_t *gbx)
{
	test_sio_basic_reserve_at(gbx, FNX_RSEGSIZE - FNX_BLKSIZE, 2 * FNX_BLKSIZE);
}

static void test_sio_basic_reserve_off(gbx_env_t *gbx)
{
	test_sio_basic_reserve_at(gbx, FNX_RSEGSIZE - 1, FNX_BLKSIZE - 1);
}

static void test_sio_basic_reserve_off2(gbx_env_t *gbx)
{
	test_sio_basic_reserve_at(gbx, (127 * FNX_RSEGSIZE) - 127, 7 * FNX_BLKSIZE);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects read-write data-consistency when I/O overlaps
 */
static void test_sio_basic_overlap(gbx_env_t *gbx)
{
	int fd;
	loff_t off;
	size_t n, cnt, bsz = 2 * FNX_BCKTSIZE;
	void *buf1, *buf2, *buf3;
	char *path;

	buf1 = gbx_newzbuf(gbx, bsz);
	buf2 = gbx_newzbuf(gbx, bsz);
	buf3 = gbx_newzbuf(gbx, bsz);
	path = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_fillrbuf(gbx, buf1, bsz);
	gbx_fillrbuf(gbx, buf2, bsz);

	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_pwrite(fd, buf1, bsz, 0, &n));
	gbx_expect_ok(gbx, gbx_sys_pread(fd, buf3, bsz, 0, &n));
	gbx_expect_eqm(gbx, buf1, buf3, bsz);

	off = 17;
	cnt = 100;
	gbx_expect_ok(gbx, gbx_sys_pwrite(fd, buf2, cnt, off, &n));
	gbx_expect_ok(gbx, gbx_sys_pread(fd, buf3, cnt, off, &n));
	gbx_expect_eqm(gbx, buf2, buf3, cnt);

	off = 2099;
	cnt = 1000;
	gbx_expect_ok(gbx, gbx_sys_pwrite(fd, buf2, cnt, off, &n));
	gbx_expect_ok(gbx, gbx_sys_pread(fd, buf3, cnt, off, &n));
	gbx_expect_eqm(gbx, buf2, buf3, cnt);

	off = 32077;
	cnt = 10000;
	gbx_expect_ok(gbx, gbx_sys_pwrite(fd, buf2, cnt, off, &n));
	gbx_expect_ok(gbx, gbx_sys_pread(fd, buf3, cnt, off, &n));
	gbx_expect_eqm(gbx, buf2, buf3, cnt);

	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects read-write data-consistency when I/O in complex patterns
 */
static void test_sio_basic_aux(gbx_env_t *gbx,
                               loff_t pos, loff_t lim, loff_t step)
{
	int fd;
	loff_t off;
	size_t nwr, bsz = FNX_BLKSIZE;
	char *buf1, *buf2;
	char *path;

	path = gbx_newpath(gbx);
	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd));
	for (off = pos; off < lim; off += step) {
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


static void test_sio_basic_aligned(gbx_env_t *gbx)
{
	loff_t pos, lim, step = FNX_BLKSIZE;

	pos = 0;
	lim = FNX_RSEGSIZE;
	test_sio_basic_aux(gbx, pos, lim, step);
	lim = 2 * FNX_RSEGSIZE;
	test_sio_basic_aux(gbx, pos, lim, step);
	pos = FNX_RSECSIZE - FNX_RSEGSIZE;
	lim = FNX_RSECSIZE;
	test_sio_basic_aux(gbx, pos, lim, step);
}

static void test_sio_basic_unaligned(gbx_env_t *gbx)
{
	loff_t pos, lim;
	const loff_t step1 = FNX_BLKSIZE + 1;
	const loff_t step2 = FNX_BLKSIZE - 1;

	pos = 0;
	lim = FNX_RSEGSIZE;
	test_sio_basic_aux(gbx, pos, lim, step1);
	test_sio_basic_aux(gbx, pos, lim, step2);

	lim = 2 * FNX_RSEGSIZE;
	test_sio_basic_aux(gbx, pos, lim, step1);
	test_sio_basic_aux(gbx, pos, lim, step2);

	pos = FNX_RSECSIZE - FNX_RSEGSIZE;
	lim = FNX_RSECSIZE;
	test_sio_basic_aux(gbx, pos, lim, step1);
	test_sio_basic_aux(gbx, pos, lim, step2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_sio_basic(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_sio_basic_simple, GBX_SIO),
		GBX_DEFTEST(test_sio_basic_rdwr_1k, GBX_SIO),
		GBX_DEFTEST(test_sio_basic_rdwr_8k, GBX_SIO),
		GBX_DEFTEST(test_sio_basic_rdwr_1m, GBX_SIO),
		GBX_DEFTEST(test_sio_basic_rdwr_8m, GBX_SIO),
		GBX_DEFTEST(test_sio_basic_space, GBX_SIO),
		GBX_DEFTEST(test_sio_basic_reserve_blk, GBX_SIO),
		GBX_DEFTEST(test_sio_basic_reserve_blk2, GBX_SIO),
		GBX_DEFTEST(test_sio_basic_reserve_off, GBX_SIO),
		GBX_DEFTEST(test_sio_basic_reserve_off2, GBX_SIO),
		GBX_DEFTEST(test_sio_basic_overlap,  GBX_SIO),
		GBX_DEFTEST(test_sio_basic_aligned,  GBX_SIO),
		GBX_DEFTEST(test_sio_basic_unaligned,  GBX_SIO),
	};
	gbx_execute(gbx, tests);
}


