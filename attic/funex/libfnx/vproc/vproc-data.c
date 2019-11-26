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
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <fnxinfra.h>
#include <fnxcore.h>
#include <fnxpstor.h>
#include "vproc-elems.h"
#include "vproc-exec.h"
#include "vproc-data.h"
#include "vproc-private.h"

#define _FNX_IOBUFSNBK(iobs) \
	(FNX_NELEMS(iobs->iob) * FNX_NELEMS(iobs->iob[0].bks))

#define FNX_IOBUFSNBK   \
	_FNX_IOBUFSNBK(((const fnx_iobufs_t *)NULL))


/* Local helpers */
static const fnx_fileref_t *getfref(const fnx_task_t *task)
{
	fnx_task_check(task);
	fnx_assert(task->tsk_fref != NULL);
	return task->tsk_fref;
}

static int hasiob(const fnx_iobufs_t *iobs, size_t i)
{
	return (i < FNX_NELEMS(iobs->iob)) && (iobs->iob[i].rng.len > 0);
}

static int vba_isnull(const fnx_vaddr_t *vba)
{
	return fnx_vaddr_isnull(vba);
}

static int lrange_isseg0(const fnx_lrange_t *lrange)
{
	const fnx_off_t off = lrange->off;
	fnx_assert(off >= 0);
	fnx_assert(off < (fnx_off_t)FNX_REGSIZE_MAX);
	return ((off >= 0) && (off < (fnx_off_t)FNX_RSEGSIZE));
}

static int lrange_issec0(const fnx_lrange_t *lrange)
{
	const fnx_off_t off = lrange->off;
	fnx_assert(off >= 0);
	fnx_assert(off < (fnx_off_t)FNX_REGSIZE_MAX);
	return ((off >= 0) && (off < (fnx_off_t)FNX_RSECSIZE));
}

static int lrange_issubblk(const fnx_lrange_t *lrange)
{
	return (lrange->len < FNX_BLKSIZE);
}

static int inode_ispseudo(const fnx_inode_t *inode)
{
	return inode->i_vnode.v_pseudo;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int reg_ispseudo(const fnx_reg_t *reg)
{
	return (int)fnx_unlikely(inode_ispseudo(&reg->r_inode));
}

static fnx_ino_t reg_getino(const fnx_reg_t *reg)
{
	return fnx_inode_getino(&reg->r_inode);
}

static fnx_off_t reg_getsize(const fnx_reg_t *reg)
{
	return fnx_inode_getsize(&reg->r_inode);
}

static fnx_off_t reg_getbcap(const fnx_reg_t *reg)
{
	const fnx_bkcnt_t bkcnt = fnx_inode_getvcap(&reg->r_inode);
	return (fnx_off_t)fnx_nbk_to_nbytes(bkcnt);
}

static void reg_setbcap(fnx_reg_t *reg, fnx_off_t bcap)
{
	const fnx_bkcnt_t bkcnt = fnx_bytes_to_nbk((fnx_size_t)bcap);
	fnx_inode_setvcap(&reg->r_inode, bkcnt);
}

static void reg_setsize(fnx_reg_t *reg, fnx_off_t off)
{
	fnx_inode_setsize(&reg->r_inode, off);
}

static void reg_refresh(fnx_reg_t *reg, fnx_bool_t mc)
{
	const fnx_flags_t flags = mc ? FNX_AMCTIME_NOW : FNX_ATIME_NOW;
	fnx_inode_setitime(&reg->r_inode, flags);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void vproc_trigger(fnx_vproc_t *vproc, fnx_vnode_t *vnode)
{
	if ((vnode != NULL) && !vnode->v_pseudo) {
		fnx_assert(vnode->v_placed || vnode->v_expired);
		fnx_vproc_put_vnode(vproc, vnode);
	}
}

static void vproc_trigger_reg(fnx_vproc_t *vproc, fnx_reg_t *reg)
{
	vproc_trigger(vproc, &reg->r_inode.i_vnode);
}

static void vproc_trigger_reg_regseg(fnx_vproc_t *vproc,
                                     fnx_reg_t *reg, fnx_regseg_t *regseg)
{
	if (regseg != NULL) {
		vproc_trigger(vproc, &regseg->rs_vnode);
	}
	vproc_trigger_reg(vproc, reg);
}

static void vproc_trigger_reg_regsec(fnx_vproc_t *vproc,
                                     fnx_reg_t *reg, fnx_regsec_t *regsec)
{
	if (regsec != NULL) {
		vproc_trigger(vproc, &regsec->rc_vnode);
	}
	vproc_trigger_reg(vproc, reg);
}

static void vproc_expire(fnx_vproc_t *vproc, fnx_vnode_t *vnode)
{
	if (vnode != NULL) {
		vnode->v_expired = FNX_TRUE;
		fnx_vproc_put_vnode(vproc, vnode);
	}
}

static int require_mutable(const fnx_vnode_t *vnode)
{
	return fnx_vnode_ismutable(vnode) ? 0 : FNX_EPEND;
}

static int require_mutable2(const fnx_regsec_t *regsec,
                            const fnx_regseg_t *regseg)
{
	int rc1, rc2;

	rc1 = (regsec != NULL) ? require_mutable(&regsec->rc_vnode) : 0;
	rc2 = (regseg != NULL) ? require_mutable(&regseg->rs_vnode) : 0;
	return ((rc1 != 0) ? rc1 : rc2);
}

static int vproc_fetch_vbk(fnx_vproc_t *vproc,
                           const fnx_vaddr_t *vba, fnx_vnode_t **vbk)
{
	int rc;

	rc = fnx_vproc_fetch_vnode(vproc, vba, vbk);
	if (rc != 0) {
		return rc;
	}
	rc = require_mutable(*vbk);
	if (rc != 0) {
		return rc;
	}
	return 0;
}

static void vproc_refresh_reg(fnx_vproc_t *vproc, fnx_reg_t *reg, fnx_bool_t mc)
{
	reg_refresh(reg, mc);
	vproc_trigger_reg(vproc, reg);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int vproc_acquire_regseg(fnx_vproc_t         *vproc,
                                fnx_reg_t           *reg,
                                const fnx_lrange_t  *lrange,
                                fnx_regseg_t       **regseg)
{
	int rc;
	fnx_vaddr_t vaddr;
	fnx_vnode_t *vnode  = NULL;
	const fnx_off_t off = lrange->off;

	fnx_vaddr_for_regseg(&vaddr, reg_getino(reg), off);
	rc  = fnx_vproc_acquire_vvnode(vproc, &vaddr, &vnode);
	if (rc != 0) {
		return rc;
	}
	*regseg = fnx_vnode_to_regseg(vnode);
	fnx_regseg_setup(*regseg, off);
	return 0;
}

static int vproc_acquire_regsec(fnx_vproc_t         *vproc,
                                fnx_reg_t           *reg,
                                const fnx_lrange_t  *lrange,
                                fnx_regsec_t       **regsec)
{
	int rc;
	fnx_vaddr_t vaddr;
	fnx_vnode_t *vnode = NULL;
	const fnx_off_t off = lrange->off;

	fnx_vaddr_for_regsec(&vaddr, reg_getino(reg), off);
	rc  = fnx_vproc_acquire_vvnode(vproc, &vaddr, &vnode);
	if (rc != 0) {
		return rc;
	}
	*regsec = fnx_vnode_to_regsec(vnode);
	return 0;
}

static int vproc_fetch_regsec(fnx_vproc_t         *vproc,
                              fnx_reg_t           *reg,
                              const fnx_lrange_t  *lrange,
                              fnx_regsec_t       **regsec)
{
	int rc;
	fnx_vaddr_t vaddr;
	fnx_vnode_t *vnode = NULL;
	const fnx_off_t off = lrange->off;

	fnx_vaddr_for_regsec(&vaddr, reg_getino(reg), off);
	rc = fnx_vproc_fetch_vnode(vproc, &vaddr, &vnode);
	if (rc != 0) {
		return rc;
	}
	rc = require_mutable(vnode);
	if (rc != 0) {
		return rc;
	}
	*regsec = fnx_vnode_to_regsec(vnode);
	return 0;
}

static int vproc_fetch_regseg(fnx_vproc_t         *vproc,
                              fnx_reg_t           *reg,
                              const fnx_lrange_t  *lrange,
                              fnx_regseg_t       **p_regseg)
{
	int rc;
	fnx_vaddr_t vaddr;
	fnx_vnode_t  *vnode  = NULL;
	fnx_regseg_t *regseg = NULL;
	const fnx_off_t  off = lrange->off;

	fnx_vaddr_for_regseg(&vaddr, reg_getino(reg), off);
	rc = fnx_vproc_fetch_vnode(vproc, &vaddr, &vnode);
	if (rc != 0) {
		return rc;
	}
	rc = require_mutable(vnode);
	if (rc != 0) {
		return rc;
	}
	regseg = fnx_vnode_to_regseg(vnode);
	*p_regseg = regseg;
	return 0;
}

static int vproc_tryfetch_secseg(fnx_vproc_t         *vproc,
                                 fnx_reg_t           *reg,
                                 const fnx_lrange_t  *lrange,
                                 fnx_regsec_t       **regsec,
                                 fnx_regseg_t       **regseg)
{
	int rc;
	const fnx_off_t off = lrange->off;

	if (lrange_isseg0(lrange)) {
		*regsec = NULL;
		*regseg = NULL;
	} else if (lrange_issec0(lrange)) {
		if (!fnx_reg_testseg(reg, off)) {
			return FNX_ENOREGS;
		}
		rc = vproc_fetch_regseg(vproc, reg, lrange, regseg);
		if (rc != 0) {
			return rc;
		}
		*regsec = NULL;
	} else {
		if (!fnx_reg_testsec(reg, off)) {
			return FNX_ENOREGS;
		}
		rc = vproc_fetch_regsec(vproc, reg, lrange, regsec);
		if (rc != 0) {
			return rc;
		}
		if (!fnx_regsec_testseg(*regsec, off)) {
			return FNX_ENOREGS;
		}
		rc = vproc_fetch_regseg(vproc, reg, lrange, regseg);
		if (rc != 0) {
			return rc;
		}
	}
	rc = require_mutable2(*regsec, *regseg);
	if (rc != 0) {
		return rc;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int vproc_yield_regsec(fnx_vproc_t         *vproc,
                              fnx_reg_t           *reg,
                              const fnx_lrange_t  *lrange,
                              fnx_regsec_t       **p_regsec)
{
	int rc;
	fnx_regsec_t *regsec = NULL;
	const fnx_off_t  off = lrange->off;
	const fnx_size_t len = lrange->len;

	if (fnx_reg_testsec(reg, off)) {
		/* Case 1: Node exists -- fetch it */
		rc = vproc_fetch_regsec(vproc, reg, lrange, &regsec);
		if (rc != 0) {
			return rc;
		}
		rc = require_mutable(&regsec->rc_vnode);
		if (rc != 0) {
			return rc;
		}
	} else {
		/* Case 2: Create new regsec and update reg's head node */
		rc = vproc_acquire_regsec(vproc, reg, lrange, &regsec);
		if (rc != 0) {
			return rc;
		}
		fnx_reg_marksec(reg, off);
		fnx_reg_wmore(reg, fnx_off_end(off, len), 0, 1);
		vproc_trigger_reg(vproc, reg);
	}
	*p_regsec = regsec;
	return 0;
}

static int vproc_yield_regseg0(fnx_vproc_t         *vproc,
                               fnx_reg_t           *reg,
                               const fnx_lrange_t  *lrange,
                               fnx_regseg_t       **p_regseg)
{
	int rc;
	fnx_regseg_t *regseg = NULL;
	const fnx_off_t  off = lrange->off;
	const fnx_size_t len = lrange->len;

	if (fnx_reg_testseg(reg, off)) {
		/* Case 1: Node exists -- fetch it */
		rc = vproc_fetch_regseg(vproc, reg, lrange, &regseg);
		if (rc != 0) {
			return rc;
		}
		rc = require_mutable(&regseg->rs_vnode);
		if (rc != 0) {
			return rc;
		}
	} else {
		/* Case 2: Create new regseg and update reg's head node*/
		rc = vproc_acquire_regseg(vproc, reg, lrange, &regseg);
		if (rc != 0) {
			return rc;
		}
		fnx_reg_markseg(reg, off);
		if (!fnx_reg_testsec(reg, off)) {
			fnx_reg_marksec(reg, off); /* Case sec0 */
		}
		fnx_reg_wmore(reg, fnx_off_end(off, len), 0, 1);
		vproc_trigger_reg(vproc, reg);
	}
	*p_regseg = regseg;
	return 0;
}

static int vproc_yield_regseg(fnx_vproc_t         *vproc,
                              fnx_reg_t           *reg,
                              fnx_regsec_t        *regsec,
                              const fnx_lrange_t  *lrange,
                              fnx_regseg_t       **p_regseg)
{
	int rc;
	fnx_regseg_t *regseg = NULL;
	const fnx_off_t  off = lrange->off;
	const fnx_size_t len = lrange->len;

	if (fnx_regsec_testseg(regsec, off)) {
		/* Node exists -- fetch it */
		rc = vproc_fetch_regseg(vproc, reg, lrange, &regseg);
		if (rc != 0) {
			return rc;
		}
		rc = require_mutable(&regseg->rs_vnode);
		if (rc != 0) {
			return rc;
		}
	} else {
		/* Create new node */
		rc = vproc_acquire_regseg(vproc, reg, lrange, &regseg);
		if (rc != 0) {
			return rc;
		}
		fnx_regsec_markseg(regsec, off);
		fnx_reg_wmore(reg, fnx_off_end(off, len), 0, 1);
		vproc_trigger_reg_regsec(vproc, reg, regsec);
	}
	*p_regseg = regseg;
	return 0;
}

static int vproc_require_secseg(fnx_vproc_t         *vproc,
                                fnx_reg_t           *reg,
                                const fnx_lrange_t  *lrange,
                                fnx_regsec_t       **regsec,
                                fnx_regseg_t       **regseg)
{
	int rc;

	if (lrange_isseg0(lrange)) {
		*regseg = NULL;
		*regsec = NULL;
	} else if (lrange_issec0(lrange)) {
		rc = vproc_yield_regseg0(vproc, reg, lrange, regseg);
		if (rc != 0) {
			return rc;
		}
		*regsec = NULL;
	} else {
		rc = vproc_yield_regsec(vproc, reg, lrange, regsec);
		if (rc != 0) {
			return rc;
		}
		rc = vproc_yield_regseg(vproc, reg, *regsec, lrange, regseg);
		if (rc != 0) {
			return rc;
		}
	}
	return 0;
}

/*
 * NB: A trivial optimization would be to fetch only partial vbks. However,
 * we need the vbk entry for proper cleanup upon post-vop. Need improvements..
 */
static int vproc_stage_vbks(fnx_vproc_t        *vproc,
                            fnx_reg_t          *reg,
                            fnx_regseg_t       *regseg,
                            const fnx_lrange_t *lrange)
{
	int rc;
	fnx_vnode_t *vbk;
	const fnx_vaddr_t *vba;

	for (size_t pos, i = 0; i < lrange->cnt; ++i) {
		pos = lrange->idx + i;
		if (lrange_isseg0(lrange)) {
			vba = &reg->r_segmap0.vba[pos];
		} else {
			vba = &regseg->rs_segmap.vba[pos];
		}
		if (vba_isnull(vba)) {
			continue;
		}
		rc = vproc_fetch_vbk(vproc, vba, &vbk);
		if (rc != 0) {
			return rc;
		}
	}
	fnx_unused(reg);
	return 0;
}

static int vproc_prepare_write(fnx_vproc_t  *vproc,
                               fnx_reg_t    *reg,
                               fnx_iobufs_t *iobufs)
{
	int rc;
	fnx_size_t cnt;
	fnx_iobuf_t  *iobuf;
	fnx_regsec_t *regsec = NULL;
	fnx_regseg_t *regseg = NULL;
	const fnx_lrange_t *lrange;
	fnx_vaddr_t vbas[FNX_IOBUFSNBK];

	cnt = fnx_iobufs_cnt(iobufs);
	fnx_assert(cnt <= fnx_nelems(vbas));
	rc = fnx_vproc_predict_next_vba(vproc, vbas, cnt);
	if (rc != 0) {
		return rc;
	}
	for (size_t i = 0; hasiob(iobufs, i); ++i) {
		iobuf  = &iobufs->iob[i];
		lrange = &iobuf->rng;
		rc = vproc_tryfetch_secseg(vproc, reg, lrange, &regsec, &regseg);
		if (rc == FNX_ENOREGS) {
			continue; /* No-mapping -- OK */
		}
		if (rc != 0) {
			return rc;
		}
		rc = vproc_stage_vbks(vproc, reg, regseg, &iobuf->rng);
		if (rc != 0) {
			return rc;
		}
	}
	for (size_t j = 0; hasiob(iobufs, j); ++j) {
		iobuf  = &iobufs->iob[j];
		lrange = &iobuf->rng;
		rc = vproc_require_secseg(vproc, reg, lrange, &regsec, &regseg);
		if (rc != 0) {
			return rc;
		}
	}
	return 0;
}

/* Creates new vbk mapping */
static int vproc_map_new_vbk(fnx_vproc_t        *vproc,
                             fnx_bkref_t        *bkref,
                             fnx_reg_t          *reg,
                             fnx_regseg_t       *regseg,
                             const fnx_lrange_t *lrange,
                             fnx_bool_t          rewrite,
                             fnx_vaddr_t        *vba,
                             fnx_vnode_t       **vbk)
{
	int rc;
	fnx_ino_t ino;
	fnx_off_t end;
	const fnx_xno_t xno = FNX_XNO_ANY;
	const fnx_vtype_e vtype = FNX_VTYPE_VBK;

	/* Acquire new vbk + associate mapping */
	ino = fnx_super_getino(vproc->super);
	rc = fnx_vproc_acquire_vnode(vproc, vtype, ino, xno, bkref, vbk);
	if (rc != 0) {
		return rc;
	}
	fnx_vaddr_copy(vba, &((*vbk)->v_vaddr));
	fnx_assert(!vba_isnull(vba));

	/* Update accounting */
	end = fnx_lrange_end(lrange);
	fnx_reg_wmore(reg, end, rewrite ? 0 : 1, 0);
	vproc_trigger_reg_regseg(vproc, reg, regseg);

	return 0;
}

/* Merge user's data when partial and expire obsolete vbk*/
static void vproc_merge_expire_vbk(fnx_vproc_t  *vproc,
                                   fnx_vnode_t  *vbk_new,
                                   fnx_vnode_t  *vbk_cur,
                                   const fnx_lrange_t *lrange)
{
	if (vbk_cur != NULL) {
		if (lrange_issubblk(lrange)) {
			fnx_bkref_merge2(vbk_new->v_bkref, vbk_cur->v_bkref, lrange);
		}
		vproc_expire(vproc, vbk_cur);
	}
}

/*
 * Write user's data, blocks granularity. Merge with existing data if partial.
 *
 * NB: No real need to fetch existing vbk if not partial. However, we do so to
 * simplify space reclamation + avoid re-use of same block in this iteration.
 * Need to be improved.
 */
static int vproc_write_regseg(fnx_vproc_t  *vproc,
                              fnx_reg_t    *reg,
                              fnx_regseg_t *regseg,
                              fnx_iobuf_t  *iobuf)
{
	int rc;
	fnx_bool_t rewrite;
	fnx_size_t len = 0;
	fnx_off_t off, end, piv;
	fnx_lrange_t srange;
	fnx_vaddr_t *vba;
	fnx_bkref_t *bkref;
	fnx_vnode_t *vbk_cur, *vbk_new;
	fnx_segmap_t *segmap;
	const fnx_lrange_t *lrange = &iobuf->rng;

	if (lrange_isseg0(lrange)) {
		segmap = &reg->r_segmap0;
	} else {
		segmap = &regseg->rs_segmap;
	}

	end = fnx_off_end(lrange->off, lrange->len);
	off = lrange->off;
	for (size_t pos, i = 0; (i < lrange->cnt); ++i) {
		off = fnx_off_end(off, len);
		pos = i + lrange->idx;
		vba = &segmap->vba[pos];
		piv = fnx_off_ceil_blk(off);
		len = fnx_off_min_len(off, end, piv);

		vbk_cur = NULL;
		rewrite = !vba_isnull(vba);
		bkref   = iobuf->bks[pos];
		if (bkref == NULL) {
			/* Ignore possible previous iterations */
			continue;
		}

		rc = rewrite ? vproc_fetch_vbk(vproc, vba, &vbk_cur) : 0;
		if (rc != 0) {
			return rc;
		}

		/* Create new mapping with user's data + own bkref */
		fnx_lrange_setup(&srange, off, len);
		rc = vproc_map_new_vbk(vproc, bkref, reg, regseg,
		                       &srange, rewrite, vba, &vbk_new);
		if (rc != 0) {
			return rc;
		}

		/* Merge new with current data and expire obsolete */
		vproc_merge_expire_vbk(vproc, vbk_new, vbk_cur, &srange);

		/* Remove iobuf ownership */
		iobuf->bks[pos] = NULL;
	}
	return 0;
}

static int vproc_write_reg(fnx_vproc_t  *vproc,
                           fnx_reg_t    *reg,
                           fnx_iobufs_t *iobufs)
{
	int rc;
	fnx_iobuf_t  *iobuf;
	fnx_regsec_t *regsec = NULL;
	fnx_regseg_t *regseg = NULL;
	const fnx_lrange_t *lrange;

	for (size_t j = 0; hasiob(iobufs, j); ++j) {
		iobuf  = &iobufs->iob[j];
		lrange = &iobuf->rng;
		rc = vproc_require_secseg(vproc, reg, lrange, &regsec, &regseg);
		if (rc != 0) {
			return rc;
		}
		rc = vproc_write_regseg(vproc, reg, regseg, iobuf);
		if (rc != 0) {
			return rc;
		}
	}
	return 0;
}

static int vproc_write_data(fnx_vproc_t        *vproc,
                            fnx_reg_t          *reg,
                            fnx_iobufs_t       *iobufs,
                            const fnx_lrange_t *lrange)
{
	int rc;

	rc = vproc_prepare_write(vproc, reg, iobufs);
	if (rc != 0) {
		return rc;
	}
	rc  = vproc_write_reg(vproc, reg, iobufs);
	if (rc != 0) {
		return rc;
	}
	vproc_refresh_reg(vproc, reg, lrange->len > 0);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int vproc_falloc_range(fnx_vproc_t        *vproc,
                              fnx_reg_t          *reg,
                              const fnx_lrange_t *lrange,
                              fnx_bool_t          keep_size)
{
	int rc;
	fnx_size_t slen;
	fnx_off_t off, end, send, piv;
	fnx_lrange_t srange;
	fnx_regsec_t *regsec = NULL;
	fnx_regseg_t *regseg = NULL;

	end = fnx_lrange_end(lrange);
	for (off = lrange->off; off < end; off = piv) {
		piv  = fnx_off_ceil_rseg(off);
		slen = fnx_off_min_len(off, piv, end);
		fnx_lrange_setup(&srange, off, slen);
		rc = vproc_require_secseg(vproc, reg, &srange, &regsec, &regseg);
		if (rc != 0) {
			return rc;
		}
		send = fnx_lrange_end(&srange);
		fnx_reg_wmore(reg, send, 0, keep_size);
		vproc_trigger_reg(vproc, reg);
	}
	return 0;
}

int fnx_vproc_exec_falloc(fnx_vproc_t *vproc,
                          fnx_task_t  *task,
                          fnx_off_t    off,
                          fnx_size_t   len,
                          fnx_bool_t   keepsz)
{
	int rc;
	fnx_reg_t *reg;
	fnx_lrange_t lrange;
	const fnx_fileref_t *fref = getfref(task);

	reg = fnx_inode_to_reg(fref->f_inode);
	if (reg_ispseudo(reg)) {
		return -ENOTSUP; /* TODO: No op? */
	}
	fnx_lrange_setup(&lrange, off, len);
	rc = vproc_falloc_range(vproc, reg, &lrange, keepsz);
	if (rc != 0) {
		return rc;
	}
	vproc_refresh_reg(vproc, reg, len > 0);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Trim-out mapped segment of v-blocks, and update accounting.
 *
 * NB: We have a costly fetch-vbk here. A trivial solution would be to simply
 * (evict-)drop-discard vaddr, but we need a vnode-objects for later queues.
 * TODO: Optimize me.
 */
static int vproc_trim_regseg(fnx_vproc_t        *vproc,
                             fnx_reg_t          *reg,
                             fnx_regseg_t       *regseg,
                             fnx_regsec_t       *regsec,
                             const fnx_lrange_t *lrange)
{
	int rc, isseg0;
	fnx_off_t wsz;
	fnx_vaddr_t  *vba;
	fnx_vnode_t  *vbk;
	fnx_segmap_t *segmap;

	isseg0 = lrange_isseg0(lrange);
	segmap = isseg0 ? &reg->r_segmap0 : &regseg->rs_segmap;

	wsz = reg_getsize(reg);
	for (size_t pos, i = 0; i < lrange->cnt; ++i) {
		pos = lrange->idx + i;
		vba = &segmap->vba[pos];
		if (vba_isnull(vba)) {
			continue;
		}
		rc = vproc_fetch_vbk(vproc, vba, &vbk);
		if (rc != 0) {
			return rc;
		}
		fnx_vaddr_setnull(vba);
		vproc_expire(vproc, vbk);
		fnx_reg_wless(reg, wsz, 1, 1);
	}
	fnx_unused(regsec);
	return 0;
}

static void vproc_settle_trimed(fnx_vproc_t        *vproc,
                                fnx_reg_t          *reg,
                                fnx_regseg_t       *regseg,
                                fnx_regsec_t       *regsec,
                                const fnx_lrange_t *lrange)
{
	int isseg0, issec0;
	fnx_off_t off;

	off =  lrange->off;
	isseg0 = lrange_isseg0(lrange);
	issec0 = lrange_issec0(lrange);

	if (isseg0 || !fnx_regseg_isempty(regseg)) {
		vproc_trigger_reg_regseg(vproc, reg, regseg);
	} else if (issec0) {
		fnx_reg_unmarkseg(reg, off);
		if (!reg->r_nsegs) {
			fnx_reg_unmarksec(reg, off);
		}
		vproc_expire(vproc, &regseg->rs_vnode);
	} else {
		fnx_regsec_unmarkseg(regsec, off);
		if (regsec && !regsec->rc_nsegs) {
			fnx_reg_unmarksec(reg, off);
			vproc_expire(vproc, &regsec->rc_vnode);
		}
		vproc_expire(vproc, &regseg->rs_vnode);
	}
}

static int vproc_trim_data(fnx_vproc_t        *vproc,
                           fnx_reg_t          *reg,
                           const fnx_lrange_t *lrange)
{
	int rc;
	fnx_size_t slen;
	fnx_off_t off, end, piv;
	fnx_lrange_t srange;
	fnx_regsec_t *regsec = NULL;
	fnx_regseg_t *regseg = NULL;

	end = fnx_lrange_end(lrange);
	for (off = lrange->off; off < end; off = piv) {
		piv  = fnx_off_ceil_rseg(off);
		slen = fnx_off_min_len(off, piv, end);

		fnx_lrange_setup(&srange, off, slen);
		if (!lrange_isseg0(&srange) && !fnx_reg_testsec(reg, off)) {
			piv  = fnx_off_ceil_rsec(off);
			continue; /* Sparse -- skip entire section */
		}
		rc = vproc_tryfetch_secseg(vproc, reg, &srange, &regsec, &regseg);
		if (rc == FNX_ENOREGS) {
			continue; /* No-mapping -- OK */
		}
		if (rc != 0) {
			return rc;
		}
		rc = vproc_trim_regseg(vproc, reg, regseg, regsec, &srange);
		if (rc != 0) {
			return rc;
		}
		vproc_settle_trimed(vproc, reg, regseg, regsec, &srange);
	}
	return 0;
}

static int vproc_punch_data(fnx_vproc_t       *vproc,
                            fnx_reg_t          *reg,
                            const fnx_lrange_t *lrange)
{
	int rc;
	fnx_lrange_t srange;
	fnx_off_t xbeg, xend;
	fnx_off_t beg, end, bend;

	bend = reg_getbcap(reg);
	beg  = fnx_lrange_begin(lrange);
	end  = fnx_off_min(fnx_lrange_end(lrange), bend);
	if (beg >= bend) {
		return 0; /* Punch past end of mapped-region; no-op */
	}
	/* Having [xbeg,xend) at blocks boundaries (no partial punch) */
	xbeg = fnx_off_xceil_blk(beg);
	xend = fnx_off_floor_blk(end);
	if (xend <= xbeg) {
		return 0; /* Zero-blocks to trim; no-op */
	}
	/* Do trim data */
	fnx_lrange_rsetup(&srange, xbeg, xend);
	rc = vproc_trim_data(vproc, reg, &srange);
	if (rc != 0) {
		return rc;
	}
	/* Update reg-file's volume-capacity */
	if ((xbeg <= bend) && (bend < xend)) {
		reg_setbcap(reg, xbeg);
	}
	return 0;
}

int fnx_vproc_trunc_data(fnx_vproc_t *vproc, fnx_reg_t *reg, fnx_off_t off)
{
	int rc;
	fnx_lrange_t lrange;
	fnx_off_t cur, beg, end;

	cur = reg_getsize(reg);
	if (off < cur) {
		beg = fnx_off_xceil_blk(off);
		end = fnx_off_ceil_blk(cur);
		fnx_lrange_rsetup(&lrange, beg, end);
		rc  = vproc_punch_data(vproc, reg, &lrange);
		if (rc != 0) {
			return rc;
		}
	}
	reg_setsize(reg, off);
	vproc_refresh_reg(vproc, reg, 1);
	return 0;
}

/*
 * Punch 'hole' within file's logical space, at block boundaries. Following the
 * semantics of fallocate(2)'s man page, which states that "within the
 * specified range, partial filesystem blocks are zeroed, and whole filesystem
 * blocks are removed from the file."
 */
int fnx_vproc_exec_punch(fnx_vproc_t *vproc, fnx_task_t *task,
                         fnx_off_t off, fnx_size_t len)
{
	int rc;
	fnx_reg_t *reg;
	fnx_lrange_t srange;
	fnx_off_t sbeg, send;
	const fnx_off_t end = fnx_off_end(off, len);
	const fnx_fileref_t *fref = getfref(task);

	reg = fnx_inode_to_reg(fref->f_inode);
	if (reg_ispseudo(reg)) {
		rc = fnx_vproc_punch_pseudo(vproc, task, off, len);
	} else {
		sbeg = fnx_off_floor_blk(off);
		send = fnx_off_ceil_blk(end);
		fnx_lrange_rsetup(&srange, sbeg, send);
		rc  = vproc_punch_data(vproc, reg, &srange);
	}
	if (rc != 0) {
		return rc;
	}
	vproc_refresh_reg(vproc, reg, len > 0);
	return 0;
}

int fnx_vproc_exec_trunc(fnx_vproc_t *vproc, fnx_task_t *task, fnx_off_t off)
{
	int rc;
	fnx_reg_t *reg;
	const fnx_fileref_t *fref = getfref(task);

	reg = fnx_inode_to_reg(fref->f_inode);
	if (reg_ispseudo(reg)) {
		rc = fnx_vproc_trunc_pseudo(vproc, task, off);
	} else {
		rc = fnx_vproc_trunc_data(vproc, reg, off);
	}
	if (rc != 0) {
		return rc;
	}
	fnx_vproc_setiattr_size(vproc, &reg->r_inode, &task->tsk_uctx, off);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void vproc_relax_iobufs(fnx_vproc_t  *vproc, fnx_iobufs_t *iobufs)
{
	size_t cnt = 0;
	fnx_bkref_t *bkref;
	fnx_bkref_t *bkrefs[FNX_IOBUFSNBK];
	const size_t arrsz = FNX_NELEMS(bkrefs);

	fnx_iobufs_decrefs(iobufs);
	fnx_iobufs_relax(iobufs, bkrefs, arrsz, &cnt);

	for (size_t i = 0; i < cnt; ++i) {
		bkref = bkrefs[i];
		if (bkref->bk_refcnt == 0) {
			fnx_assert(!bkref->bk_cached);
			fnx_bkref_detach(bkref);
			fnx_alloc_del_bk(vproc->alloc, bkref);
		}
	}
}

static int vproc_read_regseg(fnx_vproc_t  *vproc,
                             fnx_reg_t    *reg,
                             fnx_regseg_t *regseg,
                             fnx_iobuf_t  *iobuf)
{
	int rc;
	fnx_bkref_t *bkref;
	fnx_vnode_t *vbk;
	const fnx_vaddr_t  *vba;
	const fnx_lrange_t *lrange;
	const fnx_segmap_t *segmap;

	lrange = &iobuf->rng;
	if (lrange_isseg0(lrange)) {
		segmap = &reg->r_segmap0;
	} else {
		segmap = &regseg->rs_segmap;
	}

	for (size_t pos, i = 0; i < lrange->cnt; ++i) {
		pos = lrange->idx + i;
		vba = &segmap->vba[pos];
		if (vba_isnull(vba)) {
			continue;
		}
		rc = vproc_fetch_vbk(vproc, vba, &vbk);
		if (rc != 0) {
			return rc;
		}
		bkref = vbk->v_bkref;
		fnx_assert(bkref != NULL);
		fnx_assert(bkref->bk_magic == FNX_BKREF_MAGIC);

		iobuf->bks[pos] = bkref;
		bkref->bk_refcnt += 1;
	}
	return 0;
}

static int vproc_read_reg(fnx_vproc_t  *vproc,
                          fnx_reg_t    *reg,
                          fnx_iobufs_t *iobufs)
{
	int rc;
	fnx_iobuf_t *iobuf;
	fnx_regsec_t *regsec = NULL;
	fnx_regseg_t *regseg = NULL;
	const fnx_lrange_t *lrange;

	for (size_t i = 0; hasiob(iobufs, i); ++i) {
		iobuf  = &iobufs->iob[i];
		lrange = &iobuf->rng;
		rc = vproc_tryfetch_secseg(vproc, reg, lrange, &regsec, &regseg);
		if (rc == FNX_ENOREGS) {
			continue; /* No-mapping -- OK */
		}
		if (rc != 0) {
			return rc;
		}
		rc = vproc_read_regseg(vproc, reg, regseg, iobuf);
		if (rc != 0) {
			return rc;
		}
	}
	return 0;
}

static int vproc_read_data(fnx_vproc_t  *vproc,
                           fnx_reg_t    *reg,
                           fnx_iobufs_t *iobufs,
                           fnx_off_t     off,
                           fnx_size_t    len)
{
	int rc = 0;
	fnx_off_t end;
	fnx_off_t size;

	size = reg_getsize(reg);
	if (off >= size) {
		goto out_ok;
	}
	end = fnx_off_end(off, len);
	len = fnx_off_min_len(off, size, end);
	rc = fnx_iobufs_assign(iobufs, off, len, NULL);
	if (rc != 0) {
		return -EINVAL;
	}
	rc = vproc_read_reg(vproc, reg, iobufs);
	if (rc != 0) {
		vproc_relax_iobufs(vproc, iobufs); /* XXX ? is it? */
		return rc;
	}

out_ok:
	/* Update atime only in cache */
	reg_refresh(reg, 0);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_vproc_exec_open(fnx_vproc_t *vproc,
                        fnx_task_t  *task,
                        fnx_flags_t  o_flags)
{
	int rc = 0;
	fnx_off_t size;
	fnx_flags_t mask;
	fnx_reg_t   *reg;
	fnx_inode_t *inode;
	const fnx_fileref_t *fref = getfref(task);

	inode = fref->f_inode;
	mask = O_TRUNC;
	if (!(o_flags & mask)) {
		return 0;  /* OK, no trunc upon open */
	}
	mask = (O_RDWR | O_WRONLY);
	if (!(o_flags & mask)) {
		return 0; /* O_TRUNC requires O_RDWR or O_WRONLY */
	}
	size = fnx_inode_getsize(inode);
	if (size == 0) {
		return 0;  /* Nothing to do for zero size */
	}
	if (inode_ispseudo(inode)) {
		return 0; /* Nothing to do for pseudo inode */
	}
	/* Truncate upon open */
	reg = fnx_inode_to_reg(inode);
	rc = fnx_vproc_trunc_data(vproc, reg, 0);
	if (rc != 0) {
		return rc; /* Err */
	}
	vproc_refresh_reg(vproc, reg, 0);
	return 0;
}

static int has_noatime(const fnx_vproc_t *vproc, const fnx_fileref_t *fref)
{
	return (fref->f_noatime || (vproc->mntf & FNX_MNTF_NOATIME));
}

int fnx_vproc_exec_read(fnx_vproc_t *vproc, fnx_task_t *task,
                        fnx_off_t off, fnx_size_t len)
{
	int rc;
	const fnx_fileref_t *fref = getfref(task);

	fnx_reg_t *reg = fnx_inode_to_reg(fref->f_inode);
	if (reg_ispseudo(reg)) {
		rc = fnx_vproc_read_pseudo(vproc, task, off, len);
	} else {
		rc = vproc_read_data(vproc, reg, &task->tsk_iobufs, off, len);
	}
	if ((rc == 0) && !has_noatime(vproc, fref)) {
		/* Ugly atime: force committing inode to stable storage due to read */
		vproc_trigger_reg(vproc, reg);
	}
	return rc;
}

int fnx_vproc_exec_write(fnx_vproc_t *vproc, fnx_task_t *task,
                         fnx_off_t off, fnx_size_t len)
{
	int rc;
	fnx_reg_t *reg;
	fnx_lrange_t lrange;
	const fnx_fileref_t *fref = getfref(task);

	reg = fnx_inode_to_reg(fref->f_inode);
	if (reg_ispseudo(reg)) {
		rc = fnx_vproc_write_pseudo(vproc, task, off, len);
		/* Allows proper cleanup post pseudo-write */
		fnx_iobufs_increfs(&task->tsk_iobufs);
		fnx_vproc_relax_iobufs(vproc, task);
	} else {
		fnx_lrange_setup(&lrange, off, len);
		rc = vproc_write_data(vproc, reg, &task->tsk_iobufs, &lrange);
	}
	if (rc != 0) {
		return rc;
	}
	return 0;
}

int fnx_vproc_exec_fsync(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_inode_t *inode;
	const fnx_fileref_t *fref = getfref(task);

	inode = fref->f_inode;
	if (!inode_ispseudo(inode)) {
		rc = fnx_pstor_sync_vnode(vproc->pstor, &inode->i_vnode);
	}
	return rc;
}

void fnx_vproc_relax_iobufs(fnx_vproc_t *vproc, fnx_task_t *task)
{
	vproc_relax_iobufs(vproc, &task->tsk_iobufs);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int reg_need_implicit_cleanup(const fnx_reg_t *reg)
{
	const fnx_vnode_t *vnode = &reg->r_inode.i_vnode;
	const fnx_nlink_t  nlink = reg->r_inode.i_iattr.i_nlink;

	return (!vnode->v_pseudo && vnode->v_expired &&
	        (vnode->v_refcnt <= 1) && (nlink == FNX_INODE_INIT_NLINK));
}

int fnx_vproc_exec_release(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc;
	fnx_reg_t *reg;
	fnx_inode_t *inode;
	fnx_fileref_t *fref;

	fref  = task->tsk_fref;
	inode = fref->f_inode;
	reg   = fnx_inode_to_reg(inode);
	if (reg_need_implicit_cleanup(reg)) {
		rc = fnx_vproc_exec_trunc(vproc, task, 0);
		if (rc != 0) {
			return rc;
		}
		rc = fnx_vproc_fix_unlinked(vproc, &reg->r_inode);
		if (rc != 0) {
			return rc;
		}
	}
	fnx_vproc_untie_fileref(vproc, fref);
	/* TODO: Fix this code-logic as well as other places */
	if (inode->i_vnode.v_expired) {
		rc = fnx_vproc_fix_unlinked(vproc, inode);
		fnx_assert(rc == 0);
	}

	task->tsk_fref = NULL;
	return 0;
}

int fnx_vproc_exec_forget(fnx_vproc_t *vproc, fnx_task_t *task, fnx_ino_t ino)
{
	int rc;
	fnx_reg_t *reg;
	fnx_inode_t *inode = NULL;

	rc = fnx_vproc_fetch_cached_inode(vproc, ino, &inode);
	if (rc == FNX_ECACHEMISS) {
		return 0; /* OK, cache miss */
	}
	if (rc != 0) {
		return rc;
	}
	if (!fnx_inode_isreg(inode)) {
		return 0;  /* OK, not reg -- ignored */
	}
	reg = fnx_inode_to_reg(inode);
	if (reg_need_implicit_cleanup(reg)) {

		/* XXX: NO GOOD: Need to check asyn is ok! */

		rc = fnx_vproc_trunc_data(vproc, reg, 0);
		if (rc != 0) {
			return rc;
		}
	}
	fnx_unused(task);
	return 0;
}
