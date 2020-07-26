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


union voluta_xaddr {
	struct voluta_vaddr vaddr;
	struct voluta_iaddr iaddr;
};

struct voluta_super_ctx {
	union voluta_xaddr xa;
	struct voluta_sb_info    *sbi;
	struct voluta_seg_info    *sgi;
	struct voluta_vnode_info *pvi;
	struct voluta_inode_info *pii;
	struct voluta_vnode_info *vi;
	struct voluta_inode_info *ii;
	struct voluta_vaddr      *vaddr;
	struct voluta_iaddr      *iaddr;
	ino_t ino;
};

static int stage_super(struct voluta_sb_info *sbi,
		       struct voluta_vnode_info **out_vi);
static int stage_usmap(struct voluta_sb_info *sbi, size_t usp_index,
		       struct voluta_vnode_info **out_vi);
static int stage_agmap(struct voluta_sb_info *sbi, size_t ag_index,
		       struct voluta_vnode_info **out_vi);


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

static struct voluta_cache *i_cache_of(const struct voluta_inode_info *ii)
{
	return v_cache_of(i_vi(ii));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void iv_clone(const struct voluta_iv *iv, struct voluta_iv *other)
{
	memcpy(other, iv, sizeof(*other));
}

static void iv_fill_random(struct voluta_iv *iv)
{
	voluta_getentropy(iv->iv, sizeof(iv->iv));
}

static void key_clone(const struct voluta_key *key, struct voluta_key *other)
{
	memcpy(other, key, sizeof(*other));
}

static void key_fill_random(struct voluta_key *key)
{
	voluta_getentropy(key->key, sizeof(key->key));
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

static unsigned int vtype_length(enum voluta_vtype vtype)
{
	return (unsigned int)voluta_persistent_size(vtype);
}

static size_t vtype_nkbs(enum voluta_vtype vtype)
{
	return div_round_up(vtype_length(vtype), VOLUTA_KB_SIZE);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool is_partial_bk(size_t nbytes)
{
	return (nbytes < VOLUTA_BK_SIZE);
}

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

static size_t off_to_kbn(loff_t off)
{
	return (size_t)(off / VOLUTA_KB_SIZE);
}

static loff_t lba_kbn_to_off(loff_t lba, size_t kbn)
{
	return lba_to_off(lba) + (loff_t)(kbn * VOLUTA_KB_SIZE);
}

static bool ag_index_isnull(size_t ag_index)
{
	return (ag_index == VOLUTA_AG_INDEX_NULL);
}

static size_t lba_to_ag_index(loff_t lba)
{
	return (size_t)(lba / VOLUTA_NBK_IN_AG);
}

static loff_t lba_by_ag(size_t ag_index, size_t bn)
{
	const loff_t nbk_in_ag = VOLUTA_NBK_IN_AG;

	voluta_assert_lt(bn, VOLUTA_NBK_IN_AG);

	return nbk_in_ag * (loff_t)ag_index + (loff_t)bn;
}

static size_t usp_index_of_ag(size_t ag_index)
{
	size_t usp_index = 0;
	const size_t nag_in_usp = VOLUTA_NAG_IN_USP;

	if (ag_index > 0) {
		usp_index = ((ag_index - 1) / nag_in_usp) + 1;
	}
	return usp_index;
}

static loff_t lba_of_usm(size_t ag_index)
{
	size_t usp_index;
	const size_t nbk_in_ag = VOLUTA_NBK_IN_AG;
	const size_t nbk_in_usp = VOLUTA_NBK_IN_USP;

	usp_index = usp_index_of_ag(ag_index);
	voluta_assert_gt(ag_index, 0);
	voluta_assert_gt(usp_index, 0);

	return (loff_t)(nbk_in_ag + ((usp_index - 1) * nbk_in_usp));
}

static loff_t lba_of_agm(size_t ag_index)
{
	const size_t ag_slot = (ag_index - 1) % VOLUTA_NBK_IN_AG;

	voluta_assert_gt(ag_index, 0);

	return lba_of_usm(ag_index) + (loff_t)ag_slot;
}

static loff_t usmap_lba_by_index(size_t usp_index)
{
	const size_t nbk_in_ag = VOLUTA_NBK_IN_AG;
	const size_t nbk_in_usp = VOLUTA_NBK_IN_USP;

	voluta_assert_gt(usp_index, 0);
	voluta_assert_le(usp_index, VOLUTA_NUSP_MAX);

	return (loff_t)(nbk_in_ag + ((usp_index - 1) * nbk_in_usp));
}

static size_t ag_index_by_usmap(size_t usp_index, size_t ag_slot)
{
	const size_t nag_in_usp = VOLUTA_NAG_IN_USP;

	voluta_assert_gt(usp_index, 0);
	voluta_assert_le(usp_index, VOLUTA_NUSP_MAX);

	return ((usp_index - 1) * nag_in_usp) + ag_slot + 1;
}

static bool ag_index_is_umeta(size_t ag_index)
{
	bool ret = true;
	loff_t lba_usm;
	loff_t lba_agm;

	if (ag_index > 0) {
		lba_usm = lba_of_usm(ag_index);
		lba_agm = lba_of_agm(ag_index);
		ret = (lba_usm == lba_agm);
	}
	return ret;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_vaddr_reset(struct voluta_vaddr *vaddr)
{
	vaddr->off = VOLUTA_OFF_NULL;
	vaddr->lba = VOLUTA_LBA_NULL;
	vaddr->vtype = VOLUTA_VTYPE_NONE;
	vaddr->len = 0;
}

static void vaddr_reset2(struct voluta_vaddr *vaddr, enum voluta_vtype vtype)
{
	vaddr_reset(vaddr);
	vaddr->vtype = vtype;
	vaddr->len = vtype_length(vtype);
}

void voluta_vaddr_clone(const struct voluta_vaddr *vaddr,
			struct voluta_vaddr *other)
{
	other->off = vaddr->off;
	other->lba = vaddr->lba;
	other->vtype = vaddr->vtype;
	other->len = vaddr->len;
}

static void vaddr_assign(struct voluta_vaddr *vaddr,
			 enum voluta_vtype vtype, loff_t lba, size_t kbn)
{
	vaddr->lba = lba;
	vaddr->off = lba_kbn_to_off(lba, kbn);
	vaddr->vtype = vtype;
	vaddr->len = vtype_length(vtype);
}

static size_t vaddr_ag_index(const struct voluta_vaddr *vaddr)
{
	return lba_to_ag_index(vaddr->lba);
}

static size_t vaddr_usp_index(const struct voluta_vaddr *vaddr)
{
	return usp_index_of_ag(vaddr_ag_index(vaddr));
}

static void vaddr_setup_by_ag(struct voluta_vaddr *vaddr,
			      enum voluta_vtype vtype,
			      size_t ag_index, size_t bn, size_t kbn)
{
	const loff_t lba = lba_by_ag(ag_index, bn);

	vaddr_assign(vaddr, vtype, lba, kbn);
}

static void vaddr_setup_by_lba(struct voluta_vaddr *vaddr,
			       enum voluta_vtype vtype, loff_t lba)
{
	voluta_assert(!lba_isnull(lba));

	vaddr_assign(vaddr, vtype, lba, 0);
}

static void vaddr_by_lba(struct voluta_vaddr *vaddr,
			 enum voluta_vtype vtype, loff_t lba)
{
	if (!lba_isnull(lba)) {
		vaddr_setup_by_lba(vaddr, vtype, lba);
	} else {
		vaddr_reset2(vaddr, vtype);
	}
}

void voluta_vaddr_by_off(struct voluta_vaddr *vaddr,
			 enum voluta_vtype vtype, loff_t off)
{
	if (!off_isnull(off)) {
		vaddr_setup_by_lba(vaddr, vtype, off_to_lba(off));
	} else {
		vaddr_reset2(vaddr, vtype);
	}
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
	const loff_t lba = usmap_lba_by_index(usp_index);

	vaddr_by_lba(vaddr, VOLUTA_VTYPE_USMAP, lba);

	voluta_assert_gt(lba, VOLUTA_LBA_SUPER);
	voluta_assert_gt(vaddr_ag_index(vaddr), 0);
	voluta_assert_eq(vaddr_usp_index(vaddr), usp_index);
}

static void vaddr_of_agmap(struct voluta_vaddr *vaddr, size_t ag_index)
{
	vaddr_by_lba(vaddr, VOLUTA_VTYPE_AGMAP, lba_of_agm(ag_index));
	voluta_assert_ne(vaddr_ag_index(vaddr), ag_index);
}

static void vaddr_of_itnode(struct voluta_vaddr *vaddr, loff_t off)
{
	vaddr_by_off(vaddr, VOLUTA_VTYPE_ITNODE, off);
}

static size_t vaddr_nbytes(const struct voluta_vaddr *vaddr)
{
	return voluta_persistent_size(vaddr->vtype);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t nkbs_of(const struct voluta_vaddr *vaddr)
{
	return vtype_nkbs(vaddr->vtype);
}

static size_t kbn_of(const struct voluta_vaddr *vaddr)
{
	const loff_t kb_size = VOLUTA_KB_SIZE;
	const loff_t nkb_in_bk = VOLUTA_NKB_IN_BK;
	const loff_t off = vaddr->off;

	voluta_assert_eq(vaddr->off % VOLUTA_KB_SIZE, 0);

	return (size_t)((off / kb_size) % nkb_in_bk);
}

static size_t kbn_within_bo(size_t kbn)
{
	return kbn % VOLUTA_NKB_IN_BK;
}

size_t voluta_nkbs_of(const struct voluta_vaddr *vaddr)
{
	return nkbs_of(vaddr);
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
		key_fill_random(&sb->s_keys[i]);
	}
	for (size_t j = 0; j < ARRAY_SIZE(sb->s_ivs); ++j) {
		iv_fill_random(&sb->s_ivs[j]);
	}
}

static void sb_fill_randoms(struct voluta_super_block *sb)
{
	voluta_uuid_generate(&sb->s_uuid);
	sb_fill_random_iv_keys(sb);
}

static void sb_iv_key_of(const struct voluta_super_block *sb,
			 loff_t lba, struct voluta_iv_key *out_iv_key)
{
	const size_t idx = (size_t)lba;
	const size_t iv_slot = idx % ARRAY_SIZE(sb->s_ivs);
	const size_t key_slot = idx % ARRAY_SIZE(sb->s_keys);

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
	return le16_to_cpu(agr->a_nfiles);
}

static void agr_set_nfiles(struct voluta_ag_record *agr, size_t nfiles)
{
	voluta_assert_lt(nfiles, UINT16_MAX / 2);

	agr->a_nfiles = cpu_to_le16((uint16_t)nfiles);
}

static enum voluta_ag_flags agr_flags(const struct voluta_ag_record *agr)
{
	return le16_to_cpu(agr->a_flags);
}

static void agr_set_flags(struct voluta_ag_record *agr, enum voluta_ag_flags f)
{
	agr->a_flags = cpu_to_le16((uint16_t)f);
}

static bool agr_has_flags(const struct voluta_ag_record *agr,
			  const enum voluta_ag_flags mask)
{
	return ((agr_flags(agr) & mask) == mask);
}

static bool agr_is_uspacemap(const struct voluta_ag_record *agr)
{
	return agr_has_flags(agr, VOLUTA_AGF_USPACEMAP);
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
	const size_t nbytes_max = VOLUTA_AG_SIZE;
	const size_t nbytes_cur = agr_used_space(agr);

	voluta_assert_le(nbytes_cur, nbytes_max);

	return ((nbytes_cur + nbytes) <= nbytes_max);
}

static bool agr_may_alloc(const struct voluta_ag_record *agr, size_t nbytes)
{
	bool res;

	if (agr_is_uspacemap(agr)) {
		res = false;
	} else if (!agr_is_formatted(agr)) {
		res = false;
	} else if (!agr_has_nfree(agr, nbytes)) {
		res = false;
	} else if (agr_is_fragmented(agr)) {
		res = is_partial_bk(nbytes);
	} else {
		res = true;
	}
	return res;
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

static size_t usm_nags(const struct voluta_uspace_map *usm)
{
	return le16_to_cpu(usm->u_nags);
}

static void usm_set_nags(struct voluta_uspace_map *usm, size_t nags)
{
	voluta_assert_le(nags, ARRAY_SIZE(usm->u_agr));

	usm->u_nags = cpu_to_le16((uint16_t)nags);
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
	return ag_index_by_usmap(usm_index(usm), 1);
}

static size_t usm_ag_index_end(const struct voluta_uspace_map *usm)
{
	return ag_index_by_usmap(usm_index(usm), 0) + ARRAY_SIZE(usm->u_agr);
}

static void usm_init(struct voluta_uspace_map *usm,
		     size_t usp_index, size_t nag_avail)
{
	usm_set_index(usm, usp_index);
	usm_set_nags(usm, nag_avail);
	usm_set_nused(usm, 0);
	for (size_t i = 0; i < ARRAY_SIZE(usm->u_agr); ++i) {
		agr_init(&usm->u_agr[i]);
	}
}

static void usm_setup_iv_keys(struct voluta_uspace_map *usm)
{
	for (size_t i = 0; i < ARRAY_SIZE(usm->u_ivs); ++i) {
		iv_fill_random(&usm->u_ivs[i]);
	}
	for (size_t j = 0; j < ARRAY_SIZE(usm->u_keys); ++j) {
		key_fill_random(&usm->u_keys[j]);
	}
}

static struct voluta_ag_record *
usm_record_at(const struct voluta_uspace_map *usm, size_t slot)
{
	const struct voluta_ag_record *usr = &usm->u_agr[slot];

	voluta_assert_lt(slot, ARRAY_SIZE(usm->u_agr));
	return unconst(usr);
}

static struct voluta_ag_record *
usm_record_of(const struct voluta_uspace_map *usm, size_t ag_index)
{
	const size_t slot = (ag_index - 1) % ARRAY_SIZE(usm->u_agr);

	voluta_assert_gt(ag_index, 0);

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

static void usm_update_stats_of(struct voluta_uspace_map *usm,
				struct voluta_ag_record *agr,
				const struct voluta_space_stat *sp_st)
{
	const ssize_t diff = calc_used_bytes(sp_st);

	agr_update_stats(agr, sp_st);
	usm_set_nused(usm, safe_sum(usm_nused(usm), diff));

	voluta_assert_le(usm->u_nused, VOLUTA_USP_SIZE);
	voluta_assert_le(agr_used_space(agr), VOLUTA_AG_SIZE);
}

static void usm_update_stats(struct voluta_uspace_map *usm, size_t ag_index,
			     const struct voluta_space_stat *sp_st)
{
	struct voluta_ag_record *agr = usm_record_of(usm, ag_index);

	usm_update_stats_of(usm, agr, sp_st);
}

static void usm_iv_key_of(const struct voluta_uspace_map *usm,
			  loff_t lba, struct voluta_iv_key *out_iv_key)
{
	const size_t idx = (size_t)lba;
	const size_t islot = idx % ARRAY_SIZE(usm->u_ivs);
	const size_t kslot = idx % ARRAY_SIZE(usm->u_keys);

	iv_clone(&usm->u_ivs[islot], &out_iv_key->iv);
	key_clone(&usm->u_keys[kslot], &out_iv_key->key);
}

static void usm_space_stat_of(const struct voluta_uspace_map *usm,
			      size_t ag_index, struct voluta_space_stat *sp_st)
{
	const struct voluta_ag_record *agr;

	agr = usm_record_of(usm, ag_index);
	agr_stat(agr, sp_st);
}

static void usm_space_stat(const struct voluta_uspace_map *usm,
			   struct voluta_space_stat *sp_st_total)
{
	struct voluta_space_stat ag_sp_st;
	const struct voluta_ag_record *agr;
	const size_t nags = usm_nags(usm);

	for (size_t i = 0; i < nags; ++i) {
		agr = usm_record_at(usm, i);
		agr_stat(agr, &ag_sp_st);
		sum_space_stat(sp_st_total, &ag_sp_st, sp_st_total);
	}
}

static void usm_set_formatted(struct voluta_uspace_map *usm, size_t ag_index)
{
	struct voluta_ag_record *agr = usm_record_of(usm, ag_index);

	agr_set_formatted(agr);
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
	const size_t nags = usm_nags(usm);
	const struct voluta_ag_record *agr;

	agr = usm_record_of(usm, ag_index_from);
	beg = usm_record_slot(usm, agr);
	for (size_t i = beg; i < nags; ++i) {
		agr = &usm->u_agr[i];
		if (!agr_is_formatted(agr)) {
			break;
		}
		if (agr_may_alloc(agr, nbytes)) {
			return usm_resolve_ag_index(usm, agr);
		}
	}
	return VOLUTA_AG_INDEX_NULL;
}

static void usm_setup_self(struct voluta_uspace_map *usm)
{
	const size_t nused_self = sizeof(*usm);
	struct voluta_ag_record *agr_self = &usm->u_agr[0];

	usm_set_nused(usm, nused_self);
	agr_set_used_meta(agr_self, nused_self);
	agr_set_flags(agr_self, VOLUTA_AGF_USPACEMAP | VOLUTA_AGF_FORMATTED);
}

static void usm_update_formatted_ag(struct voluta_uspace_map *usm)
{
	struct voluta_ag_record *agr_self = &usm->u_agr[0];
	struct voluta_space_stat sp_st = { .ndata = 0 };

	sp_st.nmeta = (ssize_t)sizeof(struct voluta_agroup_map);
	usm_update_stats_of(usm, agr_self, &sp_st);
}

static size_t
usm_used_space_of(const struct voluta_uspace_map *usm, size_t ag_index)
{
	const struct voluta_ag_record *agr;

	agr = usm_record_of(usm, ag_index);
	return agr_used_space(agr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static uint8_t mask_of(size_t kbn, size_t nkb)
{
	const unsigned int mask = ((1U << nkb) - 1) << kbn;

	return (uint8_t)mask;
}

static enum voluta_vtype bkr_vtype(const struct voluta_bk_record *bkr)
{
	return (enum voluta_vtype)(bkr->b_vtype);
}

static void bkr_set_vtype(struct voluta_bk_record *bkr,
			  enum voluta_vtype vtype)
{
	bkr->b_vtype = (uint8_t)vtype;
}

static bool bkr_has_vtype_or_none(const struct voluta_bk_record *bkr,
				  enum voluta_vtype vtype)
{
	const enum voluta_vtype vt = bkr_vtype(bkr);

	return vtype_isnone(vt) || vtype_isequal(vt, vtype);
}

static enum voluta_vtype
bkr_vtype_at(const struct voluta_bk_record *bkr, size_t kbn) {
	const uint8_t mask = mask_of(kbn, 1);

	return (bkr->b_usemask && mask) ?
	bkr_vtype(bkr) : VOLUTA_VTYPE_NONE;
}

static void bkr_set_refcnt(struct voluta_bk_record *bkr, uint32_t refcnt)
{
	bkr->b_refcnt = cpu_to_le32(refcnt);
}

static void bkr_init(struct voluta_bk_record *bkr)
{
	bkr_set_vtype(bkr, VOLUTA_VTYPE_NONE);
	bkr_set_refcnt(bkr, 0);
	bkr->b_usemask = 0;
	bkr->b_unwritten = 0;
	bkr->b_reserved = 0;
}

static void bkr_init_arr(struct voluta_bk_record *arr, size_t cnt)
{
	for (size_t i = 0; i < cnt; ++i) {
		bkr_init(&arr[i]);
	}
}

static void bkr_set(struct voluta_bk_record *bkr,
		    enum voluta_vtype vtype, size_t kbn, size_t nkb)
{
	const uint8_t mask = mask_of(kbn, nkb);

	voluta_assert_eq(bkr->b_usemask & mask, 0);

	bkr->b_usemask |= mask;
	bkr_set_vtype(bkr, vtype);
}

static void bkr_unset(struct voluta_bk_record *bkr, size_t kbn, size_t nkb)
{
	const uint8_t mask = mask_of(kbn, nkb);

	voluta_assert_eq(bkr->b_usemask & mask, mask);

	bkr->b_usemask &= (uint8_t)(~mask);
	if (!bkr->b_usemask) {
		bkr_set_vtype(bkr, VOLUTA_VTYPE_NONE);
		bkr->b_unwritten = 0;
	}
}

static size_t bkr_usecnt(const struct voluta_bk_record *bkr)
{
	return voluta_popcount(bkr->b_usemask);
}

static size_t bkr_freecnt(const struct voluta_bk_record *bkr)
{
	return VOLUTA_NKB_IN_BK - bkr_usecnt(bkr);
}

static bool bkr_isfull(const struct voluta_bk_record *bkr)
{
	return (bkr->b_usemask == 0xFF);
}

static bool bkr_may_alloc(const struct voluta_bk_record *bkr,
			  enum voluta_vtype vtype, size_t nkb)
{
	bool ret = false;

	if (!bkr_isfull(bkr) && (bkr_freecnt(bkr) >= nkb)) {
		ret = bkr_has_vtype_or_none(bkr, vtype);
	}
	return ret;
}

static int bkr_find_free(const struct voluta_bk_record *bkr,
			 size_t nkb, size_t *out_kbn)
{
	uint8_t mask;
	const size_t nbks_in_bo = VOLUTA_NKB_IN_BK;

	for (size_t kbn = 0; (kbn + nkb) <= nbks_in_bo; kbn += nkb) {
		mask = mask_of(kbn, nkb);
		if ((bkr->b_usemask & mask) == 0) {
			*out_kbn = kbn;
			return 0;
		}
	}
	return -ENOSPC;
}

static void bkr_renew(struct voluta_bk_record *bkr)
{
	bkr_init(bkr);
}

static uint32_t bkr_crc(const struct voluta_bk_record *bkr)
{
	return le32_to_cpu(bkr->b_crc);
}

static void bkr_set_crc(struct voluta_bk_record *bkr, uint32_t crc)
{
	bkr->b_crc = cpu_to_le32(crc);
}

static void bkr_setn_kbn(struct voluta_bk_record *bkr,
			 enum voluta_vtype vtype, size_t kbn, size_t nkb)
{
	bkr_set(bkr, vtype, kbn_within_bo(kbn), nkb);
}

static void bkr_clearn_kbn(struct voluta_bk_record *bkr,
			   size_t kbn, size_t nkb)
{
	bkr_unset(bkr, kbn_within_bo(kbn), nkb);
}

static bool bkr_is_unwritten(const struct voluta_bk_record *bkr)
{
	return bkr->b_unwritten;
}

static void bkr_clear_unwritten(struct voluta_bk_record *bkr)
{
	bkr->b_unwritten = 0;
}

static void bkr_mark_unwritten(struct voluta_bk_record *bkr)
{
	bkr->b_unwritten = 1;
}

static bool bkr_isunused(const struct voluta_bk_record *bkr)
{
	return (bkr_usecnt(bkr) == 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t agm_index(const struct voluta_agroup_map *agm)
{
	return le64_to_cpu(agm->ag_index);
}

static void agm_set_index(struct voluta_agroup_map *agm, size_t ag_index)
{
	voluta_assert(!ag_index_is_umeta(ag_index));

	agm->ag_index = cpu_to_le64(ag_index);
}

static void agm_init(struct voluta_agroup_map *agm, size_t ag_index)
{
	agm_set_index(agm, ag_index);
	agm->ag_ciper_type = VOLUTA_CIPHER_AES256;
	agm->ag_ciper_mode = VOLUTA_CIPHER_MODE_CBC;
	bkr_init_arr(agm->ag_bkr, ARRAY_SIZE(agm->ag_bkr));
}

static struct voluta_bk_record *
agm_bkr_at(const struct voluta_agroup_map *agm, size_t slot)
{
	const struct voluta_bk_record *bkr = &(agm->ag_bkr[slot]);

	voluta_assert_lt(slot, ARRAY_SIZE(agm->ag_bkr));
	return unconst(bkr);
}

static size_t agm_lba_slot(const struct voluta_agroup_map *agm, loff_t lba)
{
	return (size_t)lba % ARRAY_SIZE(agm->ag_bkr);
}

static struct voluta_bk_record *
agm_bkr_by_lba(const struct voluta_agroup_map *agm, loff_t lba)
{
	return agm_bkr_at(agm, agm_lba_slot(agm, lba));
}

static struct voluta_bk_record *
agm_bkr_by_vaddr(const struct voluta_agroup_map *agm,
		 const struct voluta_vaddr *vaddr)
{
	return agm_bkr_by_lba(agm, vaddr->lba);
}

static int agm_vtype_at(const struct voluta_agroup_map *agm,
			const struct voluta_vaddr *vaddr)
{
	const struct voluta_bk_record *bkr = agm_bkr_by_lba(agm, vaddr->lba);

	return bkr_vtype_at(bkr, kbn_of(vaddr));
}

static const struct voluta_key *
agm_key_by_off(const struct voluta_agroup_map *agm, loff_t off)
{
	const size_t slot = off_to_kbn(off) % ARRAY_SIZE(agm->ag_keys);

	return &agm->ag_keys[slot];
}

static const struct voluta_iv *
agm_iv_by_off(const struct voluta_agroup_map *agm, loff_t off)
{
	const size_t slot = off_to_kbn(off) % ARRAY_SIZE(agm->ag_ivs);

	return &agm->ag_ivs[slot];
}

static void agm_iv_key_by_off(const struct voluta_agroup_map *agm,
			      loff_t off, struct voluta_iv_key *out_iv_key)
{
	const struct voluta_iv *iv = agm_iv_by_off(agm, off);
	const struct voluta_key *key = agm_key_by_off(agm, off);

	iv_clone(iv, &out_iv_key->iv);
	key_clone(key, &out_iv_key->key);
}

static void agm_setup_ivs(struct voluta_agroup_map *agm)
{
	for (size_t i = 0; i < ARRAY_SIZE(agm->ag_ivs); ++i) {
		iv_fill_random(&agm->ag_ivs[i]);
	}
}

static void agm_setup_keys(struct voluta_agroup_map *agm)
{
	for (size_t i = 0; i < ARRAY_SIZE(agm->ag_keys); ++i) {
		key_fill_random(&agm->ag_keys[i]);
	}
}

static int agm_find_nfree_at(const struct voluta_agroup_map *agm,
			     enum voluta_vtype vtype, size_t bn,
			     size_t *out_kbn)
{
	int err = -ENOSPC;
	const size_t nkb = vtype_nkbs(vtype);
	const struct voluta_bk_record *bkr = agm_bkr_at(agm, bn);

	if (bkr_may_alloc(bkr, vtype, nkb)) {
		err = bkr_find_free(bkr, nkb, out_kbn);
	}
	return err;
}

static int agm_find_free(const struct voluta_agroup_map *agm,
			 enum voluta_vtype vtype, size_t start_bn,
			 size_t *out_bn, size_t *out_kbn)
{
	int err = -ENOSPC;
	size_t bn = start_bn;
	size_t kbn = 0;
	const size_t nbkrs = ARRAY_SIZE(agm->ag_bkr);

	for (size_t i = 0; i < nbkrs; ++i) {
		if (bn >= nbkrs) {
			bn = 0;
		}
		err = agm_find_nfree_at(agm, vtype, bn, &kbn);
		if (!err) {
			*out_bn = bn;
			*out_kbn = kbn;
			break;
		}
		bn++;
	}
	return err;
}

static int agm_find_free_space(const struct voluta_agroup_map *agm,
			       enum voluta_vtype vtype, size_t start_bn,
			       struct voluta_vaddr *out_vaddr)
{
	int err;
	size_t bn;
	size_t kbn;
	size_t ag_index;

	err = agm_find_free(agm, vtype, start_bn, &bn, &kbn);
	if (err) {
		return err;
	}
	ag_index = agm_index(agm);
	vaddr_setup_by_ag(out_vaddr, vtype, ag_index, bn, kbn);
	return 0;
}

static void agm_mark_unwritten_at(struct voluta_agroup_map *agm,
				  const struct voluta_vaddr *vaddr)
{
	struct voluta_bk_record *bkr = agm_bkr_by_vaddr(agm, vaddr);

	bkr_mark_unwritten(bkr);
}

static int agm_is_unwritten_at(const struct voluta_agroup_map *agm,
			       const struct voluta_vaddr *vaddr)
{
	const struct voluta_bk_record *bkr = agm_bkr_by_vaddr(agm, vaddr);

	return bkr_is_unwritten(bkr);
}

static void agm_clear_unwritten_at(struct voluta_agroup_map *agm,
				   const struct voluta_vaddr *vaddr)
{
	struct voluta_bk_record *bkr = agm_bkr_by_vaddr(agm, vaddr);

	bkr_clear_unwritten(bkr);
}

static void agm_mark_active_space(struct voluta_agroup_map *agm,
				  const struct voluta_vaddr *vaddr)
{
	struct voluta_bk_record *bkr = agm_bkr_by_vaddr(agm, vaddr);

	bkr_setn_kbn(bkr, vaddr->vtype, kbn_of(vaddr), nkbs_of(vaddr));

	if (vaddr_isdata(vaddr)) {
		agm_mark_unwritten_at(agm, vaddr);
	}
}

static void agm_clear_active_space(struct voluta_agroup_map *agm,
				   const struct voluta_vaddr *vaddr)
{
	struct voluta_bk_record *bkr = agm_bkr_by_vaddr(agm, vaddr);

	bkr_clearn_kbn(bkr, kbn_of(vaddr), nkbs_of(vaddr));
}

static void agm_renew_if_unused(struct voluta_agroup_map *agm,
				const struct voluta_vaddr *vaddr)
{
	struct voluta_bk_record *bkr = agm_bkr_by_vaddr(agm, vaddr);

	if (bkr_isunused(bkr)) {
		bkr_renew(bkr);
	}
}

static uint32_t agm_crc(const struct voluta_agroup_map *agm,
			const struct voluta_vaddr *vaddr)
{
	const struct voluta_bk_record *bkr = agm_bkr_by_vaddr(agm, vaddr);

	voluta_assert(vaddr_isdata(vaddr));

	return bkr_crc(bkr);
}

static void agm_set_crc(struct voluta_agroup_map *agm,
			const struct voluta_vaddr *vaddr, uint32_t crc)
{
	struct voluta_bk_record *bkr = agm_bkr_by_vaddr(agm, vaddr);

	voluta_assert(vaddr_isdata(vaddr));

	bkr_set_crc(bkr, crc);
}

static void agm_calc_space_stat(const struct voluta_agroup_map *agm,
				struct voluta_space_stat *sp_st)
{
	ssize_t usecnt;
	enum voluta_vtype vtype;
	const struct voluta_bk_record *bkr;
	const size_t nslots = ARRAY_SIZE(agm->ag_bkr);
	const ssize_t kb_size = (ssize_t)(VOLUTA_KB_SIZE);

	voluta_memzero(sp_st, sizeof(*sp_st));
	for (size_t slot = 0; slot < nslots; ++slot) {
		bkr = agm_bkr_at(agm, slot);

		vtype = bkr_vtype(bkr);
		if (vtype_isnone(vtype)) {
			continue;
		}
		usecnt = (ssize_t)bkr_usecnt(bkr);
		if (vtype_isdata(vtype)) {
			sp_st->ndata += (usecnt * kb_size);
		} else {
			sp_st->nmeta += (usecnt * kb_size);
			if (vtype_isinode(vtype)) {
				sp_st->nfiles += usecnt;
			}
		}
	}
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

static bool v_issuper(const struct voluta_vnode_info *vi)
{
	return vtype_issuper(vtype_of(vi));
}

static bool v_isusmap(const struct voluta_vnode_info *vi)
{
	return vtype_isusmap(vtype_of(vi));
}

static bool v_isagmap(const struct voluta_vnode_info *vi)
{
	return vtype_isagmap(vtype_of(vi));
}

static size_t v_ag_index(const struct voluta_vnode_info *vi)
{
	voluta_assert(v_isagmap(vi));

	return agm_index(vi->u.agm);
}

static void iv_key_of_vnode(const struct voluta_vnode_info *agm_vi,
			    loff_t off, struct voluta_iv_key *out_iv_key)
{
	agm_iv_key_by_off(agm_vi->u.agm, off, out_iv_key);
}

static void iv_key_of_agmap(const struct voluta_vnode_info *usm_vi,
			    loff_t lba, struct voluta_iv_key *out_iv_key)
{
	usm_iv_key_of(usm_vi->u.usm, lba, out_iv_key);
}

static void iv_key_of_usmap(const struct voluta_vnode_info *sb_vi,
			    loff_t lba, struct voluta_iv_key *out_iv_key)
{
	sb_iv_key_of(sb_vi->u.sb, lba, out_iv_key);
}

static void iv_key_of_super(const struct voluta_sb_info *sbi,
			    struct voluta_iv_key *out_iv_key)
{
	iv_key_clone(&sbi->s_iv_key, out_iv_key);
}

void voluta_iv_key_of(const struct voluta_vnode_info *vi,
		      struct voluta_iv_key *out_iv_key)
{
	const struct voluta_vnode_info *pvi = vi->v_pvi;
	const struct voluta_sb_info *sbi = v_sbi_of(vi);
	const struct voluta_vaddr *vaddr = v_vaddr_of(vi);

	if (v_issuper(vi)) {
		voluta_assert_eq(vaddr->lba, VOLUTA_LBA_SUPER);
		iv_key_of_super(sbi, out_iv_key);
	} else if (v_isusmap(vi)) {
		iv_key_of_usmap(sbi->s_sb_vi, vaddr->lba, out_iv_key);
	} else if (v_isagmap(vi)) {
		voluta_assert(v_isusmap(pvi));
		iv_key_of_agmap(pvi, vaddr->lba, out_iv_key);
	} else {
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
	spi->sp_usp_count = div_round_up(ag_count - 1, nag_in_usp);
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

static void spi_update_meta(struct voluta_space_info *spi, ssize_t nmeta)
{
	const struct voluta_space_stat sp_st = {
		.nmeta = nmeta
	};
	spi_update_stats(spi, &sp_st);
}

static void spi_used_meta_upto(struct voluta_space_info *spi, loff_t off)
{
	spi->sp_used_meta = off;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void setup_vnode_view(struct voluta_vnode_info *vi)
{
	struct voluta_view *view = vi->view;
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

static void setup_agmap(struct voluta_vnode_info *agm_vi, size_t ag_index)
{
	struct voluta_agroup_map *agm = agm_vi->u.agm;

	setup_vnode_view(agm_vi);
	agm_init(agm, ag_index);
	agm_setup_ivs(agm);
	agm_setup_keys(agm);

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

static int allocate_space_from(struct voluta_vnode_info *agm_vi,
			       enum voluta_vtype vtype, size_t start_bn,
			       struct voluta_vaddr *out_vaddr)
{
	int err;

	err = agm_find_free_space(agm_vi->u.agm, vtype, start_bn, out_vaddr);
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
	const size_t nbytes = vtype_length(vtype);

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
	const size_t usp_index = vaddr_usp_index(vaddr);
	const size_t ag_index = vaddr_ag_index(vaddr);

	if (!usp_index || !ag_index) {
		return 0; /* not counting meta-elements of USP0/AG0 */
	}
	err = stage_usmap(sbi, usp_index, &usm_vi);
	if (err) {
		return err;
	}
	calc_stat_change(vaddr->vtype, vaddr->len, take, &sp_st);

	usm_update_stats(usm_vi->u.usm, ag_index, &sp_st);
	v_dirtify(usm_vi);

	spi_update_stats(&sbi->s_spi, &sp_st);

	return 0;
}

static fsblkcnt_t bytes_to_fsblkcnt(ssize_t nbytes)
{
	return (fsblkcnt_t)nbytes / VOLUTA_BK_SIZE;
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
	out_stvfs->f_bsize = VOLUTA_SEG_SIZE;
	out_stvfs->f_frsize = VOLUTA_BK_SIZE;
	out_stvfs->f_blocks = bytes_to_fsblkcnt(nbytes_max);
	out_stvfs->f_bfree = bytes_to_fsblkcnt(nbytes_max - nbytes_use);
	out_stvfs->f_bavail = out_stvfs->f_bfree;
	out_stvfs->f_files = (fsfilcnt_t)nfiles_max;
	out_stvfs->f_ffree = (fsfilcnt_t)(nfiles_max - spi->sp_nfiles);
	out_stvfs->f_favail = out_stvfs->f_ffree;
	out_stvfs->f_namemax = VOLUTA_NAME_MAX;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool has_unwritten_at(const struct voluta_vnode_info *agm_vi,
			     const struct voluta_vaddr *vaddr)
{
	voluta_assert(vaddr_isdata(vaddr));

	return agm_is_unwritten_at(agm_vi->u.agm, vaddr);
}

static int find_cached_seg(struct voluta_sb_info *sbi, loff_t lba,
			   struct voluta_seg_info **out_sgi)
{
	*out_sgi = voluta_cache_lookup_seg(sbi->s_cache, lba);
	return (*out_sgi != NULL) ? 0 : -ENOENT;
}

static int find_cached_seg_of(struct voluta_super_ctx *s_ctx)
{
	return find_cached_seg(s_ctx->sbi, s_ctx->vaddr->lba, &s_ctx->sgi);
}

static int commit_dirty_now(const struct voluta_super_ctx *s_ctx)
{
	return voluta_commit_dirty(s_ctx->sbi, VOLUTA_F_NOW);
}

static int spawn_seg(struct voluta_super_ctx *s_ctx)
{
	int err;
	const loff_t lba = s_ctx->vaddr->lba;

	s_ctx->sgi = voluta_cache_spawn_seg(s_ctx->sbi->s_cache, lba);
	if (s_ctx->sgi != NULL) {
		return 0;
	}
	err = commit_dirty_now(s_ctx);
	if (err) {
		return err;
	}
	s_ctx->sgi = voluta_cache_spawn_seg(s_ctx->sbi->s_cache, lba);
	if (s_ctx->sgi == NULL) {
		return -ENOMEM;
	}
	return 0;
}

static int load_seg(struct voluta_super_ctx *s_ctx)
{
	loff_t off;
	struct voluta_seg_info *sgi = s_ctx->sgi;
	struct voluta_segment *seg = sgi->seg;
	const struct voluta_pstore *pstore = s_ctx->sbi->s_pstore;

	off = lba_to_off(sgi->seg_lba);
	return voluta_pstore_read(pstore, off, sizeof(*seg), seg);
}

static void forget_seg(struct voluta_super_ctx *s_ctx)
{
	voluta_cache_forget_seg(s_ctx->sbi->s_cache, s_ctx->sgi);
	s_ctx->sgi = NULL;
}

static int stage_seg(struct voluta_super_ctx *s_ctx)
{
	int err;

	err = find_cached_seg_of(s_ctx);
	if (!err) {
		return 0; /* Cache hit */
	}
	err = spawn_seg(s_ctx);
	if (err) {
		return err;
	}
	err = load_seg(s_ctx);
	if (err) {
		forget_seg(s_ctx);
		return err;
	}
	return 0;
}

static struct voluta_view *view_of(const union voluta_kilo_block *kb)
{
	const union voluta_view_u *u;
	const struct voluta_view *v;

	u = container_of2(kb, union voluta_view_u, kb);
	v = container_of2(u, struct voluta_view, u);
	return unconst(v);
}

static struct voluta_view *
view_at(const struct voluta_seg_info *sgi, loff_t off)
{
	long kbn;
	const long kb_size = VOLUTA_KB_SIZE;
	const long nkb_in_seg = VOLUTA_NKB_IN_SEG;
	struct voluta_segment *seg = sgi->seg;

	kbn = ((off / kb_size) % nkb_in_seg);
	return view_of(&seg->u.kb[kbn]);
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

static int verify_vnode_view(struct voluta_vnode_info *vi)
{
	int err;

	if (v_isdata(vi)) {
		err = verify_data(vi);
	} else {
		err = voluta_verify_meta(vi);
	}

	if (!err) {
		vi->v_verify++;
	}
	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t start_bn_of(struct voluta_vnode_info *agm_vi)
{
	size_t start_bn;
	size_t used_space;
	const size_t ag_index = v_ag_index(agm_vi);
	const struct voluta_vnode_info *usm_vi = agm_vi->v_pvi;

	/* Heuristic for mostly append pattern */
	used_space = usm_used_space_of(usm_vi->u.usm, ag_index);
	start_bn = used_space / VOLUTA_BK_SIZE;

	return start_bn;
}

static int allocate_from(struct voluta_sb_info *sbi,
			 size_t ag_index, enum voluta_vtype vtype,
			 struct voluta_vaddr *out_vaddr)
{
	int err;
	size_t start_bn;
	struct voluta_vnode_info *agm_vi;

	err = stage_agmap(sbi, ag_index, &agm_vi);
	if (err) {
		return err;
	}
	start_bn = start_bn_of(agm_vi);
	err = allocate_space_from(agm_vi, vtype, start_bn, out_vaddr);
	if (err) {
		return err;
	}
	return 0;
}

static bool can_allocate_from(const struct voluta_vnode_info *usm_vi,
			      size_t ag_index, size_t nbytes)
{
	return usm_may_alloc_from(usm_vi->u.usm, ag_index, nbytes);
}

static int try_allocate_nbytes(struct voluta_sb_info *sbi,
			       struct voluta_vnode_info *usm_vi,
			       size_t ag_index, enum voluta_vtype vtype,
			       size_t nbytes, struct voluta_vaddr *out_vaddr)
{
	int err = -ENOSPC;
	const bool big_alloc = (nbytes >= VOLUTA_BK_SIZE);

	if (can_allocate_from(usm_vi, ag_index, nbytes)) {
		err = allocate_from(sbi, ag_index, vtype, out_vaddr);

		if (big_alloc && (err == -ENOSPC)) {
			usm_mark_fragmented(usm_vi->u.usm, ag_index);
			v_dirtify(usm_vi);
		}
	}
	return err;
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
	const size_t nbytes = vtype_length(vtype);
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
	const size_t ag_index = vaddr_ag_index(vaddr);
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
	const size_t usp_index = vaddr_usp_index(vaddr);
	const size_t ag_index = vaddr_ag_index(vaddr);

	voluta_assert_gt(usp_index, 0);
	voluta_assert_eq(usp_index, usp_index_of_ag(ag_index));

	err = stage_agmap(sbi, ag_index, &agm_vi);
	if (err) {
		return err;
	}
	deallocate_space_at(agm_vi, vaddr);

	spi->sp_usp_index_lo = min(usp_index, spi->sp_usp_index_lo);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void bind_view(struct voluta_vnode_info *vi, struct voluta_view *view)
{
	vi->view = view;

	switch (vi->vaddr.vtype) {
	case VOLUTA_VTYPE_SUPER:
		vi->u.sb = &view->u.sb;
		break;
	case VOLUTA_VTYPE_USMAP:
		vi->u.usm = &view->u.usm;
		break;
	case VOLUTA_VTYPE_AGMAP:
		vi->u.agm = &view->u.agm;
		break;
	case VOLUTA_VTYPE_ITNODE:
		vi->u.itn = &view->u.itn;
		break;
	case VOLUTA_VTYPE_INODE:
		vi->u.inode = &view->u.inode;
		break;
	case VOLUTA_VTYPE_XANODE:
		vi->u.xan = &view->u.xan;
		break;
	case VOLUTA_VTYPE_HTNODE:
		vi->u.htn = &view->u.htn;
		break;
	case VOLUTA_VTYPE_RTNODE:
		vi->u.rtn = &view->u.rtn;
		break;
	case VOLUTA_VTYPE_SYMVAL:
		vi->u.lnv = &view->u.lnv;
		break;
	case VOLUTA_VTYPE_DATA:
		vi->u.db = &view->u.db;
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
	bind_view(vi, view_at(s_ctx->sgi, vaddr->off));
	voluta_attach_to(vi, s_ctx->sgi, s_ctx->pvi, s_ctx->pii);
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
	struct voluta_cache *cache = v_cache_of(vi);
	struct voluta_seg_info *sgi = vi->v_sgi;

	voulta_cache_forget_vi(cache, vi);
	voluta_cache_try_forget_seg(cache, sgi);
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
	struct voluta_cache *cache = i_cache_of(ii);
	struct voluta_seg_info *sgi = i_sgi_of(ii);

	voulta_cache_forget_ii(cache, ii);
	voluta_cache_try_forget_seg(cache, sgi);
}

static int find_cached_seg_or_spawn(struct voluta_super_ctx *s_ctx)
{
	int err;

	err = find_cached_seg_of(s_ctx);
	if (err) {
		err = spawn_seg(s_ctx);
	}
	return err;
}

static int spawn_vmeta(struct voluta_super_ctx *s_ctx)
{
	int err;

	err = find_cached_seg_or_spawn(s_ctx);
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

	if (voluta_is_visible(vi)) {
		return 0;
	}
	err = voluta_decrypt_inplace(vi);
	if (err) {
		return err;
	}
	err = verify_vnode_view(vi);
	if (err) {
		return err;
	}
	voluta_mark_visible(vi);
	return 0;
}

static int stage_vmeta(struct voluta_super_ctx *s_ctx)
{
	int err;

	err = find_cached_vi(s_ctx);
	if (!err) {
		return 0; /* Cache hit */
	}
	err = stage_seg(s_ctx);
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

	spi_used_meta_upto(&sbi->s_spi, VOLUTA_AG_SIZE);

	return 0;
}

static int check_super(const struct voluta_sb_info *sbi,
		       const struct voluta_vnode_info *sb_vi)
{
	const size_t ag_count = sb_ag_count(sb_vi->u.sb);
	const size_t sp_ag_count = sbi->s_spi.sp_ag_count;

	if (ag_count != sp_ag_count) {
		log_err("ag-count mismatch: sb=%lu expected=%lu",
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

	spi_used_meta_upto(&sbi->s_spi, VOLUTA_AG_SIZE);

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

static void update_spi_on_usp(struct voluta_sb_info *sbi)
{
	spi_update_meta(&sbi->s_spi, VOLUTA_AG_SIZE);
}

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

static void setup_usmap(struct voluta_vnode_info *usm_vi,
			size_t usp_index, size_t nag_avail)
{
	struct voluta_uspace_map *usm = usm_vi->u.usm;

	voluta_assert_gt(nag_avail, 0);
	voluta_assert_le(nag_avail, ARRAY_SIZE(usm->u_agr));

	setup_vnode_view(usm_vi);
	usm_init(usm, usp_index, nag_avail);
	usm_setup_iv_keys(usm);
	usm_setup_self(usm);

	v_dirtify(usm_vi);
}

static int format_usmap(struct voluta_sb_info *sbi,
			size_t usp_index, size_t nag_avail)
{
	int err;
	struct voluta_vnode_info *usm_vi;

	err = spawn_usmap(sbi, usp_index, &usm_vi);
	if (err) {
		return err;
	}
	setup_usmap(usm_vi, usp_index, nag_avail);

	update_spi_on_usp(sbi);

	return 0;
}

int voluta_format_usmaps(struct voluta_sb_info *sbi)
{
	int err;
	size_t ag_used = 0;
	size_t ag_here = 1;
	const size_t ag_count = sbi->s_spi.sp_ag_count;
	const size_t usp_count = sbi->s_spi.sp_usp_count;

	voluta_assert_gt(ag_count, 1);
	voluta_assert_gt(usp_count, 0);
	voluta_assert_le(usp_count, VOLUTA_NUSP_MAX);

	for (size_t usp_index = 1; usp_index <= usp_count; ++usp_index) {
		ag_used += ag_here;
		ag_here = min(ag_count - ag_used, VOLUTA_NAG_IN_USP);

		err = format_usmap(sbi, usp_index, ag_here);
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

static int format_agmap(struct voluta_sb_info *sbi, size_t ag_index,
			struct voluta_vnode_info *usm_vi)
{
	int err;
	struct voluta_vnode_info *agm_vi;

	err = spawn_agmap(sbi, ag_index, usm_vi, &agm_vi);
	if (err) {
		return err;
	}
	setup_agmap(agm_vi, ag_index);

	usm_set_formatted(usm_vi->u.usm, ag_index);
	usm_update_formatted_ag(usm_vi->u.usm);
	v_dirtify(usm_vi);

	update_spi_by_usm(sbi, usm_vi);

	return 0;
}

int voluta_format_agmaps(struct voluta_sb_info *sbi)
{
	int err;
	size_t usp_index;
	struct voluta_vnode_info *usm_vi = NULL;
	const size_t ag_count = sbi->s_spi.sp_ag_count;

	for (size_t ag_index = 0; ag_index < ag_count; ++ag_index) {
		if (ag_index_is_umeta(ag_index)) {
			continue;
		}
		usp_index = usp_index_of_ag(ag_index);
		err = stage_usmap(sbi, usp_index, &usm_vi);
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

	update_spi_on_usp(sbi);

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
	size_t usp_index;
	struct voluta_vnode_info *usm_vi = NULL;
	const size_t ag_count = sbi->s_spi.sp_ag_count;

	for (size_t ag_index = 0; ag_index < ag_count; ++ag_index) {
		if (ag_index_is_umeta(ag_index)) {
			continue;
		}
		usp_index = usp_index_of_ag(ag_index);
		err = stage_usmap(sbi, usp_index, &usm_vi);
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

static int verify_bkr(const struct voluta_bk_record *bkr)
{
	int err;

	err = verify_vtype(bkr_vtype(bkr));
	if (err) {
		return err;
	}
	if (bkr->b_unwritten > 1) {
		return -EFSCORRUPTED;
	}
	return 0;
}

static int verify_agmap(const struct voluta_agroup_map *agm)
{
	int err;
	const struct voluta_bk_record *bkr;

	for (size_t i = 0; i < ARRAY_SIZE(agm->ag_bkr); ++i) {
		bkr = agm_bkr_at(agm, i);
		err = verify_bkr(bkr);
		if (err) {
			return err;
		}
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
	return (cache->c_qalloc->st.memsz_data / (2 * VOLUTA_SEG_SIZE));
}

static int sbi_init_commons(struct voluta_sb_info *sbi,
			    struct voluta_cache *cache,
			    struct voluta_crypto *crypto,
			    struct voluta_pstore *pstore)

{
	voluta_uuid_generate(&sbi->s_fs_uuid);
	spi_init(&sbi->s_spi);
	iv_key_reset(&sbi->s_iv_key);
	sbi->s_owner.uid = getuid();
	sbi->s_owner.gid = getgid();
	sbi->s_owner.pid = getpid();
	sbi->s_owner.umask = 0002;
	sbi->s_namei_iconv = (iconv_t)(-1);
	sbi->s_cache = cache;
	sbi->s_qalloc = cache->c_qalloc;
	sbi->s_crypto = crypto;
	sbi->s_pstore = pstore;
	sbi->s_sb_vi = NULL;
	sbi->s_iopen_limit = calc_iopen_limit(cache);
	sbi->s_iopen = 0;
	sbi->s_opstat.op_time = time(NULL);
	sbi->s_opstat.op_count = 0;
	sbi->s_opstat.op_count_mark = 0;

	return voluta_init_itable(sbi);
}

static void sbi_fini_commons(struct voluta_sb_info *sbi)
{
	voluta_fini_itable(sbi);
	spi_fini(&sbi->s_spi);
	iv_key_reset(&sbi->s_iv_key);
	sbi->s_cache = NULL;
	sbi->s_qalloc = NULL;
	sbi->s_crypto = NULL;
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
		    struct voluta_cache *cache,
		    struct voluta_crypto *crypto,
		    struct voluta_pstore *pstore)
{
	int err;

	err = sbi_init_commons(sbi, cache, crypto, pstore);
	if (err) {
		return err;
	}
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
	struct voluta_crypto *crypto = sbi->s_crypto;
	struct voluta_pstore *pstore = sbi->s_pstore;

	iv_key_clone(&sbi->s_iv_key, &iv_key);
	voluta_sbi_fini(sbi);
	err = voluta_sbi_init(sbi, cache, crypto, pstore);
	if (err) {
		return err;
	}
	iv_key_clone(&iv_key, &sbi->s_iv_key);
	return 0;
}

void voluta_sbi_setowner(struct voluta_sb_info *sbi,
			 const struct voluta_ucred *cred)
{
	sbi->s_owner.uid = cred->uid;
	sbi->s_owner.gid = cred->gid;
	sbi->s_owner.pid = cred->pid;
	sbi->s_owner.umask = cred->umask;
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

static int seal_dirty_vnodes(const struct voluta_dirtyset *dset)
{
	const struct voluta_vnode_info *vi;

	vi = voluta_dirtyset_first(dset);
	while (vi != NULL) {
		if (v_isdata(vi)) {
			seal_data(vi);
		}
		vi = voluta_dirtyset_next(dset, vi);
	}

	vi = voluta_dirtyset_first(dset);
	while (vi != NULL) {
		if (!v_isdata(vi)) {
			seal_meta(vi);
		}
		vi = voluta_dirtyset_next(dset, vi);
	}
	return 0;
}

static int collect_flush_dirty(struct voluta_sb_info *sbi,
			       struct voluta_inode_info *ii)
{
	int err;
	struct voluta_dirtyset dset;

	voluta_dirtyset_init(&dset, sbi, ii);
	voluta_dirtyset_inhabit(&dset);
	err = seal_dirty_vnodes(&dset);
	if (err) {
		return err;
	}
	err = voluta_flush_dirty(sbi, &dset);
	if (err) {
		return err;
	}
	voluta_dirtyset_purge(&dset);
	voluta_dirtyset_reset(&dset);
	return 0;
}

static int stage_agmap_of(struct voluta_super_ctx *s_ctx)
{
	const size_t ag_index = vaddr_ag_index(s_ctx->vaddr);

	return stage_agmap(s_ctx->sbi, ag_index, &s_ctx->pvi);
}

static int stage_parents(struct voluta_super_ctx *s_ctx)
{
	int err;

	err = stage_agmap_of(s_ctx);
	if (err) {
		return err;
	}
	err = stage_seg(s_ctx);
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

static bool cache_need_flush(const struct voluta_cache *cache)
{
	const size_t nbk_in_ag = VOLUTA_NBK_IN_AG;
	const size_t ndirty_vi = cache->c_vdq.lq.sz;
	const size_t ncached_vi = cache->c_vlm.count;
	const size_t npages_used = cache->c_qalloc->st.npages_used;
	const size_t npages_tota = cache->c_qalloc->st.npages;

	if (!ndirty_vi) {
		return false;
	}
	if (ndirty_vi > (4 * nbk_in_ag)) {
		return true;
	}
	if (((2 * ndirty_vi) > ncached_vi) && (ncached_vi > 64)) {
		return true;
	}
	if (npages_used > (npages_tota / 2)) {
		return true;
	}
	return false;
}

static bool need_flush(const struct voluta_sb_info *sbi)
{
	return cache_need_flush(sbi->s_cache) ||
	       voluta_pstore_exhuased(sbi->s_pstore);
}

static int commit_last(struct voluta_sb_info *sbi, int flags)
{
	int err;

	if (flags & VOLUTA_F_NOW) {
		err = voluta_collect_flushed(sbi, 0);
		if (err) {
			return err;
		}
	}
	if (flags & VOLUTA_F_SYNC) {
		err = voluta_sync_volume(sbi);
		if (err) {
			return err;
		}
	}
	return 0;
}

int voluta_commit_dirty(struct voluta_sb_info *sbi, int flags)
{
	int err;

	if (!flags && !need_flush(sbi)) {
		return 0;
	}
	err = voluta_collect_flushed(sbi, 0);
	if (err) {
		return err;
	}
	err = collect_flush_dirty(sbi, NULL);
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

	err = collect_flush_dirty(sbi, ii);
	if (err) {
		return err;
	}
	err = commit_last(sbi, flags);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_commit_dirty_now(struct voluta_sb_info *sbi)
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

static bool equal_space_stat(const struct voluta_space_stat *sp_st1,
			     const struct voluta_space_stat *sp_st2)
{
	voluta_assert_eq(sp_st1->ndata, sp_st2->ndata);
	voluta_assert_eq(sp_st1->nmeta, sp_st2->nmeta);
	voluta_assert_eq(sp_st1->nfiles, sp_st2->nfiles);

	return (sp_st1->ndata == sp_st2->ndata) &&
	       (sp_st1->nmeta == sp_st2->nmeta) &&
	       (sp_st1->nfiles == sp_st2->nfiles);
}

static int verify_agm_stat(struct voluta_vnode_info *agm_vi)
{
	struct voluta_space_stat usm_sp_st;
	struct voluta_space_stat agm_sp_st;
	const size_t ag_index = v_ag_index(agm_vi);
	const struct voluta_vnode_info *usm_vi = agm_vi->v_pvi;

	if (agm_vi->v_verify > 1) {
		return 0;
	}

	usm_space_stat_of(usm_vi->u.usm, ag_index, &usm_sp_st);
	agm_calc_space_stat(agm_vi->u.agm, &agm_sp_st);

	if (!equal_space_stat(&usm_sp_st, &agm_sp_st)) {
		return -EFSCORRUPTED;
	}
	agm_vi->v_verify++;
	return 0;
}

static int stage_agmap(struct voluta_sb_info *sbi, size_t ag_index,
		       struct voluta_vnode_info **out_vi)
{
	int err;
	size_t usp_index;
	struct voluta_vnode_info *agm_vi = NULL;
	struct voluta_super_ctx s_ctx = {
		.sbi = sbi,
		.vaddr = &s_ctx.xa.vaddr
	};

	vaddr_of_agmap(s_ctx.vaddr, ag_index);
	usp_index = vaddr_usp_index(s_ctx.vaddr);

	err = stage_usmap(sbi, usp_index, &s_ctx.pvi);
	if (err) {
		return err;
	}
	err = stage_meta(&s_ctx, &agm_vi);
	if (err) {
		return err;
	}
	voluta_assert_eq(v_ag_index(agm_vi), ag_index);

	err = verify_agm_stat(agm_vi);
	if (err) {
		/* TODO: cleanups */
		return err;
	}
	*out_vi = agm_vi;
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
	struct voluta_data_block *db = &vi->view->u.db;
	const struct voluta_vnode_info *agm_vi = vi->v_pvi;

	voluta_assert_not_null(vi->v_pvi);
	voluta_assert_eq(vi->vaddr.vtype, VOLUTA_VTYPE_DATA);
	voluta_assert_eq(vi->v_pvi->vaddr.vtype, VOLUTA_VTYPE_AGMAP);

	if (has_unwritten_at(agm_vi, v_vaddr_of(vi))) {
		voluta_memzero(db, sizeof(*db));
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

static int alloc_vspace(struct voluta_super_ctx *s_ctx,
			enum voluta_vtype vtype)
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

	err = alloc_vspace(&s_ctx, vtype);
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
		     const struct voluta_oper *op,
		     mode_t mode, ino_t parent,
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
	err = alloc_vspace(&s_ctx, VOLUTA_VTYPE_INODE);
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

	voluta_setup_new_inode(s_ctx.ii, op, mode, parent);
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

	err = alloc_vspace(&s_ctx, vtype);
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
	struct voluta_vaddr vaddr;
	const ino_t ino = i_ino_of(ii);

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
	return 0;
}

static void mark_opaque_at(struct voluta_sb_info *sbi,
			   const struct voluta_vaddr *vaddr)
{
	int err;
	struct voluta_seg_info *sgi = NULL;

	err = find_cached_seg(sbi, vaddr->lba, &sgi);
	if (!err) {
		voluta_mark_opaque_at(sgi, vaddr);
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

	err = free_vspace(sbi, v_vaddr_of(vi));
	if (err) {
		return err;
	}
	voluta_mark_opaque(vi);
	forget_cached_vi(vi);
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
