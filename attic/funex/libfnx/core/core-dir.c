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
#include <errno.h>

#include <fnxinfra.h>
#include "core-blobs.h"
#include "core-types.h"
#include "core-stats.h"
#include "core-htod.h"
#include "core-addr.h"
#include "core-addri.h"
#include "core-elems.h"
#include "core-space.h"
#include "core-super.h"
#include "core-inode.h"
#include "core-inodei.h"
#include "core-dir.h"
#include "core-diri.h"


#define FNX_DIR_INIT_MODE \
	(S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)

#define FNX_DDIRSIZE (sizeof(fnx_ddir_t))


/* Locals */
static void dir_iref(fnx_dir_t *, fnx_dirent_t *, const fnx_inode_t *);
static void dir_iref_self(fnx_dir_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void dirent_clear(fnx_dirent_t *dirent)
{
	dirent->de_ino  = FNX_INO_NULL;
	dirent->de_hash = 0;
	dirent->de_nlen = 0;
}

static int dirent_isfree(const fnx_dirent_t *dirent)
{
	return (dirent->de_ino == FNX_INO_NULL);
}

static int dirent_isused(const fnx_dirent_t *dirent)
{
	return !dirent_isfree(dirent);
}

static int dirent_isspecial(const fnx_dirent_t *dirent)
{
	const fnx_off_t doff = dirent->de_doff;
	return ((doff == FNX_DOFF_SELF) || (doff == FNX_DOFF_PARENT));
}

static int
dirent_isusedby(const fnx_dirent_t *dirent,
                fnx_hash_t hash, fnx_size_t nlen, fnx_ino_t ino)
{
	if ((dirent->de_hash != hash) ||
	    (dirent->de_nlen != nlen)) {
		return FNX_FALSE;
	}
	if ((ino != FNX_INO_ANY) &&
	    (dirent->de_ino != ino)) {
		return FNX_FALSE;
	}
	return FNX_TRUE;
}

static void dirent_assign(fnx_dirent_t *dirent, const fnx_inode_t *inode)
{
	dirent->de_ino  = fnx_inode_getino(inode);
	dirent->de_hash = inode->i_name.hash;
	dirent->de_nlen = inode->i_name.len;
}

static void encode_ino_nlen(uint64_t *val, fnx_ino_t ino, fnx_size_t nlen)
{
	*val = ((uint64_t)(ino) << 8) | ((uint64_t)(nlen & 0xFF));
}

static void decode_ino_nlen(uint64_t val, fnx_ino_t *ino, fnx_size_t *nlen)
{
	*ino  = (fnx_ino_t)(val >> 8);
	*nlen = (fnx_size_t)(val & 0xFF);
}

static void dirent_clone(const fnx_dirent_t *dirent, fnx_dirent_t *other)
{
	other->de_doff  = dirent->de_doff;
	other->de_hash  = dirent->de_hash;
	other->de_nlen  = dirent->de_nlen;
	other->de_ino   = dirent->de_ino;
}

static void dirent_htod(const fnx_dirent_t *dirent, fnx_ddirent_t *ddirent)
{
	uint64_t ino_namelen = 0;

	encode_ino_nlen(&ino_namelen, dirent->de_ino, dirent->de_nlen);
	ddirent->de_namehash      = fnx_htod_hash(dirent->de_hash);
	ddirent->de_ino_namelen   = fnx_htod_u64(ino_namelen);
}

static void dirent_dtoh(fnx_dirent_t *dirent, const fnx_ddirent_t *ddirent)
{
	uint64_t ino_namelen;

	ino_namelen     = fnx_dtoh_u64(ddirent->de_ino_namelen);
	dirent->de_hash = fnx_dtoh_hash(ddirent->de_namehash);
	decode_ino_nlen(ino_namelen, &dirent->de_ino, &dirent->de_nlen);
}


static void dents_init(fnx_dirent_t dents[], size_t nelems, fnx_off_t base)
{
	for (size_t i = 0; i < nelems; ++i) {
		dirent_clear(&dents[i]);
		dents[i].de_doff = base + (fnx_off_t)i;
	}
}

static void dents_setup(fnx_dirent_t dents[], size_t nelems, fnx_off_t base)
{
	for (size_t i = 0; i < nelems; ++i) {
		dents[i].de_doff = base + (fnx_off_t)i;
	}
}

static void
dents_clone(const fnx_dirent_t dents[], fnx_dirent_t others[], size_t nelems)
{
	for (size_t i = 0; i < nelems; ++i) {
		dirent_clone(&dents[i], &others[i]);
	}
}

static fnx_dirent_t *
dents_lookup(const fnx_dirent_t dents[], size_t nelems, size_t ndents,
             fnx_hash_t hash, fnx_size_t nlen)
{
	size_t pos, cnt = 0;
	const fnx_dirent_t *dirent = NULL;

	if ((ndents > 0) && (nlen > 0)) {
		pos = hash % nelems;
		for (size_t i = 0; i < nelems; ++i) {
			dirent = &dents[pos];
			if (dirent_isused(dirent) && !dirent_isspecial(dirent)) {
				if (dirent_isusedby(dirent, hash, nlen, FNX_INO_ANY)) {
					return (fnx_dirent_t *)dirent;
				}
				if (++cnt >= ndents) {
					break;
				}
			}
			pos = (pos + 1) % nelems;
		}
	}
	return NULL;
}

static fnx_dirent_t *
dents_search(const fnx_dirent_t dents[], size_t nelems, size_t ndents,
             fnx_off_t doff_base, fnx_off_t doff)
{
	size_t base;
	const fnx_dirent_t *dirent;

	if (ndents > 0) {
		base = (doff >= doff_base) ? fnx_off_len(doff_base, doff) : 0;
		for (size_t i = base; i < nelems; ++i) {
			dirent = &dents[i];
			if (dirent_isused(dirent)) {
				return (fnx_dirent_t *)dirent;
			}
		}
	}
	return NULL;
}


static fnx_dirent_t *dents_hpredict(const fnx_dirent_t dents[], size_t nelems,
                                    size_t ndents, const fnx_hash_t nhash)
{
	size_t pos;
	const fnx_dirent_t *dirent;

	if (ndents < nelems) {
		pos = nhash % nelems;
		for (size_t i = 0; i < nelems; ++i) {
			dirent = &dents[pos];
			if (dirent_isfree(dirent) && !dirent_isspecial(dirent)) {
				return (fnx_dirent_t *)dirent;
			}
			pos = (pos + 1) % nelems;
		}
	}
	return NULL;
}

static fnx_dirent_t *dents_predict(const fnx_dirent_t dents[], size_t nelems,
                                   size_t ndents, const fnx_inode_t *inode)
{
	return dents_hpredict(dents, nelems, ndents, inode->i_name.hash);
}

static fnx_dirent_t *dents_insert(fnx_dirent_t dents[], size_t nelems,
                                  size_t *ndents, const fnx_inode_t *inode)
{
	fnx_dirent_t *dirent;

	dirent = dents_predict(dents, nelems, *ndents, inode);
	if (dirent != NULL) {
		dirent_assign(dirent, inode);
		*ndents += 1;
	}
	return dirent;
}

static fnx_dirent_t *dents_remove(fnx_dirent_t dents[], size_t fnx_nelems,
                                  size_t *ndents, const fnx_inode_t *inode)
{
	size_t pos, cnt = 0;
	fnx_dirent_t *dirent;
	const fnx_ino_t  ino  = fnx_inode_getino(inode);
	const fnx_hash_t hash = inode->i_name.hash;
	const fnx_size_t nlen = inode->i_name.len;

	if (*ndents > 0) {
		pos = hash % fnx_nelems;
		for (size_t i = 0; i < fnx_nelems; ++i) {
			dirent = &dents[pos];
			if (dirent_isused(dirent)) {
				if (dirent_isusedby(dirent, hash, nlen, ino)) {
					dirent_clear(dirent);
					*ndents -= 1;
					return dirent;
				}
				if (++cnt >= *ndents) {
					break;
				}
			}
			pos = (pos + 1) % fnx_nelems;
		}
	}
	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_ddirseg_t *fnx_header_to_ddirseg(const fnx_header_t *hdr)
{
	return fnx_container_of(hdr, fnx_ddirseg_t, ds_hdr);
}

fnx_dirseg_t *fnx_vnode_to_dirseg(const fnx_vnode_t *vnode)
{
	return fnx_container_of(vnode, fnx_dirseg_t, ds_vnode);
}

void fnx_dirseg_init(fnx_dirseg_t *dirseg)
{
	fnx_vnode_init(&dirseg->ds_vnode, FNX_VTYPE_DIRSEG);
	dents_init(dirseg->ds_dent, fnx_nelems(dirseg->ds_dent), 0);
	dirseg->ds_index    = FNX_DOFF_NONE;
	dirseg->ds_ndents   = 0;
	dirseg->ds_magic    = FNX_DIRSEG_MAGIC;
}

void fnx_dirseg_destroy(fnx_dirseg_t *dirseg)
{
	fnx_vnode_destroy(&dirseg->ds_vnode);
	dirseg->ds_index    = FNX_DOFF_NONE;
	dirseg->ds_ndents   = 0;
	dirseg->ds_magic    = 1;
}

void fnx_dirseg_clone(const fnx_dirseg_t *dirseg, fnx_dirseg_t *other)
{
	fnx_vnode_clone(&dirseg->ds_vnode, &other->ds_vnode);
	dents_clone(dirseg->ds_dent, other->ds_dent, fnx_nelems(other->ds_dent));
	other->ds_index     = dirseg->ds_index;
	other->ds_ndents    = dirseg->ds_ndents;
}

void fnx_dirseg_setup(fnx_dirseg_t *dirseg, fnx_off_t dseg)
{
	const fnx_off_t doff_base = fnx_dseg_to_doff(dseg);

	fnx_assert(doff_base >= FNX_DIRHNDENT);
	dirseg->ds_index = dseg;
	dents_setup(dirseg->ds_dent, fnx_nelems(dirseg->ds_dent), doff_base);
}

int fnx_dirseg_isempty(const fnx_dirseg_t *dirseg)
{
	return (dirseg->ds_ndents == 0);
}


void fnx_dirseg_htod(const fnx_dirseg_t *dirseg, fnx_ddirseg_t *ddirseg)
{
	fnx_header_t *hdr = &ddirseg->ds_hdr;

	/* Header & tail */
	fnx_vnode_htod(&dirseg->ds_vnode, hdr);
	fnx_dobj_zpad(hdr, FNX_VTYPE_DIRSEG);

	/* R[0] */
	ddirseg->ds_index   = fnx_htod_u32((uint32_t)dirseg->ds_index);
	ddirseg->ds_ndents  = fnx_htod_u16((uint16_t)dirseg->ds_ndents);
	ddirseg->ds_magic   = fnx_htod_magic(dirseg->ds_magic);

	/* R[1..15] */
	for (size_t i = 0; i < fnx_nelems(ddirseg->ds_dent); ++i) {
		dirent_htod(&dirseg->ds_dent[i], &ddirseg->ds_dent[i]);
	}
}

void fnx_dirseg_dtoh(fnx_dirseg_t *dirseg, const fnx_ddirseg_t *ddirseg)
{
	int rc;
	const fnx_header_t *hdr = &ddirseg->ds_hdr;

	/* Header */
	rc = fnx_dobj_check(hdr, FNX_VTYPE_DIRSEG);
	fnx_assert(rc == 0);
	fnx_vnode_dtoh(&dirseg->ds_vnode, hdr);

	/* R[0] */
	dirseg->ds_index    = (fnx_off_t)fnx_dtoh_u32(ddirseg->ds_index);
	dirseg->ds_ndents   = fnx_dtoh_u16(ddirseg->ds_ndents);

	/* R[1..15] */
	for (size_t i = 0; i < fnx_nelems(dirseg->ds_dent); ++i) {
		dirent_dtoh(&dirseg->ds_dent[i], &ddirseg->ds_dent[i]);
	}

	/* Postop */
	fnx_dirseg_setup(dirseg, dirseg->ds_index);
}

int fnx_dirseg_dcheck(const fnx_header_t *hdr)
{
	int rc;
	fnx_vtype_e vtype;
	fnx_magic_t magic;
	fnx_size_t  value;
	fnx_dirent_t dirent;
	const fnx_ddirseg_t *ddirseg;

	vtype = fnx_dobj_vtype(hdr);
	if (vtype != FNX_VTYPE_DIRSEG) {
		return -1;
	}
	rc = fnx_dobj_check(hdr, vtype);
	if (rc != 0) {
		fnx_assert(0); /* XXX */
		return -1;
	}
	ddirseg = fnx_header_to_ddirseg(hdr);
	magic = fnx_dtoh_magic(ddirseg->ds_magic);
	fnx_assert(magic == FNX_DIRSEG_MAGIC);
	if (magic != FNX_DIRSEG_MAGIC) {
		return -1;
	}
	value = 0;
	for (size_t i = 0; i < fnx_nelems(ddirseg->ds_dent); ++i) {
		dirent_dtoh(&dirent, &ddirseg->ds_dent[i]);
		if (dirent_isused(&dirent)) {
			value += 1;
		}
	}
	fnx_assert(value == ddirseg->ds_ndents);
	if (value != ddirseg->ds_ndents) {
		return -1;
	}

	/* TODO: Check base */

	return 0;
}


fnx_dirent_t *fnx_dirseg_lookup(const fnx_dirseg_t *dirseg,
                                fnx_hash_t hash, fnx_size_t nlen)
{
	const size_t nel = fnx_nelems(dirseg->ds_dent);
	return dents_lookup(dirseg->ds_dent, nel, dirseg->ds_ndents, hash, nlen);
}

fnx_dirent_t *fnx_dirseg_ilookup(const fnx_dirseg_t *dirseg,
                                 const fnx_inode_t *inode)
{
	return fnx_dirseg_lookup(dirseg, inode->i_name.hash, inode->i_name.len);
}

fnx_dirent_t *fnx_dirseg_search(const fnx_dirseg_t *dirseg, fnx_off_t doff)
{
	const fnx_off_t doff_base = fnx_dseg_to_doff(dirseg->ds_index);
	const size_t nel = fnx_nelems(dirseg->ds_dent);

	return dents_search(dirseg->ds_dent, nel,
	                    dirseg->ds_ndents, doff_base, doff);
}

fnx_dirent_t *fnx_dirseg_link(fnx_dirseg_t *dirseg, const fnx_inode_t *inode)
{
	const size_t nel = fnx_nelems(dirseg->ds_dent);
	return dents_insert(dirseg->ds_dent, nel, &dirseg->ds_ndents, inode);
}

fnx_dirent_t *fnx_dirseg_unlink(fnx_dirseg_t *dirseg, const fnx_inode_t *inode)
{
	const size_t nel = fnx_nelems(dirseg->ds_dent);
	return dents_remove(dirseg->ds_dent, nel, &dirseg->ds_ndents, inode);
}

fnx_dirent_t *fnx_dirseg_predict(const fnx_dirseg_t *dirseg, fnx_hash_t nhash)
{
	const size_t nel = fnx_nelems(dirseg->ds_dent);
	return dents_hpredict(dirseg->ds_dent, nel, dirseg->ds_ndents, nhash);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_ddir_t *fnx_dinode_to_ddir(const fnx_dinode_t *dinode)
{
	return fnx_container_of(dinode, fnx_ddir_t, d_inode);
}

fnx_ddir_t *fnx_header_to_ddir(const fnx_header_t *hdr)
{
	return fnx_dinode_to_ddir(fnx_header_to_dinode(hdr));
}

fnx_dir_t *fnx_vnode_to_dir(const fnx_vnode_t *vnode)
{
	return fnx_inode_to_dir(fnx_vnode_to_inode(vnode));
}

fnx_dir_t *fnx_inode_to_dir(const fnx_inode_t *inode)
{
	return fnx_container_of(inode, fnx_dir_t, d_inode);
}

void fnx_dir_init(fnx_dir_t *dir)
{
	fnx_inode_t *inode = &dir->d_inode;

	fnx_inode_init(inode, FNX_VTYPE_DIR);
	dents_init(dir->d_dent, fnx_nelems(dir->d_dent), FNX_DOFF_SELF);
	fnx_bzero(dir->d_segs, sizeof(dir->d_segs));
	dir->d_nchilds  = 0;
	dir->d_ndents   = 0;
	dir->d_nsegs    = 0;
	dir->d_rootd    = FNX_FALSE;
	dir->d_magic    = FNX_DIR_MAGIC;

	inode->i_iattr.i_mode   = FNX_DIR_INIT_MODE;
	inode->i_iattr.i_nlink  = FNX_DIR_INIT_NLINK;
	inode->i_iattr.i_size   = FNX_DDIRSIZE;
}

void fnx_dir_destroy(fnx_dir_t *dir)
{
	fnx_foreach_arrelem(dir->d_dent, dirent_clear);
	fnx_inode_destroy(&dir->d_inode);
	dir->d_nchilds  = 0;
	dir->d_ndents   = 0;
	dir->d_nsegs    = 0;
	dir->d_magic    = 5;
}

void fnx_dir_setup(fnx_dir_t *dir, const fnx_uctx_t *uctx, fnx_mode_t mode)
{
	fnx_inode_setup(&dir->d_inode, uctx, mode, 0);

	dir_iref_self(dir);
	if (fnx_dir_isroot(dir)) {
		fnx_dir_iref_parentd(dir, dir);
	}
}

void fnx_dir_clone(const fnx_dir_t *dir, fnx_dir_t *other)
{
	fnx_inode_clone(&dir->d_inode, &other->d_inode);
	for (size_t i = 0; i < fnx_nelems(other->d_dent); ++i) {
		dirent_clone(&dir->d_dent[i], &other->d_dent[i]);
	}
	fnx_bcopy(other->d_segs, dir->d_segs, sizeof(other->d_segs));
	other->d_ndents  = dir->d_ndents;
	other->d_nsegs   = dir->d_nsegs;
	other->d_nchilds = dir->d_nchilds;
}

void fnx_dir_htod(const fnx_dir_t *dir, fnx_ddir_t *ddir)
{
	/* R[0..7] */
	fnx_inode_htod(&dir->d_inode, &ddir->d_inode);

	/* R[8..63] */
	ddir->d_rootd   = dir->d_rootd;
	ddir->d_nchilds = fnx_htod_u32((uint32_t)dir->d_nchilds);
	ddir->d_ndents  = fnx_htod_u32((uint32_t)dir->d_ndents);
	ddir->d_nsegs   = fnx_htod_u32((uint32_t)dir->d_nsegs);
	for (size_t i = 0; i < fnx_nelems(ddir->d_dent); ++i) {
		dirent_htod(&dir->d_dent[i], &ddir->d_dent[i]);
	}

	/* R[64..127] */
	for (size_t j = 0; j < fnx_nelems(ddir->d_segs); ++j) {
		ddir->d_segs[j] = fnx_htod_u32(dir->d_segs[j]);
	}
}

void fnx_dir_dtoh(fnx_dir_t *dir, const fnx_ddir_t *ddir)
{
	/* R[0..7] */
	fnx_inode_dtoh(&dir->d_inode, &ddir->d_inode);

	/* R[8..63] */
	dir->d_rootd    = ddir->d_rootd;
	dir->d_nchilds  = fnx_dtoh_u32(ddir->d_nchilds);
	dir->d_ndents   = fnx_dtoh_u32(ddir->d_ndents);
	dir->d_nsegs    = fnx_dtoh_u32(ddir->d_nsegs);
	for (size_t i = 0; i < fnx_nelems(dir->d_dent); ++i) {
		dirent_dtoh(&dir->d_dent[i], &ddir->d_dent[i]);
	}

	/* R[64..127] */
	for (size_t j = 0; j < fnx_nelems(dir->d_segs); ++j) {
		dir->d_segs[j] = fnx_dtoh_u32(ddir->d_segs[j]);
	}
}

int fnx_dir_dcheck(const fnx_header_t *hdr)
{
	int rc;
	uint32_t mask;
	fnx_size_t value, value1, value2, limit;
	const fnx_ddir_t *ddir;

	rc = fnx_dobj_check(hdr, FNX_VTYPE_DIR);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_inode_dcheck(hdr);
	if (rc != 0) {
		return rc;
	}
	ddir = fnx_header_to_ddir(hdr);
	value = fnx_dtoh_u32(ddir->d_nchilds);
	limit = FNX_DIRCHILD_MAX;
	if (value > limit) {
		fnx_warn("illegal-nchilds: d_nchilds=%d limit=%d",
		         (int)value, (int)limit);
		return -1;
	}
	value = fnx_dtoh_u32(ddir->d_ndents);
	limit = FNX_DIRHNDENT;
	if (value && ((value > limit) || (value < 2))) {
		fnx_warn("illegal-ndents: d_ndents=%d limit=%lu", (int)value, limit);
		return -1;
	}

	value1 = 0;
	for (size_t i = 0; i < fnx_nelems(ddir->d_segs); ++i) {
		mask = fnx_dtoh_u32(ddir->d_segs[i]);
		value1 += (fnx_size_t)fnx_popcount(mask);
	}
	value2 = fnx_dtoh_u32(ddir->d_nsegs);
	fnx_assert(value1 == value2);
	if (value1 != value2) {
		return -1;
	}
	return 0;
}


/*
 * Calculate 64-bits hash value for entry-name within directory.
 *
 * TODO: Use directory meta-info to detramian hash-function.
 */
fnx_hash_t fnx_inamehash(const fnx_name_t *name, const fnx_dir_t *dir)
{
	const fnx_ino_t dino = fnx_dir_getino(dir);
	return fnx_calc_inamehash(name, dino);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_dirent_t *self_dirent(const fnx_dir_t *dir)
{
	const fnx_dirent_t *dirent = &dir->d_dent[FNX_DOFF_SELF];
	return (fnx_dirent_t *)dirent;
}

static fnx_dirent_t *parent_dirent(const fnx_dir_t *dir)
{
	const fnx_dirent_t *dirent = &dir->d_dent[FNX_DOFF_PARENT];
	return (fnx_dirent_t *)dirent;
}

static void dir_iref(fnx_dir_t *dir, fnx_dirent_t *dent,
                     const fnx_inode_t *inode)
{
	fnx_assert(dir->d_ndents < fnx_nelems(dir->d_dent));
	dirent_assign(dent, inode);
	dir->d_ndents += 1;
}

static void dir_iref_self(fnx_dir_t *dir)
{
	dir_iref(dir, self_dirent(dir), &dir->d_inode);
}

void fnx_dir_iref_parentd(fnx_dir_t *dir, const fnx_dir_t *parentd)
{
	dir_iref(dir, parent_dirent(dir), &parentd->d_inode);
}

void fnx_dir_unref_parentd(fnx_dir_t *dir)
{
	fnx_assert(dir->d_ndents > 0);
	dirent_clear(parent_dirent(dir));
	dir->d_ndents -= 1;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Lookup for special-dirent */
const fnx_dirent_t *fnx_dir_meta(const fnx_dir_t *dir, const fnx_name_t *name)
{
	const fnx_dirent_t *dirent;

	/* Map "." to self */
	dirent = self_dirent(dir);
	if (fnx_name_isnequal(name, ".", 1)) {
		return dirent;
	}
	/* Map ".." to parent */
	dirent = parent_dirent(dir);
	if (fnx_name_isnequal(name, "..", 2)) {
		return dirent;
	}
	return NULL;
}

fnx_dirent_t *fnx_dir_lookup(const fnx_dir_t *dir, fnx_hash_t hash, size_t nlen)
{
	fnx_dirent_t *dirent  = NULL;
	const size_t beg = FNX_DOFF_BEGIN;
	const size_t nel = fnx_nelems(dir->d_dent) - beg;

	if (!fnx_dir_isempty(dir)) {
		dirent = dents_lookup(dir->d_dent + beg, nel,
		                      dir->d_ndents, hash, nlen);
	}
	return dirent;
}

fnx_dirent_t *fnx_dir_ilookup(const fnx_dir_t *dir, const fnx_inode_t *inode)
{
	return fnx_dir_lookup(dir, inode->i_name.hash, inode->i_name.len);
}

const fnx_dirent_t *fnx_dir_search(const fnx_dir_t *dir, fnx_off_t doff)
{
	const fnx_dirent_t *dirent;
	const size_t beg = FNX_DOFF_BEGIN;
	const size_t nel = fnx_nelems(dir->d_dent) - beg;
	const fnx_off_t pos = (fnx_off_t)beg;

	if (doff == FNX_DOFF_SELF) {
		dirent = self_dirent(dir);
	} else if (doff == FNX_DOFF_PARENT) {
		dirent = parent_dirent(dir);
	} else {
		dirent = dents_search(dir->d_dent + beg, nel, dir->d_ndents, pos, doff);
	}
	return dirent;
}

fnx_dirent_t *fnx_dir_link(fnx_dir_t *dir, const fnx_inode_t *inode)
{
	const size_t beg = FNX_DOFF_BEGIN;
	const size_t nel = fnx_nelems(dir->d_dent) - beg;

	return dents_insert(dir->d_dent + beg, nel, &dir->d_ndents, inode);
}

fnx_dirent_t *fnx_dir_unlink(fnx_dir_t *dir, const fnx_inode_t *inode)
{
	const size_t beg = FNX_DOFF_BEGIN;
	const size_t nel = fnx_nelems(dir->d_dent) - beg;

	return dents_remove(dir->d_dent + beg, nel, &dir->d_ndents, inode);
}

fnx_dirent_t *fnx_dir_predict(const fnx_dir_t *dir, fnx_hash_t nhash)
{
	const size_t beg = FNX_DOFF_BEGIN;
	const size_t nel = fnx_nelems(dir->d_dent) - beg;

	return dents_hpredict(dir->d_dent + beg, nel, dir->d_ndents, nhash);
}

void fnx_dir_associate_child(const fnx_dir_t  *dir,
                             const fnx_name_t *name,
                             fnx_inode_t      *child)
{
	fnx_ino_t dino;

	dino = fnx_dir_getino(dir);
	fnx_inode_associate(child, dino, name);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int dseg_isvalid(fnx_off_t dseg)
{
	return ((dseg >= 0) && (dseg < FNX_DIRNSEGS));
}

static fnx_bits_t dseg_to_mask(fnx_off_t dseg)
{
	return (1u << (dseg % 32));
}

static fnx_size_t dseg_to_bpos(fnx_off_t dseg)
{
	return ((fnx_size_t)dseg / 32);
}

int fnx_dir_hasseg(const fnx_dir_t *dir, fnx_off_t dseg)
{
	int res = FNX_FALSE;
	const fnx_bits_t mask = dseg_to_mask(dseg);
	const fnx_size_t bpos = dseg_to_bpos(dseg);

	fnx_assert(dseg_isvalid(dseg));
	fnx_assert(bpos < fnx_nelems(dir->d_segs));
	if (dir->d_nsegs > 0) {
		res = fnx_testf(dir->d_segs[bpos], mask);
	}
	return res;
}

void fnx_dir_setseg(fnx_dir_t *dir, fnx_off_t dseg)
{
	const fnx_bits_t mask = dseg_to_mask(dseg);
	const fnx_size_t bpos = dseg_to_bpos(dseg);

	fnx_assert(dseg_isvalid(dseg));
	fnx_assert(bpos < fnx_nelems(dir->d_segs));
	fnx_assert(dir->d_nsegs < FNX_DIRNSEGS);
	fnx_setf(&dir->d_segs[bpos], mask);
	dir->d_nsegs += 1;
}

void fnx_dir_unsetseg(fnx_dir_t *dir, fnx_off_t dseg)
{
	const fnx_bits_t mask = dseg_to_mask(dseg);
	const fnx_size_t bpos = dseg_to_bpos(dseg);

	fnx_assert(dseg_isvalid(dseg));
	fnx_assert(bpos < fnx_nelems(dir->d_segs));
	fnx_assert(dir->d_nsegs > 0);
	fnx_unsetf(&dir->d_segs[bpos], mask);
	dir->d_nsegs -= 1;
}

fnx_off_t fnx_dir_nextseg(const fnx_dir_t *dir, fnx_off_t dseg)
{
	fnx_off_t  ditr;
	fnx_size_t bpos;
	fnx_bits_t bits, mask;

	ditr = dseg;
	bpos = dseg_to_bpos(ditr);
	while (bpos < fnx_nelems(dir->d_segs)) {
		bits = dir->d_segs[bpos];
		if (bits) {
			mask = dseg_to_mask(ditr);
			while (mask) {
				if (fnx_testf(bits, mask)) {
					return ditr;
				}
				ditr = ditr + 1;
				mask = mask << 1;
			}
		}
		bpos = bpos + 1;
		ditr = (fnx_off_t)bpos * 32;
	}
	return FNX_DOFF_NONE;
}
