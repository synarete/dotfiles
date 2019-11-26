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

#include <fnxinfra.h>
#include <fnxcore.h>
#include <fnxpstor.h>
#include "vproc-elems.h"
#include "vproc-exec.h"
#include "vproc-data.h"
#include "vproc-private.h"

/* Local functions */
static void vproc_evict_delete_vnode(fnx_vproc_t *, fnx_vnode_t *);




/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_vproc_close(fnx_vproc_t *vproc)
{
	int rc = 0;
	const fnx_fsattr_t *fsattr;
	fnx_dir_t   *rootd = vproc->rootd;
	fnx_super_t *super = vproc->super;

	if (super == NULL) {
		goto out; /* Bootstrap case ? */
	}
	fsattr = &super->su_fsinfo.fs_attr;
	fnx_info("closing-vproc: %s", fsattr->f_uuid.str);

	rc = fnx_pstor_commit_vnode(vproc->pstor, &rootd->d_inode.i_vnode);
	if (rc != 0) {
		fnx_error("no-commit-rootd: %s", fsattr->f_uuid.str);
		goto out;
	}
	rc = fnx_pstor_commit_vnode(vproc->pstor, &super->su_vnode);
	if (rc != 0) {
		fnx_error("no-commit-super: %s", fsattr->f_uuid.str);
		goto out;
	}
	rc = fnx_pstor_sync(vproc->pstor);
	if (rc != 0) {
		fnx_error("no-sync-pstor: %s", vproc->pstor->bdev.path);
		goto out;
	}
	vproc_evict_delete_vnode(vproc, &rootd->d_inode.i_vnode);
	vproc_evict_delete_vnode(vproc, &super->su_vnode);
	vproc->rootd = NULL;
	vproc->super = NULL;

out:
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_size_t vproc_numbckts(const fnx_vproc_t *vproc)
{
	return fnx_super_getnbckts(vproc->super);
}

static fnx_fsstat_t *vproc_fsstat(const fnx_vproc_t *vproc)
{
	return &vproc->super->su_fsinfo.fs_stat;
}

static void vproc_account_vtype(fnx_vproc_t *vproc, fnx_vtype_e vtype, int n)
{
	fnx_super_t  *super  = vproc->super;
	fnx_fsstat_t *fsstat = &super->su_fsinfo.fs_stat;

	fnx_fsstat_account(fsstat, vtype, n);
	fnx_super_settimes(super, NULL, FNX_MTIME_NOW);
	fnx_vproc_put_vnode(vproc, &super->su_vnode);
}

/*
 * Find-out the next sequence of vbk vaddrs which resolves into non-full
 * bucket. Fetch related blocks into cache.
 */
int fnx_vproc_predict_next_vba(fnx_vproc_t *vproc,
                               fnx_vaddr_t vbas[], size_t nvba)
{
	int rc;
	size_t cnt, idx;
	fnx_lba_t vlba;
	fnx_vaddr_t vba;
	fnx_fsstat_t *fsstat;

	fsstat = vproc_fsstat(vproc);
	rc = fnx_fsstat_next_vlba(fsstat, &vlba);
	if (rc != 0) {
		return -ENOSPC;
	}
	idx = 0;
	cnt = 1 + (vproc_numbckts(vproc) / 2);
	while ((cnt-- > 0) && (idx < nvba)) {
		fnx_vaddr_for_vbk(&vba, vlba);
		rc = fnx_pstor_require_vaddr(vproc->pstor, &vba);
		if (rc == 0) {
			fnx_vaddr_copy(&vbas[idx++], &vba);
		} else if (rc != -ENOSPC) {
			return rc;
		}
		vlba++;
	}
	if (idx < nvba) {
		return -ENOSPC;
	}
	return 0;
}

static int vproc_predict_next_via(fnx_vproc_t *vproc,
                                  fnx_vtype_e  vtype, fnx_vaddr_t *via)
{
	int cnt, rc = 0;
	fnx_ino_t ino, ino_base;
	fnx_fsstat_t *fsstat;

	fsstat = vproc_fsstat(vproc);
	rc = fnx_fsstat_next_ino(fsstat, &ino_base);
	if (rc != 0) {
		return -ENOSPC;
	}
	cnt = (int)(vproc_numbckts(vproc) / 4);
	do {
		ino = fnx_ino_create(ino_base, vtype);
		fnx_vaddr_for_inode(via, vtype, ino);
		rc = fnx_pstor_require_vaddr(vproc->pstor, via);
		if (rc != -ENOSPC) {
			break;
		}
		ino_base++;
	} while (--cnt > 0);

	return rc;
}

static int vproc_predict_specific_vaddr(fnx_vproc_t *vproc,
                                        fnx_vtype_e  vtype,
                                        fnx_ino_t    ino,
                                        fnx_xno_t    xno,
                                        fnx_vaddr_t *vaddr)
{
	int rc;

	fnx_vaddr_by_vtype(vaddr, vtype, ino, xno);
	rc = fnx_pstor_require_vaddr(vproc->pstor, vaddr);
	fnx_assert((rc == 0) || (rc == FNX_EPEND));
	return rc;
}

static int vproc_acquire_vba(fnx_vproc_t *vproc, fnx_vaddr_t *vba)
{
	int rc;
	fnx_lba_t vlba;
	fnx_fsstat_t *fsstat;

	fsstat = vproc_fsstat(vproc);
	rc = fnx_vproc_predict_next_vba(vproc, vba, 1);
	if (rc == 0) {
		vlba = fnx_vaddr_getvlba(vba);
		fnx_fsstat_stamp_vlba(fsstat, vlba);
	}
	return rc;
}

static int vproc_acquire_via(fnx_vproc_t *vproc,
                             fnx_vtype_e  vtype, fnx_vaddr_t *vaddr)
{
	int rc = 0;
	fnx_ino_t ino_base;
	fnx_fsstat_t *fsstat;

	fsstat = vproc_fsstat(vproc);
	rc = vproc_predict_next_via(vproc, vtype, vaddr);
	if (rc == 0) {
		ino_base = fnx_ino_getbase(vaddr->ino);
		fnx_fsstat_stamp_ino(fsstat, ino_base);
	}
	return rc;
}

int fnx_vproc_acquire_vaddr(fnx_vproc_t *vproc,
                            fnx_vtype_e  vtype,
                            fnx_ino_t    ino,
                            fnx_xno_t    xno,
                            fnx_vaddr_t *vaddr)
{
	int rc;

	if (fnx_isvbk(vtype)) {
		rc = vproc_acquire_vba(vproc, vaddr);
	} else if (fnx_isitype(vtype)) {
		rc = vproc_acquire_via(vproc, vtype, vaddr);
	} else {
		rc = vproc_predict_specific_vaddr(vproc, vtype, ino, xno, vaddr);
	}
	fnx_assert((rc == 0) || (rc == FNX_EPEND));
	if (rc == 0) {
		vproc_account_vtype(vproc, vtype, 1);
	}
	return rc;
}

void fnx_vproc_forget_vaddr(fnx_vproc_t *vproc, const fnx_vaddr_t *vaddr)
{
	const fnx_vtype_e vtype = vaddr->vtype;

	fnx_assert(!fnx_vaddr_isnull(vaddr));
	vproc_account_vtype(vproc, vtype, -1);
}

void fnx_vproc_forget_vnode(fnx_vproc_t *vproc, fnx_vnode_t *vnode)
{
	if (!vnode->v_forgot) {
		fnx_vproc_forget_vaddr(vproc, &vnode->v_vaddr);
		vnode->v_forgot = FNX_TRUE;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int vproc_lookup_cached_vnode(fnx_vproc_t       *vproc,
                                     const fnx_vaddr_t *vaddr,
                                     fnx_vnode_t      **p_vnode)
{
	fnx_vnode_t *vnode;

	vnode = fnx_vcache_search_vnode(&vproc->cache->vc, vaddr);
	if (vnode == NULL) {
		return FNX_ECACHEMISS;
	}
	*p_vnode = vnode; /* Cache hit! */
	return 0;
}

int fnx_vproc_fetch_vnode(fnx_vproc_t       *vproc,
                          const fnx_vaddr_t *vaddr,
                          fnx_vnode_t      **vnode)
{
	int rc = 0;
	fnx_cache_t *cache = vproc->cache;

	rc = vproc_lookup_cached_vnode(vproc, vaddr, vnode);
	if (rc != FNX_ECACHEMISS) {
		return rc;
	}
	rc = fnx_pstor_stage_vnode(vproc->pstor, vaddr, vnode);
	if (rc != 0) {
		return rc;
	}
	fnx_vcache_store_vnode(&cache->vc, *vnode);
	return 0;
}

static int vproc_fetch_inode(fnx_vproc_t  *vproc,
                             fnx_ino_t     ino,
                             fnx_inode_t **p_inode)
{
	int rc;
	fnx_vaddr_t vaddr;
	fnx_vnode_t *vnode = NULL;
	fnx_inode_t *inode = NULL;

	fnx_vaddr_for_by_ino(&vaddr, ino);
	rc = fnx_vproc_fetch_vnode(vproc, &vaddr, &vnode);
	if (rc == 0) {
		inode = fnx_vnode_to_inode(vnode);
		fnx_inode_setsuper(inode, vproc->super);
	}
	*p_inode = inode;
	return rc;
}

int fnx_vproc_fetch_cached_inode(fnx_vproc_t  *vproc,
                                 fnx_ino_t     ino,
                                 fnx_inode_t **inode)
{
	int rc;
	fnx_vaddr_t vaddr;
	fnx_vnode_t *vnode = NULL;

	fnx_vaddr_for_by_ino(&vaddr, ino);
	rc = vproc_lookup_cached_vnode(vproc, &vaddr, &vnode);
	if (rc != FNX_ECACHEMISS) {
		*inode = fnx_vnode_to_inode(vnode);
	}
	return rc;
}

int fnx_vproc_fetch_inode(fnx_vproc_t *vproc,
                          fnx_ino_t ino, fnx_inode_t **p_inode)
{
	int rc;
	fnx_ino_t refino;
	fnx_inode_t *inode = NULL;
	fnx_inode_t *iref  = NULL;

	if (!fnx_ino_isvalid(ino)) {
		return -EINVAL; /* NB: May be called from _let function */
	}
	rc = vproc_fetch_inode(vproc, ino, &inode);
	if (rc != 0) {
		return rc;
	}
	if (fnx_inode_isreflnk(inode)) {
		/* Require ref-inode to be cached */
		refino  = inode->i_refino;
		if (!fnx_ino_isvalid(refino)) {
			fnx_assert(0);
			return -EINVAL; /* XXX Need other error: emetadata? */
		}
		rc = vproc_fetch_inode(vproc, refino, &iref);
		if (rc != 0) {
			return rc;
		}
	}

	fnx_assert(inode->i_magic == FNX_INODE_MAGIC);
	fnx_assert(inode->i_super != NULL);
	*p_inode = inode;
	return 0;
}

int fnx_vproc_fetch_dir(fnx_vproc_t *vproc,
                        fnx_ino_t ino, fnx_dir_t **dir)
{
	int rc;
	fnx_inode_t *inode = NULL;

	rc = fnx_vproc_fetch_inode(vproc, ino, &inode);
	if (rc != 0) {
		return rc;
	}
	if (!fnx_inode_isdir(inode)) {
		return -ENOTDIR;
	}
	*dir = fnx_inode_to_dir(inode);
	return 0;
}

int fnx_vproc_fetch_reg(fnx_vproc_t *vproc, fnx_ino_t ino, fnx_reg_t **reg)
{
	int rc;
	fnx_inode_t *inode = NULL;

	rc = fnx_vproc_fetch_inode(vproc, ino, &inode);
	if (rc != 0) {
		return rc;
	}
	if (fnx_inode_isdir(inode)) {
		return -EISDIR;
	}
	if (fnx_inode_isfifo(inode)) {
		return -ESPIPE;
	}
	if (!fnx_inode_isreg(inode)) {
		return -EINVAL;
	}
	*reg = fnx_inode_to_reg(inode);
	return 0;
}

int fnx_vproc_fetch_symlnk(fnx_vproc_t *vproc,
                           fnx_ino_t ino, fnx_symlnk_t **symlnk)
{
	int rc;
	fnx_inode_t *inode = NULL;

	rc = fnx_vproc_fetch_inode(vproc, ino, &inode);
	if (rc != 0) {
		return rc;
	}
	if (!fnx_inode_issymlnk(inode)) {
		return -EINVAL;
	}
	*symlnk = fnx_inode_to_symlnk(inode);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_vproc_acquire_vvnode(fnx_vproc_t       *vproc,
                             const fnx_vaddr_t *vaddr,
                             fnx_vnode_t      **p_vnode)
{
	int rc;
	fnx_vaddr_t va;
	fnx_vnode_t *vnode = NULL;
	fnx_cache_t *cache = vproc->cache;
	const fnx_vtype_e vtype = vaddr->vtype;

	rc = fnx_vproc_acquire_vaddr(vproc, vtype, vaddr->ino, vaddr->xno, &va);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_pstor_spawn_vnode(vproc->pstor, vaddr, NULL, &vnode);
	if (rc != 0) {
		fnx_vproc_forget_vaddr(vproc, vaddr);
		return rc;
	}
	fnx_vcache_store_vnode(&cache->vc, vnode);
	fnx_vproc_put_vnode(vproc, vnode);
	*p_vnode = vnode;
	return 0;
}

int fnx_vproc_acquire_vnode(fnx_vproc_t  *vproc,
                            fnx_vtype_e   vtype,
                            fnx_ino_t     ino,
                            fnx_xno_t     xno,
                            fnx_bkref_t  *bkref,
                            fnx_vnode_t **p_vnode)
{
	int rc;
	fnx_vaddr_t vaddr;
	fnx_vnode_t *vnode = NULL;
	fnx_cache_t *cache = vproc->cache;

	rc = fnx_vproc_acquire_vaddr(vproc, vtype, ino, xno, &vaddr);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_pstor_spawn_vnode(vproc->pstor, &vaddr, bkref, &vnode);
	if (rc != 0) {
		fnx_vproc_forget_vaddr(vproc, &vaddr);
		return rc;
	}
	fnx_vcache_store_vnode(&cache->vc, vnode);
	fnx_vproc_put_vnode(vproc, vnode);
	*p_vnode = vnode;
	return 0;
}

int fnx_vproc_acquire_inode(fnx_vproc_t  *vproc,
                            fnx_vtype_e   vtype,
                            fnx_inode_t **p_inode)
{
	int rc;
	fnx_vaddr_t vaddr;
	fnx_vnode_t *vnode = NULL;
	fnx_inode_t *inode = NULL;
	const fnx_ino_t ino = FNX_INO_ANY;
	const fnx_xno_t xno = FNX_XNO_NULL;

	rc = fnx_vproc_acquire_vaddr(vproc, vtype, ino, xno, &vaddr);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_pstor_spawn_vnode(vproc->pstor, &vaddr, NULL, &vnode);
	if (rc != 0) {
		fnx_vproc_forget_vaddr(vproc, &vaddr);
		return rc;
	}
	inode = fnx_vnode_to_inode(vnode);
	fnx_inode_setino(inode, vnode->v_vaddr.ino);
	fnx_inode_setsuper(inode, vproc->super);

	fnx_vcache_store_vnode(&vproc->cache->vc, vnode);
	fnx_vproc_put_vnode(vproc, vnode);
	*p_inode = inode;
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_vproc_acquire_dir(fnx_vproc_t      *vproc,
                          const fnx_uctx_t *uctx,
                          fnx_mode_t        mode,
                          fnx_dir_t       **dir)
{
	int rc;
	fnx_inode_t *inode = NULL;

	rc = fnx_vproc_acquire_inode(vproc, FNX_VTYPE_DIR, &inode);
	if (rc == 0) {
		*dir = fnx_inode_to_dir(inode);
		fnx_dir_setup(*dir, uctx, mode);
	}
	return rc;
}

int fnx_vproc_acquire_special(fnx_vproc_t      *vproc,
                              const fnx_uctx_t *uctx,
                              fnx_mode_t        mode,
                              fnx_inode_t     **inode)
{
	int rc;
	fnx_vtype_e vtype;

	vtype = fnx_mode_to_vtype(mode);
	rc = fnx_vproc_acquire_inode(vproc, vtype, inode);
	if (rc == 0) {
		fnx_inode_setup(*inode, uctx, mode, 0);
	}
	return rc;
}

int fnx_vproc_acquire_symlnk(fnx_vproc_t      *vproc,
                             const fnx_uctx_t *uctx,
                             const fnx_path_t *path,
                             fnx_symlnk_t    **symlnk)
{
	int rc;
	fnx_vtype_e vtype;
	fnx_inode_t *inode = NULL;

	vtype = fnx_vtype_by_symlnk_value(path);
	rc = fnx_vproc_acquire_inode(vproc, vtype, &inode);
	if (rc == 0) {
		*symlnk = fnx_inode_to_symlnk(inode);
		fnx_symlnk_setup(*symlnk, uctx, path);
	}
	return rc;
}

int fnx_vproc_acquire_reg(fnx_vproc_t      *vproc,
                          const fnx_uctx_t *uctx,
                          fnx_mode_t        mode,
                          fnx_reg_t       **reg)
{
	int rc;
	fnx_inode_t *inode = NULL;

	rc = fnx_vproc_acquire_inode(vproc, FNX_VTYPE_REG, &inode);
	if (rc == 0) {
		fnx_inode_setup(inode, uctx, mode, 0);
		*reg = fnx_inode_to_reg(inode);
	}
	return rc;
}

int fnx_vproc_acquire_reflnk(fnx_vproc_t      *vproc,
                             const fnx_uctx_t *uctx,
                             fnx_ino_t         refino,
                             fnx_inode_t     **inode)
{
	int rc;

	rc = fnx_vproc_acquire_inode(vproc, FNX_VTYPE_REFLNK, inode);
	if (rc == 0) {
		fnx_inode_setup(*inode, uctx, 0, 0);
		(*inode)->i_refino = refino;
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void vproc_evict_delete_vnode(fnx_vproc_t *vproc, fnx_vnode_t *vnode)
{
	fnx_vtype_e vtype;
	fnx_cache_t *cache = vproc->cache;

	if ((vnode != NULL) && (vnode->v_refcnt == 0)) {
		vtype = fnx_vnode_vtype(vnode);
		fnx_assert(vtype != FNX_VTYPE_NONE);
		if (vnode->v_cached) {
			fnx_vcache_evict_vnode(&cache->vc, vnode);
		}
		if (vtype == FNX_VTYPE_SPMAP) {
			fnx_pendq_unstage(&vproc->pstor->pendq, &vnode->v_jelem);
		} else {
			fnx_pendq_unstage(&vproc->pendq, &vnode->v_jelem);
		}
		fnx_pstor_retire_vnode(vproc->pstor, vnode);
	}
}

void fnx_vproc_retire_vnode(fnx_vproc_t *vproc, fnx_vnode_t *vnode)
{
	int rc;
	const fnx_vaddr_t *vaddr = &vnode->v_vaddr;

	fnx_assert(vaddr->vtype != FNX_VTYPE_SPMAP);
	fnx_assert(!fnx_vaddr_isnull(vaddr));
	fnx_unused(vaddr);

	if (vnode->v_placed) {
		rc = fnx_pstor_unmap_vnode(vproc->pstor, vnode);
		fnx_assert(rc == 0);
		fnx_unused(rc);
	}
	if (vnode->v_refcnt == 0) {
		fnx_vproc_forget_vnode(vproc, vnode);
		vproc_evict_delete_vnode(vproc, vnode);
	}
}

void fnx_vproc_retire_inode(fnx_vproc_t *vproc, fnx_inode_t *inode)
{
	fnx_vproc_retire_vnode(vproc, &inode->i_vnode);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_vproc_put_vnode(fnx_vproc_t *vproc, fnx_vnode_t *vnode)
{
	/* XXX: rm, FIXME */
	const fnx_vtype_e vtype = fnx_vnode_vtype(vnode);
	if ((vtype != FNX_VTYPE_SUPER) && !vnode->v_pseudo) {
		fnx_assert(fnx_vnode_ismutable(vnode));
		fnx_assert(vtype != FNX_VTYPE_SPMAP);
		fnx_pendq_stage(&vproc->pendq, &vnode->v_jelem);
	}
}

static int vproc_exec_vop(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc;
	fnx_vproc_fn vop;

	vop = fnx_get_vop(task->tsk_opcode);
	if (vop != NULL) {
		rc = vop(vproc, task);
	} else {
		rc = -ENOTSUP;
	}
	return rc;
}

static fnx_vnode_t *vproc_pop_modv(fnx_vproc_t *vproc)
{
	fnx_jelem_t *jelem;
	fnx_vnode_t *vnode;
	fnx_pendq_t *pendq;

	pendq = &vproc->pendq;
	jelem = fnx_pendq_sfront(pendq);
	if (jelem != NULL) {
		fnx_pendq_unstage(pendq, jelem);
		vnode = fnx_jelem_to_vnode(jelem);
		return vnode;
	}
	pendq = &vproc->pstor->pendq;
	jelem = fnx_pendq_sfront(pendq);
	if (jelem != NULL) {
		fnx_pendq_unstage(pendq, jelem);
		vnode = fnx_jelem_to_vnode(jelem);
		return vnode;
	}
	return NULL;
}

static int vproc_post_vop(fnx_vproc_t *vproc)
{
	int rc = 0, delay = 0;
	fnx_vnode_t *vnode = NULL;
	fnx_pstor_t *pstor = vproc->pstor;

	while ((vnode = vproc_pop_modv(vproc)) != NULL) {
		if (vnode->v_expired) {
			fnx_vproc_retire_vnode(vproc, vnode);
		} else {
			rc = fnx_pstor_commit_vnode(pstor, vnode);
			fnx_assert((rc == 0) || (rc == FNX_EDELAY));
			delay += (rc == FNX_EDELAY);
		}
	}
	return delay ? FNX_EDELAY : 0;
}

void fnx_vproc_prep_send_asio(fnx_vproc_t *vproc, fnx_bkref_t *bkref)
{
	bkref->bk_slaved = FNX_TRUE;
	bkref->bk_refcnt += 1;
	fnx_pendq_pend(&vproc->pendq, &bkref->bk_jelem);
}

static void vproc_slave_vop(fnx_vproc_t *vproc)
{
	fnx_bkref_t *bkref;
	fnx_pstor_t *pstor = vproc->pstor;

	while ((bkref = fnx_pstor_pop_sbk(pstor))) {
		fnx_vproc_prep_send_asio(vproc, bkref);
		vproc->dispatch(vproc->server, &bkref->bk_jelem);
	}
}

static void vproc_count_vop(const fnx_vproc_t *vproc,
                            const fnx_task_t *task, int oper_rc)
{
	fnx_super_t  *super  = vproc->super;
	fnx_opstat_t *opstat = &super->su_fsinfo.fs_oper;

	if (oper_rc == 0) {
		fnx_opstat_count(opstat, task->tsk_opcode);
	}
}

static int vproc_execute_vop(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int oper_rc, post_rc;

	/* Prepare */
	task->tsk_jelem.status = 0;
	task->tsk_runcnt++;

	/* Execute */
	oper_rc = vproc_exec_vop(vproc, task);

	/* Post-op */
	post_rc = vproc_post_vop(vproc);

	/* Async-slave bks */
	vproc_slave_vop(vproc);

	/* Account operation */
	vproc_count_vop(vproc, task, oper_rc);

	return (oper_rc != 0) ? oper_rc : post_rc;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_vproc_post_recv_asio(fnx_vproc_t *vproc, fnx_bkref_t *bkref)
{
	fnx_jelem_t *jelem = &bkref->bk_jelem;

	fnx_assert(bkref->bk_slaved);
	fnx_assert(bkref->bk_refcnt > 0);
	fnx_assert(jelem->jtype != FNX_JOB_NONE);
	jelem->jtype = FNX_JOB_NONE;
	bkref->bk_slaved = FNX_FALSE;
	bkref->bk_refcnt -= 1;
	fnx_pendq_unpend(&vproc->pendq, jelem);

	if (!bkref->bk_cached && !bkref->bk_refcnt) {
		fnx_pstor_retire_bk(vproc->pstor, bkref);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void vproc_reply_task(const fnx_vproc_t *vproc, fnx_task_t *task,
                             fnx_status_t status, fnx_bool_t fini)
{
	fnx_jelem_t *jelem;

	jelem = &task->tsk_jelem;
	jelem->status   = status;
	jelem->jtype    = fini ? FNX_JOB_TASK_FINI_RES : FNX_JOB_TASK_EXEC_RES;
	vproc->dispatch(vproc->server, jelem);
}

static void vproc_exec_task(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc;
	fnx_jelem_t *jelem = &task->tsk_jelem;

	/* Filter-out unsupported operation */
	if (fnx_get_vop(task->tsk_opcode) == NULL) {
		vproc_reply_task(vproc, task, -ENOTSUP, 0);
		return;
	}
	/* Execute operation via hook */
	rc = vproc_execute_vop(vproc, task);

	/* Special cases: halt/suspend task until I/O returns */
	if ((rc == FNX_EPEND) || (rc == FNX_EDELAY)) {
		jelem->status = rc;
		fnx_vproc_pend_task(vproc, task);
		return;
	}

	/* Reply task + status back to caller */
	vproc_reply_task(vproc, task, rc, 0);
}

static void vproc_fini_task(fnx_vproc_t *vproc, fnx_task_t *task)
{
	fnx_vproc_relax_iobufs(vproc, task);
	vproc_reply_task(vproc, task, 0, 1);
}

void fnx_vproc_exec_job(fnx_vproc_t *vproc, fnx_jelem_t *jelem)
{
	fnx_task_t  *task;
	fnx_bkref_t *bkref;

	switch (jelem->jtype) {
		case FNX_JOB_TASK_EXEC_REQ:
			task = fnx_jelem_to_task(jelem);
			vproc_exec_task(vproc, task);
			break;
		case FNX_JOB_TASK_FINI_REQ:
			task = fnx_jelem_to_task(jelem);
			vproc_fini_task(vproc, task);
			break;
		case FNX_JOB_BK_READ_RES:
		case FNX_JOB_BK_WRITE_RES:
		case FNX_JOB_BK_SYNC_RES:
			bkref = fnx_jelem_to_bkref(jelem);
			fnx_vproc_post_recv_asio(vproc, bkref);
			break;
		case FNX_JOB_TASK_FINI_RES:
		case FNX_JOB_TASK_EXEC_RES:
		case FNX_JOB_BK_READ_REQ:
		case FNX_JOB_BK_WRITE_REQ:
		case FNX_JOB_BK_SYNC_REQ:
		case FNX_JOB_NONE:
		default:
			fnx_panic("illegal-vproc-job: %d", jelem->jtype);
			break;
	}
}

void fnx_vproc_exec_pendq(fnx_vproc_t *vproc)
{
	int status;
	size_t limit;
	fnx_task_t *task;

	limit = vproc->pendq.pq.size;
	while (limit-- > 0) {
		task = fnx_vproc_pop_ptask(vproc);
		if (task == NULL) {
			break;
		}
		status = fnx_task_status(task);
		if (status == FNX_EPEND) {
			vproc_exec_task(vproc, task);
		} else {
			fnx_assert(status == FNX_EDELAY);
			vproc_reply_task(vproc, task, 0, 0);
		}
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_vproc_commit_dirty(fnx_vproc_t *vproc)
{
	/* FIXME */
	fnx_unused(vproc);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int vnode_isevictable(const fnx_vnode_t *vnode)
{
	if (vnode->v_refcnt > 0) {
		return FNX_FALSE;
	}
	if (vnode->v_pseudo || vnode->v_pinned) {
		return FNX_FALSE;
	}
	if (!fnx_vnode_ismutable(vnode)) {
		return FNX_FALSE;
	}
	return FNX_TRUE;
}

/* Heuristic based on common allocator accounting  and thresholds */
void fnx_vproc_squeeze(fnx_vproc_t *vproc, size_t cnt)
{
	fnx_vnode_t  *vnode;
	fnx_vcache_t *vcache = &vproc->cache->vc;

	while (cnt-- > 0) {
		vnode = fnx_vcache_poplru_vnode(vcache);
		if (vnode == NULL) {
			break;
		}
		if (!vnode_isevictable(vnode)) {
			fnx_vcache_store_vnode(vcache, vnode);
			break;
		}
		if (vnode->v_refcnt == 0) {
			vproc_evict_delete_vnode(vproc, vnode);
		} else {
			fnx_vcache_store_vnode(vcache, vnode);
			break;
		}
	}
}

fnx_task_t *fnx_vproc_pop_ptask(fnx_vproc_t *vproc)
{
	fnx_jelem_t *jelem = NULL;
	fnx_task_t  *task  = NULL;

	jelem = fnx_pendq_pfront(&vproc->pendq);
	if ((jelem != NULL) && (jelem->jtype == FNX_JOB_TASK_EXEC_REQ)) {
		fnx_pendq_unpend(&vproc->pendq, jelem);
		task = fnx_jelem_to_task(jelem);
	}
	return task;
}

void fnx_vproc_pend_task(fnx_vproc_t *vproc, fnx_task_t *task)
{
	fnx_pendq_pend(&vproc->pendq, &task->tsk_jelem);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_vproc_clear_caches(fnx_vproc_t *vproc)
{
	fnx_vnode_t *vnode;
	fnx_spmap_t *spmap;
	fnx_bkref_t *bkref;
	fnx_ucache_t *bc = &vproc->cache->uc;
	fnx_vcache_t *vc = &vproc->cache->vc;

	fnx_vproc_clear_filerefs(vproc);

	fnx_vcache_clear_des(vc);
	while ((vnode = fnx_vcache_poplru_vnode(vc)) != NULL) {
		fnx_assert(vnode->v_refcnt == 0);
		vproc_evict_delete_vnode(vproc, vnode);
	}
	while ((bkref = fnx_ucache_poplru_bk(bc)) != NULL) {
		/* XXX */
		fnx_assert(!bkref->bk_slaved);
		if ((bkref->bk_baddr.lba % FNX_BCKTNBK) != 0) {
			fnx_assert(bkref->bk_refcnt == 0);
		}
		/* XXX end */

		fnx_pstor_retire_bk(vproc->pstor, bkref);
	}
	while ((spmap = fnx_ucache_poplru_spmap(bc)) != NULL) {
		fnx_assert(spmap->sm_magic == FNX_SPMAP_MAGIC);
		fnx_assert(spmap->sm_vnode.v_refcnt == 0);
		fnx_pstor_retire_spmap(vproc->pstor, spmap);
	}

	fnx_assert(vc->vhtbl.size == 0);
	fnx_assert(bc->bkhtbl.size == 0);
	fnx_assert(bc->smhtbl.size == 0);
}
