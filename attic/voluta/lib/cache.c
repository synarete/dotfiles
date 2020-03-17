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
#include "voluta-lib.h"

#define CACHE_RETRY_MAX         128
#define CACHE_ELEM_MAGIC        0xCAC11E

/* Local functions forward declarations */
static void bki_init(struct voluta_bk_info *bki, union voluta_bk *bk);
static void bki_fini(struct voluta_bk_info *bki);
static void ii_init(struct voluta_inode_info *ii);
static void ii_fini(struct voluta_inode_info *ii);
static void vi_init(struct voluta_vnode_info *vi);
static void vi_fini(struct voluta_vnode_info *vi);
static void vi_incref(struct voluta_vnode_info *vi);
static void vi_decref(struct voluta_vnode_info *vi);
static struct voluta_vnode_info *vi_from_ce(const struct voluta_cache_elem *ce);
static struct voluta_cache_elem *vi_ce(const struct voluta_vnode_info *vi);
static void cache_shrink_some(struct voluta_cache *cache, size_t i, size_t n);
static void cache_undirtify_bk(struct voluta_cache *cache,
			       struct voluta_bk_info *bki);
static struct voluta_cache *cache_of(const struct voluta_bk_info *bki);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* LBA/offset re-hashing functions */
static uint64_t rotate(uint64_t x, size_t b)
{
	return (x << b) | (x >> (64 - b));
}

static uint64_t lba_hash(uint64_t lba)
{
	return voluta_twang_mix64(lba);
}

static uint64_t off_hash(uint64_t off)
{
	const uint64_t murmur3_c1 = 0x87c37b91114253d5ULL;

	return voluta_twang_mix64(off ^ murmur3_c1);
}

static uint64_t ino_hash(uint64_t ino)
{
	const uint64_t murmur3_c2 = 0x4cf5ad432745937fULL;

	return rotate(ino ^ murmur3_c2, ino % 19);
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

static union voluta_bk *new_bk(struct voluta_qalloc *qal)
{
	int err;
	void *mem = NULL;
	union voluta_bk *bk;

	err = voluta_malloc(qal, sizeof(*bk), &mem);
	if (err) {
		return NULL;
	}
	bk = mem;
	return bk;
}

static void del_bk(struct voluta_qalloc *qal, union voluta_bk *bk)
{
	int err;

	err = voluta_free(qal, bk, sizeof(*bk));
	voluta_assert_ok(err);
}

static struct voluta_bk_info *
new_bki(struct voluta_qalloc *qal, union voluta_bk *bk)
{
	int err;
	void *mem = NULL;
	struct voluta_bk_info *bki;

	err = voluta_malloc(qal, sizeof(*bki), &mem);
	if (err) {
		return NULL;
	}
	bki = mem;
	bki_init(bki, bk);
	return bki;
}

static void del_bki(struct voluta_qalloc *qal, struct voluta_bk_info *bki)
{
	int err;

	bki_fini(bki);
	err = voluta_free(qal, bki, sizeof(*bki));
	voluta_assert_ok(err);
}

static struct voluta_inode_info *new_ii(struct voluta_qalloc *qal)
{
	int err;
	void *mem;
	struct voluta_inode_info *ii;

	err = voluta_malloc(qal, sizeof(*ii), &mem);
	if (err) {
		return NULL;
	}
	ii = mem;
	ii_init(ii);
	return ii;
}

static void del_ii(struct voluta_qalloc *qal, struct voluta_inode_info *ii)
{
	int err;

	ii_fini(ii);
	err = voluta_free(qal, ii, sizeof(*ii));
	voluta_assert_ok(err);
}

static struct voluta_vnode_info *new_vi(struct voluta_qalloc *qal)
{
	int err;
	void *mem;
	struct voluta_vnode_info *vi;

	err = voluta_malloc(qal, sizeof(*vi), &mem);
	if (err) {
		return NULL;
	}
	vi = mem;
	vi_init(vi);
	return vi;
}

static void del_vi(struct voluta_qalloc *qal, struct voluta_vnode_info *vi)
{
	int err;

	vi_fini(vi);
	err = voluta_free(qal, vi, sizeof(*vi));
	voluta_assert_ok(err);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_cache_elem *
ce_from_htb_link(const struct voluta_list_head *lh)
{
	struct voluta_cache_elem *ce;

	ce = container_of(lh, struct voluta_cache_elem, ce_htb_lh);
	voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);

	return ce;
}

static struct voluta_cache_elem *
ce_from_lru_link(const struct voluta_list_head *lh)
{
	struct voluta_cache_elem *ce;

	ce = container_of(lh, struct voluta_cache_elem, ce_lru_lh);
	voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);

	return ce;
}

static struct voluta_cache_elem *
ce_from_drq_link(const struct voluta_list_head *lh)
{
	struct voluta_cache_elem *ce;

	ce = container_of(lh, struct voluta_cache_elem, ce_drq_lh);
	voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);

	return ce;
}

static void ce_init(struct voluta_cache_elem *ce)
{
	list_head_init(&ce->ce_htb_lh);
	list_head_init(&ce->ce_lru_lh);
	list_head_init(&ce->ce_drq_lh);
	ce->ce_cycle = 0;
	ce->ce_refcnt = 0;
	ce->ce_dirty = 0;
	ce->ce_magic = CACHE_ELEM_MAGIC;
}

static void ce_fini(struct voluta_cache_elem *ce)
{
	list_head_destroy(&ce->ce_drq_lh);
	list_head_destroy(&ce->ce_htb_lh);
	list_head_destroy(&ce->ce_lru_lh);
	ce->ce_cycle = 0;
	ce->ce_refcnt = 0;
	ce->ce_key = ~0ULL;
	ce->ce_dirty = -1;
	ce->ce_magic = -2;
}

static void ce_hmap(struct voluta_cache_elem *ce, struct voluta_list_head *hlst)
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

static void ce_relru(struct voluta_cache_elem *ce, struct voluta_list_head *lru)
{
	if (!ce_islru_front(ce, lru)) {
		ce_unlru(ce);
		ce_lru(ce, lru);
	}
}

static void ce_incref(struct voluta_cache_elem *ce)
{
	voluta_assert_lt(ce->ce_refcnt, UINT_MAX);
	ce->ce_refcnt++;
}

static void ce_decref(struct voluta_cache_elem *ce)
{
	voluta_assert_gt(ce->ce_refcnt, 0);
	ce->ce_refcnt--;
}

static bool ce_is_dirty(const struct voluta_cache_elem *ce)
{
	return (ce->ce_dirty != 0);
}

static void ce_set_dirty(struct voluta_cache_elem *ce, bool d)
{
	ce->ce_dirty = d ? 1 : 0;
}

static bool ce_is_evictable(const struct voluta_cache_elem *ce)
{
	voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);

	return !ce->ce_refcnt && !ce_is_dirty(ce);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void dirtyq_init(struct voluta_dirtyq *dq)
{
	list_head_init(&dq->dq);
	dq->sz = 0;
}

static void dirtyq_fini(struct voluta_dirtyq *dq)
{
	list_head_destroy(&dq->dq);
	dq->sz = 0;
}

static size_t dirtyq_size(const struct voluta_dirtyq *dq)
{
	return dq->sz;
}

static void dirtyq_enq(struct voluta_dirtyq *dq, struct voluta_cache_elem *ce)
{
	if (!ce_is_dirty(ce)) {
		list_push_back(&dq->dq, &ce->ce_drq_lh);
		ce_set_dirty(ce, true);
		dq->sz++;
	}
}

static void dirtyq_deq(struct voluta_dirtyq *dq, struct voluta_cache_elem *ce)
{
	if (ce_is_dirty(ce)) {
		voluta_assert_gt(dq->sz, 0);

		list_head_remove(&ce->ce_drq_lh);
		ce_set_dirty(ce, false);
		dq->sz--;
	}
}

static struct voluta_cache_elem *dirtyq_front(const struct voluta_dirtyq *dq)
{
	struct voluta_list_head *lh;
	struct voluta_cache_elem *ce = NULL;

	if (dq->sz > 0) {
		lh = list_front(&dq->dq);
		ce = ce_from_drq_link(lh);
	}
	return ce;
}

typedef void (*voluta_cache_elem_fn)(struct voluta_cache_elem *, void *);

static void dirtyq_for_each(const struct voluta_dirtyq *dq,
			    voluta_cache_elem_fn cb, void *arg)
{
	struct voluta_cache_elem *ce;
	struct voluta_list_head *itr;
	const struct voluta_list_head *end;

	itr = dq->dq.next;
	end = &dq->dq;
	while (itr != end) {
		ce = ce_from_drq_link(itr);
		itr = itr->next;

		voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);
		cb(ce, arg);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int cacheq_init(struct voluta_cacheq *cq, struct voluta_qalloc *qal,
		       size_t htbl_size, voulta_hash64_fn hash_fn)
{
	struct voluta_list_head *htbl;

	htbl = new_htbl(qal, htbl_size);
	if (htbl == NULL) {
		return -ENOMEM;
	}
	list_init(&cq->lru);
	cq->htbl = htbl;
	cq->hash_fn = hash_fn;
	cq->hsize = htbl_size;
	cq->qal = qal;
	cq->count = 0;
	return 0;
}

static void cacheq_fini(struct voluta_cacheq *cq)
{
	del_htbl(cq->qal, cq->htbl, cq->hsize);
	list_fini(&cq->lru);
	cq->htbl = NULL;
	cq->hash_fn = NULL;
	cq->hsize = 0;
	cq->qal = NULL;
}

static size_t cacheq_key_to_bin(const struct voluta_cacheq *cq, uint64_t ckey)
{
	return cq->hash_fn(ckey) % cq->hsize;
}

static void cacheq_store(struct voluta_cacheq *cq,
			 struct voluta_cache_elem *ce, uint64_t ckey)
{
	size_t bin;

	voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);

	ce->ce_key = ckey;
	bin = cacheq_key_to_bin(cq, ckey);
	ce_hmap(ce, &cq->htbl[bin]);
	ce_lru(ce, &cq->lru);
	cq->count += 1;
}

static struct voluta_cache_elem *
cacheq_find(const struct voluta_cacheq *cq, uint64_t ckey)
{
	size_t bin;
	struct voluta_cache_elem *ce;
	struct voluta_list_head *lst;
	struct voluta_list_head *itr;

	bin = cacheq_key_to_bin(cq, ckey);
	lst = &cq->htbl[bin];
	itr = lst->next;
	while (itr != lst) {
		ce = ce_from_htb_link(itr);
		if (ce->ce_key == ckey) {
			voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);
			return ce;
		}
		itr = itr->next;
	}
	return NULL;
}

static void cacheq_remove(struct voluta_cacheq *cq,
			  struct voluta_cache_elem *ce)
{
	voluta_assert_gt(cq->count, 0);
	voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);

	ce_hunmap(ce);
	ce_unlru(ce);
	cq->count -= 1;
}

static void cacheq_promote_lru(struct voluta_cacheq *cq,
			       struct voluta_cache_elem *ce)
{
	voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);
	ce_relru(ce, &cq->lru);
}

static struct voluta_cache_elem *
cacheq_get_lru(const struct voluta_cacheq *cq)
{
	struct voluta_cache_elem *ce = NULL;

	if (cq->count > 0) {
		ce = ce_from_lru_link(cq->lru.prev);
		voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);
	}
	return ce;
}

static void cacheq_for_each(struct voluta_cacheq *cq,
			    voluta_cache_elem_fn cb, void *arg)
{
	struct voluta_cache_elem *ce;
	struct voluta_list_head *lru = &cq->lru;
	struct voluta_list_head *itr = lru->prev;

	while (itr != lru) {
		ce = ce_from_lru_link(itr);
		itr = itr->prev;

		voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);
		cb(ce, arg);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_bk_info *bki_from_ce(const struct voluta_cache_elem *ce)
{
	const struct voluta_bk_info *bki = NULL;

	if (ce != NULL) {
		bki = container_of(ce, struct voluta_bk_info, b_ce);
	}
	return (struct voluta_bk_info *)bki;
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
	dirtyq_init(&bki->b_dq);
	bki_set_lba(bki, VOLUTA_LBA_NULL);
	bki->bk = bk;
	bki->b_sbi = NULL;
	bki->b_agm = NULL;
	bki->b_staged = 0;
}

static void bki_fini(struct voluta_bk_info *bki)
{
	ce_fini(&bki->b_ce);
	dirtyq_fini(&bki->b_dq);
	bki_set_lba(bki, VOLUTA_LBA_NULL);
	bki->bk = NULL;
	bki->b_sbi = NULL;
	bki->b_agm = NULL;
	bki->b_staged = -1;
}

static void bki_incref(struct voluta_bk_info *bki)
{
	ce_incref(bki_ce(bki));
}

static void bki_decref(struct voluta_bk_info *bki)
{
	ce_decref(bki_ce(bki));
}

static void bki_attach_ag(struct voluta_bk_info *bki,
			  struct voluta_vnode_info *agm_vi)
{
	if (bki->b_agm == NULL) {
		vi_incref(agm_vi);
		bki->b_agm = agm_vi;
	}
}

static void bki_detach_ag(struct voluta_bk_info *bki)
{
	if (bki->b_agm != NULL) {
		vi_decref(bki->b_agm);
		bki->b_agm = NULL;
	}
}

static void bki_dirtify_vi(struct voluta_bk_info *bki,
			   struct voluta_vnode_info *vi)
{
	dirtyq_enq(&bki->b_dq, vi_ce(vi));
}

static void bki_undirtify_vi(struct voluta_bk_info *bki,
			     struct voluta_vnode_info *vi)
{
	dirtyq_deq(&bki->b_dq, vi_ce(vi));
}

static bool bki_has_dirty(const struct voluta_bk_info *bki)
{
	return (dirtyq_size(&bki->b_dq) > 0);
}

static struct voluta_vnode_info *bki_get_dirty(struct voluta_bk_info *bki)
{
	return vi_from_ce(dirtyq_front(&bki->b_dq));
}

static void append_vi_to_dset(struct voluta_cache_elem *ce, void *arg)
{
	struct voluta_dirty_set *dset = arg;

	voluta_assert_lt(dset->cnt, ARRAY_SIZE(dset->vi));
	dset->vi[dset->cnt++] = vi_from_ce(ce);
}

void voluta_dirty_set_of(const struct voluta_bk_info *bki,
			 struct voluta_dirty_set *dset)
{
	memset(dset, 0, sizeof(*dset));
	dirtyq_for_each(&bki->b_dq, append_vi_to_dset, dset);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_bk_info *cache_new_bk(const struct voluta_cache *cache)
{
	union voluta_bk *bk;
	struct voluta_bk_info *bki;
	struct voluta_qalloc *qal = cache->qal;

	bk = new_bk(qal);
	if (bk == NULL) {
		return NULL;
	}
	bki = new_bki(qal, bk);
	if (bki == NULL) {
		del_bk(qal, bk);
		return NULL;
	}
	return bki;
}

static void cache_del_bk(const struct voluta_cache *cache,
			 struct voluta_bk_info *bki)
{
	union voluta_bk *bk = bki->bk;
	struct voluta_qalloc *qal = cache->qal;

	bki_fini(bki);
	del_bk(qal, bk);
	del_bki(qal, bki);
}

static int cache_init_bcq(struct voluta_cache *cache, size_t htbl_size)
{
	return cacheq_init(&cache->bcq, cache->qal, htbl_size, lba_hash);
}

static void cache_fini_bcq(struct voluta_cache *cache)
{
	cacheq_fini(&cache->bcq);
}

static size_t cache_ncycles(const struct voluta_cache *cache)
{
	return cache->qal->ncycles;
}

static void cache_stamp_bk(const struct voluta_cache *cache,
			   struct voluta_bk_info *bki)
{
	bki->b_ce.ce_cycle = cache_ncycles(cache);
}

static struct voluta_bk_info *
cache_find_bk(const struct voluta_cache *cache, loff_t lba)
{
	struct voluta_cache_elem *ce;
	const uint64_t key = (uint64_t)lba;

	ce = cacheq_find(&cache->bcq, key);
	return bki_from_ce(ce);
}

static void cache_store_bk(struct voluta_cache *cache,
			   struct voluta_bk_info *bki, loff_t lba)
{
	bki_set_lba(bki, lba);
	cacheq_store(&cache->bcq, &bki->b_ce, (unsigned long)lba);
}

static void cache_promote_lru_bk(struct voluta_cache *cache,
				 struct voluta_bk_info *bki)
{
	cacheq_promote_lru(&cache->bcq, &bki->b_ce);
}

static bool cache_is_evictable_bk(const struct voluta_cache *cache,
				  const struct voluta_bk_info *bki)
{
	const size_t ncycles = cache_ncycles(cache);
	const struct voluta_cache_elem *ce = bki_ce(bki);

	return (ce->ce_cycle != ncycles) && ce_is_evictable(ce);
}

static void cache_evict_bk(struct voluta_cache *cache,
			   struct voluta_bk_info *bki)
{
	voluta_assert(ce_is_evictable(bki_ce(bki)));

	cacheq_remove(&cache->bcq, &bki->b_ce);
	bki_detach_ag(bki);
	cache_del_bk(cache, bki);
}

void voluta_cache_forget_bk(struct voluta_cache *cache,
			    struct voluta_bk_info *bki)
{
	voluta_assert_eq(bki->b_ce.ce_refcnt, 0);

	cache_undirtify_bk(cache, bki);
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
	cache_stamp_bk(cache, bki);
	return bki;
}

static struct voluta_bk_info *
cache_find_stamp_bk(struct voluta_cache *cache, loff_t lba)
{
	struct voluta_bk_info *bki;

	bki = cache_find_bk(cache, lba);
	if (bki == NULL) {
		return NULL;
	}
	cache_promote_lru_bk(cache, bki);
	cache_stamp_bk(cache, bki);
	return bki;
}

int voluta_cache_lookup_bk(struct voluta_cache *cache, loff_t lba,
			   struct voluta_bk_info **out_bki)
{
	struct voluta_bk_info *bki;

	if (lba == VOLUTA_LBA_NULL) {
		return -ENOENT;
	}
	bki = cache_find_stamp_bk(cache, lba);
	if (bki == NULL) {
		return -ENOENT;
	}
	*out_bki = bki;
	return 0;
}

static struct voluta_bk_info *
cache_find_or_spawn_bk(struct voluta_cache *cache, loff_t lba)
{
	struct voluta_bk_info *bki;

	bki = cache_find_stamp_bk(cache, lba);
	if (bki != NULL) {
		return bki;
	}
	bki = cache_spawn_bk(cache, lba);
	if (bki == NULL) {
		return NULL; /* TODO: debug-trace */
	}
	return bki;
}

static void cache_dirtify_bk(struct voluta_cache *cache,
			     struct voluta_bk_info *bki)
{
	dirtyq_enq(&cache->dirtyq, bki_ce(bki));
}

static void cache_undirtify_bk(struct voluta_cache *cache,
			       struct voluta_bk_info *bki)
{
	dirtyq_deq(&cache->dirtyq, bki_ce(bki));
}

int voluta_cache_get_dirty(const struct voluta_cache *cache,
			   struct voluta_bk_info **out_bki)
{
	const struct voluta_cache_elem *ce;

	ce = dirtyq_front(&cache->dirtyq);
	if (ce == NULL) {
		return -ENOENT;
	}
	*out_bki = bki_from_ce(ce);
	return 0;
}

static struct voluta_bk_info *
cache_require_bk(struct voluta_cache *cache, loff_t lba)
{
	struct voluta_bk_info *bki = NULL;
	const size_t retry_max = CACHE_RETRY_MAX;

	for (size_t i = 0; (i < retry_max) && (bki == NULL); ++i) {
		cache_shrink_some(cache, i, retry_max);
		bki = cache_find_or_spawn_bk(cache, lba);
	}
	return bki;
}

int voluta_cache_spawn_bk(struct voluta_cache *cache, loff_t lba,
			  struct voluta_vnode_info *agm_vi,
			  struct voluta_bk_info **out_bki)
{
	struct voluta_bk_info *bki;

	bki = cache_require_bk(cache, lba);
	if (bki == NULL) {
		return -ENOMEM;
	}
	if (agm_vi != NULL) {
		bki_attach_ag(bki, agm_vi);
	}
	*out_bki = bki;
	return 0;
}

static struct voluta_bk_info *cache_get_lru_bk(struct voluta_cache *cache)
{
	struct voluta_cache_elem *ce;

	ce = cacheq_get_lru(&cache->bcq);
	return bki_from_ce(ce);
}

static void cache_try_evict_bk(struct voluta_cache *cache,
			       struct voluta_bk_info *bki)
{
	voluta_assert_not_null(bki);

	if (cache_is_evictable_bk(cache, bki)) {
		cache_evict_bk(cache, bki);
	}
}

static void try_evict_bk(struct voluta_cache_elem *ce, void *arg)
{
	struct voluta_bk_info *bki = bki_from_ce(ce);

	cache_try_evict_bk(cache_of(bki), bki);
	voluta_unused(arg);
}

static void cache_drop_evictable_bks(struct voluta_cache *cache)
{
	cacheq_for_each(&cache->bcq, try_evict_bk, NULL);
}

static void cache_evict_or_relru_bk(struct voluta_cache *cache,
				    struct voluta_bk_info *bki)
{
	if (cache_is_evictable_bk(cache, bki)) {
		cache_evict_bk(cache, bki);
	} else {
		cache_promote_lru_bk(cache, bki);
	}
}

static void cache_shrink_or_relru_bks(struct voluta_cache *cache, size_t count)
{
	struct voluta_bk_info *bki;
	const size_t n = voluta_min(count, cache->bcq.count);

	for (size_t i = 0; i < n; ++i) {
		bki = cache_get_lru_bk(cache);
		if (bki == NULL) {
			break;
		}
		cache_evict_or_relru_bk(cache, bki);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_vnode_info *vi_from_ce(const struct voluta_cache_elem *ce)
{
	const struct voluta_vnode_info *vi = NULL;

	if (ce != NULL) {
		vi = container_of(ce, struct voluta_vnode_info, ce);
	}
	return (struct voluta_vnode_info *)vi;
}

static struct voluta_cache_elem *vi_ce(const struct voluta_vnode_info *vi)
{
	return (struct voluta_cache_elem *)(&vi->ce);
}

static void vi_init(struct voluta_vnode_info *vi)
{
	voluta_vaddr_reset(&vi->vaddr);
	ce_init(vi_ce(vi));
	vi->view = NULL;
	vi->bki = NULL;
}

static void vi_fini(struct voluta_vnode_info *vi)
{
	voluta_vaddr_reset(&vi->vaddr);
	ce_fini(vi_ce(vi));
	vi->view = NULL;
	vi->u.p = NULL;
	vi->bki = NULL;
}

static void vi_assign(struct voluta_vnode_info *vi,
		      const struct voluta_vaddr *vaddr)
{
	voluta_vaddr_clone(vaddr, &vi->vaddr);
}

static void vi_incref(struct voluta_vnode_info *vi)
{
	ce_incref(vi_ce(vi));
}

static void vi_decref(struct voluta_vnode_info *vi)
{
	ce_decref(vi_ce(vi));
}

static void vi_detach_bk(struct voluta_vnode_info *vi)
{
	if (vi->bki != NULL) {
		bki_decref(vi->bki);
		vi->bki = NULL;
		vi->view = NULL;
		vi->u.p = NULL;
	}
}

static void vi_attach_bk(struct voluta_vnode_info *vi,
			 struct voluta_bk_info *bki)
{
	bki_incref(bki);
	vi->bki = bki;
}

static void vi_promote_bk(struct voluta_vnode_info *vi)
{
	cache_promote_lru_bk(cache_of(vi->bki), vi->bki);
}

static bool vi_is_evictable(const struct voluta_vnode_info *vi)
{
	return ce_is_evictable(vi_ce(vi));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_inode_info *ii_from_vi(const struct voluta_vnode_info *vi)
{
	const struct voluta_inode_info *ii = NULL;

	if (vi != NULL) {
		ii = container_of(vi, struct voluta_inode_info, vi);
	}
	return (struct voluta_inode_info *)ii;
}

static struct voluta_inode_info *ii_from_ce(const struct voluta_cache_elem *ce)
{
	return ii_from_vi(vi_from_ce(ce));
}

static void ii_init(struct voluta_inode_info *ii)
{
	vi_init(&ii->vi);
	ii->inode = NULL;
	ii->ino = VOLUTA_INO_NULL;
}

static void ii_fini(struct voluta_inode_info *ii)
{
	vi_fini(&ii->vi);
	ii->inode = NULL;
	ii->ino = VOLUTA_INO_NULL;
}

static void ii_assign(struct voluta_inode_info *ii,
		      const struct voluta_iaddr *iaddr)
{
	vi_assign(&ii->vi, &iaddr->vaddr);
	ii->ino = iaddr->ino;
}

static void ii_detach_bk(struct voluta_inode_info *ii)
{
	vi_detach_bk(&ii->vi);
}

static void ii_promote_bk(struct voluta_inode_info *ii)
{
	vi_promote_bk(&ii->vi);
}

static bool ii_is_evictable(const struct voluta_inode_info *ii)
{
	return vi_is_evictable(&ii->vi);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_vnode_info *cache_new_vi(const struct voluta_cache *cache)
{
	return new_vi(cache->qal);
}

static void cache_del_vi(const struct voluta_cache *cache,
			 struct voluta_vnode_info *vi)
{
	del_vi(cache->qal, vi);
}

static int cache_init_vcq(struct voluta_cache *cache, size_t htbl_size)
{
	return cacheq_init(&cache->vcq, cache->qal, htbl_size, off_hash);
}

static void cache_fini_vcq(struct voluta_cache *cache)
{
	cacheq_fini(&cache->vcq);
}

static struct voluta_vnode_info *
cache_find_vi(struct voluta_cache *cache, const struct voluta_vaddr *vaddr)
{
	struct voluta_cache_elem *ce;
	const uint64_t key = (uint64_t)(vaddr->off);

	ce = cacheq_find(&cache->vcq, key);
	return vi_from_ce(ce);
}

static void cache_evict_vi(struct voluta_cache *cache,
			   struct voluta_vnode_info *vi)
{
	cacheq_remove(&cache->vcq, &vi->ce);
	vi_detach_bk(vi);
	cache_del_vi(cache, vi);
}

static void cache_promote_lru_vi(struct voluta_cache *cache,
				 struct voluta_vnode_info *vi)
{
	cacheq_promote_lru(&cache->vcq, &vi->ce);
}

static void cache_stamp_vi(const struct voluta_cache *cache,
			   struct voluta_vnode_info *vi)
{
	vi->ce.ce_cycle = cache_ncycles(cache);
}

static struct voluta_vnode_info *
cache_lookup_vi(struct voluta_cache *cache, const struct voluta_vaddr *vaddr)
{
	struct voluta_vnode_info *vi;

	vi = cache_find_vi(cache, vaddr);
	if (vi == NULL) {
		return NULL;
	}
	vi_promote_bk(vi);
	cache_promote_lru_vi(cache, vi);
	cache_stamp_vi(cache, vi);
	return vi;
}

int voluta_cache_lookup_vi(struct voluta_cache *cache,
			   const struct voluta_vaddr *vaddr,
			   struct voluta_vnode_info **out_vi)
{
	*out_vi = cache_lookup_vi(cache, vaddr);

	return (*out_vi != NULL) ? 0 : -ENOENT;
}

static void cache_store_vi(struct voluta_cache *cache,
			   struct voluta_vnode_info *vi,
			   const struct voluta_vaddr *vaddr)
{
	vi_assign(vi, vaddr);
	cacheq_store(&cache->vcq, &vi->ce, (uint64_t)(vaddr->off));
}

static bool cache_is_evictable_vi(const struct voluta_cache *cache,
				  const struct voluta_vnode_info *vi)
{
	if (!vi_is_evictable(vi)) {
		return false;
	}
	if (vi->ce.ce_cycle == 0) {
		return true;
	}
	if ((vi->ce.ce_cycle + 1) < cache_ncycles(cache)) {
		return true;
	}
	return false;
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
	cache_stamp_vi(cache, vi);
	return vi;
}

static struct voluta_vnode_info *
cache_require_vi(struct voluta_cache *cache, const struct voluta_vaddr *vaddr)
{
	struct voluta_vnode_info *vi = NULL;
	const size_t retry_max = CACHE_RETRY_MAX;

	for (size_t i = 0; (i < retry_max) && (vi == NULL); ++i) {
		cache_shrink_some(cache, i, retry_max);
		vi = cache_spawn_vi(cache, vaddr);
	}
	return vi;
}

int voluta_cache_spawn_vi(struct voluta_cache *cache,
			  const struct voluta_vaddr *vaddr,
			  struct voluta_vnode_info **out_vi)
{
	*out_vi = cache_require_vi(cache, vaddr);

	return (*out_vi != NULL) ? 0 : -ENOMEM;
}

void voulta_cache_forget_vi(struct voluta_cache *cache,
			    struct voluta_vnode_info *vi)
{
	voluta_undirtify_vi(vi);
	cache_evict_vi(cache, vi);
}

void voluta_attach_vi(struct voluta_vnode_info *vi, struct voluta_bk_info *bki)
{
	voluta_assert_null(vi->bki);

	vi_attach_bk(vi, bki);
}

static struct voluta_vnode_info *cache_get_lru_vi(struct voluta_cache *cache)
{
	struct voluta_cache_elem *ce;

	ce = cacheq_get_lru(&cache->vcq);
	voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);

	return vi_from_ce(ce);
}

static void cache_try_evict_vi(struct voluta_cache *cache,
			       struct voluta_vnode_info *vi)
{
	voluta_assert_not_null(vi);

	if (cache_is_evictable_vi(cache, vi)) {
		cache_evict_vi(cache, vi);
	}
}

static struct voluta_cache *cache_of_vi(const struct voluta_vnode_info *vi)
{
	return cache_of(vi->bki);
}

static void try_evict_vi(struct voluta_cache_elem *ce, void *arg)
{
	struct voluta_vnode_info *vi = vi_from_ce(ce);

	cache_try_evict_vi(cache_of_vi(vi), vi);
	voluta_unused(arg);
}

static void cache_drop_evictable_vis(struct voluta_cache *cache)
{
	cacheq_for_each(&cache->vcq, try_evict_vi, NULL);
}

static void cache_evict_or_relru_vi(struct voluta_cache *cache,
				    struct voluta_vnode_info *vi)
{
	if (cache_is_evictable_vi(cache, vi)) {
		cache_evict_vi(cache, vi);
	} else {
		cache_promote_lru_vi(cache, vi);
	}
}

static void cache_shrink_or_relru_vis(struct voluta_cache *cache, size_t count)
{
	struct voluta_vnode_info *vi;
	const size_t n = voluta_min(count, cache->vcq.count);

	for (size_t i = 0; i < n; ++i) {
		vi = cache_get_lru_vi(cache);
		if (vi == NULL) {
			break;
		}
		cache_evict_or_relru_vi(cache, vi);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_inode_info *cache_new_ii(const struct voluta_cache *cache)
{
	return new_ii(cache->qal);
}

static void cache_del_ii(const struct voluta_cache *cache,
			 struct voluta_inode_info *ii)
{
	del_ii(cache->qal, ii);
}

static int cache_init_icq(struct voluta_cache *cache, size_t htbl_size)
{
	return cacheq_init(&cache->icq, cache->qal, htbl_size, ino_hash);
}

static void cache_fini_icq(struct voluta_cache *cache)
{
	cacheq_fini(&cache->icq);
}

static struct voluta_inode_info *cache_find_ii(struct voluta_cache *cache,
		ino_t ino)
{
	struct voluta_cache_elem *ce;
	const uint64_t key = (uint64_t)ino;

	ce = cacheq_find(&cache->icq, key);
	return ii_from_ce(ce);
}

static void cache_evict_ii(struct voluta_cache *cache,
			   struct voluta_inode_info *ii)
{
	cacheq_remove(&cache->icq, &ii->vi.ce);
	ii_detach_bk(ii);
	cache_del_ii(cache, ii);
}

static void cache_promote_lru_ii(struct voluta_cache *cache,
				 struct voluta_inode_info *ii)
{
	cacheq_promote_lru(&cache->icq, &ii->vi.ce);
}

static void cache_stamp_ii(const struct voluta_cache *cache,
			   struct voluta_inode_info *ii)
{
	cache_stamp_vi(cache, &ii->vi);
}

static struct voluta_inode_info *
cache_lookup_ii(struct voluta_cache *cache, ino_t ino)
{
	struct voluta_inode_info *ii;

	ii = cache_find_ii(cache, ino);
	if (ii == NULL) {
		return NULL;
	}
	ii_promote_bk(ii);
	cache_promote_lru_ii(cache, ii);
	cache_stamp_ii(cache, ii);
	return ii;
}

int voluta_cache_lookup_ii(struct voluta_cache *cache, ino_t ino,
			   struct voluta_inode_info **out_ii)
{
	*out_ii = cache_lookup_ii(cache, ino);

	return (*out_ii != NULL) ? 0 : -ENOENT;
}

static void cache_store_ii(struct voluta_cache *cache,
			   struct voluta_inode_info *ii,
			   const struct voluta_iaddr *iaddr)
{
	ii_assign(ii, iaddr);
	cacheq_store(&cache->icq, &ii->vi.ce, (uint64_t)(ii->ino));
}

static bool cache_is_evictable_ii(const struct voluta_cache *cache,
				  const struct voluta_inode_info *ii)
{
	return cache_is_evictable_vi(cache, &ii->vi);
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
	cache_stamp_ii(cache, ii);
	return ii;
}

static struct voluta_inode_info *
cache_require_ii(struct voluta_cache *cache, const struct voluta_iaddr *iaddr)
{
	struct voluta_inode_info *ii = NULL;
	const size_t retry_max = CACHE_RETRY_MAX;

	for (size_t i = 0; (i < retry_max) && (ii == NULL); ++i) {
		cache_shrink_some(cache, i, retry_max);
		ii = cache_spawn_ii(cache, iaddr);
	}
	return ii;
}

int voluta_cache_spawn_ii(struct voluta_cache *cache,
			  const struct voluta_iaddr *iaddr,
			  struct voluta_inode_info **out_ii)
{
	*out_ii = cache_require_ii(cache, iaddr);

	return (*out_ii != NULL) ? 0 : -ENOMEM;
}

void voulta_cache_forget_ii(struct voluta_cache *cache,
			    struct voluta_inode_info *ii)
{
	voluta_undirtify_vi(&ii->vi);
	cache_evict_ii(cache, ii);
}

bool voluta_isevictable_ii(const struct voluta_inode_info *ii)
{
	return ii_is_evictable(ii);
}

static struct voluta_inode_info *cache_get_lru_ii(struct voluta_cache *cache)
{
	struct voluta_cache_elem *ce;

	ce = cacheq_get_lru(&cache->icq);
	return ii_from_ce(ce);
}

static void cache_try_evict_ii(struct voluta_cache *cache,
			       struct voluta_inode_info *ii)
{
	voluta_assert_not_null(cache);
	voluta_assert_not_null(ii);

	if (cache_is_evictable_ii(cache, ii)) {
		cache_evict_ii(cache, ii);
	}
}
static struct voluta_cache *cache_of_ii(const struct voluta_inode_info *ii)
{
	return cache_of(ii->vi.bki);
}

static void try_evict_ii(struct voluta_cache_elem *ce, void *arg)
{
	struct voluta_inode_info *ii = ii_from_ce(ce);

	cache_try_evict_ii(cache_of_ii(ii), ii);
	voluta_unused(arg);
}

static void cache_drop_evictable_iis(struct voluta_cache *cache)
{
	cacheq_for_each(&cache->icq, try_evict_ii, NULL);
}

static void cache_evict_or_relru_ii(struct voluta_cache *cache,
				    struct voluta_inode_info *ii)
{
	if (cache_is_evictable_ii(cache, ii)) {
		cache_evict_ii(cache, ii);
	} else {
		cache_promote_lru_ii(cache, ii);
	}
}

static void cache_shrink_or_relru_iis(struct voluta_cache *cache, size_t count)
{
	struct voluta_inode_info *ii;
	const size_t n = voluta_min(count, cache->icq.count);

	for (size_t i = 0; i < n; ++i) {
		ii = cache_get_lru_ii(cache);
		if (ii == NULL) {
			break;
		}
		cache_evict_or_relru_ii(cache, ii);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int cache_init_null_bk(struct voluta_cache *cache)
{
	int err;
	void *mem;

	err = voluta_zalloc(cache->qal, sizeof(*cache->null_bk), &mem);
	if (!err) {
		cache->null_bk = mem;
	}
	return err;
}

static void cache_fini_null_bk(struct voluta_cache *cache)
{
	voluta_free(cache->qal, cache->null_bk, sizeof(*cache->null_bk));
	cache->null_bk = NULL;
}

int voluta_cache_init(struct voluta_cache *cache, struct voluta_qalloc *qal)
{
	int err;
	size_t htbl_size;
	size_t want_size;
	const size_t bk_size = VOLUTA_BK_SIZE;

	voluta_memzero(cache, sizeof(*cache));
	cache->qal = qal;
	dirtyq_init(&cache->dirtyq);
	err = cache_init_null_bk(cache);
	if (err) {
		return err;
	}
	want_size = (qal->st.memsz_data / bk_size) / 7;
	htbl_size = voluta_clamp(want_size, 1U << 13, 1U << 23);
	err = cache_init_bcq(cache, htbl_size);
	if (err) {
		/* TODO: cleanup */
		return err;
	}
	err = cache_init_icq(cache, htbl_size);
	if (err) {
		/* TODO: cleanup */
		return err;
	}
	err = cache_init_vcq(cache, htbl_size);
	if (err) {
		/* TODO: cleanup */
		return err;
	}
	return 0;
}

void voluta_cache_fini(struct voluta_cache *cache)
{
	cache_fini_vcq(cache);
	cache_fini_icq(cache);
	cache_fini_bcq(cache);
	cache_fini_null_bk(cache);
	dirtyq_fini(&cache->dirtyq);
	memset(cache, 0xFC, sizeof(*cache));
}

static void cache_shrink(struct voluta_cache *cache, size_t count)
{
	cache_shrink_or_relru_vis(cache, count);
	cache_shrink_or_relru_iis(cache, count);
	cache_shrink_or_relru_bks(cache, count);
}

static void cache_shrink_one(struct voluta_cache *cache)
{
	cache_shrink(cache, 1);
}

static void cache_shrink_all(struct voluta_cache *cache)
{
	cache_shrink(cache, UINT_MAX);
}

static void cache_shrink_some(struct voluta_cache *cache, size_t i, size_t n)
{
	if ((i + 1) >= n) {
		cache_shrink_all(cache);
	} else if (i > 0) {
		cache_shrink(cache, i);
	}
}

void voluta_cache_step_cycles(struct voluta_cache *cache, size_t n)
{
	cache->qal->ncycles += n;
}

void voluta_cache_next_cycle(struct voluta_cache *cache)
{
	voluta_cache_step_cycles(cache, 1);
}

void voluta_cache_relax(struct voluta_cache *cache, bool force)
{
	const struct voluta_qalloc *qal = cache->qal;

	if (force || ((qal->st.npages_used * 2) > qal->st.npages)) {
		cache_shrink_one(cache);
	}
}

void voluta_cache_drop(struct voluta_cache *cache)
{
	/* Multiple iterations for meta-bks as well */
	for (size_t i = 0; i < 3; ++i) {
		cache_drop_evictable_vis(cache);
		cache_drop_evictable_iis(cache);
		cache_drop_evictable_bks(cache);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_cache *cache_of(const struct voluta_bk_info *bki)
{
	return bki->b_sbi->s_cache;
}

static void dirtify_bk(struct voluta_bk_info *bki)
{
	cache_dirtify_bk(cache_of(bki), bki);
}

static void undirtify_bk(struct voluta_bk_info *bki)
{
	cache_undirtify_bk(cache_of(bki), bki);
}

void voluta_dirtify_vi(struct voluta_vnode_info *vi)
{
	struct voluta_bk_info *bki = vi->bki;

	bki_dirtify_vi(bki, vi);
	dirtify_bk(bki);
}

void voluta_undirtify_vi(struct voluta_vnode_info *vi)
{
	struct voluta_bk_info *bki = vi->bki;

	bki_undirtify_vi(bki, vi);
	if (!bki_has_dirty(bki)) {
		undirtify_bk(bki);
	}
}

void voluta_dirtify_ii(struct voluta_inode_info *ii)
{
	voluta_dirtify_vi(&ii->vi);
}

void voluta_mark_as_clean(struct voluta_bk_info *bki)
{
	struct voluta_vnode_info *vi;

	vi = bki_get_dirty(bki);
	while (vi != NULL) {
		voluta_undirtify_vi(vi);
		vi = bki_get_dirty(bki);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t voluta_inode_refcnt(const struct voluta_inode_info *ii)
{
	return ii ? ii->vi.ce.ce_refcnt : 0;
}
