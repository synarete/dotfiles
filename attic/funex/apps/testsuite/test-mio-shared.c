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
#include <unistd.h>

#include "graybox.h"
#include "syscalls.h"
#include "test-hooks.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test-helper for data-consistency when n-threads write-read same file at
 * different positions in parallel.
 */
static void test_mio_shared_aux1(gbx_sub_t *sub)
{
	int fd;
	loff_t pos, base;
	size_t i, nwr, nrd, bsz;
	void *buf1, *buf2;
	unsigned long seed;
	gbx_env_t *gbx   = sub->env;
	const char *path = sub->path;
	const size_t idx = sub->slot;
	const size_t nth = fnx_nelems(gbx->subs);
	const gbx_ioargs_t *ioargs = gbx->ioargs;

	bsz  = ioargs->bsz;
	buf1 = gbx_newzbuf(gbx, bsz);
	buf2 = gbx_newzbuf(gbx, bsz);
	gbx_expect_ok(gbx, gbx_sys_open(path, O_RDWR, 0600, &fd));
	for (i = 0; i < ioargs->cnt; ++i) {
		seed = (i | (idx << 32));
		base = (loff_t)(((i * nth) + idx) * bsz);
		pos  = ioargs->off + base;
		gbx_fillsbuf(gbx, seed, buf1, bsz);
		gbx_expect_ok(gbx, gbx_sys_pwrite(fd, buf1, bsz, pos, &nwr));
		gbx_expect_eq(gbx, bsz, nwr);
		gbx_expect_ok(gbx, gbx_sys_pread(fd, buf2, bsz, pos, &nrd));
		gbx_expect_eq(gbx, bsz, nrd);
		gbx_expect_eqm(gbx, buf1, buf2, bsz);
	}
	gbx_expect_ok(gbx, gbx_sys_close(fd));
}

static void test_mio_shared_aux2(gbx_sub_t *sub)
{
	int fd;
	loff_t pos, base;
	size_t i, nwr, nrd, bsz;
	void *buf1, *buf2;
	unsigned long seed;
	gbx_env_t *gbx   = sub->env;
	const char *path = sub->path;
	const size_t idx = sub->slot;
	const size_t nth = fnx_nelems(gbx->subs);
	const gbx_ioargs_t *ioargs = gbx->ioargs;

	bsz  = ioargs->bsz;
	buf1 = gbx_newzbuf(gbx, bsz);
	buf2 = gbx_newzbuf(gbx, bsz);

	gbx_expect_ok(gbx, gbx_sys_open(path, O_RDWR, 0600, &fd));
	for (i = 0; i < ioargs->cnt; ++i) {
		seed = (i | (idx << 32));
		base = (loff_t)(((i * nth) + idx) * bsz);
		pos  = ioargs->off + base;
		gbx_fillsbuf(gbx, seed, buf1, bsz);
		gbx_expect_ok(gbx, gbx_sys_pwrite(fd, buf1, bsz, pos, &nwr));
		gbx_expect_eq(gbx, bsz, nwr);
	}
	for (i = 0; i < ioargs->cnt; ++i) {
		seed = (i | (idx << 32));
		base = (loff_t)(((i * nth) + idx) * bsz);
		pos  = ioargs->off + base;
		gbx_fillsbuf(gbx, seed, buf1, bsz);
		gbx_expect_ok(gbx, gbx_sys_pread(fd, buf2, bsz, pos, &nrd));
		gbx_expect_eq(gbx, bsz, nrd);
		gbx_expect_eqm(gbx, buf1, buf2, bsz);
	}
	gbx_expect_ok(gbx, gbx_sys_close(fd));
}

static void test_mio_shared_aux(gbx_sub_t *sub)
{
	test_mio_shared_aux1(sub);
	test_mio_shared_aux2(sub);
}

/*
 * Test-helper: execute sub-test hook on n-threads in parallel which are
 * sharing the same data-file
 */
static void test_mio_shared_runsubs_aux(gbx_env_t *gbx,
                                        const gbx_ioargs_t *ioargs)
{
	size_t i, j;
	char *path;
	const size_t nth[] = { 1, 2, 7, 11, 17 };

	gbx->ioargs = ioargs;
	for (i = 0; i < fnx_nelems(nth); ++i) {
		path = gbx_newpath(gbx);
		gbx_expect_ok(gbx, gbx_sys_create2(path, 0600));
		for (j = 0; j < nth[i]; ++j) {
			gbx_start_sub(gbx, test_mio_shared_aux, j, path);
		}
		for (j = 0; j < nth[i]; ++j) {
			gbx_join_sub(gbx, j);
		}
		gbx_expect_ok(gbx, gbx_sys_unlink(path));
		gbx_gc(gbx);
	}
	gbx->ioargs = NULL;
}

static void test_mio_shared_runsubs(gbx_env_t *gbx,
                                    const gbx_ioargs_t *ioargs, size_t cnt)
{
	for (size_t i = 0; i < cnt; ++i) {
		test_mio_shared_runsubs_aux(gbx, &ioargs[i]);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests data-consistency when n-threads write-read to same file in parallel
 * at different positions.
 */
static void test_mio_shared_basic(gbx_env_t *gbx)
{
	const gbx_ioargs_t args[] = {
		{ 0, FNX_BLKSIZE, 1 },
		{ 0, FNX_BLKSIZE, 2 },
		{ 0, FNX_BLKSIZE, FNX_RSEGNBK / 2 },
		{ 0, FNX_BLKSIZE, FNX_RSEGNBK }
	};
	test_mio_shared_runsubs(gbx, args, fnx_nelems(args));
}

static void test_mio_shared_aligned(gbx_env_t *gbx)
{
	const gbx_ioargs_t args[] = {
		{ 0, FNX_BLKSIZE, 4 * FNX_RSEGNBK },
		{ 0, FNX_BLKSIZE, 4 },
		{ FNX_BLKSIZE, FNX_BLKSIZE, FNX_RSEGNBK },
		{ FNX_RSEGNBK, FNX_BLKSIZE, 4 },
		{ FNX_RSECNBK, FNX_BLKSIZE, FNX_RSEGNBK },
	};
	test_mio_shared_runsubs(gbx, args, fnx_nelems(args));
}

static void test_mio_shared_unaligned1(gbx_env_t *gbx)
{
	const gbx_ioargs_t args[] = {
		{ 0, FNX_BLKSIZE - 1, 1 },
		{ 0, FNX_BLKSIZE - 1, FNX_RSEGNBK },
		{ 0, FNX_BLKSIZE - 1, FNX_RSEGNBK + 1 },
		{ 0, FNX_BLKSIZE - 1, 2 * FNX_RSEGNBK }
	};
	test_mio_shared_runsubs(gbx, args, fnx_nelems(args));
}

static void test_mio_shared_unaligned2(gbx_env_t *gbx)
{
	const gbx_ioargs_t args[] = {
		{ 1, FNX_BLKSIZE - 1, 1 },
		{ FNX_RSEGSIZE - 1, FNX_BLKSIZE - 1, FNX_RSEGNBK },
		{ FNX_RSECSIZE - 1, FNX_BLKSIZE - 1, FNX_RSEGNBK + 1 },
		{ FNX_RSECSIZE - 1, FNX_RSEGSIZE - 1, 2 }
	};
	test_mio_shared_runsubs(gbx, args, fnx_nelems(args));
}

static void test_mio_shared_unaligned3(gbx_env_t *gbx)
{
	const gbx_ioargs_t args[] = {
		{ FNX_RSECSIZE + 1, FNX_BLKSIZE - 1, FNX_RSEGNBK },
		{ (2 * FNX_RSECSIZE) + 1, FNX_BLKSIZE + 1, FNX_RSEGNBK },
		{ (5 * FNX_RSECSIZE) + 7, FNX_RSEGSIZE + 1, 3 },
	};
	test_mio_shared_runsubs(gbx, args, fnx_nelems(args));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_mio_shared(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_mio_shared_basic,      GBX_MIO),
		GBX_DEFTEST(test_mio_shared_aligned,    GBX_MIO),
		GBX_DEFTEST(test_mio_shared_unaligned1, GBX_MIO),
		GBX_DEFTEST(test_mio_shared_unaligned2, GBX_MIO),
		GBX_DEFTEST(test_mio_shared_unaligned3, GBX_MIO),
	};
	gbx_execute(gbx, tests);
}
