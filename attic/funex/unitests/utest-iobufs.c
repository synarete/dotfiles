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
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/uio.h>

#include <fnxinfra.h>
#include <fnxcore.h>


#define MARK        '@'
#define SEGNBKS     FNX_RSEGNBK
#define BLKSIZE     FNX_BLKSIZE
#define SEGSIZE     (BLKSIZE*SEGNBKS)
#define BKSZ        BLKSIZE
#define SGSZ        SEGSIZE
#define SGNB        SEGNBKS


struct tparam {
	fnx_bool_t  check;
	fnx_off_t   off;
	fnx_size_t  len;
	fnx_size_t  idx;
	fnx_bkcnt_t cnt;
};
typedef struct tparam tparam_t;

struct tparams_pair {
	tparam_t first, second;
};
typedef struct tparams_pair tparams_t;


static fnx_alloc_t s_alloc;

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_iobufs_t *new_iobufs(void)
{
	fnx_iobufs_t *iobufs;

	iobufs = (fnx_iobufs_t *)fnx_xmalloc(sizeof(*iobufs), FNX_NOFAIL);
	fnx_iobufs_init(iobufs);
	fnx_iobufs_setup(iobufs, FNX_INO_NULL, &s_alloc);

	return iobufs;
}

static void del_iobufs(fnx_iobufs_t *iobufs)
{
	fnx_iobufs_release(iobufs);
	fnx_iobufs_destroy(iobufs);

	fnx_xfree(iobufs, sizeof(*iobufs), FNX_BZERO);
}

static void *new_data(size_t datsz)
{
	char *dat;

	dat   = (char *)fnx_xmalloc(datsz, FNX_NOFAIL | FNX_BZERO);
	memset(dat, MARK, datsz);

	return dat;
}

static void del_data(void *dat, size_t datsz)
{
	fnx_free(dat, datsz);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
check_iobuf(const fnx_iobuf_t *iobuf, const tparam_t *prm, const char *dat)
{
	int cmp;
	fnx_off_t off, beg, end, off2, soff;
	fnx_size_t len, bkidx, bkcnt, sz, len2, slen;
	const char *dat2;
	const fnx_lrange_t *lrange;
	const fnx_bkref_t *bkref;

	if (!prm->check) {
		return;
	}
	off = prm->off;
	len = prm->len;
	bkidx = prm->idx;
	bkcnt = prm->cnt;
	lrange = &iobuf->rng;

	fnx_assert(lrange->idx == bkidx);
	fnx_assert(lrange->cnt == bkcnt);
	if (bkcnt > 0) {
		fnx_assert(lrange->off == off);
	}

	beg = lrange->off;
	end = fnx_lrange_end(lrange);
	sz = (fnx_size_t)(end - beg);
	fnx_assert(len == sz);

	for (size_t pos, i = 0; i < prm->cnt; ++i) {
		pos = lrange->idx + i;
		bkref = iobuf->bks[pos];
		fnx_assert(bkref != NULL);
		fnx_assert(bkref->bk_blk != NULL);

		soff  = off % FNX_BLKSIZE;
		off2  = fnx_off_floor_blk(off + FNX_BLKSIZE);
		len2  = (fnx_size_t)(off2 - off);
		slen  = fnx_min(len, len2);
		dat2 = (const char *)(bkref->bk_blk) + soff;

		if (slen > 0) {
			fnx_assert(dat[0] == MARK);
		}

		fnx_assert(off2 >= off);
		cmp  = memcmp(dat, dat2, slen);
		fnx_assert(cmp == 0);

		dat += slen;
		off += (fnx_off_t)slen;
		len -= slen;
	}
}

static void
check_iobufs(const fnx_iobufs_t *iobufs,
             const tparams_t *params, const char *dat)
{
	check_iobuf(&iobufs->iob[0], &params->first, dat);
	check_iobuf(&iobufs->iob[1], &params->second, dat + params->first.len);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
test_assign(fnx_iobufs_t *iobufs,
            const tparams_t *params, const char *dat)
{
	int rc;
	fnx_off_t off;
	fnx_size_t len;

	off = params->first.off;
	len = params->first.len + params->second.len;
	rc = fnx_iobufs_assign(iobufs, off, len, dat);
	fnx_assert(rc == 0);
	check_iobufs(iobufs, params, dat);
}

static void
test_mkiovec(const fnx_iobufs_t *iobufs, const tparams_t *params)
{
	int rc;
	size_t nelems, cnt;
	fnx_iovec_t iov[2 * FNX_RSEGNBK];

	nelems = FNX_NELEMS(iov);
	rc = fnx_iobufs_mkiovec(iobufs, iov, nelems, &cnt);
	fnx_assert(rc == 0);
	fnx_assert(cnt == (params->first.cnt + params->second.cnt));
}

static void
test_iobufs(const tparams_t *params)
{
	char *dat;
	size_t len;
	fnx_iobufs_t *iobufs;

	len = params->first.len + params->second.len;
	dat = (char *)new_data(len);
	iobufs = new_iobufs();

	test_assign(iobufs, params, dat);
	test_mkiovec(iobufs, params);

	del_iobufs(iobufs);
	del_data(dat, len);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

#define PRM(o, l, i, c) \
	{ .check = 1, .off = o, .len = l, .idx = i, .cnt = c }

#define PRM_NONE \
	{ .check = 0, .off = 0, .len = 0, .idx = 0, .cnt = 0 }

#define DEFTEST(name, prm1, prm2) \
	static const tparams_t name = { prm1, prm2 }

#define RUNTEST(name) test_iobufs(& name)


static void test_specific(void)
{
	int rc;
	fnx_off_t  off = 135170;
	fnx_size_t len = 131070;
	fnx_iobufs_t *iobufs;
	char *dat;

	dat = (char *)new_data(len);
	iobufs = new_iobufs();
	rc = fnx_iobufs_assign(iobufs, off, len, dat);
	fnx_assert(rc == 0);

	del_iobufs(iobufs);
	del_data(dat, len);
}

int main(void)
{
	/* Unit-tests params definitions */
	DEFTEST(test1,
	        PRM(0, 1, 0, 1), PRM_NONE);
	DEFTEST(test2,
	        PRM(0, 2, 0, 1), PRM_NONE);
	DEFTEST(test3,
	        PRM(1, 2, 0, 1), PRM_NONE);
	DEFTEST(test4,
	        PRM(1, BKSZ - 1, 0, 1), PRM_NONE);
	DEFTEST(test5,
	        PRM(1, BKSZ + 1, 0, 2), PRM_NONE);
	DEFTEST(test6,
	        PRM(BKSZ - 1, 1, 0, 1), PRM_NONE);
	DEFTEST(test7,
	        PRM(BKSZ - 1, 2, 0, 2), PRM_NONE);
	DEFTEST(test8,
	        PRM(BKSZ + 1, 3, 1, 1), PRM_NONE);
	DEFTEST(test9,
	        PRM(BKSZ + 1, 2 * BKSZ, 1, 3), PRM_NONE);
	DEFTEST(test10,
	        PRM(10, SGSZ - 10, 0, SGNB), PRM_NONE);
	DEFTEST(test11,
	        PRM(BKSZ + 1, SGSZ - (2 * BKSZ) - 2, 1, SGNB - 2), PRM_NONE);
	DEFTEST(test12,
	        PRM(SGSZ / 2 + 1, (3 * BKSZ) + 7, SGNB / 2, 4), PRM_NONE);
	DEFTEST(test13,
	        PRM(0, SGSZ, 0, SGNB), PRM(SGSZ, 0, 0, 0));
	DEFTEST(test14,
	        PRM(0, SGSZ, 0, SGNB), PRM(SGSZ, 1, 0, 1));
	DEFTEST(test15,
	        PRM(0, SGSZ, 0, SGNB), PRM(SGSZ, SGSZ, 0, SGNB));
	DEFTEST(test16,
	        PRM(1, SGSZ - 1, 0, SGNB), PRM(SGSZ, SGSZ, 0, SGNB));
	DEFTEST(test17,
	        PRM(BKSZ + 1, SGSZ - BKSZ - 1, 1, SGNB - 1),
	        PRM(SGSZ, SGSZ - BKSZ - 1, 0, SGNB - 1));

	/* Init allocator */
	fnx_alloc_init(&s_alloc);

	/* Unit-tests execution */
	RUNTEST(test1);
	RUNTEST(test2);
	RUNTEST(test3);
	RUNTEST(test4);
	RUNTEST(test5);
	RUNTEST(test6);
	RUNTEST(test7);
	RUNTEST(test8);
	RUNTEST(test9);
	RUNTEST(test10);
	RUNTEST(test11);
	RUNTEST(test12);
	RUNTEST(test13);
	RUNTEST(test14);
	RUNTEST(test15);
	RUNTEST(test16);
	RUNTEST(test17);

	/* Specific unitests */
	test_specific();

	/* Clean & destroy allocator */
	fnx_alloc_clear(&s_alloc);
	fnx_alloc_destroy(&s_alloc);

	return EXIT_SUCCESS;
}


