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


static bool vtype_isdata(enum voluta_vtype vtype)
{
	return (vtype == VOLUTA_VTYPE_DATA);
}

static bool vtype_isinode(enum voluta_vtype vtype)
{
	return (vtype == VOLUTA_VTYPE_INODE);
}

static bool vtype_isnone(enum voluta_vtype vtype)
{
	return (vtype == VOLUTA_VTYPE_NONE);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool lba_isnull(loff_t lba)
{
	return (lba == VOLUTA_LBA_NULL);
}

bool voluta_off_isnull(loff_t off)
{
	return (off == VOLUTA_OFF_NULL);
}

static size_t nkbs_to_size(size_t nkbs)
{
	const size_t kbs_size = VOLUTA_KBS_SIZE;

	return (nkbs * kbs_size);
}

static size_t size_to_nkbs(size_t nbytes)
{
	const size_t kbs_size = VOLUTA_KBS_SIZE;

	return (nbytes + kbs_size - 1) / kbs_size;
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

static loff_t lba_sbn_to_off(loff_t lba, size_t sbn)
{
	const loff_t kbs_size = VOLUTA_KBS_SIZE;

	return lba_to_off(lba) + ((loff_t)sbn * kbs_size);
}

static loff_t off_to_kbs(loff_t off)
{
	const loff_t kbs_size = VOLUTA_KBS_SIZE;

	return (off / kbs_size);
}

static size_t off_to_sbn(loff_t off)
{
	const loff_t nkbs_in_bk = VOLUTA_NKBS_IN_BK;

	return (size_t)(off_to_kbs(off) % nkbs_in_bk);
}

static size_t ag_index_of(loff_t lba)
{
	const loff_t nbk_in_ag = VOLUTA_NBK_IN_AG;

	voluta_assert_gt(lba, 0);
	voluta_assert_ge(lba, nbk_in_ag);

	return (size_t)(lba / nbk_in_ag);
}

static loff_t lba_by_uber(loff_t uber_lba, size_t bn)
{
	voluta_assert_lt(bn, VOLUTA_NBK_IN_AG);
	voluta_assert_gt(bn, 0);

	return uber_lba + (loff_t)bn;
}

static loff_t lba_of_uber(size_t ag_index)
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

static void vaddr_setup(struct voluta_vaddr *vaddr, enum voluta_vtype vtype,
			size_t ag_index, loff_t lba, size_t sbn, size_t len)
{
	vaddr->ag_index = ag_index;
	vaddr->lba = lba;
	vaddr->len = len;
	vaddr->off = lba_sbn_to_off(lba, sbn);
	vaddr->vtype = vtype;
}

static void vaddr_setup2(struct voluta_vaddr *vaddr, enum voluta_vtype vtype,
			 size_t ag_index, size_t bn, size_t sbn, size_t len)
{
	const loff_t lba = lba_by_uber(lba_of_uber(ag_index), bn);

	vaddr_setup(vaddr, vtype, ag_index, lba, sbn, len);
}

static void vaddr_by_lba(struct voluta_vaddr *vaddr,
			 enum voluta_vtype vtype, loff_t lba)
{
	const size_t len = VOLUTA_BK_SIZE;

	if (!lba_isnull(lba)) {
		vaddr_setup(vaddr, vtype, ag_index_of(lba), lba, 0, len);
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
	size_t len = 0;

	voluta_psize_of(vtype, 0, &len);
	vaddr_by_off_len(vaddr, vtype, off, len);
}

bool voluta_vaddr_isnull(const struct voluta_vaddr *vaddr)
{
	return off_isnull(vaddr->off) || lba_isnull(vaddr->lba);
}

static void vaddr_of_uber(struct voluta_vaddr *vaddr, size_t ag_index)
{
	vaddr_by_lba(vaddr, VOLUTA_VTYPE_UBER, lba_of_uber(ag_index));
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_iaddr_reset(struct voluta_iaddr *iaddr)
{
	voluta_vaddr_reset(&iaddr->vaddr);
	iaddr->ino = VOLUTA_INO_NULL;
}

void voluta_iaddr_setup(struct voluta_iaddr *iaddr,
			ino_t ino, loff_t off, size_t len)
{
	vaddr_by_off_len(&iaddr->vaddr, VOLUTA_VTYPE_INODE, off, len);
	iaddr->ino = ino;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t nkbs_of(const struct voluta_vaddr *vaddr)
{
	return size_to_nkbs(vaddr->len);
}

static size_t sbn_of(const struct voluta_vaddr *vaddr)
{
	voluta_assert_eq(vaddr->off % VOLUTA_KBS_SIZE, 0);

	return off_to_sbn(vaddr->off);
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

static size_t bkref_nkbs(const struct voluta_bkref *bkref)
{
	return ARRAY_SIZE(bkref->b_k.vtype);
}

static void bkref_init(struct voluta_bkref *bkref)
{
	bkref->b_refcnt = 0;
	bkref->b_usecnt = 0;

	for (size_t i = 0; i < ARRAY_SIZE(bkref->b_k.vtype); ++i) {
		bkref->b_k.vtype[i] = VOLUTA_VTYPE_NONE;
	}
}

static void bkref_init_arr(struct voluta_bkref *arr, size_t cnt)
{
	for (size_t i = 0; i < cnt; ++i) {
		bkref_init(&arr[i]);
	}
}

static void bkref_renew_iv(struct voluta_bkref *bkref)
{
	voluta_fill_random_iv(&bkref->b_iv);
}

static void bkref_set_sbn(struct voluta_bkref *bkref,
			  enum voluta_vtype vtype, size_t sbn)
{
	voluta_assert_lt(bkref->b_refcnt, ARRAY_SIZE(bkref->b_k.vtype));
	voluta_assert_lt(sbn, ARRAY_SIZE(bkref->b_k.vtype));

	bkref->b_k.vtype[sbn] = (uint8_t)vtype;
	bkref->b_usecnt++;
	bkref->b_unwritten = 1;
	bkref->b_refcnt++;
}

static void bkref_clear_sbn(struct voluta_bkref *bkref, size_t sbn)
{
	voluta_assert_gt(bkref->b_usecnt, 0);
	voluta_assert_gt(bkref->b_refcnt, 0);
	voluta_assert_lt(sbn, ARRAY_SIZE(bkref->b_k.vtype));

	bkref->b_usecnt--;
	bkref->b_refcnt--;
	bkref->b_unwritten = 0;
	bkref->b_k.vtype[sbn] = VOLUTA_VTYPE_NONE;
}

static int bkref_vtype_at(const struct voluta_bkref *bkref, size_t ki)
{
	voluta_assert_lt(ki, ARRAY_SIZE(bkref->b_k.vtype));

	return bkref->b_k.vtype[ki];
}

static bool bkref_is_unwritten(const struct voluta_bkref *bkref)
{
	return bkref->b_unwritten;
}

static void bkref_clear_unwritten(struct voluta_bkref *bkref)
{
	bkref->b_unwritten = 0;
}

static void bkref_mark_unwritten(struct voluta_bkref *bkref)
{
	bkref->b_unwritten = 1;
}

static size_t bkref_nfree(const struct voluta_bkref *bkref)
{
	voluta_assert_le(bkref->b_usecnt, bkref_nkbs(bkref));

	return bkref_nkbs(bkref) - bkref->b_usecnt;
}

static size_t bkref_nused_seq(const struct voluta_bkref *bkref, size_t from_k)
{
	size_t nused = 0;
	enum voluta_vtype vtype;

	for (size_t i = from_k; i < ARRAY_SIZE(bkref->b_k.vtype); ++i) {
		vtype = bkref_vtype_at(bkref, i);
		if (vtype_isnone(vtype)) {
			break;
		}
		nused += 1;
	}
	return nused;
}

static size_t bkref_nfree_seq(const struct voluta_bkref *bkref, size_t from_k)
{
	size_t nfree = 0;
	enum voluta_vtype vtype;

	for (size_t i = from_k; i < ARRAY_SIZE(bkref->b_k.vtype); ++i) {
		vtype = bkref_vtype_at(bkref, i);
		if (!vtype_isnone(vtype)) {
			break;
		}
		nfree += 1;
	}
	return nfree;
}

static bool bkref_has_nfree(const struct voluta_bkref *bkref, size_t nkbs)
{
	return bkref_nfree(bkref) >= nkbs;
}

static int bkref_find_nfree_seq(const struct voluta_bkref *bkref,
				size_t nkbs, size_t *out_sbn)
{
	int err = -ENOSPC;
	size_t nused, nfree, mod, sbn = 0;
	const size_t nkbs_per_bk = bkref_nkbs(bkref);

	while ((sbn < nkbs_per_bk) && err) {
		nused = bkref_nused_seq(bkref, sbn);
		sbn += nused;
		mod = sbn % nkbs;
		if (mod) {
			sbn += (nkbs - mod);
		}
		nfree = bkref_nfree_seq(bkref, sbn);
		if (nfree >= nkbs) {
			*out_sbn = sbn;
			err = 0;
		}
		sbn += voluta_max(nfree, 1);
	}
	return err;
}

static int bkref_find_free_nkbs(const struct voluta_bkref *bkref,
				size_t nkbs, size_t *out_sbn)
{
	int err = -ENOSPC;

	if (bkref_has_nfree(bkref, nkbs)) {
		err = bkref_find_nfree_seq(bkref, nkbs, out_sbn);
	}
	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void bkmap_init(struct voluta_bkmap *bkmap)
{
	bkref_init_arr(bkmap->bkref, ARRAY_SIZE(bkmap->bkref));
}

static struct voluta_bkref *
bkmap_bkref_at(const struct voluta_bkmap *bkmap, size_t bn)
{
	const struct voluta_bkref *bkref;

	voluta_assert_gt(bn, 0);
	bkref = &(bkmap->bkref[bn - 1]);
	return (struct voluta_bkref *)bkref;
}

static const struct voluta_iv *
bkmap_iv_at(const struct voluta_bkmap *bkmap, size_t bn)
{
	const struct voluta_bkref *bkref = bkmap_bkref_at(bkmap, bn);

	return &bkref->b_iv;
}

static void bkmap_setup_ivs(struct voluta_bkmap *bkmap)
{
	for (size_t i = 0; i < ARRAY_SIZE(bkmap->bkref); ++i) {
		bkref_renew_iv(&bkmap->bkref[i]);
	}
}

static int bkmap_find_free_nkbs_at(const struct voluta_bkmap *bkmap,
				   size_t nkbs, size_t bn, size_t *out_sbn)
{
	const struct voluta_bkref *bkref = bkmap_bkref_at(bkmap, bn);

	return bkref_find_free_nkbs(bkref, nkbs, out_sbn);
}

static int bkmap_find_free_nkbs(const struct voluta_bkmap *bkmap,
				size_t nkbs, size_t *out_bn, size_t *out_sbn)
{
	int err;
	size_t sbn, bn;

	for (size_t i = 0; i < ARRAY_SIZE(bkmap->bkref); ++i) {
		bn = i + 1;
		err = bkmap_find_free_nkbs_at(bkmap, nkbs, bn, &sbn);
		if (!err) {
			*out_bn = bn;
			*out_sbn = sbn;
			return 0;
		}
	}
	return -ENOSPC;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t bn_of(const struct voluta_vaddr *vaddr)
{
	const loff_t nbk_in_ag = VOLUTA_NBK_IN_AG;

	return (size_t)(vaddr->lba % nbk_in_ag);
}

static void uber_init(struct voluta_uber_block *ub, size_t ag_index)
{
	ub->u_ag_index = ag_index;
	ub->u_nkbs_used = VOLUTA_NKBS_IN_BK;
	ub->u_ciper_type = VOLUTA_CIPHER_AES256;
	ub->u_ciper_mode = VOLUTA_CIPHER_MODE_CBC;
	bkmap_init(&ub->u_bkmap);
}

static void uber_set_fs_uuid(struct voluta_uber_block *ub,
			     const struct voluta_uuid *uuid)
{
	voluta_uuid_clone(uuid, &ub->u_fs_uuid);
}

static struct voluta_bkmap *uber_bkmap(const struct voluta_uber_block *ub)
{
	const struct voluta_bkmap *bkmap = &ub->u_bkmap;

	return (struct voluta_bkmap *)bkmap;
}

static struct voluta_bkref *
uber_bkref_at(const struct voluta_uber_block *ub,
	      const struct voluta_vaddr *vaddr)
{
	return bkmap_bkref_at(&ub->u_bkmap, bn_of(vaddr));
}

static void uber_setup_iv_keys(struct voluta_uber_block *ub)
{
	struct voluta_bkmap *bkmap = uber_bkmap(ub);

	voluta_fill_random_key(&ub->u_key);
	bkmap_setup_ivs(bkmap);
}

static void uber_iv_key_of(const struct voluta_uber_block *ub,
			   const struct voluta_vaddr *vaddr,
			   struct voluta_iv_key *out_iv_key)
{
	const struct voluta_iv *iv;

	iv = bkmap_iv_at(uber_bkmap(ub), bn_of(vaddr));
	iv_clone(iv, &out_iv_key->iv);
	key_clone(&ub->u_key, &out_iv_key->key);
}

static int uber_find_free_space(const struct voluta_uber_block *ub,
				enum voluta_vtype vtype, size_t nkbs,
				struct voluta_vaddr *out_vaddr)
{
	int err;
	size_t bn, sbn, len;

	err = bkmap_find_free_nkbs(&ub->u_bkmap, nkbs, &bn, &sbn);
	if (err) {
		return err;
	}
	len = nkbs_to_size(nkbs);
	vaddr_setup2(out_vaddr, vtype, ub->u_ag_index, bn, sbn, len);
	return 0;
}

static int uber_require_space(const struct voluta_uber_block *ub, size_t nbks)
{
	const size_t nkbs_in_ag = VOLUTA_NKBS_IN_AG;

	return ((ub->u_nkbs_used + nbks) <= nkbs_in_ag) ? 0 : -ENOSPC;
}

static void uber_inc_used_space(struct voluta_uber_block *ub,
				const struct voluta_vaddr *vaddr)
{
	const size_t nkbs = nkbs_of(vaddr);

	voluta_assert_le(ub->u_nkbs_used + nkbs, VOLUTA_NKBS_IN_AG);

	ub->u_nkbs_used += (uint32_t)nkbs;
}

static void uber_dec_used_space(struct voluta_uber_block *ub,
				const struct voluta_vaddr *vaddr)
{
	const size_t nkbs = nkbs_of(vaddr);

	voluta_assert_ge(ub->u_nkbs_used, nkbs);
	voluta_assert_le(ub->u_nkbs_used, VOLUTA_NKBS_IN_AG);

	ub->u_nkbs_used -= (uint32_t)nkbs;
}

static void uber_mark_active_space(struct voluta_uber_block *ub,
				   const struct voluta_vaddr *vaddr)
{
	const size_t sbn = sbn_of(vaddr);
	const size_t nkbs = nkbs_of(vaddr);
	struct voluta_bkref *bkref = uber_bkref_at(ub, vaddr);

	for (size_t i = 0; i < nkbs; ++i) {
		bkref_set_sbn(bkref, vaddr->vtype, sbn + i);
	}
	uber_inc_used_space(ub, vaddr);
}

static void uber_clear_used_space(struct voluta_uber_block *ub,
				  const struct voluta_vaddr *vaddr)
{
	const size_t sbn = sbn_of(vaddr);
	const size_t nkbs = nkbs_of(vaddr);
	struct voluta_bkref *bkref = uber_bkref_at(ub, vaddr);

	for (size_t i = 0; i < nkbs; ++i) {
		bkref_clear_sbn(bkref, sbn + i);
	}
	uber_dec_used_space(ub, vaddr);
	if (!ub->u_nkbs_used) {
		bkref_mark_unwritten(bkref);
		bkref_renew_iv(bkref);
	}
}

static int uber_has_unwritten(const struct voluta_uber_block *ub,
			      const struct voluta_vaddr *vaddr)
{
	const struct voluta_bkref *bkref = uber_bkref_at(ub, vaddr);

	return bkref_is_unwritten(bkref);
}

static void uber_clear_unwritten(struct voluta_uber_block *ub,
				 const struct voluta_vaddr *vaddr)
{
	struct voluta_bkref *bkref = uber_bkref_at(ub, vaddr);

	bkref_clear_unwritten(bkref);
}

static bool uber_is_free_bk(const struct voluta_uber_block *ub,
			    const struct voluta_vaddr *vaddr)
{
	const struct voluta_bkref *bkref = uber_bkref_at(ub, vaddr);

	return bkref_has_nfree(bkref, VOLUTA_NKBS_IN_BK);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool is_uber_bk(const struct voluta_bk_info *bki)
{
	return (bki->b_vaddr.vtype == VOLUTA_VTYPE_UBER);
}

static struct voluta_uber_block *uber_of(const struct voluta_bk_info *bki)
{
	voluta_assert_not_null(bki);
	voluta_assert_eq(bki->b_vaddr.vtype, VOLUTA_VTYPE_UBER);

	return &bki->bk->ub;
}

static const struct voluta_vaddr *bk_vaddr(const struct voluta_bk_info *bki)
{
	return voluta_vaddr_of_bk(bki);
}

static void iv_key_by_uber_of(const struct voluta_bk_info *bki,
			      struct voluta_iv_key *out_iv_key)

{
	const struct voluta_uber_block *ub = uber_of(bki->b_uber_bki);

	uber_iv_key_of(ub, bk_vaddr(bki), out_iv_key);
}

static void iv_key_of_uber_bk(const struct voluta_bk_info *bki,
			      struct voluta_iv_key *out_iv_key)
{
	size_t ag_index, iv_index, key_index;
	const struct voluta_super_block *sb = bki->b_sbi->sb;
	const struct voluta_vaddr *vaddr = bk_vaddr(bki);

	ag_index = vaddr->ag_index;
	iv_index = ag_index % ARRAY_SIZE(sb->s_ivs);
	key_index = ag_index % ARRAY_SIZE(sb->s_keys);

	iv_clone(&sb->s_ivs[iv_index], &out_iv_key->iv);
	key_clone(&sb->s_keys[key_index], &out_iv_key->key);
}

static void iv_key_of(const struct voluta_bk_info *bki,
		      struct voluta_iv_key *out_iv_key)
{
	if (is_uber_bk(bki)) {
		iv_key_of_uber_bk(bki, out_iv_key);
	} else {
		voluta_assert_not_null(bki->b_uber_bki);
		iv_key_by_uber_of(bki, out_iv_key);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int vtype_at(const struct voluta_bk_info *uber_bki,
		    const struct voluta_vaddr *vaddr)
{
	const struct voluta_uber_block *uber = uber_of(uber_bki);
	const struct voluta_bkref *bkref = uber_bkref_at(uber, vaddr);

	return bkref_vtype_at(bkref, sbn_of(vaddr));
}

static void setup_uber_bk(struct voluta_bk_info *uber_bki)
{
	struct voluta_uber_block *ub = uber_of(uber_bki);
	const struct voluta_vaddr *vaddr = bk_vaddr(uber_bki);

	voluta_stamp_vbk(uber_bki);
	uber_init(ub, vaddr->ag_index);
	uber_setup_iv_keys(ub);
	uber_set_fs_uuid(ub, &uber_bki->b_sbi->s_fs_uuid);
}

bool voluta_is_unwritten(const struct voluta_bk_info *bki)
{
	const struct voluta_uber_block *ub = uber_of(bki->b_uber_bki);

	return uber_has_unwritten(ub, bk_vaddr(bki));
}

void voluta_clear_unwritten(const struct voluta_bk_info *bki)
{
	struct voluta_bk_info *uber_bki = bki->b_uber_bki;
	struct voluta_uber_block *ub = uber_of(uber_bki);

	uber_clear_unwritten(ub, bk_vaddr(bki));
	bk_dirtify(uber_bki);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void agi_setup(struct voluta_ag_info *agi)
{
	list_head_init(&agi->lh);
	agi->nfree = VOLUTA_AG_SIZE;
	agi->uber = false;
	agi->live = false;
}

static struct voluta_ag_info *
agi_by_index(const struct voluta_ag_space *ags, size_t ag_index)
{
	const struct voluta_ag_info *agi = &ags->agi[ag_index];

	voluta_assert_lt(ag_index, ags->ag_count);
	return (struct voluta_ag_info *)agi;
}

static bool agi_has_space(const struct voluta_ag_info *agi, int nbytes)
{
	return agi->uber && (agi->nfree >= nbytes);
}

static void agi_update_free(struct voluta_ag_info *agi, int nbytes)
{
	agi->nfree += (int)nbytes;
	voluta_assert_ge(agi->nfree, 0);
	voluta_assert_le(agi->nfree, VOLUTA_AG_SIZE);
}

static bool agi_is_exhausted(const struct voluta_ag_info *agi)
{
	return (agi->nfree == 0);
}

static void ag_space_alloc(struct voluta_ag_space *ags,
			   size_t ag_index, size_t len)
{
	struct voluta_ag_info *agi = agi_by_index(ags, ag_index);

	agi_update_free(agi, -(int)len);
}

static void ag_space_free(struct voluta_ag_space *ags,
			  size_t ag_index, size_t len)
{
	struct voluta_ag_info *agi = agi_by_index(ags, ag_index);

	agi_update_free(agi, (int)len);
}

static void ag_space_mark_uber(struct voluta_ag_space *ags, size_t ag_index)
{
	struct voluta_ag_info *agi = agi_by_index(ags, ag_index);

	ag_space_alloc(ags, ag_index, VOLUTA_BK_SIZE);
	agi->uber = true;
}

static void ag_space_init(struct voluta_ag_space *ags)
{
	memset(ags, 0, sizeof(*ags));
	list_head_init(&ags->spq);
	ags->ag_count = 0;
	ags->ag_nlive = 0;
}

static void ag_space_fini(struct voluta_ag_space *ags)
{
	memset(ags, 0xF1, sizeof(*ags));
	ags->ag_count = 0;
}

static size_t ag_space_index_of(const struct voluta_ag_space *ags,
				const struct voluta_ag_info *agi)
{
	return (size_t)(agi - ags->agi);
}

static struct voluta_ag_info *agi_of(struct voluta_list_head *lh)
{
	return container_of(lh, struct voluta_ag_info, lh);
}

static void ag_space_enqueue(struct voluta_ag_space *ags,
			     struct voluta_ag_info *agi)
{
	size_t ag_index_next;
	struct voluta_list_head *itr, *end = &ags->spq;
	const size_t ag_index = ag_space_index_of(ags, agi);

	voluta_assert(!agi->live);
	itr = ags->spq.next;
	while (itr != end) {
		ag_index_next = ag_space_index_of(ags, agi_of(itr));

		voluta_assert_ne(ag_index_next, ag_index);
		if (ag_index_next > ag_index) {
			break;
		}
		itr = itr->next;
	}
	list_head_insert_before(&agi->lh, itr);
	agi->live = true;
	ags->ag_nlive++;
	voluta_assert_le(ags->ag_nlive, ags->ag_count);
}

static void ag_space_dequeue(struct voluta_ag_space *ags,
			     struct voluta_ag_info *agi)
{
	voluta_assert(agi->live);
	voluta_assert_gt(ags->ag_nlive, 0);

	list_head_remove(&agi->lh);
	agi->live = false;
	ags->ag_nlive--;
}

static struct voluta_ag_info *
ag_space_nextof(struct voluta_ag_space *ags, struct voluta_ag_info *agi)
{
	struct voluta_list_head *next = agi->lh.next;

	return (next == &ags->spq) ? NULL : agi_of(next);
}

static struct voluta_ag_info *ag_space_front(struct voluta_ag_space *ags)
{
	struct voluta_list_head *front = ags->spq.next;

	return (front == &ags->spq) ? NULL : agi_of(front);
}

static void ag_space_setup(struct voluta_ag_space *ags, loff_t space_size)
{
	ags->ag_count = (size_t)space_size / VOLUTA_AG_SIZE;
	for (size_t i = 0; i < ags->ag_count; ++i) {
		agi_setup(&ags->agi[i]);
	}
	for (size_t j = ags->ag_count; j > 1; --j) {
		ag_space_enqueue(ags, &ags->agi[j - 1]);
	}
}

static void evacuate_space(struct voluta_sb_info *sbi,
			   struct voluta_bk_info *uber_bki,
			   const struct voluta_vaddr *vaddr)
{
	struct voluta_ag_space *ags = &sbi->s_ag_space;
	struct voluta_uber_block *ub = uber_of(uber_bki);

	uber_clear_used_space(ub, vaddr);
	bk_dirtify(uber_bki);
	ag_space_free(ags, vaddr->ag_index, vaddr->len);
}

static int inhabit_space(struct voluta_sb_info *sbi,
			 struct voluta_bk_info *uber_bki,
			 enum voluta_vtype vtype, size_t nkbs,
			 struct voluta_vaddr *out_vaddr)
{
	int err;
	struct voluta_ag_space *ags = &sbi->s_ag_space;
	struct voluta_uber_block *ub = uber_of(uber_bki);

	err = uber_require_space(ub, nkbs);
	if (err) {
		return err;
	}
	err = uber_find_free_space(ub, vtype, nkbs, out_vaddr);
	if (err) {
		return err;
	}
	uber_mark_active_space(ub, out_vaddr);
	bk_dirtify(uber_bki);
	ag_space_alloc(ags, out_vaddr->ag_index, out_vaddr->len);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void derive_space_stat(struct voluta_sb_info *sbi)
{
	loff_t ag_count, blocks_free;
	const loff_t ag_nbk = VOLUTA_NBK_IN_AG;
	struct voluta_sp_stat *sp_st = &sbi->s_sp_stat;

	ag_count = (loff_t)sbi->s_ag_space.ag_count;
	blocks_free = (ag_count - 1) * (ag_nbk - 1);

	/* XXX crap; should start empty and add ubers like all others */
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
	ag_space_setup(&sbi->s_ag_space, space_size);
	derive_space_stat(sbi);
	return 0;
}

static ssize_t space_blocks_max(const struct voluta_sp_stat *sps)
{
	return sps->sp_nbytes / VOLUTA_BK_SIZE;
}

static ssize_t space_files_max(const struct voluta_sp_stat *sps)
{
	const ssize_t dirs_per_block = VOLUTA_NDIRIS_IN_BK;

	return space_blocks_max(sps) * dirs_per_block;
}

static fsblkcnt_t bytes_to_fsblkcnt(ssize_t nbytes)
{
	return (fsblkcnt_t)nbytes / VOLUTA_BK_SIZE;
}

void voluta_space_to_statvfs(const struct voluta_sb_info *sbi,
			     struct statvfs *out_stvfs)
{
	ssize_t nbytes_use, nfiles_max;
	const struct voluta_sp_stat *sps = &sbi->s_sp_stat;

	nbytes_use = sps->sp_nmeta + sps->sp_ndata;
	nfiles_max = space_files_max(sps);

	out_stvfs->f_bsize = VOLUTA_BK_SIZE;
	out_stvfs->f_frsize = VOLUTA_BK_SIZE;
	out_stvfs->f_blocks = bytes_to_fsblkcnt(sps->sp_nbytes);
	out_stvfs->f_bfree = bytes_to_fsblkcnt(sps->sp_nbytes - nbytes_use);
	out_stvfs->f_bavail = out_stvfs->f_bfree;
	out_stvfs->f_files = (fsfilcnt_t)nfiles_max;
	out_stvfs->f_ffree = (fsfilcnt_t)(nfiles_max - sps->sp_nfiles);
	out_stvfs->f_favail = out_stvfs->f_ffree;
}


static ssize_t nkbs_to_ssize(size_t nkbs)
{
	return (long)(nkbs * VOLUTA_KBS_SIZE);
}

static int check_space_take(struct voluta_sb_info *sbi,
			    enum voluta_vtype vtype, size_t nkbs)
{
	ssize_t nbytes, files_max;
	const struct voluta_sp_stat *sps = &sbi->s_sp_stat;

	nbytes = nkbs_to_ssize(nkbs);
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

static int spawn_bk(struct voluta_sb_info *sbi,
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

static void clear_unwritten_bk(struct voluta_bk_info *bki)
{
	union voluta_bk *bk = bki->bk;

	memset(bk, 0, sizeof(*bk));
}

static int load_stable_bk(struct voluta_sb_info *sbi,
			  struct voluta_bk_info *bki)
{
	struct voluta_iv_key iv_key;
	const struct voluta_vaddr *vaddr = bk_vaddr(bki);

	iv_key_of(bki, &iv_key);
	return voluta_vdi_load_dec(sbi->s_vdi, vaddr->off, &iv_key, bki->bk);
}

static int load_stage_bk(struct voluta_sb_info *sbi, struct voluta_bk_info *bki)
{
	int err;

	if (!is_uber_bk(bki) && voluta_is_unwritten(bki)) {
		clear_unwritten_bk(bki);
	} else {
		err = load_stable_bk(sbi, bki);
		if (err) {
			return err;
		}
	}
	bki->b_staged = true;
	return 0;
}

static int stage_bk_of(struct voluta_sb_info *sbi,
		       const struct voluta_vaddr *vaddr,
		       struct voluta_bk_info *uber_bki,
		       struct voluta_bk_info **out_bki)
{
	int err;
	struct voluta_vaddr bk_vaddr;

	vaddr_by_lba(&bk_vaddr, vaddr->vtype, vaddr->lba);
	err = spawn_bk(sbi, &bk_vaddr, uber_bki, out_bki);
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

static void resolve_view(const struct voluta_bk_info *bki,
			 const struct voluta_vaddr *vaddr,
			 union voluta_view **out_view)
{
	union voluta_bk *bk = bki->bk;
	const size_t sbn = off_to_sbn(vaddr->off);

	*out_view = view_of(&bk->kbs[sbn]);
}

static int verify_vnode_at(const struct voluta_bk_info *bki,
			   const struct voluta_vaddr *vaddr)
{
	union voluta_view *view;

	resolve_view(bki, vaddr, &view);
	return voluta_verify_view(view, vaddr->vtype);
}

static int stage_uber_bk(struct voluta_sb_info *sbi,
			 const struct voluta_vaddr *uber_vaddr,
			 struct voluta_bk_info **out_uber_bki)
{
	int err;
	struct voluta_bk_info *bki;

	err = stage_bk_of(sbi, uber_vaddr, NULL, &bki);
	if (err) {
		return err;
	}
	err = verify_vnode_at(bki, uber_vaddr);
	if (err) {
		return err;
	}
	*out_uber_bki = bki;
	return 0;
}

static int stage_uber_of(struct voluta_sb_info *sbi,
			 const struct voluta_vaddr *vaddr,
			 struct voluta_bk_info **out_uber_bki)
{
	struct voluta_vaddr uber_vaddr;

	vaddr_of_uber(&uber_vaddr, vaddr->ag_index);
	return stage_uber_bk(sbi, &uber_vaddr, out_uber_bki);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int
allocate_from(struct voluta_sb_info *sbi, struct voluta_ag_info *agi,
	      enum voluta_vtype vtype, size_t nkbs, struct voluta_vaddr *out)
{
	int err;
	size_t ag_index;
	struct voluta_vaddr uber_vaddr;
	struct voluta_bk_info *uber_bki;
	struct voluta_ag_space *ags = &sbi->s_ag_space;

	ag_index = ag_space_index_of(ags, agi);
	vaddr_of_uber(&uber_vaddr, ag_index);

	err = stage_uber_bk(sbi, &uber_vaddr, &uber_bki);
	if (err) {
		return err;
	}
	err = inhabit_space(sbi, uber_bki, vtype, nkbs, out);
	if (err) {
		return err;
	}
	if (agi_is_exhausted(agi)) {
		ag_space_dequeue(ags, agi);
	}
	return 0;
}

static int allocate_space(struct voluta_sb_info *sbi, enum voluta_vtype vtype,
			  size_t nkbs, struct voluta_vaddr *out_vaddr)
{
	int len, err = -ENOSPC;
	struct voluta_ag_info *agi;
	struct voluta_ag_space *ags = &sbi->s_ag_space;

	len = (int)nkbs_to_size(nkbs);
	agi = ag_space_front(ags);
	while ((agi != NULL) && (err == -ENOSPC)) {
		if (agi_has_space(agi, len)) {
			err = allocate_from(sbi, agi, vtype, nkbs, out_vaddr);
		}
		agi = ag_space_nextof(ags, agi);
	}
	return err;
}

static int deallocate_space(struct voluta_sb_info *sbi,
			    const struct voluta_vaddr *vaddr)
{
	int err;
	struct voluta_bk_info *uber_bki;
	struct voluta_ag_space *ags = &sbi->s_ag_space;
	struct voluta_ag_info *agi = agi_by_index(ags, vaddr->ag_index);

	err = stage_uber_of(sbi, vaddr, &uber_bki);
	if (err) {
		return err;
	}
	evacuate_space(sbi, uber_bki, vaddr);
	if (!agi->live) {
		ag_space_enqueue(ags, agi);
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int spawn_cached_uber(struct voluta_sb_info *sbi, loff_t uber_lba,
			     struct voluta_bk_info **out_uber_bki)
{
	struct voluta_vaddr vaddr;

	vaddr_by_lba(&vaddr, VOLUTA_VTYPE_UBER, uber_lba);
	return spawn_bk(sbi, &vaddr, NULL, out_uber_bki);
}

static int format_uber_block(struct voluta_sb_info *sbi, size_t ag_index)
{
	int err;
	loff_t uber_lba;
	struct voluta_bk_info *uber_bki;

	uber_lba = lba_of_uber(ag_index);
	err = spawn_cached_uber(sbi, uber_lba, &uber_bki);
	if (err) {
		return err;
	}
	setup_uber_bk(uber_bki);
	bk_dirtify(uber_bki);

	ag_space_mark_uber(&sbi->s_ag_space, ag_index);
	return 0;
}

static void promote_cache_cycle(struct voluta_sb_info *sbi)
{
	voluta_cache_next_cycle(sbi->s_cache);
}

int voluta_format_uber_blocks(struct voluta_sb_info *sbi)
{
	int err;
	const size_t ag_count = sbi->s_ag_space.ag_count;

	for (size_t ag_index = 1; ag_index < ag_count; ++ag_index) {
		err = format_uber_block(sbi, ag_index);
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

static union voluta_bk *sb_to_bk(struct voluta_super_block *sb)
{
	return container_of(sb, union voluta_bk, sb);
}

static void fill_super_block(struct voluta_super_block *sb)
{
	voluta_stamp_meta(&sb->s_hdr, VOLUTA_VTYPE_SUPER, sizeof(*sb));
	voluta_uuid_generate(&sb->s_uuid);
	sb->s_version = VOLUTA_VERSION;
	sb->s_birth_time = (uint64_t)time(NULL);
	/* TODO: FIll name */
	for (size_t i = 0; i < ARRAY_SIZE(sb->s_keys); ++i) {
		voluta_fill_random_key(&sb->s_keys[i]);
	}
	for (size_t j = 0; j < ARRAY_SIZE(sb->s_ivs); ++j) {
		voluta_fill_random_iv(&sb->s_ivs[j]);
	}
}

int voluta_format_super_block(struct voluta_sb_info *sbi)
{
	loff_t off;
	const union voluta_bk *sb_bk;

	fill_super_block(sbi->sb);

	off = lba_to_off(VOLUTA_LBA_SUPER);
	sb_bk = sb_to_bk(sbi->sb);
	return voluta_vdi_enc_save(sbi->s_vdi, off, &sbi->s_iv_key, sb_bk);
}

int voluta_load_super_block(struct voluta_sb_info *sbi)
{
	int err;
	loff_t off;
	struct voluta_super_block *sb = sbi->sb;
	const struct voluta_iv_key *iv_key = &sbi->s_iv_key;

	off = lba_to_off(VOLUTA_LBA_SUPER);
	err = voluta_vdi_load_dec(sbi->s_vdi, off, iv_key, sb_to_bk(sb));
	if (err) {
		return err;
	}

	voluta_assert_eq(sb->s_hdr.vtype, VOLUTA_VTYPE_SUPER);

	/* XXX FIXME */
	/* TODO: call voluta_verify_vbk properly HERE !!! */

	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int load_uber_block(struct voluta_sb_info *sbi, size_t ag_index)
{
	int err;
	struct voluta_vaddr uber_vaddr;
	struct voluta_bk_info *uber_bki = NULL;

	vaddr_of_uber(&uber_vaddr, ag_index);
	err = stage_uber_bk(sbi, &uber_vaddr, &uber_bki);
	if (err) {
		return err;
	}
	/* TODO: populate space properly */
	ag_space_mark_uber(&sbi->s_ag_space, ag_index);
	return 0;
}

int voluta_load_ag_space(struct voluta_sb_info *sbi)
{
	int err;
	const size_t ag_count = sbi->s_ag_space.ag_count;

	for (size_t ag_index = 1; ag_index < ag_count; ++ag_index) {
		err = load_uber_block(sbi, ag_index);
		if (err) {
			return err;
		}
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_load_inode_table(struct voluta_sb_info *sbi)
{
	loff_t lba, uber_lba;
	struct voluta_vaddr vaddr;
	size_t ag_index = 1;

	/* TODO: Reference itable-root from super-block */
	uber_lba = lba_of_uber(ag_index);
	lba = lba_by_uber(uber_lba, 1);
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
	case VOLUTA_VTYPE_UBER:
	case VOLUTA_VTYPE_ITNODE:
	case VOLUTA_VTYPE_INODE:
	case VOLUTA_VTYPE_XANODE:
	case VOLUTA_VTYPE_DTNODE:
	case VOLUTA_VTYPE_RTNODE:
	case VOLUTA_VTYPE_SYMVAL:
		break;
	default:
		return -EFSCORRUPTED;
	}
	return 0;
}

static int verify_bkref(const struct voluta_bkref *bkref)
{
	int err;

	/* TODO: check more */
	for (size_t i = 0; i < ARRAY_SIZE(bkref->b_k.vtype); ++i) {
		err = verify_vtype(bkref->b_k.vtype[i]);
		if (err) {
			return err;
		}
	}

	return 0;
}

static int verify_bkmap(const struct voluta_bkmap *bkmap)
{
	int err;

	for (size_t i = 0; i < ARRAY_SIZE(bkmap->bkref); ++i) {
		err = verify_bkref(&bkmap->bkref[i]);
		if (err) {
			return err;
		}
	}
	return 0;
}

int voluta_verify_uber_block(const struct voluta_uber_block *ub)
{
	int err;

	if (ub->u_ag_index > 0xFFFFFFF) { /* XXX */
		return -EFSCORRUPTED;
	}
	if (ub->u_ciper_type != VOLUTA_CIPHER_AES256) {
		return -EFSCORRUPTED;
	}
	if (ub->u_ciper_mode != VOLUTA_CIPHER_MODE_CBC) {
		return -EFSCORRUPTED;
	}
	err = verify_bkmap(&ub->u_bkmap);
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
			     struct voluta_vd_info *vdi)
{
	voluta_uuid_generate(&sbi->s_fs_uuid);
	voluta_itable_init(&sbi->s_itable);
	ag_space_init(&sbi->s_ag_space);
	sbi->s_owner = getuid();
	sbi->s_name = "voluta";
	sbi->s_birth_time = time(NULL);
	sbi->s_cache = cache;
	sbi->s_qmal = cache->qmal;
	sbi->s_vdi = vdi;
}

static void sbi_fini_commons(struct voluta_sb_info *sbi)
{
	voluta_itable_fini(&sbi->s_itable);
	ag_space_fini(&sbi->s_ag_space);
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
	sbi->s_sp_stat.sp_nmeta = LONG_MAX;
	sbi->s_sp_stat.sp_ndata = LONG_MAX;
	sbi->s_sp_stat.sp_nfiles = LONG_MAX;
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
		    struct voluta_cache *cache, struct voluta_vd_info *vdi)
{
	sbi_init_commons(sbi, cache, vdi);
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

static int check_stable_vaddr(const struct voluta_bk_info *uber_bki,
			      const struct voluta_vaddr *vaddr)
{
	enum voluta_vtype vtype;

	vtype = vtype_at(uber_bki, vaddr);
	return (vtype == vaddr->vtype) ? 0 : -EFSCORRUPTED;
}

static int stage_uber_and_bk_of(struct voluta_sb_info *sbi,
				const struct voluta_vaddr *vaddr, bool stable,
				struct voluta_bk_info **out_bki)
{
	int err;
	struct voluta_bk_info *bki, *uber_bki;

	err = stage_uber_of(sbi, vaddr, &uber_bki);
	if (err) {
		return err;
	}
	if (stable) {
		err = check_stable_vaddr(uber_bki, vaddr);
		if (err) {
			return err;
		}
	}
	err = stage_bk_of(sbi, vaddr, uber_bki, &bki);
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

static int save_dirty_bk(struct voluta_sb_info *sbi,
			 const struct voluta_bk_info *bki)
{
	struct voluta_iv_key iv_key;
	const struct voluta_vaddr *vaddr = bk_vaddr(bki);

	iv_key_of(bki, &iv_key);
	return voluta_vdi_enc_save(sbi->s_vdi, vaddr->off, &iv_key, bki->bk);
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
		err = save_dirty_bk(sbi, bki);
		if (err) {
			return err;
		}
		bk_undirtify(bki);
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
		err = voluta_vdi_sync(sbi->s_vdi, true);
	}
	return err;
}

struct voluta_sb_info *voluta_sbi_of(const struct voluta_env *env)
{
	const struct voluta_sb_info *sbi = &env->sbi;

	return (struct voluta_sb_info *)sbi;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void update_specific_view(struct voluta_vnode_info *vi)
{
	const enum voluta_vtype vtype = vi->vaddr.vtype;

	if (vtype == VOLUTA_VTYPE_DATA) {
		vi->u.df = &vi->view->data_frg;
	} else if (vtype == VOLUTA_VTYPE_RTNODE) {
		vi->u.rtn = &vi->view->radix_tnode;
	} else if (vtype == VOLUTA_VTYPE_DTNODE) {
		vi->u.dtn = &vi->view->dir_tnode;
	} else if (vtype == VOLUTA_VTYPE_ITNODE) {
		vi->u.itn = &vi->view->itable_tnode;
	} else if (vtype == VOLUTA_VTYPE_XANODE) {
		vi->u.xan = &vi->view->xattr_node;
	}
}

static void bind_vnode_to_bk(struct voluta_vnode_info *vi,
			     struct voluta_bk_info *bki)
{
	resolve_view(bki, v_vaddr_of(vi), &vi->view);
	update_specific_view(vi);
	voluta_attach_vi(vi, bki);
}

static void bind_inode_to_bk(struct voluta_inode_info *ii,
			     struct voluta_bk_info *bki)
{
	bind_vnode_to_bk(&ii->vi, bki);
	ii->inode = &ii->vi.view->inode;
}

static int stage_inode(struct voluta_sb_info *sbi,
		       const struct voluta_iaddr *iaddr, bool stable,
		       struct voluta_inode_info **out_ii)
{
	int err;
	struct voluta_bk_info *bki;

	err = stage_uber_and_bk_of(sbi, &iaddr->vaddr, stable, &bki);
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

	err = stage_uber_and_bk_of(sbi, vaddr, stable, &bki);
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
	err = stage_inode(sbi, &iaddr, true, out_ii);
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

static int discard_if_falsely_bk(struct voluta_sb_info *sbi,
				 struct voluta_bk_info *bki)
{
	int err;
	struct voluta_bk_info *uber_bki = NULL;
	const struct voluta_vaddr *vaddr = bk_vaddr(bki);

	err = stage_uber_of(sbi, vaddr, &uber_bki);
	if (err) {
		return err;
	}
	if (uber_is_free_bk(uber_of(uber_bki), vaddr)) {
		voluta_cache_forget_bk(sbi->s_cache, bki);
	}
	return 0;
}

int voluta_new_vspace(struct voluta_sb_info *sbi, enum voluta_vtype vtype,
		      size_t nkbs, struct voluta_vaddr *out_vaddr)
{
	int err;

	err = check_space_take(sbi, vtype, nkbs);
	if (err) {
		return err;
	}
	err = allocate_space(sbi, vtype, nkbs, out_vaddr);
	if (err) {
		/* TODO: cleanup */
		return err;
	}
	update_space_stat(sbi, 1, out_vaddr);
	return 0;
}

static int resolve_nkbs_of(enum voluta_vtype vtype,
			   mode_t mode, size_t *out_nkbs)
{
	int err;
	size_t psize = 0;

	err = voluta_psize_of(vtype, mode, &psize);
	if (!err) {
		*out_nkbs = size_to_nkbs(psize);
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

	err = resolve_nkbs_of(vtype, mode, &nkbs);
	if (err) {
		return err;
	}
	err = voluta_new_vspace(sbi, vtype, nkbs, &iaddr.vaddr);
	if (err) {
		return err;
	}
	err = voluta_acquire_ino(sbi, &iaddr);
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

	err = resolve_nkbs_of(vtype, 0, &nkbs);
	if (err) {
		return err;
	}
	err = voluta_new_vspace(sbi, vtype, nkbs, &vaddr);
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

	err = voluta_cache_lookup_vi(sbi->s_cache, vaddr, &vi);
	return err ? free_vspace(sbi, vaddr) : voluta_del_vnode(sbi, vi);
}
