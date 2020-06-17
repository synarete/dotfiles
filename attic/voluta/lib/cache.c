/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * This file is part of libvoluta
 *
 * Copyright (C) 2020 Shachar Sharon
 *
 * Libvoluta is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Libvoluta is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 */
#define _GNU_SOURCE 1
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include "libvoluta.h"

/* Local functions forward declarations */
static void bki_init(struct voluta_bk_info *bki, union voluta_bk *bk);
static void bki_fini(struct voluta_bk_info *bki);
static void bki_incref(struct voluta_bk_info *bki);
static void bki_decref(struct voluta_bk_info *bki);
static void vi_init(struct voluta_vnode_info *vi);
static void vi_fini(struct voluta_vnode_info *vi);
static void vi_incref(struct voluta_vnode_info *vi);
static void vi_decref(struct voluta_vnode_info *vi);
static void vi_undirtify(struct voluta_vnode_info *vi);
static void ii_init(struct voluta_inode_info *ii);
static void ii_fini(struct voluta_inode_info *ii);
static void ii_inc_nsubs(struct voluta_inode_info *ii);
static void ii_dec_nsubs(struct voluta_inode_info *ii);
static bool cache_evict_some(struct voluta_cache *cache);

typedef int (*voluta_cache_elem_fn)(struct voluta_cache_elem *, void *);

struct voluta_cache_ctx {
	struct voluta_cache *cache;
	struct voluta_bk_info *bki;
	struct voluta_vnode_info *vi;
	struct voluta_inode_info *ii;
	size_t limit;
	size_t count;
};

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void lh_init(struct voluta_list_head *lh)
{
	voluta_list_head_init(lh);
}

static void lh_fini(struct voluta_list_head *lh)
{
	voluta_assert_eq(lh, lh->next);
	voluta_list_head_destroy(lh);
}

static void an_init(struct voluta_avl_node *an)
{
	voluta_avl_node_init(an);
}

static void an_fini(struct voluta_avl_node *an)
{
	voluta_avl_node_fini(an);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* LBA/offset re-hashing functions */
static long rotate(long x, unsigned int b)
{
	return (x << b) | (x >> (64 - b));
}

static long twang_mix(long v)
{
	return (long)voluta_twang_mix64((uint64_t)v);
}

static long lba_hash(long lba)
{
	return twang_mix(lba);
}

static long off_hash(long off)
{
	return ~twang_mix(off);
}

static long ino_hash(long ino)
{
	return rotate(ino, (unsigned int)(ino % 31));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_list_head *
new_htbl(struct voluta_qalloc *qal, size_t nelems)
{
	int err;
	void *mem = NULL;
	struct voluta_list_head *htbl;

	err = voluta_nalloc(qal, sizeof(*htbl), nelems, &mem);
	if (err) {
		return NULL;
	}
	htbl = mem;
	list_head_initn(htbl, nelems);
	return htbl;
}

static void del_htbl(struct voluta_qalloc *qal,
		     struct voluta_list_head *htbl, size_t nelems)
{
	int err;

	list_head_destroyn(htbl, nelems);
	err = voluta_nfree(qal, htbl, sizeof(*htbl), nelems);
	voluta_assert_ok(err);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static union voluta_bk *malloc_bk(struct voluta_qalloc *qal)
{
	int err;
	void *mem = NULL;
	union voluta_bk *bk = NULL;

	err = voluta_malloc(qal, sizeof(*bk), &mem);
	if (likely(!err)) {
		bk = mem;
	}
	return bk;
}

static void free_bk(struct voluta_qalloc *qal, union voluta_bk *bk)
{
	int err;

	err = voluta_free(qal, bk, sizeof(*bk));
	voluta_assert_ok(err);
}

static struct voluta_bk_info *malloc_bki(struct voluta_mpool *mpool)
{
	return voluta_malloc_bki(mpool);
}

static void free_bki(struct voluta_mpool *mpool, struct voluta_bk_info *bki)
{
	voluta_free_bki(mpool, bki);
}

static struct voluta_vnode_info *malloc_vi(struct voluta_mpool *mpool)
{
	return voluta_malloc_vi(mpool);
}

static void free_vi(struct voluta_mpool *mpool, struct voluta_vnode_info *vi)
{
	voluta_free_vi(mpool, vi);
}

static struct voluta_inode_info *malloc_ii(struct voluta_mpool *mpool)
{
	return voluta_malloc_ii(mpool);
}

static void free_ii(struct voluta_mpool *mpool, struct voluta_inode_info *ii)
{
	voluta_free_ii(mpool, ii);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_cache_elem *
ce_from_htb_link(const struct voluta_list_head *lh)
{
	const struct voluta_cache_elem *ce;

	ce = container_of(lh, struct voluta_cache_elem, ce_htb_lh);
	return unconst(ce);
}

static struct voluta_cache_elem *
ce_from_lru_link(const struct voluta_list_head *lh)
{
	const struct voluta_cache_elem *ce;

	ce = container_of(lh, struct voluta_cache_elem, ce_lru_lh);
	return unconst(ce);
}

static void ce_init(struct voluta_cache_elem *ce)
{
	lh_init(&ce->ce_htb_lh);
	lh_init(&ce->ce_lru_lh);
	ce->ce_refcnt = 0;
}

static void ce_fini(struct voluta_cache_elem *ce)
{
	lh_fini(&ce->ce_htb_lh);
	lh_fini(&ce->ce_lru_lh);
	ce->ce_refcnt = 0;
}

static void ce_hmap(struct voluta_cache_elem *ce,
		    struct voluta_list_head *hlst)
{
	list_push_front(hlst, &ce->ce_htb_lh);
}

static void ce_hunmap(struct voluta_cache_elem *ce)
{
	list_head_remove(&ce->ce_htb_lh);
}

static struct voluta_list_head *ce_lru_link(struct voluta_cache_elem *ce)
{
	return &ce->ce_lru_lh;
}

static void ce_lru(struct voluta_cache_elem *ce, struct voluta_list_head *lru)
{
	list_push_front(lru, ce_lru_link(ce));
}

static void ce_unlru(struct voluta_cache_elem *ce)
{
	list_head_remove(ce_lru_link(ce));
}

static bool ce_islru_front(struct voluta_cache_elem *ce,
			   struct voluta_list_head *lru)
{
	return (list_front(lru) == ce_lru_link(ce));
}

static void ce_relru(struct voluta_cache_elem *ce,
		     struct voluta_list_head *lru)
{
	if (!ce_islru_front(ce, lru)) {
		ce_unlru(ce);
		ce_lru(ce, lru);
	}
}

static size_t ce_refcnt(const struct voluta_cache_elem *ce)
{
	return ce->ce_refcnt;
}

static void ce_incref(struct voluta_cache_elem *ce)
{
	voluta_assert_lt(ce->ce_refcnt, UINT_MAX / 2);
	ce->ce_refcnt++;
}

static void ce_decref(struct voluta_cache_elem *ce)
{
	voluta_assert_gt(ce->ce_refcnt, 0);
	ce->ce_refcnt--;
}

static bool ce_is_evictable(const struct voluta_cache_elem *ce)
{
	return !ce->ce_refcnt;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_vnode_info *dq_lh_to_vi(struct voluta_list_head *dq_lh)
{
	return container_of(dq_lh, struct voluta_vnode_info, v_dq_lh);
}

static void dirtyq_init(struct voluta_dirtyq *dq)
{
	voluta_listq_init(&dq->lq);
}

static void dirtyq_fini(struct voluta_dirtyq *dq)
{
	voluta_listq_fini(&dq->lq);
}

static void dirtyq_enq(struct voluta_dirtyq *dq,
		       struct voluta_vnode_info *vi)
{
	if (!vi->v_dirty) {
		voluta_listq_push_back(&dq->lq, &vi->v_dq_lh);
		vi->v_dirty = 1;
	}
}

static void dirtyq_deq(struct voluta_dirtyq *dq,
		       struct voluta_vnode_info *vi)
{
	if (vi->v_dirty) {
		voluta_assert_gt(dq->lq.sz, 0);
		voluta_listq_remove(&dq->lq, &vi->v_dq_lh);
		vi->v_dirty = 0;
	}
}

static struct voluta_vnode_info *dirtyq_front(const struct voluta_dirtyq *dq)
{
	struct voluta_list_head *lh;
	struct voluta_vnode_info *vi = NULL;

	lh = voluta_listq_front(&dq->lq);
	if (lh != NULL) {
		vi = dq_lh_to_vi(lh);
	}
	return vi;
}

static struct voluta_vnode_info *
dirtyq_next(const struct voluta_dirtyq *dq, const struct voluta_vnode_info *vi)
{
	struct voluta_list_head *lh;
	struct voluta_vnode_info *vi_next = NULL;

	lh = voluta_listq_next(&dq->lq, &vi->v_dq_lh);
	if (lh != NULL) {
		vi_next = dq_lh_to_vi(lh);
	}
	return vi_next;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_vnode_info *vi_from_ce(const struct voluta_cache_elem *ce)
{
	const struct voluta_vnode_info *vi = NULL;

	if (ce != NULL) {
		vi = container_of(ce, struct voluta_vnode_info, v_ce);
	}
	return unconst(vi);
}

static struct voluta_cache_elem *vi_ce(const struct voluta_vnode_info *vi)
{
	return (struct voluta_cache_elem *)(&vi->v_ce);
}

static void vi_init(struct voluta_vnode_info *vi)
{
	vaddr_reset(&vi->vaddr);
	ce_init(vi_ce(vi));
	lh_init(&vi->v_dq_lh);
	an_init(&vi->v_dt_an);
	vi->view = NULL;
	vi->v_sbi = NULL;
	vi->v_bki = NULL;
	vi->v_pvi = NULL;
	vi->v_pii = NULL;
	vi->u.p = NULL;
	vi->v_dirty = 0;
	vi->v_solid = 0;
}

static void vi_fini(struct voluta_vnode_info *vi)
{
	vaddr_reset(&vi->vaddr);
	ce_fini(vi_ce(vi));
	lh_fini(&vi->v_dq_lh);
	an_fini(&vi->v_dt_an);
	vi->view = NULL;
	vi->v_sbi = NULL;
	vi->v_bki = NULL;
	vi->v_pvi = NULL;
	vi->v_pii = NULL;
	vi->u.p = NULL;
	vi->v_dirty = -11;
	vi->v_solid = 0;
}

static void vi_assign(struct voluta_vnode_info *vi,
		      const struct voluta_vaddr *vaddr)
{
	vaddr_clone(vaddr, &vi->vaddr);
}

static size_t vi_refcnt(const struct voluta_vnode_info *vi)
{
	return ce_refcnt(vi_ce(vi));
}

static void vi_incref(struct voluta_vnode_info *vi)
{
	ce_incref(vi_ce(vi));
}

static void vi_decref(struct voluta_vnode_info *vi)
{
	ce_decref(vi_ce(vi));
}

static void vi_attach_bk(struct voluta_vnode_info *vi,
			 struct voluta_bk_info *bki)
{
	voluta_assert_null(vi->v_bki);

	bki_incref(bki);
	vi->v_bki = bki;
}

static void vi_detach_bk(struct voluta_vnode_info *vi)
{
	if (vi->v_bki != NULL) {
		bki_decref(vi->v_bki);
		vi->v_bki = NULL;
		vi->view = NULL;
		vi->u.p = NULL;
	}
}

static void vi_attach_pvi(struct voluta_vnode_info *vi,
			  struct voluta_vnode_info *pvi)
{
	voluta_assert_null(vi->v_pvi);

	if (pvi != NULL) {
		vi->v_pvi = pvi;
		vi_incref(pvi);
	}
}

static void vi_detach_pvi(struct voluta_vnode_info *vi)
{

	if (vi->v_pvi != NULL) {
		vi_decref(vi->v_pvi);
		vi->v_pvi = NULL;
	}
}


static void vi_attach_pii(struct voluta_vnode_info *vi,
			  struct voluta_inode_info *pii)
{
	if (pii != NULL) {
		vi->v_pii = pii;
		ii_inc_nsubs(pii);
	}
}

static void vi_detach_pii(struct voluta_vnode_info *vi)
{
	if (vi->v_pii != NULL) {
		ii_dec_nsubs(vi->v_pii);
		vi->v_pii = NULL;
	}
}

static void vi_detach_all(struct voluta_vnode_info *vi)
{
	vi_detach_pii(vi);
	vi_detach_pvi(vi);
	vi_detach_bk(vi);
}

static bool vi_is_evictable(const struct voluta_vnode_info *vi)
{
	voluta_assert_ne(vi->vaddr.vtype, VOLUTA_VTYPE_NONE);

	return !vi->v_dirty && ce_is_evictable(vi_ce(vi));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_inode_info *ii_from_vi(const struct voluta_vnode_info *vi)
{
	const struct voluta_inode_info *ii = NULL;

	if (vi != NULL) {
		ii = container_of(vi, struct voluta_inode_info, i_vi);
	}
	return (struct voluta_inode_info *)ii;
}

static struct voluta_inode_info *ii_from_ce(const struct voluta_cache_elem *ce)
{
	return ii_from_vi(vi_from_ce(ce));
}

static void ii_init(struct voluta_inode_info *ii)
{
	vi_init(&ii->i_vi);
	dirtyq_init(&ii->i_vdq);
	ii->inode = NULL;
	ii->i_ino = VOLUTA_INO_NULL;
	ii->i_nsubs = 0;
	ii->i_nopen = 0;
}

static void ii_fini(struct voluta_inode_info *ii)
{
	voluta_assert_eq(ii->i_nsubs, 0);
	voluta_assert_ge(ii->i_nopen, 0);

	vi_fini(&ii->i_vi);
	dirtyq_fini(&ii->i_vdq);
	ii->inode = NULL;
	ii->i_ino = VOLUTA_INO_NULL;
	ii->i_nsubs = 0;
	ii->i_nopen = INT_MIN;
}

static void ii_assign(struct voluta_inode_info *ii,
		      const struct voluta_iaddr *iaddr)
{
	vi_assign(&ii->i_vi, &iaddr->vaddr);
	ii->i_ino = iaddr->ino;
}

static bool ii_is_evictable(const struct voluta_inode_info *ii)
{
	bool ret = false;

	voluta_assert_eq(ii->i_vi.vaddr.vtype, VOLUTA_VTYPE_INODE);

	if (!ii->i_nopen && !ii->i_nsubs) {
		ret = vi_is_evictable(i_vi(ii));
	}
	return ret;
}

static void ii_inc_nsubs(struct voluta_inode_info *ii)
{
	ii->i_nsubs++;
}

static void ii_dec_nsubs(struct voluta_inode_info *ii)
{
	voluta_assert_gt(ii->i_nsubs, 0);

	ii->i_nsubs--;
}

static void ii_dirtify_vi(struct voluta_inode_info *ii,
			  struct voluta_vnode_info *vi)
{
	dirtyq_enq(&ii->i_vdq, vi);
}

static void ii_undirtify_vi(struct voluta_inode_info *ii,
			    struct voluta_vnode_info *vi)
{
	dirtyq_deq(&ii->i_vdq, vi);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_bk_info *bki_from_ce(const struct voluta_cache_elem *ce)
{
	const struct voluta_bk_info *bki = NULL;

	if (ce != NULL) {
		bki = container_of(ce, struct voluta_bk_info, b_ce);
	}
	return unconst(bki);
}

static struct voluta_cache_elem *bki_ce(const struct voluta_bk_info *bki)
{
	return (struct voluta_cache_elem *)(&bki->b_ce);
}

static void bki_set_lba(struct voluta_bk_info *bki, loff_t lba)
{
	bki->b_lba = lba;
}

static void bki_init(struct voluta_bk_info *bki, union voluta_bk *bk)
{
	ce_init(&bki->b_ce);
	bki_set_lba(bki, VOLUTA_LBA_NULL);
	bki->b_mask = 0;
	bki->bk = bk;
}

static void bki_fini(struct voluta_bk_info *bki)
{
	ce_fini(&bki->b_ce);
	bki_set_lba(bki, VOLUTA_LBA_NULL);
	bki->bk = NULL;
}

static void bki_incref(struct voluta_bk_info *bki)
{
	ce_incref(bki_ce(bki));
}

static void bki_decref(struct voluta_bk_info *bki)
{
	ce_decref(bki_ce(bki));
}

static bool bki_is_evictable(const struct voluta_bk_info *bki)
{
	return ce_is_evictable(bki_ce(bki));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t vaddr_nkbs(const struct voluta_vaddr *vaddr)
{
	return voluta_nkbs_of(vaddr);
}

static size_t vaddr_kb_index(const struct voluta_vaddr *vaddr)
{
	const loff_t kb_size = VOLUTA_KB_SIZE;
	const size_t nkb_in_bk = VOLUTA_NKB_IN_BK;

	return (size_t)(vaddr->off / kb_size) % nkb_in_bk;
}

static uint64_t view_mask_of(const struct voluta_vaddr *vaddr)
{
	uint64_t kb_mask;
	const size_t nkbs = vaddr_nkbs(vaddr);
	const size_t kidx = vaddr_kb_index(vaddr);

	kb_mask = (nkbs < 64) ? ((1UL << nkbs) - 1) : ~0UL;
	return kb_mask << kidx;
}

static void bki_mark_visible(struct voluta_bk_info *bki,
			     const struct voluta_vaddr *vaddr)
{
	bki->b_mask |= view_mask_of(vaddr);
}

static void bki_mark_opaque(struct voluta_bk_info *bki,
			    const struct voluta_vaddr *vaddr)
{
	bki->b_mask &= ~view_mask_of(vaddr);
}

static bool bki_is_visible(struct voluta_bk_info *bki,
			   const struct voluta_vaddr *vaddr)
{
	const uint64_t mask = view_mask_of(vaddr);

	return ((bki->b_mask & mask) == mask);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int lrumap_init(struct voluta_lrumap *lm, struct voluta_qalloc *qal,
		       size_t htbl_size, voulta_lhash_fn hash_fn)
{
	struct voluta_list_head *htbl;

	htbl = new_htbl(qal, htbl_size);
	if (htbl == NULL) {
		return -ENOMEM;
	}
	list_init(&lm->lru);
	lm->htbl = htbl;
	lm->hash_fn = hash_fn;
	lm->hsize = htbl_size;
	lm->count = 0;
	return 0;
}

static void lrumap_fini(struct voluta_lrumap *lm, struct voluta_qalloc *qal)
{
	del_htbl(qal, lm->htbl, lm->hsize);
	list_fini(&lm->lru);
	lm->htbl = NULL;
	lm->hash_fn = NULL;
	lm->hsize = 0;
}

static size_t lrumap_key_to_bin(const struct voluta_lrumap *lm, long ckey)
{
	return (size_t)(lm->hash_fn(ckey)) % lm->hsize;
}

static void lrumap_store(struct voluta_lrumap *lm,
			 struct voluta_cache_elem *ce, long ckey)
{
	const size_t bin = lrumap_key_to_bin(lm, ckey);

	ce->ce_key = ckey;
	ce_hmap(ce, &lm->htbl[bin]);
	ce_lru(ce, &lm->lru);
	lm->count += 1;
}

static struct voluta_cache_elem *
lrumap_find(const struct voluta_lrumap *lm, long ckey)
{
	size_t bin;
	const struct voluta_list_head *lst;
	const struct voluta_list_head *itr;
	const struct voluta_cache_elem *ce;

	bin = lrumap_key_to_bin(lm, ckey);
	lst = &lm->htbl[bin];
	itr = lst->next;
	while (itr != lst) {
		ce = ce_from_htb_link(itr);
		if (ce->ce_key == ckey) {
			return unconst(ce);
		}
		itr = itr->next;
	}
	return NULL;
}

static void lrumap_remove(struct voluta_lrumap *lm,
			  struct voluta_cache_elem *ce)
{
	ce_hunmap(ce);
	ce_unlru(ce);
	lm->count -= 1;
}

static void lrumap_promote_lru(struct voluta_lrumap *lm,
			       struct voluta_cache_elem *ce)
{
	ce_relru(ce, &lm->lru);
}

static struct voluta_cache_elem *
lrumap_get_lru(const struct voluta_lrumap *cq)
{
	struct voluta_cache_elem *ce = NULL;

	if (cq->count > 0) {
		ce = ce_from_lru_link(cq->lru.prev);
	}
	return ce;
}

static void lrumap_foreach_backward(struct voluta_lrumap *lm,
				    voluta_cache_elem_fn cb, void *arg)
{
	int ret = 0;
	size_t count;
	struct voluta_cache_elem *ce;
	struct voluta_list_head *lru = &lm->lru;
	struct voluta_list_head *itr = lru->prev;

	count = lm->count;
	while (!ret && count-- && (itr != lru)) {
		ce = ce_from_lru_link(itr);
		itr = itr->prev;
		ret = cb(ce, arg);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void cache_dirtify_vi(struct voluta_cache *cache,
			     struct voluta_vnode_info *vi)
{
	dirtyq_enq(&cache->c_vdq, vi);
}

static void cache_undirtify_vi(struct voluta_cache *cache,
			       struct voluta_vnode_info *vi)
{
	dirtyq_deq(&cache->c_vdq, vi);
}

static void cache_dirtify_ii(struct voluta_cache *cache,
			     struct voluta_inode_info *ii)
{
	cache_dirtify_vi(cache, &ii->i_vi);
}

static void cache_undirtify_ii(struct voluta_cache *cache,
			       struct voluta_inode_info *ii)
{
	cache_undirtify_vi(cache, &ii->i_vi);
}

static struct voluta_bk_info *cache_new_bk(const struct voluta_cache *cache)
{
	union voluta_bk *bk;
	struct voluta_bk_info *bki;

	bk = malloc_bk(cache->c_qalloc);
	if (bk == NULL) {
		return NULL;
	}
	bki = malloc_bki(cache->c_mpool);
	if (bki == NULL) {
		free_bk(cache->c_qalloc, bk);
		return NULL;
	}
	bki_init(bki, bk);
	return bki;
}

static void cache_del_bk(const struct voluta_cache *cache,
			 struct voluta_bk_info *bki)
{
	union voluta_bk *bk = bki->bk;

	bki_fini(bki);
	free_bk(cache->c_qalloc, bk);
	free_bki(cache->c_mpool, bki);
}

static int cache_init_blm(struct voluta_cache *cache, size_t htbl_size)
{
	return lrumap_init(&cache->c_blm, cache->c_qalloc,
			   htbl_size, lba_hash);
}

static void cache_fini_blm(struct voluta_cache *cache)
{
	lrumap_fini(&cache->c_blm, cache->c_qalloc);
}

static struct voluta_bk_info *
cache_find_bk(const struct voluta_cache *cache, loff_t lba)
{
	struct voluta_cache_elem *ce;

	ce = lrumap_find(&cache->c_blm, lba);
	return bki_from_ce(ce);
}

static void cache_store_bk(struct voluta_cache *cache,
			   struct voluta_bk_info *bki, loff_t lba)
{
	bki_set_lba(bki, lba);
	lrumap_store(&cache->c_blm, &bki->b_ce, lba);
}

static void cache_promote_lru_bk(struct voluta_cache *cache,
				 struct voluta_bk_info *bki)
{
	lrumap_promote_lru(&cache->c_blm, &bki->b_ce);
}

static void cache_evict_bk(struct voluta_cache *cache,
			   struct voluta_bk_info *bki)
{
	voluta_assert(ce_is_evictable(bki_ce(bki)));

	lrumap_remove(&cache->c_blm, &bki->b_ce);
	cache_del_bk(cache, bki);
}

void voluta_cache_forget_bk(struct voluta_cache *cache,
			    struct voluta_bk_info *bki)
{
	voluta_assert_eq(bki->b_ce.ce_refcnt, 0);

	cache_evict_bk(cache, bki);
}

static struct voluta_bk_info *
cache_spawn_bk(struct voluta_cache *cache, loff_t lba)
{
	struct voluta_bk_info *bki;

	bki = cache_new_bk(cache);
	if (bki == NULL) {
		return NULL;
	}
	cache_store_bk(cache, bki, lba);
	return bki;
}

static struct voluta_bk_info *
cache_find_relru_bk(struct voluta_cache *cache, loff_t lba)
{
	struct voluta_bk_info *bki;

	bki = cache_find_bk(cache, lba);
	if (bki == NULL) {
		return NULL;
	}
	cache_promote_lru_bk(cache, bki);
	return bki;
}

struct voluta_bk_info *
voluta_cache_lookup_bk(struct voluta_cache *cache, loff_t lba)
{
	struct voluta_bk_info *bki = NULL;

	if (lba != VOLUTA_LBA_NULL) {
		bki = cache_find_relru_bk(cache, lba);
	}
	return bki;
}

static struct voluta_bk_info *
cache_find_or_spawn_bk(struct voluta_cache *cache, loff_t lba)
{
	struct voluta_bk_info *bki;

	bki = cache_find_relru_bk(cache, lba);
	if (bki != NULL) {
		return bki;
	}
	bki = cache_spawn_bk(cache, lba);
	if (bki == NULL) {
		return NULL; /* TODO: debug-trace */
	}
	return bki;
}

static int visit_evictable_bk(struct voluta_cache_elem *ce, void *arg)
{
	int ret = 0;
	struct voluta_cache_ctx *c_ctx = arg;
	struct voluta_bk_info *bki = bki_from_ce(ce);

	if (c_ctx->count++ >= c_ctx->limit) {
		ret = 1;
	} else if (bki_is_evictable(bki)) {
		c_ctx->bki = bki;
		ret = 1;
	}
	return ret;
}

static size_t calc_search_evictable_max(const struct voluta_lrumap *lm)
{
	return clamp(lm->count / 4, 1, 256);
}

static struct voluta_bk_info *
cache_find_evictable_bk(struct voluta_cache *cache)
{
	struct voluta_cache_ctx c_ctx = {
		.cache = cache,
		.bki = NULL,
		.limit = calc_search_evictable_max(&cache->c_blm)
	};

	lrumap_foreach_backward(&cache->c_blm, visit_evictable_bk, &c_ctx);
	return c_ctx.bki;
}

static struct voluta_bk_info *
cache_require_bk(struct voluta_cache *cache, loff_t lba)
{
	struct voluta_bk_info *bki = NULL;

	bki = cache_find_or_spawn_bk(cache, lba);
	if (bki != NULL) {
		return bki;
	}
	if (!cache_evict_some(cache)) {
		return NULL;
	}
	bki = cache_find_or_spawn_bk(cache, lba);
	if (bki != NULL) {
		return bki;
	}
	return NULL;
}

struct voluta_bk_info *
voluta_cache_spawn_bk(struct voluta_cache *cache, loff_t lba)
{
	return cache_require_bk(cache, lba);
}

static struct voluta_bk_info *cache_get_lru_bk(struct voluta_cache *cache)
{
	struct voluta_cache_elem *ce;

	ce = lrumap_get_lru(&cache->c_blm);
	return bki_from_ce(ce);
}

static void cache_try_evict_bk(struct voluta_cache *cache,
			       struct voluta_bk_info *bki)
{
	voluta_assert_not_null(bki);

	if (bki_is_evictable(bki)) {
		cache_evict_bk(cache, bki);
	}
}

static int try_evict_bk(struct voluta_cache_elem *ce, void *arg)
{
	struct voluta_cache_ctx *c_ctx = arg;
	struct voluta_bk_info *bki = bki_from_ce(ce);

	cache_try_evict_bk(c_ctx->cache, bki);
	return 0;
}

static void cache_drop_evictable_bks(struct voluta_cache *cache)
{
	struct voluta_cache_ctx c_ctx = {
		.cache = cache
	};

	lrumap_foreach_backward(&cache->c_blm, try_evict_bk, &c_ctx);
}

static void cache_evict_or_relru_bk(struct voluta_cache *cache,
				    struct voluta_bk_info *bki)
{
	if (bki_is_evictable(bki)) {
		cache_evict_bk(cache, bki);
	} else {
		cache_promote_lru_bk(cache, bki);
	}
}

static void cache_shrink_or_relru_bks(struct voluta_cache *cache, size_t count)
{
	struct voluta_bk_info *bki;
	const size_t n = min(count, cache->c_blm.count);

	for (size_t i = 0; i < n; ++i) {
		bki = cache_get_lru_bk(cache);
		if (bki == NULL) {
			break;
		}
		cache_evict_or_relru_bk(cache, bki);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_cache *v_cache_of(const struct voluta_vnode_info *vi)
{
	voluta_assert_not_null(vi->v_sbi);

	return vi->v_sbi->s_cache;
}

static struct voluta_vnode_info *cache_new_vi(const struct voluta_cache *cache)
{
	struct voluta_vnode_info *vi;

	vi = malloc_vi(cache->c_mpool);
	if (vi != NULL) {
		vi_init(vi);
	}
	return vi;
}

static void cache_del_vi(const struct voluta_cache *cache,
			 struct voluta_vnode_info *vi)
{
	vi_fini(vi);
	free_vi(cache->c_mpool, vi);
}

static int cache_init_vlm(struct voluta_cache *cache, size_t htbl_size)
{
	return lrumap_init(&cache->c_vlm, cache->c_qalloc,
			   htbl_size, off_hash);
}

static void cache_fini_vlm(struct voluta_cache *cache)
{
	lrumap_fini(&cache->c_vlm, cache->c_qalloc);
}

static struct voluta_vnode_info *
cache_find_vi(struct voluta_cache *cache, const struct voluta_vaddr *vaddr)
{
	struct voluta_cache_elem *ce;

	ce = lrumap_find(&cache->c_vlm, vaddr->off);
	return vi_from_ce(ce);
}

static void cache_evict_vi(struct voluta_cache *cache,
			   struct voluta_vnode_info *vi)
{
	/* TODO: remove me */
	cache_undirtify_vi(cache, vi);

	lrumap_remove(&cache->c_vlm, vi_ce(vi));
	vi_detach_all(vi);
	cache_del_vi(cache, vi);
}

static void cache_promote_lru_vi(struct voluta_cache *cache,
				 struct voluta_vnode_info *vi)
{
	lrumap_promote_lru(&cache->c_vlm, vi_ce(vi));
}

static struct voluta_vnode_info *
cache_lookup_vi(struct voluta_cache *cache, const struct voluta_vaddr *vaddr)
{
	struct voluta_vnode_info *vi;

	vi = cache_find_vi(cache, vaddr);
	if (vi == NULL) {
		return NULL;
	}
	cache_promote_lru_vi(cache, vi);
	cache_promote_lru_bk(cache, vi->v_bki);
	return vi;
}

struct voluta_vnode_info *
voluta_cache_lookup_vi(struct voluta_cache *cache,
		       const struct voluta_vaddr *vaddr)
{
	return cache_lookup_vi(cache, vaddr);
}

static void cache_store_vi(struct voluta_cache *cache,
			   struct voluta_vnode_info *vi,
			   const struct voluta_vaddr *vaddr)
{
	vi_assign(vi, vaddr);
	lrumap_store(&cache->c_vlm, &vi->v_ce, vaddr->off);
}

static int visit_evictable_vi(struct voluta_cache_elem *ce, void *arg)
{
	int ret = 0;
	struct voluta_cache_ctx *c_ctx = arg;
	struct voluta_vnode_info *vi = vi_from_ce(ce);

	if (c_ctx->count++ >= c_ctx->limit) {
		ret = 1;
	} else if (vi_is_evictable(vi)) {
		c_ctx->vi = vi;
		ret = 1;
	}
	return ret;
}

static struct voluta_vnode_info *
cache_find_evictable_vi(struct voluta_cache *cache)
{
	struct voluta_cache_ctx c_ctx = {
		.cache = cache,
		.vi = NULL,
		.limit = calc_search_evictable_max(&cache->c_vlm)
	};

	lrumap_foreach_backward(&cache->c_vlm, visit_evictable_vi, &c_ctx);
	return c_ctx.vi;
}

static struct voluta_vnode_info *
cache_spawn_vi(struct voluta_cache *cache, const struct voluta_vaddr *vaddr)
{
	struct voluta_vnode_info *vi;

	vi = cache_new_vi(cache);
	if (vi == NULL) {
		return NULL;
	}
	cache_store_vi(cache, vi, vaddr);
	return vi;
}

static struct voluta_vnode_info *
cache_require_vi(struct voluta_cache *cache, const struct voluta_vaddr *vaddr)
{
	struct voluta_vnode_info *vi = NULL;

	vi = cache_spawn_vi(cache, vaddr);
	if (vi != NULL) {
		return vi;
	}
	if (!cache_evict_some(cache)) {
		return NULL;
	}
	vi = cache_spawn_vi(cache, vaddr);
	if (vi != NULL) {
		return vi;
	}
	return NULL;
}

struct voluta_vnode_info *
voluta_cache_spawn_vi(struct voluta_cache *cache,
		      const struct voluta_vaddr *vaddr)
{
	return cache_require_vi(cache, vaddr);
}

void voulta_cache_forget_vi(struct voluta_cache *cache,
			    struct voluta_vnode_info *vi)
{
	voluta_assert_eq(vi_refcnt(vi), 0);

	vi_undirtify(vi);
	cache_evict_vi(cache, vi);
}

void voluta_attach_to(struct voluta_vnode_info *vi,
		      struct voluta_bk_info *bki,
		      struct voluta_vnode_info *pvi,
		      struct voluta_inode_info *pii)
{
	voluta_assert_null(vi->v_bki);

	vi_attach_bk(vi, bki);
	vi_attach_pvi(vi, pvi);
	vi_attach_pii(vi, pii);
}

static struct voluta_vnode_info *cache_get_lru_vi(struct voluta_cache *cache)
{
	struct voluta_vnode_info *vi = NULL;
	const struct voluta_cache_elem *ce;

	ce = lrumap_get_lru(&cache->c_vlm);
	if (ce != NULL) {
		vi = vi_from_ce(ce);
	}
	return vi;
}

static void cache_evict_or_relru_vi(struct voluta_cache *cache,
				    struct voluta_vnode_info *vi)
{
	if (vi_is_evictable(vi)) {
		cache_evict_vi(cache, vi);
	} else {
		cache_promote_lru_vi(cache, vi);
	}
}

static int try_evict_vi(struct voluta_cache_elem *ce, void *arg)
{
	struct voluta_cache *cache = arg;
	struct voluta_vnode_info *vi = vi_from_ce(ce);

	voluta_assert_ne(vi->vaddr.vtype, VOLUTA_VTYPE_NONE);

	cache_evict_or_relru_vi(cache, vi);
	return 0;
}

static void cache_drop_evictable_vis(struct voluta_cache *cache)
{
	lrumap_foreach_backward(&cache->c_vlm, try_evict_vi, cache);
}

static void cache_shrink_or_relru_vis(struct voluta_cache *cache, size_t count)
{
	struct voluta_vnode_info *vi;
	const size_t n = min(count, cache->c_vlm.count);

	for (size_t i = 0; i < n; ++i) {
		vi = cache_get_lru_vi(cache);
		if (vi == NULL) {
			break;
		}
		cache_evict_or_relru_vi(cache, vi);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_cache *cache_of_ii(const struct voluta_inode_info *ii)
{
	return v_cache_of(&ii->i_vi);
}

static struct voluta_inode_info *cache_new_ii(const struct voluta_cache *cache)
{
	struct voluta_inode_info *ii;

	ii = malloc_ii(cache->c_mpool);
	if (ii != NULL) {
		ii_init(ii);
	}
	return ii;
}

static void cache_del_ii(const struct voluta_cache *cache,
			 struct voluta_inode_info *ii)
{
	ii_fini(ii);
	free_ii(cache->c_mpool, ii);
}

static int cache_init_ilm(struct voluta_cache *cache, size_t htbl_size)
{
	return lrumap_init(&cache->c_ilm, cache->c_qalloc,
			   htbl_size, ino_hash);
}

static void cache_fini_ilm(struct voluta_cache *cache)
{
	lrumap_fini(&cache->c_ilm, cache->c_qalloc);
}

static struct voluta_inode_info *
cache_find_ii(struct voluta_cache *cache, ino_t ino)
{
	struct voluta_cache_elem *ce;

	ce = lrumap_find(&cache->c_ilm, (long)ino);
	return ii_from_ce(ce);
}

static void cache_evict_ii(struct voluta_cache *cache,
			   struct voluta_inode_info *ii)
{
	struct voluta_vnode_info *vi = i_vi(ii);

	lrumap_remove(&cache->c_ilm, vi_ce(vi));
	vi_detach_all(vi);
	cache_del_ii(cache, ii);
}

static void cache_promote_lru_ii(struct voluta_cache *cache,
				 struct voluta_inode_info *ii)
{
	lrumap_promote_lru(&cache->c_ilm, vi_ce(&ii->i_vi));
}

static struct voluta_inode_info *
cache_lookup_ii(struct voluta_cache *cache, ino_t ino)
{
	struct voluta_inode_info *ii;

	ii = cache_find_ii(cache, ino);
	if (ii == NULL) {
		return NULL;
	}
	cache_promote_lru_ii(cache, ii);
	cache_promote_lru_bk(cache, ii->i_vi.v_bki);
	return ii;
}

struct voluta_inode_info *
voluta_cache_lookup_ii(struct voluta_cache *cache, ino_t ino)
{
	return cache_lookup_ii(cache, ino);
}

static void cache_store_ii(struct voluta_cache *cache,
			   struct voluta_inode_info *ii,
			   const struct voluta_iaddr *iaddr)
{
	ii_assign(ii, iaddr);
	lrumap_store(&cache->c_ilm, vi_ce(&ii->i_vi), (long)(ii->i_ino));
}

static int visit_evictable_ii(struct voluta_cache_elem *ce, void *arg)
{
	int ret = 0;
	struct voluta_cache_ctx *c_ctx = arg;
	struct voluta_inode_info *ii = ii_from_ce(ce);

	if (c_ctx->count++ >= c_ctx->limit) {
		ret = 1;
	} else if (ii_is_evictable(ii)) {
		c_ctx->ii = ii;
		ret = 1;
	}
	return ret;
}

static struct voluta_inode_info *
cache_find_evictable_ii(struct voluta_cache *cache)
{
	struct voluta_cache_ctx c_ctx = {
		.cache = cache,
		.ii = NULL,
		.limit = calc_search_evictable_max(&cache->c_ilm)
	};

	lrumap_foreach_backward(&cache->c_ilm, visit_evictable_ii, &c_ctx);
	return c_ctx.ii;
}

static struct voluta_inode_info *
cache_spawn_ii(struct voluta_cache *cache, const struct voluta_iaddr *iaddr)
{
	struct voluta_inode_info *ii;

	ii = cache_new_ii(cache);
	if (ii == NULL) {
		return NULL;
	}
	cache_store_ii(cache, ii, iaddr);
	return ii;
}

static struct voluta_inode_info *
cache_require_ii(struct voluta_cache *cache, const struct voluta_iaddr *iaddr)
{
	struct voluta_inode_info *ii = NULL;

	ii = cache_spawn_ii(cache, iaddr);
	if (ii != NULL) {
		return ii;
	}
	if (!cache_evict_some(cache)) {
		return NULL;
	}
	ii = cache_spawn_ii(cache, iaddr);
	if (ii != NULL) {
		return ii;
	}
	return NULL;
}

struct voluta_inode_info *
voluta_cache_spawn_ii(struct voluta_cache *cache,
		      const struct voluta_iaddr *iaddr)
{
	return cache_require_ii(cache, iaddr);
}

void voulta_cache_forget_ii(struct voluta_cache *cache,
			    struct voluta_inode_info *ii)
{
	cache_undirtify_ii(cache, ii);
	cache_evict_ii(cache, ii);
}

static struct voluta_inode_info *cache_get_lru_ii(struct voluta_cache *cache)
{
	struct voluta_cache_elem *ce;

	ce = lrumap_get_lru(&cache->c_ilm);
	return ii_from_ce(ce);
}

static void cache_evict_or_relru_ii(struct voluta_cache *cache,
				    struct voluta_inode_info *ii)
{
	if (ii_is_evictable(ii)) {
		cache_evict_ii(cache, ii);
	} else {
		cache_promote_lru_ii(cache, ii);
	}
}

static int try_evict_ii(struct voluta_cache_elem *ce, void *arg)
{
	struct voluta_cache *cache = arg;
	struct voluta_inode_info *ii = ii_from_ce(ce);

	cache_evict_or_relru_ii(cache, ii);
	return 0;
}

static void cache_drop_evictable_iis(struct voluta_cache *cache)
{
	lrumap_foreach_backward(&cache->c_ilm, try_evict_ii, cache);
}

static void cache_shrink_or_relru_iis(struct voluta_cache *cache, size_t count)
{
	struct voluta_inode_info *ii;
	const size_t n = min(count, cache->c_ilm.count);

	for (size_t i = 0; i < n; ++i) {
		ii = cache_get_lru_ii(cache);
		if (ii == NULL) {
			break;
		}
		cache_evict_or_relru_ii(cache, ii);
	}
}

static void cache_shrink_some(struct voluta_cache *cache)
{
	cache_shrink_or_relru_vis(cache, VOLUTA_NBO_IN_BK);
	cache_shrink_or_relru_iis(cache, VOLUTA_NKB_IN_BK);
	cache_shrink_or_relru_bks(cache, 1);
}

void voluta_cache_relax(struct voluta_cache *cache, bool force)
{
	const struct voluta_qalloc *qal = cache->c_qalloc;

	if (force || ((qal->st.npages_used * 4) > qal->st.npages)) {
		cache_shrink_some(cache);
	}
}

void voluta_cache_drop(struct voluta_cache *cache)
{
	size_t niter = 8;

	/* TODO: avoid needless loops */
	while (niter--) {
		cache_drop_evictable_vis(cache);
		cache_drop_evictable_iis(cache);
		cache_drop_evictable_bks(cache);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool cache_try_evict(struct voluta_cache *cache,
			    struct voluta_vnode_info *vi,
			    struct voluta_vnode_info *ui,
			    struct voluta_inode_info *ii,
			    struct voluta_bk_info *bki)
{
	bool ret = false;

	if ((vi != NULL) && vi_is_evictable(vi)) {
		cache_evict_vi(cache, vi);
	}
	if ((ui != NULL) && vi_is_evictable(ui)) {
		cache_evict_vi(cache, ui);
	}
	if ((ii != NULL) && ii_is_evictable(ii)) {
		cache_evict_ii(cache, ii);
	}
	if ((bki != NULL) && bki_is_evictable(bki)) {
		cache_evict_bk(cache, bki);
		ret = true;
	}
	return ret;
}

static bool cache_evict_by_bk(struct voluta_cache *cache,
			      struct voluta_bk_info *bki)
{
	bool ret = false;

	if (bki != NULL) {
		ret = cache_try_evict(cache, NULL, NULL, NULL, bki);
	}
	return ret;
}

static bool cache_evict_by_ii(struct voluta_cache *cache,
			      struct voluta_inode_info *ii)
{
	bool ret = false;

	if (ii != NULL) {
		ret = cache_try_evict(cache, NULL, ii->i_vi.v_pvi,
				      ii, ii->i_vi.v_bki);
	}
	return ret;
}

static bool cache_evict_by_vi(struct voluta_cache *cache,
			      struct voluta_vnode_info *vi)
{
	bool ret = false;
	bool ret_pii = false;
	struct voluta_inode_info *pii;

	if (vi != NULL) {
		pii = vi->v_pii;
		ret = cache_try_evict(cache, vi, vi->v_pvi, pii, vi->v_bki);
		ret_pii = cache_evict_by_ii(cache, pii);
	}
	return ret || ret_pii;
}

static bool cache_evict_some(struct voluta_cache *cache)
{
	bool ret = false;
	struct voluta_bk_info *bki;
	struct voluta_vnode_info *vi;
	struct voluta_inode_info *ii;

	vi = cache_find_evictable_vi(cache);
	if (cache_evict_by_vi(cache, vi)) {
		ret = true;
	}
	ii = cache_find_evictable_ii(cache);
	if (cache_evict_by_ii(cache, ii)) {
		ret = true;
	}
	bki = cache_find_evictable_bk(cache);
	if (cache_evict_by_bk(cache, bki)) {
		ret = true;
	}
	return ret;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int cache_init_null_bk(struct voluta_cache *cache)
{
	int err;
	void *mem;

	err = voluta_zalloc(cache->c_qalloc, sizeof(*cache->c_null_bk), &mem);
	if (!err) {
		cache->c_null_bk = mem;
	}
	return err;
}

static void cache_fini_null_bk(struct voluta_cache *cache)
{
	union voluta_bk *null_bk = cache->c_null_bk;

	if (null_bk != NULL) {
		voluta_free(cache->c_qalloc, null_bk, sizeof(*null_bk));
		cache->c_null_bk = NULL;
	}
}

static size_t calc_htbl_size(const struct voluta_qalloc *qal)
{
	const size_t bk_size = VOLUTA_BK_SIZE;
	const size_t want_size = (qal->st.memsz_data / bk_size) / 7;

	return clamp(want_size, 1U << 13, 1U << 23);
}

static int cache_init_lrumaps(struct voluta_cache *cache)
{
	int err;
	const size_t size = calc_htbl_size(cache->c_qalloc);

	err = cache_init_blm(cache, size);
	if (err) {
		/* TODO: cleanup */
		return err;
	}
	err = cache_init_ilm(cache, size);
	if (err) {
		/* TODO: cleanup */
		return err;
	}
	err = cache_init_vlm(cache, size);
	if (err) {
		/* TODO: cleanup */
		return err;
	}
	return 0;
}

static void cache_init_dirtyqs(struct voluta_cache *cache)
{
	dirtyq_init(&cache->c_vdq);
}

static void cache_fini_dirtyqs(struct voluta_cache *cache)
{
	dirtyq_fini(&cache->c_vdq);
}

int voluta_cache_init(struct voluta_cache *cache, struct voluta_mpool *mpool)
{
	int err;

	cache->c_mpool = mpool;
	cache->c_qalloc = mpool->mp_qal;
	cache_init_dirtyqs(cache);

	err = cache_init_null_bk(cache);
	if (err) {
		return err;
	}
	err = cache_init_lrumaps(cache);
	if (err) {
		return err;
	}
	return 0;
}

void voluta_cache_fini(struct voluta_cache *cache)
{
	cache_fini_vlm(cache);
	cache_fini_ilm(cache);
	cache_fini_blm(cache);
	cache_fini_null_bk(cache);
	cache_fini_dirtyqs(cache);
	cache->c_qalloc = NULL;
	cache->c_mpool = NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_dirtify_vi(struct voluta_vnode_info *vi)
{
	struct voluta_cache *cache = v_cache_of(vi);

	if (vi->v_pii != NULL) {
		ii_dirtify_vi(vi->v_pii, vi);
	} else {
		cache_dirtify_vi(cache, vi);
	}
}

static void vi_undirtify(struct voluta_vnode_info *vi)
{
	struct voluta_cache *cache = v_cache_of(vi);

	if (vi->v_pii != NULL) {
		ii_undirtify_vi(vi->v_pii, vi);
	} else {
		cache_undirtify_vi(cache, vi);
	}
}

void voluta_dirtify_ii(struct voluta_inode_info *ii)
{
	struct voluta_cache *cache = cache_of_ii(ii);

	cache_dirtify_ii(cache, ii);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool v_isinode(const struct voluta_vnode_info *vi)
{
	return vtype_isinode(vi->vaddr.vtype);
}

static bool v_isdata(const struct voluta_vnode_info *vi)
{
	return vtype_isdata(vi->vaddr.vtype);
}

static void vi_add_to_set(struct voluta_vnode_info *vi,
			  struct voluta_avl *avl, bool data_only)
{
	int err;
	struct voluta_avl_node *an = &vi->v_dt_an;

	if (!data_only || v_isdata(vi)) {
		err = voluta_avl_insert_unique(avl, an);
		voluta_assert_ok(err);
	}
}

static void ii_grab_dirty(const struct voluta_inode_info *ii,
			  struct voluta_avl *avl, bool data_only)
{
	struct voluta_vnode_info *vi;
	const struct voluta_dirtyq *dq = &ii->i_vdq;

	vi = dirtyq_front(dq);
	while (vi != NULL) {
		vi_add_to_set(vi, avl, data_only);
		vi = dirtyq_next(dq, vi);
	}
}

static void vi_grab_dirty_subs(const struct voluta_vnode_info *vi,
			       struct voluta_avl *avl, bool data_only)
{
	if (v_isinode(vi)) {
		ii_grab_dirty(ii_from_vi(vi), avl, data_only);
	}
}

static void cache_grab_dirty(struct voluta_cache *cache,
			     struct voluta_avl *avl, bool data_only)
{
	struct voluta_vnode_info *vi;
	const struct voluta_dirtyq *dq = &cache->c_vdq;

	vi = dirtyq_front(dq);
	while (vi != NULL) {
		vi_grab_dirty_subs(vi, avl, data_only);
		vi_add_to_set(vi, avl, data_only);
		vi = dirtyq_next(dq, vi);
	}
}

void voluta_grab_dirty_set(struct voluta_sb_info *sbi,
			   struct voluta_inode_info *ii,
			   struct voluta_avl *avl, bool data_only)
{
	if (ii != NULL) {
		ii_grab_dirty(ii, avl, data_only);
	} else {
		cache_grab_dirty(sbi->s_cache, avl, data_only);
	}
}

static void cache_clear_dirty(struct voluta_cache *cache,
			      const struct voluta_avl *avl)
{
	struct voluta_vnode_info *vi;
	const struct voluta_avl_node *itr;
	const struct voluta_avl_node *end;

	end = voluta_avl_end(avl);
	itr = voluta_avl_begin(avl);
	while (itr != end) {
		vi = avl_node_to_vi(itr);
		vi_undirtify(vi);
		itr = voluta_avl_next(avl, itr);
	}
	voluta_unused(cache);
}

void voluta_clear_dirty_set(struct voluta_sb_info *sbi,
			    const struct voluta_avl *avl)
{
	cache_clear_dirty(sbi->s_cache, avl);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_incref_of(struct voluta_vnode_info *vi)
{
	if (likely(vi != NULL)) {
		vi_incref(vi);
	}
}

void voluta_decref_of(struct voluta_vnode_info *vi)
{
	if (likely(vi != NULL)) {
		vi_decref(vi);
	}
}

size_t voluta_refcnt_of(const struct voluta_vnode_info *vi)
{
	return likely(vi != NULL) ? vi_refcnt(vi) : 0;
}

bool voluta_isevictable_ii(const struct voluta_inode_info *ii)
{
	return ii_is_evictable(ii);
}


void voluta_mark_visible(const struct voluta_vnode_info *vi)
{
	bki_mark_visible(vi->v_bki, v_vaddr_of(vi));
}

void voluta_mark_opaque_at(struct voluta_bk_info *bki,
			   const struct voluta_vaddr *vaddr)
{
	voluta_assert_eq(bki->b_lba, vaddr->lba);

	bki_mark_opaque(bki, vaddr);
}

void voluta_mark_opaque(const struct voluta_vnode_info *vi)
{
	voluta_mark_opaque_at(vi->v_bki, v_vaddr_of(vi));
}

bool voluta_is_visible(const struct voluta_vnode_info *vi)
{
	return bki_is_visible(vi->v_bki, v_vaddr_of(vi));
}

