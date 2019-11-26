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
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <limits.h>
#include "voluta-lib.h"

/* Allocation hint */
enum voluta_space_alloc_mode {
	VOLUTA_SP_TAKE = 1,
	VOLUTA_SP_GIVE = 2,
};

/* Local functions */
static void discard_if_cached_bk(struct voluta_env *,
				 struct voluta_bk_info *,
				 const struct voluta_vaddr *);

static const struct voluta_vaddr *
vaddr_of_vi(const struct voluta_vnode_info *vi)
{
	return &vi->vaddr;
}

static const struct voluta_vaddr *
vaddr_of_ii(const struct voluta_inode_info *ii)
{
	return vaddr_of_vi(&ii->vi);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void sbi_init_commons(struct voluta_sb_info *sbi,
			     struct voluta_cache *cache,
			     struct voluta_bdev *bdev)
{
	voluta_uuid_generate(&sbi->s_fs_uuid);
	voluta_itable_init(&sbi->s_itable);
	voluta_ag_space_init(&sbi->s_ag_space);
	sbi->s_owner = getuid();
	sbi->s_name = "voluta";
	sbi->s_birth_time = time(NULL);
	sbi->s_cache = cache;
	sbi->s_qmal = cache->qmal;
	sbi->s_bdev = bdev;
}

static void sbi_fini_commons(struct voluta_sb_info *sbi)
{
	voluta_itable_fini(&sbi->s_itable);
	voluta_ag_space_fini(&sbi->s_ag_space);
	sbi->s_name = NULL;
	sbi->s_owner = (uid_t)(-1);
	sbi->s_cache = NULL;
	sbi->s_qmal = NULL;
}

static void sbi_init_sp_stat(struct voluta_sb_info *sbi)
{
	sbi->s_sp_stat.sp_nbytes = 0;
	sbi->s_sp_stat.sp_nmeta = 0;
	sbi->s_sp_stat.sp_ndata = 0;
	sbi->s_sp_stat.sp_nfiles = 0;
}

static void sbi_fini_sp_stat(struct voluta_sb_info *sbi)
{
	sbi->s_sp_stat.sp_nbytes = 0;
	sbi->s_sp_stat.sp_nmeta = ULONG_MAX;
	sbi->s_sp_stat.sp_ndata = ULONG_MAX;
	sbi->s_sp_stat.sp_nfiles = ULONG_MAX;
}

static int sbi_init_meta_bks(struct voluta_sb_info *sbi)
{
	int err;
	void *sb;
	struct voluta_qmalloc *qmal = sbi->s_cache->qmal;

	err = voluta_zalloc(qmal, sizeof(*sbi->sb), &sb);
	if (err) {
		return err;
	}
	sbi->sb = sb;
	return 0;
}

static void sbi_fini_meta_bks(struct voluta_sb_info *sbi)
{
	struct voluta_qmalloc *qmal = sbi->s_cache->qmal;

	voluta_free(qmal, sbi->sb, sizeof(*sbi->sb));
	sbi->sb = NULL;
}

int voluta_sbi_init(struct voluta_sb_info *sbi,
		    struct voluta_cache *cache, struct voluta_bdev *bdev)
{
	sbi_init_commons(sbi, cache, bdev);
	sbi_init_sp_stat(sbi);
	return sbi_init_meta_bks(sbi);
}

void voluta_sbi_fini(struct voluta_sb_info *sbi)
{
	sbi_fini_meta_bks(sbi);
	sbi_fini_sp_stat(sbi);
	sbi_fini_commons(sbi);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool isdata(const struct voluta_vaddr *vaddr)
{
	return (vaddr->vtype == VOLUTA_VTYPE_DATA);
}

static bool isinode(const struct voluta_vaddr *vaddr)
{
	return (vaddr->vtype == VOLUTA_VTYPE_INODE);
}

static int update_space_stat(struct voluta_sb_info *sbi,
			     const struct voluta_vaddr *vaddr,
			     enum voluta_space_alloc_mode mode)
{
	const size_t nb = vaddr->len;
	struct voluta_sp_stat *sps = &sbi->s_sp_stat;

	voluta_assert_gt(vaddr->len, 0);
	voluta_assert_ne(vaddr->vtype, VOLUTA_VTYPE_NONE);

	if (mode == VOLUTA_SP_TAKE) {
		if (isdata(vaddr)) {
			if ((sps->sp_ndata + nb) > sps->sp_nbytes) {
				return -ENOSPC;
			}
			sps->sp_ndata += nb;
		} else {
			if ((sps->sp_nmeta + nb) > sps->sp_nbytes) {
				return -ENOSPC;
			}
			if (isinode(vaddr)) {
				if (sps->sp_nfiles > (sps->sp_nbytes / 2)) {
					return -ENOSPC;
				}
				sps->sp_nfiles += 1;
			}
			sps->sp_nmeta += nb;
		}
	} else {
		if (isdata(vaddr)) {
			sps->sp_ndata -= nb;
		} else {
			if (isinode(vaddr)) {
				sps->sp_nfiles -= 1;
			}
			sps->sp_nmeta -= nb;
		}
	}
	voluta_assert_le(sps->sp_ndata, sps->sp_nbytes);
	voluta_assert_le(sps->sp_nmeta, sps->sp_nbytes);
	voluta_assert_le(sps->sp_nmeta + sps->sp_ndata, sps->sp_nbytes);
	voluta_assert_lt(sps->sp_nfiles, sps->sp_nbytes);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int check_stable_vaddr(const struct voluta_bk_info *uber_bki,
			      const struct voluta_vaddr *vaddr)
{
	enum voluta_vtype vtype;

	vtype = voluta_vtype_at(uber_bki, vaddr);
	return (vtype == vaddr->vtype) ? 0 : -EFSCORRUPTED;
}

int voluta_stage_vbk(struct voluta_sb_info *sbi,
		     const struct voluta_vaddr *vaddr, bool stable,
		     struct voluta_bk_info **out_bki)
{
	int err;
	struct voluta_bk_info *bki, *uber_bki;

	err = voluta_stage_uber_of(sbi, vaddr, &uber_bki);
	if (err) {
		return err;
	}
	if (stable) {
		err = check_stable_vaddr(uber_bki, vaddr);
		if (err) {
			return err;
		}
	}
	err = voluta_stage_bk_of(sbi, vaddr, uber_bki, &bki);
	if (err) {
		return err;
	}
	if (stable) {
		err = voluta_verify_vbk(bki, vaddr);
		if (err) {
			return err;
		}
	}
	*out_bki = bki;
	return 0;
}

static int flush_ndirty(struct voluta_sb_info *sbi, size_t limit)
{
	int err;
	struct voluta_bk_info *bki;

	for (size_t i = 0; i < limit; ++i) {
		/* TODO: Separate flush of data from meta based on flags */
		bki = voluta_first_dirty_bk(sbi->s_cache);
		if (bki == NULL) {
			break;
		}
		err = voluta_bdev_save(sbi->s_bdev, bki);
		if (err) {
			return err;
		}
		voluta_undirtify_bk(bki);
	}
	return 0;
}

static size_t dirty_count_of(const struct voluta_cache *cache, int flags)
{
	size_t dirty_count = 0;
	const struct voluta_dirtyq *dirtyq = &cache->dirtyq;

	if (!(flags & (VOLUTA_F_DATA | VOLUTA_F_META))) {
		dirty_count = dirtyq->data_size + dirtyq->meta_size;
	} else {
		if (flags & VOLUTA_F_DATA) {
			dirty_count += dirtyq->data_size;
		}
		if (flags & VOLUTA_F_META) {
			dirty_count += dirtyq->meta_size;
		}
	}
	return dirty_count;
}

int voluta_commit_dirtyq(struct voluta_sb_info *sbi, int flags)
{
	int err;
	size_t dirty_count;

	dirty_count = dirty_count_of(sbi->s_cache, flags);
	flags &= ~(VOLUTA_F_DATA | VOLUTA_F_META);
	if (!flags && (dirty_count < 512)) { /* XXX urrr... */
		return 0;
	}
	err = flush_ndirty(sbi, dirty_count);
	if (err) {
		return err;
	}
	if (flags & VOLUTA_F_SYNC) {
		err = voluta_sync_volume(sbi->s_bdev, true);
	}
	return err;
}

struct voluta_sb_info *voluta_sbi_of(const struct voluta_env *env)
{
	const struct voluta_sb_info *sbi = &env->sbi;

	return (struct voluta_sb_info *)sbi;
}

struct voluta_super_block *voluta_sb_of(const struct voluta_env *env)
{
	const struct voluta_sb_info *sbi = voluta_sbi_of(env);

	return sbi->sb;
}

struct voluta_cache *voluta_cache_of(const struct voluta_env *env)
{
	const struct voluta_cache *cache = &env->cache;

	return (struct voluta_cache *)cache;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void update_specific_view(struct voluta_vnode_info *vi)
{
	const enum voluta_vtype vtype = vi->vaddr.vtype;

	if (vtype == VOLUTA_VTYPE_DATA) {
		vi->u.df = &vi->lview->data_frg;
	} else if (vtype == VOLUTA_VTYPE_RTNODE) {
		vi->u.rtn = &vi->lview->radix_tnode;
	} else if (vtype == VOLUTA_VTYPE_DTNODE) {
		vi->u.dtn = &vi->lview->dir_tnode;
	} else if (vtype == VOLUTA_VTYPE_ITNODE) {
		vi->u.itn = &vi->lview->itable_tnode;
	} else if (vtype == VOLUTA_VTYPE_XANODE) {
		vi->u.xan = &vi->lview->xattr_node;
	}
}

static void bind_vnode_to_bk(struct voluta_vnode_info *vi,
			     struct voluta_bk_info *bki)
{
	voluta_resolve_lview(bki, vaddr_of_vi(vi), &vi->lview);
	update_specific_view(vi);
	voluta_attach_vi(vi, bki);
}

static void bind_inode_to_bk(struct voluta_inode_info *ii,
			     struct voluta_bk_info *bki)
{
	bind_vnode_to_bk(&ii->vi, bki);
	ii->inode = &ii->vi.lview->inode;
}

static int stage_inode(struct voluta_sb_info *sbi,
		       const struct voluta_iaddr *iaddr, bool stable,
		       struct voluta_inode_info **out_ii)
{
	int err;
	struct voluta_bk_info *bki;

	err = voluta_stage_vbk(sbi, &iaddr->vaddr, stable, &bki);
	if (err) {
		return err;
	}
	err = voluta_cache_spawn_ii(sbi->s_cache, iaddr, out_ii);
	if (err) {
		return err;
	}
	bind_inode_to_bk(*out_ii, bki);
	return 0;
}

static int stage_vnode(struct voluta_sb_info *sbi,
		       const struct voluta_vaddr *vaddr, bool stable,
		       struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_bk_info *bki;

	err = voluta_stage_vbk(sbi, vaddr, stable, &bki);
	if (err) {
		return err;
	}
	err = voluta_cache_spawn_vi(sbi->s_cache, vaddr, out_vi);
	if (err) {
		return err;
	}
	bind_vnode_to_bk(*out_vi, bki);
	return 0;
}

int voluta_stage_inode(struct voluta_env *env, ino_t xino,
		       struct voluta_inode_info **out_ii)
{
	int err;
	ino_t ino;
	struct voluta_iaddr iaddr;

	err = voluta_real_ino(env, xino, &ino);
	if (err) {
		return err;
	}
	err = voluta_cache_lookup_ii(cache_of(env), ino, out_ii);
	if (!err) {
		return 0; /* Cache hit! */
	}
	err = voluta_resolve_ino(env, ino, &iaddr);
	if (err) {
		return err;
	}
	err = stage_inode(&env->sbi, &iaddr, true, out_ii);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_stage_vnode(struct voluta_sb_info *sbi,
		       const struct voluta_vaddr *vaddr,
		       struct voluta_vnode_info **out_vi)
{
	int err;

	err = voluta_cache_lookup_vi(sbi->s_cache, vaddr, out_vi);
	if (!err) {
		return 0; /* Cache hit! */
	}
	err = stage_vnode(sbi, vaddr, true, out_vi);
	if (err) {
		return err;
	}
	return 0;
}

static bool has_falsely_vtype(const struct voluta_bk_info *uber_bki,
			      const struct voluta_vaddr *vaddr)
{
	enum voluta_vtype vtype;

	vtype = voluta_vtype_at(uber_bki, vaddr);
	return (vtype == VOLUTA_VTYPE_NONE);
}

static int discard_if_falsely_bk(struct voluta_sb_info *sbi,
				 struct voluta_bk_info *bki)
{
	int err;
	struct voluta_bk_info *uber_bki = NULL;
	const struct voluta_vaddr *vaddr = &bki->b_vaddr;

	err = voluta_stage_uber_of(sbi, vaddr, &uber_bki);
	if (err) {
		return err;
	}
	if (has_falsely_vtype(uber_bki, vaddr)) {
		voluta_cache_forget_bk(sbi->s_cache, bki);
	}
	return 0;
}

int voluta_nkbs_of(enum voluta_vtype vtype, mode_t mode, size_t *out_nkbs)
{
	int err;
	size_t psize = 0;

	err = voluta_psize_of(vtype, mode, &psize);
	if (!err) {
		*out_nkbs = voluta_size_to_nkbs(psize);
	}
	return err;
}

/* TODO: cleanups and resource reclaim upon failure in every path */
int voluta_new_inode(struct voluta_env *env, mode_t mode,
		     ino_t parent_ino, struct voluta_inode_info **out_ii)
{
	int err;
	size_t nkbs;
	const enum voluta_vtype vtype = VOLUTA_VTYPE_INODE;
	struct voluta_iaddr iaddr;
	struct voluta_inode_info *ii;
	struct voluta_sb_info *sbi = &env->sbi;

	err = voluta_nkbs_of(vtype, mode, &nkbs);
	if (err) {
		return err;
	}
	err = voluta_allocate_space(sbi, vtype, nkbs, &iaddr.vaddr);
	if (err) {
		return err;
	}
	err = update_space_stat(sbi, &iaddr.vaddr, VOLUTA_SP_TAKE);
	if (err) {
		return err;
	}
	err = voluta_acquire_ino(env, &iaddr);
	if (err) {
		return err;
	}
	err = stage_inode(sbi, &iaddr, false, &ii);
	if (err) {
		/* TODO: spfree inode from ag */
		return err;
	}
	err = voluta_setup_new_inode(env, ii, mode, parent_ino);
	if (err) {
		/* TODO: spfree inode from ag */
		return err;
	}
	i_dirtify(ii);

	*out_ii = ii;
	return 0;
}

/* TODO: cleanups and resource reclaim upon failure in every path */
int voluta_new_vnode(struct voluta_sb_info *sbi, enum voluta_vtype vtype,
		     struct voluta_vnode_info **out_vi)
{
	int err;
	size_t nkbs;
	struct voluta_vaddr vaddr;
	struct voluta_vnode_info *vi;

	err = voluta_nkbs_of(vtype, 0, &nkbs);
	if (err) {
		return err;
	}
	err = voluta_allocate_space(sbi, vtype, nkbs, &vaddr);
	if (err) {
		return err;
	}
	err = update_space_stat(sbi, &vaddr, VOLUTA_SP_TAKE);
	if (err) {
		return err;
	}
	err = stage_vnode(sbi, &vaddr, false, &vi);
	if (err) {
		/* TODO: spfree inode from ag */
		return err;
	}
	voluta_stamp_vnode(vi);
	v_dirtify(vi);

	*out_vi = vi;
	return 0;
}

int voluta_del_inode(struct voluta_env *env, struct voluta_inode_info *ii)
{
	int err;
	struct voluta_bk_info *bki = ii->vi.bki;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_vaddr *vaddr = vaddr_of_ii(ii);

	err = update_space_stat(sbi, vaddr, VOLUTA_SP_GIVE);
	if (err) {
		return err;
	}
	err = voluta_discard_ino(env, i_ino_of(ii));
	if (err) {
		return err;
	}
	err = voluta_deallocate_space(sbi, vaddr);
	if (err) {
		return err;
	}
	voulta_cache_forget_ii(sbi->s_cache, ii);
	err = discard_if_falsely_bk(sbi, bki);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_del_vnode(struct voluta_env *env, struct voluta_vnode_info *vi)
{
	int err;
	struct voluta_bk_info *bki = vi->bki;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_vaddr *vaddr = vaddr_of_vi(vi);

	err = update_space_stat(sbi, vaddr, VOLUTA_SP_GIVE);
	if (err) {
		return err;
	}
	err = voluta_deallocate_space(sbi, vaddr);
	if (err) {
		return err;
	}
	voulta_cache_forget_vi(sbi->s_cache, vi);
	err = discard_if_falsely_bk(sbi, bki);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_new_bspace(struct voluta_sb_info *sbi, enum voluta_vtype vtype,
		      struct voluta_vaddr *out_vaddr)
{
	int err;

	err = voluta_allocate_space(sbi, vtype, VOLUTA_NKBS_IN_BK, out_vaddr);
	if (err) {
		/* TODO: cleanup */
		return err;
	}
	err = update_space_stat(sbi, out_vaddr, VOLUTA_SP_TAKE);
	if (err) {
		/* TODO: cleanup */
		return err;
	}
	return 0;
}

int voluta_new_block(struct voluta_sb_info *sbi, enum voluta_vtype vtype,
		     struct voluta_bk_info **out_bki)
{
	int err;
	struct voluta_vaddr vaddr;
	struct voluta_bk_info *bki, *uber_bki;

	err = voluta_new_bspace(sbi, vtype, &vaddr);
	if (err) {
		return err;
	}
	err = voluta_stage_uber_of(sbi, &vaddr, &uber_bki);
	if (err) {
		return err;
	}
	err = voluta_spawn_bk(sbi, &vaddr, uber_bki, &bki);
	if (err) {
		/* TODO: spfree */
		return err;
	}
	/*
	 * XXX: Hack; we did not read block from persistent storage yet we mark
	 * it as 'staged' so that next lookup to this block within this
	 * operation cycle get a valid block. Need to find better solution.
	 */
	bki->b_staged = true;
	*out_bki = bki;
	return 0;
}

static void discard_if_cached_bk(struct voluta_env *env,
				 struct voluta_bk_info *hint_bki,
				 const struct voluta_vaddr *vaddr)
{
	int err = 0;
	struct voluta_bk_info *bki = hint_bki;
	struct voluta_cache *cache = cache_of(env);

	if (hint_bki == NULL) {
		err = voluta_cache_lookup_bk(cache, vaddr, &bki);
	}
	if (!err && (bki != NULL)) {
		voluta_assert_eq(bki->b_vaddr.lba, vaddr->lba);
		voluta_cache_forget_bk(cache, bki);
	}
}

int voluta_del_block(struct voluta_env *env, struct voluta_bk_info *bki,
		     const struct voluta_vaddr *vaddr)
{
	int err;
	struct voluta_sb_info *sbi = sbi_of(env);

	err = update_space_stat(sbi, vaddr, VOLUTA_SP_GIVE);
	if (err) {
		/* TODO: cleanup */
		return err;
	}
	err = voluta_deallocate_space(sbi, vaddr);
	if (err) {
		return err;
	}
	discard_if_cached_bk(env, bki, vaddr);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int spawn_bk_now(struct voluta_sb_info *sbi,
			const struct voluta_vaddr *vaddr,
			struct voluta_bk_info *uber_bki,
			struct voluta_bk_info **out)
{
	int err;

	err = voluta_cache_spawn_bk(sbi->s_cache, vaddr, uber_bki, out);
	if (err != -ENOMEM) {
		return err;
	}
	err = voluta_commit_dirtyq(sbi, VOLUTA_F_DATA | VOLUTA_F_NOW);
	if (err) {
		return err;
	}
	err = voluta_cache_spawn_bk(sbi->s_cache, vaddr, uber_bki, out);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_spawn_bk(struct voluta_sb_info *sbi,
		    const struct voluta_vaddr *vaddr,
		    struct voluta_bk_info *uber_bki,
		    struct voluta_bk_info **out_bki)
{
	int err;

	err = spawn_bk_now(sbi, vaddr, uber_bki, out_bki);
	if (err) {
		return err;
	}
	if ((*out_bki)->b_sbi == NULL) {
		(*out_bki)->b_sbi = sbi;
	}
	return 0;
}

