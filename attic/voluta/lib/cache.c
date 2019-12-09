/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * This file is part of libvoluta
 *
 * Copyright (C) 2019 Shachar Sharon
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
#define CACHE_ELEM_MAGIC        VOLUTA_MASTER_MARK

/* Local functions forward declarations */
static void bk_info_init(struct voluta_bk_info *bki, union voluta_bk *bk);
static void bk_info_destroy(struct voluta_bk_info *bki);
static void inode_info_init(struct voluta_inode_info *ii);
static void inode_info_destroy(struct voluta_inode_info *ii);
static void vnode_info_init(struct voluta_vnode_info *vi);
static void vnode_info_destroy(struct voluta_vnode_info *vi);
static void cache_shrink(struct voluta_cache *, size_t);
static void cache_shrink_all(struct voluta_cache *);
static void cache_shrink_some(struct voluta_cache *, size_t, size_t);
static void cache_undirtify_bk(struct voluta_cache *, struct voluta_bk_info *);
static struct voluta_cache *cache_of_bk(const struct voluta_bk_info *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* LBA/offset re-hashing functions */
static uint64_t twang_mix64(uint64_t key)
{
	key = ~key + (key << 21);
	key = key ^ (key >> 24);
	key = key + (key << 3) + (key << 8);
	key = key ^ (key >> 14);
	key = key + (key << 2) + (key << 4);
	key = key ^ (key >> 28);
	key = key + (key << 31);

	return key;
}

static uint64_t rotate(uint64_t x, size_t b)
{
	return (x << b) | (x >> (64 - b));
}

static uint64_t lba_hash(uint64_t lba)
{
	return twang_mix64(lba);
}

static uint64_t off_hash(uint64_t off)
{
	const uint64_t murmur3_c1 = 0x87c37b91114253d5ULL;

	return twang_mix64(off ^ murmur3_c1);
}

static uint64_t ino_hash(uint64_t ino)
{
	const uint64_t murmur3_c2 = 0x4cf5ad432745937fULL;

	return rotate(ino ^ murmur3_c2, ino % 19);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_list_head *
new_htbl(struct voluta_qmalloc *qmal, size_t nelems)
{
	int err;
	void *mem = NULL;
	struct voluta_list_head *htbl;

	err = voluta_nalloc(qmal, sizeof(*htbl), nelems, &mem);
	if (err) {
		return NULL;
	}
	htbl = mem;
	list_head_initn(htbl, nelems);
	return htbl;
}

static void del_htbl(struct voluta_qmalloc *qmal,
		     struct voluta_list_head *htbl, size_t nelems)
{
	int err;

	list_head_destroyn(htbl, nelems);
	err = voluta_nfree(qmal, htbl, sizeof(*htbl), nelems);
	voluta_assert_ok(err);
}

static union voluta_bk *new_bk(struct voluta_qmalloc *qmal)
{
	int err;
	void *mem = NULL;
	union voluta_bk *bk;

	err = voluta_malloc(qmal, sizeof(*bk), &mem);
	if (err) {
		return NULL;
	}
	bk = mem;
	return bk;
}

static void del_bk(struct voluta_qmalloc *qmal, union voluta_bk *bk)
{
	int err;

	err = voluta_free(qmal, bk, sizeof(*bk));
	voluta_assert_ok(err);
}

static struct voluta_bk_info *
new_bki(struct voluta_qmalloc *qmal, union voluta_bk *bk)
{
	int err;
	void *mem = NULL;
	struct voluta_bk_info *bki;

	err = voluta_malloc(qmal, sizeof(*bki), &mem);
	if (err) {
		return NULL;
	}
	bki = mem;
	bk_info_init(bki, bk);
	return bki;
}

static void del_bki(struct voluta_qmalloc *qmal, struct voluta_bk_info *bki)
{
	int err;

	bk_info_destroy(bki);
	err = voluta_free(qmal, bki, sizeof(*bki));
	voluta_assert_ok(err);
}

static struct voluta_inode_info *new_ii(struct voluta_qmalloc *qmal)
{
	int err;
	void *mem;
	struct voluta_inode_info *ii;

	err = voluta_malloc(qmal, sizeof(*ii), &mem);
	if (err) {
		return NULL;
	}
	ii = mem;
	inode_info_init(ii);
	return ii;
}

static void del_ii(struct voluta_qmalloc *qmal, struct voluta_inode_info *ii)
{
	int err;

	inode_info_destroy(ii);
	err = voluta_free(qmal, ii, sizeof(*ii));
	voluta_assert_ok(err);
}

static struct voluta_vnode_info *new_vi(struct voluta_qmalloc *qmal)
{
	int err;
	void *mem;
	struct voluta_vnode_info *vi;

	err = voluta_zalloc(qmal, sizeof(*vi), &mem);
	if (err) {
		return NULL;
	}
	vi = mem;
	vnode_info_init(vi);
	return vi;
}

static void del_vi(struct voluta_qmalloc *qmal, struct voluta_vnode_info *vi)
{
	int err;

	vnode_info_destroy(vi);
	err = voluta_free(qmal, vi, sizeof(*vi));
	voluta_assert_ok(err);
}
/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_cache_elem *
cache_elem_from_htbl_link(const struct voluta_list_head *lnk)
{
	struct voluta_cache_elem *ce;

	ce = container_of(lnk, struct voluta_cache_elem, ce_htbl_link);
	voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);

	return ce;
}

static struct voluta_cache_elem *
cache_elem_from_lru_link(const struct voluta_list_head *lnk)
{
	struct voluta_cache_elem *ce;

	ce = container_of(lnk, struct voluta_cache_elem, ce_lru_link);
	voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);

	return ce;
}

static void cache_elem_init(struct voluta_cache_elem *ce)
{
	list_head_init(&ce->ce_lru_link);
	list_head_init(&ce->ce_htbl_link);
	ce->ce_cycle = 0;
	ce->ce_refcnt = 0;
	ce->ce_magic = CACHE_ELEM_MAGIC;
}

static void cache_elem_destroy(struct voluta_cache_elem *ce)
{
	list_head_destroy(&ce->ce_lru_link);
	list_head_destroy(&ce->ce_htbl_link);
	ce->ce_cycle = 0;
	ce->ce_refcnt = 0;
	ce->ce_key = ~0ULL;
	ce->ce_magic = -1;
}

static void cache_elem_hmap(struct voluta_cache_elem *ce,
			    struct voluta_list_head *hlst)
{
	list_push_front(hlst, &ce->ce_htbl_link);
}

static void cache_elem_hunmap(struct voluta_cache_elem *ce)
{
	list_head_remove(&ce->ce_htbl_link);
}

static void cache_elem_lru(struct voluta_cache_elem *ce,
			   struct voluta_list_head *lru)
{
	list_push_front(lru, &ce->ce_lru_link);
}

static void cache_elem_unlru(struct voluta_cache_elem *ce)
{
	list_head_remove(&ce->ce_lru_link);
}

static void cache_elem_relru(struct voluta_cache_elem *ce,
			     struct voluta_list_head *lru)
{
	cache_elem_unlru(ce);
	cache_elem_lru(ce, lru);
}

static void cache_elem_incref(struct voluta_cache_elem *ce)
{
	ce->ce_refcnt++;
}

static void cache_elem_decref(struct voluta_cache_elem *ce)
{
	voluta_assert_gt(ce->ce_refcnt, 0);
	ce->ce_refcnt--;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

typedef bool (*voluta_cachemap_eq_fn)(const struct voluta_cache_elem *ce,
				      unsigned long key);


static int cachemap_init(struct voluta_cachemap *cachemap,
			 struct voluta_qmalloc *qmal,
			 size_t htbl_size, voulta_hash64_fn hash_fn)
{
	struct voluta_list_head *htbl;

	htbl = new_htbl(qmal, htbl_size);
	if (htbl == NULL) {
		return -ENOMEM;
	}
	list_init(&cachemap->lru);
	cachemap->htbl = htbl;
	cachemap->hash_fn = hash_fn;
	cachemap->htbl_size = htbl_size;
	cachemap->qmal = qmal;
	cachemap->count = 0;
	return 0;
}

static void cachemap_destroy(struct voluta_cachemap *cachemap)
{
	del_htbl(cachemap->qmal, cachemap->htbl, cachemap->htbl_size);
	list_destroy(&cachemap->lru);
	cachemap->htbl = NULL;
	cachemap->hash_fn = NULL;
	cachemap->htbl_size = 0;
	cachemap->qmal = NULL;
}

static size_t
cachemap_hkey_to_bin(const struct voluta_cachemap *cachemap, uint64_t hkey)
{
	return hkey % cachemap->htbl_size;
}

static size_t
cachemap_ckey_to_bin(const struct voluta_cachemap *cachemap, uint64_t ckey)
{
	return cachemap_hkey_to_bin(cachemap, cachemap->hash_fn(ckey));
}

static void cachemap_store(struct voluta_cachemap *cachemap,
			   struct voluta_cache_elem *ce, uint64_t ckey)
{
	size_t bin;

	voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);

	ce->ce_key = ckey;
	bin = cachemap_ckey_to_bin(cachemap, ckey);
	cache_elem_hmap(ce, &cachemap->htbl[bin]);
	cache_elem_lru(ce, &cachemap->lru);
	cachemap->count += 1;
}

static struct voluta_cache_elem *
cachemap_find(const struct voluta_cachemap *cachemap, uint64_t ckey)
{
	size_t bin;
	struct voluta_cache_elem *ce;
	struct voluta_list_head *lst, *itr;

	bin = cachemap_ckey_to_bin(cachemap, ckey);
	lst = &cachemap->htbl[bin];
	itr = lst->next;
	while (itr != lst) {
		ce = cache_elem_from_htbl_link(itr);
		if (ce->ce_key == ckey) {
			voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);
			return ce;
		}
		itr = itr->next;
	}
	return NULL;
}

static void cachemap_remove(struct voluta_cachemap *cachemap,
			    struct voluta_cache_elem *ce)
{
	voluta_assert_gt(cachemap->count, 0);
	voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);

	cache_elem_hunmap(ce);
	cache_elem_unlru(ce);
	cachemap->count -= 1;
}

static void cachemap_promote_lru(struct voluta_cachemap *cachemap,
				 struct voluta_cache_elem *ce)
{
	voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);
	cache_elem_relru(ce, &cachemap->lru);
}

static struct voluta_cache_elem *
cachemap_get_lru(const struct voluta_cachemap *cachemap)
{
	struct voluta_cache_elem *ce = NULL;

	if (cachemap->count > 0) {
		ce = cache_elem_from_lru_link(cachemap->lru.prev);
		voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);
	}
	return ce;
}

typedef void (*voluta_cache_elem_fn)(struct voluta_cache_elem *);

static void cachemap_for_each(struct voluta_cachemap *cachemap,
			      voluta_cache_elem_fn callback)
{
	struct voluta_cache_elem *ce;
	struct voluta_list_head *itr, *lru = &cachemap->lru;

	itr = lru->prev;
	while (itr != lru) {
		ce = cache_elem_from_lru_link(itr);
		itr = itr->prev;

		voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);
		callback(ce);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_bk_info *
bk_info_from_cache_elem(const struct voluta_cache_elem *ce)
{
	const struct voluta_bk_info *bki = NULL;

	if (ce != NULL) {
		voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);
		bki = container_of(ce, struct voluta_bk_info, b_ce);
	}
	return (struct voluta_bk_info *)bki;
}

static struct voluta_bk_info *
bk_info_from_dirtyq_link(const struct voluta_list_head *lnk)
{
	return container_of(lnk, struct voluta_bk_info, b_dirtyq_link);
}

static void bk_info_init(struct voluta_bk_info *bki, union voluta_bk *bk)
{
	cache_elem_init(&bki->b_ce);
	list_head_init(&bki->b_dirtyq_link);
	vaddr_reset(&bki->b_vaddr);
	bki->bk = bk;
	bki->b_sbi = NULL;
	bki->b_uber_bki = NULL;
	bki->b_dirty = false;
	bki->b_staged = false;
}

static void bk_info_destroy(struct voluta_bk_info *bki)
{
	cache_elem_destroy(&bki->b_ce);
	list_head_destroy(&bki->b_dirtyq_link);
	vaddr_reset(&bki->b_vaddr);
	bki->bk = NULL;
	bki->b_sbi = NULL;
	bki->b_uber_bki = NULL;
}

static void bk_info_incref(struct voluta_bk_info *bki)
{
	cache_elem_incref(&bki->b_ce);
}

static void bk_info_decref(struct voluta_bk_info *bki)
{
	cache_elem_decref(&bki->b_ce);
}

static void bk_info_attach_uber(struct voluta_bk_info *bki,
				struct voluta_bk_info *uber_bki)
{
	if (!bki->b_uber_bki) {
		bk_info_incref(uber_bki);
		bki->b_uber_bki = uber_bki;
	}
}

static void bk_info_detach_uber(struct voluta_bk_info *bki)
{
	if (bki->b_uber_bki != NULL) {
		bk_info_decref(bki->b_uber_bki);
		bki->b_uber_bki = NULL;
	}
}

static void bk_info_assign(struct voluta_bk_info *bki,
			   const struct voluta_vaddr *vaddr)
{
	voluta_vaddr_clone(vaddr, &bki->b_vaddr);
}

static bool bk_info_is(const struct voluta_bk_info *bki,
		       enum voluta_vtype vtype)
{
	return (bki->b_vaddr.vtype == vtype);
}

static bool bk_info_is_uber(const struct voluta_bk_info *bki)
{
	return bk_info_is(bki, VOLUTA_VTYPE_UBER);
}

static bool bk_info_isdata(const struct voluta_bk_info *bki)
{
	return bk_info_is(bki, VOLUTA_VTYPE_DATA);
}

const struct voluta_vaddr *voluta_vaddr_of_bk(const struct voluta_bk_info *bki)
{
	return &bki->b_vaddr;
}

static loff_t bk_info_lba(const struct voluta_bk_info *bki)
{
	const struct voluta_vaddr *vaddr = voluta_vaddr_of_bk(bki);

	return vaddr->lba;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void dirtyq_init(struct voluta_dirtyq *dirtyq)
{
	list_head_init(&dirtyq->data);
	list_head_init(&dirtyq->meta);
	dirtyq->data_size = 0;
	dirtyq->meta_size = 0;
}

static void dirtyq_fini(struct voluta_dirtyq *dirtyq)
{
	list_head_destroy(&dirtyq->data);
	list_head_destroy(&dirtyq->meta);
	dirtyq->data_size = 0;
	dirtyq->meta_size = 0;
}

static struct voluta_list_head *
enq_pos_of(struct voluta_list_head *lst, const struct voluta_bk_info *bki)
{
	int steps = 4;
	loff_t lba, lba_prev;
	struct voluta_bk_info *bki_prev;
	struct voluta_list_head *itr;

	lba = bk_info_lba(bki);
	itr = lst->prev;
	while ((itr->prev != lst) && --steps) {
		bki_prev = bk_info_from_dirtyq_link(itr);
		lba_prev = bk_info_lba(bki_prev);
		if (lba_prev < lba) {
			break;
		}
		itr = itr->prev;
	}
	return itr;
}

static void dirtyq_enq_bk(struct voluta_dirtyq *dirtyq,
			  struct voluta_bk_info *bki)
{
	size_t *psz;
	struct voluta_list_head *lst, *pos;
	const bool data = bk_info_isdata(bki);

	if (!bki->b_dirty) {
		lst = data ? &dirtyq->data : &dirtyq->meta;
		psz = data ? &dirtyq->data_size : &dirtyq->meta_size;
		pos = enq_pos_of(lst, bki);
		list_head_insert_after(pos, &bki->b_dirtyq_link);
		bki->b_dirty = true;
		*psz += 1;
	}
}

static void dirtyq_deq_bk(struct voluta_dirtyq *dirtyq,
			  struct voluta_bk_info *bki)
{
	size_t *psz;
	const bool data = bk_info_isdata(bki);

	if (bki->b_dirty) {
		psz = data ? &dirtyq->data_size : &dirtyq->meta_size;

		voluta_assert_gt(*psz, 0);

		list_head_remove(&bki->b_dirtyq_link);
		bki->b_dirty = false;
		*psz -= 1;
	}
}

static struct voluta_bk_info *dirtyq_front(const struct voluta_dirtyq *dirtyq)
{
	struct voluta_bk_info *bki;

	if (dirtyq->data_size > 0) {
		bki = bk_info_from_dirtyq_link(dirtyq->data.next);
	} else if (dirtyq->meta_size > 0) {
		bki = bk_info_from_dirtyq_link(dirtyq->meta.next);
	} else {
		bki = NULL;
	}
	return bki;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_bk_info *cache_new_bk(const struct voluta_cache *cache)
{
	union voluta_bk *bk;
	struct voluta_bk_info *bki;
	struct voluta_qmalloc *qmal = cache->qmal;

	bk = new_bk(qmal);
	if (bk == NULL) {
		return NULL;
	}
	bki = new_bki(qmal, bk);
	if (bki == NULL) {
		del_bk(qmal, bk);
		return NULL;
	}
	return bki;
}

static void cache_del_bk(const struct voluta_cache *cache,
			 struct voluta_bk_info *bki)
{
	union voluta_bk *bk = bki->bk;
	struct voluta_qmalloc *qmal = cache->qmal;

	bk_info_destroy(bki);
	del_bk(qmal, bk);
	del_bki(qmal, bki);
}

static int cache_init_bmap(struct voluta_cache *cache, size_t htbl_size)
{
	return cachemap_init(&cache->bcmap, cache->qmal, htbl_size, lba_hash);
}

static void cache_destroy_bmap(struct voluta_cache *cache)
{
	cachemap_destroy(&cache->bcmap);
}

static size_t cache_ncycles(const struct voluta_cache *cache)
{
	return cache->qmal->ncycles;
}

static void cache_stamp_bk(const struct voluta_cache *cache,
			   struct voluta_bk_info *bki)
{
	bki->b_ce.ce_cycle = cache_ncycles(cache);
}

static struct voluta_bk_info *
cache_find_bk(const struct voluta_cache *cache,
	      const struct voluta_vaddr *vaddr)
{
	struct voluta_cache_elem *ce;
	const uint64_t key = (uint64_t)vaddr->lba;

	ce = cachemap_find(&cache->bcmap, key);
	return bk_info_from_cache_elem(ce);
}

static void cache_store_bk(struct voluta_cache *cache,
			   struct voluta_bk_info *bki,
			   const struct voluta_vaddr *vaddr)
{
	bk_info_assign(bki, vaddr);
	cachemap_store(&cache->bcmap, &bki->b_ce, (unsigned long)vaddr->lba);
}

static void cache_promote_lru_bk(struct voluta_cache *cache,
				 struct voluta_bk_info *bki)
{
	cachemap_promote_lru(&cache->bcmap, &bki->b_ce);
}

static bool cache_is_evictable_bk(const struct voluta_cache *cache,
				  const struct voluta_bk_info *bki)
{
	const size_t ncycles = cache_ncycles(cache);

	voluta_assert_eq(bki->b_ce.ce_magic, CACHE_ELEM_MAGIC);

	if (bki->b_ce.ce_cycle == ncycles) {
		return false;
	}
	if (bki->b_ce.ce_refcnt) {
		return false;
	}
	if (bki->b_dirty) {
		return false;
	}
	return true;
}

static bool cache_is_aged_bk(const struct voluta_cache *cache,
			     const struct voluta_bk_info *bki)
{
	return ((bki->b_ce.ce_cycle + 2) < cache_ncycles(cache));
}

static void cache_evict_bk(struct voluta_cache *cache,
			   struct voluta_bk_info *bki)
{
	cachemap_remove(&cache->bcmap, &bki->b_ce);
	bk_info_detach_uber(bki);
	cache_del_bk(cache, bki);
}

void voluta_cache_forget_bk(struct voluta_cache *cache,
			    struct voluta_bk_info *bki)
{
	voluta_assert_eq(bki->b_ce.ce_refcnt, 0);

	cache_undirtify_bk(cache, bki);
	cache_evict_bk(cache, bki);
}

void voluta_cache_try_forget_bk(struct voluta_cache *cache,
				struct voluta_bk_info *bki)
{
	if (cache_is_aged_bk(cache, bki) &&
	    cache_is_evictable_bk(cache, bki)) {
		cache_evict_bk(cache, bki);
	}
}

static struct voluta_bk_info *cache_spawn_bk(struct voluta_cache *cache,
		const struct voluta_vaddr *vaddr)
{
	struct voluta_bk_info *bki;

	voluta_assert_ne(vaddr->vtype, VOLUTA_VTYPE_NONE);
	bki = cache_new_bk(cache);
	if (bki == NULL) {
		return NULL;
	}
	cache_store_bk(cache, bki, vaddr);
	cache_stamp_bk(cache, bki);
	return bki;
}

static struct voluta_bk_info *
cache_find_stamp_bk(struct voluta_cache *cache,
		    const struct voluta_vaddr *vaddr)
{
	struct voluta_bk_info *bki;

	bki = cache_find_bk(cache, vaddr);
	if (bki == NULL) {
		return NULL;
	}
	cache_promote_lru_bk(cache, bki);
	cache_stamp_bk(cache, bki);
	return bki;
}

int voluta_cache_lookup_bk(struct voluta_cache *cache,
			   const struct voluta_vaddr *vaddr,
			   struct voluta_bk_info **out_bki)
{
	struct voluta_bk_info *bki;

	if (vaddr_isnull(vaddr)) {
		return -ENOENT;
	}
	bki = cache_find_stamp_bk(cache, vaddr);
	if (bki == NULL) {
		return -ENOENT;
	}
	*out_bki = bki;
	return 0;
}

static struct voluta_bk_info *
cache_find_or_spawn_bk(struct voluta_cache *cache,
		       const struct voluta_vaddr *vaddr)
{
	struct voluta_bk_info *bki;

	bki = cache_find_stamp_bk(cache, vaddr);
	if (bki != NULL) {
		return bki;
	}
	bki = cache_spawn_bk(cache, vaddr);
	if (bki == NULL) {
		return NULL; /* TODO: debug-trace */
	}
	return bki;
}

static void cache_dirtify_bk(struct voluta_cache *cache,
			     struct voluta_bk_info *bki)
{
	dirtyq_enq_bk(&cache->dirtyq, bki);
}

static void cache_undirtify_bk(struct voluta_cache *cache,
			       struct voluta_bk_info *bki)
{
	dirtyq_deq_bk(&cache->dirtyq, bki);
}

struct voluta_bk_info *voluta_first_dirty_bk(const struct voluta_cache *cache)
{
	return dirtyq_front(&cache->dirtyq);
}

static void attach_bk(struct voluta_bk_info *bki,
		      struct voluta_bk_info *uber_bki)
{
	voluta_assert(bk_info_is_uber(uber_bki));

	bk_info_attach_uber(bki, uber_bki);
}

static struct voluta_bk_info *
cache_require_bk(struct voluta_cache *cache, const struct voluta_vaddr *vaddr)
{
	struct voluta_bk_info *bki = NULL;
	const size_t retry_max = CACHE_RETRY_MAX;

	for (size_t i = 0; (i < retry_max) && (bki == NULL); ++i) {
		cache_shrink_some(cache, i, retry_max);
		bki = cache_find_or_spawn_bk(cache, vaddr);
	}
	return bki;
}

int voluta_cache_spawn_bk(struct voluta_cache *cache,
			  const struct voluta_vaddr *vaddr,
			  struct voluta_bk_info *uber_bki,
			  struct voluta_bk_info **out)
{
	struct voluta_bk_info *bki;

	bki = cache_require_bk(cache, vaddr);
	if (bki == NULL) {
		return -ENOMEM;
	}
	if (bki && uber_bki && !bki->b_uber_bki) {
		attach_bk(bki, uber_bki);
	}
	*out = bki;
	return 0;
}

static struct voluta_bk_info *cache_get_lru_bk(struct voluta_cache *cache)
{
	struct voluta_cache_elem *ce;

	ce = cachemap_get_lru(&cache->bcmap);
	return bk_info_from_cache_elem(ce);
}

static void cache_try_evict_bk(struct voluta_cache *cache,
			       struct voluta_bk_info *bki)
{
	voluta_assert_not_null(bki);

	if (cache_is_evictable_bk(cache, bki)) {
		cache_evict_bk(cache, bki);
	}
}

static void try_evict_bk(struct voluta_cache_elem *ce)
{
	struct voluta_bk_info *bki = bk_info_from_cache_elem(ce);

	cache_try_evict_bk(cache_of_bk(bki), bki);
}

static void cache_drop_evictable_bks(struct voluta_cache *cache)
{
	/* Multiple iterations for uber-bks as well */
	for (size_t i = 0; i < 3; ++i) {
		cachemap_for_each(&cache->bcmap, try_evict_bk);
	}
}

static void cache_shrink_or_relru_bks(struct voluta_cache *cache, size_t count)
{
	struct voluta_bk_info *bki;
	const size_t n = voluta_min(count, cache->bcmap.count);

	for (size_t i = 0; i < n; ++i) {
		bki = cache_get_lru_bk(cache);
		if (bki == NULL) {
			break;
		}
		if (cache_is_evictable_bk(cache, bki)) {
			cache_evict_bk(cache, bki);
		} else {
			cache_promote_lru_bk(cache, bki);
		}
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_vnode_info *
vnode_info_from_cache_elem(const struct voluta_cache_elem *ce)
{
	const struct voluta_vnode_info *vi = NULL;

	if (ce != NULL) {
		vi = container_of(ce, struct voluta_vnode_info, ce);
	}
	return (struct voluta_vnode_info *)vi;
}

static void vnode_info_init(struct voluta_vnode_info *vi)
{
	voluta_vaddr_reset(&vi->vaddr);
	cache_elem_init(&vi->ce);
	vi->view = NULL;
	vi->bki = NULL;
}

static void vnode_info_destroy(struct voluta_vnode_info *vi)
{
	voluta_vaddr_reset(&vi->vaddr);
	cache_elem_destroy(&vi->ce);
	vi->view = NULL;
	vi->u.p = NULL;
	vi->bki = NULL;
}

static void vnode_info_assign(struct voluta_vnode_info *vi,
			      const struct voluta_vaddr *vaddr)
{
	voluta_vaddr_clone(vaddr, &vi->vaddr);
}

static void vnode_info_detach_bk(struct voluta_vnode_info *vi)
{
	if (vi->bki != NULL) {
		bk_info_decref(vi->bki);
		vi->bki = NULL;
		vi->view = NULL;
		vi->u.p = NULL;
	}
}

static void vnode_info_attach_bk(struct voluta_vnode_info *vi,
				 struct voluta_bk_info *bki)
{
	voluta_assert_null(vi->bki);

	bk_info_incref(bki);
	vi->bki = bki;
}

static void vnode_info_promote_bk(struct voluta_vnode_info *vi)
{
	cache_promote_lru_bk(cache_of_bk(vi->bki), vi->bki);
}

static bool vnode_info_is_evictable(const struct voluta_vnode_info *vi)
{
	voluta_assert_eq(vi->ce.ce_magic, CACHE_ELEM_MAGIC);

	return (vi->ce.ce_refcnt == 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_inode_info *
inode_info_from_vnode(const struct voluta_vnode_info *vi)
{
	const struct voluta_inode_info *ii = NULL;

	if (vi != NULL) {
		ii = container_of(vi, struct voluta_inode_info, vi);
	}
	return (struct voluta_inode_info *)ii;
}

static struct voluta_inode_info *
inode_info_from_cache_elem(const struct voluta_cache_elem *ce)
{
	return inode_info_from_vnode(vnode_info_from_cache_elem(ce));
}

static void inode_info_init(struct voluta_inode_info *ii)
{
	vnode_info_init(&ii->vi);
	ii->inode = NULL;
	ii->ino = VOLUTA_INO_NULL;
}

static void inode_info_destroy(struct voluta_inode_info *ii)
{
	vnode_info_destroy(&ii->vi);
	ii->inode = NULL;
	ii->ino = VOLUTA_INO_NULL;
}

static void inode_info_assign(struct voluta_inode_info *ii,
			      const struct voluta_iaddr *iaddr)
{
	vnode_info_assign(&ii->vi, &iaddr->vaddr);
	ii->ino = iaddr->ino;
}

static void inode_info_detach_bk(struct voluta_inode_info *ii)
{
	vnode_info_detach_bk(&ii->vi);
}

static bool ino_isnull(ino_t ino)
{
	return (ino == VOLUTA_INO_NULL);
}

static bool has_null_ino(const struct voluta_inode_info *ii)
{
	return ino_isnull(i_ino_of(ii));
}

static void inode_info_promote_bk(struct voluta_inode_info *ii)
{
	vnode_info_promote_bk(&ii->vi);
}

static bool inode_info_is_evictable(const struct voluta_inode_info *ii)
{
	return vnode_info_is_evictable(&ii->vi);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_vnode_info *cache_new_vi(const struct voluta_cache *cache)
{
	return new_vi(cache->qmal);
}

static void cache_del_vi(const struct voluta_cache *cache,
			 struct voluta_vnode_info *vi)
{
	del_vi(cache->qmal, vi);
}

static int cache_init_vmap(struct voluta_cache *cache, size_t htbl_size)
{
	return cachemap_init(&cache->vcmap, cache->qmal, htbl_size, off_hash);
}

static void cache_destroy_vmap(struct voluta_cache *cache)
{
	cachemap_destroy(&cache->vcmap);
}

static struct voluta_vnode_info *
cache_find_vi(struct voluta_cache *cache, const struct voluta_vaddr *vaddr)
{
	struct voluta_cache_elem *ce;
	const uint64_t key = (uint64_t)(vaddr->off);

	ce = cachemap_find(&cache->vcmap, key);
	return vnode_info_from_cache_elem(ce);
}

static void cache_evict_vi(struct voluta_cache *cache,
			   struct voluta_vnode_info *vi)
{
	cachemap_remove(&cache->vcmap, &vi->ce);
	vnode_info_detach_bk(vi);
	cache_del_vi(cache, vi);
}

static void cache_promote_lru_vi(struct voluta_cache *cache,
				 struct voluta_vnode_info *vi)
{
	cachemap_promote_lru(&cache->vcmap, &vi->ce);
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
	vnode_info_promote_bk(vi);
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
	vnode_info_assign(vi, vaddr);
	cachemap_store(&cache->vcmap, &vi->ce, (uint64_t)(vaddr->off));
}

static bool cache_is_evictable_now_vi(const struct voluta_cache *cache,
				      const struct voluta_vnode_info *vi)
{
	if (!vnode_info_is_evictable(vi)) {
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
	cache_evict_vi(cache, vi);
}

bool voluta_isevictable_vi(const struct voluta_vnode_info *vi)
{
	return vnode_info_is_evictable(vi);
}

void voluta_attach_vi(struct voluta_vnode_info *vi, struct voluta_bk_info *bki)
{
	voluta_assert_not_null(bki->b_uber_bki);

	vnode_info_attach_bk(vi, bki);
}

static struct voluta_vnode_info *cache_get_lru_vi(struct voluta_cache *cache)
{
	struct voluta_cache_elem *ce;

	ce = cachemap_get_lru(&cache->vcmap);
	voluta_assert_eq(ce->ce_magic, CACHE_ELEM_MAGIC);

	return vnode_info_from_cache_elem(ce);
}

static void cache_try_evict_vi(struct voluta_cache *cache,
			       struct voluta_vnode_info *vi)
{
	voluta_assert_not_null(vi);

	if (cache_is_evictable_now_vi(cache, vi)) {
		cache_evict_vi(cache, vi);
	}
}

static struct voluta_cache *cache_of_vi(const struct voluta_vnode_info *vi)
{
	return cache_of_bk(vi->bki);
}

static void try_evict_vi(struct voluta_cache_elem *ce)
{
	struct voluta_vnode_info *vi = vnode_info_from_cache_elem(ce);

	cache_try_evict_vi(cache_of_vi(vi), vi);
}

static void cache_drop_evictable_vis(struct voluta_cache *cache)
{
	cachemap_for_each(&cache->vcmap, try_evict_vi);
}

static void cache_shrink_or_relru_vis(struct voluta_cache *cache, size_t count)
{
	struct voluta_vnode_info *vi;
	const size_t n = voluta_min(count, cache->vcmap.count);

	for (size_t i = 0; i < n; ++i) {
		vi = cache_get_lru_vi(cache);
		if (vi == NULL) {
			break;
		}
		if (cache_is_evictable_now_vi(cache, vi)) {
			cache_evict_vi(cache, vi);
		} else {
			cache_promote_lru_vi(cache, vi);
		}
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_inode_info *cache_new_ii(const struct voluta_cache *cache)
{
	return new_ii(cache->qmal);
}

static void cache_del_ii(const struct voluta_cache *cache,
			 struct voluta_inode_info *ii)
{
	del_ii(cache->qmal, ii);
}

static int cache_init_imap(struct voluta_cache *cache, size_t htbl_size)
{
	return cachemap_init(&cache->icmap, cache->qmal, htbl_size, ino_hash);
}

static void cache_destroy_imap(struct voluta_cache *cache)
{
	cachemap_destroy(&cache->icmap);
}

static struct voluta_inode_info *cache_find_ii(struct voluta_cache *cache,
		ino_t ino)
{
	struct voluta_cache_elem *ce;
	const uint64_t key = (uint64_t)ino;

	ce = cachemap_find(&cache->icmap, key);
	return inode_info_from_cache_elem(ce);
}

static void cache_evict_ii(struct voluta_cache *cache,
			   struct voluta_inode_info *ii)
{
	cachemap_remove(&cache->icmap, &ii->vi.ce);
	inode_info_detach_bk(ii);
	cache_del_ii(cache, ii);
}

static void cache_promote_lru_ii(struct voluta_cache *cache,
				 struct voluta_inode_info *ii)
{
	cachemap_promote_lru(&cache->icmap, &ii->vi.ce);
}

static void cache_stamp_ii(const struct voluta_cache *cache,
			   struct voluta_inode_info *ii)
{
	cache_stamp_vi(cache, &ii->vi);
}

static struct voluta_inode_info *cache_lookup_ii(struct voluta_cache *cache,
		ino_t ino)
{
	struct voluta_inode_info *ii;

	ii = cache_find_ii(cache, ino);
	if (ii == NULL) {
		return NULL;
	}
	inode_info_promote_bk(ii);
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
	inode_info_assign(ii, iaddr);
	cachemap_store(&cache->icmap, &ii->vi.ce, (uint64_t)(ii->ino));
}

static bool cache_is_evictable_now_ii(const struct voluta_cache *cache,
				      const struct voluta_inode_info *ii)
{
	return cache_is_evictable_now_vi(cache, &ii->vi);
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
	voluta_assert(!has_null_ino(ii));

	cache_evict_ii(cache, ii);
}

bool voluta_isevictable_ii(const struct voluta_inode_info *ii)
{
	return inode_info_is_evictable(ii);
}

static struct voluta_inode_info *cache_get_lru_ii(struct voluta_cache *cache)
{
	struct voluta_cache_elem *ce;

	ce = cachemap_get_lru(&cache->icmap);
	return inode_info_from_cache_elem(ce);
}

static void cache_try_evict_ii(struct voluta_cache *cache,
			       struct voluta_inode_info *ii)
{
	voluta_assert_not_null(cache);
	voluta_assert_not_null(ii);

	if (cache_is_evictable_now_ii(cache, ii)) {
		cache_evict_ii(cache, ii);
	}
}
static struct voluta_cache *cache_of_ii(const struct voluta_inode_info *ii)
{
	return cache_of_bk(ii->vi.bki);
}

static void try_evict_ii(struct voluta_cache_elem *ce)
{
	struct voluta_inode_info *ii = inode_info_from_cache_elem(ce);

	cache_try_evict_ii(cache_of_ii(ii), ii);
}

static void cache_drop_evictable_iis(struct voluta_cache *cache)
{
	cachemap_for_each(&cache->icmap, try_evict_ii);
}

static void cache_shrink_or_relru_iis(struct voluta_cache *cache, size_t count)
{
	struct voluta_inode_info *ii;
	const size_t n = voluta_min(count, cache->icmap.count);

	for (size_t i = 0; i < n; ++i) {
		ii = cache_get_lru_ii(cache);
		if (ii == NULL) {
			break;
		}
		if (cache_is_evictable_now_ii(cache, ii)) {
			cache_evict_ii(cache, ii);
		} else {
			cache_promote_lru_ii(cache, ii);
		}
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int cache_init_null_bk(struct voluta_cache *cache)
{
	int err;
	void *mem;

	err = voluta_zalloc(cache->qmal, sizeof(*cache->null_bk), &mem);
	if (!err) {
		cache->null_bk = mem;
	}
	return err;
}

static void cache_fini_null_bk(struct voluta_cache *cache)
{
	voluta_free(cache->qmal, cache->null_bk, sizeof(*cache->null_bk));
	cache->null_bk = NULL;
}

int voluta_cache_init(struct voluta_cache *cache, struct voluta_qmalloc *qmal)
{
	int err;
	size_t htbl_size, want_size;
	const size_t bk_size = VOLUTA_BK_SIZE;

	memset(cache, 0, sizeof(*cache));
	cache->qmal = qmal;
	dirtyq_init(&cache->dirtyq);
	err = cache_init_null_bk(cache);
	if (err) {
		return err;
	}
	want_size = (qmal->st.memsz_data / bk_size) / 7;
	htbl_size = voluta_clamp(want_size, 1U << 13, 1U << 23);
	err = cache_init_bmap(cache, htbl_size);
	if (err) {
		/* TODO: cleanup */
		return err;
	}
	err = cache_init_imap(cache, htbl_size);
	if (err) {
		/* TODO: cleanup */
		return err;
	}
	err = cache_init_vmap(cache, htbl_size);
	if (err) {
		/* TODO: cleanup */
		return err;
	}
	return 0;
}

void voluta_cache_fini(struct voluta_cache *cache)
{
	cache_destroy_vmap(cache);
	cache_destroy_imap(cache);
	cache_destroy_bmap(cache);
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
	cache->qmal->ncycles += n;
}

void voluta_cache_next_cycle(struct voluta_cache *cache)
{
	voluta_cache_step_cycles(cache, 1);
}

void voluta_cache_relax(struct voluta_cache *cache, bool force)
{
	const struct voluta_qmalloc *qmal = cache->qmal;

	if (force || ((qmal->st.npages_used * 2) > qmal->st.npages)) {
		cache_shrink_one(cache);
	}
}

void voluta_cache_drop(struct voluta_cache *cache)
{
	cache_drop_evictable_vis(cache);
	cache_drop_evictable_iis(cache);
	cache_drop_evictable_bks(cache);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_cache *cache_of_bk(const struct voluta_bk_info *bki)
{
	const struct voluta_sb_info *sbi = bki->b_sbi;

	return sbi->s_cache;
}

static void do_dirtify_bk(struct voluta_bk_info *bki)
{
	cache_dirtify_bk(cache_of_bk(bki), bki);
}

static void do_no_unwritten(const struct voluta_bk_info *bki)
{
	if (voluta_is_unwritten(bki)) {
		voluta_clear_unwritten(bki);
	}
}

void voluta_dirtify_bk(struct voluta_bk_info *bki)
{
	do_dirtify_bk(bki);
	if (!bk_info_is_uber(bki)) {
		do_no_unwritten(bki);
	}
}

void voluta_undirtify_bk(struct voluta_bk_info *bki)
{
	cache_undirtify_bk(cache_of_bk(bki), bki);
}

void voluta_dirtify_vi(const struct voluta_vnode_info *vi)
{
	voluta_dirtify_bk(vi->bki);
}

void voluta_undirtify_vi(const struct voluta_vnode_info *vi)
{
	voluta_undirtify_bk(vi->bki);
}

void voluta_dirtify_ii(const struct voluta_inode_info *ii)
{
	voluta_dirtify_vi(&ii->vi);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t voluta_inode_refcnt(const struct voluta_inode_info *ii)
{
	return ii ? ii->vi.ce.ce_refcnt : 0;
}
