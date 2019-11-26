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
#include "pstor-exec.h"
#include "pstor-private.h"

/* Local functions */
static int pstor_load_bk(fnx_pstor_t *, fnx_bkref_t *);
static int pstor_save_bk(fnx_pstor_t *, fnx_bkref_t *);
static int pstor_sync_bk(fnx_pstor_t *, fnx_bkref_t *);
static int pstor_load_bk_async(fnx_pstor_t *, fnx_bkref_t *);
static int pstor_save_bk_async(fnx_pstor_t *, fnx_bkref_t *);
static int pstor_sync_bk_async(fnx_pstor_t *, fnx_bkref_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const fnx_pstor_ops_t s_pstor_hooks = {
	.load_bk = pstor_load_bk,
	.save_bk = pstor_save_bk,
	.sync_bk = pstor_sync_bk
};

static const fnx_pstor_ops_t s_pstor_async_hooks = {
	.load_bk = pstor_load_bk_async,
	.save_bk = pstor_save_bk_async,
	.sync_bk = pstor_sync_bk_async
};

const fnx_pstor_ops_t *fnx_pstor_default_ops = &s_pstor_hooks;



void fnx_pstor_bind_ops(fnx_pstor_t *pstor, int have_async)
{
	if (have_async) {
		pstor->ops  = &s_pstor_async_hooks;
	} else {
		pstor->ops  = &s_pstor_hooks;
	}
}

int fnx_pstor_sync(const fnx_pstor_t *pstor)
{
	return fnx_bdev_sync(&pstor->bdev);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_pstor_retire_bk(const fnx_pstor_t *pstor, fnx_bkref_t *bkref)
{
	fnx_cache_t *cache = pstor->cache;

	if ((bkref != NULL) && !bkref->bk_refcnt) {
		if (bkref->bk_cached) {
			fnx_ucache_evict_bk(&cache->uc, bkref);
		}
		if (!bkref->bk_slaved) {
			fnx_pstor_del_bk2(pstor, bkref);
		}
	}
}

void fnx_pstor_retire_vnode(const fnx_pstor_t *pstor, fnx_vnode_t *vnode)
{
	fnx_bkref_t *bkref;

	if ((vnode != NULL) && (vnode->v_refcnt == 0)) {
		bkref = fnx_vnode_detach(vnode);
		if (bkref != NULL) {
			fnx_pstor_retire_bk(pstor, bkref);
		}
		fnx_pstor_del_vobj(pstor, vnode);
	}
}

static void pstor_put_spmap(fnx_pstor_t *pstor, fnx_spmap_t *spmap)
{
	/* XXX */
	fnx_vnode_t *vnode = &spmap->sm_vnode;
	fnx_pendq_stage(&pstor->pendq, &vnode->v_jelem);
}

void fnx_pstor_retire_spmap(const fnx_pstor_t *pstor, fnx_spmap_t *spmap)
{
	fnx_pstor_retire_vnode(pstor, &spmap->sm_vnode);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int pstor_fetch_cached_bk(fnx_pstor_t       *pstor,
                                 const fnx_baddr_t *baddrv,
                                 fnx_bkref_t      **p_bkref)
{
	fnx_baddr_t baddr;
	fnx_bkref_t *bkref = NULL;
	fnx_cache_t *cache = pstor->cache;

	fnx_baddr_floor(&baddr, baddrv);
	bkref = fnx_ucache_search_bk(&cache->uc, &baddr);
	if (bkref == NULL) {
		return FNX_ECACHEMISS;
	}
	if (!fnx_bkref_ismutable(bkref)) {
		return FNX_EPEND;
	}
	if (bkref->bk_jelem.jtype != FNX_JOB_NONE) {
		return FNX_EPEND;
	}
	*p_bkref = bkref;
	return 0;
}

static int pstor_fetch_stable_bk(fnx_pstor_t       *pstor,
                                 const fnx_baddr_t *baddrv,
                                 fnx_spmap_t       *spmap,
                                 fnx_bkref_t      **p_bkref)
{
	int rc = 0;
	fnx_baddr_t baddr;
	fnx_bkref_t *bkref = NULL;
	fnx_cache_t *cache = pstor->cache;

	fnx_baddr_floor(&baddr, baddrv);
	rc = fnx_pstor_new_bk2(pstor, &baddr, spmap, &bkref);
	if (rc != 0) {
		return rc;
	}
	fnx_assert(!bkref->bk_slaved);
	rc = pstor->ops->load_bk(pstor, bkref);
	if (rc == 0) {
		fnx_ucache_store_bk(&cache->uc, bkref);
		*p_bkref = bkref;
	} else if (rc == FNX_EPEND) {
		fnx_ucache_store_bk(&cache->uc, bkref);
		*p_bkref = NULL;
	} else {
		fnx_assert(0);
		fnx_pstor_del_bk2(pstor, bkref);
	}
	return rc;
}


static int pstor_fetch_bk(fnx_pstor_t       *pstor,
                          const fnx_baddr_t *baddrv,
                          fnx_spmap_t       *spmap,
                          fnx_bkref_t      **bkref)
{
	int rc;

	rc = pstor_fetch_cached_bk(pstor, baddrv, bkref);
	if (rc == FNX_ECACHEMISS) {
		rc = pstor_fetch_stable_bk(pstor, baddrv, spmap, bkref);
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int pstor_fetch_cached_spmap(fnx_pstor_t       *pstor,
                                    const fnx_vaddr_t *vaddr,
                                    fnx_spmap_t      **p_spmap)
{
	fnx_baddr_t sm_baddr;
	fnx_vaddr_t sm_vaddr;
	fnx_spmap_t *spmap = NULL;

	fnx_super_resolve_sm(pstor->super, vaddr, &sm_vaddr, &sm_baddr);
	spmap = fnx_ucache_search_spmap(&pstor->cache->uc, &sm_vaddr);
	if (spmap == NULL) {
		return FNX_ECACHEMISS;
	}
	if (!fnx_vnode_ismutable(&spmap->sm_vnode)) {
		return FNX_EPEND;
	}
	*p_spmap = spmap;
	return 0;
}

static int pstor_fetch_stable_spmap(fnx_pstor_t       *pstor,
                                    const fnx_vaddr_t *vaddr,
                                    fnx_spmap_t      **p_spmap)
{
	int rc;
	fnx_baddr_t sm_baddr;
	fnx_vaddr_t sm_vaddr;
	fnx_bkref_t *bkref;
	fnx_vnode_t *vnode = NULL;
	fnx_spmap_t *spmap = NULL;

	fnx_super_resolve_sm(pstor->super, vaddr, &sm_vaddr, &sm_baddr);
	rc = pstor_fetch_bk(pstor, &sm_baddr, NULL, &bkref);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_pstor_new_vobj(pstor, &sm_vaddr, &vnode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_decode_vobj(vnode, bkref, &sm_baddr);
	if (rc != 0) {
		fnx_pstor_del_vobj(pstor, vnode);
		return rc;
	}
	spmap = fnx_vnode_to_spmap(vnode);
	fnx_spmap_setup(spmap, &sm_vaddr, &sm_baddr, bkref);
	fnx_ucache_store_spmap(&pstor->cache->uc, spmap);

	*p_spmap = spmap;
	return 0;
}

static int pstor_fetch_spmap(fnx_pstor_t       *pstor,
                             const fnx_vaddr_t *vaddr,
                             fnx_spmap_t      **spmap)
{
	int rc;

	rc = pstor_fetch_cached_spmap(pstor, vaddr, spmap);
	if (rc == FNX_ECACHEMISS) {
		rc = pstor_fetch_stable_spmap(pstor, vaddr, spmap);
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_pstor_require_vaddr(fnx_pstor_t  *pstor, const fnx_vaddr_t *vaddr)
{
	int rc;
	fnx_baddr_t baddr;
	fnx_size_t  nfrgs  = 0;
	fnx_bkref_t *bkref = NULL;
	fnx_spmap_t *spmap = NULL;

	rc = pstor_fetch_spmap(pstor, vaddr, &spmap);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_spmap_predict(spmap, vaddr, &baddr);
	if (rc != 0) {
		return -ENOSPC;
	}
	rc = fnx_spmap_usageat(spmap, &baddr, &nfrgs);
	if (rc != 0) {
		return -ENOSPC;
	}
	if (!fnx_vnode_ismutable(&spmap->sm_vnode)) {
		return FNX_EPEND;
	}
	if (0 < nfrgs) {
		rc = pstor_fetch_bk(pstor, &baddr, spmap, &bkref);
		if (rc != 0) {
			return rc;
		}
		if (!fnx_bkref_ismutable(bkref)) {
			return FNX_EPEND;
		}
	}
	return 0;
}

int fnx_pstor_stage_vnode(fnx_pstor_t       *pstor,
                          const fnx_vaddr_t *vaddr,
                          fnx_vnode_t      **p_vnode)
{
	int rc;
	fnx_baddr_t baddr;
	fnx_bkref_t *bkref = NULL;
	fnx_spmap_t *spmap = NULL;
	fnx_vnode_t *vnode = NULL;
	const fnx_vtype_e vtype = vaddr->vtype;

	rc = pstor_fetch_spmap(pstor, vaddr, &spmap);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_spmap_lookup(spmap, vaddr, &baddr);
	if (rc != 0) {
		return -ENOENT;
	}
	rc = pstor_fetch_bk(pstor, &baddr, spmap, &bkref);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_pstor_new_vobj(pstor, vaddr, &vnode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_decode_vobj(vnode, bkref, &baddr);
	if (rc != 0) {
		fnx_pstor_del_vobj(pstor, vnode);
		return rc;
	}
	fnx_vnode_attach(vnode, &baddr, bkref);
	vnode->v_placed = FNX_TRUE;

	if (vtype == FNX_VTYPE_VBK) {
		fnx_assert(bkref->bk_cached);
		fnx_ucache_evict_bk(&pstor->cache->uc, bkref);
	}
	*p_vnode = vnode;
	return 0;
}

static int pstor_prep_obtain(fnx_pstor_t       *pstor,
                             const fnx_spmap_t *spmap,
                             const fnx_vaddr_t *vaddr,
                             fnx_bkref_t       *bkref_in,
                             fnx_bkref_t      **p_bkref)
{
	int rc;
	fnx_baddr_t baddr;
	fnx_size_t  nfrgs  = 0;
	fnx_bkref_t *bkref = NULL;

	rc = fnx_spmap_predict(spmap, vaddr, &baddr);
	if (rc != 0) {
		return -ENOSPC;
	}
	rc = fnx_spmap_usageat(spmap, &baddr, &nfrgs);
	if (rc != 0) {
		return -ENOSPC;
	}
	fnx_baddr_floor(&baddr, &baddr);
	if (bkref_in != NULL) {
		/* Trivial case: using user's input bk */
		bkref = bkref_in;
		fnx_bkref_bind(bkref, &baddr);
	} else if (nfrgs == 0) {
		/* Unused block: used cached or create new entry + zero */
		rc = pstor_fetch_cached_bk(pstor, &baddr, &bkref);
		if (rc == FNX_ECACHEMISS) {
			rc = fnx_pstor_new_bk(pstor, &baddr, &bkref);
		}
		if (rc != 0) {
			return rc;
		}
		fnx_bkref_bzero(bkref);
	} else {
		/* Existing block: fetch from stable storage */
		rc = pstor_fetch_bk(pstor, &baddr, NULL, &bkref);
		if (rc != 0) {
			return rc;
		}
	}
	*p_bkref = bkref;
	return 0;
}

static void pstor_post_obtain(fnx_pstor_t *pstor,
                              fnx_spmap_t *spmap,
                              fnx_bkref_t *bkref)
{
	fnx_cache_t *cache = pstor->cache;

	if (bkref->bk_spmap == NULL) {
		fnx_bkref_attach(bkref, spmap);
	}
	if (!bkref->bk_cached) {
		fnx_ucache_store_bk(&cache->uc, bkref);
	}
}

int fnx_pstor_spawn_vnode(fnx_pstor_t       *pstor,
                          const fnx_vaddr_t *vaddr,
                          fnx_bkref_t       *bkref_in,
                          fnx_vnode_t      **p_vnode)
{
	int rc;
	fnx_baddr_t baddr;
	fnx_spmap_t *spmap = NULL;
	fnx_vnode_t *vnode = NULL;
	fnx_bkref_t *bkref = bkref_in;

	rc = pstor_fetch_spmap(pstor, vaddr, &spmap);
	if (rc != 0) {
		return rc;
	}
	rc = pstor_prep_obtain(pstor, spmap, vaddr, bkref_in, &bkref);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_pstor_new_vobj(pstor, vaddr, &vnode);
	if (rc != 0) {
		return rc;
	}
	/* XXX */
	rc = fnx_spmap_lookup(spmap, vaddr, &baddr);
	fnx_assert(rc != 0);

	rc = fnx_spmap_insert(spmap, vaddr, &baddr);
	fnx_assert(rc == 0);
	if (rc != 0) {
		fnx_pstor_del_vobj(pstor, vnode);
		return -ENOSPC;
	}
	fnx_vnode_attach(vnode, &baddr, bkref);
	vnode->v_placed = FNX_TRUE;
	pstor_put_spmap(pstor, spmap);
	pstor_post_obtain(pstor, spmap, bkref);

	*p_vnode = vnode;
	return 0;
}

int fnx_pstor_commit_vnode(fnx_pstor_t *pstor, fnx_vnode_t *vnode)
{
	int rc;
	fnx_bkref_t *bkref = vnode->v_bkref;
	const fnx_baddr_t *baddr = &vnode->v_baddr;

	fnx_assert(baddr->lba != 0);
	fnx_assert(bkref != NULL);
	fnx_assert(!bkref->bk_slaved);
	fnx_assert(bkref->bk_refcnt > 0);
	fnx_assert(bkref->bk_cached);
	fnx_assert(baddr->lba == bkref->bk_baddr.lba);
	fnx_assert(vnode->v_placed);

	rc = fnx_encode_vobj(vnode, bkref, baddr);
	fnx_assert(rc == 0);
	if (rc != 0)  {
		return rc;
	}
	rc = pstor->ops->save_bk(pstor, bkref);
	if (rc == 0) {
		vnode->v_placed = FNX_TRUE;
	}
	return rc;
}

static void pstor_detach_evict_vbk(fnx_pstor_t *pstor, fnx_vnode_t *vbk)
{
	fnx_bkref_t *bkref;
	fnx_ucache_t *ucache = &pstor->cache->uc;

	bkref = fnx_vnode_detach(vbk);
	if (bkref != NULL) {
		if (bkref->bk_cached) {
			fnx_ucache_evict_bk(ucache, bkref);
		}
		if (bkref->bk_refcnt == 0) {
			fnx_pstor_del_bk2(pstor, bkref);
		}
	}
}

int fnx_pstor_unmap_vnode(fnx_pstor_t *pstor, fnx_vnode_t *vnode)
{
	int rc;
	fnx_spmap_t *spmap = NULL;
	const fnx_vaddr_t *vaddr = &vnode->v_vaddr;
	const fnx_vtype_e  vtype = vaddr->vtype;

	fnx_assert(vtype != FNX_VTYPE_SPMAP);
	fnx_assert(vnode->v_placed);

	rc = pstor_fetch_spmap(pstor, vaddr, &spmap);
	fnx_assert(rc == 0);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_spmap_remove(spmap, vaddr);
	if (rc != 0) {
		return -ENOENT;
	}
	vnode->v_placed = FNX_FALSE;
	if (vtype == FNX_VTYPE_VBK) {
		pstor_detach_evict_vbk(pstor, vnode);
	}
	pstor_put_spmap(pstor, spmap);
	return rc;
}

int fnx_pstor_sync_vnode(fnx_pstor_t *pstor, fnx_vnode_t *vnode)
{
	int rc;
	fnx_bkref_t *bkref;

	bkref = vnode->v_bkref;
	fnx_assert(bkref != NULL);
	fnx_assert(!bkref->bk_slaved);
	rc = pstor->ops->sync_bk(pstor, bkref);
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int pstor_load_bk(fnx_pstor_t *pstor, fnx_bkref_t *bkref)
{
	return fnx_pstor_read_bk(pstor, bkref);
}

static int pstor_save_bk(fnx_pstor_t *pstor, fnx_bkref_t *bkref)
{
	return fnx_pstor_write_bk(pstor, bkref);
}

static int pstor_sync_bk(fnx_pstor_t *pstor, fnx_bkref_t *bkref)
{
	fnx_pstor_sync(pstor);
	fnx_unused(bkref);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int bkref_isevictable(const fnx_bkref_t *bkref)
{
	return (bkref->bk_refcnt == 0) && !bkref->bk_slaved;
}

static int spmap_isevictable(const fnx_spmap_t *spmap)
{
	const fnx_bkref_t *bkref = spmap->sm_vnode.v_bkref;
	return (spmap->sm_vnode.v_refcnt == 0) && !bkref->bk_slaved;
}

static void pstor_squeeze_bklru(fnx_pstor_t *pstor, size_t cnt)
{
	size_t lim = 2, restore = 0;
	fnx_bkref_t *bkref;
	fnx_ucache_t *ucache = &pstor->cache->uc;

	while ((cnt-- > 0) && (restore < lim)) {
		bkref = fnx_ucache_poplru_bk(ucache);
		if (bkref == NULL) {
			break;
		}
		if (bkref_isevictable(bkref)) {
			fnx_pstor_del_bk2(pstor, bkref);
		} else {
			fnx_ucache_store_bk(ucache, bkref);
			restore++;
		}
	}
}

static void pstor_squeeze_smlru(fnx_pstor_t *pstor, size_t cnt)
{
	size_t lim = 1, restore = 0;
	fnx_spmap_t *spmap;
	fnx_ucache_t *ucache = &pstor->cache->uc;

	while ((cnt-- > 0) && (restore < lim)) {
		spmap = fnx_ucache_poplru_spmap(ucache);
		if (spmap == NULL) {
			break;
		}
		if (spmap_isevictable(spmap)) {
			fnx_pstor_retire_spmap(pstor, spmap);
		} else {
			fnx_ucache_store_spmap(ucache, spmap);
			restore++;
		}
	}
}

void fnx_pstor_squeeze(fnx_pstor_t *pstor, size_t cnt)
{
	pstor_squeeze_smlru(pstor, cnt);
	pstor_squeeze_bklru(pstor, cnt);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_bkref_t *fnx_pstor_pop_sbk(fnx_pstor_t *pstor)
{
	fnx_link_t  *qlnk;
	fnx_bkref_t *bkref = NULL;

	qlnk = fnx_list_pop_front(&pstor->sbkq);
	if (qlnk != NULL) {
		bkref = fnx_qlink_to_bkref(qlnk);
	}
	return bkref;
}

static void pstor_push_sbk(fnx_pstor_t *pstor,
                           fnx_bkref_t *bkref, fnx_jtype_e jtype)
{
	fnx_link_t *qlnk;

	fnx_assert(!bkref->bk_slaved);
	qlnk = fnx_bkref_to_qlink(bkref);
	if (!fnx_link_islinked(qlnk)) {
		fnx_assert(bkref->bk_jelem.jtype == FNX_JOB_NONE);
		bkref->bk_jelem.jtype = jtype;
		fnx_list_push_back(&pstor->sbkq, qlnk);
	} else {
		fnx_assert(bkref->bk_jelem.jtype == jtype);
		fnx_assert(fnx_link_isunlinked(&bkref->bk_jelem.plink));
	}
}

static int pstor_load_bk_async(fnx_pstor_t *pstor, fnx_bkref_t *bkref)
{
	pstor_push_sbk(pstor, bkref, FNX_JOB_BK_READ_REQ);
	return FNX_EPEND;
}

static int pstor_save_bk_async(fnx_pstor_t *pstor, fnx_bkref_t *bkref)
{
	pstor_push_sbk(pstor, bkref, FNX_JOB_BK_WRITE_REQ);
	return FNX_EDELAY;
}

static int pstor_sync_bk_async(fnx_pstor_t *pstor, fnx_bkref_t *bkref)
{
	pstor_push_sbk(pstor, bkref, FNX_JOB_BK_SYNC_REQ);
	return FNX_EDELAY;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void pstor_reply_bk(const fnx_pstor_t *pstor, fnx_bkref_t *bkref,
                           fnx_jtype_e jtype, fnx_status_t status)
{
	fnx_jelem_t *jelem = &bkref->bk_jelem;

	fnx_assert(status == 0);
	jelem->status   = status;
	jelem->jtype    = jtype;
	pstor->dispatch(pstor->server, jelem);
}

void fnx_pstor_exec_job(fnx_pstor_t *pstor, fnx_jelem_t *jelem)
{
	int rc;
	fnx_bkref_t *bkref;

	switch (jelem->jtype) {
		case FNX_JOB_BK_READ_REQ:
			bkref = fnx_jelem_to_bkref(jelem);
			rc = fnx_pstor_read_bk(pstor, bkref);
			pstor_reply_bk(pstor, bkref, FNX_JOB_BK_READ_RES, rc);
			break;
		case FNX_JOB_BK_WRITE_REQ:
			bkref = fnx_jelem_to_bkref(jelem);
			rc = fnx_pstor_write_bk(pstor, bkref);
			pstor_reply_bk(pstor, bkref, FNX_JOB_BK_WRITE_RES, rc);
			break;
		case FNX_JOB_BK_SYNC_REQ:
			bkref = fnx_jelem_to_bkref(jelem);
			rc = fnx_pstor_sync_bk(pstor, bkref);
			pstor_reply_bk(pstor, bkref, FNX_JOB_BK_SYNC_RES, rc);
			break;
		case FNX_JOB_NONE:
		case FNX_JOB_TASK_EXEC_REQ:
		case FNX_JOB_TASK_EXEC_RES:
		case FNX_JOB_TASK_FINI_REQ:
		case FNX_JOB_TASK_FINI_RES:
		case FNX_JOB_BK_READ_RES:
		case FNX_JOB_BK_WRITE_RES:
		case FNX_JOB_BK_SYNC_RES:
		default:
			fnx_panic("illegal-pstor-job: jtype=%d", (int)jelem->jtype);
	}
}
