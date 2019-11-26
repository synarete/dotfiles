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
#include <limits.h>

#include <fnxhash.h>
#include <fnxinfra.h>
#include "core-blobs.h"
#include "core-types.h"
#include "core-stats.h"
#include "core-addr.h"
#include "core-addri.h"
#include "core-elems.h"


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_off_t lba_to_frg(fnx_lba_t lba)
{
	return (fnx_off_t)(lba * FNX_BLKNFRG);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Directory addressing */
fnx_off_t fnx_hash_to_dseg(fnx_hash_t hash)
{
	/* Avoiding modulo on power of 2; using prime + mask */
	fnx_staticassert((FNX_DIRNSEGS - 1) == 0x7FFF);
	return (fnx_off_t)((hash % 32771) & 0x7FFF);
}

int fnx_doff_isvalid(fnx_off_t doff)
{
	return (((doff >= FNX_DOFF_SELF) && (doff < FNX_DOFF_END)) ||
	        (doff == FNX_DOFF_PROOT));
}

fnx_off_t fnx_doff_to_dseg(fnx_off_t doff)
{
	return (doff - FNX_DOFF_BEGINS) / FNX_DSEGNDENT;
}

fnx_off_t fnx_dseg_to_doff(fnx_off_t dseg)
{
	return FNX_DOFF_BEGINS + (dseg * FNX_DSEGNDENT);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_isvalid_volsz(fnx_off_t volsz)
{
	const fnx_off_t volsz_min = (fnx_off_t)FNX_VOLSIZE_MIN;
	const fnx_off_t volsz_max = (fnx_off_t)FNX_VOLSIZE_MAX;

	return ((volsz_min <= volsz) && (volsz <= volsz_max));
}

/* Converts volume-size to blocks-count; forces section alignment */
fnx_bkcnt_t fnx_volsz_to_bkcnt(fnx_off_t volsz)
{
	return ((fnx_bkcnt_t)(volsz) / FNX_BCKTSIZE) * FNX_BCKTNBK;
}

fnx_bkcnt_t fnx_bytes_to_nfrg(fnx_size_t nbytes)
{
	return (nbytes + FNX_FRGSIZE - 1) / FNX_FRGSIZE;
}

fnx_bkcnt_t fnx_bytes_to_nbk(fnx_size_t nbytes)
{
	return (nbytes + FNX_BLKSIZE - 1) / FNX_BLKSIZE;
}

fnx_bkcnt_t fnx_nfrg_to_nbk(fnx_bkcnt_t nfrg)
{
	return (nfrg + FNX_BLKNFRG - 1) / FNX_BLKNFRG;
}

fnx_size_t fnx_nfrg_to_nbytes(fnx_bkcnt_t nfrg)
{
	return fnx_nbk_to_nbytes(fnx_nfrg_to_nbk(nfrg));
}

/* Converts LBA to virtual-LBA vies-versa: turns MSB on/off */
fnx_lba_t fnx_lba_to_vlba(fnx_lba_t lba)
{
	return lba | FNX_LBA_APEX0;
}

fnx_lba_t fnx_vlba_to_lba(fnx_lba_t vlba)
{
	return vlba & ~FNX_LBA_APEX0;
}

/* Converts file's logical range to number of blocks/fragments */
static fnx_bkcnt_t fnx_range_to_nbk(fnx_off_t off, fnx_size_t nbytes)
{
	fnx_bkcnt_t beg, end, nbk, bsz = FNX_BLKSIZE;
	const fnx_bkcnt_t pos = (fnx_bkcnt_t)off;

	beg = pos / bsz;
	end = (pos + nbytes + bsz - 1) / bsz;
	nbk = end - beg;
	return nbk;
}

fnx_bkcnt_t fnx_range_to_nfrg(fnx_off_t off, fnx_size_t nbytes)
{
	return fnx_range_to_nbk(off, nbytes) * FNX_BLKNFRG;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_bkcnt_t range_to_nbk(fnx_off_t off, fnx_size_t len)
{
	fnx_size_t blen;
	fnx_off_t boff, bend;
	fnx_bkcnt_t bkcnt = 0;

	if (len > 0) {
		boff  = fnx_off_floor_blk(off);
		bend  = fnx_off_end(off, len);
		blen  = fnx_off_len(boff, bend);
		bkcnt = fnx_bytes_to_nbk(blen);
	}
	return bkcnt;
}

/* Offset to block-index within-segment */
static fnx_lba_t off_to_lba(fnx_off_t off)
{
	return (fnx_lba_t)(off / FNX_BLKSIZE);
}

static fnx_size_t off_to_segbi(fnx_off_t off)
{
	fnx_off_t seg;
	fnx_lba_t lba, beg;
	fnx_size_t idx;

	seg = fnx_off_floor_rseg(off);
	beg = off_to_lba(seg);
	lba = off_to_lba(off);
	idx = (lba - beg);
	return idx;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int is_sulrange(fnx_off_t roff, fnx_size_t rlen,
                       fnx_off_t soff, fnx_size_t slen)
{
	fnx_off_t rfin, sfin;

	rfin = fnx_off_end(roff, rlen);
	sfin = fnx_off_end(soff, slen);

	return ((roff <= soff) && (sfin <= rfin));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_baddr_setup(fnx_baddr_t *baddr,
                     fnx_vol_t vol, fnx_lba_t lba, fnx_off_t frg)
{
	baddr->vol = vol;
	baddr->lba = lba;
	baddr->frg = (short)frg;
}

void fnx_baddr_setup2(fnx_baddr_t *baddr,
                      fnx_vol_t vol, fnx_lba_t lba, fnx_off_t frg)
{
	fnx_off_t frg_abs, frg2;
	fnx_lba_t lba2;

	frg_abs = lba_to_frg(lba) + frg;
	lba2 = fnx_frg_to_lba(frg_abs);
	frg2 = frg_abs - lba_to_frg(lba2);

	fnx_baddr_setup(baddr, vol, lba2, frg2);
}

void fnx_baddr_reset(fnx_baddr_t *baddr)
{
	baddr->vol = FNX_VOL_NULL;
	baddr->lba = FNX_LBA_NULL;
	baddr->frg = 0;
}

void fnx_baddr_copy(fnx_baddr_t *tgt, const fnx_baddr_t *src)
{
	tgt->vol = src->vol;
	tgt->lba = src->lba;
	tgt->frg = src->frg;
}

/* Force non-fragmented block address; downcast to block address */
void fnx_baddr_floor(fnx_baddr_t *tgt, const fnx_baddr_t *src)
{
	tgt->vol = src->vol;
	tgt->lba = src->lba;
	tgt->frg = 0;
}

int fnx_baddr_isequal(const fnx_baddr_t *baddr1,
                      const fnx_baddr_t *baddr2)
{
	return ((baddr1->lba == baddr2->lba) &&
	        (baddr1->vol == baddr2->vol) &&
	        (baddr1->frg == baddr2->frg));
}

int fnx_baddr_isnull(const fnx_baddr_t *baddr)
{
	return ((baddr->lba == FNX_LBA_NULL) ||
	        (baddr->vol == FNX_VOL_NULL));
}

fnx_lba_t fnx_baddr_lbafin(const fnx_baddr_t *baddr)
{
	fnx_off_t beg, end;

	beg = lba_to_frg(baddr->lba);
	end = beg + baddr->frg;
	return fnx_frg_to_lba(end);
}

/*
 * Retruns the distance between to block-address, in fragments units.
 * If not within the same volume, returns non-valid value.
 */
fnx_off_t fnx_baddr_frgdist(const fnx_baddr_t *addr1,
                            const fnx_baddr_t *addr2)
{
	fnx_off_t frg1, frg2, dis = LONG_MIN;

	if (addr1->vol == addr2->vol) {
		frg1 = lba_to_frg(addr1->lba) + addr1->frg;
		frg2 = lba_to_frg(addr2->lba) + addr2->frg;
		dis  = frg2 - frg1;
	}
	return dis;
}


/*
 * Block-address generators: addressing for fixed-LBA objects.
 */
void fnx_baddr_for_super(fnx_baddr_t *baddr)
{
	fnx_baddr_setup(baddr, FNX_VOL_ROOT, FNX_LBA_SUPER, 0);
}

void fnx_baddr_for_spmap(fnx_baddr_t *baddr, fnx_size_t bckt)
{
	fnx_lba_t lba;

	lba = fnx_bckt_to_lba(bckt);
	fnx_baddr_setup(baddr, FNX_VOL_ROOT, lba, 0);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_lrange_init(fnx_lrange_t *lrange)
{
	lrange->off = 0;
	lrange->len = 0;
	lrange->bki = 0;
	lrange->idx = 0;
	lrange->cnt = 0;
}

void fnx_lrange_destroy(fnx_lrange_t *lrange)
{
	lrange->off = -1;
	lrange->len = 0;
	lrange->bki = 0;
	lrange->idx = 0;
	lrange->cnt = 0;
}

void fnx_lrange_copy(fnx_lrange_t *lrange, const fnx_lrange_t *other)
{
	lrange->off = other->off;
	lrange->len = other->len;
	lrange->bki = other->bki;
	lrange->idx = other->idx;
	lrange->cnt = other->cnt;
}

void fnx_lrange_setup(fnx_lrange_t *lrange, fnx_off_t off, fnx_size_t len)
{
	lrange->off = off;
	lrange->len = len;
	lrange->bki = off_to_lba(off);
	lrange->idx = off_to_segbi(off);
	lrange->cnt = range_to_nbk(off, len);
}

void fnx_lrange_rsetup(fnx_lrange_t *lrange, fnx_off_t beg, fnx_off_t end)
{
	fnx_lrange_setup(lrange, beg, fnx_off_len(beg, end));
}

int fnx_lrange_issub(const fnx_lrange_t *lrange, fnx_off_t off, fnx_size_t len)
{
	return is_sulrange(lrange->off, lrange->len, off, len);
}

int fnx_lrange_issub2(const fnx_lrange_t *lrange, const fnx_lrange_t *sub)
{
	return fnx_lrange_issub(lrange, sub->off, sub->len);
}

fnx_off_t fnx_lrange_begin(const fnx_lrange_t *lrange)
{
	return lrange->off;
}

fnx_off_t fnx_lrange_end(const fnx_lrange_t *lrange)
{
	return fnx_off_end(lrange->off, lrange->len);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_vaddr_init(fnx_vaddr_t *vaddr, fnx_vtype_e vtype)
{
	vaddr->ino    = FNX_INO_NULL;
	vaddr->xno    = FNX_XNO_NULL;
	vaddr->vtype  = vtype;
}

void fnx_vaddr_destroy(fnx_vaddr_t *vaddr)
{
	vaddr->ino    = FNX_INO_NULL;
	vaddr->xno    = FNX_XNO_NULL;
	vaddr->vtype  = FNX_VTYPE_NONE;
}

void fnx_vaddr_setup(fnx_vaddr_t *vaddr, fnx_vtype_e vtype,
                     fnx_ino_t ino, fnx_xno_t xno)
{
	vaddr->ino   = ino;
	vaddr->xno   = xno;
	vaddr->vtype = vtype;
}

void fnx_vaddr_copy(fnx_vaddr_t *vaddr, const fnx_vaddr_t *other)
{
	vaddr->ino    = other->ino;
	vaddr->xno    = other->xno;
	vaddr->vtype  = other->vtype;
}

void fnx_vaddr_setnull(fnx_vaddr_t *vaddr)
{
	vaddr->ino = FNX_INO_NULL;
	if (vaddr->vtype == FNX_VTYPE_VBK) {
		vaddr->xno = FNX_LBA_VNULL;
	} else {
		vaddr->xno = FNX_INO_NULL;
	}
}

int fnx_vaddr_isnull(const fnx_vaddr_t *vaddr)
{
	return ((vaddr->vtype == FNX_VTYPE_NONE) ||
	        (vaddr->ino == FNX_INO_NULL) ||
	        (vaddr->xno == FNX_LBA_VNULL));
}

int fnx_vaddr_isequal(const fnx_vaddr_t *x, const fnx_vaddr_t *y)
{
	return (x->ino == y->ino) &&
	       (x->xno == y->xno) &&
	       (x->vtype == y->vtype);
}

static uint64_t vtype_inflate(fnx_vtype_e vtype)
{
	const uint64_t v = (uint64_t)vtype;
	return (v | (v << 16) | (v << 32) | (v << 48));
}

static uint64_t vaddr_calcvhash1(const fnx_vaddr_t *vaddr)
{
	uint64_t in[4], res = 0;
	const uint8_t seed[16] = {
		0x09, 0xae, 0x16, 0xa3, 0xb2, 0xf9, 0x04, 0x04,
		0xb4, 0x92, 0xb6, 0x6f, 0xbe, 0x98, 0xf2, 0x73
	};

	in[0] = 1;
	in[1] = vtype_inflate(vaddr->vtype);
	in[2] = (uint64_t)(vaddr->ino);
	in[3] = (uint64_t)(vaddr->xno)  + 1;
	blgn_siphash64(seed, in, sizeof(in), &res);
	return res;
}

static uint64_t vaddr_calcvhash2(const fnx_vaddr_t *vaddr)
{
	uint64_t in[4], res[2];

	in[0] = 2;
	in[1] = (uint64_t)(vaddr->ino);
	in[2] = vtype_inflate(vaddr->vtype);
	in[3] = (uint64_t)(vaddr->xno) + 1;
	blgn_murmur3(in, sizeof(in), 0, res);
	return (res[0] ^ res[1]);
}

static uint64_t vaddr_calcvhash3(const fnx_vaddr_t *vaddr)
{
	uint64_t in[4], res;

	in[0] = 3;
	in[1] = (uint64_t)(vaddr->ino);
	in[2] = (uint64_t)(vaddr->xno) + 1;
	in[3] = vtype_inflate(vaddr->vtype);
	blgn_bardak(in, sizeof(in), &res);
	return res;
}

static uint64_t vaddr_calcihash(const fnx_vaddr_t *vaddr)
{
	const uint64_t ino = (uint64_t)(vaddr->ino);
	return blgn_wang(ino);
}

/* Calculates hash-value for vaddr */
fnx_hash_t fnx_vaddr_hash(const fnx_vaddr_t *vaddr)
{
	fnx_hash_t hash;
	const fnx_vtype_e vtype = vaddr->vtype;
	const fnx_xno_t xno = vaddr->xno;

	if (vtype == FNX_VTYPE_SPMAP) {
		hash = (fnx_hash_t)(xno - 1);
	} else if (fnx_vaddr_isi(vaddr)) {
		/* Inode's hash (or lookup-by-ino without explicit vtype) */
		hash = vaddr_calcihash(vaddr);
	} else if (vtype == FNX_VTYPE_VBK) {
		/* Vbk's hash: reduce collisions with multiple hash functions */
		hash = (xno & 1) ? vaddr_calcvhash1(vaddr) : vaddr_calcvhash2(vaddr);
	} else {
		/* Vnode's hash for all other meta objs */
		hash = vaddr_calcvhash3(vaddr);
	}
	return hash;
}

/*
 * Returns TRUE if vaddr is referring to address-by-ino; Special case: when
 * lookup-by-ino without explicit vtype. Normal case: when having inode type.
 */
int fnx_vaddr_isi(const fnx_vaddr_t *vaddr)
{
	int res = FNX_FALSE;
	const fnx_vtype_e vtype = vaddr->vtype;

	if (vaddr->xno == FNX_XNO_NULL) {
		res = ((vtype == FNX_VTYPE_ANY) || fnx_isitype(vtype));
	}
	return res;
}

fnx_lba_t fnx_vaddr_getvlba(const fnx_vaddr_t *vaddr)
{
	fnx_lba_t vlba = FNX_LBA_VNULL;

	if (vaddr->vtype == FNX_VTYPE_VBK) {
		vlba = (fnx_lba_t)vaddr->xno;
	}
	return vlba;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * V-address generators: for each sub-type, creates unique address-tuple.
 */
void fnx_vaddr_for_super(fnx_vaddr_t *vaddr)
{
	fnx_vaddr_setup(vaddr, FNX_VTYPE_SUPER, fnx_ino_super(), FNX_VOL_ROOT);
}

void fnx_vaddr_for_spmap(fnx_vaddr_t *vaddr, fnx_size_t idx)
{
	fnx_vaddr_setup(vaddr, FNX_VTYPE_SPMAP, fnx_ino_super(), (fnx_xno_t)idx);
}

void fnx_vaddr_for_inode(fnx_vaddr_t *vaddr, fnx_vtype_e vtype, fnx_ino_t ino)
{
	fnx_vaddr_setup(vaddr, vtype, ino, FNX_XNO_NULL);
}

void fnx_vaddr_for_by_ino(fnx_vaddr_t *vaddr, fnx_ino_t ino)
{
	const fnx_vtype_e vtype = fnx_ino_getvtype(ino);
	fnx_vaddr_for_inode(vaddr, vtype, ino);
}

void fnx_vaddr_for_dir(fnx_vaddr_t *vaddr, fnx_ino_t ino)
{
	fnx_vaddr_for_inode(vaddr, FNX_VTYPE_DIR, ino);
}

void fnx_vaddr_for_dirseg(fnx_vaddr_t *vaddr, fnx_ino_t ino, fnx_off_t dseg)
{
	fnx_vaddr_setup(vaddr, FNX_VTYPE_DIRSEG, ino, (fnx_xno_t)dseg);
}

void fnx_vaddr_for_reg(fnx_vaddr_t *vaddr, fnx_ino_t ino)
{
	fnx_vaddr_for_inode(vaddr, FNX_VTYPE_REG, ino);
}

void fnx_vaddr_for_regseg(fnx_vaddr_t *vaddr, fnx_ino_t ino, fnx_off_t off)
{
	fnx_off_t loff;

	loff = fnx_off_floor_rseg(off);
	fnx_vaddr_setup(vaddr, FNX_VTYPE_REGSEG, ino, (fnx_xno_t)loff);
}

void fnx_vaddr_for_regsec(fnx_vaddr_t *vaddr, fnx_ino_t ino, fnx_off_t off)
{
	fnx_off_t loff;

	fnx_assert(off >= (fnx_off_t)FNX_RSECSIZE);
	fnx_assert(off < (fnx_off_t)FNX_REGSIZE_MAX);

	loff = fnx_off_floor_rsec(off);
	fnx_vaddr_setup(vaddr, FNX_VTYPE_REGSEC, ino, (fnx_xno_t)loff);
}

void fnx_vaddr_for_vbk(fnx_vaddr_t *vaddr, fnx_lba_t lba)
{
	fnx_lba_t vlba;

	vlba = fnx_lba_to_vlba(lba);
	fnx_vaddr_setup(vaddr, FNX_VTYPE_VBK, fnx_ino_super(), (fnx_xno_t)vlba);
}

void fnx_vaddr_by_vtype(fnx_vaddr_t *vaddr, fnx_vtype_e vtype,
                        fnx_ino_t ino, fnx_xno_t xno)
{
	switch (vtype) {
		case FNX_VTYPE_SPMAP:
			fnx_vaddr_for_spmap(vaddr, xno);
			break;
		case FNX_VTYPE_SUPER:
			fnx_vaddr_for_super(vaddr);
			break;
		case FNX_VTYPE_CHRDEV:
		case FNX_VTYPE_BLKDEV:
		case FNX_VTYPE_SOCK:
		case FNX_VTYPE_FIFO:
		case FNX_VTYPE_SYMLNK1:
		case FNX_VTYPE_SYMLNK2:
		case FNX_VTYPE_SYMLNK3:
		case FNX_VTYPE_REFLNK:
			fnx_vaddr_for_inode(vaddr, vtype, ino);
			break;
		case FNX_VTYPE_REG:
			fnx_vaddr_for_reg(vaddr, ino);
			break;
		case FNX_VTYPE_DIR:
			fnx_vaddr_for_dir(vaddr, ino);
			break;
		case FNX_VTYPE_DIRSEG:
			fnx_vaddr_for_dirseg(vaddr, ino, (fnx_off_t)xno);
			break;
		case FNX_VTYPE_REGSEC:
			fnx_vaddr_for_regsec(vaddr, ino, (fnx_off_t)xno);
			break;
		case FNX_VTYPE_REGSEG:
			fnx_vaddr_for_regseg(vaddr, ino, (fnx_off_t)xno);
			break;
		case FNX_VTYPE_VBK:
			fnx_vaddr_for_vbk(vaddr, (fnx_lba_t)xno);
			break;
		case FNX_VTYPE_ANY:
		case FNX_VTYPE_EMPTY:
		case FNX_VTYPE_NONE:
		default:
			fnx_vaddr_init(vaddr, vtype);
			break;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void vba_reset(fnx_vaddr_t *vaddr)
{
	fnx_vaddr_for_vbk(vaddr, FNX_LBA_VNULL);
}

static void vba_copy(fnx_vaddr_t *vaddr, const fnx_vaddr_t *other)
{
	fnx_vaddr_copy(vaddr, other);
}

void fnx_segmap_init(fnx_segmap_t *segmap)
{
	fnx_lrange_init(&segmap->rng);
	fnx_foreach_arrelem(segmap->vba, vba_reset);
}

void fnx_segmap_destroy(fnx_segmap_t *segmap)
{
	fnx_lrange_destroy(&segmap->rng);
}

void fnx_segmap_copy(fnx_segmap_t *segmap, const fnx_segmap_t *other)
{
	fnx_lrange_copy(&segmap->rng, &other->rng);
	for (size_t i = 0; i < fnx_nelems(segmap->vba); ++i) {
		vba_copy(&segmap->vba[i], &other->vba[i]);
	}
}

int fnx_segmap_setup(fnx_segmap_t *segmap, fnx_off_t off, fnx_size_t len)
{
	fnx_size_t idx;
	fnx_bkcnt_t cnt;
	const fnx_bkcnt_t nbk_max = FNX_NELEMS(segmap->vba);

	fnx_lrange_setup(&segmap->rng, off, len);
	idx = segmap->rng.idx;
	cnt = segmap->rng.cnt;
	return ((idx + cnt) <= nbk_max) ? 0 : -1;
}

size_t fnx_segmap_nmaps(const fnx_segmap_t *segmap)
{
	size_t nmaps = 0;
	const fnx_vaddr_t *vaddr;
	const fnx_lrange_t *rng = &segmap->rng;

	for (size_t pos, i = 0; i < rng->cnt; ++i) {
		pos = rng->idx + i;
		vaddr = &segmap->vba[pos];
		if (!fnx_vaddr_isnull(vaddr)) {
			++nmaps;
		}
	}
	return nmaps;
}


