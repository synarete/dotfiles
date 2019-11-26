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
#include <fnxinfra.h>
#include "core-blobs.h"
#include "core-types.h"
#include "core-stats.h"
#include "core-proto.h"
#include "core-addr.h"
#include "core-elems.h"
#include "core-space.h"
#include "core-file.h"
#include "core-iobuf.h"
#include "core-inode.h"
#include "core-inodei.h"
#include "core-dir.h"
#include "core-jobsq.h"
#include "core-task.h"
#include "core-alloc.h"
#include "core-alloc-slab.h"


#define SLAB_MAGIC          (0xDEADBEEF)
/* #define SOBJ_MAGIC       (0xABADF00D) */

#define SLAB_BKREF_NOBJS    (31)
#define SLAB_BKREF_NBLKS    (32)

#define SLAB_TASK_NOBJS     (13)
#define SLAB_TASK_NBLKS     (16)

#define SLAB_SPMAP_NOBJS    (31)
#define SLAB_SPMAP_NBLKS    (32)

#define SLAB_DIR_NOBJS      (16)
#define SLAB_DIR_NBLKS      (24)

#define SLAB_DIRSEG_NOBJS   (106)
#define SLAB_DIRSEG_NBLKS   (16)

#define SLAB_INODE_NOBJS    (184)
#define SLAB_INODE_NBLKS    (16)

#define SLAB_REG_NOBJS      (42)
#define SLAB_REG_NBLKS      (16)

#define SLAB_REGSEG_NOBJS   (72)
#define SLAB_REGSEG_NBLKS   (16)

#define SLAB_REGSEC_NOBJS   (255)
#define SLAB_REGSEC_NBLKS   (16)

#define SLAB_SYMLNK_NOBJS   (26)
#define SLAB_SYMLNK_NBLKS   (16)

#define SLAB_VBK_NOBJS      (511)
#define SLAB_VBK_NBLKS      (16)


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

#define NBLKS(nbytes) \
	((nbytes + FNX_BLKSIZE - 1) / FNX_BLKSIZE)

#define SLAB_NOBJS_TO_NBLKS(type, nobjs) \
	NBLKS(sizeof(fnx_slab_t) + (sizeof(type) * nobjs))

#define CHKSINFO(type, nobjs, nblks) \
	FNX_STATICASSERT_EQ(SLAB_NOBJS_TO_NBLKS(type, nobjs), nblks)

static void fnx_check_sinfos(void)
{
	CHKSINFO(fnx_sobj_task_t, SLAB_TASK_NOBJS, SLAB_TASK_NBLKS);
	CHKSINFO(fnx_sobj_spmap_t, SLAB_SPMAP_NOBJS, SLAB_SPMAP_NBLKS);
	CHKSINFO(fnx_sobj_dir_t, SLAB_DIR_NOBJS, SLAB_DIR_NBLKS);
	CHKSINFO(fnx_sobj_dirseg_t, SLAB_DIRSEG_NOBJS, SLAB_DIRSEG_NBLKS);
	CHKSINFO(fnx_sobj_inode_t, SLAB_INODE_NOBJS, SLAB_INODE_NBLKS);
	CHKSINFO(fnx_sobj_reg_t, SLAB_REG_NOBJS, SLAB_REG_NBLKS);
	CHKSINFO(fnx_sobj_regseg_t, SLAB_REGSEG_NOBJS, SLAB_REGSEG_NBLKS);
	CHKSINFO(fnx_sobj_regsec_t, SLAB_REGSEC_NOBJS, SLAB_REGSEC_NBLKS);
	CHKSINFO(fnx_sobj_symlnk_t, SLAB_SYMLNK_NOBJS, SLAB_SYMLNK_NBLKS);
	CHKSINFO(fnx_sobj_symlnk_t, SLAB_SYMLNK_NOBJS, SLAB_SYMLNK_NBLKS);
	CHKSINFO(fnx_sobj_vbk_t, SLAB_VBK_NOBJS, SLAB_VBK_NBLKS);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

#define DEFSINFO1(type, sub, nobjs, nblks) \
	sizeof(type), offsetof(type, sub), nobjs, nblks, 1

#define DEFSINFO(type, sub, nobjs, nblks) \
	sizeof(type), offsetof(type, sub), nobjs, nblks, 0

static const fnx_sinfo_t s_sinfo_bkref = {
	DEFSINFO1(fnx_sobj_bkref_t, bkref, SLAB_BKREF_NOBJS, SLAB_BKREF_NBLKS)
};

static const fnx_sinfo_t s_sinfo_task = {
	DEFSINFO(fnx_sobj_task_t, task, SLAB_TASK_NOBJS, SLAB_TASK_NBLKS)
};

static const fnx_sinfo_t s_sinfo_spmap = {
	DEFSINFO(fnx_sobj_spmap_t, spmap, SLAB_SPMAP_NOBJS, SLAB_SPMAP_NBLKS)
};

static const fnx_sinfo_t s_sinfo_dir = {
	DEFSINFO(fnx_sobj_dir_t, dir, SLAB_DIR_NOBJS, SLAB_DIR_NBLKS)
};

static const fnx_sinfo_t s_sinfo_dirseg = {
	DEFSINFO(fnx_sobj_dirseg_t, dirseg, SLAB_DIRSEG_NOBJS, SLAB_DIRSEG_NBLKS)
};

static const fnx_sinfo_t s_sinfo_inode = {
	DEFSINFO(fnx_sobj_inode_t, inode, SLAB_INODE_NOBJS, SLAB_INODE_NBLKS)
};

static const fnx_sinfo_t s_sinfo_reg = {
	DEFSINFO(fnx_sobj_reg_t, reg, SLAB_REG_NOBJS, SLAB_REG_NBLKS)
};

static const fnx_sinfo_t s_sinfo_regseg = {
	DEFSINFO(fnx_sobj_regseg_t, regseg, SLAB_REGSEG_NOBJS, SLAB_REGSEG_NBLKS)
};

static const fnx_sinfo_t s_sinfo_regsec = {
	DEFSINFO(fnx_sobj_regsec_t, regsec, SLAB_REGSEC_NOBJS, SLAB_REGSEC_NBLKS)
};

static const fnx_sinfo_t s_sinfo_symlnk = {
	DEFSINFO(fnx_sobj_symlnk_t, symlnk, SLAB_SYMLNK_NOBJS, SLAB_SYMLNK_NBLKS)
};

static const fnx_sinfo_t s_sinfo_vbk = {
	DEFSINFO(fnx_sobj_vbk_t, vbk, SLAB_VBK_NOBJS, SLAB_VBK_NBLKS)
};

static const fnx_sinfo_t *s_sinfos[] = {
	[FNX_SPOOL_TASK]    = &s_sinfo_task,
	[FNX_SPOOL_BKREF]   = &s_sinfo_bkref,
	[FNX_SPOOL_SPMAP]   = &s_sinfo_spmap,
	[FNX_SPOOL_DIR]     = &s_sinfo_dir,
	[FNX_SPOOL_DIRSEG]  = &s_sinfo_dirseg,
	[FNX_SPOOL_INODE]   = &s_sinfo_inode,
	[FNX_SPOOL_REG]     = &s_sinfo_reg,
	[FNX_SPOOL_REGSEG]  = &s_sinfo_regseg,
	[FNX_SPOOL_REGSEC]  = &s_sinfo_regsec,
	[FNX_SPOOL_SYMLNK]  = &s_sinfo_symlnk,
	[FNX_SPOOL_VBK]     = &s_sinfo_vbk,
};


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t sinfo_prefix_nbk(const fnx_sinfo_t *sinfo)
{
	const size_t sz = (sinfo->objsz * sinfo->nobjs) + sizeof(fnx_slab_t);
	return fnx_bytes_to_nbk(sz);
}

static void fnx_verify_sinfo(const fnx_sinfo_t *sinfo)
{
	size_t nbk;

	fnx_assert(sinfo->objsz > sizeof(fnx_sobj_t));
	fnx_assert(sinfo->offset >= sizeof(fnx_sobj_t));
	fnx_assert((sinfo->offset % 8) == 0);
	fnx_assert((sinfo->nblks % 8) == 0);
	fnx_assert(sinfo->nobjs > 1);

	nbk = sinfo_prefix_nbk(sinfo);
	if (sinfo->hasexb) {
		nbk += sinfo->nobjs;
	}
	fnx_assert(nbk == sinfo->nblks);
}

void fnx_verify_sinfos(void)
{
	const fnx_sinfo_t *sinfo = NULL;

	fnx_check_sinfos();
	for (size_t i = 0; i < fnx_nelems(s_sinfos); ++i) {
		if ((sinfo = s_sinfos[i]) != NULL) {
			fnx_verify_sinfo(sinfo);
		}
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_sobj_t *link_to_sobj(const fnx_link_t *lnk)
{
	const fnx_sobj_t *sobj;

	sobj = fnx_container_of(lnk, fnx_sobj_t, lnk);
	return (fnx_sobj_t *)sobj;
}

static void sobj_setup(fnx_sobj_t *sobj, fnx_slab_t *slab, fnx_blk_t *blk)
{
	fnx_link_init(&sobj->lnk);
	sobj->blk   = blk;
	sobj->slab  = slab;
}

static void *sobj_getsub(fnx_sobj_t *sobj)
{
	fnx_byte_t *ptr = (fnx_byte_t *)sobj;
	fnx_size_t  off = sobj->slab->sinfo->offset;
	return (ptr + off);
}

static int sobj_islinked(const fnx_sobj_t *sobj)
{
	return fnx_link_islinked(&sobj->lnk);
}

static fnx_sobj_t *slab_getsobj(fnx_slab_t *slab, size_t i)
{
	fnx_byte_t *sptr;
	fnx_sobj_t *sobj;

	fnx_assert(i < slab->sinfo->nobjs);
	fnx_assert(slab->sinfo->objsz > sizeof(*sobj));
	sptr = (fnx_byte_t *)slab->smem + (i * slab->sinfo->objsz);
	sobj = (fnx_sobj_t *)((void *)sptr);
	return sobj;
}

static fnx_blk_t *slab_getblki(fnx_slab_t *slab, size_t i)
{
	size_t nbk_prefix;
	fnx_blk_t  *blk = NULL;

	if (slab->sinfo->hasexb) {
		nbk_prefix = sinfo_prefix_nbk(slab->sinfo);
		blk = (fnx_blk_t *)slab->smem + nbk_prefix + i;
	}
	return blk;
}

static void slab_setup(fnx_slab_t *slab, const fnx_sinfo_t *sinfo, void *smem)
{
	fnx_sobj_t *sobj;
	fnx_blk_t  *blki;

	slab->magic     = SLAB_MAGIC;
	slab->sinfo     = sinfo;
	slab->avail     = sinfo->nobjs;
	slab->smem      = smem;
	for (size_t i = 0; i < slab->avail; ++i) {
		sobj = slab_getsobj(slab, i);
		blki = slab_getblki(slab, i);
		sobj_setup(sobj, slab, blki);
	}
}

static int slab_isunused(const fnx_slab_t *slab)
{
	return (slab->avail == slab->sinfo->nobjs);
}

static int slab_iscold(const fnx_slab_t *slab)
{
	return (slab->avail >= (slab->sinfo->nobjs / 2));
}

static fnx_slab_t *slab_new(const fnx_sinfo_t *sinfo)
{
	void *smem;
	size_t ssz;
	fnx_byte_t *sptr;
	fnx_slab_t *slab = NULL;

	smem = fnx_mmap_bks(sinfo->nblks);
	if (smem != NULL) {
		ssz  = (sinfo->nobjs * sinfo->objsz);
		sptr = (fnx_byte_t *)smem + ssz;
		slab = (fnx_slab_t *)((void *)sptr);
		slab_setup(slab, sinfo, smem);
	}
	return slab;
}

static void slab_del(fnx_slab_t *slab)
{
	const fnx_sinfo_t *sinfo = slab->sinfo;
	fnx_munmap_bks(slab->smem, sinfo->nblks);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void spool_init(fnx_spool_t *spool, const fnx_sinfo_t *sinfo)
{
	fnx_assert(sinfo != NULL);
	fnx_list_init(&spool->spool);
	fnx_mutex_init(&spool->mutex);
	spool->sinfo = sinfo;
	spool->bkcnt = 0;
}

static void spool_destory(fnx_spool_t *spool)
{
	fnx_list_destroy(&spool->spool);
	fnx_mutex_destroy(&spool->mutex);
	spool->sinfo = NULL;
	spool->bkcnt = 0;
}

static void spool_lock(fnx_spool_t *spool)
{
	fnx_mutex_lock(&spool->mutex);
}

static void spool_unlock(fnx_spool_t *spool)
{
	fnx_mutex_unlock(&spool->mutex);
}

/*
 * FIFO-like semantics: pop the first available free slab-object; implicitly
 * makes is parent slab wormer.
 *
 * NB: We do *not* re-zero data here (which one may expect as a mimic to
 * mmap's guarantee for zeroing semantics); instead, higher layer should zero
 * unused regions.
 */
static fnx_sobj_t *spool_pop_sobj(fnx_spool_t *spool)
{
	fnx_link_t *lnk;
	fnx_sobj_t *sobj = NULL;

	lnk = fnx_list_pop_front(&spool->spool);
	if (lnk != NULL) {
		sobj = link_to_sobj(lnk);
		fnx_assert(sobj->slab->avail > 0);
		sobj->slab->avail -= 1;
	}
	return sobj;
}

/*
 * Returns unused slab's object to free queue, using a heuristic which tries to
 * be nice to the system: if it belongs to a cold slab (one which is less then
 * half used), push it to end-of-queue so that its parent slab may be evicted
 * soon. Otherwise, push it to front of queue so that it will be used for next
 * allocation.
 */
static void spool_push_sobj(fnx_spool_t *spool, fnx_sobj_t *sobj, int tail)
{
	fnx_assert(!sobj_islinked(sobj));
	fnx_assert(sobj->slab->avail < sobj->slab->sinfo->nobjs);

	sobj->slab->avail += 1;
	if (tail || slab_iscold(sobj->slab)) {
		fnx_list_push_back(&spool->spool, &sobj->lnk);
	} else {
		fnx_list_push_front(&spool->spool, &sobj->lnk);
	}
}

static void spool_push_new_core(fnx_spool_t *spool)
{
	fnx_slab_t *slab;
	fnx_sobj_t *sobj;
	const fnx_sinfo_t *sinfo = spool->sinfo;

	slab = slab_new(sinfo);
	if (slab == NULL) {
		return;
	}
	spool->bkcnt += sinfo->nblks;
	for (size_t i = 0; i < sinfo->nobjs; ++i) {
		sobj = slab_getsobj(slab, i);
		fnx_list_push_front(&spool->spool, &sobj->lnk);
	}
}

static void spool_pop_del_core(fnx_spool_t *spool, fnx_slab_t *slab)
{
	fnx_sobj_t *sobj;
	const fnx_sinfo_t *sinfo = spool->sinfo;

	fnx_assert(spool->bkcnt >= sinfo->nblks);
	for (size_t i = 0; i < sinfo->nobjs; ++i) {
		sobj = slab_getsobj(slab, i);
		fnx_assert(sobj_islinked(sobj));
		fnx_list_remove(&spool->spool, &sobj->lnk);
	}
	spool->bkcnt -= sinfo->nblks;
	slab_del(slab);
}

static int spool_isempty(const fnx_spool_t *spool)
{
	return fnx_list_isempty(&spool->spool);
}

static fnx_sobj_t *spool_pop_new_sobj(fnx_spool_t *spool)
{
	fnx_sobj_t *sobj;

	spool_lock(spool);
	if (spool_isempty(spool)) {
		spool_push_new_core(spool);
	}
	sobj = spool_pop_sobj(spool);
	spool_unlock(spool);
	return sobj;
}

static void spool_push_del_sobj(fnx_spool_t *spool, fnx_sobj_t *sobj)
{
	fnx_slab_t *slab = sobj->slab;

	spool_lock(spool);
	spool_push_sobj(spool, sobj, 0);
	if (slab_isunused(slab)) {
		spool_pop_del_core(spool, slab);
	}
	spool_unlock(spool);
}

static void spool_clear_sobjs(fnx_spool_t *spool)
{
	fnx_slab_t *slab;
	fnx_sobj_t *sobj;

	spool_lock(spool);
	while ((sobj = spool_pop_sobj(spool)) != NULL) {
		slab = sobj->slab;
		spool_push_sobj(spool, sobj, 1);
		if (slab_isunused(slab)) {
			spool_pop_del_core(spool, slab);
		}
	}
	spool_unlock(spool);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void *alloc_malloc(fnx_alloc_t *alloc, size_t sz)
{
	/* TODO: Map to better allocator */
	fnx_unused(alloc);
	return malloc(sz);
}

static void alloc_free(fnx_alloc_t *alloc, void *ptr, size_t n)
{
	fnx_assert(n > 0);
	fnx_unused2(alloc, n);
	free(ptr);
}

static void *malloc_hook(size_t sz, void *hint)
{
	return alloc_malloc((fnx_alloc_t *)hint, sz);
}

static void free_hook(void *ptr, size_t n, void *hint)
{
	alloc_free((fnx_alloc_t *)hint, ptr, n);
}

void fnx_alloc_init(fnx_alloc_t *alloc)
{
	fnx_staticassert(fnx_nelems(alloc->spools) == fnx_nelems(s_sinfos));

	fnx_mutex_init(&alloc->mutex);
	alloc->base.malloc = malloc_hook;
	alloc->base.free   = free_hook;
	alloc->base.userp  = alloc;

	/* TODO: Allow overriding by user at process' start */
	alloc->limit = (1 << 20);

	for (size_t i = 0; i < fnx_nelems(alloc->spools); ++i) {
		spool_init(&alloc->spools[i], s_sinfos[i]);
	}
}

void fnx_alloc_destroy(fnx_alloc_t *alloc)
{
	fnx_mutex_destroy(&alloc->mutex);
	for (size_t i = 0; i < fnx_nelems(alloc->spools); ++i) {
		spool_destory(&alloc->spools[i]);
	}
}

void fnx_alloc_clear(fnx_alloc_t *alloc)
{
	for (size_t i = 0; i < fnx_nelems(alloc->spools); ++i) {
		spool_clear_sobjs(&alloc->spools[i]);
	}
}

size_t fnx_alloc_total_nbk(const fnx_alloc_t *alloc)
{
	size_t total = 0;

	/* NB: no locks */
	for (size_t i = 0; i < fnx_nelems(alloc->spools); ++i) {
		total += alloc->spools[i].bkcnt;
	}
	return total;
}

static fnx_spool_t *alloc_getspool(fnx_alloc_t *alloc, fnx_spool_type_e type)
{
	fnx_assert((size_t)type < fnx_nelems(alloc->spools));
	return &alloc->spools[type];
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_sobj_t *task_to_sobj(const fnx_task_t *task)
{
	const fnx_sobj_t  *sobj;
	const fnx_sobj_task_t *sobj_task;

	sobj_task = fnx_container_of(task, fnx_sobj_task_t, task);
	sobj = &sobj_task->sobj;
	return (fnx_sobj_t *)sobj;
}

static fnx_task_t *sobj_to_task(fnx_sobj_t *sobj)
{
	return (fnx_task_t *)sobj_getsub(sobj);
}

static fnx_task_t *alloc_new_task(fnx_alloc_t *alloc)
{
	fnx_sobj_t  *sobj;
	fnx_task_t  *task  = NULL;
	fnx_spool_t *spool = alloc_getspool(alloc, FNX_SPOOL_TASK);

	if ((sobj = spool_pop_new_sobj(spool)) != NULL) {
		task = sobj_to_task(sobj);
	}
	return task;
}

static void alloc_del_task(fnx_alloc_t *alloc, fnx_task_t *task)
{
	fnx_sobj_t  *sobj  = task_to_sobj(task);
	fnx_spool_t *spool = alloc_getspool(alloc, FNX_SPOOL_TASK);

	spool_push_del_sobj(spool, sobj);
}

fnx_task_t *fnx_alloc_new_task(fnx_alloc_t *alloc, int opc)
{
	fnx_task_t *task;

	task = alloc_new_task(alloc);
	if (task != NULL) {
		fnx_task_init(task, opc);
	}
	return task;
}

void fnx_alloc_del_task(fnx_alloc_t *alloc, fnx_task_t *task)
{
	fnx_task_destroy(task);
	alloc_del_task(alloc, task);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_sobj_t *bkref_to_sobj(const fnx_bkref_t *bkref)
{
	const fnx_sobj_t *sobj;
	const fnx_sobj_bkref_t *sobj_bkref;

	sobj_bkref = fnx_container_of(bkref, fnx_sobj_bkref_t, bkref);
	sobj = &sobj_bkref->sobj;
	return (fnx_sobj_t *)sobj;
}

static fnx_blk_t *bkref_get_sblk(const fnx_bkref_t *bkref)
{
	fnx_sobj_t *sobj;

	sobj = bkref_to_sobj(bkref);
	fnx_assert(sobj->blk != NULL);
	return sobj->blk;
}

static fnx_bkref_t *alloc_new_bkref(fnx_alloc_t *alloc)
{
	fnx_sobj_t  *sobj;
	fnx_bkref_t *bkref = NULL;
	fnx_spool_t *spool = alloc_getspool(alloc, FNX_SPOOL_BKREF);

	sobj = spool_pop_new_sobj(spool);
	if (sobj != NULL) {
		bkref = (fnx_bkref_t *)sobj_getsub(sobj);
	}
	return bkref;
}

static void alloc_del_bkref(fnx_alloc_t *alloc, fnx_bkref_t *bkref)
{
	fnx_sobj_t  *sobj;
	fnx_spool_t *spool = alloc_getspool(alloc, FNX_SPOOL_BKREF);

	sobj  = bkref_to_sobj(bkref);
	spool_push_del_sobj(spool, sobj);
}

fnx_bkref_t *fnx_alloc_new_bk(fnx_alloc_t *alloc, const fnx_baddr_t *baddr)
{
	fnx_blk_t *blk;
	fnx_bkref_t *bkref;

	bkref = alloc_new_bkref(alloc);
	if (bkref != NULL) {
		blk = bkref_get_sblk(bkref);
		fnx_bkref_init(bkref);
		fnx_bkref_setup(bkref, baddr, blk);
	}
	return bkref;
}

void fnx_alloc_del_bk(fnx_alloc_t *alloc, fnx_bkref_t *bkref)
{
	if (bkref != NULL) {
		fnx_bkref_destroy(bkref);
		alloc_del_bkref(alloc, bkref);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_sobj_t *spmap_to_sobj(fnx_spmap_t *spmap)
{
	fnx_sobj_spmap_t *sobj_spmap;

	sobj_spmap = fnx_container_of(spmap, fnx_sobj_spmap_t, spmap);
	return &sobj_spmap->sobj;
}

static fnx_spmap_t *sobj_to_spmap(fnx_sobj_t *sobj)
{
	return (fnx_spmap_t *)sobj_getsub(sobj);
}

static fnx_vnode_t *alloc_new_vspmap(fnx_alloc_t *alloc)
{
	fnx_sobj_t  *sobj;
	fnx_spmap_t *spmap;
	fnx_vnode_t *vnode = NULL;
	fnx_spool_t *spool = alloc_getspool(alloc, FNX_SPOOL_SPMAP);

	if ((sobj = spool_pop_new_sobj(spool)) != NULL) {
		spmap = sobj_to_spmap(sobj);
		vnode = &spmap->sm_vnode;
	}
	return vnode;
}

static void alloc_del_vspmap(fnx_alloc_t *alloc, fnx_vnode_t *vnode)
{
	fnx_spmap_t *spmap = fnx_vnode_to_spmap(vnode);
	fnx_spool_t *spool = alloc_getspool(alloc, FNX_SPOOL_SPMAP);
	spool_push_del_sobj(spool, spmap_to_sobj(spmap));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_sobj_t *inode_to_sobj(fnx_inode_t *inode)
{
	fnx_sobj_inode_t *sobj_inode;

	sobj_inode = fnx_container_of(inode, fnx_sobj_inode_t, inode);
	return &sobj_inode->sobj;
}

static fnx_inode_t *sobj_to_inode(fnx_sobj_t *sobj)
{
	return (fnx_inode_t *)sobj_getsub(sobj);
}

static fnx_vnode_t *alloc_new_vinode(fnx_alloc_t *alloc)
{
	fnx_sobj_t  *sobj;
	fnx_inode_t *inode;
	fnx_vnode_t *vnode = NULL;
	fnx_spool_t *spool = alloc_getspool(alloc, FNX_SPOOL_INODE);

	if ((sobj = spool_pop_new_sobj(spool)) != NULL) {
		inode = sobj_to_inode(sobj);
		vnode = &inode->i_vnode;
	}
	return vnode;
}

static void alloc_del_vinode(fnx_alloc_t *alloc, fnx_vnode_t *vnode)
{
	fnx_inode_t *inode = fnx_vnode_to_inode(vnode);
	fnx_spool_t *spool = alloc_getspool(alloc, FNX_SPOOL_INODE);

	spool_push_del_sobj(spool, inode_to_sobj(inode));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_sobj_t *dir_to_sobj(fnx_dir_t *dir)
{
	fnx_sobj_dir_t *sobj_dir;

	sobj_dir = fnx_container_of(dir, fnx_sobj_dir_t, dir);
	return &sobj_dir->sobj;
}

static fnx_dir_t *sobj_to_dir(fnx_sobj_t *sobj)
{
	return (fnx_dir_t *)sobj_getsub(sobj);
}

static fnx_vnode_t *alloc_new_vdir(fnx_alloc_t *alloc)
{
	fnx_sobj_t  *sobj;
	fnx_dir_t   *dir;
	fnx_vnode_t *vnode = NULL;
	fnx_spool_t *spool = alloc_getspool(alloc, FNX_SPOOL_DIR);

	if ((sobj = spool_pop_new_sobj(spool)) != NULL) {
		dir = sobj_to_dir(sobj);
		vnode = &dir->d_inode.i_vnode;
	}
	return vnode;
}

static void alloc_del_vdir(fnx_alloc_t *alloc, fnx_vnode_t *vnode)
{
	fnx_dir_t *dir = fnx_vnode_to_dir(vnode);
	fnx_spool_t *spool = alloc_getspool(alloc, FNX_SPOOL_DIR);
	spool_push_del_sobj(spool, dir_to_sobj(dir));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_sobj_t *dirseg_to_sobj(fnx_dirseg_t *dirseg)
{
	fnx_sobj_dirseg_t *sobj_dirseg;

	sobj_dirseg = fnx_container_of(dirseg, fnx_sobj_dirseg_t, dirseg);
	return &sobj_dirseg->sobj;
}

static fnx_dirseg_t *sobj_to_dirseg(fnx_sobj_t *sobj)
{
	return (fnx_dirseg_t *)sobj_getsub(sobj);
}

static fnx_vnode_t *alloc_new_vdirseg(fnx_alloc_t *alloc)
{
	fnx_sobj_t   *sobj;
	fnx_dirseg_t *dirseg;
	fnx_vnode_t  *vnode = NULL;
	fnx_spool_t  *spool = alloc_getspool(alloc, FNX_SPOOL_DIRSEG);

	if ((sobj = spool_pop_new_sobj(spool)) != NULL) {
		dirseg = sobj_to_dirseg(sobj);
		vnode = &dirseg->ds_vnode;
	}
	return vnode;
}

static void alloc_del_vdirseg(fnx_alloc_t *alloc, fnx_vnode_t *vnode)
{
	fnx_dirseg_t *dirseg = fnx_vnode_to_dirseg(vnode);
	fnx_spool_t  *spool  = alloc_getspool(alloc, FNX_SPOOL_DIRSEG);
	spool_push_del_sobj(spool, dirseg_to_sobj(dirseg));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_sobj_t *reg_to_sobj(fnx_reg_t *reg)
{
	fnx_sobj_reg_t *sobj_reg;

	sobj_reg = fnx_container_of(reg, fnx_sobj_reg_t, reg);
	return &sobj_reg->sobj;
}

static fnx_reg_t *sobj_to_reg(fnx_sobj_t *sobj)
{
	return (fnx_reg_t *)sobj_getsub(sobj);
}

static fnx_vnode_t *alloc_new_vreg(fnx_alloc_t *alloc)
{
	fnx_sobj_t  *sobj;
	fnx_reg_t   *reg;
	fnx_vnode_t *vnode = NULL;
	fnx_spool_t *spool = alloc_getspool(alloc, FNX_SPOOL_REG);

	if ((sobj = spool_pop_new_sobj(spool)) != NULL) {
		reg = sobj_to_reg(sobj);
		vnode = &reg->r_inode.i_vnode;
	}
	return vnode;
}

static void alloc_del_vreg(fnx_alloc_t *alloc, fnx_vnode_t *vnode)
{
	fnx_reg_t  *reg = fnx_vnode_to_reg(vnode);
	fnx_spool_t *spool = alloc_getspool(alloc, FNX_SPOOL_REG);

	spool_push_del_sobj(spool, reg_to_sobj(reg));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_sobj_t *regseg_to_sobj(fnx_regseg_t *regseg)
{
	fnx_sobj_regseg_t *sobj_regseg;

	sobj_regseg = fnx_container_of(regseg, fnx_sobj_regseg_t, regseg);
	return &sobj_regseg->sobj;
}

static fnx_regseg_t *sobj_to_regseg(fnx_sobj_t *sobj)
{
	return (fnx_regseg_t *)sobj_getsub(sobj);
}

static fnx_vnode_t *alloc_new_vregseg(fnx_alloc_t *alloc)
{
	fnx_sobj_t   *sobj;
	fnx_regseg_t *regseg;
	fnx_vnode_t  *vnode = NULL;
	fnx_spool_t  *spool = alloc_getspool(alloc, FNX_SPOOL_REGSEG);

	if ((sobj = spool_pop_new_sobj(spool)) != NULL) {
		regseg = sobj_to_regseg(sobj);
		vnode  = &regseg->rs_vnode;
	}
	return vnode;
}

static void alloc_del_vregseg(fnx_alloc_t *alloc, fnx_vnode_t *vnode)
{
	fnx_regseg_t *regseg = fnx_vnode_to_regseg(vnode);
	fnx_spool_t  *spool  = alloc_getspool(alloc, FNX_SPOOL_REGSEG);

	spool_push_del_sobj(spool, regseg_to_sobj(regseg));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_sobj_t *regsec_to_sobj(fnx_regsec_t *regsec)
{
	fnx_sobj_regsec_t *sobj_regsec;

	sobj_regsec = fnx_container_of(regsec, fnx_sobj_regsec_t, regsec);
	return &sobj_regsec->sobj;
}

static fnx_regsec_t *sobj_to_regsec(fnx_sobj_t *sobj)
{
	return (fnx_regsec_t *)sobj_getsub(sobj);
}

static fnx_vnode_t *alloc_new_vregsec(fnx_alloc_t *alloc)
{
	fnx_sobj_t   *sobj;
	fnx_regsec_t *regsec;
	fnx_vnode_t  *vnode = NULL;
	fnx_spool_t  *spool = alloc_getspool(alloc, FNX_SPOOL_REGSEC);

	if ((sobj = spool_pop_new_sobj(spool)) != NULL) {
		regsec = sobj_to_regsec(sobj);
		vnode  = &regsec->rc_vnode;
	}
	return vnode;
}

static void alloc_del_vregsec(fnx_alloc_t *alloc, fnx_vnode_t *vnode)
{
	fnx_regsec_t *regsec = fnx_vnode_to_regsec(vnode);
	fnx_spool_t  *spool  = alloc_getspool(alloc, FNX_SPOOL_REGSEC);

	spool_push_del_sobj(spool, regsec_to_sobj(regsec));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_sobj_t *symlnk_to_sobj(fnx_symlnk_t *symlnk)
{
	fnx_sobj_symlnk_t *sobj_symlnk;

	sobj_symlnk = fnx_container_of(symlnk, fnx_sobj_symlnk_t, symlnk);
	return &sobj_symlnk->sobj;
}

static fnx_symlnk_t *sobj_to_symlnk(fnx_sobj_t *sobj)
{
	return (fnx_symlnk_t *)sobj_getsub(sobj);
}

static fnx_vnode_t *alloc_new_vsymlnk(fnx_alloc_t *alloc)
{
	fnx_sobj_t   *sobj;
	fnx_symlnk_t *symlnk;
	fnx_vnode_t  *vnode = NULL;
	fnx_spool_t  *spool = alloc_getspool(alloc, FNX_SPOOL_SYMLNK);

	if ((sobj = spool_pop_new_sobj(spool)) != NULL) {
		symlnk = sobj_to_symlnk(sobj);
		vnode  = &symlnk->sl_inode.i_vnode;
	}
	return vnode;
}

static void alloc_del_vsymlnk(fnx_alloc_t *alloc, fnx_vnode_t *vnode)
{
	fnx_symlnk_t *symlnk = fnx_vnode_to_symlnk(vnode);
	fnx_spool_t  *spool  = alloc_getspool(alloc, FNX_SPOOL_SYMLNK);
	spool_push_del_sobj(spool, symlnk_to_sobj(symlnk));
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_sobj_t *vbk_to_sobj(fnx_vnode_t *vbk)
{
	fnx_sobj_vbk_t *sobj_vbk;

	sobj_vbk = fnx_container_of(vbk, fnx_sobj_vbk_t, vbk);
	return &sobj_vbk->sobj;
}

static fnx_vnode_t *sobj_to_vbk(fnx_sobj_t *sobj)
{
	return (fnx_vnode_t *)sobj_getsub(sobj);
}

static fnx_vnode_t *alloc_new_vvbk(fnx_alloc_t *alloc)
{
	fnx_sobj_t   *sobj;
	fnx_vnode_t  *vnode = NULL;
	fnx_spool_t  *spool = alloc_getspool(alloc, FNX_SPOOL_VBK);

	if ((sobj = spool_pop_new_sobj(spool)) != NULL) {
		vnode = sobj_to_vbk(sobj);
	}
	return vnode;
}

static void alloc_del_vvbk(fnx_alloc_t *alloc, fnx_vnode_t *vbk)
{
	fnx_spool_t  *spool  = alloc_getspool(alloc, FNX_SPOOL_VBK);
	spool_push_del_sobj(spool, vbk_to_sobj(vbk));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_vnode_t *fnx_alloc_new_vobj(fnx_alloc_t *alloc, fnx_vtype_e vtype)
{
	fnx_vnode_t *vnode;
	const fnx_vinfo_t *vinfo;

	/* Allocate raw memory */
	vinfo = fnx_get_vinfo(vtype);
	switch (vtype) {
		case FNX_VTYPE_SPMAP:
			vnode = alloc_new_vspmap(alloc);
			break;
		case FNX_VTYPE_DIR:
			vnode = alloc_new_vdir(alloc);
			break;
		case FNX_VTYPE_DIRSEG:
			vnode = alloc_new_vdirseg(alloc);
			break;
		case FNX_VTYPE_REG:
			vnode = alloc_new_vreg(alloc);
			break;
		case FNX_VTYPE_REGSEG:
			vnode = alloc_new_vregseg(alloc);
			break;
		case FNX_VTYPE_REGSEC:
			vnode = alloc_new_vregsec(alloc);
			break;
		case FNX_VTYPE_SYMLNK1:
		case FNX_VTYPE_SYMLNK2:
		case FNX_VTYPE_SYMLNK3:
			vnode = alloc_new_vsymlnk(alloc);
			break;
		case FNX_VTYPE_CHRDEV:
		case FNX_VTYPE_BLKDEV:
		case FNX_VTYPE_SOCK:
		case FNX_VTYPE_FIFO:
		case FNX_VTYPE_REFLNK:
			fnx_assert(vinfo->size == sizeof(fnx_inode_t));
			vnode = alloc_new_vinode(alloc);
			break;
		case FNX_VTYPE_VBK:
			vnode = alloc_new_vvbk(alloc);
			break;
		case FNX_VTYPE_SUPER:
			vnode = alloc_malloc(alloc, vinfo->size);
			break;
		case FNX_VTYPE_NONE:
		case FNX_VTYPE_EMPTY:
		case FNX_VTYPE_ANY:
		default:
			vnode = NULL;
			break;
	}

	if (vnode == NULL) {
		fnx_error("no-new-vnode vtype=%d", vtype);
		fnx_assert(0); /* XXX */
		return NULL;
	}

	/* Initialize via hook */
	vinfo->init(vnode);
	return vnode;
}

fnx_vnode_t *fnx_alloc_new_vobj2(fnx_alloc_t *alloc, const fnx_vaddr_t *vaddr)
{
	fnx_vnode_t *vnode;

	vnode = fnx_alloc_new_vobj(alloc, vaddr->vtype);
	if (vnode != NULL) {
		fnx_vnode_setvaddr(vnode, vaddr);
	}
	return vnode;
}

void fnx_alloc_del_vobj(fnx_alloc_t *alloc, fnx_vnode_t *vnode)
{
	fnx_vtype_e vtype;
	const fnx_vinfo_t *vinfo;

	if (vnode == NULL) {
		return;
	}
	/* Destroy via hook */
	vtype = fnx_vnode_vtype(vnode);
	vinfo = fnx_get_vinfo(vtype);
	vinfo->destroy(vnode);

	/* Deallocate raw memory */
	switch (vtype) {
		case FNX_VTYPE_SPMAP:
			alloc_del_vspmap(alloc, vnode);
			break;
		case FNX_VTYPE_DIR:
			alloc_del_vdir(alloc, vnode);
			break;
		case FNX_VTYPE_DIRSEG:
			alloc_del_vdirseg(alloc, vnode);
			break;
		case FNX_VTYPE_REG:
			alloc_del_vreg(alloc, vnode);
			break;
		case FNX_VTYPE_REGSEG:
			alloc_del_vregseg(alloc, vnode);
			break;
		case FNX_VTYPE_REGSEC:
			alloc_del_vregsec(alloc, vnode);
			break;
		case FNX_VTYPE_SYMLNK1:
		case FNX_VTYPE_SYMLNK2:
		case FNX_VTYPE_SYMLNK3:
			alloc_del_vsymlnk(alloc, vnode);
			break;
		case FNX_VTYPE_CHRDEV:
		case FNX_VTYPE_BLKDEV:
		case FNX_VTYPE_SOCK:
		case FNX_VTYPE_FIFO:
		case FNX_VTYPE_REFLNK:
			fnx_assert(vinfo->size == sizeof(fnx_inode_t));
			alloc_del_vinode(alloc, vnode);
			break;
		case FNX_VTYPE_VBK:
			alloc_del_vvbk(alloc, vnode);
			break;
		case FNX_VTYPE_SUPER:
			alloc_free(alloc, vnode, vinfo->size);
			break;
		case FNX_VTYPE_NONE:
		case FNX_VTYPE_EMPTY:
		case FNX_VTYPE_ANY:
		default:
			vnode = NULL;
			break;
	}
}
