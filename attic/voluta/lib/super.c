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
static int stage_agmap(struct voluta_sb_info *sbi, size_t ag_index,
		       struct voluta_vnode_info **out_vi);
static void bind_vnode_to_bk(struct voluta_vnode_info *vi,
			     struct voluta_bk_info *bki);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static bool vtype_isdata(enum voluta_vtype vtype)
{
	return vtype_isequal(vtype, VOLUTA_VTYPE_DATA);
}

static bool vtype_isinode(enum voluta_vtype vtype)
{
	return vtype_isequal(vtype, VOLUTA_VTYPE_INODE);
}

static bool vtype_isnone(enum voluta_vtype vtype)
{
	return vtype_isequal(vtype, VOLUTA_VTYPE_NONE);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool lba_isnull(loff_t lba)
{
	return (lba == VOLUTA_LBA_NULL);
}

static bool lba_isagmap(loff_t lba)
{
	return ((lba % VOLUTA_NBK_IN_AG) == 0);
}

static size_t nkb_to_size(size_t nkb)
{
	return (nkb * VOLUTA_KB_SIZE);
}

static size_t size_to_nkb(size_t nbytes)
{
	const size_t kb_size = VOLUTA_KB_SIZE;

	return (nbytes + kb_size - 1) / kb_size;
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
	voluta_assert_ge(lba, nbk_in_ag);

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

	voluta_assert_gt(ag_index, 0);
	return (loff_t)(nbk_in_ag * ag_index);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_vaddr_reset(struct voluta_vaddr *vaddr)
{
	vaddr->ag_index = VOLUTA_AG_INDEX_NULL;
	vaddr->off = VOLUTA_OFF_NULL;
	vaddr->lba = VOLUTA_LBA_NULL;
	vaddr->len = 0;
	vaddr->vtype = VOLUTA_VTYPE_NONE;
}

void voluta_vaddr_clone(const struct voluta_vaddr *vaddr,
			struct voluta_vaddr *other)
{
	other->ag_index = vaddr->ag_index;
	other->off = vaddr->off;
	other->lba = vaddr->lba;
	other->len = vaddr->len;
	other->vtype = vaddr->vtype;
}

static void vaddr_assign(struct voluta_vaddr *vaddr, enum voluta_vtype vtype,
			 size_t ag_index, loff_t lba, size_t kbn, size_t len)
{
	vaddr->ag_index = ag_index;
	vaddr->lba = lba;
	vaddr->len = len;
	vaddr->off = lba_kbn_to_off(lba, kbn);
	vaddr->vtype = vtype;
}

static void vaddr_setup(struct voluta_vaddr *vaddr, enum voluta_vtype vtype,
			size_t ag_index, size_t bn, size_t kbn, size_t len)
{
	const loff_t lba = lba_by_ag(lba_of_agm(ag_index), bn);

	vaddr_assign(vaddr, vtype, ag_index, lba, kbn, len);
}

static void vaddr_by_lba(struct voluta_vaddr *vaddr,
			 enum voluta_vtype vtype, loff_t lba)
{
	const size_t len = VOLUTA_BK_SIZE;

	if (!lba_isnull(lba)) {
		vaddr_assign(vaddr, vtype, ag_index_of(lba), lba, 0, len);
	} else {
		vaddr_reset(vaddr);
		vaddr->vtype = vtype;
	}
}

static void vaddr_by_off_len(struct voluta_vaddr *vaddr,
			     enum voluta_vtype vtype, loff_t off, size_t len)
{
	const loff_t lba = off_to_lba_safe(off);

	vaddr_by_lba(vaddr, vtype, lba);
	vaddr->off = off;
	vaddr->len = len;
}

void voluta_vaddr_by_off(struct voluta_vaddr *vaddr,
			 enum voluta_vtype vtype, loff_t off)
{
	const size_t len = voluta_persistent_size(vtype);

	vaddr_by_off_len(vaddr, vtype, off, len);
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

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t nkb_of(const struct voluta_vaddr *vaddr)
{
	return size_to_nkb(vaddr->len);
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

static enum voluta_vtype bo_vtype_at(const struct voluta_boctet *bo, size_t kbn)
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

static void bkref_clearn_kbn(struct voluta_bkref *bkref, size_t kbn, size_t nkb)
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

static int bkref_find_free(const struct voluta_bkref *bkref, bool partial,
			   enum voluta_vtype vtype, size_t nkb, size_t *out_kbn)
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
	voluta_vaddr_by_off(out_vaddr, vtype, off);
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
	size_t bn, kbn, len;

	err = agm_find_free(agm, vtype, nkb, &bn, &kbn);
	if (err) {
		return err;
	}
	len = nkb_to_size(nkb);
	vaddr_setup(out_vaddr, vtype, agm_index(agm), bn, kbn, len);
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

static bool is_agmap(const struct voluta_bk_info *bki)
{
	return lba_isagmap(bki->b_lba);
}

static void iv_key_by_agmap(const struct voluta_bk_info *bki,
			    struct voluta_iv_key *out_iv_key)
{
	const struct voluta_vnode_info *agm_vi = bki->b_agm;

	agm_iv_key_of(agm_vi->u.agm, bki->b_lba, out_iv_key);
}

static void iv_key_of_agmap(const struct voluta_bk_info *bki,
			    struct voluta_iv_key *out_iv_key)
{
	const struct voluta_super_block *sb = bki->b_sbi->sb;

	sb_iv_key_of(sb, ag_index_of(bki->b_lba), out_iv_key);
}

static void iv_key_of(const struct voluta_bk_info *bki,
		      struct voluta_iv_key *out_iv_key)
{
	if (is_agmap(bki)) {
		iv_key_of_agmap(bki, out_iv_key);
	} else {
		iv_key_by_agmap(bki, out_iv_key);
	}
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
	struct voluta_vnode_info *agm_vi = vi->bki->b_agm;

	voluta_assert_not_null(agm_vi);
	voluta_assert_eq(vaddr->vtype, VOLUTA_VTYPE_DATA);

	agm_clear_unwritten_at(agm_vi->u.agm, vaddr);
	v_dirtify(agm_vi);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void agi_setup(struct voluta_ag_info *agi, size_t ag_index)
{
	list_head_init(&agi->lh);
	agi->index = (int)ag_index;
	agi->nfree = VOLUTA_AG_SIZE;
	agi->formatted = false;
	agi->live = false;
	agi->cap_alloc_bo = true;
}

static size_t agi_index(const struct voluta_ag_info *agi)
{
	return (size_t)(agi->index);
}

static struct voluta_ag_info *
agi_by_index(const struct voluta_ag_space *ags, size_t ag_index)
{
	const struct voluta_ag_info *agi = &ags->agi[ag_index];

	voluta_assert_lt(ag_index, ags->ags_count);
	return (struct voluta_ag_info *)agi;
}

static bool agi_cap_alloc_space(const struct voluta_ag_info *agi, int nbytes)
{
	const int bo_size = VOLUTA_BO_SIZE;

	if (!agi->formatted) {
		return false;
	}
	if (agi->nfree < nbytes) {
		return false;
	}
	if ((nbytes == bo_size) && !agi->cap_alloc_bo) {
		return false;
	}
	return true;
}

static void agi_hint_failed_alloc(struct voluta_ag_info *agi, int nbytes)
{
	const int bo_size = VOLUTA_BO_SIZE;

	if (nbytes == bo_size) {
		agi->cap_alloc_bo = false;
	}
}

static void agi_update_free(struct voluta_ag_info *agi, int nbytes)
{
	const int bo_size = VOLUTA_BO_SIZE;

	agi->nfree += (int)nbytes;
	if (agi->nfree >= bo_size) {
		agi->cap_alloc_bo = true;
	}

	voluta_assert_ge(agi->nfree, 0);
	voluta_assert_le(agi->nfree, VOLUTA_AG_SIZE);
}

static bool agi_is_exhausted(const struct voluta_ag_info *agi)
{
	return (agi->nfree == 0);
}

static struct voluta_ag_info *agi_of(struct voluta_list_head *lh)
{
	return container_of(lh, struct voluta_ag_info, lh);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ags_alloc(struct voluta_ag_space *ags,
		      const struct voluta_vaddr *vaddr)
{
	struct voluta_ag_info *agi = agi_by_index(ags, vaddr->ag_index);

	agi_update_free(agi, -(int)(vaddr->len));
}

static void ags_free(struct voluta_ag_space *ags,
		     const struct voluta_vaddr *vaddr)
{
	struct voluta_ag_info *agi = agi_by_index(ags, vaddr->ag_index);

	agi_update_free(agi, (int)vaddr->len);
}

static void ags_set_formatted(struct voluta_ag_space *ags, size_t ag_index)
{
	struct voluta_vaddr vaddr;
	struct voluta_ag_info *agi = agi_by_index(ags, ag_index);

	vaddr_of_agmap(&vaddr, ag_index);
	ags_alloc(ags, &vaddr);
	agi->formatted = true;
}

static void ags_init(struct voluta_ag_space *ags)
{
	voluta_memzero(ags, sizeof(*ags));
	list_head_init(&ags->spq);
	ags->ags_count = 0;
	ags->ags_nlive = 0;
}

static void ags_fini(struct voluta_ag_space *ags)
{
	voluta_memzero(ags, sizeof(*ags));
}

static void ags_enqueue(struct voluta_ag_space *ags, struct voluta_ag_info *agi)
{
	size_t ag_index_next;
	struct voluta_list_head *itr, *end = &ags->spq;
	const size_t ag_index = agi_index(agi);

	voluta_assert(!agi->live);
	itr = ags->spq.next;
	while (itr != end) {
		ag_index_next = agi_index(agi_of(itr));
		if (ag_index_next > ag_index) {
			break;
		}
		itr = itr->next;
	}
	list_head_insert_before(&agi->lh, itr);
	agi->live = true;
	ags->ags_nlive++;
	voluta_assert_le(ags->ags_nlive, ags->ags_count);
}

static void ags_dequeue(struct voluta_ag_space *ags, struct voluta_ag_info *agi)
{
	voluta_assert(agi->live);
	voluta_assert_gt(ags->ags_nlive, 0);

	list_head_remove(&agi->lh);
	agi->live = false;
	ags->ags_nlive--;
}

static struct voluta_ag_info *
ags_nextof(struct voluta_ag_space *ags, struct voluta_ag_info *agi)
{
	struct voluta_list_head *next = agi->lh.next;

	return (next == &ags->spq) ? NULL : agi_of(next);
}

static struct voluta_ag_info *ags_front(struct voluta_ag_space *ags)
{
	struct voluta_list_head *front = ags->spq.next;

	return (front == &ags->spq) ? NULL : agi_of(front);
}

static void ags_setup(struct voluta_ag_space *ags, loff_t space_size)
{
	ags->ags_count = (size_t)space_size / VOLUTA_AG_SIZE;
	for (size_t i = 0; i < ags->ags_count; ++i) {
		agi_setup(&ags->agi[i], i);
	}
	for (size_t j = ags->ags_count; j > 1; --j) {
		ags_enqueue(ags, &ags->agi[j - 1]);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void evacuate_space(struct voluta_sb_info *sbi,
			   struct voluta_vnode_info *agm_vi,
			   const struct voluta_vaddr *vaddr)
{
	struct voluta_ag_space *ags = &sbi->s_ag_space;

	agm_clear_active_space(agm_vi->u.agm, vaddr);
	agm_renew_if_unused(agm_vi->u.agm, vaddr);
	v_dirtify(agm_vi);
	ags_free(ags, vaddr);
}

static int require_space(const struct voluta_vnode_info *agm_vi, size_t nkb)
{
	return agm_has_space(agm_vi->u.agm, nkb) ? 0 : -ENOSPC;
}

static int inhabit_space(struct voluta_sb_info *sbi,
			 struct voluta_vnode_info *agm_vi,
			 enum voluta_vtype vtype, size_t nkb,
			 struct voluta_vaddr *out_vaddr)
{
	int err;
	struct voluta_ag_space *ags = &sbi->s_ag_space;

	err = require_space(agm_vi, nkb);
	if (err) {
		return err;
	}
	err = agm_find_free_space(agm_vi->u.agm, vtype, nkb, out_vaddr);
	if (err) {
		return err;
	}
	agm_mark_active_space(agm_vi->u.agm, out_vaddr);
	v_dirtify(agm_vi);
	ags_alloc(ags, out_vaddr);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void derive_space_stat(struct voluta_sb_info *sbi)
{
	loff_t ag_count, blocks_free;
	const loff_t ag_nbk = VOLUTA_NBK_IN_AG;
	struct voluta_sp_stat *sp_st = &sbi->s_sp_stat;

	ag_count = (loff_t)sbi->s_ag_space.ags_count;
	blocks_free = (ag_count - 1) * (ag_nbk - 1);

	/* XXX crap; should start empty and add meta like all others */
	/* TODO: fix upon recon/remount flow */
	sp_st->sp_nbytes = blocks_free * VOLUTA_BK_SIZE;
	sp_st->sp_nmeta = 0;
	sp_st->sp_ndata = 0;
	sp_st->sp_nfiles = 0;
}

int voluta_prepare_space(struct voluta_sb_info *sbi,
			 const char *volume_path, loff_t size)
{
	int err;
	loff_t space_size;

	err = voluta_resolve_volume_size(volume_path, size, &space_size);
	if (err) {
		return err;
	}
	ags_setup(&sbi->s_ag_space, space_size);
	derive_space_stat(sbi);
	return 0;
}

static ssize_t space_files_max(const struct voluta_sp_stat *sps)
{
	const ssize_t space_blocks_max = sps->sp_nbytes / VOLUTA_BK_SIZE;
	const ssize_t inodes_per_block = (VOLUTA_BK_SIZE / VOLUTA_INODE_SIZE);

	return (space_blocks_max * inodes_per_block) / 2;
}

static fsblkcnt_t bytes_to_fsblkcnt(ssize_t nbytes)
{
	return (fsblkcnt_t)nbytes / VOLUTA_DS_SIZE;
}

void voluta_space_to_statvfs(const struct voluta_sb_info *sbi,
			     struct statvfs *out_stvfs)
{
	ssize_t nbytes_use, nfiles_max;
	const struct voluta_sp_stat *sps = &sbi->s_sp_stat;

	nbytes_use = sps->sp_nmeta + sps->sp_ndata;
	nfiles_max = space_files_max(sps);

	out_stvfs->f_bsize = VOLUTA_BK_SIZE;
	out_stvfs->f_frsize = VOLUTA_DS_SIZE;
	out_stvfs->f_blocks = bytes_to_fsblkcnt(sps->sp_nbytes);
	out_stvfs->f_bfree = bytes_to_fsblkcnt(sps->sp_nbytes - nbytes_use);
	out_stvfs->f_bavail = out_stvfs->f_bfree;
	out_stvfs->f_files = (fsfilcnt_t)nfiles_max;
	out_stvfs->f_ffree = (fsfilcnt_t)(nfiles_max - sps->sp_nfiles);
	out_stvfs->f_favail = out_stvfs->f_ffree;
}

static ssize_t nkb_to_nbytes(size_t nkb)
{
	return (ssize_t)(nkb * VOLUTA_KB_SIZE);
}

static int check_space_alloc(struct voluta_sb_info *sbi,
			     enum voluta_vtype vtype, size_t nkb)
{
	ssize_t nbytes, files_max;
	const struct voluta_sp_stat *sps = &sbi->s_sp_stat;

	nbytes = nkb_to_nbytes(nkb);
	if (vtype_isdata(vtype)) {
		if ((sps->sp_ndata + nbytes) > sps->sp_nbytes) {
			return -ENOSPC;
		}
	} else {
		if ((sps->sp_nmeta + nbytes) > sps->sp_nbytes) {
			return -ENOSPC;
		}
		if (vtype_isinode(vtype)) {
			files_max = space_files_max(sps);
			if (sps->sp_nfiles >= files_max) {
				return -ENOSPC;
			}
		}
	}
	return 0;
}

static void update_space_stat(struct voluta_sb_info *sbi, int take,
			      const struct voluta_vaddr *vaddr)
{
	ssize_t nbytes, nfiles;
	struct voluta_sp_stat *sps = &sbi->s_sp_stat;

	if (take > 0) {
		nbytes = (long)(vaddr->len);
		nfiles = 1;
	} else {
		nbytes = -(long)(vaddr->len);
		nfiles = -1;
	}

	if (vtype_isdata(vaddr->vtype)) {
		sps->sp_ndata += nbytes;
	} else {
		if (vtype_isinode(vaddr->vtype)) {
			sps->sp_nfiles += nfiles;
		}
		sps->sp_nmeta += nbytes;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int spawn_bk_now(struct voluta_sb_info *sbi, loff_t lba,
			struct voluta_vnode_info *agm_vi,
			struct voluta_bk_info **out_bki)
{
	int err;

	err = voluta_cache_spawn_bk(sbi->s_cache, lba, agm_vi, out_bki);
	if (err != -ENOMEM) {
		return err;
	}
	err = voluta_commit_dirtyq(sbi, VOLUTA_F_NOW);
	if (err) {
		return err;
	}
	err = voluta_cache_spawn_bk(sbi->s_cache, lba, agm_vi, out_bki);
	if (err) {
		return err;
	}
	return 0;
}

static int spawn_bk(struct voluta_sb_info *sbi, loff_t lba,
		    struct voluta_vnode_info *agm_vi,
		    struct voluta_bk_info **out_bki)
{
	int err;

	err = spawn_bk_now(sbi, lba, agm_vi, out_bki);
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
	const struct voluta_vnode_info *agm_vi = bki->b_agm;

	for (size_t i = 0; i < ARRAY_SIZE(bk->bo); ++i) {
		kbn = kbn_of_bo(i);
		if (has_unwritten_data(agm_vi->u.agm, bki->b_lba, kbn)) {
			zero_unwritten_bo(&bk->bo[i]);
		}
	}
}

static int load_stage_bk(struct voluta_sb_info *sbi, struct voluta_bk_info *bki)
{
	int err;

	err = load_stable_bk(sbi, bki);
	if (err) {
		return err;
	}
	bki->b_staged = 1;

	if (!is_agmap(bki)) {
		zero_unwritten(bki);
	}
	return 0;
}

static int stage_bk_of(struct voluta_sb_info *sbi, loff_t lba,
		       struct voluta_vnode_info *agm_vi,
		       struct voluta_bk_info **out_bki)
{
	int err;

	err = spawn_bk(sbi, lba, agm_vi, out_bki);
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
	union voluta_view *view = view_at(bki, vaddr->off);
	const struct voluta_sb_info *sbi = bki->b_sbi;

	return voluta_verify_view(view, sbi->s_cstore->crypto, vaddr->vtype);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int allocate_from(struct voluta_sb_info *sbi,
			 struct voluta_ag_info *agi,
			 enum voluta_vtype vtype, size_t nkb,
			 struct voluta_vaddr *out)
{
	int err;
	size_t ag_index;
	struct voluta_vnode_info *agm_vi;
	struct voluta_ag_space *ags = &sbi->s_ag_space;

	ag_index = agi_index(agi);
	err = stage_agmap(sbi, ag_index, &agm_vi);
	if (err) {
		return err;
	}
	err = inhabit_space(sbi, agm_vi, vtype, nkb, out);
	if (err) {
		return err;
	}
	if (agi_is_exhausted(agi)) {
		ags_dequeue(ags, agi);
	}
	return 0;
}

/*
 * TODO-0003: Track free whole-blocks per agi
 *
 * No need to try and allocate whole block from fragmented AG
 */
static int try_allocate_from(struct voluta_sb_info *sbi,
			     struct voluta_ag_info *agi,
			     enum voluta_vtype vtype, size_t nkb,
			     struct voluta_vaddr *out_vaddr)
{
	int err = -ENOSPC;
	const int nbytes = (int)nkb_to_size(nkb);

	if (agi_cap_alloc_space(agi, nbytes)) {
		err = allocate_from(sbi, agi, vtype, nkb, out_vaddr);
		if (err == -ENOSPC) {
			agi_hint_failed_alloc(agi, nbytes);
		}
	}
	return err;
}

static int allocate_space(struct voluta_sb_info *sbi,
			  enum voluta_vtype vtype, size_t nkb,
			  struct voluta_vaddr *out_vaddr)
{
	int err = -ENOSPC;
	struct voluta_ag_info *agi;
	struct voluta_ag_space *ags = &sbi->s_ag_space;
	const size_t limit = vtype_isdata(vtype) ? 256 : 1024; /* XXX */

	agi = ags_front(ags);
	for (size_t i = 0; i < limit; ++i) {
		if (agi == NULL) {
			break;
		}
		voluta_assert_eq(agi->nfree % VOLUTA_KB_SIZE, 0);
		err = try_allocate_from(sbi, agi, vtype, nkb, out_vaddr);
		if (err != -ENOSPC) {
			break;
		}
		agi = ags_nextof(ags, agi);
	}
	return err;
}

static int deallocate_space(struct voluta_sb_info *sbi,
			    const struct voluta_vaddr *vaddr)
{
	int err;
	struct voluta_vnode_info *agm_vi;
	struct voluta_ag_space *ags = &sbi->s_ag_space;
	struct voluta_ag_info *agi = agi_by_index(ags, vaddr->ag_index);

	err = stage_agmap(sbi, vaddr->ag_index, &agm_vi);
	if (err) {
		return err;
	}
	evacuate_space(sbi, agm_vi, vaddr);
	if (!agi->live) {
		ags_enqueue(ags, agi);
	}
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

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int spawn_cached_agmap(struct voluta_sb_info *sbi, size_t ag_index,
			      struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vaddr vaddr;
	struct voluta_bk_info *bki;

	vaddr_of_agmap(&vaddr, ag_index);
	err = spawn_bk(sbi, vaddr.lba, NULL, &bki);
	if (err) {
		return err;
	}
	err = spawn_cached_vi(sbi, &vaddr, out_vi);
	if (err) {
		return err;
	}
	bind_vnode_to_bk(*out_vi, bki);
	return 0;
}

static int format_agmap(struct voluta_sb_info *sbi, size_t ag_index)
{
	int err;
	struct voluta_vnode_info *agm_vi;

	err = spawn_cached_agmap(sbi, ag_index, &agm_vi);
	if (err) {
		return err;
	}
	setup_agmap(sbi, agm_vi);
	v_dirtify(agm_vi);

	ags_set_formatted(&sbi->s_ag_space, ag_index);
	return 0;
}

static void promote_cache_cycle(struct voluta_sb_info *sbi)
{
	voluta_cache_next_cycle(sbi->s_cache);
}

int voluta_format_agmaps(struct voluta_sb_info *sbi)
{
	int err;
	const size_t ag_count = sbi->s_ag_space.ags_count;

	for (size_t ag_index = 1; ag_index < ag_count; ++ag_index) {
		err = format_agmap(sbi, ag_index);
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

static int load_agmap(struct voluta_sb_info *sbi, size_t ag_index)
{
	int err;
	struct voluta_vnode_info *agm_vi;

	err = stage_agmap(sbi, ag_index, &agm_vi);
	if (err) {
		return err;
	}
	/* TODO: populate space properly */
	ags_set_formatted(&sbi->s_ag_space, ag_index);
	return 0;
}

int voluta_load_agmaps(struct voluta_sb_info *sbi)
{
	int err;
	const size_t ag_count = sbi->s_ag_space.ags_count;

	for (size_t ag_index = 1; ag_index < ag_count; ++ag_index) {
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
	size_t ag_index = 1;

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
	case VOLUTA_VTYPE_DATA:
	case VOLUTA_VTYPE_SUPER:
	case VOLUTA_VTYPE_AGMAP:
	case VOLUTA_VTYPE_ITNODE:
	case VOLUTA_VTYPE_INODE:
	case VOLUTA_VTYPE_XANODE:
	case VOLUTA_VTYPE_HTNODE:
	case VOLUTA_VTYPE_RTNODE:
	case VOLUTA_VTYPE_SYMVAL:
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

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void sbi_init_commons(struct voluta_sb_info *sbi,
			     struct voluta_cache *cache,
			     struct voluta_cstore *cstore)
{
	voluta_uuid_generate(&sbi->s_fs_uuid);
	voluta_init_itable(sbi);
	ags_init(&sbi->s_ag_space);
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
	ags_fini(&sbi->s_ag_space);
	sbi->s_name = NULL;
	sbi->s_owner = (uid_t)(-1);
	sbi->s_cache = NULL;
	sbi->s_qalloc = NULL;
	sbi->s_cstore = NULL;
	sbi->s_pstore = NULL;
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
	sbi->s_sp_stat.sp_nmeta = LONG_MAX;
	sbi->s_sp_stat.sp_ndata = LONG_MAX;
	sbi->s_sp_stat.sp_nfiles = LONG_MAX;
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
	sbi_init_sp_stat(sbi);
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
	sbi_fini_iconv(sbi);
	sbi_fini_meta_bks(sbi);
	sbi_fini_sp_stat(sbi);
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

	stamp_dirty_bk(sbi, bki);

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
	case VOLUTA_VTYPE_AGMAP:
		vi->u.agm = &vi->view->agm;
		break;
	case VOLUTA_VTYPE_ITNODE:
		vi->u.itn = &vi->view->itn;
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
	case VOLUTA_VTYPE_SUPER:
	case VOLUTA_VTYPE_INODE:
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

static void bind_inode_to_bk(struct voluta_inode_info *ii,
			     struct voluta_bk_info *bki)
{
	bind_vnode_to_bk(&ii->vi, bki);
	ii->inode = &ii->vi.view->inode;
}

static int stage_agmap(struct voluta_sb_info *sbi, size_t ag_index,
		       struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vaddr vaddr;
	struct voluta_bk_info *bki;

	vaddr_of_agmap(&vaddr, ag_index);
	err = find_cached_vi(sbi, &vaddr, out_vi);
	if (!err) {
		return 0; /* Cache hit */
	}
	err = stage_bk_of(sbi, vaddr.lba, NULL, &bki);
	if (err) {
		return err;
	}
	err = verify_vnode_at(bki, &vaddr);
	if (err) {
		return err;
	}
	err = spawn_cached_vi(sbi, &vaddr, out_vi);
	if (err) {
		return err;
	}
	bind_vnode_to_bk(*out_vi, bki);
	return 0;
}

static int stage_inode(struct voluta_sb_info *sbi, bool stable,
		       const struct voluta_iaddr *iaddr,
		       struct voluta_inode_info **out_ii)
{
	int err;
	struct voluta_bk_info *bki;

	err = stage_ag_and_bk_of(sbi, stable, &iaddr->vaddr, &bki);
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
	err = spawn_cached_vi(sbi, vaddr, out_vi);
	if (err) {
		return err;
	}
	bind_vnode_to_bk(*out_vi, bki);
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

	psize = voluta_persistent_size(vtype);
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
		      enum voluta_vtype vtype, struct voluta_vaddr *out_vaddr)
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
	update_space_stat(sbi, 1, out_vaddr);
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
	update_space_stat(sbi, -1, vaddr);
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
	update_space_stat(sbi, -1, vaddr);
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
