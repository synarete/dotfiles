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
 * Test-helper for data-consistency when n-threads write-read different files
 * in parallel.
 */
static void test_mio_unlinked_aux1(gbx_sub_t *sub)
{
	int fd;
	loff_t pos, base;
	size_t i, nwr, nrd, bsz;
	void *buf1, *buf2;
	unsigned long seed;
	gbx_env_t *gbx   = sub->env;
	const size_t idx = sub->slot;
	const gbx_ioargs_t *ioargs = gbx->ioargs;

	fd   = sub->fd;
	bsz  = ioargs->bsz;
	buf1 = gbx_newzbuf(gbx, bsz);
	buf2 = gbx_newzbuf(gbx, bsz);
	for (i = 0; i < ioargs->cnt; ++i) {
		seed = (i | (idx << 32));
		base = (loff_t)(i * bsz);
		pos  = ioargs->off + base;
		gbx_fillsbuf(gbx, seed, buf1, bsz);
		gbx_expect_ok(gbx, gbx_sys_pwrite(fd, buf1, bsz, pos, &nwr));
		gbx_expect_eq(gbx, bsz, nwr);
		gbx_expect_ok(gbx, gbx_sys_pread(fd, buf2, bsz, pos, &nrd));
		gbx_expect_eq(gbx, bsz, nrd);
		gbx_expect_eqm(gbx, buf1, buf2, bsz);
	}
}

static void test_mio_unlinked_aux2(gbx_sub_t *sub)
{
	int fd;
	loff_t pos, base;
	size_t i, nwr, nrd, bsz;
	void *buf1, *buf2;
	unsigned long seed;
	gbx_env_t *gbx   = sub->env;
	const size_t idx = sub->slot;
	const gbx_ioargs_t *ioargs = gbx->ioargs;

	fd   = sub->fd;
	bsz  = ioargs->bsz;
	buf1 = gbx_newzbuf(gbx, bsz);
	buf2 = gbx_newzbuf(gbx, bsz);

	for (i = 0; i < ioargs->cnt; ++i) {
		seed = (i | (idx << 32));
		base = (loff_t)(i * bsz);
		pos  = ioargs->off + base;
		gbx_fillsbuf(gbx, seed, buf1, bsz);
		gbx_expect_ok(gbx, gbx_sys_pwrite(fd, buf1, bsz, pos, &nwr));
		gbx_expect_eq(gbx, bsz, nwr);
	}
	for (i = 0; i < ioargs->cnt; ++i) {
		seed = (i | (idx << 32));
		base = (loff_t)(i * bsz);
		pos  = ioargs->off + base;
		gbx_fillsbuf(gbx, seed, buf1, bsz);
		gbx_expect_ok(gbx, gbx_sys_pread(fd, buf2, bsz, pos, &nrd));
		gbx_expect_eq(gbx, bsz, nrd);
		gbx_expect_eqm(gbx, buf1, buf2, bsz);
	}
}

static void test_mio_unlinked_aux(gbx_sub_t *sub)
{
	test_mio_unlinked_aux1(sub);
	test_mio_unlinked_aux2(sub);
}

/*
 * Test-helper: execute sub-test hook on n-threads in parallel where each sub
 * thread has its own unique data-file
 */
static void test_mio_unlinked_runsubs_aux(gbx_env_t *gbx,
        const gbx_ioargs_t *ioargs)
{
	size_t i, j;
	char *dpath, *rpath;
	const size_t nth[] = { 1, 2, 7, 11, 17 };
	int fd[fnx_nelems(gbx->subs)];
	const int flags = O_CREAT | O_RDWR;

	gbx->ioargs = ioargs;
	for (i = 0; i < fnx_nelems(nth); ++i) {
		dpath = gbx_newpath(gbx);
		gbx_expect_ok(gbx, gbx_sys_mkdir(dpath, 0700));
		for (j = 0; j < nth[i]; ++j) {
			rpath = gbx_newpathf(gbx, dpath, "%lu", j);
			gbx_expect_ok(gbx, gbx_sys_open(rpath, flags, 0600, &fd[j]));
			gbx_expect_ok(gbx, gbx_sys_unlink(rpath));
			gbx_start_fsub(gbx, test_mio_unlinked_aux, j, fd[j]);
		}
		for (j = 0; j < nth[i]; ++j) {
			gbx_join_sub(gbx, j);
			gbx_expect_ok(gbx, gbx_sys_close(fd[j]));
		}
		gbx_expect_ok(gbx, gbx_sys_rmdir(dpath));
		gbx_gc(gbx);
	}
	gbx->ioargs = NULL;
}

static void test_mio_unlinked_runsubs(gbx_env_t *gbx,
                                      const gbx_ioargs_t *ioargs, size_t cnt)
{
	for (size_t i = 0; i < cnt; ++i) {
		test_mio_unlinked_runsubs_aux(gbx, &ioargs[i]);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests data-consistency when n-threads write-read to multiple different
 * files in parallel.
 */
static void test_mio_unlinked_basic(gbx_env_t *gbx)
{
	const gbx_ioargs_t args[] = {
		{ 0, FNX_BLKSIZE, 1 },
		{ 0, FNX_BLKSIZE, 2 },
		{ 0, FNX_BLKSIZE, FNX_RSEGNBK / 2 },
		{ 0, FNX_BLKSIZE, FNX_RSEGNBK }
	};
	test_mio_unlinked_runsubs(gbx, args, fnx_nelems(args));
}

static void test_mio_unlinked_aligned(gbx_env_t *gbx)
{
	const gbx_ioargs_t args[] = {
		{ FNX_BLKSIZE, FNX_BLKSIZE, FNX_RSEGNBK },
		{ FNX_RSEGSIZE - FNX_BLKSIZE, FNX_BLKSIZE, FNX_RSEGNBK },
		{ FNX_RSEGNBK, FNX_BLKSIZE, 4 },
		{ FNX_RSECNBK, FNX_BLKSIZE, FNX_RSEGNBK },
	};
	test_mio_unlinked_runsubs(gbx, args, fnx_nelems(args));
}

static void test_mio_unlinked_unaligned1(gbx_env_t *gbx)
{
	const gbx_ioargs_t args[] = {
		{ 0, FNX_BLKSIZE - 1, 1 },
		{ 0, FNX_BLKSIZE - 1, FNX_RSEGNBK },
		{ 0, FNX_BLKSIZE - 1, FNX_RSEGNBK + 1 },
		{ 0, FNX_BLKSIZE - 1, 2 * FNX_RSEGNBK }
	};
	test_mio_unlinked_runsubs(gbx, args, fnx_nelems(args));
}

static void test_mio_unlinked_unaligned2(gbx_env_t *gbx)
{
	const gbx_ioargs_t args[] = {
		{ 1, FNX_BLKSIZE - 1, 1 },
		{ FNX_RSEGSIZE - 1, FNX_BLKSIZE - 1, FNX_RSEGNBK },
		{ FNX_RSECSIZE - 1, FNX_BLKSIZE - 1, FNX_RSEGNBK + 1 },
		{ FNX_RSECSIZE - 1, FNX_RSEGSIZE - 1, 2 }
	};
	test_mio_unlinked_runsubs(gbx, args, fnx_nelems(args));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_mio_unlinked(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_mio_unlinked_basic,      GBX_MIO),
		GBX_DEFTEST(test_mio_unlinked_aligned,    GBX_MIO),
		GBX_DEFTEST(test_mio_unlinked_unaligned1, GBX_MIO),
		GBX_DEFTEST(test_mio_unlinked_unaligned2, GBX_MIO),
	};
	gbx_execute(gbx, tests);
}

