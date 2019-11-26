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
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include "voluta-lib.h"

#define MPAGE_SHIFT             VOLUTA_PAGE_SHIFT_MIN
#define MPAGE_SIZE              VOLUTA_PAGE_SIZE_MIN
#define MPAGE_NSEGS             (MPAGE_SIZE / MSLAB_SEG_SIZE)
#define MPAGES_IN_BK            (VOLUTA_BK_SIZE / MPAGE_SIZE)
#define MSLAB_SHIFT_MIN         VOLUTA_CACHELINE_SHIFT_MIN
#define MSLAB_SHIFT_MAX         (MPAGE_SHIFT - 1)
#define MSLAB_SIZE_MIN          (1U << MSLAB_SHIFT_MIN)
#define MSLAB_SIZE_MAX          (1U << MSLAB_SHIFT_MAX)
#define MSLAB_SEG_SIZE          MSLAB_SIZE_MIN
#define MSLAB_INDEX_NONE        (-1)
#define MALLOC_SIZE_MAX         (8 * VOLUTA_MEGA)

#define STATICASSERT_EQ(x_, y_) \
	VOLUTA_STATICASSERT_EQ(x_, y_)

#define STATICASSERT_SIZEOF(t_, s_) \
	STATICASSERT_EQ(sizeof(t_), s_)

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct voluta_slab_seg {
	uint64_t data[6]; /* keep first */
	struct voluta_list_head link;
} voluta_aligned64;


union voluta_page {
	struct voluta_slab_seg seg[MPAGE_NSEGS];
	uint8_t data[MPAGE_SIZE];
} voluta_packed_aligned64;


struct voluta_page_info {
	struct voluta_page_info *prev;
	union voluta_page *pg;
	struct voluta_list_head link;
	size_t pg_index;
	size_t pg_count; /* num pages free/used */
	int pg_free;
	int slab_index;
	int slab_nused;
	int slab_nelems;

} voluta_aligned64;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void static_assert_alloc_sizes(void)
{
	STATICASSERT_SIZEOF(struct voluta_slab_seg, VOLUTA_CACHELINE_SIZE_MIN);
	STATICASSERT_SIZEOF(struct voluta_slab_seg, MSLAB_SEG_SIZE);
	STATICASSERT_SIZEOF(union voluta_page, MPAGE_SIZE);
	STATICASSERT_SIZEOF(struct voluta_page_info, VOLUTA_CACHELINE_SIZE_MIN);
	STATICASSERT_EQ(MPAGES_IN_BK, 4);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_page_info *link_to_page_info(const struct voluta_list_head
		*link)
{
	return container_of(link, struct voluta_page_info, link);
}

static void page_info_update(struct voluta_page_info *pgi,
			     struct voluta_page_info *prev, size_t count)
{
	pgi->prev = prev;
	pgi->pg_count = count;
	pgi->pg_free = 1;
}

static void page_info_mute(struct voluta_page_info *pgi)
{
	page_info_update(pgi, NULL, 0);
}

static void page_info_init(struct voluta_page_info *pgi,
			   union voluta_page *pg, size_t pg_index)
{
	list_head_init(&pgi->link);
	page_info_mute(pgi);
	pgi->pg = pg;
	pgi->pg_index = pg_index;
	pgi->slab_nused = 0;
	pgi->slab_index = MSLAB_INDEX_NONE;
}

static void page_info_push_head(struct voluta_page_info *pgi,
				struct voluta_list_head *ls)
{
	list_push_front(ls, &pgi->link);
}

static void page_info_push_tail(struct voluta_page_info *pgi,
				struct voluta_list_head *ls)
{
	list_push_back(ls, &pgi->link);
}

static void page_info_unlink(struct voluta_page_info *pgi)
{
	list_head_remove(&pgi->link);
}

static void page_info_unlink_mute(struct voluta_page_info *pgi)
{
	page_info_unlink(pgi);
	page_info_mute(pgi);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_slab_seg *link_to_slab_seg(const struct voluta_list_head
		*link)
{
	return container_of(link, struct voluta_slab_seg, link);
}

static struct voluta_list_head *slab_seg_to_link(struct voluta_slab_seg *seg)
{
	return &seg->link;
}

static bool slab_issize(size_t size)
{
	return ((size >= MSLAB_SIZE_MIN) && (size <= MSLAB_SIZE_MAX));
}

static int slab_size_to_index(size_t size, size_t *out_index)
{
	size_t idx, nlz, shift = MSLAB_SHIFT_MIN;

	if (!slab_issize(size)) {
		return -EINVAL;
	}
	nlz = voluta_clz(((unsigned int)size - 1) >> shift);
	if (!nlz || (nlz > 32)) {
		return -EINVAL;
	}
	idx = 32 - nlz;
	if (idx >= VOLUTA_ALLOC_NSLABS) {
		return -EINVAL;
	}
	*out_index = idx;
	return 0;
}

static void slab_init(struct voluta_slab *slab, size_t sindex, size_t elemsz)
{
	int err;
	size_t index_by_elemsz = 0;

	err = slab_size_to_index(elemsz, &index_by_elemsz);
	if (err || (sindex != index_by_elemsz)) {
		voluta_panic("slab: index=%lu elemsz=%lu", sindex, elemsz);
	}
	list_init(&slab->free_list);
	slab->elemsz = elemsz;
	slab->nfree = 0;
	slab->nused = 0;
	slab->sindex = sindex;
}

static void slab_fini(struct voluta_slab *slab)
{
	list_init(&slab->free_list);
	slab->elemsz = 0;
	slab->nfree = 0;
	slab->nused = 0;
	slab->sindex = UINT_MAX;
}

static void slab_expand(struct voluta_slab *slab, struct voluta_page_info *pgi)
{
	struct voluta_slab_seg *seg;
	union voluta_page *pg = pgi->pg;
	const size_t step = slab->elemsz / sizeof(*seg);

	pgi->slab_index = (int)slab->sindex;
	pgi->slab_nelems = (int)(sizeof(*pg) / slab->elemsz);
	pgi->slab_nused = 0;
	for (size_t i = 0; i < ARRAY_SIZE(pg->seg); i += step) {
		seg = &pg->seg[i];
		list_push_back(&slab->free_list, &seg->link);
		slab->nfree++;
	}
}

static void slab_shrink(struct voluta_slab *slab, struct voluta_page_info *pgi)
{
	struct voluta_slab_seg *seg;
	union voluta_page *pg = pgi->pg;
	const size_t step = slab->elemsz / sizeof(*seg);

	voluta_assert_eq(pgi->slab_index, slab->sindex);
	voluta_assert_eq(pgi->slab_nused, 0);

	for (size_t i = 0; i < ARRAY_SIZE(pg->seg); i += step) {
		voluta_assert_gt(slab->nfree, 0);

		seg = &pg->seg[i];
		list_head_remove(&seg->link);
		slab->nfree--;
	}
	pgi->slab_index = -1;
	pgi->slab_nelems = 0;
}

static struct voluta_slab_seg *slab_alloc(struct voluta_slab *slab)
{
	struct voluta_list_head *lh;
	struct voluta_slab_seg *seg = NULL;

	lh = list_pop_front(&slab->free_list);
	if (lh == NULL) {
		return NULL;
	}
	voluta_assert_gt(slab->nfree, 0);
	slab->nfree--;
	slab->nused++;
	seg = link_to_slab_seg(lh);
	return seg;
}

static void slab_free(struct voluta_slab *slab, struct voluta_slab_seg *seg)
{
	struct voluta_list_head *lh;

	lh = slab_seg_to_link(seg);
	list_push_front(&slab->free_list, lh);
	voluta_assert_gt(slab->nused, 0);
	slab->nused--;
	slab->nfree++;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int resolve_mem_sizes(size_t npgs, size_t *msz_data, size_t *msz_meta)
{
	const size_t npgs_max = UINT_MAX; /* TODO: proper upper limit */

	if ((npgs == 0) || (npgs > npgs_max)) {
		return -EINVAL;
	}
	*msz_data = npgs * sizeof(union voluta_page);
	*msz_meta = npgs * sizeof(struct voluta_page_info);
	return 0;
}

static int memfd_setup(const char *name, size_t size,
		       int *out_fd, void **out_mem)
{
	int err, fd = -1;
	void *mem = NULL;
	const int prot = PROT_READ | PROT_WRITE;
	const int flags = MAP_SHARED;

	err = voluta_sys_memfd_create(name, 0, &fd);
	if (err) {
		return err;
	}
	err = voluta_sys_ftruncate(fd, (loff_t)size);
	if (err) {
		voluta_sys_close(fd);
		return err;
	}
	err = voluta_sys_mmap(NULL, size, prot, flags, fd, 0, &mem);
	if (err) {
		voluta_sys_close(fd);
		return err;
	}
	*out_fd = fd;
	*out_mem = mem;
	return 0;
}

static int memfd_close(int fd, void *mem, size_t memsz)
{
	int err;

	err = voluta_sys_munmap(mem, memsz);
	if (err) {
		return err;
	}
	err = voluta_sys_close(fd);
	if (err) {
		return err;
	}
	return 0;
}

static int qmalloc_init_memfd(struct voluta_qmalloc *qmal, size_t npgs)
{
	int err;

	err = resolve_mem_sizes(npgs, &qmal->st.memsz_data,
				&qmal->st.memsz_meta);
	if (err) {
		return err;
	}
	err = memfd_setup("voluta-mem-data", qmal->st.memsz_data,
			  &qmal->memfd_data, &qmal->mem_data);
	if (err) {
		return err;
	}
	err = memfd_setup("voluta-mem-meta", qmal->st.memsz_meta,
			  &qmal->memfd_meta, &qmal->mem_meta);
	if (err) {
		memfd_close(qmal->memfd_data,
			    qmal->mem_data, qmal->st.memsz_data);
		return err;
	}
	qmal->st.nbytes_used = 0;
	qmal->st.npages = npgs;
	return 0;
}

static int qmalloc_fini_memfd(struct voluta_qmalloc *qmal)
{
	int err;

	if (!qmal->st.npages) {
		return 0;
	}
	err = memfd_close(qmal->memfd_data, qmal->mem_data,
			  qmal->st.memsz_data);
	if (err) {
		return err;
	}
	err = memfd_close(qmal->memfd_meta, qmal->mem_meta,
			  qmal->st.memsz_meta);
	if (err) {
		return err;
	}
	qmal->memfd_data = -1;
	qmal->memfd_meta = -1;
	qmal->mem_data = NULL;
	qmal->mem_meta = NULL;
	qmal->st.memsz_data = 0;
	qmal->st.memsz_meta = 0;
	return 0;
}

static void qmalloc_init_slabs(struct voluta_qmalloc *qmal)
{
	size_t elemsz, shift = MSLAB_SHIFT_MIN;
	struct voluta_slab *slab;

	for (size_t i = 0; i < ARRAY_SIZE(qmal->slabs); ++i) {
		voluta_assert_le(shift, MSLAB_SHIFT_MAX);
		elemsz = 1U << shift++;
		slab = &qmal->slabs[i];
		slab_init(slab, i, elemsz);
	}
}

static void qmalloc_fini_slabs(struct voluta_qmalloc *qmal)
{
	for (size_t i = 0; i < ARRAY_SIZE(qmal->slabs); ++i) {
		slab_fini(&qmal->slabs[i]);
	}
}

static union voluta_page *qmalloc_page_at(const struct voluta_qmalloc *qmal,
			size_t idx)
{
	union voluta_page *pg_arr = qmal->mem_data;

	return (idx < qmal->st.npages) ? pg_arr + idx : NULL;
}

static struct voluta_page_info *qmalloc_page_info_at(const struct voluta_qmalloc
		*qmal,
		size_t idx)
{
	struct voluta_page_info *pgi_arr = qmal->mem_meta;

	return (idx < qmal->st.npages) ? pgi_arr + idx : NULL;
}

static struct voluta_page_info *
qmalloc_next(const struct voluta_qmalloc *qmal,
	     const struct voluta_page_info *pgi, size_t npgs)
{
	return qmalloc_page_info_at(qmal, pgi->pg_index + npgs);
}

static void qmalloc_update(const struct voluta_qmalloc *qmal,
			   struct voluta_page_info *pgi, size_t npgs)
{
	struct voluta_page_info *pgi_next;

	pgi_next = qmalloc_next(qmal, pgi, npgs);
	if (pgi_next != NULL) {
		pgi_next->prev = pgi;
	}
}

static void qmalloc_add_free(struct voluta_qmalloc *qmal,
			     struct voluta_page_info *pgi,
			     struct voluta_page_info *prev, size_t npgs)
{
	const size_t head_threshold = MPAGES_IN_BK;
	struct voluta_list_head *free_list = &qmal->free_list;

	page_info_update(pgi, prev, npgs);
	qmalloc_update(qmal, pgi, npgs);
	if (npgs >= head_threshold) {
		page_info_push_head(pgi, free_list);
	} else {
		page_info_push_tail(pgi, free_list);
	}
}

static void qmalloc_init_pages(struct voluta_qmalloc *qmal)
{
	union voluta_page *pg;
	struct voluta_page_info *pgi;

	for (size_t i = 0; i < qmal->st.npages; ++i) {
		pg = qmalloc_page_at(qmal, i);
		pgi = qmalloc_page_info_at(qmal, i);
		page_info_init(pgi, pg, i);
	}

	list_init(&qmal->free_list);
	pgi = qmalloc_page_info_at(qmal, 0);
	qmalloc_add_free(qmal, pgi, NULL, qmal->st.npages);
}

int voluta_qmalloc_init(struct voluta_qmalloc *qmal, size_t mlimit)
{
	int err;
	size_t npgs;
	const size_t qmalloc_size_min = (16 * VOLUTA_MEGA);

	if (mlimit < qmalloc_size_min) {
		return -EINVAL;
	}
	static_assert_alloc_sizes();
	qmal->st.npages_used = 0;
	qmal->st.nbytes_used = 0;
	qmal->ncycles = 1;
	qmal->pedantic_mode = false;

	npgs = mlimit / MPAGE_SIZE;
	err = qmalloc_init_memfd(qmal, npgs);
	if (err) {
		return err;
	}
	qmalloc_init_pages(qmal);
	qmalloc_init_slabs(qmal);
	return 0;
}

int voluta_qmalloc_fini(struct voluta_qmalloc *qmal)
{
	/* TODO: release all pending memory-elements in slabs */
	qmalloc_fini_slabs(qmal);
	return qmalloc_fini_memfd(qmal);
}

int voluta_qmalloc_new(size_t mlimit, struct voluta_qmalloc **out_qmal)
{
	int err;
	void *page;
	size_t page_size;

	page_size = voluta_sc_page_size();
	if (sizeof(**out_qmal) > page_size) {
		return -ENOMEM;
	}

	err = voluta_mmap_memory(page_size, &page);
	if (err) {
		return err;
	}

	err = voluta_qmalloc_init(page, mlimit);
	if (err) {
		voluta_munmap_memory(page, page_size);
		return err;
	}
	*out_qmal = page;
	return 0;
}

int voluta_qmalloc_del(struct voluta_qmalloc *qmal)
{
	int err;
	void *page = qmal;
	size_t page_size;

	err = voluta_qmalloc_fini(qmal);
	if (err) {
		return err;
	}
	page_size = voluta_sc_page_size();
	voluta_munmap_memory(page, page_size);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t nbytes_to_npgs(size_t nbytes)
{
	return (nbytes + MPAGE_SIZE - 1) / MPAGE_SIZE;
}

static size_t npgs_to_nbytes(size_t npgs)
{
	return npgs * MPAGE_SIZE;
}

static loff_t qmalloc_ptr_to_off(const struct voluta_qmalloc *qmal,
				 const void *ptr)
{
	return (const char *)ptr - (const char *)qmal->mem_data;
}

static size_t qmalloc_ptr_to_pgn(const struct voluta_qmalloc *qmal,
				 const void *ptr)
{
	const loff_t off = qmalloc_ptr_to_off(qmal, ptr);

	return (size_t)off / MPAGE_SIZE;
}

static bool qmalloc_isvalid_range(const struct voluta_qmalloc *qmal,
				  loff_t off_beg, loff_t off_end)
{
	return (off_beg >= 0) && (off_end < (loff_t)qmal->st.memsz_data);
}

static bool qmalloc_isinrange(const struct voluta_qmalloc *qmal,
			      const void *ptr, size_t nb)
{
	const loff_t off = qmalloc_ptr_to_off(qmal, ptr);

	return qmalloc_isvalid_range(qmal, off, off + (loff_t)nb);
}

static struct voluta_page_info *
qmalloc_page_info_of(const struct voluta_qmalloc *qmal, const void *ptr)
{
	const size_t pgn = qmalloc_ptr_to_pgn(qmal, ptr);

	voluta_assert_lt(pgn, qmal->st.npages);
	return qmalloc_page_info_at(qmal, pgn);
}

static struct voluta_slab_seg *
qmalloc_slab_seg_of(const struct voluta_qmalloc *qmal, const void *ptr)
{
	loff_t off;
	size_t idx;
	struct voluta_slab_seg *seg = qmal->mem_data;

	off = qmalloc_ptr_to_off(qmal, ptr);
	idx = (size_t)off / sizeof(*seg);

	return &seg[idx];
}

static struct voluta_page_info *
qmalloc_search_free_from_tail(struct voluta_qmalloc *qmal, size_t npgs)
{
	struct voluta_page_info *pgi;
	struct voluta_list_head *itr, *free_list = &qmal->free_list;

	itr = free_list->prev;
	while (itr != free_list) {
		pgi = link_to_page_info(itr);
		if (pgi->pg_count >= npgs) {
			return pgi;
		}
		itr = itr->prev;
	}
	return NULL;
}

static struct voluta_page_info *
qmalloc_search_free_from_head(struct voluta_qmalloc *qmal, size_t npgs)
{
	struct voluta_page_info *pgi;
	struct voluta_list_head *itr, *free_list = &qmal->free_list;

	itr = free_list->next;
	while (itr != free_list) {
		pgi = link_to_page_info(itr);
		if (pgi->pg_count >= npgs) {
			return pgi;
		}
		itr = itr->next;
	}
	return NULL;
}

static struct voluta_page_info *qmalloc_search_free_list(
	struct voluta_qmalloc *qmal,
	size_t npgs)
{
	struct voluta_page_info *pgi = NULL;
	const size_t head_threshold = MPAGES_IN_BK;

	if ((qmal->st.npages_used + npgs) <= qmal->st.npages) {
		if (npgs >= head_threshold) {
			pgi = qmalloc_search_free_from_head(qmal, npgs);
		} else {
			pgi = qmalloc_search_free_from_tail(qmal, npgs);
		}
	}
	return pgi;
}

static struct voluta_page_info *qmalloc_alloc_npgs(struct voluta_qmalloc *qmal,
		size_t npgs)
{
	struct voluta_page_info *pgi_next, *pgi = NULL;

	pgi = qmalloc_search_free_list(qmal, npgs);
	if (pgi == NULL) {
		return NULL;
	}
	voluta_assert_eq(pgi->slab_index, MSLAB_INDEX_NONE);
	voluta_assert_ge(pgi->pg_count, npgs);

	page_info_unlink(pgi);
	pgi->pg_free = 0;
	if (pgi->pg_count == npgs) {
		return pgi;
	}
	pgi_next = qmalloc_next(qmal, pgi, npgs);
	voluta_assert_eq(pgi_next->slab_index, MSLAB_INDEX_NONE);
	voluta_assert_eq(pgi_next->pg_count, 0);
	voluta_assert_eq(pgi_next->pg_free, 1);
	qmalloc_add_free(qmal, pgi_next, pgi, pgi->pg_count - npgs);

	pgi->pg_count = npgs;
	return pgi;
}

static struct voluta_slab *qmalloc_slab_of(struct voluta_qmalloc *qmal,
		size_t nbytes)
{
	int err;
	size_t sindex;
	struct voluta_slab *slab = NULL;

	err = slab_size_to_index(nbytes, &sindex);
	if (!err && (sindex < ARRAY_SIZE(qmal->slabs))) {
		slab = &qmal->slabs[sindex];
	}
	return slab;
}

static int qmalloc_require_slab_space(struct voluta_qmalloc *qmal,
				      struct voluta_slab *slab)
{
	struct voluta_page_info *pgi;

	if (slab->nfree > 0) {
		return 0;
	}
	pgi = qmalloc_alloc_npgs(qmal, 1);
	if (pgi == NULL) {
		return -ENOMEM;
	}
	slab_expand(slab, pgi);
	return 0;
}

static struct voluta_slab_seg *
qmalloc_alloc_from_slab(struct voluta_qmalloc *qmal, struct voluta_slab *slab)
{
	struct voluta_slab_seg *seg;
	struct voluta_page_info *pgi;

	seg = slab_alloc(slab);
	if (seg == NULL) {
		return NULL;
	}
	pgi = qmalloc_page_info_of(qmal, seg);

	voluta_assert_lt(pgi->slab_nused, pgi->slab_nelems);
	pgi->slab_nused += 1;

	return seg;
}

static int qmalloc_alloc_slab(struct voluta_qmalloc *qmal, size_t nbytes,
			      struct voluta_slab_seg **out_seg)
{
	int err;
	struct voluta_slab *slab;
	struct voluta_slab_seg *seg;

	slab = qmalloc_slab_of(qmal, nbytes);
	if (slab == NULL) {
		return -ENOMEM;
	}
	err = qmalloc_require_slab_space(qmal, slab);
	if (err) {
		return err;
	}
	seg = qmalloc_alloc_from_slab(qmal, slab);
	if (seg == NULL) {
		return -ENOMEM;
	}
	*out_seg = seg;
	return 0;
}

static int qmalloc_check_alloc(const struct voluta_qmalloc *qmal, size_t nbytes)
{
	const size_t nbytes_max = MALLOC_SIZE_MAX;

	if (qmal->mem_data == NULL) {
		return -ENOMEM;
	}
	if (nbytes > nbytes_max) {
		return -ENOMEM;
	}
	if (!nbytes) {
		return -EINVAL;
	}
	return 0;
}

static int qmalloc_alloc_sub_pg(struct voluta_qmalloc *qmal, size_t nbytes,
				void **out_ptr)
{
	int err;
	struct voluta_slab_seg *seg;

	err = qmalloc_alloc_slab(qmal, nbytes, &seg);
	if (err) {
		return err;
	}
	*out_ptr = seg->data;
	return 0;
}

static int qmalloc_alloc_multi_pg(struct voluta_qmalloc *qmal, size_t nbytes,
				  void **out_ptr)
{
	size_t npgs;
	struct voluta_page_info *pgi;

	npgs = nbytes_to_npgs(nbytes);
	pgi = qmalloc_alloc_npgs(qmal, npgs);
	if (pgi == NULL) {
		return -ENOMEM;
	}
	*out_ptr = pgi->pg->data;
	qmal->st.npages_used += npgs;
	voluta_assert_ge(qmal->st.npages, qmal->st.npages_used);
	return 0;
}

int voluta_malloc(struct voluta_qmalloc *qmal, size_t nbytes, void **out_ptr)
{
	int err;

	err = qmalloc_check_alloc(qmal, nbytes);
	if (err) {
		return err;
	}
	if (slab_issize(nbytes)) {
		err = qmalloc_alloc_sub_pg(qmal, nbytes, out_ptr);
	} else {
		err = qmalloc_alloc_multi_pg(qmal, nbytes, out_ptr);
	}
	if (err) {
		return err;
	}
	qmal->st.nbytes_used += nbytes;
	return 0;
}

int voluta_nalloc(struct voluta_qmalloc *qmal, size_t elemsz,
		  size_t nelems, void **out_ptr)
{
	return voluta_malloc(qmal, elemsz * nelems, out_ptr);
}

int voluta_zalloc(struct voluta_qmalloc *qmal, size_t nbytes, void **out_ptr)
{
	int err;

	err = voluta_malloc(qmal, nbytes, out_ptr);
	if (!err) {
		memset(*out_ptr, 0, nbytes);
	}
	return err;
}

static int qmalloc_check_free(const struct voluta_qmalloc *qmal,
			      const void *ptr, size_t nbytes)
{
	if ((qmal->mem_data == NULL) || (ptr == NULL)) {
		return -EINVAL;
	}
	if (!nbytes || (nbytes > MALLOC_SIZE_MAX)) {
		return -EINVAL;
	}
	if (!qmalloc_isinrange(qmal, ptr, nbytes)) {
		return -EINVAL;
	}
	return 0;
}

static void
qmalloc_punch_hole_at(const struct voluta_qmalloc *qmal,
		      const struct voluta_page_info *pgi, size_t npgs)
{
	int err;
	loff_t off, len;
	const int mode = FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE;

	off = (loff_t)npgs_to_nbytes(pgi->pg_index);
	len = (loff_t)npgs_to_nbytes(npgs);
	err = voluta_sys_fallocate(qmal->memfd_data, mode, off, len);
	voluta_assert_ok(err);
}

static int qmalloc_free_npgs(struct voluta_qmalloc *qmal,
			     struct voluta_page_info *pgi, size_t npgs)
{
	struct voluta_page_info *pgi_next, *pgi_prev;

	qmalloc_punch_hole_at(qmal, pgi, npgs);
	pgi_next = qmalloc_next(qmal, pgi, npgs);
	if (pgi_next && pgi_next->pg_free) {
		voluta_assert_gt(pgi_next->pg_count, 0);
		npgs += pgi_next->pg_count;
		page_info_unlink_mute(pgi_next);
	}
	pgi_prev = pgi->prev;
	if (pgi_prev && pgi_prev->pg_free) {
		voluta_assert_gt(pgi_prev->pg_count, 0);
		npgs += pgi_prev->pg_count;
		page_info_mute(pgi);
		pgi = pgi_prev;
		pgi_prev = pgi_prev->prev;
		page_info_unlink_mute(pgi);
	}
	qmalloc_add_free(qmal, pgi, pgi_prev, npgs);
	return 0;
}


static void qmalloc_free_to_slab(struct voluta_qmalloc *qmal,
				 struct voluta_slab *slab,
				 struct voluta_slab_seg *seg)
{
	struct voluta_page_info *pgi = qmalloc_page_info_of(qmal, seg);

	voluta_assert_eq(pgi->slab_index, slab->sindex);
	slab_free(slab, seg);

	voluta_assert_le(pgi->slab_nused, pgi->slab_nelems);
	voluta_assert_gt(pgi->slab_nused, 0);
	pgi->slab_nused -= 1;
	if (!pgi->slab_nused) {
		slab_shrink(slab, pgi);
		qmalloc_free_npgs(qmal, pgi, 1);
	}
}

static int qmalloc_free_slab(struct voluta_qmalloc *qmal,
			     struct voluta_slab_seg *seg, size_t nbytes)
{
	struct voluta_slab *slab;

	slab = qmalloc_slab_of(qmal, nbytes);
	if (slab == NULL) {
		return -EINVAL;
	}
	voluta_assert_ge(slab->elemsz, nbytes);
	voluta_assert_lt(slab->elemsz / 2, nbytes);
	if (nbytes > slab->elemsz) {
		return -EINVAL;
	}
	if (nbytes <= (slab->elemsz / 2)) {
		return -EINVAL;
	}
	qmalloc_free_to_slab(qmal, slab, seg);
	return 0;
}

static int qmalloc_free_sub_pg(struct voluta_qmalloc *qmal, void *ptr,
			       size_t nbytes)
{
	struct voluta_slab_seg *seg;

	seg = qmalloc_slab_seg_of(qmal, ptr);
	return qmalloc_free_slab(qmal, seg, nbytes);
}

static int qmalloc_free_multi_pg(struct voluta_qmalloc *qmal, void *ptr,
				 size_t nbytes)
{
	size_t npgs;
	struct voluta_page_info *pgi;

	npgs = nbytes_to_npgs(nbytes);
	voluta_assert_ge(qmal->st.npages_used, npgs);

	pgi = qmalloc_page_info_of(qmal, ptr);
	voluta_assert_eq(pgi->pg_count, npgs);
	qmalloc_free_npgs(qmal, pgi, npgs);
	qmal->st.npages_used -= npgs;
	return 0;
}

static void
qmalloc_wreck_data(const struct voluta_qmalloc *qmal, void *ptr, size_t nbytes)
{
	voluta_assert_ge(qmal->st.nbytes_used, nbytes);

	if (qmal->pedantic_mode) {
		memset(ptr, 0xF3, nbytes);
	}
}

int voluta_free(struct voluta_qmalloc *qmal, void *ptr, size_t nbytes)
{
	int err;

	if ((ptr == NULL) || (nbytes == 0)) {
		return 0;
	}
	err = qmalloc_check_free(qmal, ptr, nbytes);
	if (err) {
		return err;
	}
	qmalloc_wreck_data(qmal, ptr, nbytes);
	if (slab_issize(nbytes)) {
		err = qmalloc_free_sub_pg(qmal, ptr, nbytes);
	} else {
		err = qmalloc_free_multi_pg(qmal, ptr, nbytes);
	}
	if (err) {
		return err;
	}
	qmal->st.nbytes_used -= nbytes;
	return err;
}

int voluta_zfree(struct voluta_qmalloc *qmal, void *ptr, size_t nbytes)
{
	int err = 0;

	if (ptr != NULL) {
		memset(ptr, 0, nbytes);
		err = voluta_free(qmal, ptr, nbytes);
	}
	return err;
}

int voluta_nfree(struct voluta_qmalloc *qmal, void *ptr, size_t elemsz,
		 size_t nelems)
{
	return voluta_free(qmal, ptr, elemsz * nelems);
}

int voluta_memref(const struct voluta_qmalloc *qmal, const void *ptr,
		  size_t len, struct voluta_qmref *out_qmref)
{
	int err = -ERANGE;

	if (qmalloc_isinrange(qmal, ptr, len)) {
		out_qmref->off = qmalloc_ptr_to_off(qmal, ptr);
		out_qmref->len = len;
		out_qmref->mem = (void *)ptr;
		out_qmref->fd  = qmal->memfd_data;
		err = 0;
	}
	return err;
}

void voluta_qmstat(const struct voluta_qmalloc *qmal,
		   struct voluta_qmstat *qmst)
{
	memcpy(qmst, &qmal->st, sizeof(*qmst));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t align_down(size_t sz, size_t align)
{
	return (sz / align) * align;
}

int voluta_resolve_memsize(size_t mem_want, size_t *out_memsize)
{
	int err;
	struct rlimit rlim;
	size_t mem_floor, mem_ceil, mem_rlim, mem_glim;
	size_t page_size, phys_pages, mem_total, mem_uget;
	const size_t mem_align = 4 * VOLUTA_MEGA;

	page_size = voluta_sc_page_size();
	phys_pages = voluta_sc_phys_pages();
	mem_total = (page_size * phys_pages);
	mem_floor = 32 * mem_align;
	if (mem_total < mem_floor) {
		return -ENOMEM;
	}
	err = getrlimit(RLIMIT_AS, &rlim);
	if (err) {
		return err;
	}
	mem_rlim = rlim.rlim_cur;
	if (mem_rlim < mem_floor) {
		return -ENOMEM;
	}
	mem_glim = 4 * VOLUTA_GIGA;
	mem_ceil = voluta_min3(mem_glim, mem_rlim, mem_total / 4);
	mem_uget = voluta_clamp(mem_want, mem_floor, mem_ceil);
	*out_memsize = align_down(mem_uget, mem_align);
	return 0;
}

