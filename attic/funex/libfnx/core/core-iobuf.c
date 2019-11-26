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
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/uio.h>

#include <fnxinfra.h>
#include "core-blobs.h"
#include "core-types.h"
#include "core-stats.h"
#include "core-addr.h"
#include "core-addri.h"
#include "core-elems.h"
#include "core-alloc.h"
#include "core-iobuf.h"
#include "core-jobsq.h"


/* Nil-block */
static const fnx_blk_t s_nilbk;


static fnx_off_t off_within_blk(fnx_off_t off)
{
	return off % (fnx_off_t)FNX_BLKSIZE;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void *getdptr(const void *blk, fnx_off_t off)
{
	return ((char *)blk + (ptrdiff_t)off);
}

static void *getnptr(const void *buf, fnx_size_t len)
{
	return getdptr(buf, (fnx_off_t)len);
}

static void copy_data(void *tgtb, fnx_off_t tgt_off,
                      const void *srcb, fnx_off_t src_off, fnx_size_t len)
{
	void *tgt;
	const void *src;
	fnx_off_t end;

	end = fnx_off_end(tgt_off, len);
	fnx_assert((end - tgt_off) <= FNX_BLKSIZE);
	end = fnx_off_end(src_off, len);
	fnx_assert((end - src_off) <= FNX_BLKSIZE);

	tgt = getdptr(tgtb, tgt_off);
	src = getdptr(srcb, src_off);

	memcpy(tgt, src, len);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void bkref_bzero(fnx_bkref_t *bkref)
{
	fnx_blk_t *blk = bkref->bk_blk;
	fnx_bzero(blk, sizeof(*blk));
}

static void bkref_prepare(fnx_bkref_t *bkref, fnx_off_t off, fnx_size_t len)
{
	if ((off > 0) || (len < FNX_BLKSIZE)) {
		bkref_bzero(bkref);
	}
}

static void bkref_assign(fnx_bkref_t *bkref,
                         fnx_off_t off, fnx_size_t len, const void *buf)
{
	fnx_assert(len <= FNX_BLKSIZE);
	fnx_assert(((size_t)off_within_blk(off) + len) <= FNX_BLKSIZE);
	copy_data(bkref->bk_blk, off, buf, 0, len);
}

static void bkref_merge(fnx_bkref_t *bkref, fnx_off_t xoff, fnx_size_t xlen,
                        const fnx_bkref_t *other)
{
	fnx_off_t  off;
	fnx_size_t len;

	fnx_assert(xlen <= FNX_BLKSIZE);
	fnx_assert(((size_t)off_within_blk(xoff) + xlen) <= FNX_BLKSIZE);

	off  = off_within_blk(xoff);
	len  = fnx_off_len(0, off);
	copy_data(bkref->bk_blk, 0, other->bk_blk, 0, len);

	off = fnx_off_end(off, xlen);
	len = fnx_off_len(off, FNX_BLKSIZE);
	copy_data(bkref->bk_blk, off, other->bk_blk, off, len);
}

static void datab_to_iovec(const void *datab,
                           fnx_off_t off, fnx_size_t len, fnx_iovec_t *iov)
{
	fnx_assert(datab != NULL);
	fnx_assert(off < FNX_BLKSIZE);
	fnx_assert((off + (int)len) <= FNX_BLKSIZE);

	iov->iov_base = getdptr(datab, off);
	iov->iov_len  = len;
}

static void bkref_to_iovec(const fnx_bkref_t *bkref,
                           fnx_off_t off, fnx_size_t len, fnx_iovec_t *iov)
{
	datab_to_iovec(bkref->bk_blk, off, len, iov);
}

void fnx_bkref_init(fnx_bkref_t *bkref)
{
	fnx_jelem_init(&bkref->bk_jelem);
	fnx_link_init(&bkref->bk_clink);
	fnx_baddr_reset(&bkref->bk_baddr);
	bkref->bk_refcnt    = 0;
	bkref->bk_blk       = NULL;
	bkref->bk_hlnk      = NULL;
	bkref->bk_spmap     = NULL;
	bkref->bk_cached    = FNX_FALSE;
	bkref->bk_slaved    = FNX_FALSE;
	bkref->bk_magic     = FNX_BKREF_MAGIC;
}

void fnx_bkref_destroy(fnx_bkref_t *bkref)
{
	fnx_assert(bkref->bk_magic == FNX_BKREF_MAGIC);
	fnx_assert(bkref->bk_hlnk == NULL);
	fnx_assert(!bkref->bk_cached);
	fnx_jelem_destroy(&bkref->bk_jelem);
	fnx_link_destroy(&bkref->bk_clink);
	fnx_baddr_reset(&bkref->bk_baddr);
	bkref->bk_blk       = NULL;
	bkref->bk_spmap     = NULL;
	bkref->bk_magic     = 6;
}

void fnx_bkref_setup(fnx_bkref_t *bkref, const fnx_baddr_t *baddr, void *blk)
{
	fnx_bkref_bind(bkref, baddr);
	bkref->bk_blk = blk;
}

void fnx_bkref_bzero(fnx_bkref_t *bkref)
{
	fnx_bzero(bkref->bk_blk, FNX_BLKSIZE);
}

void fnx_bkref_merge(fnx_bkref_t *bkref, const fnx_bkref_t *other,
                     fnx_off_t xoff, fnx_size_t xlen)
{
	fnx_assert(other != NULL);
	bkref_merge(bkref, xoff, xlen, other);
}

void fnx_bkref_merge2(fnx_bkref_t        *bkref,
                      const fnx_bkref_t  *other,
                      const fnx_lrange_t *lrange)
{
	fnx_bkref_merge(bkref, other, lrange->off, lrange->len);
}


void fnx_bkref_bind(fnx_bkref_t *bkref, const fnx_baddr_t *baddr)
{
	fnx_assert(baddr->frg == 0);
	fnx_baddr_copy(&bkref->bk_baddr, baddr);
}

int fnx_bkref_isbinded(const fnx_bkref_t *bkref)
{
	return !fnx_baddr_isnull(&bkref->bk_baddr);
}

int fnx_bkref_ismutable(const fnx_bkref_t *bkref)
{
	fnx_assert(bkref != NULL);
	return !bkref->bk_slaved;
}

void fnx_bkref_attach(fnx_bkref_t *bkref, fnx_spmap_t *spmap)
{
	fnx_assert(bkref->bk_spmap == NULL);

	if (spmap != NULL) {
		bkref->bk_spmap = spmap;
		spmap->sm_vnode.v_refcnt++;
	}
}

fnx_spmap_t *fnx_bkref_detach(fnx_bkref_t *bkref)
{
	fnx_spmap_t *spmap = bkref->bk_spmap;

	if (spmap != NULL) {
		bkref->bk_spmap = NULL;
		spmap->sm_vnode.v_refcnt--;
	}
	return spmap;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_bkref_t *malloc_bk(fnx_alloc_t *alloc)
{
	const fnx_baddr_t nil_baddr = { FNX_VOL_NULL, FNX_LBA_NULL, 0 };
	return fnx_alloc_new_bk(alloc, &nil_baddr);
}

static void free_bk(fnx_alloc_t *alloc, fnx_bkref_t **bkref)
{
	fnx_alloc_del_bk(alloc, *bkref);
	*bkref = NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void fnx_iobuf_init(fnx_iobuf_t *iobuf, fnx_ino_t ino,
                           fnx_alloc_t *alloc)
{
	fnx_lrange_init(&iobuf->rng);
	fnx_bzero(iobuf->bks, sizeof(iobuf->bks));
	iobuf->ino   = ino;
	iobuf->alloc = alloc;
}

static void fnx_iobuf_destroy(fnx_iobuf_t *iobuf)
{
	fnx_lrange_destroy(&iobuf->rng);
	fnx_bzero(iobuf->bks, sizeof(iobuf->bks));
	iobuf->ino   = FNX_INO_NULL;
	iobuf->alloc = NULL;
}

static void iobuf_dealloc_bks(fnx_iobuf_t *iobuf)
{
	size_t i, pos;
	fnx_bkref_t **bkref;
	const fnx_lrange_t *lrange;

	lrange = &iobuf->rng;
	for (i = 0; i < lrange->cnt; ++i) {
		pos   = lrange->idx + i;
		bkref = &iobuf->bks[pos];
		if (*bkref != NULL) {
			free_bk(iobuf->alloc, bkref);
		}
	}
}

static int iobuf_alloc_bks(fnx_iobuf_t *iobuf)
{
	fnx_bkref_t *bkref;
	const fnx_lrange_t *lrange;

	lrange = &iobuf->rng;
	for (size_t pos, i = 0; i < lrange->cnt; ++i) {
		pos = lrange->idx + i;
		fnx_assert(pos < FNX_NELEMS(iobuf->bks));
		fnx_assert(iobuf->bks[pos] == NULL);

		bkref = malloc_bk(iobuf->alloc);
		if (bkref == NULL) {
			iobuf_dealloc_bks(iobuf);
			return -ENOMEM;
		}
		iobuf->bks[pos] = bkref;
	}
	return 0;
}

static void iobuf_assign_buf(const fnx_iobuf_t *iobuf, const void *buf)
{
	fnx_off_t off, off2, soff;
	fnx_size_t len, len2, slen;
	const void *dat;
	fnx_bkref_t *bkref;
	const fnx_lrange_t *lrange;

	lrange = &iobuf->rng;
	off = lrange->off;
	len = lrange->len;
	dat = buf;
	for (size_t pos, i = 0; i < lrange->cnt; ++i) {
		pos   = lrange->idx + i;
		bkref = iobuf->bks[pos];

		soff  = off_within_blk(off);
		off2  = fnx_off_ceil_blk(off);
		len2  = fnx_off_len(off, off2);
		slen  = fnx_size_min(len, len2);
		bkref_prepare(bkref, soff, slen);
		bkref_assign(bkref, soff, slen, dat);

		dat = getnptr(dat, slen);
		off = fnx_off_end(off, slen);
		len -= slen;
	}
}

static int iobuf_assign(fnx_iobuf_t *iobuf,
                        fnx_off_t off, fnx_size_t len, const void *buf)
{
	int rc;

	if ((iobuf == NULL) && (len == 0)) {
		return 0;
	}
	fnx_lrange_setup(&iobuf->rng, off, len);
	if (buf == NULL) {
		return 0; /* OK, no-copy-data */
	}
	rc = iobuf_alloc_bks(iobuf);
	if (rc != 0) {
		return rc;
	}
	iobuf_assign_buf(iobuf, buf);
	return 0;
}

static int iobuf_mkiovec(const fnx_iobuf_t *iobuf,
                         fnx_iovec_t *iovec, size_t cnt, size_t *res)
{
	fnx_off_t off, off2, soff;
	fnx_size_t len, slen, len2;
	const fnx_bkref_t  *bkref;
	const fnx_lrange_t *lrange;

	lrange = &iobuf->rng;
	if (cnt < lrange->cnt) {
		return -1;
	}
	off = lrange->off;
	len = lrange->len;
	for (size_t pos, i = 0; i < lrange->cnt; ++i) {
		pos  = lrange->idx + i;

		soff = off_within_blk(off);
		off2 = fnx_off_ceil_blk(off);
		len2 = fnx_off_len(off, off2);
		slen = fnx_size_min(len, len2);

		bkref = iobuf->bks[pos];
		if (bkref != NULL) {
			bkref_to_iovec(bkref, soff, slen, &iovec[i]);
		} else {
			datab_to_iovec(s_nilbk, soff, slen, &iovec[i]);
		}

		off = fnx_off_end(off, slen);
		len -= slen;
	}
	*res = lrange->cnt;
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t niob(const fnx_iobufs_t *iob)
{
	return FNX_NELEMS((iob)->iob);
}

void fnx_iobufs_init(fnx_iobufs_t *iobufs)
{
	for (size_t i = 0; i < niob(iobufs); ++i) {
		fnx_iobuf_init(&iobufs->iob[i], FNX_INO_NULL, NULL);
	}
}

void fnx_iobufs_destroy(fnx_iobufs_t *iobufs)
{
	for (size_t i = 0; i < niob(iobufs); ++i) {
		fnx_iobuf_destroy(&iobufs->iob[i]);
	}
}

void fnx_iobufs_setup(fnx_iobufs_t *iobufs, fnx_ino_t ino,
                      fnx_alloc_t *alloc)
{
	for (size_t i = 0; i < niob(iobufs); ++i) {
		iobufs->iob[i].ino   = ino;
		iobufs->iob[i].alloc = alloc;
	}
}

void fnx_iobufs_release(fnx_iobufs_t *iobufs)
{
	for (size_t i = 0; i < niob(iobufs); ++i) {
		iobuf_dealloc_bks(&iobufs->iob[i]);
	}
}

static fnx_bkcnt_t iobufs_bkcnt(const fnx_iobufs_t *iobufs)
{
	fnx_bkcnt_t cnt = 0;

	for (size_t i = 0; i < niob(iobufs); ++i) {
		cnt += iobufs->iob[i].rng.cnt;
	}
	return cnt;
}

fnx_off_t fnx_iobufs_beg(const fnx_iobufs_t *iobufs)
{
	return iobufs->iob[0].rng.off;
}

fnx_off_t fnx_iobufs_end(const fnx_iobufs_t *iobufs)
{
	fnx_off_t off, end = 0;

	for (size_t i = 0; i < niob(iobufs); ++i) {
		off = fnx_lrange_end(&iobufs->iob[i].rng);
		end = fnx_off_max(off, end);
	}
	return end;
}

fnx_size_t fnx_iobufs_len(const fnx_iobufs_t *iobufs)
{
	fnx_size_t len = 0;

	for (size_t i = 0; i < niob(iobufs); ++i) {
		len += iobufs->iob[i].rng.len;
	}
	return len;
}

fnx_size_t fnx_iobufs_cnt(const fnx_iobufs_t *iobufs)
{
	fnx_size_t cnt = 0;

	for (size_t i = 0; i < niob(iobufs); ++i) {
		cnt += iobufs->iob[i].rng.cnt;
	}
	return cnt;
}

static fnx_bool_t isvalidseg(fnx_off_t off, fnx_size_t len)
{
	fnx_size_t dif;
	fnx_off_t beg, ref, end;
	const fnx_iobufs_t *iobs    = NULL;
	const fnx_size_t nsegs_max  = fnx_nelems(iobs->iob);
	const fnx_size_t seg_size   = FNX_RSEGSIZE;
	const fnx_size_t blk_size   = FNX_BLKSIZE;

	beg = fnx_off_floor_rseg(off);
	ref = fnx_off_end(off, len + blk_size);
	end = fnx_off_floor_rseg(ref);
	dif = fnx_off_len(beg, end);

	return (dif <= (nsegs_max * seg_size));
}

int fnx_iobufs_assign(fnx_iobufs_t *iobufs,
                      fnx_off_t off, fnx_size_t len, const void *buf)
{
	int rc = -1;
	fnx_off_t uoff;
	fnx_size_t rlen, slen;

	if (!isvalidseg(off, len)) {
		return -1;
	}
	for (size_t i = 0; i < niob(iobufs); ++i) {
		uoff = fnx_off_ceil_rseg(off);
		rlen = fnx_off_len(off, uoff);
		slen = fnx_size_min(len, rlen);
		if (slen == 0) {
			break;
		}
		rc = iobuf_assign(&iobufs->iob[i], off, slen, buf);
		if (rc != 0) {
			return rc;
		}
		off = fnx_off_end(off, slen);
		len -= slen;
		if (buf != NULL) {
			buf = getnptr(buf, slen);
		}
	}
	return 0;
}

int fnx_iobufs_mkiovec(const fnx_iobufs_t *iobufs,
                       fnx_iovec_t *iovec, size_t cnt, size_t *res)
{
	int rc = -1;
	size_t total, nset = 0, nn = 0;

	total = iobufs_bkcnt(iobufs);
	if (total > cnt) {
		return -1;
	}
	for (size_t i = 0; i < niob(iobufs); ++i) {
		rc = iobuf_mkiovec(&iobufs->iob[i], &iovec[nset], cnt - nset, &nn);
		if (rc != 0) {
			break;
		}
		nset += nn;
	}
	*res = nset;
	return rc;
}

void fnx_iobufs_increfs(const fnx_iobufs_t *iobufs)
{
	fnx_bkref_t *bkref;
	const fnx_iobuf_t *iobuf;

	for (size_t i = 0; i < niob(iobufs); ++i) {
		iobuf = &iobufs->iob[i];
		for (size_t pos, j = 0; j < iobuf->rng.cnt; ++j) {
			pos = iobuf->rng.idx + j;
			bkref = iobuf->bks[pos];
			if (bkref != NULL) {
				bkref->bk_refcnt += 1;
			}
		}
	}
}

void fnx_iobufs_decrefs(const fnx_iobufs_t *iobufs)
{
	fnx_bkref_t *bkref;
	const fnx_iobuf_t *iobuf;

	for (size_t i = 0; i < niob(iobufs); ++i) {
		iobuf = &iobufs->iob[i];
		for (size_t pos, j = 0; j < iobuf->rng.cnt; ++j) {
			pos = iobuf->rng.idx + j;
			bkref = iobuf->bks[pos];
			if (bkref != NULL) {
				fnx_assert(bkref->bk_refcnt > 0);
				bkref->bk_refcnt -= 1;
			}
		}
	}
}

int fnx_iobufs_relax(fnx_iobufs_t *iobufs,
                     fnx_bkref_t *bkrefs[], size_t lim, size_t *nn)
{
	size_t cnt;
	fnx_bkref_t *bkref;
	fnx_iobuf_t *iobuf;

	*nn = 0;
	cnt = 0;
	for (size_t i = 0; i < niob(iobufs); ++i) {
		iobuf = &iobufs->iob[i];
		for (size_t pos, j = 0; j < iobuf->rng.cnt; ++j) {
			pos = iobuf->rng.idx + j;
			bkref = iobuf->bks[pos];
			if (bkref == NULL) {
				continue;
			}
			fnx_assert(cnt < lim);
			if (cnt >= lim) {
				return -1;
			}
			iobuf->bks[pos] = NULL;
			bkrefs[cnt++] = bkref;
		}
	}
	*nn = cnt;
	return 0;
}

/* XXX -- debug only; remove me */
void fnx_iobufs_verify(const fnx_iobufs_t *iobufs)
{
	const fnx_bkref_t *bkref;
	const fnx_iobuf_t *iobuf;

	for (size_t i = 0; i < niob(iobufs); ++i) {
		iobuf = &iobufs->iob[i];
		for (size_t pos, j = 0; j < iobuf->rng.cnt; ++j) {
			pos = iobuf->rng.idx + j;
			fnx_assert(pos < fnx_nelems(iobuf->bks));
			bkref = iobuf->bks[pos];
			fnx_assert(bkref != NULL);
		}
	}
}
