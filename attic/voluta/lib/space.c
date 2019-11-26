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
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include "voluta-lib.h"

#define BITS_PER_BYTE                   8
#define BITS_SIZE(a)                    (sizeof(a) * BITS_PER_BYTE)
#define STATICASSERT_EQ(a, b)           VOLUTA_STATICASSERT_EQ(a, b)
#define STATICASSERT_EQSIZEOF(a, b)     STATICASSERT_EQ(sizeof(a), sizeof(b))


bool voluta_ino_isnull(ino_t ino)
{
	return (ino == VOLUTA_INO_NULL);
}

bool voluta_lba_isnull(loff_t lba)
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

size_t voluta_size_to_nkbs(size_t nbytes)
{
	return size_to_nkbs(nbytes);
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

void voluta_vaddr_by_lba(struct voluta_vaddr *vaddr,
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

	voluta_vaddr_by_lba(vaddr, vtype, lba);
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
	voluta_vaddr_by_lba(vaddr, VOLUTA_VTYPE_UBER, lba_of_uber(ag_index));
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_iaddr_reset(struct voluta_iaddr *iaddr)
{
	voluta_vaddr_reset(&iaddr->vaddr);
	iaddr->ino = VOLUTA_INO_NULL;
}

void voluta_iaddr_setup(struct voluta_iaddr *iaddr, ino_t ino,
			const struct voluta_vaddr *vaddr)
{
	voluta_vaddr_clone(vaddr, &iaddr->vaddr);
	iaddr->vaddr.vtype = VOLUTA_VTYPE_INODE;
	iaddr->ino = ino;
}

void voluta_iaddr_setup2(struct voluta_iaddr *iaddr,
			 ino_t ino, loff_t off, size_t len)
{
	vaddr_by_off_len(&iaddr->vaddr, VOLUTA_VTYPE_INODE, off, len);
	iaddr->ino = ino;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t nkbs_of(const struct voluta_vaddr *vaddr)
{
	return voluta_size_to_nkbs(vaddr->len);
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
	return BITS_SIZE(bkref->b_smask);
}

static uint16_t bn_full_bitmask(void)
{
	const size_t nkbs_per_bk = bkref_nkbs(NULL);

	return (uint16_t)((1 << nkbs_per_bk) - 1);
}

static uint16_t sbn_to_bitmask(size_t sbn)
{
	uint16_t mask;
	size_t shift, nbits;
	const struct voluta_bkref *bkref = NULL;

	STATICASSERT_EQSIZEOF(mask, bkref->b_smask);

	nbits = bkref_nkbs(bkref);
	shift = (nbits - 1) & sbn;
	mask = (uint16_t)(1 << shift);
	voluta_assert_lt(sbn, nbits);

	return mask;
}

static void bkref_init(struct voluta_bkref *bkref)
{
	bkref->b_vtype = VOLUTA_VTYPE_NONE;
	bkref->b_flags = VOLUTA_BKF_NONE;
	bkref->b_smask = 0;
	bkref->b_cnt = 0;
}

static void bkref_init_arr(struct voluta_bkref *arr, size_t cnt)
{
	for (size_t i = 0; i < cnt; ++i) {
		bkref_init(&arr[i]);
	}
}

static void bkref_set_bn(struct voluta_bkref *bkref, enum voluta_vtype vtype)
{
	voluta_assert_eq(bkref->b_vtype, VOLUTA_VTYPE_NONE);
	voluta_assert_eq(bkref->b_cnt, 0);
	voluta_assert_eq(bkref->b_smask, 0);

	bkref->b_vtype = (uint8_t)vtype;
	bkref->b_flags |= VOLUTA_BKF_UNWRITTEN;
	bkref->b_smask = bn_full_bitmask();
	bkref->b_cnt = 1;
}

static void bkref_clear_bn(struct voluta_bkref *bkref)
{
	voluta_assert_ne(bkref->b_vtype, VOLUTA_VTYPE_NONE);
	voluta_assert_eq(bkref->b_cnt, 1);
	voluta_assert_eq(bkref->b_smask, bn_full_bitmask());

	bkref->b_vtype = VOLUTA_VTYPE_NONE;
	bkref->b_flags = VOLUTA_BKF_NONE;
	bkref->b_smask = 0;
	bkref->b_cnt = 0;
}

static void bkref_set_sbn(struct voluta_bkref *bkref,
			  enum voluta_vtype vtype, size_t sbn)
{
	const uint16_t mask = sbn_to_bitmask(sbn);

	voluta_assert_lt(bkref->b_cnt, bkref_nkbs(bkref));

	bkref->b_vtype = (uint8_t)vtype;
	bkref->b_smask |= mask;
	bkref->b_cnt += 1;
}

static void bkref_clear_sbn(struct voluta_bkref *bkref, size_t sbn)
{
	const uint16_t mask = sbn_to_bitmask(sbn);

	voluta_assert_gt(bkref->b_cnt, 0);

	bkref->b_smask &= (uint16_t)(~mask);
	bkref->b_cnt -= 1;
	if (!bkref->b_cnt) {
		bkref->b_vtype = VOLUTA_VTYPE_NONE;
	}
}

static bool smask_isset_at(uint16_t smask, size_t sbn)
{
	const uint16_t mask = sbn_to_bitmask(sbn);

	return ((smask & mask) == mask);
}

static bool smask_is_full(uint16_t smask)
{
	return (smask == bn_full_bitmask());
}

static bool bkref_test_sbn(const struct voluta_bkref *bkref, size_t sbn)
{
	return smask_isset_at(bkref->b_smask, sbn);
}

static bool bkref_is_full(const struct voluta_bkref *bkref)
{
	return smask_is_full(bkref->b_smask);
}

static bool bkref_may_hold(const struct voluta_bkref *bkref,
			   enum voluta_vtype vtype)
{
	const enum voluta_vtype b_vtype = bkref->b_vtype;

	if (bkref_is_full(bkref)) {
		voluta_assert_ne(b_vtype, VOLUTA_VTYPE_NONE);
		return false;
	}
	if (b_vtype == VOLUTA_VTYPE_NONE) {
		voluta_assert_eq(bkref->b_smask, 0);
		return true;
	}
	return (b_vtype == vtype);
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

static struct voluta_iv *
bkmap_iv_at(const struct voluta_bkmap *bkmap, size_t bn)
{
	const struct voluta_iv *iv;
	const struct voluta_bkref *bkref;

	bkref = bkmap_bkref_at(bkmap, bn);
	iv = &bkref->b_iv;
	return (struct voluta_iv *)iv;
}

static void bkmap_renew_iv(struct voluta_bkmap *bkmap, size_t bn)
{
	voluta_fill_random_iv(bkmap_iv_at(bkmap, bn));
}

static void bkmap_setup_ivs(struct voluta_bkmap *bkmap)
{
	size_t bn;

	for (size_t i = 0; i < ARRAY_SIZE(bkmap->bkref); ++i) {
		bn = i + 1;
		bkmap_renew_iv(bkmap, bn);
	}
}

static bool bkmap_is_free_bk(const struct voluta_bkmap *bkmap, size_t bn)
{
	const struct voluta_bkref *bkref = bkmap_bkref_at(bkmap, bn);
	const enum voluta_vtype vtype = bkref->b_vtype;

	return (vtype == VOLUTA_VTYPE_NONE);
}

static int bkmap_find_free_bk(const struct voluta_bkmap *bkmap, size_t *out_bn)
{
	size_t bn;

	for (size_t i = 0; i < ARRAY_SIZE(bkmap->bkref); ++i) {
		bn = i + 1;
		if (bkmap_is_free_bk(bkmap, bn)) {
			*out_bn = bn;
			return 0;
		}
	}
	return -ENOSPC;
}

static bool bkmap_is_free_kbs(const struct voluta_bkmap *bkmap,
			      size_t bn, size_t sbn)
{
	const struct voluta_bkref *bkref = bkmap_bkref_at(bkmap, bn);

	return !bkref_test_sbn(bkref, sbn);
}

static size_t bkmap_count_used_kbs_at(const struct voluta_bkmap *bkmap,
				      size_t bn, size_t sbn)
{
	size_t nkbs_per_bk, nused = 0;
	const struct voluta_bkref *bkref = bkmap_bkref_at(bkmap, bn);

	nkbs_per_bk = bkref_nkbs(bkref);
	for (size_t i = 0; (sbn + i) < nkbs_per_bk; ++i) {
		if (bkmap_is_free_kbs(bkmap, bn, sbn + i)) {
			break;
		}
		nused += 1;
	}
	voluta_assert_le(nused, bkref->b_cnt);
	voluta_assert_le(nused, nkbs_per_bk);
	return nused;
}

static size_t bkmap_count_free_kbs_at(const struct voluta_bkmap *bkmap,
				      size_t bn, size_t sbn)
{
	size_t nkbs_per_bk, nfree = 0;
	const struct voluta_bkref *bkref = bkmap_bkref_at(bkmap, bn);

	nkbs_per_bk = bkref_nkbs(bkref);
	for (size_t i = 0; (sbn + i) < nkbs_per_bk; ++i) {
		if (!bkmap_is_free_kbs(bkmap, bn, sbn + i)) {
			break;
		}
		nfree += 1;
	}
	voluta_assert_le(bkref->b_cnt + nfree, nkbs_per_bk);
	voluta_assert_le(nfree, nkbs_per_bk);
	return nfree;
}

static int bkmap_find_free_nkbs_at(const struct voluta_bkmap *bkmap,
				   size_t nkbs, size_t bn, size_t *out_sbn)
{
	size_t nused, nfree, mod, sbn = 0;
	const size_t nkbs_per_bk = bkref_nkbs(NULL);

	while (sbn < nkbs_per_bk) {
		nused = bkmap_count_used_kbs_at(bkmap, bn, sbn);
		sbn += nused;
		mod = sbn % nkbs;
		if (mod) {
			sbn += (nkbs - mod);
		}
		nfree = bkmap_count_free_kbs_at(bkmap, bn, sbn);
		if (nfree >= nkbs) {
			*out_sbn = sbn;
			return 0;
		}
		sbn += voluta_max(nfree, 1);
	}
	return -ENOSPC;
}

static int bkmap_find_free_nkbs(const struct voluta_bkmap *bkmap,
				enum voluta_vtype vtype, size_t nkbs,
				size_t *out_bn, size_t *out_sbn)
{
	int err;
	size_t sbn, bn;
	const struct voluta_bkref *bkref;

	for (size_t i = 0; i < ARRAY_SIZE(bkmap->bkref); ++i) {
		bn = i + 1;
		bkref = bkmap_bkref_at(bkmap, bn);
		if (!bkref_may_hold(bkref, vtype)) {
			continue;
		}
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
	ub->u_nkbs_used = 0;
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

static void uber_set_iv_keys(struct voluta_uber_block *ub)
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

static int uber_find_free_nkbs(const struct voluta_uber_block *ub,
			       enum voluta_vtype vtype, size_t nkbs,
			       struct voluta_vaddr *out_vaddr)
{
	int err;
	size_t bn, sbn, len;

	err = bkmap_find_free_nkbs(&ub->u_bkmap, vtype, nkbs, &bn, &sbn);
	if (err) {
		return err;
	}
	len = nkbs_to_size(nkbs);
	vaddr_setup2(out_vaddr, vtype, ub->u_ag_index, bn, sbn, len);
	return 0;
}

static int uber_find_free(const struct voluta_uber_block *ub,
			  enum voluta_vtype vtype,
			  struct voluta_vaddr *out_vaddr)
{
	int err;
	size_t bn, bk_size = VOLUTA_BK_SIZE;

	err = bkmap_find_free_bk(&ub->u_bkmap, &bn);
	if (err) {
		return err;
	}
	vaddr_setup2(out_vaddr, vtype, ub->u_ag_index, bn, 0, bk_size);
	return 0;
}

static int uber_require_space(const struct voluta_uber_block *ub)
{
	return (ub->u_nkbs_used == VOLUTA_NKBS_IN_AG) ? -ENOSPC : 0;
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

static void uber_set_nkbs_slots(struct voluta_uber_block *ub,
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

static void uber_clear_nkbs_slots(struct voluta_uber_block *ub,
				  const struct voluta_vaddr *vaddr)
{
	const size_t sbn = sbn_of(vaddr);
	const size_t nkbs = nkbs_of(vaddr);
	struct voluta_bkref *bkref = uber_bkref_at(ub, vaddr);

	for (size_t i = 0; i < nkbs; ++i) {
		bkref_clear_sbn(bkref, sbn + i);
	}
	uber_dec_used_space(ub, vaddr);
}

static void uber_set_bk_slot(struct voluta_uber_block *ub,
			     const struct voluta_vaddr *vaddr)
{
	struct voluta_bkmap *bkmap = uber_bkmap(ub);
	struct voluta_bkref *bkref = uber_bkref_at(ub, vaddr);

	bkref_set_bn(bkref, vaddr->vtype);
	bkmap_renew_iv(bkmap, bn_of(vaddr));
	uber_inc_used_space(ub, vaddr);
}

static void uber_clear_bk_slot(struct voluta_uber_block *ub,
			       const struct voluta_vaddr *vaddr)
{
	struct voluta_bkref *bkref = uber_bkref_at(ub, vaddr);

	bkref_clear_bn(bkref);
	uber_dec_used_space(ub, vaddr);
}

static void uber_setn(struct voluta_uber_block *ub,
		      const struct voluta_vaddr *vaddr)
{
	uber_set_nkbs_slots(ub, vaddr);
}

static void uber_unsetn(struct voluta_uber_block *ub,
			const struct voluta_vaddr *vaddr)
{
	uber_clear_nkbs_slots(ub, vaddr);
}

static void uber_set(struct voluta_uber_block *ub,
		     const struct voluta_vaddr *vaddr)
{
	voluta_assert_ne(vaddr->vtype, VOLUTA_VTYPE_NONE);
	uber_set_bk_slot(ub, vaddr);
}

static void uber_unset(struct voluta_uber_block *ub,
		       const struct voluta_vaddr *vaddr)
{
	uber_clear_bk_slot(ub, vaddr);
}

static int uber_bkflags_of(const struct voluta_uber_block *ub,
			   const struct voluta_vaddr *vaddr)
{
	const struct voluta_bkref *bkref = uber_bkref_at(ub, vaddr);

	return bkref->b_flags;
}

static void uber_unsetf(struct voluta_uber_block *ub,
			const struct voluta_vaddr *vaddr,
			enum voluta_bk_flags flags)
{
	struct voluta_bkref *bkref = uber_bkref_at(ub, vaddr);

	bkref->b_flags &= (uint8_t)(~flags);
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

static void iv_key_by_uber_of(const struct voluta_bk_info *bki,
			      struct voluta_iv_key *out_iv_key)

{
	const struct voluta_uber_block *ub = uber_of(bki->b_uber_bki);

	uber_iv_key_of(ub, bk_vaddr_of(bki), out_iv_key);
}

static void iv_key_of_uber_bk(const struct voluta_bk_info *bki,
			      struct voluta_iv_key *out_iv_key)
{
	size_t ag_index, iv_index, key_index;
	const struct voluta_super_block *sb = bki->b_sbi->sb;
	const struct voluta_vaddr *vaddr = bk_vaddr_of(bki);

	ag_index = vaddr->ag_index;
	iv_index = ag_index % ARRAY_SIZE(sb->s_ivs);
	key_index = ag_index % ARRAY_SIZE(sb->s_keys);

	iv_clone(&sb->s_ivs[iv_index], &out_iv_key->iv);
	key_clone(&sb->s_keys[key_index], &out_iv_key->key);
}

void voluta_iv_key_of(const struct voluta_bk_info *bki,
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

enum voluta_vtype voluta_vtype_at(const struct voluta_bk_info *uber_bki,
				  const struct voluta_vaddr *vaddr)
{
	size_t sbn;
	const struct voluta_uber_block *uber = uber_of(uber_bki);
	const struct voluta_bkref *bkref = uber_bkref_at(uber, vaddr);

	if (vaddr->len == VOLUTA_BK_SIZE) {
		/* Special case of "false" block. TODO: be more elegant */
		return bkref->b_vtype;
	}
	if (bkref_is_full(bkref)) {
		return bkref->b_vtype;
	}
	sbn = sbn_of(vaddr);
	if (bkref_test_sbn(bkref, sbn)) {
		return bkref->b_vtype;
	}
	return VOLUTA_VTYPE_NONE;
}

static int inhabit_bk(struct voluta_bk_info *uber_bki,
		      enum voluta_vtype vtype,
		      struct voluta_vaddr *out_vaddr)
{
	int err;
	struct voluta_uber_block *ub = uber_of(uber_bki);

	err = uber_require_space(ub);
	if (err) {
		return err;
	}
	err = uber_find_free(ub, vtype, out_vaddr);
	if (err) {
		return err;
	}
	uber_set(ub, out_vaddr);
	return 0;
}

static void evacuate_bk(struct voluta_bk_info *uber_bki,
			const struct voluta_vaddr *vaddr)
{
	struct voluta_uber_block *ub = uber_of(uber_bki);

	uber_unset(ub, vaddr);
}

static int inhabit_nkbs(struct voluta_bk_info *uber_bki,
			enum voluta_vtype vtype, size_t nkbs,
			struct voluta_vaddr *out_vaddr)
{
	int err;
	struct voluta_uber_block *ub = uber_of(uber_bki);

	err = uber_require_space(ub);
	if (err) {
		return err;
	}
	err = uber_find_free_nkbs(ub, vtype, nkbs, out_vaddr);
	if (err) {
		return err;
	}
	uber_setn(ub, out_vaddr);
	return 0;
}

static void evacuate_nkbs(struct voluta_bk_info *uber_bki,
			  const struct voluta_vaddr *vaddr)
{
	struct voluta_uber_block *ub = uber_of(uber_bki);

	uber_unsetn(ub, vaddr);
}

static void setup_uber_bk(struct voluta_bk_info *uber_bki)
{
	struct voluta_uber_block *ub = uber_of(uber_bki);
	const struct voluta_vaddr *vaddr = bk_vaddr_of(uber_bki);

	voluta_stamp_vbk(uber_bki);
	uber_init(ub, vaddr->ag_index);
	uber_set_iv_keys(ub);
	uber_set_fs_uuid(ub, &uber_bki->b_sbi->s_fs_uuid);
}

enum voluta_bk_flags voluta_get_bkflags_of(const struct voluta_bk_info *bki)
{
	const struct voluta_uber_block *ub = uber_of(bki->b_uber_bki);

	return uber_bkflags_of(ub, &bki->b_vaddr);
}

void voluta_clear_bkflags_of(const struct voluta_bk_info *bki,
			     enum voluta_bk_flags bkf)
{
	struct voluta_uber_block *ub;
	struct voluta_bk_info *uber_bki = bki->b_uber_bki;

	ub = uber_of(uber_bki);
	uber_unsetf(ub, &bki->b_vaddr, bkf);
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

void voluta_ag_space_init(struct voluta_ag_space *ags)
{
	memset(ags, 0, sizeof(*ags));
	list_head_init(&ags->spq);
	ags->ag_count = 0;
	ags->ag_nlive = 0;
}

void voluta_ag_space_fini(struct voluta_ag_space *ags)
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
	const bool full_bk = (vaddr->len == VOLUTA_BK_SIZE);
	struct voluta_ag_space *ags = &sbi->s_ag_space;

	if (full_bk) {
		evacuate_bk(uber_bki, vaddr);
	} else {
		evacuate_nkbs(uber_bki, vaddr);
	}
	bk_dirtify(uber_bki);
	ag_space_free(ags, vaddr->ag_index, vaddr->len);
}

static int inhabit_space(struct voluta_sb_info *sbi,
			 struct voluta_bk_info *uber_bki,
			 enum voluta_vtype vtype, size_t nkbs,
			 struct voluta_vaddr *vaddr)
{
	int err;
	const bool full_bk = (nkbs == VOLUTA_NKBS_IN_BK);
	struct voluta_ag_space *ags = &sbi->s_ag_space;

	if (full_bk) {
		err = inhabit_bk(uber_bki, vtype, vaddr);
	} else {
		err = inhabit_nkbs(uber_bki, vtype, nkbs, vaddr);
	}
	if (err) {
		return err;
	}
	bk_dirtify(uber_bki);
	ag_space_alloc(ags, vaddr->ag_index, vaddr->len);
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
	sp_st->sp_nbytes = (size_t)(blocks_free * VOLUTA_BK_SIZE);
	sp_st->sp_nmeta = 0;
	sp_st->sp_ndata = 0;
	sp_st->sp_nfiles = 0;
}

int voluta_prepare_space(struct voluta_env *env, loff_t size)
{
	int err;
	loff_t space_size;
	struct voluta_sb_info *sbi = sbi_of(env);

	err = voluta_vol_resolve_size(env->volume_path, size, &space_size);
	if (err) {
		return err;
	}
	ag_space_setup(&sbi->s_ag_space, space_size);
	derive_space_stat(sbi);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void clear_unwritten_bk(struct voluta_bk_info *bki)
{
	union voluta_bk *bk = bki->bk;

	memset(bk, 0, sizeof(*bk));
}

static bool is_unwritten(const struct voluta_bk_info *bki)
{
	bool ret = false;
	enum voluta_bk_flags bk_flags, unwritten = VOLUTA_BKF_UNWRITTEN;

	if (!is_uber_bk(bki)) {
		bk_flags = voluta_get_bkflags_of(bki);
		ret = ((bk_flags & unwritten) == unwritten);
	}
	return ret;
}

static int voluta_load_bk(struct voluta_sb_info *sbi,
			  struct voluta_bk_info *bki)
{
	int err;

	if (is_unwritten(bki)) {
		clear_unwritten_bk(bki);
	} else {
		err = voluta_bdev_load(sbi->s_bdev, bki);
		if (err) {
			return err;
		}
	}
	bki->b_staged = true;
	return 0;
}

int voluta_stage_bk_of(struct voluta_sb_info *sbi,
		       const struct voluta_vaddr *vaddr,
		       struct voluta_bk_info *uber_bki,
		       struct voluta_bk_info **out_bki)
{
	int err;
	struct voluta_vaddr bk_vaddr;

	voluta_vaddr_by_lba(&bk_vaddr, vaddr->vtype, vaddr->lba);
	err = voluta_spawn_bk(sbi, &bk_vaddr, uber_bki, out_bki);
	if (err) {
		return err;
	}
	if ((*out_bki)->b_staged) {
		return 0;
	}
	err = voluta_load_bk(sbi, *out_bki);
	if (err) {
		/* TODO: Detach, clean from cache */
		return err;
	}
	return 0;
}

int voluta_stage_uber_bk(struct voluta_sb_info *sbi,
			 const struct voluta_vaddr *uber_vaddr,
			 struct voluta_bk_info **out_uber_bki)
{
	int err;
	struct voluta_bk_info *bki;

	err = voluta_stage_bk_of(sbi, uber_vaddr, NULL, &bki);
	if (err) {
		return err;
	}
	err = voluta_verify_vbk(bki, uber_vaddr);
	if (err) {
		return err;
	}
	*out_uber_bki = bki;
	return 0;
}

int voluta_stage_uber_of(struct voluta_sb_info *sbi,
			 const struct voluta_vaddr *vaddr,
			 struct voluta_bk_info **out_uber_bki)
{
	struct voluta_vaddr uber_vaddr;

	vaddr_of_uber(&uber_vaddr, vaddr->ag_index);
	return voluta_stage_uber_bk(sbi, &uber_vaddr, out_uber_bki);
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

	err = voluta_stage_uber_bk(sbi, &uber_vaddr, &uber_bki);
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

int voluta_allocate_space(struct voluta_sb_info *sbi, enum voluta_vtype vtype,
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

int voluta_deallocate_space(struct voluta_sb_info *sbi,
			    const struct voluta_vaddr *vaddr)
{
	int err;
	struct voluta_bk_info *uber_bki;
	struct voluta_ag_space *ags = &sbi->s_ag_space;
	struct voluta_ag_info *agi = agi_by_index(ags, vaddr->ag_index);

	err = voluta_stage_uber_of(sbi, vaddr, &uber_bki);
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

	voluta_vaddr_by_lba(&vaddr, VOLUTA_VTYPE_UBER, uber_lba);
	return voluta_spawn_bk(sbi, &vaddr, NULL, out_uber_bki);
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
	return voluta_bdev_save_bk(sbi->s_bdev, off, &sbi->s_iv_key, sb_bk);
}

int voluta_load_super_block(struct voluta_sb_info *sbi)
{
	int err;
	loff_t off;
	struct voluta_super_block *sb = sbi->sb;
	const struct voluta_iv_key *iv_key = &sbi->s_iv_key;

	off = lba_to_off(VOLUTA_LBA_SUPER);
	err = voluta_bdev_load_bk(sbi->s_bdev, off, iv_key, sb_to_bk(sb));
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
	err = voluta_stage_uber_bk(sbi, &uber_vaddr, &uber_bki);
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

static int try_load_itable_root(struct voluta_env *env, size_t ag_index)
{
	int err;
	loff_t lba, uber_lba;
	struct voluta_vaddr vaddr;
	const size_t nbk_in_ag = VOLUTA_NBK_IN_AG;

	uber_lba = lba_of_uber(ag_index);
	for (size_t bn = 1; bn < nbk_in_ag; ++bn) {
		lba = lba_by_uber(uber_lba, bn);
		voluta_vaddr_by_lba(&vaddr, VOLUTA_VTYPE_ITNODE, lba);
		err = voluta_reload_itable(env, &vaddr);
		if (!err || (err != -ENOENT)) {
			return err;
		}
		promote_cache_cycle(sbi_of(env));
	}
	return -ENOENT;
}

int voluta_load_itable(struct voluta_env *env)
{
	int err;

	err = try_load_itable_root(env, 1); /* XXX */
	if (!err || (err != -ENOENT)) {
		return err;
	}
	return -ENOENT;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static union voluta_lview *lview_of(const union voluta_kbs *kbs)
{
	const union voluta_lview *lview;

	lview = container_of(kbs, union voluta_lview, kbs);
	return (union voluta_lview *)lview;
}

void voluta_resolve_lview(const struct voluta_bk_info *bki,
			  const struct voluta_vaddr *vaddr,
			  union voluta_lview **out_lview)
{
	union voluta_bk *bk = bki->bk;
	const size_t sbn = off_to_sbn(vaddr->off);

	*out_lview = lview_of(&bk->kbs[sbn]);
}

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

static int verify_bk_flags(enum voluta_bk_flags bk_flags)
{
	int err;

	switch (bk_flags) {
	case VOLUTA_BKF_NONE:
	case VOLUTA_BKF_UNWRITTEN:
		err = 0;
		break;
	default:
		err = -EFSCORRUPTED;
		break;
	}
	return err;
}

static int verify_bkref(const struct voluta_bkref *bkref)
{
	int err;

	/* TODO: check more */
	err = verify_vtype(bkref->b_vtype);
	if (err) {
		return err;
	}
	err = verify_bk_flags(bkref->b_flags);
	if (err) {
		return err;
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
