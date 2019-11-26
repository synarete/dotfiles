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
#include <linux/limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <gcrypt.h>
#include "voluta-lib.h"

#define BITS_SIZE(a)    (CHAR_BIT * sizeof(a))

#define MEMBER_SIZE(type, member) \
	sizeof(((const type *)NULL)->member)

#define MEMBER_NELEMS(type, member) \
	VOLUTA_ARRAY_SIZE(((const type *)NULL)->member)

#define MEMBER_NBITS(type, member) \
	BITS_SIZE(((const type *)NULL)->member)

#define REQUIRE_EQ(a, b) \
	VOLUTA_STATICASSERT_EQ(a, b)

#define REQUIRE_LT(a, b) \
	VOLUTA_STATICASSERT_LT(a, b)

#define REQUIRE_GT(a, b) \
	VOLUTA_STATICASSERT_GT(a, b)

#define REQUIRE_GE(a, b) \
	VOLUTA_STATICASSERT_GE(a, b)

#define REQUIRE_ZERO(a) \
	REQUIRE_EQ(a, 0)

#define REQUIRE_TYPE_SIZE(type, size) \
	REQUIRE_EQ(sizeof(type), size)

#define REQUIRE_TYPE_BK_SIZE(type) \
	REQUIRE_TYPE_SIZE(type, VOLUTA_BK_SIZE)

#define REQUIRE_MEMBER_SIZE(type, f, size) \
	REQUIRE_EQ(MEMBER_SIZE(type, f), size)

#define REQUIRE_MEMBER_BK_SIZE(type, f) \
	REQUIRE_EQ(MEMBER_SIZE(type, f), VOLUTA_BK_SIZE)

#define REQUIRE_NELEMS(type, f, nelems) \
	REQUIRE_EQ(MEMBER_NELEMS(type, f), nelems)

#define REQUIRE_NBITS(type, f, nbits) \
	REQUIRE_EQ(MEMBER_NBITS(type, f), nbits)

#define ISALIGNED32(off) \
	(((off) % 4) == 0)

#define ISALIGNED64(off) \
	(((off) % 8) == 0)

#define ISOFFSET(type, member, off) \
	(offsetof(type, member) == (off))

#define REQUIRE_XOFFSET(type, member, off) \
	VOLUTA_STATICASSERT(ISOFFSET(type, member, off))

#define REQUIRE_AOFFSET(type, member, off) \
	VOLUTA_STATICASSERT(ISOFFSET(type, member, off) && ISALIGNED32(off))

#define REQUIRE_AOFFSET64(type, member, off) \
	VOLUTA_STATICASSERT(ISOFFSET(type, member, off) && ISALIGNED64(off))

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void static_assert_fundamental_types_size(void)
{
	REQUIRE_TYPE_SIZE(uint8_t, 1);
	REQUIRE_TYPE_SIZE(uint16_t, 2);
	REQUIRE_TYPE_SIZE(uint32_t, 4);
	REQUIRE_TYPE_SIZE(uint64_t, 8);
	REQUIRE_TYPE_SIZE(int8_t, 1);
	REQUIRE_TYPE_SIZE(int16_t, 2);
	REQUIRE_TYPE_SIZE(int32_t, 4);
	REQUIRE_TYPE_SIZE(int64_t, 8);
	REQUIRE_TYPE_SIZE(size_t, 8);
	REQUIRE_TYPE_SIZE(loff_t, 8);
	REQUIRE_TYPE_SIZE(ino_t, 8);
}

static void static_assert_persistent_types_size(void)
{
	REQUIRE_TYPE_SIZE(struct voluta_int56, 7);
	REQUIRE_TYPE_SIZE(struct voluta_header, VOLUTA_HEADER_SIZE);
	REQUIRE_TYPE_SIZE(struct voluta_uuid, VOLUTA_UUID_SIZE);
	REQUIRE_TYPE_SIZE(struct voluta_name, VOLUTA_NAME_MAX + 1);
	REQUIRE_TYPE_SIZE(struct voluta_iv, VOLUTA_IV_SIZE);
	REQUIRE_TYPE_SIZE(struct voluta_key, VOLUTA_KEY_SIZE);
	REQUIRE_TYPE_SIZE(struct voluta_itable_entry, 20);
	REQUIRE_TYPE_SIZE(struct voluta_bkref, 28);
	REQUIRE_TYPE_SIZE(struct voluta_dir_entry, 16);
	REQUIRE_TYPE_SIZE(struct voluta_xattr_entry, 8);
	REQUIRE_TYPE_SIZE(struct voluta_tree_ref, 32);
	REQUIRE_TYPE_SIZE(union voluta_inode_specific, 256);
	REQUIRE_TYPE_SIZE(struct voluta_inode, VOLUTA_INODE_SIZE);
	REQUIRE_TYPE_SIZE(struct voluta_kbs_inode, VOLUTA_KBS_SIZE);
	REQUIRE_TYPE_SIZE(union voluta_kbs, VOLUTA_KBS_SIZE);
	REQUIRE_TYPE_SIZE(struct voluta_dir_tlink, 192);
	REQUIRE_TYPE_SIZE(struct voluta_dir_inode, VOLUTA_DIRINODE_SIZE);
	REQUIRE_TYPE_SIZE(struct voluta_reg_inode, VOLUTA_REGINODE_SIZE);
	REQUIRE_TYPE_SIZE(struct voluta_lnk_symval, VOLUTA_LNKSYMVAL_SIZE);
	REQUIRE_TYPE_SIZE(struct voluta_xattr_node, VOLUTA_XATTRNODE_SIZE);
	REQUIRE_TYPE_SIZE(struct voluta_dir_tnode, VOLUTA_DIRTNODE_SIZE);

	REQUIRE_MEMBER_SIZE(struct voluta_super_block,
			    s_uuid, VOLUTA_UUID_SIZE);
	REQUIRE_MEMBER_SIZE(union voluta_bk, bk, VOLUTA_BK_SIZE);
	REQUIRE_MEMBER_SIZE(struct voluta_itable_tnode, it_child_off, 1024);

	REQUIRE_TYPE_BK_SIZE(struct voluta_super_block);
	REQUIRE_TYPE_BK_SIZE(struct voluta_uber_block);
	REQUIRE_TYPE_BK_SIZE(struct voluta_itable_tnode);
	REQUIRE_TYPE_BK_SIZE(union voluta_vnode_block);
	REQUIRE_TYPE_BK_SIZE(struct voluta_radix_tnode);
	REQUIRE_TYPE_BK_SIZE(union voluta_vnode_block);
	REQUIRE_TYPE_BK_SIZE(union voluta_bk);

	REQUIRE_MEMBER_BK_SIZE(union voluta_vnode_block, kbs);
	REQUIRE_MEMBER_BK_SIZE(union voluta_vnode_block, inode);
	REQUIRE_MEMBER_BK_SIZE(union voluta_vnode_block, dir_inode);
}

static void static_assert_persistent_types_alignment(void)
{
	REQUIRE_AOFFSET(struct voluta_super_block, s_hdr, 0);
	REQUIRE_AOFFSET(struct voluta_super_block, s_keys, 1024 * 4);
	REQUIRE_AOFFSET(struct voluta_super_block, s_ivs, 1024 * 8);
	REQUIRE_AOFFSET(struct voluta_uber_block, u_hdr, 0);
	REQUIRE_AOFFSET(struct voluta_uber_block, u_fs_uuid, 16);
	REQUIRE_AOFFSET(struct voluta_uber_block, u_bkmap, 2048);
	REQUIRE_AOFFSET(struct voluta_inode, i_hdr, 0);
	REQUIRE_AOFFSET(struct voluta_inode, i_uuid, 16);
	REQUIRE_AOFFSET(struct voluta_inode, i_ino, 32);
	REQUIRE_AOFFSET(struct voluta_inode, i_parent_ino, 40);
	REQUIRE_AOFFSET(struct voluta_inode, i_uid, 48);
	REQUIRE_AOFFSET(struct voluta_inode, i_gid, 52);
	REQUIRE_AOFFSET(struct voluta_inode, i_mode, 56);
	REQUIRE_AOFFSET(struct voluta_inode, i_flags, 60);
	REQUIRE_AOFFSET(struct voluta_inode, i_size, 64);
	REQUIRE_AOFFSET(struct voluta_inode, i_blocks, 72);
	REQUIRE_AOFFSET(struct voluta_inode, i_nlink, 80);
	REQUIRE_AOFFSET(struct voluta_inode, i_attributes, 88);
	REQUIRE_AOFFSET(struct voluta_inode, i_s, 128);
	REQUIRE_AOFFSET(struct voluta_inode, i_times, 384);
	REQUIRE_AOFFSET(struct voluta_inode, i_nxattr, 448);
	REQUIRE_AOFFSET(struct voluta_inode, i_xattr_off, 456);
	REQUIRE_AOFFSET(struct voluta_inode, i_xe, 512);
	REQUIRE_AOFFSET(struct voluta_kbs_inode, i, 0);
	REQUIRE_AOFFSET(struct voluta_dir_inode, d_i, 0);
	REQUIRE_AOFFSET64(struct voluta_dir_inode, d_h, VOLUTA_KBS_SIZE);
	REQUIRE_AOFFSET64(struct voluta_dir_head, de, 256);
	REQUIRE_AOFFSET(struct voluta_reg_inode, r_i, 0);
	REQUIRE_AOFFSET(struct voluta_dir_entry, de_ino, 0);
	REQUIRE_AOFFSET(struct voluta_dir_entry, de_nents, 8);
	REQUIRE_AOFFSET(struct voluta_dir_entry, de_nlen, 12);
	REQUIRE_XOFFSET(struct voluta_dir_entry, de_dtype, 14);
	REQUIRE_AOFFSET(struct voluta_dir_tnode, dt_hdr, 0);
	REQUIRE_AOFFSET(struct voluta_dir_tnode, dt_tlink, 64);
	REQUIRE_AOFFSET(struct voluta_radix_tnode, r_hdr, 0);
	REQUIRE_AOFFSET(struct voluta_radix_tnode, r_child_off, 2048);
	REQUIRE_AOFFSET(struct voluta_xattr_node, xa_hdr, 0);
}

static void static_assert_defs_consistency(void)
{
	REQUIRE_EQ(CHAR_BIT, 8);
	REQUIRE_ZERO(VOLUTA_BK_SIZE % VOLUTA_KBS_SIZE);
	REQUIRE_LT(VOLUTA_DIRENT_MAX, VOLUTA_DIROFF_MAX);
	REQUIRE_LT(VOLUTA_DIRDEPTH_MAX, VOLUTA_HASH256_LEN);
	REQUIRE_GT(VOLUTA_DIRENT_MAX, VOLUTA_LINK_MAX);

	REQUIRE_NBITS(struct voluta_bkref, b_smask, VOLUTA_NKBS_IN_BK);
	REQUIRE_NELEMS(struct voluta_radix_tnode, r_child_off,
		       VOLUTA_FILEMAP_NSLOTS);
	REQUIRE_NELEMS(struct voluta_bkmap, bkref,
		       (VOLUTA_AG_SIZE / VOLUTA_BK_SIZE) - 1);
	REQUIRE_NELEMS(struct voluta_dir_head, de, VOLUTA_DIRHEAD_NENTS);
	REQUIRE_NELEMS(struct voluta_dir_tnode, de, VOLUTA_DIRNODE_NENTS);
	REQUIRE_NELEMS(struct voluta_dir_tlink, dl_child_off,
		       VOLUTA_DIRNODE_NCHILDS);
}

static void static_assert_external_constants(void)
{
	REQUIRE_EQ(VOLUTA_NAME_MAX, NAME_MAX);
	REQUIRE_EQ(VOLUTA_PATH_MAX, PATH_MAX);
	REQUIRE_GE(VOLUTA_LINK_MAX, LINK_MAX);
	REQUIRE_EQ(VOLUTA_CIPHER_AES256, GCRY_CIPHER_AES256);
	REQUIRE_EQ(VOLUTA_CIPHER_MODE_CBC, GCRY_CIPHER_MODE_CBC);
	REQUIRE_EQ(VOLUTA_CIPHER_MODE_GCM, GCRY_CIPHER_MODE_GCM);
}

void voluta_verify_persistent_format(void)
{
	static_assert_fundamental_types_size();
	static_assert_persistent_types_size();
	static_assert_persistent_types_alignment();
	static_assert_defs_consistency();
	static_assert_external_constants();
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int psize_by_itype(mode_t mode, size_t *out_size)
{
	int err = 0;

	if (S_ISDIR(mode)) {
		*out_size = sizeof(struct voluta_dir_inode);
	} else if (S_ISREG(mode)) {
		*out_size = sizeof(struct voluta_reg_inode);
	} else if (S_ISLNK(mode)) {
		*out_size = sizeof(struct voluta_kbs_inode);
	} else if (S_ISFIFO(mode) || S_ISSOCK(mode)) {
		*out_size = sizeof(struct voluta_kbs_inode);
	} else {
		err = -ENOTSUP;
	}
	return err;
}

static int psize_of_type(size_t type_size, size_t *out_size)
{
	*out_size = type_size;
	return 0;
}

#define psize_of(t_, out_)  psize_of_type(sizeof(t_), out_)


int voluta_psize_of(enum voluta_vtype vtype, mode_t mode, size_t *out_size)
{
	switch (vtype) {
	case VOLUTA_VTYPE_DATA:
		return psize_of(union voluta_bk, out_size);
	case VOLUTA_VTYPE_SUPER:
		return psize_of(struct voluta_super_block, out_size);
	case VOLUTA_VTYPE_UBER:
		return psize_of(struct voluta_uber_block, out_size);
	case VOLUTA_VTYPE_ITNODE:
		return psize_of(struct voluta_itable_tnode, out_size);
	case VOLUTA_VTYPE_INODE:
		return psize_by_itype(mode, out_size);
	case VOLUTA_VTYPE_XANODE:
		return psize_of(struct voluta_xattr_node, out_size);
	case VOLUTA_VTYPE_DTNODE:
		return psize_of(struct voluta_dir_tnode, out_size);
	case VOLUTA_VTYPE_RTNODE:
		return psize_of(struct voluta_radix_tnode, out_size);
	case VOLUTA_VTYPE_SYMVAL:
		return psize_of(struct voluta_lnk_symval, out_size);
	case VOLUTA_VTYPE_NONE:
		return -EINVAL;
	default:
		break;
	}
	return -ENOTSUP;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int verify_hdr(const struct voluta_header *hdr,
		      enum voluta_vtype vtype)
{
	/* XXX remove me */
	voluta_assert_eq(hdr->magic, VOLUTA_MAGIC);
	voluta_assert_eq(hdr->vtype, vtype);

	if (hdr->magic != VOLUTA_MAGIC) {
		return -EFSCORRUPTED;
	}
	if (hdr->vtype != vtype) {
		return -EFSCORRUPTED;
	}
	if (!hdr->size || (hdr->size % VOLUTA_KBS_SIZE)) {
		return -EFSCORRUPTED;
	}
	return 0;
}

int voluta_verify_ino(ino_t ino)
{
	const ino_t ino_null = VOLUTA_INO_NULL;

	voluta_assert_ne(ino, ino_null);
	voluta_assert_lt(ino, ULONG_MAX >> 8); /* XXX */
	return (ino != ino_null) ? 0 : -EFSCORRUPTED;
}

int voluta_verify_off(loff_t off)
{
	return (off_isnull(off) || (off >= 0)) ? 0 : -EFSCORRUPTED;
}

int voluta_verify_count(size_t count, size_t expected)
{
	return (count == expected) ? 0 : -EFSCORRUPTED;
}

int voluta_verify_vbk(const struct voluta_bk_info *bki,
		      const struct voluta_vaddr *vaddr)
{
	int err;
	union voluta_lview *lview;
	const enum voluta_vtype vtype = vaddr->vtype;

	if (vtype == VOLUTA_VTYPE_DATA) {
		return 0;
	}
	voluta_resolve_lview(bki, vaddr, &lview);
	err = verify_hdr(&lview->hdr, vtype);
	if (err) {
		return err;
	}
	switch (vaddr->vtype) {
	case VOLUTA_VTYPE_SUPER:
		return voluta_verify_super_block(&lview->bk.sb);
	case VOLUTA_VTYPE_UBER:
		return voluta_verify_uber_block(&lview->bk.ub);
	case VOLUTA_VTYPE_ITNODE:
		return voluta_verify_itnode(&lview->itable_tnode);
	case VOLUTA_VTYPE_INODE:
		return voluta_verify_inode(&lview->inode);
	case VOLUTA_VTYPE_XANODE:
		return voluta_verify_xattr_node(&lview->xattr_node);
	case VOLUTA_VTYPE_DTNODE:
		return voluta_verify_dir_tnode(&lview->dir_tnode);
	case VOLUTA_VTYPE_RTNODE:
		return voluta_verify_radix_tnode(&lview->radix_tnode);
	case VOLUTA_VTYPE_SYMVAL:
		return voluta_verify_symval(&lview->lnk_symval);
	case VOLUTA_VTYPE_DATA:
		return 0;
	case VOLUTA_VTYPE_NONE:
	default:
		break;
	}
	return -EFSCORRUPTED;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void stamp_hdr(struct voluta_header *hdr, enum voluta_vtype vtype,
		      size_t size)
{
	hdr->magic = VOLUTA_MAGIC;
	hdr->vtype = (uint16_t)vtype;
	hdr->size = (uint16_t)size;
	hdr->crc = 0; /* TODO: FIXME */
}

void voluta_stamp_meta(struct voluta_header *hdr,
		       enum voluta_vtype vtype, size_t size)
{
	memset(hdr, 0, size);
	stamp_hdr(hdr, vtype, size);
}

void voluta_stamp_vbk(struct voluta_bk_info *bki)
{
	union voluta_bk *bk = bki->bk;
	struct voluta_header *hdr = &bk->hdr;

	voluta_stamp_meta(hdr, bki->b_vaddr.vtype, sizeof(*bk));
}

void voluta_stamp_vnode(struct voluta_vnode_info *vi)
{
	union voluta_lview *lview = vi->lview;
	const size_t phy_len = vi->vaddr.len;
	const enum voluta_vtype vtype = vi->vaddr.vtype;

	voluta_assert_ne(vtype, VOLUTA_VTYPE_NONE);
	voluta_assert_ne(vtype, VOLUTA_VTYPE_DATA);

	memset(lview, 0, phy_len);
	stamp_hdr(&lview->hdr, vtype, phy_len);
}

