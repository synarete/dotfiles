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
#include <unistd.h>
#include <fcntl.h>

#include <fnxhash.h>
#include <fnxinfra.h>
#include "core-blobs.h"
#include "core-types.h"
#include "core-stats.h"
#include "core-htod.h"
#include "core-addr.h"
#include "core-addri.h"
#include "core-elems.h"
#include "core-iobuf.h"
#include "core-jobsq.h"
#include "core-space.h"
#include "core-super.h"
#include "core-inode.h"
#include "core-inodei.h"

#define MODE_BASE  (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_vtype_e fnx_mode_to_vtype(fnx_mode_t mode)
{
	fnx_mode_t mtype;
	fnx_vtype_e vtype;

	mtype = mode & S_IFMT;
	switch (mtype) {
		case S_IFCHR:
			vtype = FNX_VTYPE_CHRDEV;
			break;
		case S_IFBLK:
			vtype = FNX_VTYPE_BLKDEV;
			break;
		case S_IFSOCK:
			vtype = FNX_VTYPE_SOCK;
			break;
		case S_IFIFO:
			vtype = FNX_VTYPE_FIFO;
			break;
		case S_IFLNK:
			vtype = FNX_VTYPE_SYMLNK1;
			break;
		case S_IFREG:
			vtype = FNX_VTYPE_REG;
			break;
		case S_IFDIR:
			vtype = FNX_VTYPE_DIR;
			break;
		default:
			vtype = FNX_VTYPE_EMPTY;
			break;
	}
	return vtype;
}

fnx_mode_t fnx_vtype_to_mode(fnx_vtype_e vtype)
{
	fnx_mode_t mode;

	switch ((int)vtype) {
		case FNX_VTYPE_DIR:
			mode = S_IFDIR;
			break;
		case FNX_VTYPE_REG:
			mode = S_IFREG;
			break;
		case FNX_VTYPE_CHRDEV:
			mode = S_IFCHR;
			break;
		case FNX_VTYPE_BLKDEV:
			mode = S_IFBLK;
			break;
		case FNX_VTYPE_SOCK:
			mode = S_IFSOCK;
			break;
		case FNX_VTYPE_FIFO:
			mode = S_IFIFO;
			break;
		case FNX_VTYPE_SYMLNK1:
		case FNX_VTYPE_SYMLNK2:
		case FNX_VTYPE_SYMLNK3:
			mode = S_IFLNK;
			break;
		default:
			mode = 0;
			break;
	}
	return mode;
}

fnx_mode_t fnx_vtype_to_crmode(fnx_vtype_e vtype, fnx_mode_t mode,
                               fnx_mode_t umsk)
{
	fnx_mode_t i_mode, c_mode, fmt_mask;

	i_mode   = fnx_vtype_to_mode(vtype);
	fmt_mask = (fnx_mode_t)S_IFMT;
	c_mode   = (mode & ~umsk) & ~fmt_mask;

	return (i_mode | c_mode);
}

fnx_vtype_e fnx_vtype_by_symlnk_value(const fnx_path_t *path)
{
	fnx_vtype_e vtype;
	const fnx_size_t len = path->len;

	if (len <= FNX_SYMLNK1_MAX) {
		vtype = FNX_VTYPE_SYMLNK1;
	} else if (len <= FNX_SYMLNK2_MAX) {
		vtype = FNX_VTYPE_SYMLNK2;
	} else if (len < FNX_SYMLNK3_MAX) {
		vtype = FNX_VTYPE_SYMLNK3;
	} else {
		fnx_assert(0);
		vtype = FNX_VTYPE_NONE;
	}
	return vtype;
}

int fnx_mode_isspecial(fnx_mode_t mode)
{
	fnx_vtype_e vtype;

	vtype = fnx_mode_to_vtype(mode);
	return ((vtype == FNX_VTYPE_CHRDEV) ||
	        (vtype == FNX_VTYPE_BLKDEV) ||
	        (vtype == FNX_VTYPE_SOCK) ||
	        (vtype == FNX_VTYPE_FIFO));
}

int fnx_mode_isreg(fnx_mode_t mode)
{
	fnx_vtype_e vtype;

	vtype = fnx_mode_to_vtype(mode);
	return (vtype == FNX_VTYPE_REG);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_vnode_init(fnx_vnode_t *vnode, fnx_vtype_e vtype)
{
	fnx_jelem_init(&vnode->v_jelem);
	fnx_link_init(&vnode->v_clink);
	fnx_vaddr_init(&vnode->v_vaddr, vtype);
	fnx_baddr_reset(&vnode->v_baddr);
	vnode->v_refcnt     = 0;
	vnode->v_pseudo     = FNX_FALSE;
	vnode->v_cached     = FNX_FALSE;
	vnode->v_expired    = FNX_FALSE;
	vnode->v_forgot     = FNX_FALSE;
	vnode->v_placed     = FNX_FALSE;
	vnode->v_pinned     = FNX_FALSE;
	vnode->v_magic      = FNX_VNODE_MAGIC;
	vnode->v_hlnk       = NULL;
	vnode->v_bkref      = NULL;
}

void fnx_vnode_destroy(fnx_vnode_t *vnode)
{
	fnx_assert(!vnode->v_cached);
	fnx_assert(vnode->v_magic == FNX_VNODE_MAGIC);
	fnx_assert(vnode->v_hlnk == NULL);
	fnx_assert(fnx_link_isunlinked(&vnode->v_clink));

	fnx_jelem_destroy(&vnode->v_jelem);
	fnx_link_destroy(&vnode->v_clink);
	fnx_vaddr_destroy(&vnode->v_vaddr);
	fnx_baddr_reset(&vnode->v_baddr);
	vnode->v_vaddr.ino   = FNX_INO_NULL;
	vnode->v_vaddr.xno   = 0;
	vnode->v_vaddr.vtype = FNX_VTYPE_NONE;
	vnode->v_magic       = 0;
}

void fnx_vnode_clone(const fnx_vnode_t *vnode, fnx_vnode_t *other)
{
	fnx_vaddr_copy(&other->v_vaddr, &vnode->v_vaddr);
	fnx_baddr_copy(&other->v_baddr, &vnode->v_baddr);
}

static fnx_size_t vtype_to_dobjnfrg(fnx_vtype_e vtype)
{
	return fnx_bytes_to_nfrg(fnx_get_dobjsize(vtype));
}

void fnx_vnode_htod(const fnx_vnode_t *vnode, fnx_header_t *hdr)
{
	fnx_size_t nfrg;
	fnx_vtype_e vtype;
	const fnx_vaddr_t *vaddr;

	vaddr   = &vnode->v_vaddr;
	vtype   = vaddr->vtype;
	nfrg    = vtype_to_dobjnfrg(vtype);
	fnx_dobj_assign(hdr, vtype, vaddr->ino, vaddr->xno, nfrg);
}

void fnx_vnode_dtoh(fnx_vnode_t *vnode, const fnx_header_t *hdr)
{
	fnx_size_t nfrg, hdrlen;
	fnx_vaddr_t vaddr;

	vaddr.vtype = fnx_dobj_vtype(hdr);
	vaddr.ino   = fnx_dobj_getino(hdr);
	vaddr.xno   = fnx_dobj_getxno(hdr);

	nfrg    = vtype_to_dobjnfrg(vaddr.vtype);
	hdrlen  = fnx_dobj_getlen(hdr);
	fnx_assert(nfrg == hdrlen); /* XXX */

	fnx_vnode_setvaddr(vnode, &vaddr);
}

void fnx_vnode_setvaddr(fnx_vnode_t *vnode, const fnx_vaddr_t *vaddr)
{
	fnx_vaddr_copy(&vnode->v_vaddr, vaddr);
}

void fnx_vnode_attach(fnx_vnode_t      *vnode,
                      const fnx_baddr_t *baddr,
                      fnx_bkref_t       *bkref)
{
	fnx_assert(baddr->lba != 0);
	fnx_baddr_copy(&vnode->v_baddr, baddr);
	vnode->v_bkref = bkref;
	if (bkref != NULL) {
		fnx_assert(bkref->bk_magic == FNX_BKREF_MAGIC);
		fnx_assert(bkref->bk_baddr.vol == baddr->vol);
		fnx_assert(bkref->bk_baddr.lba == baddr->lba);
		bkref->bk_refcnt += 1;
	}
}

fnx_bkref_t *fnx_vnode_detach(fnx_vnode_t *vnode)
{
	fnx_bkref_t *bkref = vnode->v_bkref;
	if (bkref != NULL) {
		fnx_assert(bkref->bk_magic == FNX_BKREF_MAGIC);
		fnx_assert(bkref->bk_refcnt > 0);
		vnode->v_bkref = NULL;
		bkref->bk_refcnt -= 1;
	}
	return bkref;
}

int fnx_vnode_ismutable(const fnx_vnode_t *vnode)
{
	int res = FNX_TRUE;
	const fnx_bkref_t *bkref;

	bkref = vnode->v_bkref;
	if (bkref != NULL) { /* May be NULL only if pseudo inode */
		if (!fnx_bkref_ismutable(bkref)) {
			res = FNX_FALSE;
		} else if (bkref->bk_spmap != NULL) {
			bkref = bkref->bk_spmap->sm_vnode.v_bkref;
			if (!fnx_bkref_ismutable(bkref)) {
				res = FNX_FALSE;
			}
		}
	}
	return res;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_dinode_t *fnx_header_to_dinode(const fnx_header_t *hdr)
{
	return fnx_container_of(hdr, fnx_dinode_t, i_hdr);
}

fnx_inode_t *fnx_vnode_to_inode(const fnx_vnode_t *vnode)
{
	return fnx_container_of(vnode, fnx_inode_t, i_vnode);
}

void fnx_inode_init(fnx_inode_t *inode, fnx_vtype_e vtype)
{
	fnx_vnode_init(&inode->i_vnode, vtype);
	fnx_name_init(&inode->i_name);
	fnx_iattr_init(&inode->i_iattr);
	inode->i_parentd    = FNX_INO_NULL;
	inode->i_refino     = FNX_INO_NULL;
	inode->i_tlen       = 0;
	inode->i_nhfunc     = FNX_HFUNC_SIP_DINO;
	inode->i_magic      = FNX_INODE_MAGIC;
	inode->i_super      = NULL;
	inode->i_meta       = NULL;
	inode->i_flags      = 0;

	inode->i_iattr.i_mode = fnx_vtype_to_crmode(vtype, MODE_BASE, 0);
}

void fnx_inode_destroy(fnx_inode_t *inode)
{
	fnx_assert(inode->i_magic == FNX_INODE_MAGIC);
	fnx_vnode_destroy(&inode->i_vnode);
	fnx_name_destroy(&inode->i_name);
	fnx_iattr_destroy(&inode->i_iattr);
	inode->i_super      = NULL;
	inode->i_meta       = NULL;
	inode->i_flags      = 0;
	inode->i_magic      = 4;
}

void fnx_inode_setino(fnx_inode_t *inode, fnx_ino_t ino)
{
	fnx_vaddr_t vaddr;

	fnx_vaddr_for_inode(&vaddr, fnx_inode_vtype(inode), ino);
	fnx_vnode_setvaddr(&inode->i_vnode, &vaddr);
	inode->i_iattr.i_ino = ino;
}

void fnx_inode_setsuper(fnx_inode_t *inode, fnx_super_t *super)
{
	inode->i_super = super;
	inode->i_iattr.i_gen = super->su_fsinfo.fs_attr.f_gen;
	fnx_inode_setitime(inode, FNX_BTIME_NOW);
}

static void inode_setcmode(fnx_inode_t *inode, const fnx_uctx_t *uctx,
                           fnx_mode_t mode, fnx_mode_t xmsk)
{
	fnx_mode_t cmod, umsk;

	umsk = uctx->u_cred.cr_umask;
	cmod = fnx_vtype_to_crmode(fnx_inode_vtype(inode), mode, umsk);

	inode->i_iattr.i_mode = cmod | xmsk;
}

static void inode_setcred(fnx_inode_t *inode, const fnx_uctx_t *uctx)
{
	inode->i_iattr.i_uid = uctx->u_cred.cr_uid;
	inode->i_iattr.i_gid = uctx->u_cred.cr_gid;
}

void fnx_inode_setup(fnx_inode_t *inode, const fnx_uctx_t *uctx,
                     fnx_mode_t mode, fnx_mode_t xmsk)
{
	inode_setcred(inode, uctx);
	inode_setcmode(inode, uctx, mode, xmsk);
	fnx_inode_setitime(inode, FNX_BAMCTIME_NOW);
}

fnx_hash_t fnx_calc_inamehash(const fnx_name_t *name, fnx_ino_t parent_ino)
{
	uint64_t dino, hash = 0;
	uint8_t seed[16] = {
		/* ASCII: "funexfilesystem" */
		0x66, 0x75, 0x6e, 0x65, 0x78, 0x66, 0x69, 0x6c,
		0x65, 0x73, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x00
	};

	dino = (uint64_t)parent_ino;
	for (size_t i = 0, j = 8; i < 8; ++i, ++j) {
		seed[i] ^= (uint8_t)(dino >> (8 * i));
		seed[j] ^= (uint8_t)(~dino >> (8 * i));
	}

	blgn_siphash64(seed, name->str, name->len, &hash);
	return hash | 1; /* Force non-zero hash */
}

void fnx_inode_associate(fnx_inode_t *inode,
                         fnx_ino_t dino, const fnx_name_t *name)
{
	inode->i_parentd = dino;
	if (name != NULL) {
		fnx_name_copy(&inode->i_name, name);
		if (name->hash != 0) {
			inode->i_name.hash = name->hash;
		} else {
			inode->i_name.hash = fnx_calc_inamehash(name, dino);
		}
	} else {
		fnx_name_setup(&inode->i_name, "");
		inode->i_name.hash = 0;
	}
}

void fnx_inode_clone(const fnx_inode_t *inode, fnx_inode_t *other)
{
	fnx_vnode_clone(&inode->i_vnode, &other->i_vnode);
	fnx_name_copy(&other->i_name, &inode->i_name);
	fnx_iattr_copy(&other->i_iattr, &inode->i_iattr);
	other->i_parentd    = inode->i_parentd;
	other->i_refino     = inode->i_refino;
	other->i_tlen       = inode->i_tlen;
	other->i_nhfunc     = inode->i_nhfunc;
	other->i_flags      = inode->i_flags;
}

void fnx_inode_htod(const fnx_inode_t *inode, fnx_dinode_t *dinode)
{
	fnx_header_t *hdr = &dinode->i_hdr;

	/* Header & tail */
	fnx_vnode_htod(&inode->i_vnode, hdr);
	fnx_dobj_zpad(hdr, fnx_inode_vtype(inode));

	/* R[0] */
	dinode->i_parentd   = fnx_htod_ino(inode->i_parentd);
	dinode->i_refino    = fnx_htod_ino(inode->i_refino);
	dinode->i_flags     = fnx_htod_flags(inode->i_flags);

	/* R[1..2] */
	fnx_iattr_htod(&inode->i_iattr, &dinode->i_attr);

	/* R[3] */
	dinode->i_magic     = fnx_htod_magic(inode->i_magic);
	dinode->i_tlen      = fnx_htod_u16((uint16_t)(inode->i_tlen));
	dinode->i_nhfunc    = fnx_htod_hfunc(inode->i_nhfunc);
	dinode->i_nhash     = fnx_htod_hash(inode->i_name.hash);

	/* R[4..7] */
	fnx_htod_name(dinode->i_name, &dinode->i_namelen, &inode->i_name);
}

void fnx_inode_dtoh(fnx_inode_t *inode, const fnx_dinode_t *dinode)
{
	const fnx_header_t *hdr = &dinode->i_hdr;

	/* Header */
	fnx_vnode_dtoh(&inode->i_vnode, hdr);

	/* R[0] */
	inode->i_parentd    = fnx_dtoh_ino(dinode->i_parentd);
	inode->i_refino     = fnx_dtoh_ino(dinode->i_refino);
	inode->i_flags      = fnx_dtoh_flags(dinode->i_flags);

	/* R[1..2] */
	fnx_iattr_dtoh(&inode->i_iattr, &dinode->i_attr);
	inode->i_iattr.i_ino = inode->i_vnode.v_vaddr.ino; /* TODO: why? */

	/* R[3] */
	inode->i_tlen       = fnx_dtoh_u16(dinode->i_tlen);
	inode->i_nhfunc     = fnx_dtoh_hfunc(dinode->i_nhfunc);
	inode->i_name.hash  = fnx_dtoh_hash(dinode->i_nhash);

	/* R[4..7] */
	fnx_dtoh_name(&inode->i_name, dinode->i_name, dinode->i_namelen);
}

int fnx_inode_dcheck(const fnx_header_t *hdr)
{
	int rc;
	fnx_magic_t magic;
	fnx_hfunc_e hfunc;
	const fnx_dinode_t *dinode;

	rc = fnx_dobj_check(hdr, FNX_VTYPE_ANY);
	if (rc != 0) {
		return rc;
	}
	dinode = fnx_header_to_dinode(hdr);
	magic = fnx_dtoh_magic(dinode->i_magic);
	if (magic != FNX_INODE_MAGIC) {
		fnx_warn("illegal-inode-magic: magic=%#x expected=%#x",
		         magic, FNX_INODE_MAGIC);
		return -1;
	}
	hfunc = fnx_dtoh_hfunc(dinode->i_nhfunc);
	if (hfunc != FNX_HFUNC_SIP_DINO) {
		fnx_assert(hfunc == FNX_HFUNC_SIP_DINO);
		return -1;
	}
	/* TODO: Check hash-value is correct
	 * NB: Have hcheck for checks after conversion to host repr?
	 */

	return 0;
}

void fnx_inode_setitimes(fnx_inode_t *inode, fnx_flags_t flags,
                         const fnx_times_t *tm)
{
	fnx_times_fill(&inode->i_iattr.i_times, flags, tm);
}

void fnx_inode_setitime(fnx_inode_t *inode, fnx_flags_t flags)
{
	fnx_times_fill(&inode->i_iattr.i_times, flags, NULL);
}

void fnx_inode_getname(const fnx_inode_t *inode, fnx_name_t *name)
{
	fnx_name_copy(name, &inode->i_name);
}

int fnx_inode_hasname(const fnx_inode_t *inode, const fnx_name_t *name)
{
	return fnx_name_isnequal(&inode->i_name, name->str, name->len);
}

int fnx_inode_isdir(const fnx_inode_t *inode)
{
	return (fnx_inode_vtype(inode) == FNX_VTYPE_DIR);
}

int fnx_inode_isreg(const fnx_inode_t *inode)
{
	return (fnx_inode_vtype(inode) == FNX_VTYPE_REG);
}

int fnx_inode_isfifo(const fnx_inode_t *inode)
{
	return (fnx_inode_vtype(inode) == FNX_VTYPE_FIFO);
}

int fnx_inode_issock(const fnx_inode_t *inode)
{
	return (fnx_inode_vtype(inode) == FNX_VTYPE_SOCK);
}

int fnx_inode_issymlnk(const fnx_inode_t *inode)
{
	const fnx_vtype_e vtype = fnx_inode_vtype(inode);
	return fnx_isvsymlnk(vtype);
}

int fnx_inode_isreflnk(const fnx_inode_t *inode)
{
	return (fnx_inode_vtype(inode) == FNX_VTYPE_REFLNK);
}

fnx_ino_t fnx_inode_getrefino(const fnx_inode_t *inode)
{
	const fnx_vtype_e vtype = fnx_inode_vtype(inode);
	return ((vtype != FNX_VTYPE_REFLNK) ?
	        fnx_inode_getino(inode) : inode->i_refino);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_inode_testf(const fnx_inode_t *inode, fnx_flags_t mask)
{
	return fnx_testlf(inode->i_flags, mask);
}

void fnx_inode_setf(fnx_inode_t *inode, fnx_flags_t mask)
{
	fnx_setf(&inode->i_flags, mask);
}

void fnx_inode_unsetf(fnx_inode_t *inode, fnx_flags_t mask)
{
	fnx_unsetf(&inode->i_flags, mask);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_mode_t
inode_rwx(const fnx_inode_t *inode, const fnx_uctx_t *uctx)
{
	fnx_mode_t i_mode, res_mask;
	fnx_uid_t uid, i_uid;
	fnx_gid_t gid, i_gid;
	const fnx_cred_t *cred;
	const fnx_iattr_t *iattr;

	cred = &uctx->u_cred;
	uid = cred->cr_uid;
	gid = cred->cr_gid;

	iattr = &inode->i_iattr;
	i_mode = iattr->i_mode;
	i_uid = iattr->i_uid;
	i_gid = iattr->i_gid;

	res_mask = F_OK;
	if (uctx->u_root) {
		res_mask |= R_OK | W_OK;
		if (fnx_inode_isreg(inode)) {
			if (i_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
				res_mask |= X_OK;
			}
		} else {
			res_mask |= X_OK;
		}
	} else if (uid == i_uid) {
		/* Owner permissions */
		if (i_mode & S_IRUSR) {
			res_mask |= R_OK;
		}
		if (i_mode & S_IWUSR) {
			res_mask |= W_OK;
		}
		if (i_mode & S_IXUSR) {
			res_mask |= X_OK;
		}
	} else if (gid == i_gid) {
		/* Group permissions */
		if (i_mode & S_IRGRP) {
			res_mask |= R_OK;
		}
		if (i_mode & S_IWGRP) {
			res_mask |= W_OK;
		}
		if (i_mode & S_IXGRP) {
			res_mask |= X_OK;
		}
		/* TODO: Check for supplementary groups */
	} else {
		/* Other permissions */
		if (i_mode & S_IROTH) {
			res_mask |= R_OK;
		}
		if (i_mode & S_IWOTH) {
			res_mask |= W_OK;
		}
		if (i_mode & S_IXOTH) {
			res_mask |= X_OK;
		}
	}
	return res_mask;
}

int fnx_inode_access(const fnx_inode_t *inode,
                     const fnx_uctx_t *uctx, fnx_mode_t mask)
{
	fnx_mode_t rwx;

	rwx = inode_rwx(inode, uctx);
	return ((rwx & mask) == mask) ? 0 : -1;
}


void fnx_inode_getiattr(const fnx_inode_t *inode, fnx_iattr_t *iattr)
{
	fnx_bcopy(iattr, &inode->i_iattr, sizeof(*iattr));
}

static int mode_isexec(fnx_mode_t mode)
{
	const fnx_mode_t mask = S_IXUSR | S_IXGRP | S_IXOTH;
	return ((mode & mask) != 0);
}

int fnx_inode_isexec(const fnx_inode_t *inode)
{
	return mode_isexec(inode->i_iattr.i_mode);
}

static fnx_mode_t mode_clearbits(fnx_mode_t mode, fnx_mode_t bits)
{
	fnx_mode_t mask;

	mask = ~((mode_t)(bits));
	return (mode & mask);
}

void fnx_inode_clearsuid(fnx_inode_t *inode)
{
	inode->i_iattr.i_mode = mode_clearbits(inode->i_iattr.i_mode, S_ISUID);
}

void fnx_inode_clearsgid(fnx_inode_t *inode)
{
	inode->i_iattr.i_mode = mode_clearbits(inode->i_iattr.i_mode, S_ISGID);
}



