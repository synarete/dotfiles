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
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <fnxinfra.h>
#include "core-blobs.h"
#include "core-types.h"
#include "core-stats.h"
#include "core-proto.h"
#include "core-htod.h"
#include "core-addr.h"
#include "core-addri.h"
#include "core-elems.h"
#include "core-space.h"
#include "core-super.h"
#include "core-inode.h"
#include "core-inodei.h"
#include "core-file.h"

#define FNX_FREF_MAGIC          (0x2AF455EC)
#define FNX_SYMLNK_MAGIC        (0x78AACE13)
#define FNX_REG_MAGIC           (0x5AF4992B)


#define FNX_REG_INIT_MODE       (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define FNX_SYMLNK_INIT_MODE    (S_IFLNK | S_IRWXU | S_IRWXG | S_IRWXO)
#define FNX_SYMLNK_INIT_XMSK    (S_IRWXU | S_IRWXG | S_IRWXO)



/* Space bitmaps helper: converts seg/sec index to u32 bitsmask */
static uint32_t seg_slot_mask(fnx_off_t seg)
{
	fnx_assert(seg >= 0);
	return (1u << ((size_t)(seg % 32)));
}

static uint32_t sec_slot_mask(fnx_off_t sec)
{
	fnx_assert(sec >= 0);
	return (1u << ((size_t)(sec % 32)));
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_fileref_check(const fnx_fileref_t *fref)
{
	fnx_assert(fref != NULL);
	fnx_assert(fref->f_magic == FNX_FREF_MAGIC);
}

fnx_fileref_t *fnx_link_to_fileref(const fnx_link_t *lnk)
{
	const fnx_fileref_t *fref;

	fref = fnx_container_of(lnk, fnx_fileref_t, f_link);
	return (fnx_fileref_t *)fref;
}

void fnx_fileref_init(fnx_fileref_t *fref)
{
	fnx_bzero(fref, sizeof(*fref));

	fnx_link_init(&fref->f_link);
	fref->f_magic   = FNX_FREF_MAGIC;
	fref->f_inuse  = FNX_FALSE;
	fref->f_inode   = NULL;
}

void fnx_fileref_destroy(fnx_fileref_t *fref)
{
	fnx_link_destroy(&fref->f_link);
	fnx_bff(fref, sizeof(*fref));
}

static void fileref_set_flags(fnx_fileref_t *fref, fnx_flags_t flags)
{
	if ((flags & O_RDONLY) == O_RDONLY) {
		fref->f_readable = FNX_TRUE;
	}
	if ((flags & O_WRONLY) == O_WRONLY) {
		fref->f_writeable = FNX_TRUE;
	}
	if ((flags & O_RDWR) == O_RDWR) {
		fref->f_readable  = FNX_TRUE;
		fref->f_writeable = FNX_TRUE;
	}

	if ((flags & O_APPEND) == O_APPEND) {
		/* FIXME */
	}
	if ((flags & O_NOATIME) == O_NOATIME) {
		fref->f_noatime = FNX_TRUE;
	}
	if ((flags & O_SYNC) == O_SYNC) {
		fref->f_sync = FNX_TRUE;
	}
}

static void fileref_set_unique_id(fnx_fileref_t *fref)
{
	/* TODO: Make true unique identifier, crypto. Do not use uuid_generate; it
	 * is too-heavy.
	 */
	(void)fref;
}

void fnx_fileref_attach(fnx_fileref_t *fref, fnx_inode_t *inode,
                        fnx_flags_t flags)
{
	fnx_assert(inode->i_magic == FNX_INODE_MAGIC);

	fileref_set_flags(fref, flags);
	fileref_set_unique_id(fref);
	fref->f_inode = inode;
	inode->i_vnode.v_refcnt += 1;
}

fnx_inode_t *fnx_fileref_detach(fnx_fileref_t *fref)
{
	fnx_inode_t *inode;

	inode = fref->f_inode;
	fnx_assert(inode->i_magic == FNX_INODE_MAGIC);
	fnx_assert(inode->i_vnode.v_refcnt > 0);
	fref->f_inode = NULL;
	inode->i_vnode.v_refcnt -= 1;
	return inode;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Regular-file
 */
static fnx_dreg_t *fnx_dinode_to_dreg(const fnx_dinode_t *dinode)
{
	return fnx_container_of(dinode, fnx_dreg_t, r_inode);
}

fnx_dreg_t *fnx_header_to_dreg(const fnx_header_t *hdr)
{
	return fnx_dinode_to_dreg(fnx_header_to_dinode(hdr));
}

fnx_reg_t *fnx_inode_to_reg(const fnx_inode_t *inode)
{
	return fnx_container_of(inode, fnx_reg_t, r_inode);
}

fnx_reg_t *fnx_vnode_to_reg(const fnx_vnode_t *vnode)
{
	return fnx_inode_to_reg(fnx_vnode_to_inode(vnode));
}

void fnx_reg_init(fnx_reg_t *reg)
{
	fnx_inode_init(&reg->r_inode, FNX_VTYPE_REG);
	reg->r_inode.i_iattr.i_mode = FNX_REG_INIT_MODE;
	fnx_segmap_init(&reg->r_segmap0);
	fnx_segmap_setup(&reg->r_segmap0, 0, FNX_RSEGSIZE);
	fnx_bzero(reg->r_rsegs, sizeof(reg->r_rsegs));
	fnx_bzero(reg->r_rsecs, sizeof(reg->r_rsecs));
	reg->r_nsegs = 0;
	reg->r_nsecs = 0;
	reg->r_magic = FNX_REG_MAGIC;
}

void fnx_reg_destroy(fnx_reg_t *reg)
{
	fnx_inode_destroy(&reg->r_inode);
	fnx_segmap_destroy(&reg->r_segmap0);
	reg->r_magic = 8;
}

void fnx_reg_clone(const fnx_reg_t *reg, fnx_reg_t *other)
{
	fnx_inode_clone(&reg->r_inode, &other->r_inode);
	fnx_bcopy(other->r_rsegs, reg->r_rsegs, sizeof(other->r_rsegs));
	fnx_bcopy(other->r_rsecs, reg->r_rsecs, sizeof(other->r_rsecs));
	other->r_nsegs = reg->r_nsegs;
	other->r_nsecs = reg->r_nsecs;
}

void fnx_reg_htod(const fnx_reg_t *reg, fnx_dreg_t *dreg)
{
	size_t i;
	fnx_lba_t vlba;
	fnx_assert(reg->r_magic == FNX_REG_MAGIC);

	/* R[0..7] */
	fnx_inode_htod(&reg->r_inode, &dreg->r_inode);

	/* R[8..15] */
	for (i = 0; i < fnx_nelems(dreg->r_vlba_rseg0); ++i) {
		vlba = fnx_vaddr_getvlba(&reg->r_segmap0.vba[i]);
		dreg->r_vlba_rseg0[i] = fnx_htod_lba(vlba);
	}

	/* R[16..23] */
	for (i = 0; i < fnx_nelems(dreg->r_rsegs); ++i) {
		dreg->r_rsegs[i] = fnx_htod_u32(reg->r_rsegs[i]);
	}

	/* R[24..31] */
	for (i = 0; i < fnx_nelems(dreg->r_rsecs); ++i) {
		dreg->r_rsecs[i] = fnx_htod_u32(reg->r_rsecs[i]);
	}

	if (reg->r_nsegs > 0) {
		fnx_assert(reg->r_nsecs > 0);
	}
}

void fnx_reg_dtoh(fnx_reg_t *reg, const fnx_dreg_t *dreg)
{
	size_t i;
	fnx_assert(reg->r_magic == FNX_REG_MAGIC);

	/* R[0..7] */
	fnx_inode_dtoh(&reg->r_inode, &dreg->r_inode);

	/* R[8..15] */
	for (i = 0; i < fnx_nelems(reg->r_segmap0.vba); ++i) {
		fnx_vaddr_for_vbk(&reg->r_segmap0.vba[i], dreg->r_vlba_rseg0[i]);
	}

	/* R[16..23] */
	reg->r_nsegs = 0;
	for (i = 0; i < fnx_nelems(reg->r_rsegs); ++i) {
		reg->r_rsegs[i] = fnx_dtoh_u32(dreg->r_rsegs[i]);
		reg->r_nsegs += (size_t)fnx_popcount(reg->r_rsegs[i]);
	}

	/* R[24..31] */
	reg->r_nsecs = 0;
	for (i = 0; i < fnx_nelems(reg->r_rsecs); ++i) {
		reg->r_rsecs[i] = fnx_dtoh_u32(dreg->r_rsecs[i]);
		reg->r_nsecs += (size_t)fnx_popcount(reg->r_rsecs[i]);
	}
	if (reg->r_nsegs > 0) {
		fnx_assert(reg->r_nsecs > 0);
	}
}

int fnx_reg_dcheck(const fnx_header_t *hdr)
{
	int rc;
	const fnx_dreg_t *dreg;

	rc = fnx_dobj_check(hdr, FNX_VTYPE_REG);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_inode_dcheck(hdr);
	if (rc != 0) {
		return rc;
	}
	dreg = fnx_header_to_dreg(hdr);
	fnx_unused(dreg); /* TODO: any chekc with dreg? */
	return 0;
}

void fnx_reg_wmore(fnx_reg_t *reg, fnx_off_t wsize,
                   fnx_bkcnt_t nbks, fnx_bool_t keepsz)
{
	fnx_off_t   isize, nsize;
	fnx_bkcnt_t wbcap, nbcap;
	fnx_iattr_t *iattr = &reg->r_inode.i_iattr;

	fnx_assert((iattr->i_nblk + nbks) <= fnx_bytes_to_nbk(FNX_REGSIZE_MAX));

	isize = iattr->i_size;
	nsize = fnx_off_max(isize, wsize);
	wbcap = fnx_bytes_to_nbk((fnx_size_t)nsize);
	nbcap = fnx_bkcnt_max(iattr->i_vcap, wbcap);

	iattr->i_vcap = nbcap;
	iattr->i_nblk += nbks;
	if (!keepsz && (nsize > isize)) {
		iattr->i_size = nsize;
	}
}

void fnx_reg_wless(fnx_reg_t *reg, fnx_off_t wsize,
                   fnx_bkcnt_t nbks, fnx_bool_t keepsz)
{
	fnx_off_t   isize, nsize;
	fnx_bkcnt_t wbcap, nbcap;
	fnx_iattr_t *iattr = &reg->r_inode.i_iattr;

	fnx_assert(iattr->i_nblk >= nbks);

	isize = iattr->i_size;
	nsize = fnx_off_max(isize, wsize);
	wbcap = fnx_bytes_to_nbk((fnx_size_t)nsize);
	nbcap = fnx_bkcnt_max(iattr->i_vcap, wbcap);

	iattr->i_vcap = nbcap;
	iattr->i_nblk -= nbks;
	if (!keepsz && (nsize < isize)) {
		iattr->i_size = nsize;
	}
}

static uint32_t *reg_seg_slot(const fnx_reg_t *reg, fnx_off_t seg)
{
	size_t idx;
	const uint32_t *slot = NULL;

	idx  = ((size_t)seg / 32) % fnx_nelems(reg->r_rsegs);
	slot = &reg->r_rsegs[idx];
	return (uint32_t *)slot;
}

static uint32_t *reg_sec_slot(const fnx_reg_t *reg, fnx_off_t sec)
{
	size_t idx;
	const uint32_t *slot = NULL;

	idx  = ((size_t)sec / 32) % fnx_nelems(reg->r_rsecs);
	slot = &reg->r_rsecs[idx];
	return (uint32_t *)slot;
}


void fnx_reg_markseg(fnx_reg_t *reg, fnx_off_t off)
{
	fnx_off_t seg;
	uint32_t mask;
	uint32_t *slot;

	fnx_assert(fnx_off_to_rsec(off) == 0);
	seg  = fnx_off_to_rseg(off);
	slot = reg_seg_slot(reg, seg);
	mask = seg_slot_mask(seg);

	fnx_assert((*slot & mask) != mask);
	fnx_assert(reg->r_nsegs < FNX_RSECNSEG);

	fnx_setf(slot, mask);
	reg->r_nsegs += 1;
}

void fnx_reg_unmarkseg(fnx_reg_t *reg, fnx_off_t off)
{
	fnx_off_t seg;
	uint32_t mask;
	uint32_t *slot;

	fnx_assert(fnx_off_to_rsec(off) == 0);
	seg  = fnx_off_to_rseg(off);
	slot = reg_seg_slot(reg, seg);
	mask = seg_slot_mask(seg);

	fnx_assert((*slot & mask) == mask);
	fnx_assert(reg->r_nsegs > 0);
	fnx_assert(reg->r_nsecs > 0);

	fnx_unsetf(slot, mask);
	reg->r_nsegs -= 1;
}

int fnx_reg_testseg(const fnx_reg_t *reg, fnx_off_t off)
{
	int res;
	fnx_off_t seg;
	uint32_t mask;
	const uint32_t *slot;

	fnx_assert(fnx_off_to_rsec(off) == 0);
	seg  = fnx_off_to_rseg(off);
	slot = reg_seg_slot(reg, seg);
	mask = seg_slot_mask(seg);
	res  = fnx_testf(*slot, mask);
	return res;
}

void fnx_reg_marksec(fnx_reg_t *reg, fnx_off_t off)
{
	fnx_off_t sec;
	uint32_t mask;
	uint32_t *slot;

	sec  = fnx_off_to_rsec(off);
	slot = reg_sec_slot(reg, sec);
	mask = sec_slot_mask(sec);

	fnx_assert((*slot & mask) != mask);
	fnx_assert(reg->r_nsecs < FNX_REGNSEC);

	fnx_setf(slot, mask);
	reg->r_nsecs += 1;
}

void fnx_reg_unmarksec(fnx_reg_t *reg, fnx_off_t off)
{
	fnx_off_t sec;
	uint32_t mask;
	uint32_t *slot;

	sec  = fnx_off_to_rsec(off);
	slot = reg_sec_slot(reg, sec);
	mask = sec_slot_mask(sec);

	fnx_assert((*slot & mask) == mask);
	fnx_assert(reg->r_nsecs > 0);

	fnx_unsetf(slot, mask);
	reg->r_nsecs -= 1;
}

int fnx_reg_testsec(const fnx_reg_t *reg, fnx_off_t off)
{
	int res;
	fnx_off_t sec;
	uint32_t mask;
	const uint32_t *slot;

	sec  = fnx_off_to_rsec(off);
	slot = reg_sec_slot(reg, sec);
	mask = sec_slot_mask(sec);
	res  = fnx_testf(*slot, mask);
	return res;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_dregsec_t *fnx_header_to_dregsec(const fnx_header_t *hdr)
{
	return fnx_container_of(hdr, fnx_dregsec_t, rc_hdr);
}

fnx_regsec_t *fnx_vnode_to_regsec(const fnx_vnode_t *vnode)
{
	return fnx_container_of(vnode, fnx_regsec_t, rc_vnode);
}

void fnx_regsec_init(fnx_regsec_t *regsec)
{
	fnx_vnode_init(&regsec->rc_vnode, FNX_VTYPE_REGSEC);
	fnx_bzero(regsec->rc_rsegs, sizeof(regsec->rc_rsegs));
	regsec->rc_nsegs = 0;
	regsec->rc_magic = FNX_REGSEC_MAGIC;
}

void fnx_regsec_destroy(fnx_regsec_t *regsec)
{
	fnx_vnode_destroy(&regsec->rc_vnode);
	regsec->rc_nsegs = (fnx_size_t)(-1);
	regsec->rc_magic = 0;
}

void fnx_regsec_clone(const fnx_regsec_t *regsec, fnx_regsec_t *other)
{
	fnx_vnode_clone(&regsec->rc_vnode, &other->rc_vnode);
	fnx_bcopy(other->rc_rsegs, regsec->rc_rsegs, sizeof(other->rc_rsegs));
	other->rc_nsegs = regsec->rc_nsegs;
}

void fnx_regsec_htod(const fnx_regsec_t *regsec, fnx_dregsec_t *dregsec)
{
	fnx_header_t *hdr = &dregsec->rc_hdr;

	/* Header & tail */
	fnx_vnode_htod(&regsec->rc_vnode, hdr);
	fnx_dobj_zpad(hdr, FNX_VTYPE_REGSEC);

	/* R[1..5] */
	for (size_t i = 0; i < fnx_nelems(dregsec->rc_rsegs); ++i) {
		dregsec->rc_rsegs[i] = fnx_htod_u32(regsec->rc_rsegs[i]);
	}
}

void fnx_regsec_dtoh(fnx_regsec_t *regsec, const fnx_dregsec_t *dregsec)
{
	int rc;
	const fnx_header_t *hdr = &dregsec->rc_hdr;

	/* Header */
	rc = fnx_dobj_check(hdr, FNX_VTYPE_REGSEC);
	fnx_assert(rc == 0);
	fnx_vnode_dtoh(&regsec->rc_vnode, hdr);

	/* R[1..5] */
	regsec->rc_nsegs = 0;
	for (size_t i = 0; i < fnx_nelems(regsec->rc_rsegs); ++i) {
		regsec->rc_rsegs[i] = fnx_dtoh_u32(dregsec->rc_rsegs[i]);
		regsec->rc_nsegs += (size_t)fnx_popcount(regsec->rc_rsegs[i]);
	}
}

int fnx_regsec_dcheck(const fnx_header_t *hdr)
{
	int rc;
	fnx_vtype_e vtype;
	const fnx_dregsec_t *dregsec;

	vtype = fnx_dobj_vtype(hdr);
	if (vtype != FNX_VTYPE_REGSEC) {
		return -1;
	}
	rc = fnx_dobj_check(hdr, vtype);
	if (rc != 0) {
		return rc;
	}
	dregsec = fnx_header_to_dregsec(hdr);
	fnx_unused(dregsec);
	return 0;
}

static uint32_t *regsec_seg_slot(const fnx_regsec_t *regsec, fnx_off_t seg)
{
	size_t idx;
	const uint32_t *slot = NULL;

	idx  = ((size_t)seg / 32) % fnx_nelems(regsec->rc_rsegs);
	slot = &regsec->rc_rsegs[idx];
	return (uint32_t *)slot;
}

void fnx_regsec_markseg(fnx_regsec_t *regsec, fnx_off_t off)
{
	fnx_off_t seg;
	uint32_t mask;
	uint32_t *slot;

	seg  = fnx_off_to_rseg(off);
	slot = regsec_seg_slot(regsec, seg);
	mask = seg_slot_mask(seg);

	fnx_assert((*slot & mask) != mask);
	fnx_assert(regsec->rc_nsegs < (32 * fnx_nelems(regsec->rc_rsegs)));

	fnx_setf(slot, mask);
	regsec->rc_nsegs += 1;
}

void fnx_regsec_unmarkseg(fnx_regsec_t *regsec, fnx_off_t off)
{
	fnx_off_t seg;
	uint32_t mask;
	uint32_t *slot;

	seg  = fnx_off_to_rseg(off);
	slot = regsec_seg_slot(regsec, seg);
	mask = seg_slot_mask(seg);

	fnx_assert((*slot & mask) == mask);
	fnx_assert(regsec->rc_nsegs > 0);

	fnx_unsetf(slot, mask);
	regsec->rc_nsegs -= 1;
}

int fnx_regsec_testseg(const fnx_regsec_t *regsec, fnx_off_t off)
{
	int res;
	fnx_off_t seg;
	uint32_t mask;
	const uint32_t *slot;

	seg  = fnx_off_to_rseg(off);
	slot = regsec_seg_slot(regsec, seg);
	mask = seg_slot_mask(seg);
	res  = fnx_testf(*slot, mask);
	return res;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_dregseg_t *fnx_header_to_dregseg(const fnx_header_t *hdr)
{
	return fnx_container_of(hdr, fnx_dregseg_t, rs_hdr);
}

fnx_regseg_t *fnx_vnode_to_regseg(const fnx_vnode_t *vnode)
{
	return fnx_container_of(vnode, fnx_regseg_t, rs_vnode);
}

void fnx_regseg_init(fnx_regseg_t *regseg)
{
	fnx_vnode_init(&regseg->rs_vnode, FNX_VTYPE_REGSEG);
	fnx_segmap_init(&regseg->rs_segmap);
	regseg->rs_magic = FNX_REGSEG_MAGIC;
}

void fnx_regseg_destroy(fnx_regseg_t *regseg)
{
	fnx_segmap_destroy(&regseg->rs_segmap);
	fnx_vnode_destroy(&regseg->rs_vnode);
	regseg->rs_magic = 0;
}

void fnx_regseg_clone(const fnx_regseg_t *regseg, fnx_regseg_t *other)
{
	fnx_vnode_clone(&regseg->rs_vnode, &other->rs_vnode);
	fnx_segmap_copy(&other->rs_segmap, &regseg->rs_segmap);
}

int fnx_regseg_setup(fnx_regseg_t *regseg, fnx_off_t off)
{
	int rc;
	fnx_off_t loff;
	fnx_size_t len;

	loff = fnx_off_floor_rseg(off);
	len  = FNX_RSEGSIZE;
	rc   = fnx_segmap_setup(&regseg->rs_segmap, loff, len);
	fnx_assert(rc == 0); /* FIXME */
	return rc;
}

static void vbaddr_htod(const fnx_vaddr_t *vaddr, uint64_t *dlba)
{
	fnx_lba_t lba;

	lba = (fnx_lba_t)vaddr->xno;
	*dlba = fnx_htod_lba(lba);
}

static void vbaddr_dtoh(fnx_vaddr_t *vaddr, const uint64_t *dlba)
{
	fnx_lba_t lba;

	lba = fnx_dtoh_lba(*dlba);
	if (lba < FNX_LBA_APEX0) {
		lba = FNX_LBA_NULL;
	}
	fnx_vaddr_for_vbk(vaddr, lba);
}

void fnx_regseg_htod(const fnx_regseg_t *regseg, fnx_dregseg_t *dregseg)
{
	fnx_off_t off;
	fnx_header_t *hdr = &dregseg->rs_hdr;

	/* Header & tail */
	fnx_vnode_htod(&regseg->rs_vnode, hdr);
	fnx_dobj_zpad(hdr, FNX_VTYPE_REGSEG);

	/* R[0] */
	off = regseg->rs_segmap.rng.off;
	fnx_assert(off == fnx_off_floor_rseg(off));
	fnx_assert(off == (fnx_off_t)(hdr->h_xno));
	dregseg->rs_off = fnx_htod_off(off);

	/* R[1..14] */
	fnx_bzero(dregseg->rs_vlba, sizeof(dregseg->rs_vlba));
	for (size_t i = 0; i < fnx_nelems(dregseg->rs_vlba); ++i) {
		vbaddr_htod(&regseg->rs_segmap.vba[i], &dregseg->rs_vlba[i]);
	}
}

void fnx_regseg_dtoh(fnx_regseg_t *regseg, const fnx_dregseg_t *dregseg)
{
	int rc;
	fnx_off_t off;
	const fnx_header_t *hdr = &dregseg->rs_hdr;

	/* Header */
	rc = fnx_dobj_check(hdr, FNX_VTYPE_REGSEG);
	fnx_assert(rc == 0);
	fnx_vnode_dtoh(&regseg->rs_vnode, hdr);

	/* R[0] */
	off = fnx_dtoh_off(dregseg->rs_off);
	regseg->rs_segmap.rng.off = off;
	regseg->rs_segmap.rng.len = FNX_RSEGSIZE;
	regseg->rs_segmap.rng.idx = 0;
	regseg->rs_segmap.rng.cnt = FNX_RSEGNBK;

	/* R[1..14] */
	for (size_t i = 0; i < fnx_nelems(regseg->rs_segmap.vba); ++i) {
		vbaddr_dtoh(&regseg->rs_segmap.vba[i], &dregseg->rs_vlba[i]);
	}
}

int fnx_regseg_dcheck(const fnx_header_t *hdr)
{
	int rc;
	fnx_vtype_e vtype;
	const fnx_dregseg_t *dregseg;

	vtype = fnx_dobj_vtype(hdr);
	if (vtype != FNX_VTYPE_REGSEG) {
		return -1;
	}
	rc = fnx_dobj_check(hdr, vtype);
	if (rc != 0) {
		fnx_assert(0); /* XXX */
		return -1;
	}
	dregseg = fnx_header_to_dregseg(hdr);
	fnx_unused(dregseg); /* TODO: Check magic? */
	return 0;
}

int fnx_regseg_isempty(const fnx_regseg_t *regseg)
{
	return (fnx_segmap_nmaps(&regseg->rs_segmap) == 0);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Symlink operations:
 */

static fnx_dsymlnk1_t *fnx_dinode_to_dsymlnk1(const fnx_dinode_t *dinode)
{
	return fnx_container_of(dinode, fnx_dsymlnk1_t, sl_inode);
}

static fnx_dsymlnk2_t *fnx_dinode_to_dsymlnk2(const fnx_dinode_t *dinode)
{
	return fnx_container_of(dinode, fnx_dsymlnk2_t, sl_inode);
}

static fnx_dsymlnk3_t *fnx_dinode_to_dsymlnk3(const fnx_dinode_t *dinode)
{
	return fnx_container_of(dinode, fnx_dsymlnk3_t, sl_inode);
}

fnx_dsymlnk1_t *fnx_header_to_dsymlnk1(const fnx_header_t *hdr)
{
	return fnx_dinode_to_dsymlnk1(fnx_header_to_dinode(hdr));
}

fnx_dsymlnk2_t *fnx_header_to_dsymlnk2(const fnx_header_t *hdr)
{
	return fnx_dinode_to_dsymlnk2(fnx_header_to_dinode(hdr));
}

fnx_dsymlnk3_t *fnx_header_to_dsymlnk3(const fnx_header_t *hdr)
{
	return fnx_dinode_to_dsymlnk3(fnx_header_to_dinode(hdr));
}

fnx_symlnk_t *fnx_vnode_to_symlnk(const fnx_vnode_t *vnode)
{
	return fnx_inode_to_symlnk(fnx_vnode_to_inode(vnode));
}

fnx_symlnk_t *fnx_inode_to_symlnk(const fnx_inode_t *inode)
{
	return fnx_container_of(inode, fnx_symlnk_t, sl_inode);
}

void fnx_symlnk_init(fnx_symlnk_t *symlnk, fnx_vtype_e vtype)
{
	fnx_inode_t *inode = &symlnk->sl_inode;

	fnx_inode_init(inode, vtype);
	fnx_path_init(&symlnk->sl_value);
	inode->i_iattr.i_mode   = FNX_SYMLNK_INIT_MODE;
	symlnk->sl_magic        = FNX_SYMLNK_MAGIC;
}

void fnx_symlnk_destroy(fnx_symlnk_t *symlnk)
{
	fnx_inode_destroy(&symlnk->sl_inode);
	fnx_path_setup(&symlnk->sl_value, "");
	symlnk->sl_magic    = 5;
}

void fnx_symlnk_clone(const fnx_symlnk_t *symlnk, fnx_symlnk_t *other)
{
	fnx_inode_clone(&symlnk->sl_inode, &other->sl_inode);
	fnx_path_copy(&other->sl_value, &symlnk->sl_value);
}

static size_t symlnk_value_bufsz(const fnx_symlnk_t *symlnk)
{
	size_t sz;
	fnx_vtype_e vtype;

	vtype = fnx_inode_vtype(&symlnk->sl_inode);
	if (vtype == FNX_VTYPE_SYMLNK1) {
		sz = FNX_SYMLNK1_MAX;
	} else if (vtype == FNX_VTYPE_SYMLNK2) {
		sz = FNX_SYMLNK2_MAX;
	} else {
		fnx_assert(vtype == FNX_VTYPE_SYMLNK3);
		sz = FNX_SYMLNK3_MAX;
	}
	return sz;
}

static void symlnk_assign(fnx_symlnk_t *symlnk, const fnx_path_t *path)
{
	size_t bsz, len;
	fnx_iattr_t *iattr;

	len = path->len;
	bsz = symlnk_value_bufsz(symlnk);

	fnx_assert(symlnk->sl_magic == FNX_SYMLNK_MAGIC);
	fnx_assert(len < FNX_PATH_MAX);
	fnx_assert(len <= bsz);

	fnx_path_copy(&symlnk->sl_value, path);
	symlnk->sl_inode.i_tlen   = len;
	iattr = &symlnk->sl_inode.i_iattr;
	iattr->i_size = (fnx_off_t)len;
}

void fnx_symlnk_setup(fnx_symlnk_t *symlnk,
                      const fnx_uctx_t *uctx, const fnx_path_t *slnk)
{
	mode_t mode, xmsk;

	mode = FNX_SYMLNK_INIT_MODE;
	xmsk = FNX_SYMLNK_INIT_XMSK;
	fnx_inode_setup(&symlnk->sl_inode, uctx, mode, xmsk);
	symlnk_assign(symlnk, slnk);

	fnx_assert(symlnk->sl_magic == FNX_SYMLNK_MAGIC);
}

void fnx_symlnk_htod1(const fnx_symlnk_t *symlnk, fnx_dsymlnk1_t *dsymlnk1)
{
	fnx_dinode_t *dinode = &dsymlnk1->sl_inode;

	fnx_assert(fnx_inode_vtype(&symlnk->sl_inode) == FNX_VTYPE_SYMLNK1);

	/* R[0..7] */
	fnx_inode_htod(&symlnk->sl_inode, dinode);

	/* R[8..15] */
	fnx_htod_path(dsymlnk1->sl_lnk, &dinode->i_tlen, &symlnk->sl_value);
}

void fnx_symlnk_dtoh1(fnx_symlnk_t *symlnk, const fnx_dsymlnk1_t *dsymlnk1)
{
	int rc;
	const fnx_dinode_t *dinode = &dsymlnk1->sl_inode;
	const fnx_header_t *hdr    = &dinode->i_hdr;

	fnx_assert(fnx_inode_vtype(&symlnk->sl_inode) == FNX_VTYPE_SYMLNK1);

	/* Header */
	rc = fnx_dobj_check(hdr, FNX_VTYPE_SYMLNK1);
	fnx_assert(rc == 0);

	/* R[0..7] */
	fnx_inode_dtoh(&symlnk->sl_inode, dinode);

	/* R[8..15] */
	fnx_dtoh_path(&symlnk->sl_value, dsymlnk1->sl_lnk, dinode->i_tlen);
	symlnk->sl_inode.i_tlen = symlnk->sl_value.len;
}

void fnx_symlnk_htod2(const fnx_symlnk_t *symlnk, fnx_dsymlnk2_t *dsymlnk2)
{
	fnx_dinode_t *dinode = &dsymlnk2->sl_inode;

	fnx_assert(fnx_inode_vtype(&symlnk->sl_inode) == FNX_VTYPE_SYMLNK2);

	/* R[0..7] */
	fnx_inode_htod(&symlnk->sl_inode, dinode);

	/* R[8..15] */
	fnx_htod_path(dsymlnk2->sl_lnk, &dinode->i_tlen, &symlnk->sl_value);
}

void fnx_symlnk_dtoh2(fnx_symlnk_t *symlnk, const fnx_dsymlnk2_t *dsymlnk2)
{
	int rc;
	const fnx_dinode_t *dinode = &dsymlnk2->sl_inode;
	const fnx_header_t *hdr    = &dinode->i_hdr;

	fnx_assert(fnx_inode_vtype(&symlnk->sl_inode) == FNX_VTYPE_SYMLNK2);

	/* Header */
	rc = fnx_dobj_check(hdr, FNX_VTYPE_SYMLNK2);
	fnx_assert(rc == 0);

	/* R[0..7] */
	fnx_inode_dtoh(&symlnk->sl_inode, dinode);

	/* R[8..15] */
	fnx_dtoh_path(&symlnk->sl_value, dsymlnk2->sl_lnk, dinode->i_tlen);
	symlnk->sl_inode.i_tlen = symlnk->sl_value.len;
}

void fnx_symlnk_htod3(const fnx_symlnk_t *symlnk, fnx_dsymlnk3_t *dsymlnk3)
{
	fnx_dinode_t *dinode = &dsymlnk3->sl_inode;

	fnx_assert(fnx_inode_vtype(&symlnk->sl_inode) == FNX_VTYPE_SYMLNK3);

	/* R[0..7] */
	fnx_inode_htod(&symlnk->sl_inode, dinode);

	/* R[8..15] */
	fnx_htod_path(dsymlnk3->sl_lnk, &dinode->i_tlen, &symlnk->sl_value);
}

void fnx_symlnk_dtoh3(fnx_symlnk_t *symlnk, const fnx_dsymlnk3_t *dsymlnk3)
{
	int rc;
	const fnx_dinode_t *dinode = &dsymlnk3->sl_inode;
	const fnx_header_t *hdr    = &dinode->i_hdr;

	fnx_assert(fnx_inode_vtype(&symlnk->sl_inode) == FNX_VTYPE_SYMLNK3);

	/* Header */
	rc = fnx_dobj_check(hdr, FNX_VTYPE_SYMLNK3);
	fnx_assert(rc == 0);

	/* R[0..7] */
	fnx_inode_dtoh(&symlnk->sl_inode, dinode);

	/* R[8..15] */
	fnx_dtoh_path(&symlnk->sl_value, dsymlnk3->sl_lnk, dinode->i_tlen);
	symlnk->sl_inode.i_tlen = symlnk->sl_value.len;
}

int fnx_symlnk_dcheck(const fnx_header_t *hdr)
{
	int rc;
	fnx_vtype_e vtype;

	vtype = fnx_dobj_vtype(hdr);
	if ((vtype != FNX_VTYPE_SYMLNK1) &&
	    (vtype != FNX_VTYPE_SYMLNK2) &&
	    (vtype != FNX_VTYPE_SYMLNK3)) {
		return -1;
	}
	rc = fnx_dobj_check(hdr, vtype);
	if (rc != 0) {
		fnx_assert(0); /* XXX */
		return -1;
	}
	/* TODO: Extra checks */
	return 0;
}
