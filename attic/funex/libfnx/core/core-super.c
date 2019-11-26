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
#include <sys/ioctl.h>

#include <fnxinfra.h>
#include "core-blobs.h"
#include "core-types.h"
#include "core-stats.h"
#include "core-htod.h"
#include "core-addr.h"
#include "core-addri.h"
#include "core-elems.h"
#include "core-inode.h"
#include "core-inodei.h"
#include "core-space.h"
#include "core-super.h"


#define FNX_BYTE_ORDER      __BYTE_ORDER


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_dsuper_t *fnx_header_to_dsuper(const fnx_header_t *hdr)
{
	return fnx_container_of(hdr, fnx_dsuper_t, su_hdr);
}

fnx_super_t *fnx_vnode_to_super(const fnx_vnode_t *vnode)
{
	return fnx_container_of(vnode, fnx_super_t, su_vnode);
}

void fnx_super_init(fnx_super_t *super)
{
	fnx_vnode_init(&super->su_vnode, FNX_VTYPE_SUPER);
	fnx_name_init(&super->su_name);
	fnx_name_init(&super->su_version);
	fnx_bzero_obj(&super->su_fsinfo);
	fnx_fsattr_init(&super->su_fsinfo.fs_attr);
	fnx_fsstat_init(&super->su_fsinfo.fs_stat);
	fnx_times_init(&super->su_times);
	fnx_uctx_init(&super->su_uctx);
	super->su_active    = FNX_TRUE;
	super->su_flags     = 0;
	super->su_volsize   = 0;
	super->su_private   = NULL;
	super->su_magic     = FNX_SUPER_MAGIC;
}

void fnx_super_destroy(fnx_super_t *super)
{
	fnx_assert(super->su_magic == FNX_SUPER_MAGIC);
	fnx_vnode_destroy(&super->su_vnode);
	fnx_name_destroy(&super->su_name);
	fnx_bzero_obj(&super->su_fsinfo);
	super->su_flags     = 0;
	super->su_volsize   = 0;
	super->su_private   = NULL;
	super->su_magic     = 0;
}

void fnx_super_clone(const fnx_super_t *super, fnx_super_t *other)
{
	fnx_vnode_clone(&super->su_vnode, &other->su_vnode);
	fnx_name_copy(&other->su_name, &super->su_name);
	fnx_name_copy(&other->su_version, &super->su_version);
	fnx_fsinfo_copy(&other->su_fsinfo, &super->su_fsinfo);
	fnx_times_copy(&other->su_times, &super->su_times);
	fnx_uctx_copy(&other->su_uctx, &super->su_uctx);
	other->su_flags     = super->su_flags;
	other->su_volsize   = super->su_volsize;
}

void fnx_super_setup(fnx_super_t *super, const char *name, const char *vers)
{
	fnx_name_setup(&super->su_name, name);
	fnx_name_setup(&super->su_version, vers);
	fnx_vaddr_for_super(&super->su_vnode.v_vaddr);
	fnx_baddr_for_super(&super->su_vnode.v_baddr);
}

void fnx_super_htod(const fnx_super_t *super, fnx_dsuper_t *dsuper)
{
	fnx_header_t *hdr = &dsuper->su_hdr;

	/* Header & tail */
	fnx_vnode_htod(&super->su_vnode, hdr);
	fnx_dobj_zpad(hdr, FNX_VTYPE_SUPER);

	/* R[0] */
	dsuper->su_flags    = fnx_htod_flags(super->su_flags);
	dsuper->su_secsize  = fnx_htod_u32(FNX_BCKTSIZE);
	dsuper->su_blksize  = fnx_htod_u16(FNX_BLKSIZE);
	dsuper->su_frgsize  = fnx_htod_u16(FNX_FRGSIZE);
	dsuper->su_endian   = fnx_htod_u16(FNX_BYTE_ORDER);
	dsuper->su_ostype   = 0; /* XXX */

	/* R[1] */
	dsuper->su_volsize  = fnx_htod_u64(super->su_volsize);

	/* R[2] */
	fnx_fsattr_htod(&super->su_fsinfo.fs_attr, &dsuper->su_fsattr);

	/* R[3..6] */
	fnx_fsstat_htod(&super->su_fsinfo.fs_stat, &dsuper->su_fsstat);

	/* R[7] */
	fnx_times_htod(&super->su_times, &dsuper->su_times);
	dsuper->su_magic = fnx_htod_magic(super->su_magic);

	/* R[8..15] */
	fnx_htod_name(dsuper->su_version, &dsuper->su_verslen, &super->su_version);
	fnx_htod_name(dsuper->su_name, &dsuper->su_namelen, &super->su_name);
}


void fnx_super_dtoh(fnx_super_t *super, const fnx_dsuper_t *dsuper)
{
	int rc;
	const fnx_header_t *hdr = &dsuper->su_hdr;

	/* Header */
	rc = fnx_dobj_check(hdr, FNX_VTYPE_SUPER);
	fnx_assert(rc == 0);
	fnx_vnode_dtoh(&super->su_vnode, hdr);

	/* R[0] */
	super->su_flags     = fnx_dtoh_flags(dsuper->su_flags);

	/* R[1] */
	super->su_volsize   = fnx_dtoh_u64(dsuper->su_volsize);

	/* R[2] */
	fnx_fsattr_dtoh(&super->su_fsinfo.fs_attr, &dsuper->su_fsattr);

	/* R[3..6] */
	fnx_fsstat_dtoh(&super->su_fsinfo.fs_stat, &dsuper->su_fsstat);

	/* R[7] */
	fnx_times_dtoh(&super->su_times, &dsuper->su_times);
	super->su_magic = fnx_dtoh_magic(dsuper->su_magic);

	/* R[8..15] */
	fnx_dtoh_name(&super->su_name, dsuper->su_name, dsuper->su_namelen);
	fnx_dtoh_name(&super->su_version, dsuper->su_version, dsuper->su_verslen);
}

int fnx_super_dcheck(const fnx_header_t *hdr)
{
	int rc;
	size_t size;
	fnx_vtype_e vtype;
	fnx_magic_t magic;
	const fnx_dsuper_t *dsuper;

	vtype = fnx_dobj_vtype(hdr);
	if (vtype != FNX_VTYPE_SUPER) {
		fnx_warn("not-super-vtype: vtype=%d", (int)vtype);
		return -1;
	}
	rc = fnx_dobj_check(hdr, vtype);
	if (rc != 0) {
		return rc;
	}
	dsuper = fnx_header_to_dsuper(hdr);
	magic  = fnx_dtoh_magic(dsuper->su_magic);
	if (magic != FNX_SUPER_MAGIC) {
		fnx_warn("not-super-magic: magic=%#x expected=%#x",
		         magic, FNX_SUPER_MAGIC);
		return -1;
	}
	size = fnx_dtoh_u16(dsuper->su_frgsize);
	if (size != FNX_FRGSIZE) {
		fnx_warn("bad-frgsize: size=%d expected=%d", (int)size, FNX_FRGSIZE);
		return -1;
	}
	size = fnx_dtoh_u16(dsuper->su_blksize);
	if (size != FNX_BLKSIZE) {
		fnx_warn("bad-blksize: size=%d expected=%d", (int)size, FNX_BLKSIZE);
		return -1;
	}
	size = fnx_dtoh_u32(dsuper->su_secsize);
	if (size != FNX_BCKTSIZE) {
		fnx_warn("bad-secsize: size=%d expected=%lu", (int)size, FNX_BCKTSIZE);
		return -1;
	}

	return 0;
}

void fnx_super_settimes(fnx_super_t *super,
                        const fnx_times_t *tms, fnx_flags_t flags)
{
	fnx_times_fill(&super->su_times, flags, tms);
}

void fnx_super_getfsinfo(const fnx_super_t *super, fnx_fsinfo_t *fsinfo)
{
	fnx_bcopy(fsinfo, &super->su_fsinfo, sizeof(*fsinfo));
}

void fnx_super_fillnewi(const fnx_super_t *super, fnx_inode_t *inode)
{
	inode->i_iattr.i_gen = super->su_fsinfo.fs_attr.f_gen;
}

int fnx_super_hasnexti(const fnx_super_t *super, int prvlgd)
{
	return fnx_fsstat_hasnexti(&super->su_fsinfo.fs_stat, prvlgd);
}

int fnx_super_hasfreeb(const fnx_super_t *super, fnx_bkcnt_t nfrgs, int prvlgd)
{
	return fnx_fsstat_hasfreeb(&super->su_fsinfo.fs_stat, nfrgs, prvlgd);
}

fnx_ino_t fnx_super_getino(const fnx_super_t *super)
{
	return super->su_vnode.v_vaddr.ino;
}

fnx_size_t fnx_super_getnbckts(const fnx_super_t *super)
{
	return super->su_fsinfo.fs_attr.f_nbckts;
}



/*
 * Heuristic calculation of fs attributes and stats for freshly-formatted
 * file-system over raw data volume. Excludes first meta-data section and
 * first block within each bucket (section).
 * Assuming no more then single inode per block.
 */
void fnx_super_devise_fs(fnx_super_t *super, fnx_size_t vol_nbytes)
{
	fnx_bkcnt_t vol_nbk, bused;
	fnx_size_t nbckt, nbk_user;
	fnx_fsinfo_t *fsinfo = &super->su_fsinfo;

	vol_nbk  = fnx_bytes_to_nbk(vol_nbytes);
	bused    = fnx_get_dobjnfrgs(FNX_VTYPE_DIR);
	nbckt    = fnx_nbk_to_nbckt(vol_nbk) - 1;
	nbk_user = nbckt * (FNX_BCKTNBK - 1);

	fsinfo->fs_stat.f_apex_ino  = FNX_INO_APEX0;
	fsinfo->fs_stat.f_apex_vlba = FNX_LBA_APEX0;
	fsinfo->fs_stat.f_inodes    = nbk_user;
	fsinfo->fs_stat.f_iused     = 1; /* Root-dir */
	fsinfo->fs_stat.f_bfrgs     = fnx_nbk_to_nfrg(nbk_user);
	fsinfo->fs_stat.f_bused     = bused;
	fsinfo->fs_stat.f_super     = 1;
	fsinfo->fs_stat.f_dir       = 1;
	fsinfo->fs_attr.f_nbckts    = nbckt;
	fsinfo->fs_attr.f_rootino   = FNX_INO_ROOT;
	super->su_volsize   = vol_nbytes;
}


/*
 * Resolves virtual address to space-map's block physical address.
 */
void fnx_super_resolve_sm(const fnx_super_t *super,
                          const fnx_vaddr_t *vaddr,
                          fnx_vaddr_t *vaddr_spmap,
                          fnx_baddr_t *baddr_spmap)
{
	fnx_hash_t hash;
	fnx_size_t vidx, nbckts;

	nbckts = fnx_super_getnbckts(super);
	hash   = fnx_vaddr_hash(vaddr);
	vidx   = (hash % nbckts) + 1;
	fnx_vaddr_for_spmap(vaddr_spmap, vidx);
	fnx_baddr_for_spmap(baddr_spmap, vidx);
}


