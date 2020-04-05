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

/* Number of k-blocks in allocation-space */
#define VOLUTA_NKB_IN_AS        (VOLUTA_NKB_IN_AG - VOLUTA_NKB_IN_BK)



/* Local functions forward declarations */
static int stage_usmap(struct voluta_sb_info *sbi, size_t usp_index,
		       struct voluta_vnode_info **out_vi);
static int stage_agmap(struct voluta_sb_info *sbi, size_t ag_index,
		       struct voluta_vnode_info **out_vi);
static int stage_usmap_of(struct voluta_sb_info *sbi, size_t ag_index,
			  struct voluta_vnode_info **out_vi);
static void bind_vnode_to_bk(struct voluta_vnode_info *vi,
			     struct voluta_bk_info *bki);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool vtype_isusmap(enum voluta_vtype vtype)
{
	return vtype_isequal(vtype, VOLUTA_VTYPE_USMAP);
}

static bool vtype_isagmap(enum voluta_vtype vtype)
{
	return vtype_isequal(vtype, VOLUTA_VTYPE_AGMAP);
}

static bool vtype_isinode(enum voluta_vtype vtype)
{
	return vtype_isequal(vtype, VOLUTA_VTYPE_INODE);
}

static bool vtype_isdata(enum voluta_vtype vtype)
{
	return vtype_isequal(vtype, VOLUTA_VTYPE_DATA);
}

static bool vtype_isnone(enum voluta_vtype vtype)
{
	return vtype_isequal(vtype, VOLUTA_VTYPE_NONE);
}

static size_t vtype_psize(enum voluta_vtype vtype)
{
	return voluta_persistent_size(vtype);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool lba_isnull(loff_t lba)
{
	return (lba == VOLUTA_LBA_NULL);
}

static size_t nkb_to_size(size_t nkb)
{
	return (nkb * VOLUTA_KB_SIZE);
}

static size_t size_to_nkb(size_t nbytes)
{
	return div_round_up(nbytes, VOLUTA_KB_SIZE);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static loff_t lba_to_off(loff_t lba)
{
	return lba * (loff_t)VOLUTA_BK_SIZE;
}

static loff_t off_to_lba(loff_t off)
{
	const loff_t bk_size = VOLUTA_BK_SIZE;

	return off / bk_size;
}

static loff_t off_to_lba_safe(loff_t off)
{
	return !off_isnull(off) ? off_to_lba(off) : VOLUTA_LBA_NULL;
}

static loff_t lba_kbn_to_off(loff_t lba, size_t kbn)
{
	const loff_t kb_size = VOLUTA_KB_SIZE;

	return lba_to_off(lba) + ((loff_t)kbn * kb_size);
}

static size_t off_to_kbn(loff_t off)
{
	const loff_t kb_size = VOLUTA_KB_SIZE;
	const loff_t nkb_in_bk = VOLUTA_NKB_IN_BK;

	return (size_t)((off / kb_size) % nkb_in_bk);
}

static size_t ag_index_of(loff_t lba)
{
	const loff_t nbk_in_ag = VOLUTA_NBK_IN_AG;

	voluta_assert_gt(lba, 0);

	return (size_t)(lba / nbk_in_ag);
}

static loff_t lba_by_ag(loff_t ag_lba, size_t bn)
{
	voluta_assert_lt(bn, VOLUTA_NBK_IN_AG);
	voluta_assert_gt(bn, 0);

	return ag_lba + (loff_t)bn;
}

static loff_t lba_of_agm(size_t ag_index)
{
	const size_t nbk_in_ag = VOLUTA_NBK_IN_AG;

	voluta_assert_ge(ag_index, 2);
	return (loff_t)(nbk_in_ag * ag_index);
}

static size_t usp_index_of(size_t ag_index)
{
	const size_t nag_in_usp = VOLUTA_NAG_IN_USP;

	voluta_assert_ge(ag_index, 2);

	return ((ag_index - 2) / nag_in_usp) + 1;
}

static loff_t usmap_off_by_index(size_t usp_index)
{
	const size_t usm_size = VOLUTA_USMAP_SIZE;
	const loff_t off_base = VOLUTA_AG_SIZE;

	voluta_assert_ge(usp_index, 1);
	voluta_assert_le(usp_index, VOLUTA_NUSP_MAX);

	return off_base + (loff_t)((usp_index - 1) * usm_size);
}

static size_t ag_index_by_usmap(size_t usp_index, size_t ag_slot)
{
	const size_t nag_in_usp = VOLUTA_NAG_IN_USP;

	voluta_assert_ge(usp_index, 1);
	voluta_assert_le(usp_index, VOLUTA_NUSP_MAX);

	return ((usp_index - 1) * nag_in_usp) + ag_slot + 2;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_vaddr_reset(struct voluta_vaddr *vaddr)
{
	vaddr->ag_index = VOLUTA_AG_INDEX_NULL;
	vaddr->off = VOLUTA_OFF_NULL;
	vaddr->lba = VOLUTA_LBA_NULL;
	vaddr->vtype = VOLUTA_VTYPE_NONE;
}

void voluta_vaddr_clone(const struct voluta_vaddr *vaddr,
			struct voluta_vaddr *other)
{
	other->ag_index = vaddr->ag_index;
	other->off = vaddr->off;
	other->lba = vaddr->lba;
	other->vtype = vaddr->vtype;
}

static void vaddr_assign(struct voluta_vaddr *vaddr, enum voluta_vtype vtype,
			 size_t ag_index, loff_t lba, size_t kbn)
{
	vaddr->ag_index = ag_index;
	vaddr->lba = lba;
	vaddr->off = lba_kbn_to_off(lba, kbn);
	vaddr->vtype = vtype;
}

static void vaddr_setup(struct voluta_vaddr *vaddr, enum voluta_vtype vtype,
			size_t ag_index, size_t bn, size_t kbn)
{
	const loff_t lba = lba_by_ag(lba_of_agm(ag_index), bn);

	vaddr_assign(vaddr, vtype, ag_index, lba, kbn);
}

static void vaddr_by_lba(struct voluta_vaddr *vaddr,
			 enum voluta_vtype vtype, loff_t lba)
{
	if (!lba_isnull(lba)) {
		vaddr_assign(vaddr, vtype, ag_index_of(lba), lba, 0);
	} else {
		vaddr_reset(vaddr);
		vaddr->vtype = vtype;
	}
}

void voluta_vaddr_by_off(struct voluta_vaddr *vaddr,
			 enum voluta_vtype vtype, loff_t off)
{
	const loff_t lba = off_to_lba_safe(off);

	vaddr_by_lba(vaddr, vtype, lba);
	vaddr->off = off;
}

bool voluta_vaddr_isnull(const struct voluta_vaddr *vaddr)
{
	return off_isnull(vaddr->off) || lba_isnull(vaddr->lba);
}

static bool vaddr_isdata(const struct voluta_vaddr *vaddr)
{
	return vtype_isdata(vaddr->vtype);
}

static void vaddr_of_agmap(struct voluta_vaddr *vaddr, size_t ag_index)
{
	vaddr_by_lba(vaddr, VOLUTA_VTYPE_AGMAP, lba_of_agm(ag_index));
}

static void vaddr_of_usmap(struct voluta_vaddr *vaddr, size_t usp_index)
{
	const loff_t off = usmap_off_by_index(usp_index);

	vaddr_by_off(vaddr, VOLUTA_VTYPE_USMAP, off);
	voluta_assert_eq(vaddr->ag_index, 1);
}

size_t voluta_vaddr_len(const struct voluta_vaddr *vaddr)
{
	return vtype_psize(vaddr->vtype);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t nkb_of(const struct voluta_vaddr *vaddr)
{
	return size_to_nkb(vaddr_len(vaddr));
}

static size_t kbn_of(const struct voluta_vaddr *vaddr)
{
	voluta_assert_eq(vaddr->off % VOLUTA_KB_SIZE, 0);

	return off_to_kbn(vaddr->off);
}

static size_t kbn_within_bo(size_t kbn)
{
	return kbn % VOLUTA_NKB_IN_BO;
}

static size_t kbn_within_bk(size_t bo_index, size_t kbn_within_bo)
{
	return (bo_index * VOLUTA_NKB_IN_BO) + kbn_within_bo;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void iv_clone(const struct voluta_iv *iv, struct voluta_iv *other)
{
	memcpy(other, iv, sizeof(*other));
}

static void key_clone(const struct voluta_key *key, struct voluta_key *other)
{
	memcpy(other, key, sizeof(*other));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static ssize_t nbytes_of(const struct voluta_space_stat *sp_st)
{
	return sp_st->ndata + sp_st->nmeta;
}

static void sum_space_stat(struct voluta_space_stat *res,
			   const struct voluta_space_stat *st1,
			   const struct voluta_space_stat *st2)

{
	res->nmeta = st1->nmeta + st2->nmeta;
	res->ndata = st1->ndata + st2->ndata;
	res->nfiles = st1->nfiles + st2->nfiles;
}

static size_t safe_sum(size_t cur, ssize_t dif)
{
	size_t val = cur;

	if (dif > 0) {
		val += (size_t)dif;
		voluta_assert_gt(val, cur);
	} else if (dif < 0) {
		val -= (size_t)(-dif);
		voluta_assert_lt(val, cur);
	}
	return val;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static union voluta_bk *sb_to_bk(struct voluta_super_block *sb)
{
	return container_of(sb, union voluta_bk, sb);
}

static void sb_setup(struct voluta_super_block *sb, time_t now)
{
	voluta_stamp_meta(&sb->s_hdr, VOLUTA_VTYPE_SUPER, sizeof(*sb));
	voluta_uuid_generate(&sb->s_uuid);
	sb->s_version = VOLUTA_VERSION;
	sb->s_birth_time = (uint64_t)now;
	/* TODO: FIll name */
	for (size_t i = 0; i < ARRAY_SIZE(sb->s_keys); ++i) {
		voluta_fill_random_key(&sb->s_keys[i]);
	}
	for (size_t j = 0; j < ARRAY_SIZE(sb->s_ivs); ++j) {
		voluta_fill_random_iv(&sb->s_ivs[j]);
	}
}

static void sb_iv_key_of(const struct voluta_super_block *sb,
			 size_t ag_index, struct voluta_iv_key *out_iv_key)
{
	size_t iv_index, key_index;

	iv_index = ag_index % ARRAY_SIZE(sb->s_ivs);
	key_index = ag_index % ARRAY_SIZE(sb->s_keys);

	iv_clone(&sb->s_ivs[iv_index], &out_iv_key->iv);
	key_clone(&sb->s_keys[key_index], &out_iv_key->key);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t agr_used_meta(const struct voluta_ag_record *agr)
{
	return le32_to_cpu(agr->a_used_meta);
}

static void agr_set_used_meta(struct voluta_ag_record *agr, size_t used_meta)
{
	agr->a_used_meta = cpu_to_le32((uint32_t)used_meta);
}

static size_t agr_used_data(const struct voluta_ag_record *agr)
{
	return le32_to_cpu(agr->a_used_data);
}

static void agr_set_used_data(struct voluta_ag_record *agr, size_t used_data)
{
	agr->a_used_data = cpu_to_le32((uint32_t)used_data);
}

static size_t agr_nfiles(const struct voluta_ag_record *agr)
{
	return le32_to_cpu(agr->a_nfiles);
}

static void agr_set_nfiles(struct voluta_ag_record *agr, size_t nfiles)
{
	agr->a_nfiles = cpu_to_le32((uint32_t)nfiles);
}

static enum voluta_ag_flags agr_flags(const struct voluta_ag_record *agr)
{
	return le32_to_cpu(agr->a_flags);
}

static void agr_set_flags(struct voluta_ag_record *agr, enum voluta_ag_flags f)
{
	agr->a_flags = cpu_to_le32(f);
}

static bool agr_has_flags(const struct voluta_ag_record *agr,
			  const enum voluta_ag_flags mask)
{
	return ((agr_flags(agr) & mask) == mask);
}

static bool agr_is_formatted(const struct voluta_ag_record *agr)
{
	return agr_has_flags(agr, VOLUTA_AGF_FORMATTED);
}

static void agr_set_formatted(struct voluta_ag_record *agr)
{
	agr_set_flags(agr, agr_flags(agr) | VOLUTA_AGF_FORMATTED);
}

static void agr_init(struct voluta_ag_record *agr)
{
	agr_set_used_meta(agr, 0);
	agr_set_used_data(agr, 0);
	agr_set_nfiles(agr, 0);
	agr_set_flags(agr, 0);
}

static void agr_stat(const struct voluta_ag_record *agr,
		     struct voluta_space_stat *sp_st)
{
	sp_st->nmeta = (ssize_t)agr_used_meta(agr);
	sp_st->ndata = (ssize_t)agr_used_data(agr);
	sp_st->nfiles = (ssize_t)agr_nfiles(agr);
}

static void agr_update_stats(struct voluta_ag_record *agr,
			     const struct voluta_space_stat *sp_st)
{
	agr_set_used_data(agr, safe_sum(agr_used_data(agr), sp_st->ndata));
	agr_set_used_meta(agr, safe_sum(agr_used_meta(agr), sp_st->nmeta));
	agr_set_nfiles(agr, safe_sum(agr_nfiles(agr), sp_st->nfiles));
}

static size_t agr_used_space(const struct voluta_ag_record *agr)
{
	return agr_used_meta(agr) + agr_used_data(agr);
}

static bool agr_has_nfree(const struct voluta_ag_record *agr, size_t nbytes)
{
	/* TODO: Required formatted */
	const size_t nused = agr_used_space(agr);
	const size_t ag_size = VOLUTA_AG_SIZE;

	return ((nused + nbytes) <= ag_size);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t usm_index(const struct voluta_uspace_map *usm)
{
	return le32_to_cpu(usm->u_index);
}

static void usm_set_index(struct voluta_uspace_map *usm, size_t usp_index)
{
	usm->u_index = cpu_to_le32((uint32_t)usp_index);
}

static size_t usm_nused(const struct voluta_uspace_map *usm)
{
	return le64_to_cpu(usm->u_nused);
}

static void usm_set_nused(struct voluta_uspace_map *usm, size_t nused)
{
	voluta_assert_le(nused, VOLUTA_USP_SIZE);
	usm->u_nused = cpu_to_le64(nused);
}

static void usm_ag_range(const struct voluta_uspace_map *usm,
			 size_t *ag_index_beg, size_t *ag_index_end)
{
	const size_t ag_index_base = ag_index_by_usmap(usm_index(usm), 0);

	*ag_index_beg = ag_index_base;
	*ag_index_end = ag_index_base + VOLUTA_NAG_IN_USP;
}

static void usm_init(struct voluta_uspace_map *usm, size_t usp_index)
{
	usm_set_index(usm, usp_index);
	usm_set_nused(usm, 0);
	for (size_t i = 0; i < ARRAY_SIZE(usm->u_agr); ++i) {
		agr_init(&usm->u_agr[i]);
	}
}

static void usm_setup_iv_keys(struct voluta_uspace_map *usm)
{
	for (size_t i = 0; i < ARRAY_SIZE(usm->u_ivs); ++i) {
		voluta_fill_random_iv(&usm->u_ivs[i]);
	}
	for (size_t j = 0; j < ARRAY_SIZE(usm->u_keys); ++j) {
		voluta_fill_random_key(&usm->u_keys[j]);
	}
}

static struct voluta_ag_record *
usm_record_at(const struct voluta_uspace_map *usm, size_t slot)
{
	const struct voluta_ag_record *usr = &usm->u_agr[slot];

	voluta_assert_lt(slot, ARRAY_SIZE(usm->u_agr));
	return (struct voluta_ag_record *)usr;
}

static struct voluta_ag_record *
usm_record_of(const struct voluta_uspace_map *usm, size_t ag_index)
{
	const size_t slot = (ag_index - 2) % ARRAY_SIZE(usm->u_agr);

	voluta_assert_ge(ag_index, 2);

	return usm_record_at(usm, slot);
}

static size_t usm_resolve_ag_index(const struct voluta_uspace_map *usm,
				   const struct voluta_ag_record *agr)
{
	const size_t ag_slot = (size_t)(agr - usm->u_agr);
	const size_t usp_index = usm_index(usm);

	voluta_assert(agr >= usm->u_agr);
	voluta_assert_lt(ag_slot, ARRAY_SIZE(usm->u_agr));

	return ag_index_by_usmap(usp_index, ag_slot);
}

static void usm_update_stats(struct voluta_uspace_map *usm, size_t ag_index,
			     const struct voluta_space_stat *sp_st)
{
	const ssize_t diff = nbytes_of(sp_st);
	struct voluta_ag_record *agr = usm_record_of(usm, ag_index);

	agr_update_stats(agr, sp_st);
	usm_set_nused(usm, safe_sum(usm_nused(usm), diff));
}

static void usm_iv_key_of(const struct voluta_uspace_map *usm,
			  size_t ag_index, struct voluta_iv_key *out_iv_key)
{
	const size_t islot = ag_index % ARRAY_SIZE(usm->u_ivs);
	const size_t kslot = ag_index % ARRAY_SIZE(usm->u_keys);

	iv_clone(&usm->u_ivs[islot], &out_iv_key->iv);
	key_clone(&usm->u_keys[kslot], &out_iv_key->key);
}

static void usm_total_stat(const struct voluta_uspace_map *usm,
			   struct voluta_space_stat *sp_st_total)
{
	struct voluta_space_stat ag_sp_st;
	const struct voluta_ag_record *agr;

	for (size_t i = 0; i < ARRAY_SIZE(usm->u_agr); ++i) {
		agr = usm_record_at(usm, i);
		agr_stat(agr, &ag_sp_st);
		sum_space_stat(sp_st_total, &ag_sp_st, sp_st_total);
	}
}

static void usm_set_formatted(struct voluta_uspace_map *usm,
			      size_t ag_index, size_t agmap_size)
{
	struct voluta_ag_record *agr = usm_record_of(usm, ag_index);
	struct voluta_space_stat sp_st = {
		.nmeta = (ssize_t)agmap_size
	};

	agr_set_formatted(agr);
	usm_update_stats(usm, ag_index, &sp_st);
}

static bool usm_is_formatted(struct voluta_uspace_map *usm, size_t ag_index)
{
	const struct voluta_ag_record *agr = usm_record_of(usm, ag_index);

	return agr_is_formatted(agr);
}

static bool usm_may_alloc(const struct voluta_uspace_map *usm, size_t nbytes)
{
	const size_t nbytes_max = VOLUTA_USP_SIZE;
	const size_t nbytes_cur = usm_nused(usm);

	return ((nbytes_cur + nbytes) <= nbytes_max);
}

static bool usm_may_alloc_from(const struct voluta_uspace_map *usm,
			       size_t ag_index, size_t nbytes)
{
	const struct voluta_ag_record *agr = usm_record_of(usm, ag_index);

	return agr_is_formatted(agr) && agr_has_nfree(agr, nbytes);
}

static size_t usm_record_slot(const struct voluta_uspace_map *usm,
			      const struct voluta_ag_record *agr)
{
	voluta_assert(agr >= usm->u_agr);

	return (size_t)(agr - usm->u_agr);
}

static size_t usm_find_avail(const struct voluta_uspace_map *usm,
			     size_t ag_index_from, size_t nbytes)
{
	size_t beg;
	size_t ag_index = VOLUTA_AG_INDEX_NULL;
	const struct voluta_ag_record *agr;

	agr = usm_record_of(usm, ag_index_from);
	beg = usm_record_slot(usm, agr);
	for (size_t i = beg; i < ARRAY_SIZE(usm->u_agr); ++i) {
		agr = &usm->u_agr[i];
		if (!agr_is_formatted(agr)) {
			break;
		}
		if (agr_has_nfree(agr, nbytes)) {
			ag_index = usm_resolve_ag_index(usm, agr);
			break;
		}
	}
	return ag_index;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static uint8_t mask_of(size_t kbn, size_t nkb)
{
	const unsigned int mask = ((1U << nkb) - 1) << kbn;

	return (uint8_t)mask;
}

static enum voluta_vtype bo_vtype(const struct voluta_boctet *bo)
{
	return (enum voluta_vtype)(bo->vtype);
}

static void bo_set_vtype(struct voluta_boctet *bo, enum voluta_vtype vtype)
{
	bo->vtype = (uint8_t)vtype;
}

static bool bo_has_vtype_or_none(const struct voluta_boctet *bo,
				 enum voluta_vtype vtype)
{
	const enum voluta_vtype vt = bo_vtype(bo);

	return vtype_isnone(vt) || vtype_isequal(vt, vtype);
}

static enum voluta_vtype bo_vtype_at(const struct voluta_boctet *bo,
				     size_t kbn)
{
	const uint8_t mask = mask_of(kbn, 1);

	return (bo->usemask && mask) ? bo_vtype(bo) : VOLUTA_VTYPE_NONE;
}

static void bo_set_refcnt(struct voluta_boctet *bo, uint32_t refcnt)
{
	bo->refcnt = cpu_to_le32(refcnt);
}

static void bo_init(struct voluta_boctet *bo)
{
	bo_set_vtype(bo, VOLUTA_VTYPE_NONE);
	bo_set_refcnt(bo, 0);
	bo->usemask = 0;
	bo->unwritten = 0;
	bo->reserved = 0;
}

static void bo_init_arr(struct voluta_boctet *bo_arr, size_t cnt)
{
	for (size_t i = 0; i < cnt; ++i) {
		bo_init(&bo_arr[i]);
	}
}

static void bo_set(struct voluta_boctet *bo,
		   enum voluta_vtype vtype, size_t kbn, size_t nkb)
{
	const uint8_t mask = mask_of(kbn, nkb);

	voluta_assert_eq(bo->usemask & mask, 0);

	bo->usemask |= mask;
	bo_set_vtype(bo, vtype);
}

static void bo_unset(struct voluta_boctet *bo, size_t kbn, size_t nkb)
{
	const uint8_t mask = mask_of(kbn, nkb);

	voluta_assert_eq(bo->usemask & mask, mask);

	bo->usemask &= (uint8_t)(~mask);
	if (!bo->usemask) {
		bo_set_vtype(bo, VOLUTA_VTYPE_NONE);
		bo->unwritten = 0;
	}
}

static size_t bo_usecnt(const struct voluta_boctet *bo)
{
	return voluta_popcount(bo->usemask);
}

static size_t bo_freecnt(const struct voluta_boctet *bo)
{
	return VOLUTA_NKB_IN_BO - bo_usecnt(bo);
}

static bool bo_isfull(const struct voluta_boctet *bo)
{
	return (bo->usemask == 0xFF);
}

static bool bo_isallfree(const struct voluta_boctet *bo)
{
	return (bo->usemask == 0x00);
}

static bool bo_may_alloc(const struct voluta_boctet *bo,
			 enum voluta_vtype vtype, size_t nkb)
{
	bool ret = false;

	if (!bo_isfull(bo) && (bo_freecnt(bo) >= nkb)) {
		ret = bo_has_vtype_or_none(bo, vtype);
	}
	return ret;
}

static int bo_find_free(const struct voluta_boctet *bo,
			size_t nkb, size_t *out_kbn)
{
	uint8_t mask;
	const size_t nbks_in_bo = VOLUTA_NKB_IN_BO;

	for (size_t kbn = 0; (kbn + nkb) <= nbks_in_bo; kbn += nkb) {
		mask = mask_of(kbn, nkb);
		if ((bo->usemask & mask) == 0) {
			*out_kbn = kbn;
			return 0;
		}
	}
	return -ENOSPC;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void bkref_init(struct voluta_bkref *bkref)
{
	bo_init_arr(bkref->bo, ARRAY_SIZE(bkref->bo));
}

static void bkref_init_arr(struct voluta_bkref *arr, size_t cnt)
{
	for (size_t i = 0; i < cnt; ++i) {
		bkref_init(&arr[i]);
	}
}

static void bkref_renew_iv_key(struct voluta_bkref *bkref)
{
	voluta_fill_random_iv(&bkref->b_iv);
	voluta_fill_random_key(&bkref->b_key);
}

static void bkref_renew(struct voluta_bkref *bkref)
{
	bkref_renew_iv_key(bkref);
	bo_init_arr(bkref->bo, ARRAY_SIZE(bkref->bo));
}

static void bkref_extern_iv_key(const struct voluta_bkref *bkref,
				struct voluta_iv_key *out_iv_key)
{
	iv_clone(&bkref->b_iv, &out_iv_key->iv);
	key_clone(&bkref->b_key, &out_iv_key->key);
}

static struct voluta_boctet *
bkref_boctet_of(const struct voluta_bkref *bkref, size_t kbn)
{
	const size_t bo_index = kbn / VOLUTA_NKB_IN_BO;
	const  struct voluta_boctet *bo = &bkref->bo[bo_index];

	voluta_assert_lt(bo_index, ARRAY_SIZE(bkref->bo));
	return (struct voluta_boctet *)bo;
}

static void bkref_setn_kbn(struct voluta_bkref *bkref,
			   enum voluta_vtype vtype, size_t kbn, size_t nkb)
{
	struct voluta_boctet *bo = bkref_boctet_of(bkref, kbn);

	bo_set(bo, vtype, kbn_within_bo(kbn), nkb);
}

static void bkref_clearn_kbn(struct voluta_bkref *bkref, size_t kbn,
			     size_t nkb)
{
	struct voluta_boctet *bo = bkref_boctet_of(bkref, kbn);

	bo_unset(bo, kbn_within_bo(kbn), nkb);
}

static int bkref_vtype_at(const struct voluta_bkref *bkref, size_t kbn)
{
	const struct voluta_boctet *bo = bkref_boctet_of(bkref, kbn);

	return bo_vtype_at(bo, kbn_within_bo(kbn));
}

static bool bkref_is_unwritten(const struct voluta_bkref *bkref, size_t kbn)
{
	const struct voluta_boctet *bo = bkref_boctet_of(bkref, kbn);

	return bo->unwritten;
}

static void bkref_clear_unwritten(struct voluta_bkref *bkref, size_t kbn)
{
	struct voluta_boctet *bo = bkref_boctet_of(bkref, kbn);

	bo->unwritten = 0;
}

static void bkref_mark_unwritten(struct voluta_bkref *bkref, size_t kbn)
{
	struct voluta_boctet *bo = bkref_boctet_of(bkref, kbn);

	bo->unwritten = 1;
}

static size_t bkref_nused(const struct voluta_bkref *bkref)
{
	size_t cnt = 0;
	const struct voluta_boctet *bo;

	for (size_t i = 0; i < ARRAY_SIZE(bkref->bo); ++i) {
		bo = &bkref->bo[i];
		cnt += bo_usecnt(bo);
	}
	return cnt;
}

static bool bkref_isunused(const struct voluta_bkref *bkref)
{
	const struct voluta_boctet *bo;

	for (size_t i = 0; i < ARRAY_SIZE(bkref->bo); ++i) {
		bo = &bkref->bo[i];

		if (bo_usecnt(bo)) {
			return false;
		}
	}
	return true;
}

static int bkref_find_free(const struct voluta_bkref *bkref,
			   bool partial, enum voluta_vtype vtype,
			   size_t nkb, size_t *out_kbn)
{
	int err;
	size_t kbn;
	const struct voluta_boctet *bo;

	for (size_t boi = 0; boi < ARRAY_SIZE(bkref->bo); ++boi) {
		bo = &bkref->bo[boi];
		if (partial && bo_isallfree(bo)) {
			continue;
		}
		if (bo_may_alloc(bo, vtype, nkb)) {
			err = bo_find_free(bo, nkb, &kbn);
			if (!err) {
				*out_kbn = kbn_within_bk(boi, kbn);
				return 0;
			}
		}
	}
	return -ENOSPC;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t bn_of_lba(loff_t lba)
{
	const loff_t nbk_in_ag = VOLUTA_NBK_IN_AG;
	const size_t bn = (size_t)(lba % nbk_in_ag);

	voluta_assert_ne(bn, 0);
	return bn;
}

static size_t agm_index(const struct voluta_agroup_map *agm)
{
	return le64_to_cpu(agm->ag_index);
}

static void agm_set_index(struct voluta_agroup_map *agm, size_t ag_index)
{
	agm->ag_index = cpu_to_le64(ag_index);
}

static size_t agm_nkb_used(const struct voluta_agroup_map *agm)
{
	return le32_to_cpu(agm->ag_nkb_used);
}

static void agm_set_nkb_used(struct voluta_agroup_map *agm, size_t nkb)
{
	voluta_assert_le(nkb, VOLUTA_NKB_IN_AS);
	agm->ag_nkb_used = cpu_to_le32((uint32_t)nkb);
}

static void agm_init(struct voluta_agroup_map *agm, size_t ag_index)
{
	agm_set_index(agm, ag_index);
	agm_set_nkb_used(agm, 0);
	agm->ag_ciper_type = VOLUTA_CIPHER_AES256;
	agm->ag_ciper_mode = VOLUTA_CIPHER_MODE_CBC;
	bkref_init_arr(agm->ag_bkref, ARRAY_SIZE(agm->ag_bkref));
}

static void agm_set_fs_uuid(struct voluta_agroup_map *agm,
			    const struct voluta_uuid *uuid)
{
	voluta_uuid_clone(uuid, &agm->ag_fs_uuid);
}

static struct voluta_bkref *
agm_bkref_at(const struct voluta_agroup_map *agm, size_t slot)
{
	const struct voluta_bkref *bkref = &(agm->ag_bkref[slot]);

	voluta_assert_lt(slot, ARRAY_SIZE(agm->ag_bkref));
	return (struct voluta_bkref *)bkref;
}

static struct voluta_bkref *
agm_bkref_of(const struct voluta_agroup_map *agm, size_t bn)
{
	voluta_assert_gt(bn, 0);
	return agm_bkref_at(agm, bn - 1);
}

static struct voluta_bkref *
agm_bkref_by_lba(const struct voluta_agroup_map *agm, loff_t lba)
{
	return agm_bkref_of(agm, bn_of_lba(lba));
}

static struct voluta_bkref *
agm_bkref_of_vaddr(const struct voluta_agroup_map *agm,
		   const struct voluta_vaddr *vaddr)
{
	return agm_bkref_by_lba(agm, vaddr->lba);
}

static int agm_vtype_at(const struct voluta_agroup_map *agm,
			const struct voluta_vaddr *vaddr)
{
	const struct voluta_bkref *bkref = agm_bkref_by_lba(agm, vaddr->lba);

	return bkref_vtype_at(bkref, kbn_of(vaddr));
}

static void agm_resolve(const struct voluta_agroup_map *agm, loff_t lba,
			size_t kbn, struct voluta_vaddr *out_vaddr)
{
	loff_t off;
	enum voluta_vtype vtype;
	const size_t bn = bn_of_lba(lba);
	const struct voluta_bkref *bkref = agm_bkref_of(agm, bn);

	vtype = bkref_vtype_at(bkref, kbn);
	off = lba_kbn_to_off(lba, kbn);
	vaddr_by_off(out_vaddr, vtype, off);
}

static void agm_iv_key_of(const struct voluta_agroup_map *agm, loff_t lba,
			  struct voluta_iv_key *out_iv_key)
{
	const size_t bn = bn_of_lba(lba);
	const struct voluta_bkref *bkref = agm_bkref_of(agm, bn);

	bkref_extern_iv_key(bkref, out_iv_key);
}

static void agm_setup_iv_keys(struct voluta_agroup_map *agm)
{
	for (size_t i = 0; i < ARRAY_SIZE(agm->ag_bkref); ++i) {
		bkref_renew_iv_key(agm_bkref_at(agm, i));
	}
}

static int agm_find_nfree_at(const struct voluta_agroup_map *agm,
			     bool partial, enum voluta_vtype vtype,
			     size_t nkb, size_t bn, size_t *out_kbn)
{
	const struct voluta_bkref *bkref = agm_bkref_of(agm, bn);

	return bkref_find_free(bkref, partial, vtype, nkb, out_kbn);
}

static int agm_find_nfree(const struct voluta_agroup_map *agm,
			  bool partial, enum voluta_vtype vtype,
			  size_t nkb, size_t *out_bn, size_t *out_kbn)
{
	int err;
	size_t kbn, bn;
	const size_t nkb_in_bk = VOLUTA_NKB_IN_BK;
	const size_t nbkrefs = ARRAY_SIZE(agm->ag_bkref);
	const size_t nbk_used = agm_nkb_used(agm);

	/* Heuristic logic for mostly write-append pattern */
	bn = partial ? 1 : ((nbk_used / nkb_in_bk) + 1);
	for (size_t i = 0; i < nbkrefs; ++i) {
		if (bn > nbkrefs) {
			bn = 1;
		}
		err = agm_find_nfree_at(agm, partial, vtype, nkb, bn, &kbn);
		if (!err) {
			*out_bn = bn;
			*out_kbn = kbn;
			return 0;
		}
		bn += 1;
	}
	return -ENOSPC;
}

static int agm_find_free(const struct voluta_agroup_map *agm,
			 enum voluta_vtype vtype, size_t nkb,
			 size_t *out_bn, size_t *out_kbn)
{
	int err = -ENOSPC;
	const size_t nkb_in_bk = VOLUTA_NKB_IN_BK;

	if ((nkb < nkb_in_bk) && agm_nkb_used(agm)) {
		err = agm_find_nfree(agm, true, vtype, nkb, out_bn, out_kbn);
	}
	if (err == -ENOSPC) {
		err = agm_find_nfree(agm, false, vtype, nkb, out_bn, out_kbn);
	}
	return err;
}

static int agm_find_free_space(const struct voluta_agroup_map *agm,
			       enum voluta_vtype vtype, size_t nkb,
			       struct voluta_vaddr *out_vaddr)
{
	int err;
	size_t bn;
	size_t kbn;

	err = agm_find_free(agm, vtype, nkb, &bn, &kbn);
	if (err) {
		return err;
	}
	vaddr_setup(out_vaddr, vtype, agm_index(agm), bn, kbn);
	return 0;
}

static void agm_mark_unwritten_at(struct voluta_agroup_map *agm,
				  const struct voluta_vaddr *vaddr)
{
	struct voluta_bkref *bkref = agm_bkref_of_vaddr(agm, vaddr);

	bkref_mark_unwritten(bkref, kbn_of(vaddr));
}

static int agm_is_unwritten_at(const struct voluta_agroup_map *agm,
			       const struct voluta_vaddr *vaddr)
{
	const struct voluta_bkref *bkref = agm_bkref_of_vaddr(agm, vaddr);

	return bkref_is_unwritten(bkref, kbn_of(vaddr));
}

static void agm_clear_unwritten_at(struct voluta_agroup_map *agm,
				   const struct voluta_vaddr *vaddr)
{
	struct voluta_bkref *bkref = agm_bkref_of_vaddr(agm, vaddr);

	bkref_clear_unwritten(bkref, kbn_of(vaddr));
}

static void agm_inc_used_space(struct voluta_agroup_map *agm,
			       const struct voluta_vaddr *vaddr)
{
	agm_set_nkb_used(agm, agm_nkb_used(agm) + nkb_of(vaddr));
}

static void agm_dec_used_space(struct voluta_agroup_map *agm,
			       const struct voluta_vaddr *vaddr)
{
	agm_set_nkb_used(agm, agm_nkb_used(agm) - nkb_of(vaddr));
}

static void agm_mark_active_space(struct voluta_agroup_map *agm,
				  const struct voluta_vaddr *vaddr)
{
	struct voluta_bkref *bkref = agm_bkref_of_vaddr(agm, vaddr);

	bkref_setn_kbn(bkref, vaddr->vtype, kbn_of(vaddr), nkb_of(vaddr));
	agm_inc_used_space(agm, vaddr);

	if (vtype_isdata(vaddr->vtype)) {
		agm_mark_unwritten_at(agm, vaddr);
	}
}

static void agm_clear_active_space(struct voluta_agroup_map *agm,
				   const struct voluta_vaddr *vaddr)
{
	struct voluta_bkref *bkref = agm_bkref_of_vaddr(agm, vaddr);

	bkref_clearn_kbn(bkref, kbn_of(vaddr), nkb_of(vaddr));
	agm_dec_used_space(agm, vaddr);
}

static void agm_renew_if_unused(struct voluta_agroup_map *agm,
				const struct voluta_vaddr *vaddr)
{
	struct voluta_bkref *bkref = agm_bkref_of_vaddr(agm, vaddr);

	if (bkref_isunused(bkref)) {
		bkref_renew(bkref);
	}
}

static bool agm_is_free_bk(const struct voluta_agroup_map *agm, loff_t lba)
{
	const struct voluta_bkref *bkref = agm_bkref_by_lba(agm, lba);

	return bkref_isunused(bkref);
}

static bool agm_has_space(const struct voluta_agroup_map *agm, size_t nkb)
{
	const size_t nkb_in_as = VOLUTA_NKB_IN_AS;

	return ((agm_nkb_used(agm) + nkb) <= nkb_in_as);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static enum voluta_vtype vtype_of(const struct voluta_vnode_info *vi)
{
	return (vi != NULL) ? v_vaddr_of(vi)->vtype :  VOLUTA_VTYPE_NONE;
}

static size_t psize_of(const struct voluta_vnode_info *vi)
{
	return vtype_psize(vtype_of(vi));
}

static void iv_key_by_agmap(const struct voluta_bk_info *bki,
			    struct voluta_iv_key *out_iv_key)
{
	const struct voluta_vnode_info *agm_vi = bki->b_ua_vi;

	agm_iv_key_of(agm_vi->u.agm, bki->b_lba, out_iv_key);
}

static void iv_key_by_usmap(const struct voluta_bk_info *bki,
			    struct voluta_iv_key *out_iv_key)
{
	const size_t ag_index = ag_index_of(bki->b_lba);
	const struct voluta_vnode_info *usm_vi = bki->b_ua_vi;

	usm_iv_key_of(usm_vi->u.usm, ag_index, out_iv_key);
}

static void iv_key_by_super(const struct voluta_bk_info *bki,
			    struct voluta_iv_key *out_iv_key)
{
	const struct voluta_super_block *sb = bki->b_sbi->sb;

	sb_iv_key_of(sb, ag_index_of(bki->b_lba), out_iv_key);
}

static void iv_key_of(const struct voluta_bk_info *bki,
		      struct voluta_iv_key *out_iv_key)
{
	const enum voluta_vtype vtype = vtype_of(bki->b_ua_vi);

	if (vtype_isagmap(vtype)) {
		iv_key_by_agmap(bki, out_iv_key);
	} else if (vtype_isusmap(vtype)) {
		iv_key_by_usmap(bki, out_iv_key);
	} else {
		iv_key_by_super(bki, out_iv_key);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void spi_init(struct voluta_space_info *spi)
{
	spi->sp_size = -1;
	spi->sp_usp_count = 0;
	spi->sp_usp_index_lo = 0;
	spi->sp_ag_count = 0;
	spi->sp_ag_apex = 0;
	spi->sp_used_meta = 0;
	spi->sp_used_data = 0;
	spi->sp_nfiles = 0;
}

static void spi_fini(struct voluta_space_info *spi)
{
	spi->sp_size = -1;
	spi->sp_usp_count = 0;
	spi->sp_usp_index_lo = 0;
	spi->sp_ag_count = 0;
	spi->sp_ag_apex = 0;
	spi->sp_used_meta = INT_MIN;
	spi->sp_used_data = INT_MIN;
	spi->sp_nfiles = INT_MIN;
}

static void spi_setup(struct voluta_space_info *spi, loff_t space_size)
{
	const size_t nag_in_usp = VOLUTA_NAG_IN_USP;
	const size_t ag_count = (size_t)space_size / VOLUTA_AG_SIZE;

	spi->sp_size = space_size;
	spi->sp_ag_count = ag_count;
	spi->sp_usp_count = div_round_up(ag_count - 2, nag_in_usp);
	spi->sp_usp_index_lo = 1;
}

static void spi_accum_stat(struct voluta_space_info *spi,
			   const struct voluta_space_stat *sp_st)
{
	spi->sp_used_data += sp_st->ndata;
	spi->sp_used_meta += sp_st->nmeta;
	spi->sp_nfiles += sp_st->nfiles;

	voluta_assert_ge(spi->sp_used_data, 0);
	voluta_assert_ge(spi->sp_used_meta, 0);
	voluta_assert_ge(spi->sp_nfiles, 0);
}

static ssize_t spi_used_bytes(const struct voluta_space_info *spi)
{
	return spi->sp_used_meta + spi->sp_used_data;
}

static size_t spi_calc_user_blimit(const struct voluta_space_info *spi)
{
	size_t bfree = 0;
	const size_t ag_nbk = VOLUTA_NBK_IN_AG;

	if (spi->sp_ag_count > 0) {
		bfree = (spi->sp_ag_count - 2) * (ag_nbk - 1);
	}
	return bfree;
}

static ssize_t spi_calc_bytes_limit(const struct voluta_space_info *spi)
{
	return (ssize_t)(spi_calc_user_blimit(spi) * VOLUTA_BK_SIZE);
}

static ssize_t spi_calc_files_limit(const struct voluta_space_info *spi)
{
	const size_t userb_max = spi_calc_user_blimit(spi);
	const size_t inodes_per_block = (VOLUTA_BK_SIZE / VOLUTA_INODE_SIZE);

	return (ssize_t)(userb_max * inodes_per_block) / 2;
}

static ssize_t spi_calc_bytes_inuse(const struct voluta_space_info *spi)
{
	const size_t ag_size = VOLUTA_AG_SIZE;

	return spi_used_bytes(spi) + (ssize_t)(2 * ag_size);
}

static bool spi_may_alloc_data(const struct voluta_space_info *spi, size_t nb)
{
	const ssize_t user_limit = spi_calc_bytes_limit(spi);
	const ssize_t used_data = spi->sp_used_data;

	return ((used_data + (ssize_t)nb) <= user_limit);
}

static bool spi_may_alloc_meta(const struct voluta_space_info *spi,
			       size_t nbytes, bool new_file)
{
	bool ret = true;
	ssize_t files_max;
	const ssize_t user_limit = spi_calc_bytes_limit(spi);
	const ssize_t used_meta = spi->sp_used_meta;

	if ((used_meta + (ssize_t)nbytes) > user_limit) {
		ret = false;
	} else if (new_file) {
		files_max = spi_calc_files_limit(spi);
		ret = (spi->sp_nfiles < files_max);
	}
	return ret;
}

static void spi_update_stats(struct voluta_space_info *spi,
			     const struct voluta_space_stat *sp_st)
{
	spi->sp_used_data += sp_st->ndata;
	spi->sp_used_meta += sp_st->nmeta;
	spi->sp_nfiles += sp_st->nfiles;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int vtype_at(const struct voluta_vnode_info *agm_vi,
		    const struct voluta_vaddr *vaddr)
{
	return agm_vtype_at(agm_vi->u.agm, vaddr);
}

static void setup_agmap(const struct voluta_sb_info *sbi,
			struct voluta_vnode_info *agm_vi)
{
	voluta_stamp_vnode(agm_vi);
	agm_init(agm_vi->u.agm, agm_vi->vaddr.ag_index);
	agm_setup_iv_keys(agm_vi->u.agm);
	agm_set_fs_uuid(agm_vi->u.agm, &sbi->s_fs_uuid);
}

void voluta_clear_unwritten(const struct voluta_vnode_info *vi)
{
	const struct voluta_vaddr *vaddr = v_vaddr_of(vi);
	struct voluta_vnode_info *agm_vi = vi->bki->b_ua_vi;

	voluta_assert(vtype_isagmap(vtype_of(agm_vi)));
	voluta_assert_not_null(agm_vi);
	voluta_assert_eq(vaddr->vtype, VOLUTA_VTYPE_DATA);

	agm_clear_unwritten_at(agm_vi->u.agm, vaddr);
	v_dirtify(agm_vi);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void deallocate_space_at(struct voluta_vnode_info *agm_vi,
				const struct voluta_vaddr *vaddr)
{
	agm_clear_active_space(agm_vi->u.agm, vaddr);
	agm_renew_if_unused(agm_vi->u.agm, vaddr);
	v_dirtify(agm_vi);
}

static int require_space(const struct voluta_vnode_info *agm_vi, size_t nkb)
{
	return agm_has_space(agm_vi->u.agm, nkb) ? 0 : -ENOSPC;
}

static int allocate_space_from(struct voluta_vnode_info *agm_vi,
			       enum voluta_vtype vtype, size_t nkb,
			       struct voluta_vaddr *vaddr)
{
	int err;

	err = require_space(agm_vi, nkb);
	if (err) {
		return err;
	}
	err = agm_find_free_space(agm_vi->u.agm, vtype, nkb, vaddr);
	if (err) {
		return err;
	}
	agm_mark_active_space(agm_vi->u.agm, vaddr);
	v_dirtify(agm_vi);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_prepare_space(struct voluta_sb_info *sbi,
			 const char *volume_path, loff_t size)
{
	int err;
	loff_t space_size;

	err = voluta_resolve_volume_size(volume_path, size, &space_size);
	if (err) {
		return err;
	}
	spi_setup(&sbi->s_spi, space_size);
	return 0;
}

static size_t nkb_to_nbytes(size_t nkb)
{
	return (nkb * VOLUTA_KB_SIZE);
}

static int check_space_alloc(struct voluta_sb_info *sbi,
			     enum voluta_vtype vtype, size_t nkb)
{
	bool ok;
	const bool new_file = vtype_isinode(vtype);
	const size_t nbytes = nkb_to_nbytes(nkb);

	if (vtype_isdata(vtype)) {
		ok = spi_may_alloc_data(&sbi->s_spi, nbytes);
	} else {
		ok = spi_may_alloc_meta(&sbi->s_spi, nbytes, new_file);
	}
	return ok ? 0 : -ENOSPC;
}

static void calc_stat_change(enum voluta_vtype vtype, size_t nbytes, int take,
			     struct voluta_space_stat *sp_st)
{
	sp_st->ndata = 0;
	sp_st->nmeta = 0;
	sp_st->nfiles = 0;

	if (take > 0) {
		if (vtype_isdata(vtype)) {
			sp_st->ndata = (ssize_t)nbytes;
		} else {
			sp_st->nmeta = (ssize_t)nbytes;
		}
		if (vtype_isinode(vtype)) {
			sp_st->nfiles = 1;
		}
	} else if (take < 0) {
		if (vtype_isdata(vtype)) {
			sp_st->ndata = -((ssize_t)nbytes);
		} else {
			sp_st->nmeta = -((ssize_t)nbytes);
		}
		if (vtype_isinode(vtype)) {
			sp_st->nfiles = -1;
		}
	}
}

static int update_space_stat(struct voluta_sb_info *sbi, int take,
			     const struct voluta_vaddr *vaddr)
{
	int err;
	struct voluta_space_stat sp_st;
	struct voluta_vnode_info *usm_vi = NULL;
	const size_t ag_index = vaddr->ag_index;
	const size_t nbytes = vaddr_len(vaddr);

	if (ag_index == 0) {
		/* No accounting to meta-elements of AG-0 */
		return 0;
	}
	err = stage_usmap_of(sbi, ag_index, &usm_vi);
	if (err) {
		return err;
	}
	calc_stat_change(vaddr->vtype, nbytes, take, &sp_st);
	spi_update_stats(&sbi->s_spi, &sp_st);

	usm_update_stats(usm_vi->u.usm, ag_index, &sp_st);
	v_dirtify(usm_vi);
	return 0;
}

static fsblkcnt_t bytes_to_fsblkcnt(ssize_t nbytes)
{
	return (fsblkcnt_t)nbytes / VOLUTA_DS_SIZE;
}

void voluta_space_to_statvfs(const struct voluta_sb_info *sbi,
			     struct statvfs *out_stvfs)
{
	const struct voluta_space_info *spi = &sbi->s_spi;
	const ssize_t nbytes_max = spi_calc_bytes_limit(spi);
	const ssize_t nbytes_use = spi_calc_bytes_inuse(spi);
	const ssize_t nfiles_max = spi_calc_files_limit(spi);

	voluta_assert_ge(nbytes_max, nbytes_use);
	voluta_assert_ge(nfiles_max, spi->sp_nfiles);

	out_stvfs->f_bsize = VOLUTA_BK_SIZE;
	out_stvfs->f_frsize = VOLUTA_DS_SIZE;
	out_stvfs->f_blocks = bytes_to_fsblkcnt(nbytes_max);
	out_stvfs->f_bfree = bytes_to_fsblkcnt(nbytes_max - nbytes_use);
	out_stvfs->f_bavail = out_stvfs->f_bfree;
	out_stvfs->f_files = (fsfilcnt_t)nfiles_max;
	out_stvfs->f_ffree = (fsfilcnt_t)(nfiles_max - spi->sp_nfiles);
	out_stvfs->f_favail = out_stvfs->f_ffree;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int spawn_bk_now(struct voluta_sb_info *sbi, loff_t lba,
			struct voluta_vnode_info *uag_vi,
			struct voluta_bk_info **out_bki)
{
	int err;

	err = voluta_cache_spawn_bk(sbi->s_cache, lba, uag_vi, out_bki);
	if (err != -ENOMEM) {
		return err;
	}
	err = voluta_commit_dirtyq(sbi, VOLUTA_F_NOW);
	if (err) {
		return err;
	}
	err = voluta_cache_spawn_bk(sbi->s_cache, lba, uag_vi, out_bki);
	if (err) {
		return err;
	}
	return 0;
}

static int spawn_bk(struct voluta_sb_info *sbi, loff_t lba,
		    struct voluta_vnode_info *uag_vi,
		    struct voluta_bk_info **out_bki)
{
	int err;

	err = spawn_bk_now(sbi, lba, uag_vi, out_bki);
	if (err) {
		return err;
	}
	(*out_bki)->b_sbi = sbi;
	return 0;
}

static int load_stable_bk(struct voluta_sb_info *sbi,
			  struct voluta_bk_info *bki)
{
	struct voluta_iv_key iv_key;
	const loff_t off = lba_to_off(bki->b_lba);

	iv_key_of(bki, &iv_key);
	return voluta_cstore_load_dec(sbi->s_cstore, off, &iv_key, bki->bk);
}

static bool has_unwritten_data(const struct voluta_agroup_map *agm,
			       loff_t lba, size_t kbn)
{
	struct voluta_vaddr vaddr;

	agm_resolve(agm, lba, kbn, &vaddr);
	return vaddr_isdata(&vaddr) && agm_is_unwritten_at(agm, &vaddr);
}

static void zero_unwritten_bo(union voluta_bo *bo)
{
	voluta_memzero(bo, sizeof(*bo));
}

static size_t kbn_of_bo(size_t bo_index)
{
	return bo_index * VOLUTA_NKB_IN_BO;
}

static void zero_unwritten(struct voluta_bk_info *bki)
{
	size_t kbn;
	union voluta_bk *bk = bki->bk;
	const struct voluta_vnode_info *agm_vi = bki->b_ua_vi;

	for (size_t i = 0; i < ARRAY_SIZE(bk->bo); ++i) {
		kbn = kbn_of_bo(i);
		if (has_unwritten_data(agm_vi->u.agm, bki->b_lba, kbn)) {
			zero_unwritten_bo(&bk->bo[i]);
		}
	}
}

static int load_stage_bk(struct voluta_sb_info *sbi,
			 struct voluta_bk_info *bki)
{
	int err;
	enum voluta_vtype vtype;

	err = load_stable_bk(sbi, bki);
	if (err) {
		return err;
	}
	bki->b_staged = 1;

	vtype = vtype_of(bki->b_ua_vi);
	if (vtype_isagmap(vtype)) {
		zero_unwritten(bki);
	}
	return 0;
}

static int stage_bk_of(struct voluta_sb_info *sbi, loff_t lba,
		       struct voluta_vnode_info *uag_vi,
		       struct voluta_bk_info **out_bki)
{
	int err;

	err = spawn_bk(sbi, lba, uag_vi, out_bki);
	if (err) {
		return err;
	}
	if ((*out_bki)->b_staged) {
		return 0;
	}
	err = load_stage_bk(sbi, *out_bki);
	if (err) {
		/* TODO: Detach, clean from cache */
		return err;
	}
	return 0;
}

static union voluta_view *view_of(const union voluta_kbs *kbs)
{
	const union voluta_view *view;

	view = container_of(kbs, union voluta_view, kbs);
	return (union voluta_view *)view;
}

static union voluta_view *view_at(const struct voluta_bk_info *bki, loff_t off)
{
	union voluta_bk *bk = bki->bk;
	const size_t kbn = off_to_kbn(off);

	return view_of(&bk->kbs[kbn]);
}

static int verify_vnode_at(const struct voluta_bk_info *bki,
			   const struct voluta_vaddr *vaddr)
{
	const union voluta_view *view = view_at(bki, vaddr->off);
	const struct voluta_sb_info *sbi = bki->b_sbi;

	return voluta_verify_view(view, sbi->s_cstore->crypto, vaddr->vtype);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void hint_exhausted(struct voluta_sb_info *sbi,
			   struct voluta_vnode_info *vi)
{
	voluta_cache_may_evict_vi(sbi->s_cache, vi);
}

static int allocate_from(struct voluta_sb_info *sbi, size_t ag_index,
			 enum voluta_vtype vtype, size_t nkb,
			 struct voluta_vaddr *out)
{
	int err;
	struct voluta_vnode_info *agm_vi;

	err = stage_agmap(sbi, ag_index, &agm_vi);
	if (err) {
		return err;
	}
	err = allocate_space_from(agm_vi, vtype, nkb, out);
	if (!err || (err != -ENOSPC)) {
		return err;
	}
	hint_exhausted(sbi, agm_vi);
	return -ENOSPC;
}

/*
 * TODO-0003: Track free whole-blocks per agi
 *
 * No need to try and allocate whole block from fragmented AG
 */
static int try_allocate_from(struct voluta_sb_info *sbi,
			     struct voluta_vnode_info *usm_vi,
			     enum voluta_vtype vtype, size_t nkb,
			     struct voluta_vaddr *out_vaddr)
{
	int err;
	size_t agi_beg;
	size_t agi_end;
	size_t agi_from;
	const size_t nbytes = nkb_to_size(nkb);

	if (!usm_may_alloc(usm_vi->u.usm, nbytes)) {
		return -ENOSPC;
	}
	usm_ag_range(usm_vi->u.usm, &agi_beg, &agi_end);
	voluta_assert_ge(agi_beg, 2);
	voluta_assert_gt(agi_end, agi_beg);

	agi_from = usm_find_avail(usm_vi->u.usm, agi_beg, nbytes);
	if (agi_from == VOLUTA_AG_INDEX_NULL) {
		return -ENOSPC;
	}
	for (size_t ag_index = agi_from; ag_index < agi_end; ++ag_index) {
		if (!usm_may_alloc_from(usm_vi->u.usm, ag_index, nbytes)) {
			continue;
		}
		err = allocate_from(sbi, ag_index, vtype, nkb, out_vaddr);
		if (!err || (err != -ENOSPC)) {
			return err;
		}
	}
	return -ENOSPC;
}

static int allocate_space(struct voluta_sb_info *sbi,
			  enum voluta_vtype vtype, size_t nkb,
			  struct voluta_vaddr *out_vaddr)
{
	int err;
	size_t usp_index;
	struct voluta_vnode_info *usm_vi;
	struct voluta_space_info *spi = &sbi->s_spi;

	usp_index = spi->sp_usp_index_lo;
	while (usp_index <= spi->sp_usp_count) {
		err = stage_usmap(sbi, usp_index, &usm_vi);
		if (err) {
			return err;
		}
		err = try_allocate_from(sbi, usm_vi, vtype, nkb, out_vaddr);
		if (!err || (err != -ENOSPC)) {
			return err;
		}
		usp_index++;
		spi->sp_usp_index_lo = usp_index;
	}
	return -ENOSPC;
}

static int deallocate_space(struct voluta_sb_info *sbi,
			    const struct voluta_vaddr *vaddr)
{
	int err;
	struct voluta_vnode_info *agm_vi;
	struct voluta_space_info *spi = &sbi->s_spi;
	const size_t usp_index = usp_index_of(vaddr->ag_index);

	err = stage_agmap(sbi, vaddr->ag_index, &agm_vi);
	if (err) {
		return err;
	}
	deallocate_space_at(agm_vi, vaddr);

	voluta_assert_ge(usp_index, 1);
	spi->sp_usp_index_lo = min(usp_index, spi->sp_usp_index_lo);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int find_cached_vi(const struct voluta_sb_info *sbi,
			  const struct voluta_vaddr *vaddr,
			  struct voluta_vnode_info **out_vi)
{
	return voluta_cache_lookup_vi(sbi->s_cache, vaddr, out_vi);
}

static int spawn_cached_vi(const struct voluta_sb_info *sbi,
			   const struct voluta_vaddr *vaddr,
			   struct voluta_vnode_info **out_vi)
{
	return voluta_cache_spawn_vi(sbi->s_cache, vaddr, out_vi);
}

static int spawn_bind_cached_vi(const struct voluta_sb_info *sbi,
				const struct voluta_vaddr *vaddr,
				struct voluta_bk_info *bki,
				struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi;

	err = spawn_cached_vi(sbi, vaddr, &vi);
	if (err) {
		return err;
	}
	bind_vnode_to_bk(vi, bki);
	*out_vi = vi;
	return 0;
}

static int spawn_bind_metamap(struct voluta_sb_info *sbi,
			      const struct voluta_vaddr *vaddr,
			      struct voluta_vnode_info *uag_vi,
			      struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_bk_info *bki;

	err = spawn_bk(sbi, vaddr->lba, uag_vi, &bki);
	if (err) {
		return err;
	}
	err = spawn_bind_cached_vi(sbi, vaddr, bki, out_vi);
	if (err) {
		return err;
	}
	return 0;
}

static int stage_metamap(struct voluta_sb_info *sbi,
			 const struct voluta_vaddr *vaddr,
			 struct voluta_vnode_info *ug_vi,
			 struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_bk_info *bki;

	err = find_cached_vi(sbi, vaddr, out_vi);
	if (!err) {
		return 0; /* Cache hit */
	}
	err = stage_bk_of(sbi, vaddr->lba, ug_vi, &bki);
	if (err) {
		return err;
	}
	err = verify_vnode_at(bki, vaddr);
	if (err) {
		return err;
	}
	err = spawn_bind_cached_vi(sbi, vaddr, bki, out_vi);
	if (err) {
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void promote_cache_cycle(struct voluta_sb_info *sbi)
{
	voluta_cache_next_cycle(sbi->s_cache);
}

static int spawn_cached_usmap(struct voluta_sb_info *sbi, size_t usp_index,
			      struct voluta_vnode_info **out_vi)
{
	struct voluta_vaddr vaddr;

	vaddr_of_usmap(&vaddr, usp_index);
	return spawn_bind_metamap(sbi, &vaddr, NULL, out_vi);
}

static void setup_usmap(struct voluta_vnode_info *usm_vi, size_t usp_index)
{
	voluta_stamp_vnode(usm_vi);
	usm_init(usm_vi->u.usm, usp_index);
	usm_setup_iv_keys(usm_vi->u.usm);
}

static int format_usmap(struct voluta_sb_info *sbi, size_t usp_index)
{
	int err;
	struct voluta_vnode_info *usm_vi;

	err = spawn_cached_usmap(sbi, usp_index, &usm_vi);
	if (err) {
		return err;
	}
	setup_usmap(usm_vi, usp_index);
	v_dirtify(usm_vi);
	return 0;
}

int voluta_format_usmaps(struct voluta_sb_info *sbi)
{
	int err;
	const size_t usp_count = sbi->s_spi.sp_usp_count;

	voluta_assert_gt(usp_count, 0);
	voluta_assert_le(usp_count, VOLUTA_NUSP_MAX);

	for (size_t usp_index = 1; usp_index <= usp_count; ++usp_index) {
		err = format_usmap(sbi, usp_index);
		if (err) {
			return err;
		}
		err = voluta_commit_dirtyq(sbi, VOLUTA_F_NOW);
		if (err) {
			return err;
		}
		promote_cache_cycle(sbi);
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int spawn_cached_agmap(struct voluta_sb_info *sbi, size_t ag_index,
			      struct voluta_vnode_info *ug_vi,
			      struct voluta_vnode_info **out_vi)
{
	struct voluta_vaddr vaddr;

	vaddr_of_agmap(&vaddr, ag_index);
	return spawn_bind_metamap(sbi, &vaddr, ug_vi, out_vi);
}

static int format_agmap(struct voluta_sb_info *sbi, size_t ag_index,
			struct voluta_vnode_info *usm_vi)
{
	int err;
	struct voluta_vnode_info *agm_vi;

	err = spawn_cached_agmap(sbi, ag_index, usm_vi, &agm_vi);
	if (err) {
		return err;
	}
	setup_agmap(sbi, agm_vi);
	v_dirtify(agm_vi);

	usm_set_formatted(usm_vi->u.usm, ag_index, psize_of(agm_vi));
	v_dirtify(usm_vi);

	return 0;
}

int voluta_format_agmaps(struct voluta_sb_info *sbi)
{
	int err;
	struct voluta_vnode_info *usm_vi = NULL;
	const size_t ag_count = sbi->s_spi.sp_ag_count;

	for (size_t ag_index = 2; ag_index < ag_count; ++ag_index) {
		err = stage_usmap_of(sbi, ag_index, &usm_vi);
		if (err) {
			return err;
		}
		err = format_agmap(sbi, ag_index, usm_vi);
		if (err) {
			return err;
		}
		err = voluta_commit_dirtyq(sbi, VOLUTA_F_NOW);
		if (err) {
			return err;
		}
		promote_cache_cycle(sbi);
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void fill_super_block(struct voluta_super_block *sb)
{
	sb_setup(sb, time(NULL));
}

int voluta_format_sb(struct voluta_sb_info *sbi)
{
	loff_t off;
	const union voluta_bk *sb_bk;

	fill_super_block(sbi->sb);

	off = lba_to_off(VOLUTA_LBA_SUPER);
	sb_bk = sb_to_bk(sbi->sb);
	return voluta_cstore_enc_save(sbi->s_cstore, off,
				      &sbi->s_iv_key, sb_bk);
}

int voluta_load_sb(struct voluta_sb_info *sbi)
{
	int err;
	loff_t off;
	struct voluta_super_block *sb = sbi->sb;
	const struct voluta_iv_key *iv_key = &sbi->s_iv_key;

	off = lba_to_off(VOLUTA_LBA_SUPER);
	err = voluta_cstore_load_dec(sbi->s_cstore, off, iv_key, sb_to_bk(sb));
	if (err) {
		return err;
	}

	/* TODO: verify */

	/* XXX FIXME */
	/* TODO: call voluta_verify_vbk properly HERE !!! */

	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int load_usmap(struct voluta_sb_info *sbi, size_t usp_index)
{
	int err;
	struct voluta_vnode_info *agm_vi;
	struct voluta_space_stat sp_st = {
		.ndata = 0,
		.nmeta = 0,
		.nfiles = 0
	};

	err = stage_usmap(sbi, usp_index, &agm_vi);
	if (err) {
		return err;
	}
	usm_total_stat(agm_vi->u.usm, &sp_st);
	spi_accum_stat(&sbi->s_spi, &sp_st);
	return 0;
}

int voluta_load_usmaps(struct voluta_sb_info *sbi)
{
	int err;
	const size_t usp_count = sbi->s_spi.sp_usp_count;

	voluta_assert_gt(usp_count, 0);
	voluta_assert_le(usp_count, VOLUTA_NUSP_MAX);

	for (size_t usp_index = 1; usp_index <= usp_count; ++usp_index) {
		err = load_usmap(sbi, usp_index);
		if (err) {
			return err;
		}
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int load_agmap(struct voluta_sb_info *sbi, size_t ag_index)
{
	int err;
	struct voluta_vnode_info *agm_vi;
	struct voluta_vnode_info *usm_vi;

	err = stage_agmap(sbi, ag_index, &agm_vi);
	if (err) {
		return err;
	}
	usm_vi = agm_vi->bki->b_ua_vi;
	if (!usm_is_formatted(usm_vi->u.usm, ag_index)) {
		return -EFSCORRUPTED;
	}
	return 0;
}

int voluta_load_agmaps(struct voluta_sb_info *sbi)
{
	int err;
	struct voluta_vnode_info *usm_vi = NULL;
	const size_t ag_count = sbi->s_spi.sp_ag_count;

	for (size_t ag_index = 2; ag_index < ag_count; ++ag_index) {
		err = stage_usmap_of(sbi, ag_index, &usm_vi);
		if (err) {
			return err;
		}
		err = load_agmap(sbi, ag_index);
		if (err) {
			return err;
		}
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_load_itable(struct voluta_sb_info *sbi)
{
	loff_t lba, agm_lba;
	struct voluta_vaddr vaddr;
	size_t ag_index = 2;

	/* TODO: Reference itable-root from super-block */
	agm_lba = lba_of_agm(ag_index);
	lba = lba_by_ag(agm_lba, 1);
	vaddr_by_lba(&vaddr, VOLUTA_VTYPE_ITNODE, lba);

	return voluta_reload_itable(sbi, &vaddr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int verify_vtype(enum voluta_vtype vtype)
{
	switch (vtype) {
	case VOLUTA_VTYPE_NONE:
	case VOLUTA_VTYPE_SUPER:
	case VOLUTA_VTYPE_USMAP:
	case VOLUTA_VTYPE_AGMAP:
	case VOLUTA_VTYPE_ITNODE:
	case VOLUTA_VTYPE_INODE:
	case VOLUTA_VTYPE_XANODE:
	case VOLUTA_VTYPE_HTNODE:
	case VOLUTA_VTYPE_RTNODE:
	case VOLUTA_VTYPE_SYMVAL:
	case VOLUTA_VTYPE_DATA:
		break;
	default:
		return -EFSCORRUPTED;
	}
	return 0;
}

static int verify_boctet(const struct voluta_boctet *bo)
{
	int err;

	err = verify_vtype(bo_vtype(bo));
	if (err) {
		return err;
	}
	if (bo->unwritten > 1) {
		return -EFSCORRUPTED;
	}
	return 0;
}

static int verify_bkref(const struct voluta_bkref *bkref)
{
	int err = 0;

	for (size_t i = 0; (i < ARRAY_SIZE(bkref->bo)) && !err; ++i) {
		err = verify_boctet(&bkref->bo[i]);
	}
	return err;
}

static int verify_agmap(const struct voluta_agroup_map *agm)
{
	int err;
	size_t usecnt = 0;
	const size_t nkb_used = agm_nkb_used(agm);
	const struct voluta_bkref *bkref;

	for (size_t i = 0; i < ARRAY_SIZE(agm->ag_bkref); ++i) {
		bkref = agm_bkref_at(agm, i);
		err = verify_bkref(bkref);
		if (err) {
			return err;
		}
		usecnt += bkref_nused(bkref);
	}
	if (usecnt != nkb_used) {
		return -EFSCORRUPTED;
	}
	return 0;
}

int voluta_verify_agroup_map(const struct voluta_agroup_map *agm)
{
	int err;

	if (agm_index(agm) > 0xFFFFFFFUL) { /* XXX */
		return -EFSCORRUPTED;
	}
	if (agm->ag_ciper_type != VOLUTA_CIPHER_AES256) {
		return -EFSCORRUPTED;
	}
	if (agm->ag_ciper_mode != VOLUTA_CIPHER_MODE_CBC) {
		return -EFSCORRUPTED;
	}
	err = verify_agmap(agm);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_verify_super_block(const struct voluta_super_block *sb)
{
	if (sb->s_version != VOLUTA_VERSION) {
		return -EFSCORRUPTED;
	}
	return 0;
}

int voluta_verify_uspace_map(const struct voluta_uspace_map *usm)
{
	size_t usp_index;
	const size_t usp_index_max = VOLUTA_NUSP_MAX;

	usp_index = usm_index(usm);
	if (usp_index >= usp_index_max) {
		return -EFSCORRUPTED;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void sbi_init_commons(struct voluta_sb_info *sbi,
			     struct voluta_cache *cache,
			     struct voluta_cstore *cstore)
{
	voluta_uuid_generate(&sbi->s_fs_uuid);
	voluta_init_itable(sbi);
	spi_init(&sbi->s_spi);
	sbi->s_owner = getuid();
	sbi->s_name = "voluta";
	sbi->s_namei_iconv = (iconv_t)(-1);
	sbi->s_birth_time = time(NULL);
	sbi->s_cache = cache;
	sbi->s_qalloc = cache->qal;
	sbi->s_cstore = cstore;
	sbi->s_pstore = &cstore->pstore;
}

static void sbi_fini_commons(struct voluta_sb_info *sbi)
{
	voluta_fini_itable(sbi);
	spi_fini(&sbi->s_spi);
	sbi->s_name = NULL;
	sbi->s_owner = (uid_t)(-1);
	sbi->s_cache = NULL;
	sbi->s_qalloc = NULL;
	sbi->s_cstore = NULL;
	sbi->s_pstore = NULL;
}

static int sbi_init_meta_bks(struct voluta_sb_info *sbi)
{
	int err;
	void *sb;
	struct voluta_qalloc *qal = sbi->s_cache->qal;

	err = voluta_zalloc(qal, sizeof(*sbi->sb), &sb);
	if (err) {
		return err;
	}
	sbi->sb = sb;
	return 0;
}

static void sbi_fini_meta_bks(struct voluta_sb_info *sbi)
{
	struct voluta_qalloc *qal = sbi->s_cache->qal;

	voluta_free(qal, sbi->sb, sizeof(*sbi->sb));
	sbi->sb = NULL;
}

static int sbi_init_iconv(struct voluta_sb_info *sbi)
{
	int err = 0;

	/* Using UTF32LE to avoid BOM (byte-order-mark) character */
	sbi->s_namei_iconv = iconv_open("UTF32LE", "UTF8");
	if (sbi->s_namei_iconv == (iconv_t)(-1)) {
		err = errno ? -errno : -ENOTSUP;
	}
	return err;
}

static void sbi_fini_iconv(struct voluta_sb_info *sbi)
{
	if (sbi->s_namei_iconv == (iconv_t)(-1)) {
		iconv_close(sbi->s_namei_iconv);
		sbi->s_namei_iconv = (iconv_t)(-1);
	}
}

int voluta_sbi_init(struct voluta_sb_info *sbi,
		    struct voluta_cache *cache, struct voluta_cstore *cstore)
{
	int err;

	sbi_init_commons(sbi, cache, cstore);
	err = sbi_init_meta_bks(sbi);
	if (err) {
		return err;
	}
	err = sbi_init_iconv(sbi);
	if (err) {
		/* TODO: cleanups */
		return err;
	}
	return 0;
}

void voluta_sbi_fini(struct voluta_sb_info *sbi)
{
	/* TODO: FINI SPR */

	sbi_fini_iconv(sbi);
	sbi_fini_meta_bks(sbi);
	sbi_fini_commons(sbi);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int check_stable_vaddr(const struct voluta_vnode_info *agm_vi,
			      const struct voluta_vaddr *vaddr)
{
	const enum voluta_vtype vtype = vtype_at(agm_vi, vaddr);

	return (vtype == vaddr->vtype) ? 0 : -EFSCORRUPTED;
}

static int stage_ag_and_bk_of(struct voluta_sb_info *sbi, bool stable,
			      const struct voluta_vaddr *vaddr,
			      struct voluta_bk_info **out_bki)
{
	int err;
	struct voluta_bk_info *bki;
	struct voluta_vnode_info *agm_vi;

	err = stage_agmap(sbi, vaddr->ag_index, &agm_vi);
	if (err) {
		return err;
	}
	if (stable) {
		err = check_stable_vaddr(agm_vi, vaddr);
		if (err) {
			return err;
		}
	}
	err = stage_bk_of(sbi, vaddr->lba, agm_vi, &bki);
	if (err) {
		return err;
	}
	if (stable) {
		err = verify_vnode_at(bki, vaddr);
		if (err) {
			return err;
		}
	}
	*out_bki = bki;
	return 0;
}

static void stamp_dirty_bk(const struct voluta_sb_info *sbi,
			   const struct voluta_bk_info *bki)
{
	struct voluta_dirty_set dset;
	struct voluta_vnode_info *vi;
	const struct voluta_crypto *crypto = sbi->s_cstore->crypto;

	voluta_dirty_set_of(bki, &dset);
	voluta_assert_gt(dset.cnt, 0);
	for (size_t i = 0; i < dset.cnt; ++i) {
		vi = dset.vi[i];
		if (!vaddr_isdata(v_vaddr_of(vi))) {
			voluta_seal_vnode(vi, crypto);
		}
	}
}

static int save_dirty_bk(struct voluta_sb_info *sbi,
			 const struct voluta_bk_info *bki)
{
	struct voluta_iv_key iv_key;
	const loff_t off = lba_to_off(bki->b_lba);

	iv_key_of(bki, &iv_key);
	return voluta_cstore_enc_save(sbi->s_cstore, off, &iv_key, bki->bk);
}

static struct voluta_bk_info *next_dirty_bk(const struct voluta_sb_info *sbi)
{
	int err;
	struct voluta_bk_info *bki = NULL;

	err = voluta_cache_get_dirty(sbi->s_cache, &bki);
	return !err ? bki : NULL;
}

static int flush_ndirty(struct voluta_sb_info *sbi, size_t limit)
{
	int err;
	struct voluta_bk_info *bki;

	for (size_t i = 0; i < limit; ++i) {
		/* TODO: Separate flush of data from meta based on flags */
		bki = next_dirty_bk(sbi);
		if (bki == NULL) {
			break;
		}
		stamp_dirty_bk(sbi, bki);
		err = save_dirty_bk(sbi, bki);
		if (err) {
			return err;
		}
		voluta_mark_as_clean(bki);
	}
	return 0;
}

static size_t dirty_count_of(const struct voluta_cache *cache)
{
	return cache->dirtyq.sz;
}

int voluta_commit_dirtyq(struct voluta_sb_info *sbi, int flags)
{
	int err;
	size_t dirty_count;

	dirty_count = dirty_count_of(sbi->s_cache);
	if (!flags && (dirty_count < 512)) { /* XXX urrr... */
		return 0;
	}
	err = flush_ndirty(sbi, dirty_count);
	if (err) {
		return err;
	}
	if (flags & VOLUTA_F_SYNC) {
		err = voluta_pstore_sync(sbi->s_pstore, true);
	}
	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void update_specific_view(struct voluta_vnode_info *vi)
{
	const enum voluta_vtype vtype = vi->vaddr.vtype;

	switch (vtype) {
	case VOLUTA_VTYPE_SUPER:
		vi->u.sb = &vi->view->bk.sb;
		break;
	case VOLUTA_VTYPE_USMAP:
		vi->u.usm = &vi->view->usm;
		break;
	case VOLUTA_VTYPE_AGMAP:
		vi->u.agm = &vi->view->agm;
		break;
	case VOLUTA_VTYPE_ITNODE:
		vi->u.itn = &vi->view->itn;
		break;
	case VOLUTA_VTYPE_INODE:
		vi->u.inode = &vi->view->inode;
		break;
	case VOLUTA_VTYPE_XANODE:
		vi->u.xan = &vi->view->xan;
		break;
	case VOLUTA_VTYPE_HTNODE:
		vi->u.htn = &vi->view->htn;
		break;
	case VOLUTA_VTYPE_RTNODE:
		vi->u.rtn = &vi->view->rtn;
		break;
	case VOLUTA_VTYPE_SYMVAL:
		vi->u.lnv = &vi->view->lnv;
		break;
	case VOLUTA_VTYPE_DATA:
		vi->u.ds = &vi->view->ds;
		break;
	case VOLUTA_VTYPE_NONE:
	default:
		break;
	}
}

static void bind_vnode_to_bk(struct voluta_vnode_info *vi,
			     struct voluta_bk_info *bki)
{
	const struct voluta_vaddr *vaddr = v_vaddr_of(vi);

	vi->view = view_at(bki, vaddr->off);
	update_specific_view(vi);
	voluta_attach_vi(vi, bki);
}

static int stage_usmap(struct voluta_sb_info *sbi, size_t usp_index,
		       struct voluta_vnode_info **out_vi)
{
	struct voluta_vaddr vaddr;

	vaddr_of_usmap(&vaddr, usp_index);
	return stage_metamap(sbi, &vaddr, NULL, out_vi);
}

static int stage_usmap_of(struct voluta_sb_info *sbi, size_t ag_index,
			  struct voluta_vnode_info **out_vi)
{
	const size_t usp_index = usp_index_of(ag_index);

	return stage_usmap(sbi, usp_index, out_vi);
}

static int stage_bind_agmap(struct voluta_sb_info *sbi, size_t ag_index,
			    struct voluta_vnode_info *usm_vi,
			    struct voluta_vnode_info **out_vi)
{
	struct voluta_vaddr vaddr;

	vaddr_of_agmap(&vaddr, ag_index);
	return stage_metamap(sbi, &vaddr, usm_vi, out_vi);
}

static int stage_agmap(struct voluta_sb_info *sbi, size_t ag_index,
		       struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *usm_vi;

	err = stage_usmap_of(sbi, ag_index, &usm_vi);
	if (err) {
		return err;
	}
	err = stage_bind_agmap(sbi, ag_index, usm_vi, out_vi);
	if (err) {
		return err;
	}
	return 0;
}

static void bind_inode_to_bk(struct voluta_inode_info *ii,
			     struct voluta_bk_info *bki)
{
	bind_vnode_to_bk(&ii->vi, bki);
	ii->inode = ii->vi.u.inode;
}

static int stage_inode(struct voluta_sb_info *sbi, bool stable,
		       const struct voluta_iaddr *iaddr,
		       struct voluta_inode_info **out_ii)
{
	int err;
	struct voluta_bk_info *bki;
	struct voluta_inode_info *ii;

	err = stage_ag_and_bk_of(sbi, stable, &iaddr->vaddr, &bki);
	if (err) {
		return err;
	}
	err = voluta_cache_spawn_ii(sbi->s_cache, iaddr, &ii);
	if (err) {
		return err;
	}
	bind_inode_to_bk(ii, bki);
	voluta_refresh_atime(ii, true);
	*out_ii = ii;
	return 0;
}

static int stage_vnode(struct voluta_sb_info *sbi, bool stable,
		       const struct voluta_vaddr *vaddr,
		       struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_bk_info *bki;

	err = stage_ag_and_bk_of(sbi, stable, vaddr, &bki);
	if (err) {
		return err;
	}
	err = spawn_bind_cached_vi(sbi, vaddr, bki, out_vi);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_stage_inode(struct voluta_sb_info *sbi, ino_t xino,
		       struct voluta_inode_info **out_ii)
{
	int err;
	ino_t ino;
	struct voluta_iaddr iaddr;

	err = voluta_real_ino(sbi, xino, &ino);
	if (err) {
		return err;
	}
	err = voluta_cache_lookup_ii(sbi->s_cache, ino, out_ii);
	if (!err) {
		return 0; /* Cache hit! */
	}
	err = voluta_resolve_ino(sbi, ino, &iaddr);
	if (err) {
		return err;
	}
	err = stage_inode(sbi, true, &iaddr, out_ii);
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

	err = find_cached_vi(sbi, vaddr, out_vi);
	if (!err) {
		return 0; /* Cache hit! */
	}
	err = stage_vnode(sbi, true, vaddr, out_vi);
	if (err) {
		return err;
	}
	return 0;
}

static int discard_if_falsely_bk(struct voluta_sb_info *sbi,
				 struct voluta_bk_info *bki)
{
	int err;
	size_t ag_index;
	struct voluta_vnode_info *agm_vi;

	ag_index = ag_index_of(bki->b_lba);
	err = stage_agmap(sbi, ag_index, &agm_vi);
	if (err) {
		return err;
	}
	if (agm_is_free_bk(agm_vi->u.agm, bki->b_lba)) {
		voluta_cache_forget_bk(sbi->s_cache, bki);
	}
	return 0;
}

static int resolve_nkb_of(enum voluta_vtype vtype, size_t *out_nkb)
{
	int err = 0;
	size_t psize;

	psize = vtype_psize(vtype);
	if (psize) {
		*out_nkb = size_to_nkb(psize);
	} else {
		err = -ENOSPC;
	}
	return err;
}

/*
 * Special case where block is cached, has just been marked 'unwritten, but not
 * zeroed yet.
 */
static void fixup_unwritten_cached_bk(struct voluta_sb_info *sbi, loff_t lba)
{
	int err;
	struct voluta_bk_info *bki;

	err = voluta_cache_lookup_bk(sbi->s_cache, lba, &bki);
	if (!err) {
		zero_unwritten(bki);
	}
}

int voluta_new_vspace(struct voluta_sb_info *sbi,
		      enum voluta_vtype vtype,
		      struct voluta_vaddr *out_vaddr)
{
	int err;
	size_t nkb;

	err = resolve_nkb_of(vtype, &nkb);
	if (err) {
		return err;
	}
	err = check_space_alloc(sbi, vtype, nkb);
	if (err) {
		return err;
	}
	err = allocate_space(sbi, vtype, nkb, out_vaddr);
	if (err) {
		/* TODO: cleanup */
		return err;
	}
	err = update_space_stat(sbi, 1, out_vaddr);
	if (err) {
		return err;
	}
	fixup_unwritten_cached_bk(sbi, out_vaddr->lba);

	return 0;
}

static int require_supported_itype(mode_t mode)
{
	return (S_ISDIR(mode) || S_ISREG(mode) || S_ISLNK(mode) ||
		S_ISSOCK(mode) || S_ISFIFO(mode)) ? 0 : -ENOTSUP;
}

/* TODO: cleanups and resource reclaim upon failure in every path */
int voluta_new_inode(struct voluta_sb_info *sbi,
		     const struct voluta_oper_info *opi,
		     mode_t mode, ino_t parent_ino,
		     struct voluta_inode_info **out_ii)
{
	int err;
	const enum voluta_vtype vtype = VOLUTA_VTYPE_INODE;
	struct voluta_iaddr iaddr;
	struct voluta_inode_info *ii;

	err = require_supported_itype(mode);
	if (err) {
		return err;
	}
	err = voluta_new_vspace(sbi, vtype, &iaddr.vaddr);
	if (err) {
		return err;
	}
	err = voluta_acquire_ino(sbi, &iaddr);
	if (err) {
		return err;
	}
	err = stage_inode(sbi, false, &iaddr, &ii);
	if (err) {
		/* TODO: spfree inode from ag */
		return err;
	}
	err = voluta_setup_new_inode(ii, opi, mode, parent_ino);
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
	struct voluta_vaddr vaddr;
	struct voluta_vnode_info *vi;

	err = voluta_new_vspace(sbi, vtype, &vaddr);
	if (err) {
		return err;
	}
	err = stage_vnode(sbi, false, &vaddr, &vi);
	if (err) {
		/* TODO: spfree inode from ag */
		return err;
	}
	voluta_stamp_vnode(vi);
	v_dirtify(vi);

	*out_vi = vi;
	return 0;
}

int voluta_del_inode(struct voluta_sb_info *sbi, struct voluta_inode_info *ii)
{
	int err;
	struct voluta_bk_info *bki = ii->vi.bki;
	const struct voluta_vaddr *vaddr = i_vaddr_of(ii);

	err = voluta_discard_ino(sbi, i_ino_of(ii));
	if (err) {
		return err;
	}
	err = deallocate_space(sbi, vaddr);
	if (err) {
		return err;
	}
	err = update_space_stat(sbi, -1, vaddr);
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

static int free_vspace(struct voluta_sb_info *sbi,
		       const struct voluta_vaddr *vaddr)
{
	int err;

	err = deallocate_space(sbi, vaddr);
	if (err) {
		return err;
	}
	err = update_space_stat(sbi, -1, vaddr);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_del_vnode(struct voluta_sb_info *sbi, struct voluta_vnode_info *vi)
{
	int err;
	struct voluta_bk_info *bki = vi->bki;

	err = free_vspace(sbi, v_vaddr_of(vi));
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

int voluta_del_vnode_at(struct voluta_sb_info *sbi,
			const struct voluta_vaddr *vaddr)
{
	int err;
	struct voluta_vnode_info *vi;

	err = find_cached_vi(sbi, vaddr, &vi);
	return err ? free_vspace(sbi, vaddr) : voluta_del_vnode(sbi, vi);
}
