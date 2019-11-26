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
#include <sys/mman.h>

#include <fnxhash.h>
#include <fnxinfra.h>
#include "core-blobs.h"
#include "core-types.h"
#include "core-stats.h"
#include "core-addr.h"
#include "core-elems.h"
#include "core-inode.h"
#include "core-space.h"
#include "core-alloc.h"
#include "core-cache.h"


/* Default number of entries in hash-tables */
/* TODO: Dynamic value based on boot settings */
#define FNX_CACHE_NENT      (393241)


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Memory allocation helpers: align to mmap block allocation */
static void *mmalloc(size_t sz)
{
	void *mem;
	const size_t nbk = fnx_bytes_to_nbk(sz);

	mem = fnx_mmap_bks(nbk);
	if (mem != NULL) {
		memset(mem, 0, sz); /* Assuming case MAP_UNINITIALIZED */
	}
	return mem;
}

static void mfree(void *mem, size_t sz)
{
	const size_t nbk = fnx_bytes_to_nbk(sz);
	fnx_munmap_bks(mem, nbk);
}


/* LRUs helper */
static void transfer(fnx_link_t *lnk, fnx_list_t *from, fnx_list_t *to)
{
	if (from != NULL) {
		fnx_list_remove(from, lnk);
	}
	if (to != NULL) {
		fnx_list_push_back(to, lnk);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_bool_t isequal(const fnx_vaddr_t *vaddr1, const fnx_vaddr_t *vaddr2)
{
	return fnx_vaddr_isequal(vaddr1, vaddr2);
}

static unsigned long vaddr_hkey(const fnx_vaddr_t *vaddr)
{
	unsigned long hkey, base, vino, vxno;

	base = (unsigned long)(vaddr->vtype) * 12582917UL;
	vino = (unsigned long)(vaddr->ino);
	vxno = (unsigned long)(vaddr->xno);
	hkey = fnx_rotleft(base, 17) ^ vino ^ ~vxno;

	return hkey;
}

static void vhtbl_init(fnx_vhtbl_t *vhtbl)
{
	vhtbl->size = 0;
	vhtbl->tcap = 0;
	vhtbl->htbl = NULL;
}

static void vhtbl_destroy(fnx_vhtbl_t *vhtbl)
{
	vhtbl->size = 0;
	vhtbl->tcap = 0;
	vhtbl->htbl = NULL;
}

static int vhtbl_setup(fnx_vhtbl_t *vhtbl, size_t nent)
{
	size_t sz, esz = sizeof(vhtbl->htbl[0]);

	if (vhtbl->htbl != NULL) {
		sz = vhtbl->tcap * esz;
		mfree(vhtbl->htbl, sz);
		vhtbl_init(vhtbl);
	}
	if (nent > 0) {
		sz = nent * esz;
		vhtbl->htbl = (fnx_vnode_t **)mmalloc(sz);
		if (vhtbl->htbl == NULL) {
			return -1;
		}
		vhtbl->tcap = nent;
	}
	return 0;
}

static size_t vhtbl_gethpos(const fnx_vhtbl_t *vhtbl,
                            const fnx_vaddr_t *vaddr)
{
	unsigned long hkey, hpos;

	hkey = vaddr_hkey(vaddr);
	hpos = hkey % vhtbl->tcap;
	return hpos;
}

static fnx_vnode_t *vhtbl_lookup(const fnx_vhtbl_t *vhtbl,
                                 const fnx_vaddr_t *vaddr)
{
	size_t hpos;
	const fnx_vnode_t *vnode;
	fnx_vnode_t *const *hlnk;

	hpos = vhtbl_gethpos(vhtbl, vaddr);
	hlnk = &vhtbl->htbl[hpos];
	while ((vnode = *hlnk) != NULL) {
		if (isequal(&vnode->v_vaddr, vaddr)) {
			break;
		}
		hlnk = &vnode->v_hlnk;
	}
	return (fnx_vnode_t *)vnode;
}

static void vhtbl_insert(fnx_vhtbl_t *vhtbl, fnx_vnode_t *vnode)
{
	size_t hpos;
	fnx_vnode_t **hlnk;

	hpos = vhtbl_gethpos(vhtbl, &vnode->v_vaddr);
	hlnk = &vhtbl->htbl[hpos];
	vnode->v_hlnk = *hlnk;
	*hlnk = vnode;
	vhtbl->size += 1;
}

static void vhtbl_remove(fnx_vhtbl_t *vhtbl, fnx_vnode_t *vnode)
{
	size_t hpos;
	fnx_vnode_t **hlnk;

	hpos = vhtbl_gethpos(vhtbl, &vnode->v_vaddr);
	hlnk = &vhtbl->htbl[hpos];
	while (*hlnk != vnode) {
		hlnk = &(*hlnk)->v_hlnk;
	}
	*hlnk = vnode->v_hlnk;
	vnode->v_hlnk = NULL;
	vhtbl->size -= 1;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_bkref_t *clink_to_bkref(const fnx_link_t *lnk)
{
	const fnx_bkref_t *bkref;

	bkref = fnx_container_of(lnk, fnx_bkref_t, bk_clink);
	fnx_assert(bkref->bk_magic == FNX_BKREF_MAGIC);
	return (fnx_bkref_t *)bkref;
}

static unsigned long baddr_hkey(const fnx_baddr_t *baddr)
{
	return blgn_wang(baddr->lba);
}

static void bkhtbl_init(fnx_bkhtbl_t *bkhtbl)
{
	bkhtbl->size = 0;
	bkhtbl->tcap = 0;
	bkhtbl->htbl = NULL;
}

static void bkhtbl_destroy(fnx_bkhtbl_t *bkhtbl)
{
	bkhtbl->size = 0;
	bkhtbl->tcap = 0;
	bkhtbl->htbl = NULL;
}

static int bkhtbl_setup(fnx_bkhtbl_t *bkhtbl, size_t nent)
{
	size_t sz, esz = sizeof(bkhtbl->htbl[0]);

	if (bkhtbl->htbl != NULL) {
		sz = bkhtbl->tcap * esz;
		mfree(bkhtbl->htbl, sz);
		bkhtbl_init(bkhtbl);
	}
	if (nent > 0) {
		sz = nent * esz;
		bkhtbl->htbl = (fnx_bkref_t **)mmalloc(sz);
		if (bkhtbl->htbl == NULL) {
			return -1;
		}
		bkhtbl->tcap = nent;
	}
	return 0;
}

static size_t bkhtbl_gethpos(const fnx_bkhtbl_t *bkhtbl,
                             const fnx_baddr_t *baddr)
{
	unsigned long hkey, hpos;

	hkey = baddr_hkey(baddr);
	hpos = hkey % bkhtbl->tcap;
	return hpos;
}

static fnx_bkref_t *bkhtbl_lookup(const fnx_bkhtbl_t *bkhtbl,
                                  const fnx_baddr_t *baddr)
{
	size_t hpos;
	fnx_bkref_t *bkref;
	fnx_bkref_t *const *hlnk;

	hpos = bkhtbl_gethpos(bkhtbl, baddr);
	hlnk = &bkhtbl->htbl[hpos];
	while ((bkref = *hlnk) != NULL) {
		if (fnx_baddr_isequal(&bkref->bk_baddr, baddr)) {
			break;
		}
		hlnk = &bkref->bk_hlnk;
	}
	return bkref;
}

static void bkhtbl_insert(fnx_bkhtbl_t *bkhtbl, fnx_bkref_t *bkref)
{
	size_t hpos;
	fnx_bkref_t **hlnk;

	hpos = bkhtbl_gethpos(bkhtbl, &bkref->bk_baddr);
	hlnk = &bkhtbl->htbl[hpos];
	bkref->bk_hlnk = *hlnk;
	*hlnk = bkref;
	bkhtbl->size += 1;
}

static void bkhtbl_remove(fnx_bkhtbl_t *bkhtbl, fnx_bkref_t *bkref)
{
	size_t hpos;
	fnx_bkref_t **hlnk;

	fnx_assert(bkhtbl->size > 0);

	hpos = bkhtbl_gethpos(bkhtbl, &bkref->bk_baddr);
	hlnk = &bkhtbl->htbl[hpos];
	while (*hlnk != bkref) {
		fnx_assert(*hlnk != NULL);
		hlnk = &(*hlnk)->bk_hlnk;
	}
	*hlnk = bkref->bk_hlnk;
	bkref->bk_hlnk = NULL;
	bkhtbl->size -= 1;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void dehtbl_init(fnx_dehtbl_t *dehtbl)
{
	dehtbl->tcap = 0;
	dehtbl->htbl = NULL;
}

static void dehtbl_destroy(fnx_dehtbl_t *dehtbl)
{
	dehtbl->tcap = 0;
	dehtbl->htbl = NULL;
}

static int dehtbl_setup(fnx_dehtbl_t *dehtbl, size_t nent)
{
	size_t sz, esz = sizeof(dehtbl->htbl[0]);

	if (dehtbl->htbl != NULL) {
		sz = dehtbl->tcap * esz;
		mfree(dehtbl->htbl, sz);
		dehtbl_init(dehtbl);
	}
	if (nent > 0) {
		sz = nent * esz;
		dehtbl->htbl = (fnx_dentry_t *)mmalloc(sz);
		if (dehtbl->htbl == NULL) {
			return -1;
		}
		dehtbl->tcap = nent;
	}
	return 0;
}

static void dehtbl_clear(fnx_dehtbl_t *dehtbl)
{
	for (size_t i = 0; i < dehtbl->tcap; ++i) {
		dehtbl->htbl[i].ino = FNX_INO_NULL;
	}
}

static fnx_hash_t dehash(fnx_ino_t dino, fnx_hash_t hash, fnx_size_t nlen)
{
	fnx_hash_t hval = 0;

	hval = hash ^ ~(fnx_hash_t)dino;
	hval ^= (fnx_hash_t)fnx_rotleft((nlen * 8648737), 19);

	return hval;
}

static fnx_dentry_t *dehtbl_gethent(const fnx_dehtbl_t *dehtbl, fnx_ino_t dino,
                                    fnx_hash_t hash, fnx_size_t nlen)
{
	size_t hpos;
	fnx_hash_t hval;
	const fnx_dentry_t *dent;

	hval = dehash(dino, hash, nlen);
	hpos = hval % dehtbl->tcap;
	dent = &dehtbl->htbl[hpos];

	return (fnx_dentry_t *)dent;
}

static void dehtbl_remap(fnx_dehtbl_t *dehtbl, fnx_ino_t dino,
                         fnx_hash_t hash, fnx_size_t nlen, fnx_ino_t ino)
{
	fnx_dentry_t *dent;

	dent = dehtbl_gethent(dehtbl, dino, hash, nlen);
	dent->dino = dino;
	dent->hash = hash;
	dent->nlen = nlen;
	dent->ino  = ino;   /* New|Override */
}

static const fnx_dentry_t *
dehtbl_lookup(const fnx_dehtbl_t *dehtbl, fnx_ino_t dino,
              fnx_hash_t hash, fnx_size_t nlen)
{
	fnx_dentry_t *dent, *dent2 = NULL;

	dent = dehtbl_gethent(dehtbl, dino, hash, nlen);
	if ((dent->ino != FNX_INO_NULL) &&
	    (dent->hash == hash) &&
	    (dent->dino == dino) &&
	    (dent->nlen == nlen)) {
		dent2 = dent;
	}
	return dent2;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/


static void ucache_belru_bk(fnx_ucache_t *ucache, fnx_bkref_t *bkref)
{
	transfer(&bkref->bk_clink, NULL, &ucache->bklru);
}

static void ucache_relru_bk(fnx_ucache_t *ucache, fnx_bkref_t *bkref)
{
	transfer(&bkref->bk_clink, &ucache->bklru, &ucache->bklru);
}

static void ucache_unlru_bk(fnx_ucache_t *ucache, fnx_bkref_t *bkref)
{
	transfer(&bkref->bk_clink, &ucache->bklru, NULL);
}

static void ucache_assert_no_other(const fnx_ucache_t *ucache,
                                   const fnx_bkref_t  *bkref)
{
	const fnx_bkref_t  *other;
	const fnx_bkhtbl_t *bkhtbl = &ucache->bkhtbl;

	other = bkhtbl_lookup(bkhtbl, &bkref->bk_baddr);
	fnx_assert(other == NULL);
}


void fnx_ucache_store_bk(fnx_ucache_t *ucache, fnx_bkref_t *bkref)
{
	fnx_bkhtbl_t *bkhtbl = &ucache->bkhtbl;

	ucache_assert_no_other(ucache, bkref);
	bkhtbl_insert(bkhtbl, bkref);
	bkref->bk_cached = FNX_TRUE;
	ucache_belru_bk(ucache, bkref);
}

fnx_bkref_t *fnx_ucache_search_bk(fnx_ucache_t *cache,
                                  const fnx_baddr_t *baddr)
{
	fnx_bkref_t *bkref;
	const fnx_bkhtbl_t *bkhtbl = &cache->bkhtbl;

	bkref = bkhtbl_lookup(bkhtbl, baddr);
	if (bkref != NULL) {
		ucache_relru_bk(cache, bkref);
	}
	return bkref;
}

void fnx_ucache_evict_bk(fnx_ucache_t *ucache, fnx_bkref_t *bkref)
{
	fnx_bkhtbl_t *bkhtbl = &ucache->bkhtbl;

	fnx_assert(bkref->bk_cached);
	bkhtbl_remove(bkhtbl, bkref);
	ucache_unlru_bk(ucache, bkref);
	bkref->bk_cached = FNX_FALSE;
}

fnx_bkref_t *fnx_ucache_poplru_bk(fnx_ucache_t *ucache)
{
	fnx_link_t  *lnk;
	fnx_bkref_t *bkref = NULL;

	lnk = fnx_list_front(&ucache->bklru);
	if (lnk != NULL) {
		bkref = clink_to_bkref(lnk);
		fnx_assert(bkref->bk_magic == FNX_BKREF_MAGIC);
		fnx_ucache_evict_bk(ucache, bkref);
	}
	return bkref;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_vnode_t *clink_to_vnode(const fnx_link_t *lnk)
{
	return fnx_container_of(lnk, fnx_vnode_t, v_clink);
}

static void ucache_belru_spmap(fnx_ucache_t *ucache, fnx_spmap_t *spmap)
{
	transfer(&spmap->sm_vnode.v_clink, NULL, &ucache->smlru);
}

static void ucache_relru_spmap(fnx_ucache_t *ucache, fnx_spmap_t *spmap)
{
	transfer(&spmap->sm_vnode.v_clink, &ucache->smlru, &ucache->smlru);
}

static void ucache_unlru_spmap(fnx_ucache_t *ucache, fnx_spmap_t *spmap)
{
	transfer(&spmap->sm_vnode.v_clink, &ucache->smlru, NULL);
}

void fnx_ucache_store_spmap(fnx_ucache_t *ucache, fnx_spmap_t *spmap)
{
	fnx_vnode_t *vnode = &spmap->sm_vnode;

	fnx_assert(!vnode->v_cached);
	vhtbl_insert(&ucache->smhtbl, vnode);
	ucache_belru_spmap(ucache, spmap);
	vnode->v_cached = FNX_TRUE;
}

fnx_spmap_t *fnx_ucache_search_spmap(fnx_ucache_t *ucache,
                                     const fnx_vaddr_t *vaddr)
{
	fnx_vnode_t *vnode;
	fnx_spmap_t *spmap = NULL;

	vnode = vhtbl_lookup(&ucache->smhtbl, vaddr);
	if (vnode != NULL) {
		spmap = fnx_vnode_to_spmap(vnode);
		ucache_relru_spmap(ucache, spmap);
		fnx_assert(spmap->sm_vnode.v_bkref != NULL);
	}
	return spmap;
}

void fnx_ucache_evict_spmap(fnx_ucache_t *ucache, fnx_spmap_t *spmap)
{
	fnx_vnode_t *vnode = &spmap->sm_vnode;

	vhtbl_remove(&ucache->smhtbl, vnode);
	ucache_unlru_spmap(ucache, spmap);
	vnode->v_cached = FNX_FALSE;
}

fnx_spmap_t *fnx_ucache_poplru_spmap(fnx_ucache_t *ucache)
{
	fnx_link_t  *lnk;
	fnx_vnode_t *vnode;
	fnx_spmap_t *spmap = NULL;

	lnk = fnx_list_front(&ucache->smlru);
	if (lnk != NULL) {
		vnode = clink_to_vnode(lnk);
		spmap = fnx_vnode_to_spmap(vnode);
		fnx_ucache_evict_spmap(ucache, spmap);
	}
	return spmap;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_ucache_init(fnx_ucache_t *ucache)
{
	fnx_bzero(ucache, sizeof(*ucache));
	bkhtbl_init(&ucache->bkhtbl);
	vhtbl_init(&ucache->smhtbl);
	fnx_list_init(&ucache->smlru);
	fnx_list_init(&ucache->bklru);
}

void fnx_ucache_destroy(fnx_ucache_t *ucache)
{
	bkhtbl_setup(&ucache->bkhtbl, 0);
	bkhtbl_destroy(&ucache->bkhtbl);
	vhtbl_setup(&ucache->smhtbl, 0);
	vhtbl_destroy(&ucache->smhtbl);
	fnx_list_destroy(&ucache->smlru);
	fnx_list_destroy(&ucache->bklru);
}

int fnx_ucache_setup(fnx_ucache_t *ucache)
{
	int rc;
	const size_t nent = FNX_CACHE_NENT;

	rc = vhtbl_setup(&ucache->smhtbl, nent);
	if (rc != 0) {
		return rc;
	}
	rc = bkhtbl_setup(&ucache->bkhtbl, nent);
	if (rc != 0) {
		vhtbl_setup(&ucache->smhtbl, 0); /* Failure-cleanup */
		return rc;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_vcache_init(fnx_vcache_t *vcache)
{
	fnx_bzero(vcache, sizeof(*vcache));
	dehtbl_init(&vcache->dehtbl);
	vhtbl_init(&vcache->vhtbl);
	fnx_list_init(&vcache->vlru);
}

void fnx_vcache_destroy(fnx_vcache_t *vcache)
{
	dehtbl_setup(&vcache->dehtbl, 0);
	vhtbl_setup(&vcache->vhtbl, 0);
	dehtbl_destroy(&vcache->dehtbl);
	vhtbl_destroy(&vcache->vhtbl);
	fnx_list_destroy(&vcache->vlru);
}

int fnx_vcache_setup(fnx_vcache_t *vcache)
{
	int rc;
	const size_t nent = FNX_CACHE_NENT;

	rc = vhtbl_setup(&vcache->vhtbl, nent);
	if (rc != 0) {
		return rc;
	}
	rc = dehtbl_setup(&vcache->dehtbl, nent);
	if (rc != 0) {
		vhtbl_setup(&vcache->vhtbl, 0);
		return rc;
	}
	return 0;
}

static int islinked(const fnx_vnode_t *vnode)
{
	return fnx_link_islinked(&vnode->v_clink);
}

static void vcache_belru_vnode(fnx_vcache_t *vcache, fnx_vnode_t *vnode)
{
	transfer(&vnode->v_clink, NULL, &vcache->vlru);
}

static void vcache_relru_vnode(fnx_vcache_t *vcache, fnx_vnode_t *vnode)
{
	transfer(&vnode->v_clink, &vcache->vlru, &vcache->vlru);
}

static void vcache_unlru_vnode(fnx_vcache_t *vcache, fnx_vnode_t *vnode)
{
	transfer(&vnode->v_clink, &vcache->vlru, NULL);
}

void fnx_vcache_store_vnode(fnx_vcache_t *vcache, fnx_vnode_t *vnode)
{
	fnx_assert(!vnode->v_cached);
	fnx_assert(!islinked(vnode));

	vhtbl_insert(&vcache->vhtbl, vnode);
	vcache_belru_vnode(vcache, vnode);
	vnode->v_cached = FNX_TRUE;
}

fnx_vnode_t *fnx_vcache_search_vnode(fnx_vcache_t *vcache,
                                     const fnx_vaddr_t *vaddr)
{
	fnx_vnode_t *vnode;

	vnode = vhtbl_lookup(&vcache->vhtbl, vaddr);
	if (vnode != NULL) {
		fnx_assert(islinked(vnode));
		vcache_relru_vnode(vcache, vnode);
	}
	return vnode;
}

void fnx_vcache_evict_vnode(fnx_vcache_t *vcache, fnx_vnode_t *vnode)
{
	fnx_assert(vnode->v_cached);
	fnx_assert(islinked(vnode));

	vhtbl_remove(&vcache->vhtbl, vnode);
	vcache_unlru_vnode(vcache, vnode);
	vnode->v_cached = FNX_FALSE;
}

fnx_vnode_t *fnx_vcache_poplru_vnode(fnx_vcache_t *vcache)
{
	fnx_link_t  *lnk;
	fnx_vnode_t *vnode = NULL;

	lnk = fnx_list_front(&vcache->vlru);
	if (lnk != NULL) {
		vnode = clink_to_vnode(lnk);
		fnx_vcache_evict_vnode(vcache, vnode);
	}
	return vnode;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_vcache_remap_de(fnx_vcache_t *vcache, fnx_ino_t dino,
                         fnx_hash_t hash, fnx_size_t nlen, fnx_ino_t ino)
{
	if ((hash != 0) && (nlen != 0)) {
		dehtbl_remap(&vcache->dehtbl, dino, hash, nlen, ino);
	}
}

fnx_ino_t fnx_vcache_lookup_de(const fnx_vcache_t *vcache, fnx_ino_t dino,
                               fnx_hash_t hash, fnx_size_t nlen)
{
	const fnx_dentry_t *dent = NULL;

	if ((hash != 0) && (nlen != 0)) {
		dent = dehtbl_lookup(&vcache->dehtbl, dino, hash, nlen);
	}
	return (dent != NULL) ? dent->ino : FNX_INO_NULL;
}

void fnx_vcache_clear_des(fnx_vcache_t *vcache)
{
	dehtbl_clear(&vcache->dehtbl);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_cache_init(fnx_cache_t *cache)
{
	fnx_ucache_init(&cache->uc);
	fnx_vcache_init(&cache->vc);
}

void fnx_cache_destroy(fnx_cache_t *cache)
{
	fnx_ucache_destroy(&cache->uc);
	fnx_vcache_destroy(&cache->vc);
}

int fnx_cache_setup(fnx_cache_t *cache)
{
	int rc1, rc2;

	rc1 = fnx_ucache_setup(&cache->uc);
	rc2 = fnx_vcache_setup(&cache->vc);
	return (rc1 != 0) ? rc1 : rc2;
}



