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


/* Local macros */
#define put_vnode(vproc, v) \
	fnx_vproc_put_vnode(vproc, v)


/* Local functions */
static int vproc_acquire_dirseg(fnx_vproc_t *, const fnx_dir_t *,
                                fnx_off_t , fnx_dirseg_t **);

static int vproc_fetch_dirseg(fnx_vproc_t *, const fnx_dir_t *,
                              fnx_off_t, fnx_dirseg_t **);

static int vproc_fetch_hdirseg(fnx_vproc_t *, const fnx_dir_t *,
                               fnx_hash_t, fnx_dirseg_t **);

static int vproc_fetch_idirseg(fnx_vproc_t *, const fnx_dir_t *,
                               const fnx_inode_t *, fnx_dirseg_t **);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_nlink_t dir_getnlink(const fnx_dir_t *dir)
{
	return fnx_inode_getnlink(&dir->d_inode);
}

static fnx_inode_t *dir_inode(const fnx_dir_t *dir)
{
	const fnx_inode_t *inode = &dir->d_inode;
	return (fnx_inode_t *)inode;
}

static fnx_vnode_t *dir_vnode(const fnx_dir_t *dir)
{
	const fnx_vnode_t *vnode = &dir->d_inode.i_vnode;
	return (fnx_vnode_t *)vnode;
}

static fnx_hash_t dir_getnhash(const fnx_dir_t  *dir, const fnx_name_t *name)
{
	return (name->hash != 0) ? name->hash : fnx_inamehash(name, dir);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
dir_update_linked(fnx_dir_t *dir, fnx_inode_t *inode, fnx_inode_t *iref)
{
	if (fnx_inode_isreflnk(inode)) {
		fnx_assert(iref != NULL);
		iref->i_iattr.i_nlink++;
	} else {
		inode->i_iattr.i_nlink++;
	}
	dir->d_nchilds++;
}

static void
dir_update_linkedd(fnx_dir_t *parentd, fnx_dir_t *childd)
{
	fnx_inode_t *child = &childd->d_inode;

	fnx_dir_iref_parentd(childd, parentd);
	child->i_iattr.i_nlink++;

	parentd->d_nchilds++;
	parentd->d_inode.i_iattr.i_nlink++;
}

static void
dir_update_unlinked(fnx_dir_t *dir, fnx_inode_t *child, fnx_inode_t *iref)
{
	if (fnx_inode_isreflnk(child)) {
		fnx_assert(iref != NULL);
		fnx_assert(iref->i_iattr.i_nlink > 0);
		iref->i_iattr.i_nlink--;
	} else {
		fnx_assert(child->i_iattr.i_nlink > 0);
		child->i_iattr.i_nlink--;
	}
	fnx_assert(dir->d_nchilds > 0);
	dir->d_nchilds--;
}

static void
dir_update_unlinkedd(fnx_dir_t *parentd, fnx_dir_t *childd)
{
	fnx_assert(childd->d_inode.i_iattr.i_nlink > 0);
	fnx_dir_unref_parentd(childd);
	childd->d_inode.i_iattr.i_nlink--;

	fnx_assert(parentd->d_nchilds > 0);
	fnx_assert(parentd->d_inode.i_iattr.i_nlink > 0);
	parentd->d_nchilds--;
	parentd->d_inode.i_iattr.i_nlink--;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int vproc_require_mutable(fnx_vproc_t *vproc, const fnx_vnode_t *vnode)
{
	if (!fnx_vnode_ismutable(vnode)) {
		return FNX_EPEND;
	}
	fnx_unused(vproc);
	return 0;
}

static int vproc_require_mutabled(fnx_vproc_t *vproc, const fnx_dir_t *dir)
{
	return vproc_require_mutable(vproc, dir_vnode(dir));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int vproc_lookup_special(fnx_vproc_t       *vproc,
                                const fnx_dir_t   *dir,
                                const fnx_name_t  *name,
                                fnx_inode_t      **inode)
{
	int rc;
	fnx_bool_t isroot;
	const fnx_dirent_t *dirent;

	/* Case 1: pseudo-namespace's root */
	*inode = NULL;
	isroot = fnx_dir_isroot(dir);
	if (isroot && fnx_name_isequal(name, FNX_PSROOTNAME)) {
		rc = fnx_vproc_fetch_inode(vproc, fnx_ino_psroot(), inode);
		if ((rc == 0) && fnx_inode_hasname(*inode, name)) {
			return 0;
		}
	}
	/* Case 2: dot or dot-dot */
	*inode = NULL;
	dirent = fnx_dir_meta(dir, name);
	if (dirent == NULL) {
		return -ENOENT;
	}
	rc = fnx_vproc_fetch_inode(vproc, dirent->de_ino, inode);
	if (rc != 0) {
		return rc;
	}
	if (!fnx_inode_hasname(*inode, name)) {
		return -ENOENT;
	}
	return 0;
}

static int vproc_dirent_to_inode(fnx_vproc_t        *vproc,
                                 const fnx_dirent_t *dent,
                                 const fnx_name_t   *name,
                                 fnx_inode_t       **p_inode)
{
	int rc, res;
	fnx_inode_t *inode = NULL;

	rc = fnx_vproc_fetch_inode(vproc, dent->de_ino, &inode);
	if (rc != 0) {
		return rc;
	}
	fnx_assert(inode != NULL);
	fnx_assert(inode->i_magic == FNX_INODE_MAGIC);
	fnx_assert(inode->i_vnode.v_vaddr.ino == dent->de_ino);
	res = fnx_inode_hasname(inode, name);
	if (!res) {
		return -ENOENT;
	}
	/* TODO: Improve me */
	*p_inode = inode;
	return 0;
}

static int vproc_lookup_dirent(fnx_vproc_t      *vproc,
                               const fnx_dir_t  *dir,
                               const fnx_name_t *name,
                               fnx_dirent_t    **dirent)
{
	int rc;
	fnx_hash_t  nhash;
	fnx_dirseg_t *dirseg = NULL;

	/* Lookup within top-node */
	nhash = dir_getnhash(dir, name);
	*dirent = fnx_dir_lookup(dir, nhash, name->len);
	if (*dirent != NULL) {
		return 0; /* OK, match at dir-top */
	}
	/* Lookup within dir sub-segment node */
	rc = vproc_fetch_hdirseg(vproc, dir, nhash, &dirseg);
	if (rc != 0) {
		return rc;
	}
	*dirent = fnx_dirseg_lookup(dirseg, nhash, name->len);
	if (*dirent != NULL) {
		return 0;
	}

	/* No entry in all levels */
	return -ENOENT;
}

static int vproc_lookup_cached_de(fnx_vproc_t      *vproc,
                                  const fnx_dir_t  *dir,
                                  const fnx_name_t *name,
                                  fnx_inode_t     **inode)
{
	int rc;
	fnx_ino_t dino, ino;

	dino = fnx_dir_getino(dir);
	ino  = fnx_vcache_lookup_de(&vproc->cache->vc, dino, name->hash, name->len);
	if (ino == FNX_INO_NULL) {
		return -ENOENT;
	}
	rc = fnx_vproc_fetch_inode(vproc, ino, inode);
	if (rc != 0) {
		return rc;
	}
	if (!fnx_inode_hasname(*inode, name)) {
		return -ENOENT;
	}
	return 0;
}

int fnx_vproc_lookup_iinode(fnx_vproc_t      *vproc,
                            const fnx_dir_t  *dir,
                            const fnx_name_t *name,
                            fnx_inode_t     **inode)
{
	int rc;
	fnx_dirent_t *dirent = NULL;

	/* Ensure result inode is NULL, in-case of optional lookup */
	*inode = NULL;

	/* Lookup for dot, dot-dot or pseudo */
	rc = vproc_lookup_special(vproc, dir, name, inode);
	if (rc == 0) {
		return 0;
	}
	/* Try dentries-cache */
	rc = vproc_lookup_cached_de(vproc, dir, name, inode);
	if (rc == 0) {
		return 0;
	}
	/* Lookup within dir-subs tree */
	rc = vproc_lookup_dirent(vproc, dir, name, &dirent);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_dirent_to_inode(vproc, dirent, name, inode);
	if (rc != 0) {
		return rc; /* TODO: keep on iter iff rc==-ENOENT*/
	}
	return 0;
}

/* Lookup for optional inode */
int fnx_vproc_lookup_ientry(fnx_vproc_t      *vproc,
                            const fnx_dir_t  *dir,
                            const fnx_name_t *name,
                            fnx_inode_t     **inode)
{
	int rc;

	rc = fnx_vproc_lookup_iinode(vproc, dir, name, inode);
	return ((rc == 0) || (rc == -ENOENT)) ? 0 : rc;
}

int fnx_vproc_lookup_inode(fnx_vproc_t      *vproc,
                           const fnx_dir_t  *dir,
                           const fnx_name_t *name,
                           fnx_inode_t     **p_inode)
{
	int rc;
	fnx_inode_t *inode = NULL;

	rc = fnx_vproc_lookup_iinode(vproc, dir, name, &inode);
	if (rc != 0) {
		return rc;
	}
	if (fnx_inode_isreflnk(inode)) {
		/* Fetch referenced inode */
		rc = fnx_vproc_fetch_inode(vproc, inode->i_refino, &inode);
		if (rc != 0) {
			return rc;
		}
	}
	*p_inode = inode;
	return 0;
}

int fnx_vproc_lookup_link(fnx_vproc_t      *vproc,
                          const fnx_dir_t  *dir,
                          const fnx_name_t *name,
                          fnx_inode_t     **inode)
{
	int rc;
	fnx_inode_t *hlnk = NULL;

	/* NB: Must check here for inode; doing lookup for the link itself! */
	rc = fnx_vproc_lookup_iinode(vproc, dir, name, &hlnk);
	if (rc != 0) {
		return rc;
	}
	if (fnx_inode_isdir(hlnk)) {
		return -EISDIR;
	}
	*inode = hlnk;
	return 0;
}

int fnx_vproc_lookup_dir(fnx_vproc_t      *vproc,
                         const fnx_dir_t  *parentd,
                         const fnx_name_t *name,
                         fnx_dir_t       **dir)
{
	int rc;
	fnx_inode_t *inode = NULL;

	rc = fnx_vproc_lookup_inode(vproc, parentd, name, &inode);
	if (rc != 0) {
		return rc;
	}
	if (!fnx_inode_isdir(inode)) {
		return -ENOTDIR;
	}
	*dir = fnx_inode_to_dir(inode);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_off_t doff_plus(const fnx_dirent_t *dirent)
{
	/* TODO: Improve? */
	fnx_off_t doff, doff_next = FNX_DOFF_NONE;

	doff = dirent->de_doff;
	if ((doff >= FNX_DOFF_SELF) && (doff < FNX_DOFF_END)) {
		doff_next = doff + 1;
	}
	return doff_next;
}

static int vproc_search_dirtop(fnx_vproc_t     *vproc,
                               const fnx_dir_t *dir,
                               fnx_off_t        doff,
                               fnx_dirent_t   **dirent,
                               fnx_off_t       *doff_next)
{
	if ((doff < 0) || (FNX_DOFF_BEGINS <= doff)) {
		return FNX_EEOS;
	}
	*dirent = (fnx_dirent_t *)fnx_dir_search(dir, doff);
	if (*dirent == NULL) {
		return FNX_EEOS;
	}
	*doff_next = doff_plus(*dirent);
	fnx_unused(vproc);
	return 0;
}

static int vproc_search_dirseg(fnx_vproc_t     *vproc,
                               const fnx_dir_t *dir,
                               fnx_off_t        doff,
                               fnx_dirent_t   **dirent,
                               fnx_off_t       *doff_next)
{
	int rc, cnt = 0;
	fnx_off_t dseg, dlim;
	fnx_dirseg_t *dirseg = NULL;

	if (doff >= FNX_DOFF_END) {
		return FNX_EEOS;
	}
	if (dir->d_nsegs == 0) {
		return FNX_EEOS;
	}
	doff = fnx_off_max(doff, FNX_DOFF_BEGINS);
	dseg = fnx_doff_to_dseg(doff);
	dlim = fnx_doff_to_dseg(FNX_DOFF_END);
	while ((dseg < dlim) && (dseg != FNX_DOFF_NONE)) {
		if (cnt++ > 0) {
			doff = fnx_dseg_to_doff(dseg);
		}
		rc = vproc_fetch_dirseg(vproc, dir, dseg, &dirseg);
		if (rc == 0) {
			*dirent = fnx_dirseg_search(dirseg, doff);
			if (*dirent != NULL) {
				*doff_next = doff_plus(*dirent);
				return 0; /* Habemus dirent */
			}
		} else if (rc != -ENOENT) {
			return rc;
		}
		dseg = fnx_dir_nextseg(dir, dseg + 1);
	}
	return FNX_EEOS;
}

static int vproc_search_dirent(fnx_vproc_t     *vproc,
                               const fnx_dir_t *dir,
                               fnx_off_t        doff,
                               fnx_dirent_t   **dirent,
                               fnx_off_t       *doff_next)
{
	int rc = FNX_EEOS;

	fnx_assert(dir->d_dent[FNX_DOFF_PARENT].de_ino != FNX_INO_NULL);

	/* Nothing to look for if non-valid dir-offset */
	if (!fnx_doff_isvalid(doff)) {
		return FNX_EEOS;
	}

	/* Search for top-node child */
	*dirent     = NULL;
	*doff_next  = FNX_DOFF_NONE;
	rc = vproc_search_dirtop(vproc, dir, doff, dirent, doff_next);
	if ((rc == 0) || (rc != FNX_EEOS)) {
		return rc;
	}
	/* Search within sub-segments */
	rc = vproc_search_dirseg(vproc, dir, doff, dirent, doff_next);
	if ((rc == 0) || (rc != FNX_EEOS)) {
		return rc;
	}
	/* End-of-stream */
	return FNX_EEOS;
}

static int search_normal(fnx_vproc_t     *vproc,
                         const fnx_dir_t *dir,
                         fnx_off_t        doff,
                         fnx_inode_t    **hlnk,
                         fnx_inode_t    **child,
                         fnx_off_t       *doff_next)
{
	int rc;
	fnx_inode_t *reflnk, *inode = NULL;
	fnx_dirent_t *dirent = NULL;

	/* Fast-lookup for self */
	if (doff == FNX_DOFF_SELF) {
		*child      = dir_inode(dir);
		*doff_next  = FNX_DOFF_PARENT;
		return 0;
	}
	/* Perform dir's multi-level search for child's entry */
	rc = vproc_search_dirent(vproc, dir, doff, &dirent, doff_next);
	if (rc != 0) {
		return rc;
	}
	/* Resolve from dirent to actual inode */
	rc = fnx_vproc_fetch_inode(vproc, dirent->de_ino, &inode);
	if (rc != 0) {
		return rc;
	}
	fnx_assert(inode != NULL);
	*hlnk = inode;

	/* May need to do extra resolve in case of hard-link */
	if (fnx_inode_isreflnk(inode)) {
		reflnk  = inode;
		rc = fnx_vproc_fetch_inode(vproc, reflnk->i_refino, &inode);
		if (rc != 0) {
			return rc;
		}
		fnx_assert(inode != NULL);
	}
	*child = inode;
	return 0;
}

static int search_pseudo(fnx_vproc_t     *vproc,
                         const fnx_dir_t *dir,
                         fnx_off_t        doff,
                         fnx_inode_t    **hlnk,
                         fnx_inode_t    **child,
                         fnx_off_t       *doff_next)
{
	int rc;
	fnx_inode_t *inode;

	if (!fnx_doff_isvalid(doff)) {
		return FNX_EEOS;
	}
	if (!fnx_dir_isroot(dir)) {
		return FNX_EEOS;
	}
	rc = fnx_vproc_fetch_inode(vproc, fnx_ino_psroot(), &inode);
	fnx_assert(rc == 0);
	fnx_unused(rc);
	*hlnk = *child = inode;
	*doff_next = FNX_DOFF_NONE;
	return 0;
}

static int search_dir(fnx_vproc_t     *vproc,
                      const fnx_dir_t *dir,
                      fnx_off_t        doff,
                      fnx_inode_t    **hlnk,
                      fnx_inode_t    **child,
                      fnx_off_t       *doff_next)
{
	int rc = 0;

	/* By default, no next dir-offset */
	*doff_next = FNX_DOFF_NONE;

	/* Directory's two-level search for child's entry */
	rc = search_normal(vproc, dir, doff, hlnk, child, doff_next);
	if ((rc != 0) && (rc != FNX_EPEND)) {
		/* Special case for root-dir with associated pseudo namespace */
		rc = search_pseudo(vproc, dir, doff, hlnk, child, doff_next);
	}
	return rc;
}

/* Returns inode's name; for self and parent returns special string-names */
static fnx_name_t *getname(fnx_inode_t *inode, fnx_off_t doff)
{
	static fnx_name_t s_dot1 = { .hash = 0, .len = 1, .str = "." };
	static fnx_name_t s_dot2 = { .hash = 0, .len = 2, .str = ".." };

	if (doff == FNX_DOFF_SELF) {
		return &s_dot1;
	}
	if (doff == FNX_DOFF_PARENT) {
		return &s_dot2;
	}
	return &inode->i_name;
}

int fnx_vproc_search_dent(fnx_vproc_t     *vproc,
                          const fnx_dir_t *dir,
                          fnx_off_t        doff,
                          fnx_name_t     **name,
                          fnx_inode_t    **child,
                          fnx_off_t       *next)
{
	int rc;
	fnx_inode_t *hlnk = NULL;

	rc = search_dir(vproc, dir, doff, &hlnk, child, next);
	if (rc == 0) {
		*name = getname(hlnk, doff);
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int vproc_yield_dirseg(fnx_vproc_t   *vproc,
                              fnx_dir_t     *dir,
                              fnx_hash_t     nhash,
                              fnx_dirseg_t **p_dirseg)
{
	int rc;
	fnx_off_t dseg;
	fnx_dirseg_t *dirseg = NULL;

	dseg = fnx_hash_to_dseg(nhash);
	if (fnx_dir_hasseg(dir, dseg)) {
		rc = vproc_fetch_dirseg(vproc, dir, dseg, &dirseg);
		if (rc != 0) {
			return rc;
		}
		rc = vproc_require_mutable(vproc, &dirseg->ds_vnode);
		if (rc != 0) {
			return rc;
		}
	} else {
		rc = vproc_require_mutabled(vproc, dir);
		if (rc != 0) {
			return rc;
		}
		rc = vproc_acquire_dirseg(vproc, dir, dseg, &dirseg);
		if (rc != 0) {
			return rc;
		}
		fnx_dir_setseg(dir, dseg);
		put_vnode(vproc, dir_vnode(dir));
	}
	*p_dirseg = dirseg;
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void vproc_map_cached_de(fnx_vproc_t       *vproc,
                                const fnx_dir_t   *parentd,
                                const fnx_inode_t *child)
{
	fnx_hash_t hash;
	fnx_size_t nlen;
	fnx_ino_t  dino, ino;
	fnx_cache_t *cache = vproc->cache;

	dino = fnx_dir_getino(parentd);
	hash = child->i_name.hash;
	nlen = child->i_name.len;
	ino  = fnx_inode_getrefino(child);
	fnx_vcache_remap_de(&cache->vc, dino, hash, nlen, ino);
}

static void vproc_unmap_cached_de(fnx_vproc_t       *vproc,
                                  const fnx_dir_t   *parentd,
                                  const fnx_inode_t *child)
{
	fnx_hash_t hash;
	fnx_size_t nlen;
	fnx_ino_t  dino, ino;
	fnx_cache_t *cache = vproc->cache;

	dino = fnx_dir_getino(parentd);
	hash = child->i_name.hash;
	nlen = child->i_name.len;
	ino  = FNX_INO_NULL;
	fnx_vcache_remap_de(&cache->vc, dino, hash, nlen, ino);
}

static void vproc_associate_link(fnx_vproc_t      *vproc,
                                 const fnx_dir_t  *parentd,
                                 const fnx_name_t *name,
                                 fnx_inode_t      *child)
{
	fnx_dir_associate_child(parentd, name, child);
	vproc_map_cached_de(vproc, parentd, child);
}


static void vproc_dissociate_link(fnx_vproc_t      *vproc,
                                  const fnx_dir_t  *parentd,
                                  fnx_inode_t      *child)
{
	vproc_unmap_cached_de(vproc, parentd, child);
	fnx_inode_associate(child, FNX_INO_NULL, NULL);
}

static void vproc_settle_unlinked(fnx_vproc_t  *vproc,
                                  fnx_dir_t    *dir,
                                  fnx_dirseg_t *dirseg)
{
	if (dirseg && fnx_dirseg_isempty(dirseg)) {
		if (fnx_dir_hasseg(dir, dirseg->ds_index)) {
			fnx_dir_unsetseg(dir, dirseg->ds_index);
		}
		dirseg->ds_vnode.v_expired = FNX_TRUE;
		put_vnode(vproc, &dirseg->ds_vnode);
		put_vnode(vproc, dir_vnode(dir));
	}
}

static void vproc_settle_linked(fnx_vproc_t  *vproc,
                                fnx_dir_t    *dir,
                                fnx_dirseg_t *dirseg)
{
	if (dirseg && !fnx_dirseg_isempty(dirseg)) {
		if (!fnx_dir_hasseg(dir, dirseg->ds_index)) {
			fnx_dir_setseg(dir, dirseg->ds_index);
		}
		dirseg->ds_vnode.v_expired = FNX_FALSE;
		put_vnode(vproc, &dirseg->ds_vnode);
		put_vnode(vproc, dir_vnode(dir));
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int
vproc_fetch_iref(fnx_vproc_t *vproc, fnx_inode_t *inode, fnx_inode_t **iref)
{
	int rc = 0;

	if (fnx_inode_isreflnk(inode)) {
		fnx_assert(inode->i_refino != FNX_INO_NULL);
		rc = fnx_vproc_fetch_inode(vproc, inode->i_refino, iref);
	}
	return rc;
}

int fnx_vproc_prep_link(fnx_vproc_t      *vproc,
                        fnx_dir_t        *parentd,
                        const fnx_name_t *name)
{
	int rc;
	fnx_hash_t nhash;
	fnx_dirent_t *dirent = NULL;
	fnx_dirseg_t *dirseg = NULL;

	rc = vproc_require_mutabled(vproc, parentd);
	if (rc != 0) {
		return rc;
	}
	nhash = dir_getnhash(parentd, name);
	dirent = fnx_dir_predict(parentd, nhash);
	if (dirent != NULL) {
		return 0; /* It is possible to link at dir-top */
	}
	rc = vproc_yield_dirseg(vproc, parentd, nhash, &dirseg);
	if (rc != 0) {
		return rc;
	}
	dirent = fnx_dirseg_predict(dirseg, nhash);
	if (dirent == NULL) {
		return -ENOSPC; /* Unable to add child */
	}
	rc = vproc_require_mutable(vproc, &dirseg->ds_vnode);
	if (rc != 0) {
		return rc;
	}
	return 0;
}

static void vproc_update_linked(fnx_vproc_t *vproc,
                                fnx_dir_t   *parentd,
                                fnx_inode_t *child,
                                fnx_inode_t *iref)
{
	fnx_dir_t *childd;

	/* Post-op: fix namespace linkage */
	if (fnx_inode_isdir(child)) {
		childd = fnx_inode_to_dir(child);
		dir_update_linkedd(parentd, childd);
		fnx_inode_setitime(dir_inode(parentd), FNX_AMCTIME_NOW);
		fnx_inode_setitime(dir_inode(childd), FNX_AMCTIME_NOW);
	} else if (fnx_inode_isreflnk(child)) {
		fnx_assert(iref != NULL);
		dir_update_linked(parentd, child, iref);
		fnx_inode_setitime(dir_inode(parentd), FNX_MCTIME_NOW);
		fnx_inode_setitime(child, FNX_MCTIME_NOW);
		fnx_inode_setitime(iref, FNX_MCTIME_NOW);
	} else {
		fnx_assert(iref == NULL);
		dir_update_linked(parentd, child, NULL);
		fnx_inode_setitime(dir_inode(parentd), FNX_MCTIME_NOW);
		fnx_inode_setitime(child, FNX_AMCTIME_NOW);
	}
	fnx_unused(vproc);
}

static void vproc_put_xxlinkxx(fnx_vproc_t  *vproc,
                               fnx_dir_t    *dir,
                               fnx_inode_t  *child,
                               fnx_inode_t  *iref,
                               fnx_dirseg_t *dirseg,
                               fnx_dirseg_t *newdirseg)
{
	if (newdirseg != NULL) {
		put_vnode(vproc, &newdirseg->ds_vnode);
	}
	if (dirseg != NULL) {
		put_vnode(vproc, &dirseg->ds_vnode);
	}
	if (iref != NULL) {
		put_vnode(vproc, &iref->i_vnode);
	}
	put_vnode(vproc, &child->i_vnode);
	put_vnode(vproc, dir_vnode(dir));
}

int fnx_vproc_link_child(fnx_vproc_t      *vproc,
                         fnx_dir_t        *parentd,
                         fnx_inode_t      *inode,
                         const fnx_name_t *name)
{
	int rc;
	fnx_hash_t nhash;
	fnx_inode_t  *iref   = NULL;
	fnx_dirent_t *dirent = NULL;
	fnx_dirseg_t *dirseg = NULL;

	nhash = dir_getnhash(parentd, name);
	rc = vproc_fetch_iref(vproc, inode, &iref);
	if (rc != 0) {
		return rc;
	}
	dirent = fnx_dir_predict(parentd, nhash);
	if (dirent != NULL) {
		vproc_associate_link(vproc, parentd, name, inode);
		fnx_dir_link(parentd, inode);
		goto out_ok; /* Linked at dir's head */
	}
	rc = vproc_yield_dirseg(vproc, parentd, nhash, &dirseg);
	if (rc != 0) {
		return rc;
	}
	dirent = fnx_dirseg_predict(dirseg, nhash);
	if (dirent != NULL) {
		vproc_associate_link(vproc, parentd, name, inode);
		fnx_dirseg_link(dirseg, inode);
		goto out_ok; /* Linked at dir segment */
	}
	/* Unable to add child */
	return -ENOSPC;

out_ok:
	/* Post-link settings */
	vproc_update_linked(vproc, parentd, inode, iref);
	vproc_settle_linked(vproc, parentd, dirseg);
	vproc_put_xxlinkxx(vproc, parentd, inode, iref, dirseg, NULL);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_vproc_prep_unlink(fnx_vproc_t *vproc,
                          fnx_dir_t   *parentd,
                          fnx_inode_t *inode)
{
	int rc;
	fnx_dirent_t *dirent = NULL;
	fnx_dirseg_t *dirseg = NULL;

	rc = vproc_require_mutabled(vproc, parentd);
	if (rc != 0) {
		return rc;
	}
	dirent = fnx_dir_ilookup(parentd, inode);
	if (dirent != NULL) {
		return 0;
	}
	rc = vproc_fetch_idirseg(vproc, parentd, inode, &dirseg);
	if (rc != 0) {
		return rc;
	}
	dirent = fnx_dirseg_ilookup(dirseg, inode);
	if (dirent == NULL) {
		fnx_assert(0); /* XXX: Internal error: broken namespace! */
		return -EIO;
	}
	rc = vproc_require_mutable(vproc, &dirseg->ds_vnode);
	if (rc != 0) {
		return rc;
	}
	return 0;
}

static int vproc_unlink_reg_data(fnx_vproc_t *vproc, fnx_reg_t *reg)
{
	int rc = 0;
	fnx_size_t  refcnt;
	fnx_nlink_t nlink_curr, nlink_last;

	refcnt = fnx_inode_getrefcnt(&reg->r_inode);
	if (refcnt > 0) {
		return 0;
	}
	nlink_curr = fnx_inode_getnlink(&reg->r_inode);
	nlink_last = FNX_INODE_INIT_NLINK + 1;
	if (nlink_curr != nlink_last) {
		return 0;
	}
	rc = fnx_vproc_trunc_data(vproc, reg, 0);
	return rc;
}

static int vproc_trunc_upon_unlink(fnx_vproc_t *vproc, fnx_inode_t *inode)
{
	int rc;
	fnx_reg_t   *reg  = NULL;
	fnx_inode_t *iref = NULL;

	/* Possibly, implict data-truncate of reg-file upon unlink */
	rc = vproc_fetch_iref(vproc, inode, &iref);
	if (rc != 0) {
		return rc;
	}
	if (iref && fnx_inode_isreg(iref)) {
		reg = fnx_inode_to_reg(iref);
		rc = vproc_unlink_reg_data(vproc, reg);
		if (rc != 0) {
			return rc;
		}
	} else if (fnx_inode_isreg(inode)) {
		reg = fnx_inode_to_reg(inode);
		rc = vproc_unlink_reg_data(vproc, reg);
		if (rc != 0) {
			return rc;
		}
	}
	return 0;
}

int fnx_vproc_prep_xunlink(fnx_vproc_t *vproc,
                           fnx_dir_t   *parentd,
                           fnx_inode_t *inode)
{
	int rc;

	/* Possibly, implicit data-truncation of reg-file upon unlink */
	rc = vproc_trunc_upon_unlink(vproc, inode);
	if (rc != 0) {
		return rc;
	}
	/* Now, unlink like any other */
	rc = fnx_vproc_prep_unlink(vproc, parentd, inode);
	if (rc != 0) {
		return rc;
	}
	return 0;
}

static void vproc_update_unlinked(fnx_vproc_t *vproc,
                                  fnx_dir_t   *parentd,
                                  fnx_inode_t *child,
                                  fnx_inode_t *iref)
{
	fnx_dir_t *childd;

	/* Post-op: fix dir linkage accounting ant time-stamps */
	if (fnx_inode_isdir(child)) {
		childd = fnx_inode_to_dir(child);
		dir_update_unlinkedd(parentd, childd);
		fnx_inode_setitime(dir_inode(parentd), FNX_MCTIME_NOW);
		fnx_inode_setitime(dir_inode(childd), FNX_CTIME_NOW);
	} else if (fnx_inode_isreflnk(child)) {
		fnx_assert(iref != NULL);
		dir_update_unlinked(parentd, child, iref);
		fnx_inode_setitime(dir_inode(parentd), FNX_MCTIME_NOW);
		fnx_inode_setitime(iref, FNX_CTIME_NOW);
	} else {
		fnx_assert(iref == NULL);
		dir_update_unlinked(parentd, child, NULL);
		fnx_inode_setitime(dir_inode(parentd), FNX_MCTIME_NOW);
		fnx_inode_setitime(child, FNX_CTIME_NOW);
	}
	fnx_unused(vproc);
}

int fnx_vproc_unlink_child(fnx_vproc_t *vproc,
                           fnx_dir_t   *parentd,
                           fnx_inode_t *inode)
{
	int rc;
	fnx_inode_t *iref = NULL;
	fnx_dirent_t *dirent = NULL;
	fnx_dirseg_t *dirseg = NULL;

	rc = vproc_fetch_iref(vproc, inode, &iref);
	if (rc != 0) {
		return rc;
	}
	dirent = fnx_dir_ilookup(parentd, inode);
	if (dirent != NULL) {
		dirent = fnx_dir_unlink(parentd, inode);
		fnx_assert(dirent != NULL); /* XXX */
		goto out_ok; /* Unlinked OK from dir's head */
	}
	rc = vproc_fetch_idirseg(vproc, parentd, inode, &dirseg);
	if (rc != 0) {
		return rc;
	}
	dirent = fnx_dirseg_ilookup(dirseg, inode);
	if (dirent != NULL) {
		fnx_dirseg_unlink(dirseg, inode);
		goto out_ok; /* Unlinked OK from dir's segment */
	}
	/* Could-not unlink: internal error! */
	fnx_assert(0);
	return -EIO;

out_ok:
	vproc_dissociate_link(vproc, parentd, inode);
	vproc_update_unlinked(vproc, parentd, inode, iref);
	vproc_settle_unlinked(vproc, parentd, dirseg);
	vproc_put_xxlinkxx(vproc, parentd, inode, iref, dirseg, NULL);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_vproc_prep_rename(fnx_vproc_t       *vproc,
                          fnx_dir_t         *parentd,
                          const fnx_inode_t *inode,
                          const fnx_name_t  *newname)
{
	int rc;
	fnx_hash_t newnhash;
	fnx_dirent_t *dirent = NULL;
	fnx_dirseg_t *dirseg = NULL;
	fnx_dirseg_t *newdirseg = NULL;

	rc = vproc_require_mutabled(vproc, parentd);
	if (rc != 0) {
		return rc;
	}
	dirent = fnx_dir_ilookup(parentd, inode);
	if (dirent != NULL) {
		return 0;
	}
	rc = vproc_fetch_idirseg(vproc, parentd, inode, &dirseg);
	if (rc != 0) {
		return rc;
	}
	dirent = fnx_dirseg_ilookup(dirseg, inode);
	if (dirent == NULL) {
		fnx_assert(0);
		return -EIO; /* XXX  internal error! */
	}
	newnhash = dir_getnhash(parentd, newname);
	rc = vproc_yield_dirseg(vproc, parentd, newnhash, &newdirseg);
	if (rc != 0) {
		return rc;
	}
	dirent = fnx_dirseg_predict(newdirseg, newnhash);
	if (dirent == NULL) {
		return -ENOSPC;
	}
	rc = vproc_require_mutable(vproc, &dirseg->ds_vnode);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_require_mutable(vproc, &newdirseg->ds_vnode);
	if (rc != 0) {
		return rc;
	}
	return 0;
}

int fnx_vproc_rename_inplace(fnx_vproc_t      *vproc,
                             fnx_dir_t        *parentd,
                             fnx_inode_t      *inode,
                             const fnx_name_t *newname)
{
	int rc;
	fnx_hash_t newnhash;
	fnx_inode_t  *iref   = NULL;
	fnx_dirent_t *dirent = NULL;
	fnx_dirseg_t *dirseg = NULL;
	fnx_dirseg_t *newdirseg = NULL;

	rc = vproc_fetch_iref(vproc, inode, &iref);
	if (rc != 0) {
		return rc;
	}
	dirent = fnx_dir_ilookup(parentd, inode);
	if (dirent != NULL) {
		dirent = fnx_dir_unlink(parentd, inode);
		fnx_assert(dirent != NULL); /* XXX */
		vproc_dissociate_link(vproc, parentd, inode);
		vproc_associate_link(vproc, parentd, newname, inode);
		fnx_dir_link(parentd, inode);
		goto out_ok;
	}
	rc = vproc_fetch_idirseg(vproc, parentd, inode, &dirseg);
	if (rc != 0) {
		return rc;
	}
	newnhash = dir_getnhash(parentd, newname);
	rc = vproc_yield_dirseg(vproc, parentd, newnhash, &newdirseg);
	if (rc != 0) {
		return rc;
	}
	dirent = fnx_dirseg_predict(newdirseg, newnhash);
	if (dirent != NULL) {
		fnx_dirseg_unlink(dirseg, inode);
		vproc_dissociate_link(vproc, parentd, inode);
		vproc_associate_link(vproc, parentd, newname, inode);
		fnx_dirseg_link(newdirseg, inode);
		goto out_ok;
	}
	return -ENOSPC;

out_ok:
	/* Post rename settings */
	vproc_update_unlinked(vproc, parentd, inode, iref);
	vproc_update_linked(vproc, parentd, inode, iref);
	vproc_settle_unlinked(vproc, parentd, dirseg);
	vproc_settle_linked(vproc, parentd, newdirseg);
	vproc_put_xxlinkxx(vproc, parentd, inode, iref, dirseg, newdirseg);
	return 0;
}

int fnx_vproc_rename_replace(fnx_vproc_t *vproc,
                             fnx_dir_t   *parentd,
                             fnx_inode_t *inode,
                             fnx_inode_t *curinode)
{
	int rc;
	fnx_name_t newname;
	fnx_inode_t  *iref      = NULL;
	fnx_inode_t  *curiref   = NULL;
	fnx_dirseg_t *dirseg    = NULL;
	fnx_dirseg_t *curdirseg = NULL;
	fnx_dirent_t *dirent    = NULL;
	fnx_dirent_t *curdirent = NULL;

	/* Using destinations inode's name, excluding its hash */
	fnx_name_copy(&newname, &curinode->i_name);
	newname.hash = 0;

	rc = vproc_fetch_iref(vproc, inode, &iref);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_fetch_iref(vproc, curinode, &curiref);
	if (rc != 0) {
		return rc;
	}
	dirent  = fnx_dir_ilookup(parentd, inode);
	curdirent = fnx_dir_ilookup(parentd, curinode);
	if ((dirent != NULL) && (curdirent != NULL)) {
		/* Both linked at top -- exchange */
		fnx_dir_unlink(parentd, inode);
		fnx_dir_unlink(parentd, curinode);
		vproc_dissociate_link(vproc, parentd, inode);
		vproc_dissociate_link(vproc, parentd, curinode);
		vproc_associate_link(vproc, parentd, &newname, inode);
		fnx_dir_link(parentd, inode);
	} else if ((dirent == NULL) && (curdirent != NULL)) {
		/* Unlink child from sub and overwrite at-top */
		rc = vproc_fetch_idirseg(vproc, parentd, inode, &dirseg);
		if (rc != 0) {
			return rc;
		}
		fnx_dirseg_unlink(dirseg, inode);
		fnx_dir_unlink(parentd, curinode);
		vproc_dissociate_link(vproc, parentd, inode);
		vproc_dissociate_link(vproc, parentd, curinode);
		vproc_associate_link(vproc, parentd, &newname, inode);
		fnx_dir_link(parentd, inode);
	} else if ((dirent != NULL) && (curdirent == NULL)) {
		/* Unlink current from sub and re-rename at-top */
		rc = vproc_fetch_idirseg(vproc, parentd, curinode, &curdirseg);
		if (rc != 0) {
			return rc;
		}
		fnx_dirseg_unlink(curdirseg, curinode);
		fnx_dir_unlink(parentd, inode);
		vproc_dissociate_link(vproc, parentd, inode);
		vproc_dissociate_link(vproc, parentd, curinode);
		vproc_associate_link(vproc, parentd, &newname, inode);
		fnx_dir_link(parentd, inode);
	} else {
		/* Unlink child from sub and overwrite at-sub */
		rc = vproc_fetch_idirseg(vproc, parentd, inode, &dirseg);
		if (rc != 0) {
			return rc;
		}
		rc = vproc_fetch_idirseg(vproc, parentd, curinode, &curdirseg);
		if (rc != 0) {
			return rc;
		}
		fnx_dirseg_unlink(dirseg, inode);
		fnx_dirseg_unlink(curdirseg, curinode);
		vproc_dissociate_link(vproc, parentd, inode);
		vproc_dissociate_link(vproc, parentd, curinode);
		vproc_associate_link(vproc, parentd, &newname, inode);
		fnx_dirseg_link(curdirseg, inode);
	}

	/* Finally.. */
	vproc_update_unlinked(vproc, parentd, curinode, curiref);
	vproc_update_unlinked(vproc, parentd, inode, iref);
	vproc_update_linked(vproc, parentd, inode, iref);
	vproc_settle_unlinked(vproc, parentd, dirseg);
	vproc_settle_unlinked(vproc, parentd, curdirseg);
	vproc_settle_linked(vproc, parentd, curdirseg);
	vproc_put_xxlinkxx(vproc, parentd, inode, iref, dirseg, curdirseg);
	vproc_put_xxlinkxx(vproc, parentd, curinode, curiref, curdirseg, NULL);
	return 0;
}

int fnx_vproc_rename_override(fnx_vproc_t *vproc,
                              fnx_dir_t   *parentd,
                              fnx_inode_t *inode,
                              fnx_dir_t   *newparentd,
                              fnx_inode_t *curinode)
{
	int rc;
	fnx_name_t newname;
	fnx_inode_t  *iref      = NULL;
	fnx_inode_t  *curiref   = NULL;
	fnx_dirseg_t *dirseg    = NULL;
	fnx_dirseg_t *curdirseg = NULL;
	fnx_dirent_t *dirent    = NULL;
	fnx_dirent_t *curdirent = NULL;

	/* Using destinations inode's name, excluding its hash */
	fnx_name_copy(&newname, &curinode->i_name);
	newname.hash = 0;

	rc = vproc_fetch_iref(vproc, inode, &iref);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_fetch_iref(vproc, curinode, &curiref);
	if (rc != 0) {
		return rc;
	}
	dirent  = fnx_dir_ilookup(parentd, inode);
	curdirent = fnx_dir_ilookup(newparentd, curinode);
	if ((dirent != NULL) && (curdirent != NULL)) {
		/* Both linked at top -- exchange */
		fnx_dir_unlink(parentd, inode);
		fnx_dir_unlink(newparentd, curinode);
		vproc_dissociate_link(vproc, parentd, inode);
		vproc_dissociate_link(vproc, newparentd, curinode);
		vproc_associate_link(vproc, newparentd, &newname, inode);
		fnx_dir_link(newparentd, inode);
	} else if ((dirent == NULL) && (curdirent != NULL)) {
		/* Unlink child from sub and overwrite at-top */
		rc = vproc_fetch_idirseg(vproc, parentd, inode, &dirseg);
		if (rc != 0) {
			return rc;
		}
		fnx_dirseg_unlink(dirseg, inode);
		fnx_dir_unlink(newparentd, curinode);
		vproc_dissociate_link(vproc, parentd, inode);
		vproc_dissociate_link(vproc, newparentd, curinode);
		vproc_associate_link(vproc, newparentd, &newname, inode);
		fnx_dir_link(newparentd, inode);
	} else if ((dirent != NULL) && (curdirent == NULL)) {
		/* Unlink current from sub and re-rename at-sub */
		rc = vproc_fetch_idirseg(vproc, parentd, curinode, &curdirseg);
		if (rc != 0) {
			return rc;
		}
		fnx_dirseg_unlink(curdirseg, curinode);
		fnx_dir_unlink(parentd, inode);
		vproc_dissociate_link(vproc, parentd, inode);
		vproc_dissociate_link(vproc, newparentd, curinode);
		vproc_associate_link(vproc, newparentd, &newname, inode);
		fnx_dirseg_link(curdirseg, inode);
	} else {
		/* Unlink child from sub and overwrite at-sub */
		rc = vproc_fetch_idirseg(vproc, parentd, inode, &dirseg);
		if (rc != 0) {
			return rc;
		}
		rc = vproc_fetch_idirseg(vproc, newparentd, curinode, &curdirseg);
		if (rc != 0) {
			return rc;
		}
		fnx_dirseg_unlink(dirseg, inode);
		fnx_dirseg_unlink(curdirseg, curinode);
		vproc_dissociate_link(vproc, parentd, inode);
		vproc_dissociate_link(vproc, newparentd, curinode);
		vproc_associate_link(vproc, newparentd, &newname, inode);
		fnx_dirseg_link(curdirseg, inode);
	}

	/* Finally.. */
	vproc_update_unlinked(vproc, newparentd, curinode, curiref);
	vproc_update_unlinked(vproc, parentd, inode, iref);
	vproc_update_linked(vproc, newparentd, inode, iref);
	vproc_settle_unlinked(vproc, parentd, dirseg);
	vproc_settle_unlinked(vproc, newparentd, curdirseg);
	vproc_settle_linked(vproc, newparentd, curdirseg);
	vproc_put_xxlinkxx(vproc, parentd, inode, iref, dirseg, curdirseg);
	vproc_put_xxlinkxx(vproc, newparentd, curinode, curiref, curdirseg, NULL);
	return 0;
}



/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void vproc_finalize_inode(fnx_vproc_t *vproc, fnx_inode_t *inode)
{
	fnx_vnode_t *vnode = &inode->i_vnode;

	vnode->v_expired = FNX_TRUE;
	fnx_vproc_forget_vnode(vproc, vnode);
	put_vnode(vproc, &inode->i_vnode);
}

static int vproc_settle_unlinked_dir(fnx_vproc_t *vproc, fnx_dir_t *dir)
{
	fnx_nlink_t nlink;
	fnx_vnode_t *vnode;

	vnode = &dir->d_inode.i_vnode;
	nlink = dir_getnlink(dir);
	if (nlink > FNX_DIR_INIT_NLINK) {
		return 0;
	}
	vnode->v_expired = FNX_TRUE;
	if (dir->d_inode.i_vnode.v_refcnt > 0) {
		return 0;
	}
	fnx_assert(dir->d_nchilds == 0);
	vproc_finalize_inode(vproc, dir_inode(dir));
	return 0;
}

static int vproc_settle_unlinked_inode(fnx_vproc_t *vproc, fnx_inode_t *inode)
{
	int rc, isreg;
	fnx_nlink_t nlink;
	fnx_reg_t *reg;

	fnx_assert(!inode->i_vnode.v_pseudo);

	isreg = fnx_inode_isreg(inode);
	nlink = fnx_inode_getnlink(inode);
	if (nlink > FNX_INODE_INIT_NLINK) {
		return 0;
	}
	vproc_finalize_inode(vproc, inode);
	if (inode->i_vnode.v_refcnt > 0) {
		return 0;
	}
	if (isreg) { /* XXX needed? */
		reg = fnx_inode_to_reg(inode);
		rc = fnx_vproc_trunc_data(vproc, reg, 0);
		if (rc != 0) {
			return rc;
		}
	}
	return 0;
}

int fnx_vproc_fix_unlinked(fnx_vproc_t *vproc, fnx_inode_t *inode)
{
	int rc;
	fnx_dir_t *dir;
	fnx_inode_t *iref = NULL;

	if (inode == NULL) {
		return 0; /* OK case for rename */
	}
	rc = vproc_fetch_iref(vproc, inode, &iref);
	if (rc != 0) {
		return rc;
	}
	if (fnx_inode_isdir(inode)) {
		dir = fnx_inode_to_dir(inode);
		rc  = vproc_settle_unlinked_dir(vproc, dir);
	} else if (fnx_inode_isreflnk(inode)) {
		fnx_assert(iref != NULL);
		rc = vproc_settle_unlinked_inode(vproc, iref);
		if (rc == 0) {
			rc = vproc_settle_unlinked_inode(vproc, inode);
		}
	} else {
		fnx_assert(iref == NULL);
		rc = vproc_settle_unlinked_inode(vproc, inode);
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int vproc_acquire_dirseg(fnx_vproc_t     *vproc,
                                const fnx_dir_t *dir,
                                const fnx_off_t  dseg,
                                fnx_dirseg_t   **dirseg)
{
	int rc;
	fnx_ino_t ino;
	fnx_xno_t xno;
	fnx_vnode_t *vnode = NULL;
	const fnx_vtype_e vtype = FNX_VTYPE_DIRSEG;

	ino = fnx_dir_getino(dir);
	xno = (fnx_xno_t)dseg;
	rc  = fnx_vproc_acquire_vnode(vproc, vtype, ino, xno, NULL, &vnode);
	if (rc == 0) {
		*dirseg = fnx_vnode_to_dirseg(vnode);
		fnx_dirseg_setup(*dirseg, dseg);
	}
	return rc;
}

static int vproc_fetch_dirseg(fnx_vproc_t        *vproc,
                              const fnx_dir_t    *dir,
                              const fnx_off_t     dseg,
                              fnx_dirseg_t      **dirseg)
{
	int rc = -ENOENT;
	fnx_vaddr_t vaddr;
	fnx_vnode_t *vnode;

	if (!fnx_dir_hasseg(dir, dseg)) {
		return -ENOENT;
	}
	fnx_vaddr_for_dirseg(&vaddr, fnx_dir_getino(dir), dseg);
	rc = fnx_vproc_fetch_vnode(vproc, &vaddr, &vnode);
	fnx_assert(rc != -ENOENT);
	if (rc != 0) {
		return rc;
	}
	*dirseg = fnx_vnode_to_dirseg(vnode);
	return 0;
}

static int vproc_fetch_hdirseg(fnx_vproc_t        *vproc,
                               const fnx_dir_t    *dir,
                               const fnx_hash_t    nhash,
                               fnx_dirseg_t      **dirseg)
{
	const fnx_off_t dseg = fnx_hash_to_dseg(nhash);
	return vproc_fetch_dirseg(vproc, dir, dseg, dirseg);
}

static int vproc_fetch_idirseg(fnx_vproc_t        *vproc,
                               const fnx_dir_t    *dir,
                               const fnx_inode_t  *inode,
                               fnx_dirseg_t      **dirseg)
{
	const fnx_hash_t nhash = inode->i_name.hash;
	fnx_assert(nhash != 0);
	return vproc_fetch_hdirseg(vproc, dir, nhash, dirseg);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/


static int inode_is_size(const fnx_inode_t *inode, fnx_off_t size)
{
	return (fnx_inode_getsize(inode) == size);
}

static int inode_is_clearsgid_needed(const fnx_inode_t *inode,
                                     const fnx_uctx_t  *uctx)
{
	const int cap_fsetid = fnx_uctx_iscap(uctx, FNX_CAPF_FSETID);
	return (!fnx_isgroupmember(uctx, inode->i_iattr.i_gid) && !cap_fsetid);
}

static int inode_is_clearsuid_needed(const fnx_inode_t *inode,
                                     const fnx_uctx_t  *uctx)
{
	const int cap_chown = fnx_uctx_iscap(uctx, FNX_CAPF_CHOWN);
	return (fnx_inode_isreg(inode) && fnx_inode_isexec(inode) && !cap_chown);
}

static void inode_refresh_sgid(fnx_inode_t *inode, const fnx_uctx_t *uctx)
{
	if (inode_is_clearsgid_needed(inode, uctx)) {
		fnx_inode_clearsgid(inode);
	}
}

static void inode_refresh_suid(fnx_inode_t *inode, const fnx_uctx_t *uctx)
{
	if (inode_is_clearsuid_needed(inode, uctx)) {
		fnx_inode_clearsuid(inode);
	}
}

static void inode_setattr_chmod(fnx_inode_t *inode,
                                const fnx_uctx_t *uctx, fnx_mode_t mode)
{
	fnx_iattr_t *iattr = &inode->i_iattr;

	iattr->i_mode = mode;
	inode_refresh_sgid(inode, uctx);
}

static void inode_setattr_chown_uid(fnx_inode_t *inode,
                                    const fnx_uctx_t *uctx, fnx_uid_t uid)
{
	fnx_iattr_t *iattr = &inode->i_iattr;

	iattr->i_uid = uid;
	inode_refresh_suid(inode, uctx);
}

static void inode_setattr_chown_gid(fnx_inode_t *inode,
                                    const fnx_uctx_t *uctx, fnx_gid_t gid)
{
	fnx_iattr_t *iattr = &inode->i_iattr;

	iattr->i_gid = gid;
	inode_refresh_sgid(inode, uctx);
}

static void inode_setattr_utimes(fnx_inode_t *inode, fnx_flags_t flags,
                                 const fnx_times_t *times)
{
	fnx_inode_setitimes(inode, flags, times);
}

static void inode_setattr_setsize(fnx_inode_t *inode,
                                  const fnx_uctx_t *uctx, fnx_off_t size)
{
	fnx_inode_setsize(inode, size);
	inode_refresh_suid(inode, uctx);
	inode_refresh_sgid(inode, uctx);
}

int fnx_vproc_setiattr(fnx_vproc_t *vproc, fnx_inode_t *inode,
                       const fnx_uctx_t *uctx, fnx_flags_t flags,
                       fnx_mode_t mode, fnx_uid_t uid, fnx_gid_t gid,
                       fnx_off_t size, const fnx_times_t *itms)
{
	fnx_flags_t tmflags = 0;

	if (flags & FNX_SETATTR_MODE) {
		inode_setattr_chmod(inode, uctx, mode);
		tmflags |= FNX_CTIME_NOW;
	}
	if ((flags & FNX_SETATTR_GID) && !fnx_gid_isnull(gid)) {
		inode_setattr_chown_gid(inode, uctx, gid);
		tmflags |= FNX_CTIME_NOW;
	}
	if ((flags & FNX_SETATTR_UID) && !fnx_uid_isnull(uid)) {
		inode_setattr_chown_uid(inode, uctx, uid);
		tmflags |= FNX_CTIME_NOW;
	}
	if ((flags & FNX_SETATTR_SIZE) && !inode_is_size(inode, size)) {
		inode_setattr_setsize(inode, uctx, size);
		tmflags |= FNX_ACTIME_NOW;
	}
	if (flags & FNX_SETATTR_AMTIME_ANY) {
		inode_setattr_utimes(inode, flags, itms);
	}

	fnx_inode_setitime(inode, tmflags);
	put_vnode(vproc, &inode->i_vnode);
	return 0;
}

void fnx_vproc_setiattr_size(fnx_vproc_t *vproc, fnx_inode_t *inode,
                             const fnx_uctx_t *uctx, fnx_off_t size)
{
	const fnx_flags_t flags = FNX_SETATTR_SIZE;
	fnx_vproc_setiattr(vproc, inode, uctx, flags, 0, 0, 0, size, NULL);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_vproc_read_symlnk(fnx_vproc_t *vproc,
                          fnx_symlnk_t *symlnk, fnx_path_t *path)
{
	fnx_path_copy(path, &symlnk->sl_value);

	/*
	 * NB: Not sure if symlink's atime should be updated upon readlink. For
	 * now, update only in cache.
	 *
	 * Important: If changing semantics (i.e., uncomment 'put_vone') then must
	 * also ensure 'symlnk' is mutable.
	 */
	fnx_inode_setitime(&symlnk->sl_inode, FNX_ATIME_NOW);
	/*put_vnode(vproc, &symlnk->sl_inode.i_vnode);*/
	fnx_unused(vproc);
	return 0;
}
