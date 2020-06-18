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
#include "libvoluta.h"

#define AG_INDEX_NULL VOLUTA_AG_INDEX_NULL

union voluta_xaddr {
	struct voluta_vaddr vaddr;
	struct voluta_iaddr iaddr;
};

struct voluta_super_ctx {
	union voluta_xaddr xa;
	struct voluta_sb_info    *sbi;
	struct voluta_bk_info    *bki;
	struct voluta_vnode_info *pvi;
	struct voluta_inode_info *pii;
	struct voluta_vnode_info *vi;
	struct voluta_inode_info *ii;
	struct voluta_vaddr      *vaddr;
	struct voluta_iaddr      *iaddr;
	ino_t ino;
};

/* Local functions forward declarations */
static int stage_super(struct voluta_sb_info *sbi,
		       struct voluta_vnode_info **out_vi);
static int stage_usmap(struct voluta_sb_info *sbi, size_t usp_index,
		       struct voluta_vnode_info **out_vi);
static int stage_agmap(struct voluta_sb_info *sbi, size_t ag_index,
		       struct voluta_vnode_info **out_vi);
static int stage_usmap_of(struct voluta_sb_info *sbi, size_t ag_index,
			  struct voluta_vnode_info **out_vi);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool vtype_issuper(enum voluta_vtype vtype)
{
	return vtype_isequal(vtype, VOLUTA_VTYPE_SUPER);
}

static bool vtype_isusmap(enum voluta_vtype vtype)
{
	return vtype_isequal(vtype, VOLUTA_VTYPE_USMAP);
}

static bool vtype_isagmap(enum voluta_vtype vtype)
{
	return vtype_isequal(vtype, VOLUTA_VTYPE_AGMAP);
}

static bool vtype_isnone(enum voluta_vtype vtype)
{
	return vtype_isequal(vtype, VOLUTA_VTYPE_NONE);
}

static size_t vtype_psize(enum voluta_vtype vtype)
{
	return voluta_persistent_size(vtype);
}

static size_t vtype_nkbs(enum voluta_vtype vtype)
{
	return div_round_up(vtype_psize(vtype), VOLUTA_KB_SIZE);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool lba_isequal(loff_t lba1, loff_t lba2)
{
	return (lba1 == lba2);
}

static bool lba_isnull(loff_t lba)
{
	return lba_isequal(lba, VOLUTA_LBA_NULL);
}

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

static bool ag_index_isnull(size_t ag_index)
{
	return (ag_index == AG_INDEX_NULL);
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
	vaddr->ag_index = AG_INDEX_NULL;
	vaddr->off = VOLUTA_OFF_NULL;
	vaddr->lba = VOLUTA_LBA_NULL;
	vaddr->vtype = VOLUTA_VTYPE_NONE;
}

static void vaddr_reset2(struct voluta_vaddr *vaddr, enum voluta_vtype vtype)
{
	vaddr_reset(vaddr);
	vaddr->vtype = vtype;
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
		vaddr_reset2(vaddr, vtype);
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

bool voluta_vaddr_isdata(const struct voluta_vaddr *vaddr)
{
	return vtype_isdata(vaddr->vtype);
}

static void vaddr_of_super(struct voluta_vaddr *vaddr)
{
	vaddr_by_lba(vaddr, VOLUTA_VTYPE_SUPER, VOLUTA_LBA_SUPER);
}

static void vaddr_of_usmap(struct voluta_vaddr *vaddr, size_t usp_index)
{
	const loff_t off = usmap_off_by_index(usp_index);

	vaddr_by_off(vaddr, VOLUTA_VTYPE_USMAP, off);
	voluta_assert_eq(vaddr->ag_index, 1);
}

static void vaddr_of_agmap(struct voluta_vaddr *vaddr, size_t ag_index)
{
	vaddr_by_lba(vaddr, VOLUTA_VTYPE_AGMAP, lba_of_agm(ag_index));
}

static void vaddr_of_itnode(struct voluta_vaddr *vaddr, loff_t off)
{
	vaddr_by_off(vaddr, VOLUTA_VTYPE_ITNODE, off);
}

static size_t vaddr_nbytes(const struct voluta_vaddr *vaddr)
{
	return voluta_persistent_size(vaddr->vtype);
}

static bool vaddr_issuper(const struct voluta_vaddr *vaddr)
{
	return vtype_issuper(vaddr->vtype);
}

static bool vaddr_isusmap(const struct voluta_vaddr *vaddr)
{
	return vtype_isusmap(vaddr->vtype);
}

static bool vaddr_isagmap(const struct voluta_vaddr *vaddr)
{
	return vtype_isagmap(vaddr->vtype);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t nkbs_of(const struct voluta_vaddr *vaddr)
{
	return vtype_nkbs(vaddr->vtype);
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

size_t voluta_nkbs_of(const struct voluta_vaddr *vaddr)
{
	return nkbs_of(vaddr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void iv_clone(const struct voluta_iv *iv, struct voluta_iv *other)
{
	memcpy(other, iv, sizeof(*other));
}

static void iv_mask(struct voluta_iv *iv, uint64_t b)
{
	VOLUTA_STATICASSERT_EQ(ARRAY_SIZE(iv->iv), 2 * sizeof(b));

	for (size_t i = 0; i < ARRAY_SIZE(iv->iv); ++i) {
		iv->iv[i] ^= (uint8_t)(b >> (4 * i));
	}
}

static void iv_clone_mask(const struct voluta_iv *iv_in,
			  struct voluta_iv *iv_out, uint64_t b)
{
	iv_clone(iv_in, iv_out);
	iv_mask(iv_out, b);
}

static void key_clone(const struct voluta_key *key, struct voluta_key *other)
{
	memcpy(other, key, sizeof(*other));
}

static void iv_key_clone(const struct voluta_iv_key *iv_key,
			 struct voluta_iv_key *other)
{
	iv_clone(&iv_key->iv, &other->iv);
	key_clone(&iv_key->key, &other->key);
}

static void iv_key_reset(struct voluta_iv_key *iv_key)
{
	voluta_memzero(iv_key, sizeof(*iv_key));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static ssize_t calc_used_bytes(const struct voluta_space_stat *sp_st)
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

static uint32_t sb_version(const struct voluta_super_block *sb)
{
	return le32_to_cpu(sb->s_version);
}

static void sb_set_version(struct voluta_super_block *sb, uint32_t version)
{
	sb->s_version = cpu_to_le32(version);
}

static const char *sb_fs_name(const struct voluta_super_block *sb)
{
	return (const char *)(sb->s_fs_name.name);
}

static void sb_set_fs_name(struct voluta_super_block *sb, const char *name)
{
	const size_t name_max = sizeof(sb->s_fs_name.name) - 1;
	const size_t name_len = min(strlen(name), name_max);

	memcpy(sb->s_fs_name.name, name, name_len);
}

static time_t sb_birth_time(const struct voluta_super_block *sb)
{
	return (time_t)le64_to_cpu(sb->s_birth_time);
}

static void sb_set_birth_time(struct voluta_super_block *sb, time_t btime)
{
	sb->s_birth_time = cpu_to_le64((uint64_t)btime);
}

static size_t sb_ag_count(const struct voluta_super_block *sb)
{
	return le64_to_cpu(sb->s_ag_count);
}

static void sb_set_ag_count(struct voluta_super_block *sb, size_t ag_count)
{
	sb->s_ag_count = cpu_to_le64(ag_count);
}

static loff_t sb_itable_root(const struct voluta_super_block *sb)
{
	return off_to_cpu(sb->s_itable_root);
}

static void sb_set_itable_root(struct voluta_super_block *sb, loff_t off)
{
	sb->s_itable_root = cpu_to_off(off);
}

static void sb_init(struct voluta_super_block *sb)
{
	sb_set_version(sb, VOLUTA_VERSION);
	sb_set_itable_root(sb, VOLUTA_OFF_NULL);
	sb_set_ag_count(sb, 0);
}

static void sb_fill_random_iv_keys(struct voluta_super_block *sb)
{
	for (size_t i = 0; i < ARRAY_SIZE(sb->s_keys); ++i) {
		voluta_fill_random_key(&sb->s_keys[i]);
	}
	for (size_t j = 0; j < ARRAY_SIZE(sb->s_ivs); ++j) {
		voluta_fill_random_iv(&sb->s_ivs[j]);
	}
}

static void sb_fill_randoms(struct voluta_super_block *sb)
{
	voluta_uuid_generate(&sb->s_uuid);
	sb_fill_random_iv_keys(sb);
}

static void sb_iv_key_of(const struct voluta_super_block *sb,
			 loff_t off, struct voluta_iv_key *out_iv_key)
{
	const size_t iv_slot = (size_t)off % ARRAY_SIZE(sb->s_ivs);
	const size_t key_slot = (size_t)off % ARRAY_SIZE(sb->s_keys);

	iv_clone(&sb->s_ivs[iv_slot], &out_iv_key->iv);
	key_clone(&sb->s_keys[key_slot], &out_iv_key->key);
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

static bool agr_is_fragmented(const struct voluta_ag_record *agr)
{
	return agr_has_flags(agr, VOLUTA_AGF_FRAGMENTED);
}

static void agr_set_fragmented(struct voluta_ag_record *agr)
{
	agr_set_flags(agr, agr_flags(agr) | VOLUTA_AGF_FRAGMENTED);
}

static void agr_unset_fragmented(struct voluta_ag_record *agr)
{
	const enum voluta_ag_flags mask = VOLUTA_AGF_FRAGMENTED;

	agr_set_flags(agr, agr_flags(agr) & ~mask);
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
	const size_t nused = agr_used_space(agr);
	const size_t ag_size = VOLUTA_AG_SIZE;

	return ((nused + nbytes) <= ag_size);
}

static bool agr_may_alloc(const struct voluta_ag_record *agr, size_t nbytes)
{
	const size_t bo_size = VOLUTA_BO_SIZE;

	if (!agr_is_formatted(agr)) {
		return false;
	}
	if (!agr_has_nfree(agr, nbytes)) {
		return false;
	}
	if ((nbytes >= bo_size) && agr_is_fragmented(agr)) {
		return false;
	}
	return true;
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

static size_t usm_ag_index_beg(const struct voluta_uspace_map *usm)
{
	return ag_index_by_usmap(usm_index(usm), 0);
}

static size_t usm_ag_index_end(const struct voluta_uspace_map *usm)
{
	return usm_ag_index_beg(usm) + VOLUTA_NAG_IN_USP;
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
	const ssize_t diff = calc_used_bytes(sp_st);
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

static void usm_space_stat(const struct voluta_uspace_map *usm,
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
			      size_t ag_index, size_t nmeta_base)
{
	struct voluta_ag_record *agr = usm_record_of(usm, ag_index);
	struct voluta_space_stat sp_st = {
		.nmeta = (ssize_t)nmeta_base
	};

	agr_set_formatted(agr);
	usm_update_stats(usm, ag_index, &sp_st);
}

static bool usm_is_formatted(struct voluta_uspace_map *usm, size_t ag_index)
{
	return agr_is_formatted(usm_record_of(usm, ag_index));
}

static bool usm_is_fragmented(struct voluta_uspace_map *usm, size_t ag_index)
{
	return agr_is_fragmented(usm_record_of(usm, ag_index));
}

static bool usm_may_alloc(const struct voluta_uspace_map *usm, size_t nbytes)
{
	const size_t nbytes_max = VOLUTA_USP_SIZE;
	const size_t nbytes_cur = usm_nused(usm);

	voluta_assert_le(nbytes_cur, nbytes_max);

	return ((nbytes_cur + nbytes) <= nbytes_max);
}

static bool usm_may_alloc_from(const struct voluta_uspace_map *usm,
			       size_t ag_index, size_t nbytes)
{
	const struct voluta_ag_record *agr = usm_record_of(usm, ag_index);

	return agr_may_alloc(agr, nbytes);
}

static void usm_mark_fragmented(struct voluta_uspace_map *usm, size_t ag_index)
{
	agr_set_fragmented(usm_record_of(usm, ag_index));
}

static void usm_clear_fragmented(struct voluta_uspace_map *usm,
				 size_t ag_index)
{
	agr_unset_fragmented(usm_record_of(usm, ag_index));
}

static size_t usm_record_slot(const struct voluta_uspace_map *usm,
			      const struct voluta_ag_record *agr)
{
	voluta_assert(agr >= usm->u_agr);

	return (size_t)(agr - usm->u_agr);
}

static size_t usm_ag_index_tip(const struct voluta_uspace_map *usm)
{
	const size_t ag_size = VOLUTA_AG_SIZE;
	const size_t nused = usm_nused(usm);
	const size_t ag_index_beg = usm_ag_index_beg(usm);

	return ag_index_beg + (nused / ag_size);
}

static size_t usm_find_avail(const struct voluta_uspace_map *usm,
			     size_t ag_index_from, size_t nbytes)
{
	size_t beg;
	const struct voluta_ag_record *agr;

	agr = usm_record_of(usm, ag_index_from);
	beg = usm_record_slot(usm, agr);
	for (size_t i = beg; i < ARRAY_SIZE(usm->u_agr); ++i) {
		agr = &usm->u_agr[i];
		if (!agr_is_formatted(agr)) {
			break;
		}
		if (agr_may_alloc(agr, nbytes)) {
			return usm_resolve_ag_index(usm, agr);
		}
	}
	return AG_INDEX_NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static uint8_t mask_of(size_t kbn, size_t nkb)
{
	const unsigned int mask = ((1U << nkb) - 1) << kbn;

	return (uint8_t)mask;
}

static enum voluta_vtype bo_vtype(const struct voluta_boctet *bo)
{
	return (enum voluta_vtype)(bo->bo_vtype);
}

static void bo_set_vtype(struct voluta_boctet *bo, enum voluta_vtype vtype)
{
	bo->bo_vtype = (uint8_t)vtype;
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

	return (bo->bo_usemask && mask) ? bo_vtype(bo) : VOLUTA_VTYPE_NONE;
}

static void bo_set_refcnt(struct voluta_boctet *bo, uint32_t refcnt)
{
	bo->bo_refcnt = cpu_to_le32(refcnt);
}

static void bo_init(struct voluta_boctet *bo)
{
	bo_set_vtype(bo, VOLUTA_VTYPE_NONE);
	bo_set_refcnt(bo, 0);
	bo->bo_usemask = 0;
	bo->bo_unwritten = 0;
	bo->bo_reserved = 0;
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

	voluta_assert_eq(bo->bo_usemask & mask, 0);

	bo->bo_usemask |= mask;
	bo_set_vtype(bo, vtype);
}

static void bo_unset(struct voluta_boctet *bo, size_t kbn, size_t nkb)
{
	const uint8_t mask = mask_of(kbn, nkb);

	voluta_assert_eq(bo->bo_usemask & mask, mask);

	bo->bo_usemask &= (uint8_t)(~mask);
	if (!bo->bo_usemask) {
		bo_set_vtype(bo, VOLUTA_VTYPE_NONE);
		bo->bo_unwritten = 0;
	}
}

static size_t bo_usecnt(const struct voluta_boctet *bo)
{
	return voluta_popcount(bo->bo_usemask);
}

static size_t bo_freecnt(const struct voluta_boctet *bo)
{
	return VOLUTA_NKB_IN_BO - bo_usecnt(bo);
}

static bool bo_isfull(const struct voluta_boctet *bo)
{
	return (bo->bo_usemask == 0xFF);
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
		if ((bo->bo_usemask & mask) == 0) {
			*out_kbn = kbn;
			return 0;
		}
	}
	return -ENOSPC;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void bkref_init(struct voluta_bkref *bkref)
{
	bo_init_arr(bkref->b_oct, ARRAY_SIZE(bkref->b_oct));
}

static void bkref_init_arr(struct voluta_bkref *arr, size_t cnt)
{
	for (size_t i = 0; i < cnt; ++i) {
		bkref_init(&arr[i]);
	}
}

static void bkref_renew_key(struct voluta_bkref *bkref)
{
	voluta_fill_random_key(&bkref->b_key);
}

static void bkref_renew(struct voluta_bkref *bkref)
{
	bkref_renew_key(bkref);
	bo_init_arr(bkref->b_oct, ARRAY_SIZE(bkref->b_oct));
}

static void bkref_key(const struct voluta_bkref *bkref, struct voluta_key *key)
{
	key_clone(&bkref->b_key, key);
}

static size_t kbn_to_bo_index(size_t kbn)
{
	return kbn / VOLUTA_NKB_IN_BO;
}

static uint32_t bkref_crc(const struct voluta_bkref *bkref, size_t kbn)
{
	const size_t bo_index = kbn_to_bo_index(kbn);

	return le32_to_cpu(bkref->b_crc[bo_index]);
}

static void bkref_set_crc(struct voluta_bkref *bkref, size_t kbn, uint32_t crc)
{
	const size_t bo_index = kbn_to_bo_index(kbn);

	voluta_assert_eq(kbn % VOLUTA_NKB_IN_BO, 0);

	bkref->b_crc[bo_index] = cpu_to_le32(crc);
}

static struct voluta_boctet *
bkref_boctet_of(const struct voluta_bkref *bkref, size_t kbn)
{
	const size_t bo_index = kbn_to_bo_index(kbn);
	const  struct voluta_boctet *bo = &bkref->b_oct[bo_index];

	voluta_assert_lt(bo_index, ARRAY_SIZE(bkref->b_oct));
	return (struct voluta_boctet *)bo;
}

static void bkref_setn_kbn(struct voluta_bkref *bkref,
			   enum voluta_vtype vtype, size_t kbn, size_t nkb)
{
	struct voluta_boctet *bo = bkref_boctet_of(bkref, kbn);

	bo_set(bo, vtype, kbn_within_bo(kbn), nkb);
}

static void bkref_clearn_kbn(struct voluta_bkref *bkref,
			     size_t kbn, size_t nkb)
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

	return bo->bo_unwritten;
}

static void bkref_clear_unwritten(struct voluta_bkref *bkref, size_t kbn)
{
	struct voluta_boctet *bo = bkref_boctet_of(bkref, kbn);

	bo->bo_unwritten = 0;
}

static void bkref_mark_unwritten(struct voluta_bkref *bkref, size_t kbn)
{
	struct voluta_boctet *bo = bkref_boctet_of(bkref, kbn);

	bo->bo_unwritten = 1;
}

static size_t bkref_nused(const struct voluta_bkref *bkref)
{
	size_t cnt = 0;
	const struct voluta_boctet *bo;

	for (size_t i = 0; i < ARRAY_SIZE(bkref->b_oct); ++i) {
		bo = &bkref->b_oct[i];
		cnt += bo_usecnt(bo);
	}
	return cnt;
}

static bool bkref_isunused(const struct voluta_bkref *bkref)
{
	const struct voluta_boctet *bo;

	for (size_t i = 0; i < ARRAY_SIZE(bkref->b_oct); ++i) {
		bo = &bkref->b_oct[i];

		if (bo_usecnt(bo)) {
			return false;
		}
	}
	return true;
}

static int bkref_find_free(const struct voluta_bkref *bkref,
			   enum voluta_vtype vtype, size_t *out_kbn)
{
	int err;
	size_t kbn;
	const struct voluta_boctet *bo;
	const size_t nkb = vtype_nkbs(vtype);

	for (size_t boi = 0; boi < ARRAY_SIZE(bkref->b_oct); ++boi) {
		bo = &bkref->b_oct[boi];
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
	voluta_assert_le(nkb, VOLUTA_NKB_IN_AG);
	agm->ag_nkb_used = cpu_to_le32((uint32_t)nkb);
}

static size_t agm_nbytes_used(const struct voluta_agroup_map *agm)
{
	return agm_nkb_used(agm) * VOLUTA_KB_SIZE;
}

static void agm_init(struct voluta_agroup_map *agm, size_t ag_index)
{
	agm_set_index(agm, ag_index);
	agm_set_nkb_used(agm, 0);
	agm->ag_ciper_type = VOLUTA_CIPHER_AES256;
	agm->ag_ciper_mode = VOLUTA_CIPHER_MODE_CBC;
	bkref_init_arr(agm->ag_bkref, ARRAY_SIZE(agm->ag_bkref));
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
agm_bkref_by_vaddr(const struct voluta_agroup_map *agm,
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

static const struct voluta_iv *
agm_iv_by_lba(const struct voluta_agroup_map *agm, loff_t lba)
{
	const size_t idx = (size_t)lba % ARRAY_SIZE(agm->ag_ivs);

	return &agm->ag_ivs[idx];
}

static void agm_iv_key_of(const struct voluta_agroup_map *agm, loff_t off,
			  struct voluta_iv_key *out_iv_key)
{
	const struct voluta_iv *iv;
	const struct voluta_bkref *bkref;
	const loff_t lba = off_to_lba(off);

	iv = agm_iv_by_lba(agm, lba);
	iv_clone_mask(iv, &out_iv_key->iv, (uint64_t)off);

	bkref = agm_bkref_by_lba(agm, lba);
	bkref_key(bkref, &out_iv_key->key);
}

static void agm_setup_ivs(struct voluta_agroup_map *agm)
{
	for (size_t i = 0; i < ARRAY_SIZE(agm->ag_ivs); ++i) {
		voluta_fill_random_iv(&agm->ag_ivs[i]);
	}
}

static void agm_setup_keys(struct voluta_agroup_map *agm)
{
	for (size_t i = 0; i < ARRAY_SIZE(agm->ag_bkref); ++i) {
		bkref_renew_key(agm_bkref_at(agm, i));
	}
}

static int agm_find_nfree_at(const struct voluta_agroup_map *agm,
			     enum voluta_vtype vtype,  size_t bn,
			     size_t *out_kbn)
{
	const struct voluta_bkref *bkref = agm_bkref_of(agm, bn);

	return bkref_find_free(bkref, vtype, out_kbn);
}

static int agm_find_free(const struct voluta_agroup_map *agm,
			 enum voluta_vtype vtype,
			 size_t *out_bn, size_t *out_kbn)
{
	int err = -ENOSPC;
	size_t bn;
	size_t kbn = 0;
	const size_t nkb_in_bk = VOLUTA_NKB_IN_BK;
	const size_t nbk_in_ag = VOLUTA_NBK_IN_AG;
	const size_t nbkrefs = ARRAY_SIZE(agm->ag_bkref);

	/* Heuristic logic for mostly write-append patterns */
	bn = (agm_nkb_used(agm) / nkb_in_bk);

	for (size_t i = 0; i < nbkrefs; ++i) {
		bn = clamp(bn, 1, nbkrefs);
		err = agm_find_nfree_at(agm, vtype, bn, &kbn);
		if (!err) {
			*out_bn = bn;
			*out_kbn = kbn;
			break;
		}
		bn = (bn + 1) % nbk_in_ag;
	}
	return err;
}

static int agm_find_free_space(const struct voluta_agroup_map *agm,
			       enum voluta_vtype vtype,
			       struct voluta_vaddr *out_vaddr)
{
	int err;
	size_t bn;
	size_t kbn;

	err = agm_find_free(agm, vtype, &bn, &kbn);
	if (err) {
		return err;
	}
	vaddr_setup(out_vaddr, vtype, agm_index(agm), bn, kbn);
	return 0;
}

static void agm_mark_unwritten_at(struct voluta_agroup_map *agm,
				  const struct voluta_vaddr *vaddr)
{
	struct voluta_bkref *bkref = agm_bkref_by_vaddr(agm, vaddr);

	bkref_mark_unwritten(bkref, kbn_of(vaddr));
}

static int agm_is_unwritten_at(const struct voluta_agroup_map *agm,
			       const struct voluta_vaddr *vaddr)
{
	const struct voluta_bkref *bkref = agm_bkref_by_vaddr(agm, vaddr);

	return bkref_is_unwritten(bkref, kbn_of(vaddr));
}

static void agm_clear_unwritten_at(struct voluta_agroup_map *agm,
				   const struct voluta_vaddr *vaddr)
{
	struct voluta_bkref *bkref = agm_bkref_by_vaddr(agm, vaddr);

	bkref_clear_unwritten(bkref, kbn_of(vaddr));
}

static void agm_inc_used_space(struct voluta_agroup_map *agm,
			       const struct voluta_vaddr *vaddr)
{
	agm_set_nkb_used(agm, agm_nkb_used(agm) + nkbs_of(vaddr));
}

static void agm_dec_used_space(struct voluta_agroup_map *agm,
			       const struct voluta_vaddr *vaddr)
{
	agm_set_nkb_used(agm, agm_nkb_used(agm) - nkbs_of(vaddr));
}

static void agm_mark_active_space(struct voluta_agroup_map *agm,
				  const struct voluta_vaddr *vaddr)
{
	struct voluta_bkref *bkref = agm_bkref_by_vaddr(agm, vaddr);

	bkref_setn_kbn(bkref, vaddr->vtype, kbn_of(vaddr), nkbs_of(vaddr));
	agm_inc_used_space(agm, vaddr);

	if (vaddr_isdata(vaddr)) {
		agm_mark_unwritten_at(agm, vaddr);
	}
}

static void agm_clear_active_space(struct voluta_agroup_map *agm,
				   const struct voluta_vaddr *vaddr)
{
	struct voluta_bkref *bkref = agm_bkref_by_vaddr(agm, vaddr);

	bkref_clearn_kbn(bkref, kbn_of(vaddr), nkbs_of(vaddr));
	agm_dec_used_space(agm, vaddr);
}

static void agm_renew_if_unused(struct voluta_agroup_map *agm,
				const struct voluta_vaddr *vaddr)
{
	struct voluta_bkref *bkref = agm_bkref_by_vaddr(agm, vaddr);

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
	const size_t nkb_in_ag = VOLUTA_NKB_IN_AG;
	const size_t nkb_used = agm_nkb_used(agm);

	voluta_assert_ge(nkb_used, vtype_nkbs(VOLUTA_VTYPE_AGMAP));

	return ((nkb_used + nkb) <= nkb_in_ag);
}

static uint32_t agm_crc(const struct voluta_agroup_map *agm,
			const struct voluta_vaddr *vaddr)
{
	const struct voluta_bkref *bkref = agm_bkref_by_vaddr(agm, vaddr);

	voluta_assert(vaddr_isdata(vaddr));

	return bkref_crc(bkref, kbn_of(vaddr));
}

static void agm_set_crc(struct voluta_agroup_map *agm,
			const struct voluta_vaddr *vaddr, uint32_t crc)
{
	struct voluta_bkref *bkref = agm_bkref_by_vaddr(agm, vaddr);

	voluta_assert(vaddr_isdata(vaddr));

	bkref_set_crc(bkref, kbn_of(vaddr), crc);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static enum voluta_vtype vtype_of(const struct voluta_vnode_info *vi)
{
	return (vi != NULL) ? v_vaddr_of(vi)->vtype : VOLUTA_VTYPE_NONE;
}

static bool v_isdata(const struct voluta_vnode_info *vi)
{
	return vtype_isdata(vtype_of(vi));
}

static bool v_isusmap(const struct voluta_vnode_info *vi)
{
	return vtype_isusmap(vtype_of(vi));
}

static bool v_isagmap(const struct voluta_vnode_info *vi)
{
	return vtype_isagmap(vtype_of(vi));
}

static void iv_key_of_vnode(const struct voluta_vnode_info *agm_vi,
			    loff_t off, struct voluta_iv_key *out_iv_key)
{
	agm_iv_key_of(agm_vi->u.agm, off, out_iv_key);
}

static void iv_key_of_agmap(const struct voluta_vnode_info *usm_vi,
			    size_t ag_index, struct voluta_iv_key *out_iv_key)
{
	usm_iv_key_of(usm_vi->u.usm, ag_index, out_iv_key);
}

static void iv_key_of_usmap(const struct voluta_vnode_info *sb_vi,
			    loff_t off, struct voluta_iv_key *out_iv_key)
{
	sb_iv_key_of(sb_vi->u.sb, off, out_iv_key);
}

static void iv_key_of_super(const struct voluta_sb_info *sbi,
			    struct voluta_iv_key *out_iv_key)
{
	iv_key_clone(&sbi->s_iv_key, out_iv_key);
}

void voluta_iv_key_of(const struct voluta_vnode_info *vi,
		      struct voluta_iv_key *out_iv_key)
{
	const struct voluta_sb_info *sbi = v_sbi_of(vi);
	const struct voluta_vaddr *vaddr = v_vaddr_of(vi);
	const struct voluta_vnode_info *pvi = vi->v_pvi;

	voluta_assert_not_null(sbi);

	if (vaddr_issuper(vaddr)) {
		voluta_assert_eq(vaddr->lba, VOLUTA_LBA_SUPER);
		iv_key_of_super(sbi, out_iv_key);
	} else if (vaddr_isusmap(vaddr)) {
		voluta_assert_null(pvi);
		iv_key_of_usmap(sbi->s_sb_vi, vaddr->off, out_iv_key);
	} else if (vaddr_isagmap(vaddr)) {
		voluta_assert_not_null(pvi);
		voluta_assert(v_isusmap(pvi));
		iv_key_of_agmap(pvi, vaddr->ag_index, out_iv_key);
	} else {
		voluta_assert_not_null(pvi);
		voluta_assert(v_isagmap(pvi));
		iv_key_of_vnode(pvi, vaddr->off, out_iv_key);
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

static ssize_t spi_space_limit(const struct voluta_space_info *spi)
{
	const size_t ag_size = VOLUTA_AG_SIZE;

	return (ssize_t)(ag_size * spi->sp_ag_count);
}

static ssize_t spi_calc_inodes_limit(const struct voluta_space_info *spi)
{
	const ssize_t inode_size = VOLUTA_INODE_SIZE;
	const ssize_t bytes_limit = spi_space_limit(spi);
	const ssize_t inodes_limit = (bytes_limit / inode_size) >> 2;

	return inodes_limit;
}

static bool spi_may_alloc_data(const struct voluta_space_info *spi, size_t nb)
{
	const ssize_t user_limit = (31 * spi_space_limit(spi)) / 32;
	const ssize_t used_bytes = spi_used_bytes(spi);

	return ((used_bytes + (ssize_t)nb) <= user_limit);
}

static bool spi_may_alloc_meta(const struct voluta_space_info *spi,
			       size_t nb, bool new_file)
{
	bool ret = true;
	ssize_t files_max;
	const ssize_t limit = spi_space_limit(spi);
	const ssize_t unsed = spi_used_bytes(spi);

	if ((unsed + (ssize_t)nb) > limit) {
		ret = false;
	} else if (new_file) {
		files_max = spi_calc_inodes_limit(spi);
		ret = (spi->sp_nfiles < files_max);
	}
	return ret;
}

static void spi_update_stats(struct voluta_space_info *spi,
			     const struct voluta_space_stat *sp_st)
{
	ssize_t nbytes_dif;
	ssize_t nbytes_max;
	ssize_t nbytes_use;

	spi->sp_used_data += sp_st->ndata;
	spi->sp_used_meta += sp_st->nmeta;
	spi->sp_nfiles += sp_st->nfiles;

	nbytes_max = spi_space_limit(spi);
	nbytes_use = spi_used_bytes(spi);
	nbytes_dif = nbytes_max - nbytes_use;
	voluta_assert_ge(nbytes_dif, 0);
}

static void spi_used_meta_upto(struct voluta_space_info *spi,
			       const struct voluta_vaddr *vaddr)
{
	spi->sp_used_meta = vaddr->off + (loff_t)vaddr_nbytes(vaddr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void setup_vnode_view(struct voluta_vnode_info *vi)
{
	union voluta_view *view = vi->view;
	const struct voluta_vaddr *vaddr = v_vaddr_of(vi);

	voluta_memzero(view, vaddr_nbytes(vaddr));
	if (!vaddr_isdata(vaddr)) {
		voluta_stamp_view(view, vaddr->vtype);
	}
	voluta_mark_visible(vi);
	v_dirtify(vi);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int vtype_at(const struct voluta_vnode_info *agm_vi,
		    const struct voluta_vaddr *vaddr)
{
	return agm_vtype_at(agm_vi->u.agm, vaddr);
}

static void setup_agmap(struct voluta_vnode_info *agm_vi)
{
	struct voluta_agroup_map *agm = agm_vi->u.agm;
	const struct voluta_vaddr *vaddr = v_vaddr_of(agm_vi);

	setup_vnode_view(agm_vi);
	agm_init(agm, vaddr->ag_index);
	agm_setup_ivs(agm);
	agm_setup_keys(agm);
	agm_set_nkb_used(agm, nkbs_of(vaddr));

	v_dirtify(agm_vi);
}

void voluta_clear_unwritten(const struct voluta_vnode_info *vi)
{
	const struct voluta_vaddr *vaddr = v_vaddr_of(vi);
	struct voluta_vnode_info *agm_vi = vi->v_pvi;

	voluta_assert_not_null(agm_vi);
	voluta_assert_eq(agm_vi->vaddr.vtype, VOLUTA_VTYPE_AGMAP);
	voluta_assert_eq(vaddr->vtype, VOLUTA_VTYPE_DATA);

	agm_clear_unwritten_at(agm_vi->u.agm, vaddr);
	v_dirtify(agm_vi);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int require_space(const struct voluta_vnode_info *agm_vi, size_t nkb)
{
	return agm_has_space(agm_vi->u.agm, nkb) ? 0 : -ENOSPC;
}

static int allocate_space_from(struct voluta_vnode_info *agm_vi,
			       enum voluta_vtype vtype,
			       struct voluta_vaddr *out_vaddr)
{
	int err;
	const size_t nkb = vtype_nkbs(vtype);

	err = require_space(agm_vi, nkb);
	if (err) {
		return err;
	}
	err = agm_find_free_space(agm_vi->u.agm, vtype, out_vaddr);
	if (err) {
		return err;
	}
	agm_mark_active_space(agm_vi->u.agm, out_vaddr);
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

static int check_avail_space(struct voluta_sb_info *sbi,
			     enum voluta_vtype vtype)
{
	bool ok;
	const bool new_file = vtype_isinode(vtype);
	const size_t nbytes = vtype_psize(vtype);

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
	const size_t nbytes = vtype_psize(vaddr->vtype);

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

void voluta_statvfs_of(const struct voluta_sb_info *sbi,
		       struct statvfs *out_stvfs)
{
	const struct voluta_space_info *spi = &sbi->s_spi;
	const ssize_t nbytes_max = spi_space_limit(spi);
	const ssize_t nbytes_use = spi_used_bytes(spi);
	const ssize_t nfiles_max = spi_calc_inodes_limit(spi);

	voluta_assert_ge(nbytes_max, nbytes_use);
	voluta_assert_ge(nfiles_max, spi->sp_nfiles);

	voluta_memzero(out_stvfs, sizeof(*out_stvfs));
	out_stvfs->f_bsize = VOLUTA_BK_SIZE;
	out_stvfs->f_frsize = VOLUTA_DS_SIZE;
	out_stvfs->f_blocks = bytes_to_fsblkcnt(nbytes_max);
	out_stvfs->f_bfree = bytes_to_fsblkcnt(nbytes_max - nbytes_use);
	out_stvfs->f_bavail = out_stvfs->f_bfree;
	out_stvfs->f_files = (fsfilcnt_t)nfiles_max;
	out_stvfs->f_ffree = (fsfilcnt_t)(nfiles_max - spi->sp_nfiles);
	out_stvfs->f_favail = out_stvfs->f_ffree;
	out_stvfs->f_namemax = VOLUTA_NAME_MAX;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_cache *cache_of(const struct voluta_super_ctx *s_ctx)
{
	return s_ctx->sbi->s_cache;
}

static struct voluta_cache *v_cache_of(const struct voluta_vnode_info *vi)
{
	const struct voluta_sb_info *sbi = v_sbi_of(vi);

	return sbi->s_cache;
}

static bool has_unwritten_at(const struct voluta_vnode_info *agm_vi,
			     const struct voluta_vaddr *vaddr)
{
	voluta_assert(vaddr_isdata(vaddr));

	return agm_is_unwritten_at(agm_vi->u.agm, vaddr);
}

static int find_cached_bk(struct voluta_sb_info *sbi, loff_t lba,
			  struct voluta_bk_info **out_bki)
{
	*out_bki = voluta_cache_lookup_bk(sbi->s_cache, lba);
	return (*out_bki != NULL) ? 0 : -ENOENT;
}

static int find_cached_bk_of(struct voluta_super_ctx *s_ctx)
{
	return find_cached_bk(s_ctx->sbi, s_ctx->vaddr->lba, &s_ctx->bki);
}

static int commit_dirty_now(const struct voluta_super_ctx *s_ctx)
{
	return voluta_commit_dirty(s_ctx->sbi, VOLUTA_F_NOW);
}

static int spawn_bk_of(struct voluta_super_ctx *s_ctx)
{
	int err;
	const loff_t lba = s_ctx->vaddr->lba;

	s_ctx->bki = voluta_cache_spawn_bk(s_ctx->sbi->s_cache, lba);
	if (s_ctx->bki != NULL) {
		return 0;
	}
	err = commit_dirty_now(s_ctx);
	if (err) {
		return err;
	}
	s_ctx->bki = voluta_cache_spawn_bk(s_ctx->sbi->s_cache, lba);
	if (s_ctx->bki == NULL) {
		return -ENOMEM;
	}
	return 0;
}

static int load_bk_of(struct voluta_super_ctx *s_ctx)
{
	struct voluta_bk_info *bki = s_ctx->bki;
	const loff_t off = lba_to_off(bki->b_lba);

	return voluta_cstore_stage_bk(s_ctx->sbi->s_cstore, off, bki->bk);
}

static void forget_bk(struct voluta_sb_info *sbi, struct voluta_bk_info *bki)
{
	voluta_cache_forget_bk(sbi->s_cache, bki);
}

static void forget_bk_of(struct voluta_super_ctx *s_ctx)
{
	forget_bk(s_ctx->sbi, s_ctx->bki);
	s_ctx->bki = NULL;
}

static int stage_bk_of(struct voluta_super_ctx *s_ctx)
{
	int err;

	err = find_cached_bk_of(s_ctx);
	if (!err) {
		return 0; /* Cache hit */
	}
	err = spawn_bk_of(s_ctx);
	if (err) {
		return err;
	}
	err = load_bk_of(s_ctx);
	if (err) {
		forget_bk_of(s_ctx);
		return err;
	}
	return 0;
}

static union voluta_view *view_of(const union voluta_kb *kbs)
{
	const union voluta_view *view;

	view = container_of(kbs, union voluta_view, kb);
	return (union voluta_view *)view;
}

static union voluta_view *view_at(const struct voluta_bk_info *bki, loff_t off)
{
	union voluta_bk *bk = bki->bk;
	const size_t kbn = off_to_kbn(off);

	return view_of(&bk->kb[kbn]);
}

static int verify_data(const struct voluta_vnode_info *vi)
{
	uint32_t ds_csum;
	uint32_t bo_csum;
	const struct voluta_vaddr *vaddr = v_vaddr_of(vi);
	const struct voluta_vnode_info *agm_vi = vi->v_pvi;

	if (has_unwritten_at(agm_vi, vaddr)) {
		return 0;
	}
	ds_csum = voluta_calc_chekcsum(vi);
	bo_csum = agm_crc(agm_vi->u.agm, vaddr);
	if (bo_csum != ds_csum) {
		return -EIO;
	}
	return 0;
}

static int check_vnode_view(const struct voluta_vnode_info *vi)
{
	int err = 0;

	if (v_isdata(vi)) {
		err = verify_data(vi);
	} else {
		err = voluta_verify_meta(vi);
	}
	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int allocate_from(struct voluta_sb_info *sbi,
			 size_t ag_index, enum voluta_vtype vtype,
			 struct voluta_vaddr *out_vaddr)
{
	int err;
	struct voluta_vnode_info *agm_vi;

	err = stage_agmap(sbi, ag_index, &agm_vi);
	if (err) {
		return err;
	}
	err = allocate_space_from(agm_vi, vtype, out_vaddr);
	if (err) {
		return err;
	}
	return 0;
}

static int try_allocate_nbytes(struct voluta_sb_info *sbi,
			       struct voluta_vnode_info *usm_vi,
			       size_t ag_index, enum voluta_vtype vtype,
			       size_t nbytes, struct voluta_vaddr *out_vaddr)
{
	int err;
	const bool big_alloc = (nbytes >= VOLUTA_BO_SIZE);

	if (!usm_may_alloc_from(usm_vi->u.usm, ag_index, nbytes)) {
		return -ENOSPC;
	}
	err = allocate_from(sbi, ag_index, vtype, out_vaddr);
	if (err != -ENOSPC) {
		return err;
	}
	if (big_alloc) {
		usm_mark_fragmented(usm_vi->u.usm, ag_index);
		v_dirtify(usm_vi);
	}
	return -ENOSPC;
}

static int try_allocate_within(struct voluta_sb_info *sbi,
			       struct voluta_vnode_info *usm_vi,
			       enum voluta_vtype vtype, size_t nbytes,
			       size_t ag_index_beg, size_t ag_index_end,
			       struct voluta_vaddr *out_vaddr)
{
	int err = -ENOSPC;
	size_t ag_index;

	ag_index = ag_index_beg;
	while (ag_index < ag_index_end) {
		ag_index = usm_find_avail(usm_vi->u.usm, ag_index, nbytes);
		if (ag_index_isnull(ag_index)) {
			break;
		}
		err = try_allocate_nbytes(sbi, usm_vi, ag_index,
					  vtype, nbytes, out_vaddr);
		if (err != -ENOSPC) {
			break;
		}
		ag_index++;
	}
	return err;
}

static int try_allocate_from(struct voluta_sb_info *sbi,
			     struct voluta_vnode_info *usm_vi,
			     enum voluta_vtype vtype,
			     struct voluta_vaddr *out_vaddr)
{
	int err;
	const size_t nbytes = vtype_psize(vtype);
	const size_t ag_index_beg = usm_ag_index_beg(usm_vi->u.usm);
	const size_t ag_index_end = usm_ag_index_end(usm_vi->u.usm);
	const size_t ag_index_tip = usm_ag_index_tip(usm_vi->u.usm);

	/* no space left */
	if (!usm_may_alloc(usm_vi->u.usm, nbytes)) {
		return -ENOSPC;
	}
	/* fast search */
	err = try_allocate_within(sbi, usm_vi, vtype, nbytes,
				  ag_index_tip, ag_index_end, out_vaddr);
	if (err != -ENOSPC) {
		return err;
	}
	/* slow search */
	err = try_allocate_within(sbi, usm_vi, vtype, nbytes,
				  ag_index_beg, ag_index_tip, out_vaddr);
	return err;
}

static int allocate_space(struct voluta_sb_info *sbi,
			  enum voluta_vtype vtype,
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
		err = try_allocate_from(sbi, usm_vi, vtype, out_vaddr);
		if (!err || (err != -ENOSPC)) {
			return err;
		}
		usp_index++;
		spi->sp_usp_index_lo = usp_index;
	}
	return -ENOSPC;
}

static void deallocate_space_at(struct voluta_vnode_info *agm_vi,
				const struct voluta_vaddr *vaddr)
{
	const size_t nkb = nkbs_of(vaddr);
	const size_t ag_index = vaddr->ag_index;
	struct voluta_vnode_info *usm_vi = agm_vi->v_pvi;

	voluta_assert_not_null(usm_vi);
	voluta_assert_eq(usm_vi->vaddr.vtype, VOLUTA_VTYPE_USMAP);

	agm_clear_active_space(agm_vi->u.agm, vaddr);
	agm_renew_if_unused(agm_vi->u.agm, vaddr);
	v_dirtify(agm_vi);

	if ((nkb > 1) && usm_is_fragmented(usm_vi->u.usm, ag_index)) {
		usm_clear_fragmented(usm_vi->u.usm, ag_index);
		v_dirtify(usm_vi);
	}
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

static void bind_view(struct voluta_vnode_info *vi, union voluta_view *view)
{
	vi->view = view;

	switch (vi->vaddr.vtype) {
	case VOLUTA_VTYPE_SUPER:
		vi->u.sb = &view->sb;
		break;
	case VOLUTA_VTYPE_USMAP:
		vi->u.usm = &view->usm;
		break;
	case VOLUTA_VTYPE_AGMAP:
		vi->u.agm = &view->agm;
		break;
	case VOLUTA_VTYPE_ITNODE:
		vi->u.itn = &view->itn;
		break;
	case VOLUTA_VTYPE_INODE:
		vi->u.inode = &view->inode;
		break;
	case VOLUTA_VTYPE_XANODE:
		vi->u.xan = &view->xan;
		break;
	case VOLUTA_VTYPE_HTNODE:
		vi->u.htn = &view->htn;
		break;
	case VOLUTA_VTYPE_RTNODE:
		vi->u.rtn = &view->rtn;
		break;
	case VOLUTA_VTYPE_SYMVAL:
		vi->u.lnv = &view->lnv;
		break;
	case VOLUTA_VTYPE_DATA:
		vi->u.ds = &view->ds;
		break;
	case VOLUTA_VTYPE_NONE:
	default:
		break;
	}
}

static void bind_vnode(struct voluta_super_ctx *s_ctx)
{
	struct voluta_vnode_info *vi = s_ctx->vi;
	const struct voluta_vaddr *vaddr = v_vaddr_of(vi);

	vi->v_sbi = s_ctx->sbi;
	bind_view(vi, view_at(s_ctx->bki, vaddr->off));
	voluta_attach_to(vi, s_ctx->bki, s_ctx->pvi, s_ctx->pii);
}

static void bind_inode(struct voluta_super_ctx *s_ctx)
{
	struct voluta_inode_info *ii = s_ctx->ii;

	s_ctx->vi = i_vi(ii);
	bind_vnode(s_ctx);
	ii->inode = ii->i_vi.u.inode;
}

static int find_cached_vi(struct voluta_super_ctx *s_ctx)
{
	s_ctx->vi = voluta_cache_lookup_vi(cache_of(s_ctx), s_ctx->vaddr);
	return (s_ctx->vi != NULL) ? 0 : -ENOENT;
}

static int spawn_vi(struct voluta_super_ctx *s_ctx)
{
	int err;
	struct voluta_cache *cache = cache_of(s_ctx);

	s_ctx->vi = voluta_cache_spawn_vi(cache, s_ctx->vaddr);
	if (s_ctx->vi != NULL) {
		return 0;
	}
	err = commit_dirty_now(s_ctx);
	if (err) {
		return err;
	}
	s_ctx->vi = voluta_cache_spawn_vi(cache, s_ctx->vaddr);
	if (s_ctx->vi == NULL) {
		return -ENOMEM;
	}
	return 0;
}

static int spawn_bind_vi(struct voluta_super_ctx *s_ctx)
{
	int err;

	err = spawn_vi(s_ctx);
	if (!err) {
		bind_vnode(s_ctx);
	}
	return err;
}

static void forget_cached_vi(struct voluta_vnode_info *vi)
{
	voulta_cache_forget_vi(v_cache_of(vi), vi);
}

static int spawn_ii(struct voluta_super_ctx *s_ctx)
{
	int err;
	struct voluta_cache *cache = cache_of(s_ctx);

	s_ctx->ii = voluta_cache_spawn_ii(cache, s_ctx->iaddr);
	if (s_ctx->ii != NULL) {
		return 0;
	}
	err = commit_dirty_now(s_ctx);
	if (err) {
		return err;
	}
	s_ctx->ii = voluta_cache_spawn_ii(cache, s_ctx->iaddr);
	if (s_ctx->ii == NULL) {
		return -ENOMEM;
	}
	s_ctx->vi = i_vi(s_ctx->ii);
	return 0;
}

static int spawn_bind_ii(struct voluta_super_ctx *s_ctx)
{
	int err;

	err = spawn_ii(s_ctx);
	if (!err) {
		bind_inode(s_ctx);
	}
	return err;
}

static void forget_cached_ii(struct voluta_inode_info *ii)
{
	voulta_cache_forget_ii(v_cache_of(i_vi(ii)), ii);
}

static int find_cached_or_spawn_bk(struct voluta_super_ctx *s_ctx)
{
	int err;

	err = find_cached_bk_of(s_ctx);
	if (err) {
		err = spawn_bk_of(s_ctx);
	}
	return err;
}

static int spawn_vmeta(struct voluta_super_ctx *s_ctx)
{
	int err;

	err = find_cached_or_spawn_bk(s_ctx);
	if (err) {
		return err;
	}
	err = spawn_bind_vi(s_ctx);
	if (err) {
		return err;
	}
	return 0;
}

static int spawn_meta(struct voluta_super_ctx *s_ctx,
		      struct voluta_vnode_info **out_vi)
{
	int err;

	err = spawn_vmeta(s_ctx);
	if (!err) {
		*out_vi = s_ctx->vi;
	}
	return err;
}

static int decrypt_and_review(struct voluta_super_ctx *s_ctx)
{
	int err;
	struct voluta_vnode_info *vi = s_ctx->vi;

	err = voluta_decrypt_reload(vi);
	if (err) {
		return err;
	}
	err = check_vnode_view(vi);
	if (err) {
		return err;
	}
	return 0;
}

static int stage_vmeta(struct voluta_super_ctx *s_ctx)
{
	int err;

	err = find_cached_vi(s_ctx);
	if (!err) {
		return 0; /* Cache hit */
	}
	err = stage_bk_of(s_ctx);
	if (err) {
		return err;
	}
	err = spawn_bind_vi(s_ctx);
	if (err) {
		return err;
	}
	err = decrypt_and_review(s_ctx);
	if (err) {
		forget_cached_vi(s_ctx->vi);
		return err;
	}
	return 0;
}

static int stage_meta(struct voluta_super_ctx *s_ctx,
		      struct voluta_vnode_info **out_vi)
{
	int err;

	err = stage_vmeta(s_ctx);
	if (!err) {
		*out_vi = s_ctx->vi;
	}
	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int spawn_super(struct voluta_sb_info *sbi,
		       struct voluta_vnode_info **out_vi)
{
	struct voluta_super_ctx s_ctx = {
		.sbi = sbi,
		.vaddr = &s_ctx.xa.vaddr
	};

	vaddr_of_super(s_ctx.vaddr);
	return spawn_meta(&s_ctx, out_vi);
}

static void attach_super(struct voluta_sb_info *sbi,
			 struct voluta_vnode_info *sb_vi)
{
	sbi->s_sb_vi = sb_vi;
	v_incref(sb_vi);
}

static void detach_super(struct voluta_sb_info *sbi)
{
	v_decref(sbi->s_sb_vi);
	sbi->s_sb_vi = NULL;
}

static time_t time_now(void)
{
	return time(NULL);
}

static void setup_super(struct voluta_sb_info *sbi,
			struct voluta_vnode_info *sb_vi, const char *fs_name)
{
	struct voluta_super_block *sb = sb_vi->u.sb;

	setup_vnode_view(sb_vi);
	sb_init(sb);
	sb_set_birth_time(sb, time_now());
	sb_set_fs_name(sb, fs_name);
	sb_set_ag_count(sb, sbi->s_spi.sp_ag_count);
	sb_fill_randoms(sb);
}

int voluta_format_super(struct voluta_sb_info *sbi, const char *name)
{
	int err;
	struct voluta_vnode_info *sb_vi;
	const char *fs_name = (name != NULL) ? name : "";

	err = spawn_super(sbi, &sb_vi);
	if (err) {
		return err;
	}
	setup_super(sbi, sb_vi, fs_name);
	attach_super(sbi, sb_vi);
	spi_used_meta_upto(&sbi->s_spi, v_vaddr_of(sb_vi));

	return 0;
}

static int check_super(const struct voluta_sb_info *sbi,
		       const struct voluta_vnode_info *sb_vi)
{
	const size_t ag_count = sb_ag_count(sb_vi->u.sb);
	const size_t sp_ag_count = sbi->s_spi.sp_ag_count;

	if (ag_count != sp_ag_count) {
		log_error("ag-count mismatch: sb=%lu expected=%lu",
			  ag_count, sp_ag_count);
		return -EFSCORRUPTED;
	}
	return 0;
}

int voluta_load_super(struct voluta_sb_info *sbi)
{
	int err;
	struct voluta_vnode_info *sb_vi = NULL;

	err = stage_super(sbi, &sb_vi);
	if (err) {
		return err;
	}
	err = check_super(sbi, sb_vi);
	if (err) {
		return err;
	}

	attach_super(sbi, sb_vi);
	spi_used_meta_upto(&sbi->s_spi, v_vaddr_of(sb_vi));

	return 0;
}

int voluta_shut_super(struct voluta_sb_info *sbi)
{
	int err;
	const struct voluta_vnode_info *sb_vi = sbi->s_sb_vi;

	if (sb_vi == NULL) {
		return 0;
	}
	log_debug("shut-super: name=%s", sb_fs_name(sb_vi->u.sb));

	detach_super(sbi);
	err = voluta_init_itable(sbi);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_bind_itable(struct voluta_sb_info *sbi,
		       struct voluta_vnode_info *it_root)
{
	struct voluta_vnode_info *sb_vi = sbi->s_sb_vi;
	const struct voluta_vaddr *vaddr = v_vaddr_of(it_root);

	sb_set_itable_root(sb_vi->u.sb, vaddr->off);
	v_dirtify(sb_vi);

	return 0;
}

int voluta_format_itable(struct voluta_sb_info *sbi)
{
	int err;
	struct voluta_vnode_info *sb_vi = sbi->s_sb_vi;
	const struct voluta_vaddr *itroot_vaddr = &sbi->s_itable.it_root_vaddr;

	err = voluta_create_itable(sbi);
	if (err) {
		return err;
	}
	sb_set_itable_root(sb_vi->u.sb, itroot_vaddr->off);
	v_dirtify(sb_vi);

	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int spawn_usmap(struct voluta_sb_info *sbi, size_t usp_index,
		       struct voluta_vnode_info **out_vi)
{
	struct voluta_super_ctx s_ctx = {
		.sbi = sbi,
		.vaddr = &s_ctx.xa.vaddr
	};

	vaddr_of_usmap(s_ctx.vaddr, usp_index);
	return spawn_meta(&s_ctx, out_vi);
}

static void setup_usmap(struct voluta_vnode_info *usm_vi, size_t usp_index)
{
	struct voluta_uspace_map *usm = usm_vi->u.usm;

	setup_vnode_view(usm_vi);
	usm_init(usm, usp_index);
	usm_setup_iv_keys(usm);

	v_dirtify(usm_vi);
}

static int format_usmap(struct voluta_sb_info *sbi, size_t usp_index)
{
	int err;
	struct voluta_vnode_info *usm_vi;

	err = spawn_usmap(sbi, usp_index, &usm_vi);
	if (err) {
		return err;
	}
	setup_usmap(usm_vi, usp_index);

	spi_used_meta_upto(&sbi->s_spi, v_vaddr_of(usm_vi));

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
		err = voluta_commit_dirty(sbi, VOLUTA_F_NOW);
		if (err) {
			return err;
		}
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int spawn_agmap(struct voluta_sb_info *sbi, size_t ag_index,
		       struct voluta_vnode_info *pvi,
		       struct voluta_vnode_info **out_vi)
{
	struct voluta_super_ctx s_ctx = {
		.sbi = sbi,
		.pvi = pvi,
		.vaddr = &s_ctx.xa.vaddr
	};

	vaddr_of_agmap(s_ctx.vaddr, ag_index);
	return spawn_meta(&s_ctx, out_vi);
}

static void update_spi_by_usm(struct voluta_sb_info *sbi,
			      const struct voluta_vnode_info *usm_vi)
{
	struct voluta_space_stat sp_st = {
		.ndata = 0,
		.nmeta = 0,
		.nfiles = 0
	};

	usm_space_stat(usm_vi->u.usm, &sp_st);
	spi_accum_stat(&sbi->s_spi, &sp_st);
}

static int format_agmap(struct voluta_sb_info *sbi, size_t ag_index,
			struct voluta_vnode_info *usm_vi)
{
	int err;
	size_t nmeta;
	struct voluta_vnode_info *agm_vi;

	err = spawn_agmap(sbi, ag_index, usm_vi, &agm_vi);
	if (err) {
		return err;
	}
	setup_agmap(agm_vi);

	nmeta = agm_nbytes_used(agm_vi->u.agm);
	usm_set_formatted(usm_vi->u.usm, ag_index, nmeta);
	v_dirtify(usm_vi);

	update_spi_by_usm(sbi, usm_vi);

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
		err = voluta_commit_dirty(sbi, VOLUTA_F_NOW);
		if (err) {
			return err;
		}
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int load_usmap(struct voluta_sb_info *sbi, size_t usp_index)
{
	int err;
	struct voluta_vnode_info *usm_vi;

	err = stage_usmap(sbi, usp_index, &usm_vi);
	if (err) {
		return err;
	}
	update_spi_by_usm(sbi, usm_vi);
	return 0;
}

int voluta_load_usmaps(struct voluta_sb_info *sbi)
{
	int err;
	struct voluta_vaddr vaddr;
	const size_t usp_count = sbi->s_spi.sp_usp_count;

	voluta_assert_gt(usp_count, 0);
	voluta_assert_le(usp_count, VOLUTA_NUSP_MAX);

	/* To first 2 AGs are resereved for super & US-maps */
	vaddr_of_agmap(&vaddr, 2);
	spi_used_meta_upto(&sbi->s_spi, &vaddr);

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
	usm_vi = agm_vi->v_pvi;
	voluta_assert_not_null(usm_vi);
	voluta_assert_eq(usm_vi->vaddr.vtype, VOLUTA_VTYPE_USMAP);

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

static int resolve_itroot(const struct voluta_sb_info *sbi,
			  struct voluta_vaddr *out_vaddr)
{
	loff_t off;
	const struct voluta_vnode_info *sb_vi = sbi->s_sb_vi;

	off = sb_itable_root(sb_vi->u.sb);
	vaddr_of_itnode(out_vaddr, off);

	return vaddr_isnull(out_vaddr) ? -ENOENT : 0;
}

static int new_ino_set(struct voluta_sb_info *sbi,
		       struct voluta_ino_set **out_ino_set)
{
	int err;
	void *ptr = NULL;

	err = voluta_zalloc(sbi->s_qalloc, sizeof(**out_ino_set), &ptr);
	if (!err) {
		*out_ino_set = ptr;
	}
	return err;
}

static void del_ino_set(struct voluta_sb_info *sbi,
			struct voluta_ino_set *ino_set)
{
	if (ino_set != NULL) {
		voluta_free(sbi->s_qalloc, ino_set, sizeof(*ino_set));
	}
}

static int scan_stage_root(struct voluta_sb_info *sbi,
			   const struct voluta_ino_set *ino_set,
			   struct voluta_inode_info **out_root_ii)
{
	int err;
	ino_t ino;
	struct voluta_inode_info *ii;

	for (size_t i = 0; i < ino_set->cnt; ++i) {
		ino = ino_set->ino[i];
		err = voluta_stage_inode(sbi, ino, &ii);
		if (err) {
			return err;
		}
		if (voluta_is_rootd(ii)) {
			*out_root_ii = ii;
			return 0;
		}
	}
	return -ENOENT;
}

static int scan_root_inode(struct voluta_sb_info *sbi,
			   struct voluta_inode_info **out_root_ii)
{
	int err;
	struct voluta_ino_set *ino_set = NULL;

	err = new_ino_set(sbi, &ino_set);
	if (err) {
		return err;
	}
	err = voluta_parse_itable_top(sbi, ino_set);
	if (err) {
		goto out;
	}
	err = scan_stage_root(sbi, ino_set, out_root_ii);
	if (err) {
		goto out;
	}
out:
	del_ino_set(sbi, ino_set);
	return err;
}

int voluta_load_itable(struct voluta_sb_info *sbi)
{
	int err;
	struct voluta_vaddr vaddr;
	struct voluta_inode_info *root_ii;

	err = resolve_itroot(sbi, &vaddr);
	if (err) {
		return err;
	}
	err = voluta_reload_itable(sbi, &vaddr);
	if (err) {
		return err;
	}
	err = scan_root_inode(sbi, &root_ii);
	if (err) {
		return err;
	}
	voluta_bind_root_ino(sbi, i_ino_of(root_ii));
	return 0;
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
	if (bo->bo_unwritten > 1) {
		return -EFSCORRUPTED;
	}
	return 0;
}

static int verify_bkref(const struct voluta_bkref *bkref)
{
	int err = 0;

	for (size_t i = 0; (i < ARRAY_SIZE(bkref->b_oct)) && !err; ++i) {
		err = verify_boctet(&bkref->b_oct[i]);
	}
	return err;
}

static int verify_agmap(const struct voluta_agroup_map *agm)
{
	int err;
	size_t nkb_cnt = 0;
	const size_t nkb_used = agm_nkb_used(agm);
	const size_t nkb_head = vtype_nkbs(VOLUTA_VTYPE_AGMAP);
	const struct voluta_bkref *bkref;

	for (size_t i = 0; i < ARRAY_SIZE(agm->ag_bkref); ++i) {
		bkref = agm_bkref_at(agm, i);
		err = verify_bkref(bkref);
		if (err) {
			return err;
		}
		nkb_cnt += bkref_nused(bkref);
	}
	if ((nkb_head + nkb_cnt) != nkb_used) {
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
	if (sb_version(sb) != VOLUTA_VERSION) {
		return -EFSCORRUPTED;
	}
	if (sb_birth_time(sb) <= 0) {
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

static size_t calc_iopen_limit(const struct voluta_cache *cache)
{
	return (cache->c_qalloc->st.memsz_data / (2 * VOLUTA_BK_SIZE));
}

static void sbi_init_commons(struct voluta_sb_info *sbi,
			     struct voluta_cache *cache,
			     struct voluta_cstore *cstore)
{
	voluta_uuid_generate(&sbi->s_fs_uuid);
	voluta_init_itable(sbi);
	spi_init(&sbi->s_spi);
	iv_key_reset(&sbi->s_iv_key);
	sbi->s_owner = getuid();
	sbi->s_namei_iconv = (iconv_t)(-1);
	sbi->s_cache = cache;
	sbi->s_qalloc = cache->c_qalloc;
	sbi->s_cstore = cstore;
	sbi->s_pstore = &cstore->pstore;
	sbi->s_sb_vi = NULL;
	sbi->s_iopen_limit = calc_iopen_limit(cache);
	sbi->s_iopen = 0;
}

static void sbi_fini_commons(struct voluta_sb_info *sbi)
{
	voluta_fini_itable(sbi);
	spi_fini(&sbi->s_spi);
	iv_key_reset(&sbi->s_iv_key);
	sbi->s_owner = (uid_t)(-1);
	sbi->s_cache = NULL;
	sbi->s_qalloc = NULL;
	sbi->s_cstore = NULL;
	sbi->s_pstore = NULL;
	sbi->s_sb_vi = NULL;
	sbi->s_iopen_limit = 0;
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
	err = sbi_init_iconv(sbi);
	if (err) {
		sbi_fini_commons(sbi);
	}
	return err;
}

void voluta_sbi_fini(struct voluta_sb_info *sbi)
{
	sbi_fini_iconv(sbi);
	sbi_fini_commons(sbi);
}

int voluta_sbi_reinit(struct voluta_sb_info *sbi)
{
	int err;
	struct voluta_iv_key iv_key;
	struct voluta_cache *cache = sbi->s_cache;
	struct voluta_cstore *cstore = sbi->s_cstore;

	iv_key_clone(&sbi->s_iv_key, &iv_key);
	voluta_sbi_fini(sbi);
	err = voluta_sbi_init(sbi, cache, cstore);
	if (err) {
		return err;
	}
	iv_key_clone(&iv_key, &sbi->s_iv_key);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void seal_data(const struct voluta_vnode_info *vi)
{
	uint32_t csum;
	struct voluta_vnode_info *agm_vi = vi->v_pvi;

	voluta_assert_eq(agm_vi->v_dirty, 1);

	csum = voluta_calc_chekcsum(vi);
	agm_set_crc(agm_vi->u.agm, v_vaddr_of(vi), csum);
}

static void seal_meta(const struct voluta_vnode_info *vi)
{
	voluta_seal_meta(vi);
}

static int seal_dirty_vnodes(const struct voluta_dirtymap *dmap)
{
	const struct voluta_vnode_info *vi;

	vi = voluta_dirtymap_first(dmap);
	while (vi != NULL) {
		if (v_isdata(vi)) {
			seal_data(vi);
		}
		vi = voluta_dirtymap_next(dmap, vi);
	}

	vi = voluta_dirtymap_first(dmap);
	while (vi != NULL) {
		if (!v_isdata(vi)) {
			seal_meta(vi);
		}
		vi = voluta_dirtymap_next(dmap, vi);
	}
	return 0;
}

static int flush_dirty_vnodes(const struct voluta_dirtymap *dset)
{
	int err;
	const struct voluta_vnode_info *vi;

	vi = voluta_dirtymap_first(dset);
	while (vi != NULL) {
		err = voluta_encrypt_destage(vi);
		if (err) {
			return err;
		}
		vi = voluta_dirtymap_next(dset, vi);
	}
	return 0;
}

static int flush_dirty(struct voluta_sb_info *sbi,
		       struct voluta_inode_info *ii)
{
	int err;
	struct voluta_dirtymap dmap;

	voluta_dirtymap_init(&dmap, sbi, ii);
	voluta_dirtymap_inhabit(&dmap);
	err = seal_dirty_vnodes(&dmap);
	if (err) {
		return err;
	}
	err = flush_dirty_vnodes(&dmap);
	if (err) {
		return err;
	}
	voluta_dirtymap_purge(&dmap);
	voluta_dirtymap_reset(&dmap);
	return 0;
}

static int stage_agmap_of(struct voluta_super_ctx *s_ctx)
{
	return stage_agmap(s_ctx->sbi, s_ctx->vaddr->ag_index, &s_ctx->pvi);
}

static int stage_parents(struct voluta_super_ctx *s_ctx)
{
	int err;

	err = stage_agmap_of(s_ctx);
	if (err) {
		return err;
	}
	err = stage_bk_of(s_ctx);
	if (err) {
		return err;
	}
	return 0;
}

static int check_stable_vaddr(const struct voluta_super_ctx *s_ctx)
{
	const enum voluta_vtype vtype = vtype_at(s_ctx->pvi, s_ctx->vaddr);

	return (vtype == s_ctx->vaddr->vtype) ? 0 : -EFSCORRUPTED;
}

static bool need_flush(const struct voluta_cache *cache)
{
	const size_t nbk_in_ag = VOLUTA_NBK_IN_AG;
	const size_t ndirty_vi = cache->c_vdq.lq.sz;
	const size_t npages_used = cache->c_qalloc->st.npages_used;
	const size_t npages_tota = cache->c_qalloc->st.npages;

	if (!ndirty_vi) {
		return false;
	}
	if (ndirty_vi > nbk_in_ag) {
		return true;
	}
	if (npages_used >= (npages_tota / 2)) {
		return true;
	}
	return false;
}

static int commit_last(struct voluta_sb_info *sbi, int flags)
{
	return (flags & VOLUTA_F_SYNC) ?
	       voluta_pstore_sync(sbi->s_pstore, true) : 0;
}

int voluta_commit_dirty(struct voluta_sb_info *sbi, int flags)
{
	int err;

	if (!flags && !need_flush(sbi->s_cache)) {
		return 0;
	}
	err = flush_dirty(sbi, NULL);
	if (err) {
		return err;
	}
	err = commit_last(sbi, flags);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_commit_dirty_of(struct voluta_inode_info *ii, int flags)
{
	int err;
	struct voluta_sb_info *sbi = i_sbi_of(ii);

	err = flush_dirty(sbi, ii);
	if (err) {
		return err;
	}
	err = commit_last(sbi, flags);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_flush_dirty(struct voluta_sb_info *sbi)
{
	return voluta_commit_dirty(sbi, VOLUTA_F_SYNC | VOLUTA_F_NOW);
}

int voluta_drop_caches(struct voluta_sb_info *sbi)
{
	voluta_cache_drop(sbi->s_cache);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int stage_super(struct voluta_sb_info *sbi,
		       struct voluta_vnode_info **out_vi)
{
	struct voluta_super_ctx s_ctx = {
		.sbi = sbi,
		.vaddr = &s_ctx.xa.vaddr
	};

	vaddr_of_super(s_ctx.vaddr);
	return stage_meta(&s_ctx, out_vi);
}

static int stage_usmap(struct voluta_sb_info *sbi, size_t usp_index,
		       struct voluta_vnode_info **out_vi)
{
	struct voluta_super_ctx s_ctx = {
		.sbi = sbi,
		.vaddr = &s_ctx.xa.vaddr
	};

	vaddr_of_usmap(s_ctx.vaddr, usp_index);
	return stage_meta(&s_ctx, out_vi);
}

static int stage_usmap_of(struct voluta_sb_info *sbi, size_t ag_index,
			  struct voluta_vnode_info **out_vi)
{
	const size_t usp_index = usp_index_of(ag_index);

	return stage_usmap(sbi, usp_index, out_vi);
}

static int stage_agmap(struct voluta_sb_info *sbi, size_t ag_index,
		       struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_super_ctx s_ctx = {
		.sbi = sbi,
		.vaddr = &s_ctx.xa.vaddr
	};

	vaddr_of_agmap(s_ctx.vaddr, ag_index);
	err = stage_usmap_of(sbi, ag_index, &s_ctx.pvi);
	if (err) {
		return err;
	}
	err = stage_meta(&s_ctx, out_vi);
	if (err) {
		return err;
	}
	return 0;
}

static int spawn_inode(struct voluta_super_ctx *s_ctx)
{
	int err;

	err = stage_parents(s_ctx);
	if (err) {
		return err;
	}
	err = spawn_bind_ii(s_ctx);
	if (err) {
		return err;
	}
	return 0;
}

static int stage_vnode(struct voluta_super_ctx *s_ctx)
{
	int err;

	err = find_cached_vi(s_ctx);
	if (!err) {
		return 0; /* Cache hit */
	}
	err = stage_parents(s_ctx);
	if (err) {
		return err;
	}
	err = check_stable_vaddr(s_ctx);
	if (err) {
		return err;
	}
	err = spawn_bind_vi(s_ctx);
	if (err) {
		return err;
	}
	err = decrypt_and_review(s_ctx);
	if (err) {
		forget_cached_vi(s_ctx->vi);
		return err;
	}
	return 0;
}

static int spawn_vnode(struct voluta_super_ctx *s_ctx)
{
	int err;

	err = stage_parents(s_ctx);
	if (err) {
		return err;
	}
	err = spawn_bind_vi(s_ctx);
	if (err) {
		return err;
	}
	return 0;
}

static int find_cached_ii(struct voluta_super_ctx *s_ctx, ino_t ino)
{
	s_ctx->ii = voluta_cache_lookup_ii(cache_of(s_ctx), ino);
	return (s_ctx->ii != NULL) ? 0 : -ENOENT;
}

static int stage_inode(struct voluta_super_ctx *s_ctx)
{
	int err;

	err = find_cached_ii(s_ctx, s_ctx->ino);
	if (!err) {
		return 0; /* Cache hit */
	}
	err = voluta_resolve_ino(s_ctx->sbi, s_ctx->ino, s_ctx->iaddr);
	if (err) {
		return err;
	}
	err = stage_parents(s_ctx);
	if (err) {
		return err;
	}
	err = check_stable_vaddr(s_ctx);
	if (err) {
		return err;
	}
	err = spawn_bind_ii(s_ctx);
	if (err) {
		return err;
	}
	err = decrypt_and_review(s_ctx);
	if (err) {
		forget_cached_ii(s_ctx->ii);
		return err;
	}
	voluta_refresh_atime(s_ctx->ii, true);
	return 0;
}

int voluta_stage_inode(struct voluta_sb_info *sbi, ino_t xino,
		       struct voluta_inode_info **out_ii)
{
	int err;
	struct voluta_super_ctx s_ctx = {
		.sbi = sbi,
		.vaddr = &s_ctx.xa.iaddr.vaddr,
		.iaddr = &s_ctx.xa.iaddr,
	};

	err = voluta_real_ino(sbi, xino, &s_ctx.ino);
	if (err) {
		return err;
	}
	err = stage_inode(&s_ctx);
	if (err) {
		return err;
	}
	*out_ii = s_ctx.ii;
	return 0;
}

int voluta_stage_vnode(struct voluta_sb_info *sbi,
		       const struct voluta_vaddr *vaddr,
		       struct voluta_inode_info *pii,
		       struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_super_ctx s_ctx = {
		.sbi = sbi,
		.pii = pii,
		.vaddr = &s_ctx.xa.vaddr
	};

	vaddr_clone(vaddr, s_ctx.vaddr);
	err = stage_vnode(&s_ctx);
	if (err) {
		return err;
	}
	*out_vi = s_ctx.vi;
	return 0;
}

static void bzero_unwritten_data(struct voluta_vnode_info *vi)
{
	struct voluta_data_seg *ds = &vi->view->ds;
	const struct voluta_vnode_info *agm_vi = vi->v_pvi;

	voluta_assert_not_null(vi->v_pvi);
	voluta_assert_eq(vi->vaddr.vtype, VOLUTA_VTYPE_DATA);
	voluta_assert_eq(vi->v_pvi->vaddr.vtype, VOLUTA_VTYPE_AGMAP);

	if (has_unwritten_at(agm_vi, v_vaddr_of(vi))) {
		voluta_memzero(ds, sizeof(*ds));
	}
}

int voluta_stage_data(struct voluta_sb_info *sbi,
		      const struct voluta_vaddr *vaddr,
		      struct voluta_inode_info *pii,
		      struct voluta_vnode_info **out_vi)
{
	int err;

	err = voluta_stage_vnode(sbi, vaddr, pii, out_vi);
	if (err) {
		return err;
	}
	bzero_unwritten_data(*out_vi);

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
		forget_bk(sbi, bki);
	}
	return 0;
}

static int alloc_space(struct voluta_super_ctx *s_ctx, enum voluta_vtype vtype)
{
	int err;

	err = check_avail_space(s_ctx->sbi, vtype);
	if (err) {
		return err;
	}
	err = allocate_space(s_ctx->sbi, vtype, s_ctx->vaddr);
	if (err) {
		/* TODO: cleanup */
		return err;
	}
	err = update_space_stat(s_ctx->sbi, 1, s_ctx->vaddr);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_alloc_vspace(struct voluta_sb_info *sbi,
			enum voluta_vtype vtype,
			struct voluta_vaddr *out_vaddr)
{
	int err;
	struct voluta_super_ctx s_ctx = {
		.sbi = sbi,
		.vaddr = &s_ctx.xa.vaddr
	};

	err = alloc_space(&s_ctx, vtype);
	if (err) {
		return err;
	}
	vaddr_clone(s_ctx.vaddr, out_vaddr);
	return 0;
}

static int require_supported_itype(mode_t mode)
{
	const mode_t sup = S_IFDIR | S_IFREG | S_IFLNK | S_IFSOCK | S_IFIFO;

	return (((mode & S_IFMT) | sup) == sup) ? 0 : -ENOTSUP;
}

/* TODO: cleanups and resource reclaim upon failure in every path */
int voluta_new_inode(struct voluta_sb_info *sbi,
		     const struct voluta_oper_ctx *op_ctx,
		     mode_t mode, ino_t parent_ino,
		     struct voluta_inode_info **out_ii)
{
	int err;
	struct voluta_super_ctx s_ctx = {
		.sbi = sbi,
		.vaddr = &s_ctx.xa.iaddr.vaddr,
		.iaddr = &s_ctx.xa.iaddr
	};

	err = require_supported_itype(mode);
	if (err) {
		return err;
	}
	err = alloc_space(&s_ctx, VOLUTA_VTYPE_INODE);
	if (err) {
		return err;
	}
	err = voluta_acquire_ino(s_ctx.sbi, s_ctx.iaddr);
	if (err) {
		return err;
	}
	err = spawn_inode(&s_ctx);
	if (err) {
		/* TODO: spfree inode from ag */
		return err;
	}
	setup_vnode_view(s_ctx.vi);

	voluta_setup_new_inode(s_ctx.ii, op_ctx, mode, parent_ino);
	*out_ii = s_ctx.ii;
	return 0;
}

/* TODO: cleanups and resource reclaim upon failure in every path */
int voluta_new_vnode(struct voluta_sb_info *sbi,
		     struct voluta_inode_info *pii,
		     enum voluta_vtype vtype,
		     struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_super_ctx s_ctx = {
		.sbi = sbi,
		.pii = pii,
		.vaddr = &s_ctx.xa.vaddr
	};

	err = alloc_space(&s_ctx, vtype);
	if (err) {
		return err;
	}
	err = spawn_vnode(&s_ctx);
	if (err) {
		/* TODO: spfree inode from ag */
		return err;
	}
	setup_vnode_view(s_ctx.vi);

	*out_vi = s_ctx.vi;
	return 0;
}

int voluta_del_inode(struct voluta_sb_info *sbi, struct voluta_inode_info *ii)
{
	int err;
	ino_t ino;
	struct voluta_vaddr vaddr;
	struct voluta_bk_info *bki = i_bki_of(ii);

	ino = i_ino_of(ii);
	vaddr_clone(i_vaddr_of(ii), &vaddr);

	voulta_cache_forget_ii(sbi->s_cache, ii);

	err = voluta_discard_ino(sbi, ino);
	if (err) {
		return err;
	}
	err = deallocate_space(sbi, &vaddr);
	if (err) {
		return err;
	}
	err = update_space_stat(sbi, -1, &vaddr);
	if (err) {
		return err;
	}
	err = discard_if_falsely_bk(sbi, bki);
	if (err) {
		return err;
	}
	return 0;
}

static void mark_opaque_at(struct voluta_sb_info *sbi,
			   const struct voluta_vaddr *vaddr)
{
	int err;
	struct voluta_bk_info *bki = NULL;

	err = find_cached_bk(sbi, vaddr->lba, &bki);
	if (!err) {
		voluta_mark_opaque_at(bki, vaddr);
	}
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

static int free_vspace_at(struct voluta_sb_info *sbi,
			  const struct voluta_vaddr *vaddr)
{
	int err;

	err = free_vspace(sbi, vaddr);
	if (!err) {
		mark_opaque_at(sbi, vaddr);
	}
	return err;
}

int voluta_del_vnode(struct voluta_sb_info *sbi, struct voluta_vnode_info *vi)
{
	int err;
	struct voluta_bk_info *bki = vi->v_bki;

	err = free_vspace(sbi, v_vaddr_of(vi));
	if (err) {
		return err;
	}

	voluta_mark_opaque(vi);
	forget_cached_vi(vi);

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
	struct voluta_super_ctx s_ctx = {
		.sbi = sbi,
		.vaddr = &s_ctx.xa.vaddr,

	};

	vaddr_clone(vaddr, s_ctx.vaddr);
	err = find_cached_vi(&s_ctx);
	if (!err) {
		err = voluta_del_vnode(s_ctx.sbi, s_ctx.vi);
	} else if (err == -ENOENT) {
		err = free_vspace_at(s_ctx.sbi, s_ctx.vaddr);
	}
	return err;
}
