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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "graybox.h"
#include "syscalls.h"
#include "test-hooks.h"

#ifndef FALLOC_FL_KEEP_SIZE
#define FALLOC_FL_KEEP_SIZE    0x01
#endif
#ifndef FALLOC_FL_PUNCH_HOLE
#define FALLOC_FL_PUNCH_HOLE   0x02
#endif



static const gbx_ioargs_t s_aligned_ioargs[] = {
	{ 0, 1, 0 },
	{ 0, FNX_BLKSIZE, 0 },
	{ 0, FNX_RSEGSIZE, 0 },
	{ FNX_BLKSIZE, FNX_BLKSIZE, 0 },
	{ FNX_BLKSIZE, 2 * FNX_BLKSIZE, 0 },
	{ FNX_BLKSIZE, FNX_RSEGSIZE, 0 },
	{ FNX_RSEGSIZE - FNX_BLKSIZE, FNX_BLKSIZE, 0 },
	{ FNX_RSEGSIZE, FNX_BLKSIZE, 0 },
	{ FNX_RSEGSIZE - FNX_BLKSIZE, 2 * FNX_BLKSIZE, 0 },
	{ FNX_RSECSIZE, FNX_BLKSIZE, 0 },
	{ FNX_RSECSIZE - FNX_BLKSIZE, 2 * FNX_BLKSIZE, 0 },
	{ FNX_RSECSIZE + FNX_BLKSIZE, FNX_BLKSIZE, 0 },
};

static const gbx_ioargs_t s_unaligned_ioargs[] = {
	{ 1, 2, 0 },
	{ 1, FNX_BLKSIZE - 2, 0 },
	{ 1, FNX_BLKSIZE + 2, 0 },
	{ 1, FNX_RSEGSIZE - 2, 0 },
	{ 1, FNX_RSEGSIZE + 2, 0 },
	{ FNX_BLKSIZE - 1, FNX_BLKSIZE + 2, 0 },
	{ FNX_RSEGSIZE - FNX_BLKSIZE + 1, 2 * FNX_BLKSIZE + 1, 0 },
	{ FNX_RSEGSIZE - 1, FNX_BLKSIZE + 11, 0 },
	{ FNX_RSEGSIZE - FNX_BLKSIZE - 1, 11 * FNX_BLKSIZE, 0 },
	{ FNX_RSECSIZE - 1, FNX_BLKSIZE + 2, 0 },
	{ FNX_RSECSIZE - FNX_BLKSIZE - 1, 2 * FNX_BLKSIZE + 2, 0 },
	{ FNX_RSECSIZE + FNX_BLKSIZE + 1, FNX_BLKSIZE - 1, 0 },
};


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects write to modify file's stat's size & blocks attributes properly.
 * Performs sequential write, followed by over-write on same region.
 */
static void test_sio_stat_write_aux(gbx_env_t *gbx, const gbx_ioargs_t *ioargs)
{
	int fd;
	size_t nwr;
	struct stat st;
	char *path;
	void *buf;
	const loff_t off = ioargs->off;
	const size_t bsz = ioargs->bsz;
	const blkcnt_t cnt = (blkcnt_t)fnx_range_to_nfrg(off, bsz);

	path = gbx_newpath1(gbx, gbx_genname(gbx));
	gbx_expect(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd), 0);
	gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st));
	gbx_expect_eq(gbx, st.st_size, 0);
	gbx_expect_eq(gbx, st.st_blocks, 0);

	buf = gbx_newrbuf(gbx, bsz);
	gbx_expect_ok(gbx, gbx_sys_pwrite(fd, buf, bsz, off, &nwr));
	gbx_expect_eq(gbx, bsz, nwr);
	gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st));
	gbx_expect_eq(gbx, st.st_size, off + (loff_t)bsz);
	gbx_expect_eq(gbx, st.st_blocks, cnt);

	buf = gbx_newrbuf(gbx, bsz);
	gbx_expect_ok(gbx, gbx_sys_pwrite(fd, buf, bsz, off, &nwr));
	gbx_expect_eq(gbx, bsz, nwr);
	gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st));
	gbx_expect_eq(gbx, st.st_size, off + (loff_t)bsz);
	gbx_expect_eq(gbx, st.st_blocks, cnt);

	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

static void test_sio_stat_write_aligned(gbx_env_t *gbx)
{
	for (size_t i = 0; i < fnx_nelems(s_aligned_ioargs); ++i) {
		test_sio_stat_write_aux(gbx, &s_aligned_ioargs[i]);
	}
}

static void test_sio_stat_write_unaligned(gbx_env_t *gbx)
{
	for (size_t i = 0; i < fnx_nelems(s_unaligned_ioargs); ++i) {
		test_sio_stat_write_aux(gbx, &s_unaligned_ioargs[i]);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Expects write-punch to modify file's stat's size & blocks attributes
 * properly. Performs sequential write, followed by fallocate-punch on same
 * data region.
 */
static void test_sio_stat_punch_aux(gbx_env_t *gbx, const gbx_ioargs_t *ioargs)
{
	int fd, mode;
	size_t nwr;
	struct stat st;
	char *path;
	void *buf;
	const loff_t off = ioargs->off;
	const size_t bsz = ioargs->bsz;
	const blkcnt_t cnt = (blkcnt_t)fnx_range_to_nfrg(off, bsz);

	path = gbx_newpath1(gbx, gbx_genname(gbx));
	gbx_expect(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd), 0);
	gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st));
	gbx_expect_eq(gbx, st.st_size, 0);
	gbx_expect_eq(gbx, st.st_blocks, 0);

	buf = gbx_newrbuf(gbx, bsz);
	gbx_expect_ok(gbx, gbx_sys_pwrite(fd, buf, bsz, off, &nwr));
	gbx_expect_eq(gbx, bsz, nwr);
	gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st));
	gbx_expect_eq(gbx, st.st_size, off + (loff_t)bsz);
	gbx_expect_eq(gbx, st.st_blocks, cnt);

	mode = FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE;
	gbx_expect_ok(gbx, gbx_sys_fallocate(fd, mode, off, (loff_t)bsz));
	gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st));
	gbx_expect_eq(gbx, st.st_size, off + (loff_t)bsz);
	gbx_expect_eq(gbx, st.st_blocks, 0);

	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

static void test_sio_stat_punch_aligned(gbx_env_t *gbx)
{
	for (size_t i = 0; i < fnx_nelems(s_aligned_ioargs); ++i) {
		test_sio_stat_punch_aux(gbx, &s_aligned_ioargs[i]);
	}
}

static void test_sio_stat_punch_unaligned(gbx_env_t *gbx)
{
	for (size_t i = 0; i < fnx_nelems(s_unaligned_ioargs); ++i) {
		test_sio_stat_punch_aux(gbx, &s_unaligned_ioargs[i]);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_sio_stat(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_sio_stat_write_aligned, GBX_SIO | GBX_CUSTOM),
		GBX_DEFTEST(test_sio_stat_write_unaligned, GBX_SIO | GBX_CUSTOM),
		GBX_DEFTEST(test_sio_stat_punch_aligned, GBX_SIO | GBX_CUSTOM),
		GBX_DEFTEST(test_sio_stat_punch_unaligned, GBX_SIO | GBX_CUSTOM),
	};
	gbx_execute(gbx, tests);
}
