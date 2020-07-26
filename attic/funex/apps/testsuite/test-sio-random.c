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
 * Tests read-write data-consistency for a sequence of IOs at pseudo random
 * offsets.
 */
static void test_sio_random_aux(gbx_env_t *gbx, loff_t from,
                                size_t len, size_t cnt, int rewrite)
{
	int fd, *pseq;
	loff_t pos;
	size_t i, j, nwr, nrd, nitr, seed;
	void *buf1, *buf2;
	char *path;

	nitr = rewrite ? 2 : 1;
	path = gbx_newpath1(gbx, gbx_genname(gbx));
	buf1 = gbx_newzbuf(gbx, len);
	buf2 = gbx_newzbuf(gbx, len);
	pseq = gbx_newzbuf(gbx, cnt * sizeof(*pseq));
	blgn_prandom_tseq(pseq, cnt, 0);

	gbx_ttrace(gbx, "from=%ld len=%lu cnt=%lu rewrite=%d",
	           from, len, cnt, rewrite);
	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0640, &fd));
	for (i = 0; i < nitr; ++i) {
		for (j = 0; j < cnt; ++j) {
			pos  = from + ((loff_t)len * pseq[j]);
			seed = i + j + (size_t)pos;
			gbx_fillsbuf(gbx, seed, buf1, len);

			gbx_expect_ok(gbx, gbx_sys_pwrite(fd, buf1, len, pos, &nwr));
			gbx_expect_eq(gbx, nwr, len);
		}
		for (j = 0; j < cnt; ++j) {
			pos  = from + ((loff_t)len * pseq[j]);
			seed = i + j + (size_t)pos;
			gbx_fillsbuf(gbx, seed, buf1, len);

			gbx_expect_ok(gbx, gbx_sys_pread(fd, buf2, len, pos, &nrd));
			gbx_expect_eq(gbx, nrd, len);
			gbx_expect_eqm(gbx, buf1, buf2, len);
		}
	}
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}


static void test_sio_random_io(gbx_env_t *gbx, loff_t from,
                               size_t len, size_t cnt)
{
	test_sio_random_aux(gbx, from, len, cnt, 0);
	test_sio_random_aux(gbx, from, len, cnt, 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_sio_random_aligned_blk(gbx_env_t *gbx, size_t cnt)
{
	loff_t from;
	const size_t len = FNX_BLKSIZE;

	from = 0;
	test_sio_random_io(gbx, from, len, cnt);
	from = (loff_t)FNX_BLKSIZE;
	test_sio_random_io(gbx, from, len, cnt);
	from = (loff_t)FNX_RSEGSIZE;
	test_sio_random_io(gbx, from, len, cnt);
	from = (loff_t)(FNX_RSEGSIZE - FNX_BLKSIZE);
	test_sio_random_io(gbx, from, len, cnt);
	from = (loff_t)FNX_RSECSIZE;
	test_sio_random_io(gbx, from, len, cnt);
	from = (loff_t)(FNX_RSECSIZE - FNX_BLKSIZE);
	test_sio_random_io(gbx, from, len, cnt);
	from = (loff_t)(FNX_RSECSIZE + FNX_BLKSIZE);
	test_sio_random_io(gbx, from, len, cnt);
	from = (loff_t)(FNX_RSECSIZE - (len * cnt));
	test_sio_random_io(gbx, from, len, cnt);
	from = (loff_t)((FNX_RSEGSIZE * FNX_REGNSEG) / 2);
	test_sio_random_io(gbx, from, len, cnt);
	from = (loff_t)(FNX_REGSIZE_MAX - (len * cnt));
	test_sio_random_io(gbx, from, len, cnt);
}


static void test_sio_random_aligned_blk1(gbx_env_t *gbx)
{
	test_sio_random_aligned_blk(gbx, 1);
}

static void test_sio_random_aligned_blk2(gbx_env_t *gbx)
{
	test_sio_random_aligned_blk(gbx, 2);
}

static void test_sio_random_aligned_blk63(gbx_env_t *gbx)
{
	fnx_staticassert(FNX_RSEGNBK > 63);
	test_sio_random_aligned_blk(gbx, 63);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_sio_random_aligned_rseg(gbx_env_t *gbx, size_t cnt)
{
	loff_t from;
	const size_t bsz = FNX_RSEGSIZE;

	from = 0;
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)FNX_BLKSIZE;
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)FNX_RSEGSIZE;
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)(FNX_RSEGSIZE - FNX_BLKSIZE);
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)FNX_RSECSIZE;
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)(FNX_RSECSIZE - FNX_BLKSIZE);
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)(FNX_RSECSIZE + FNX_BLKSIZE);
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)(FNX_RSECSIZE - FNX_RSEGSIZE);
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)(FNX_RSECSIZE - FNX_RSEGSIZE);
	test_sio_random_io(gbx, from, bsz, 2 * cnt);
	from = (loff_t)(2 * FNX_RSECSIZE);
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)((FNX_RSEGSIZE * FNX_REGNSEG) / 2);
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)(FNX_REGSIZE_MAX - (bsz * cnt));
	test_sio_random_io(gbx, from, bsz, cnt);
}

static void test_sio_random_aligned_rseg1(gbx_env_t *gbx)
{
	test_sio_random_aligned_rseg(gbx, 1);
}

static void test_sio_random_aligned_rseg2(gbx_env_t *gbx)
{
	test_sio_random_aligned_rseg(gbx, 2);
}

static void test_sio_random_aligned_rseg3(gbx_env_t *gbx)
{
	test_sio_random_aligned_rseg(gbx, 3);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_sio_random_unaligned_blk(gbx_env_t *gbx, size_t cnt)
{
	loff_t from;
	const size_t bsz = FNX_BLKSIZE;

	from = 1;
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)FNX_BLKSIZE - 11;
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)FNX_BLKSIZE + 11;
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)FNX_RSEGSIZE - 11;
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)(FNX_RSEGSIZE - FNX_BLKSIZE - 1);
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)FNX_RSECSIZE - 11;
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)(FNX_RSECSIZE - FNX_BLKSIZE - 1);
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)(FNX_RSECSIZE + FNX_BLKSIZE + 1);
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)(FNX_RSECSIZE - (bsz * cnt) + 1);
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)((FNX_RSEGSIZE * FNX_REGNSEG) / 11);
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)(FNX_REGSIZE_MAX - (bsz * cnt) - 11);
	test_sio_random_io(gbx, from, bsz, cnt);
}

static void test_sio_random_unaligned_blk1(gbx_env_t *gbx)
{
	test_sio_random_unaligned_blk(gbx, 1);
}

static void test_sio_random_unaligned_blk2(gbx_env_t *gbx)
{
	test_sio_random_unaligned_blk(gbx, 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_sio_random_unaligned_rseg(gbx_env_t *gbx, size_t cnt)
{
	loff_t from;
	const size_t bsz = FNX_RSEGSIZE;

	from = 1;
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)FNX_BLKSIZE - 11;
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)FNX_BLKSIZE + 11;
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)FNX_RSEGSIZE - 11;
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)(FNX_RSEGSIZE - FNX_BLKSIZE - 1);
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)FNX_RSECSIZE - 11;
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)(FNX_RSECSIZE - FNX_BLKSIZE - 1);
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)(FNX_RSECSIZE + FNX_BLKSIZE + 1);
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)(FNX_RSECSIZE - (bsz * cnt) + 1);
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)((FNX_RSEGSIZE * FNX_REGNSEG) / 11);
	test_sio_random_io(gbx, from, bsz, cnt);
	from = (loff_t)(FNX_REGSIZE_MAX - (bsz * cnt) - 11);
	test_sio_random_io(gbx, from, bsz, cnt);
}

static void test_sio_random_unaligned_rseg1(gbx_env_t *gbx)
{
	test_sio_random_unaligned_rseg(gbx, 1);
}

static void test_sio_random_unaligned_rseg2(gbx_env_t *gbx)
{
	test_sio_random_unaligned_rseg(gbx, 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
test_sio_random_unaligned_aux(gbx_env_t *gbx, size_t len, size_t cnt)
{
	loff_t from;

	from = 7;
	test_sio_random_io(gbx, from, len, cnt);
	from = (loff_t)FNX_BLKSIZE - 7;
	test_sio_random_io(gbx, from, len, cnt);
	from = (loff_t)FNX_BLKSIZE + 7;
	test_sio_random_io(gbx, from, len, cnt);
	from = (loff_t)FNX_RSEGSIZE - 7;
	test_sio_random_io(gbx, from, len, cnt);
	from = (loff_t)FNX_RSEGSIZE / 7;
	test_sio_random_io(gbx, from, len, cnt);
	from = (loff_t)FNX_RSECSIZE - 7;
	test_sio_random_io(gbx, from, len, cnt);
	from = (loff_t)FNX_RSECSIZE / 7;
	test_sio_random_io(gbx, from, len, cnt);
	from = (loff_t)(FNX_RSECSIZE + (len * cnt) - 7);
	test_sio_random_io(gbx, from, len, cnt);
	from = (loff_t)(((FNX_RSEGSIZE * FNX_REGNSEG) / 7) - 7);
	test_sio_random_io(gbx, from, len, cnt);
	from = (loff_t)(FNX_REGSIZE_MAX - (len * cnt) - 7);
	test_sio_random_io(gbx, from, len, cnt);
}

static void test_sio_random_unaligned_small(gbx_env_t *gbx)
{
	const size_t len = 7907;

	test_sio_random_unaligned_aux(gbx, len, 1);
	test_sio_random_unaligned_aux(gbx, len, 7);
	test_sio_random_unaligned_aux(gbx, len, 79);
	test_sio_random_unaligned_aux(gbx, len, 797);
}

static void test_sio_random_unaligned_large(gbx_env_t *gbx)
{
	const size_t len = 66601;

	test_sio_random_unaligned_aux(gbx, len, 1);
	test_sio_random_unaligned_aux(gbx, len, 61);
	test_sio_random_unaligned_aux(gbx, len, 661);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_sio_random(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_sio_random_aligned_blk1, GBX_SIO),
		GBX_DEFTEST(test_sio_random_aligned_blk2, GBX_SIO),
		GBX_DEFTEST(test_sio_random_aligned_blk63, GBX_SIO),
		GBX_DEFTEST(test_sio_random_aligned_rseg1, GBX_SIO),
		GBX_DEFTEST(test_sio_random_aligned_rseg2, GBX_SIO),
		GBX_DEFTEST(test_sio_random_aligned_rseg3, GBX_SIO),
		GBX_DEFTEST(test_sio_random_unaligned_blk1, GBX_SIO),
		GBX_DEFTEST(test_sio_random_unaligned_blk2, GBX_SIO),
		GBX_DEFTEST(test_sio_random_unaligned_rseg1, GBX_SIO),
		GBX_DEFTEST(test_sio_random_unaligned_rseg2, GBX_SIO),
		GBX_DEFTEST(test_sio_random_unaligned_small, GBX_SIO),
		GBX_DEFTEST(test_sio_random_unaligned_large, GBX_SIO),
	};
	gbx_execute(gbx, tests);
}

