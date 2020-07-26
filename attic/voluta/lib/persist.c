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
#include <linux/limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <gcrypt.h>
#include "libvoluta.h"

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

#define REQUIRE_TYPE_SEG_SIZE(type) \
	REQUIRE_TYPE_SIZE(type, VOLUTA_SEG_SIZE)

#define REQUIRE_TYPE_BK_SIZE(type) \
	REQUIRE_TYPE_SIZE(type, VOLUTA_BK_SIZE)

#define REQUIRE_TYPE_KB_SIZE(type) \
	REQUIRE_TYPE_SIZE(type, VOLUTA_KB_SIZE)

#define REQUIRE_MEMBER_SIZE(type, f, size) \
	REQUIRE_EQ(MEMBER_SIZE(type, f), size)

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
	REQUIRE_TYPE_KB_SIZE(struct voluta_vboot_record);
	REQUIRE_TYPE_KB_SIZE(union voluta_kilo_block);
	REQUIRE_TYPE_KB_SIZE(struct voluta_inode);
	REQUIRE_TYPE_KB_SIZE(struct voluta_lnk_value);
	REQUIRE_TYPE_BK_SIZE(struct voluta_super_block);
	REQUIRE_TYPE_BK_SIZE(struct voluta_uspace_map);
	REQUIRE_TYPE_BK_SIZE(struct voluta_agroup_map);
	REQUIRE_TYPE_BK_SIZE(struct voluta_itable_tnode);
	REQUIRE_TYPE_BK_SIZE(struct voluta_xattr_node);
	REQUIRE_TYPE_BK_SIZE(struct voluta_dir_htree_node);
	REQUIRE_TYPE_BK_SIZE(struct voluta_radix_tnode);
	REQUIRE_TYPE_BK_SIZE(struct voluta_data_block);
	REQUIRE_TYPE_BK_SIZE(union voluta_block);
	REQUIRE_TYPE_SEG_SIZE(union voluta_segment_u);
	REQUIRE_TYPE_SEG_SIZE(struct voluta_segment);
	REQUIRE_TYPE_SIZE(struct voluta_vboot_pub, 512);
	REQUIRE_TYPE_SIZE(struct voluta_vboot_pri, 512);
	REQUIRE_TYPE_SIZE(struct voluta_header, VOLUTA_HEADER_SIZE);
	REQUIRE_TYPE_SIZE(struct voluta_uuid, VOLUTA_UUID_SIZE);
	REQUIRE_TYPE_SIZE(struct voluta_name, VOLUTA_NAME_MAX + 1);
	REQUIRE_TYPE_SIZE(struct voluta_iv, VOLUTA_IV_SIZE);
	REQUIRE_TYPE_SIZE(struct voluta_key, VOLUTA_KEY_SIZE);
	REQUIRE_TYPE_SIZE(struct voluta_ag_record, 12);
	REQUIRE_TYPE_SIZE(struct voluta_bk_record, 12);
	REQUIRE_TYPE_SIZE(struct voluta_itable_entry, 16);
	REQUIRE_TYPE_SIZE(struct voluta_dir_entry, 16);
	REQUIRE_TYPE_SIZE(struct voluta_xattr_entry, 8);
	REQUIRE_TYPE_SIZE(struct voluta_reg_ispec, 64);
	REQUIRE_TYPE_SIZE(struct voluta_dir_ispec, 64);
	REQUIRE_TYPE_SIZE(struct voluta_inode_times, 64);
	REQUIRE_TYPE_SIZE(struct voluta_inode_xattr, 320);
	REQUIRE_TYPE_SIZE(union voluta_inode_specific, 512);
	REQUIRE_TYPE_SIZE(struct voluta_inode, VOLUTA_INODE_SIZE);
	REQUIRE_TYPE_SIZE(struct voluta_lnk_value, VOLUTA_SYMLNK_VAL_SIZE);
	REQUIRE_TYPE_SIZE(struct voluta_xattr_node, VOLUTA_XATTR_NODE_SIZE);
	REQUIRE_TYPE_SIZE(struct voluta_radix_tnode, VOLUTA_FILE_RTNODE_SIZE);
	REQUIRE_TYPE_SIZE(struct voluta_itable_tnode, VOLUTA_ITNODE_SIZE);
	REQUIRE_TYPE_SIZE(struct voluta_dir_htree_node,
			  VOLUTA_DIR_HTNODE_SIZE);
	REQUIRE_TYPE_SIZE(struct voluta_mntmsg, 2048);
}

static void static_assert_persistent_types_members(void)
{
	REQUIRE_NBITS(struct voluta_header, h_vtype, 16);
	REQUIRE_NBITS(struct voluta_bk_record, b_usemask, VOLUTA_NKB_IN_BK);
	REQUIRE_MEMBER_SIZE(union voluta_segment_u, seg, VOLUTA_SEG_SIZE);
	REQUIRE_MEMBER_SIZE(struct voluta_itable_tnode, it_child, 2048);
	REQUIRE_MEMBER_SIZE(struct voluta_super_block,
			    s_uuid, VOLUTA_UUID_SIZE);
	REQUIRE_NELEMS(struct voluta_radix_tnode,
		       r_child_lo, VOLUTA_FILE_MAP_NCHILD);
	REQUIRE_NELEMS(struct voluta_dir_htree_node, de,
		       VOLUTA_DIR_HTNODE_NENTS);
	REQUIRE_NELEMS(struct voluta_dir_htree_node, dh_child,
		       VOLUTA_DIR_HTNODE_NCHILDS);
	REQUIRE_NBITS(struct voluta_seg_info, seg_mask, VOLUTA_NKB_IN_SEG);
}

static void static_assert_persistent_types_alignment(void)
{
	REQUIRE_AOFFSET(struct voluta_super_block, s_hdr, 0);
	REQUIRE_AOFFSET64(struct voluta_super_block, s_uuid, 32);
	REQUIRE_AOFFSET64(struct voluta_super_block, s_fs_name, 64);
	REQUIRE_AOFFSET64(struct voluta_super_block, s_birth_time, 512);
	REQUIRE_AOFFSET64(struct voluta_super_block, s_ag_count, 520);
	REQUIRE_AOFFSET64(struct voluta_super_block, s_itable_root, 2048);
	REQUIRE_AOFFSET64(struct voluta_super_block, s_ivs, 4096);
	REQUIRE_AOFFSET64(struct voluta_super_block, s_keys, 5536);
	REQUIRE_AOFFSET(struct voluta_uspace_map, u_hdr, 0);
	REQUIRE_AOFFSET(struct voluta_uspace_map, u_keys, 64);
	REQUIRE_AOFFSET(struct voluta_uspace_map, u_ivs, 1408);
	REQUIRE_AOFFSET(struct voluta_uspace_map, u_agr, 2048);
	REQUIRE_AOFFSET(struct voluta_agroup_map, ag_hdr, 0);
	REQUIRE_AOFFSET64(struct voluta_agroup_map, ag_keys, 64);
	REQUIRE_AOFFSET64(struct voluta_agroup_map, ag_ivs, 1408);
	REQUIRE_AOFFSET64(struct voluta_agroup_map, ag_bkr, 2048);
	REQUIRE_AOFFSET64(struct voluta_itable_tnode, ite, 64);
	REQUIRE_AOFFSET64(struct voluta_itable_tnode, it_child, 6144);
	REQUIRE_AOFFSET(struct voluta_inode, i_hdr, 0);
	REQUIRE_AOFFSET(struct voluta_inode, i_uuid, 16);
	REQUIRE_AOFFSET(struct voluta_inode, i_ino, 32);
	REQUIRE_AOFFSET(struct voluta_inode, i_parent, 40);
	REQUIRE_AOFFSET(struct voluta_inode, i_uid, 48);
	REQUIRE_AOFFSET(struct voluta_inode, i_gid, 52);
	REQUIRE_AOFFSET(struct voluta_inode, i_mode, 56);
	REQUIRE_AOFFSET(struct voluta_inode, i_flags, 60);
	REQUIRE_AOFFSET(struct voluta_inode, i_size, 64);
	REQUIRE_AOFFSET(struct voluta_inode, i_blocks, 72);
	REQUIRE_AOFFSET(struct voluta_inode, i_nlink, 80);
	REQUIRE_AOFFSET(struct voluta_inode, i_attributes, 88);
	REQUIRE_AOFFSET64(struct voluta_inode, i_t, 128);
	REQUIRE_AOFFSET64(struct voluta_inode, i_x, 192);
	REQUIRE_AOFFSET64(struct voluta_inode, i_s, 512);
	REQUIRE_AOFFSET(union voluta_kilo_block, inode, 0);
	REQUIRE_AOFFSET(struct voluta_dir_entry, de_ino, 0);
	REQUIRE_AOFFSET(struct voluta_dir_entry, de_nents, 8);
	REQUIRE_XOFFSET(struct voluta_dir_entry, de_nprev, 10);
	REQUIRE_XOFFSET(struct voluta_dir_entry, de_name_len, 12);
	REQUIRE_XOFFSET(struct voluta_dir_entry, de_dt, 14);
	REQUIRE_AOFFSET(struct voluta_dir_htree_node, dh_hdr, 0);
	REQUIRE_AOFFSET64(struct voluta_dir_htree_node, de, 64);
	REQUIRE_AOFFSET(struct voluta_radix_tnode, r_hdr, 0);
	REQUIRE_AOFFSET64(struct voluta_radix_tnode, r_zeros, 64);
	REQUIRE_AOFFSET64(struct voluta_radix_tnode, r_child_hi, 2048);
	REQUIRE_AOFFSET64(struct voluta_radix_tnode, r_child_lo, 4096);
	REQUIRE_AOFFSET(struct voluta_inode_xattr, xa_nents, 0);
	REQUIRE_AOFFSET(struct voluta_inode_xattr, xa_off, 8);
	REQUIRE_AOFFSET(struct voluta_xattr_node, xa_hdr, 0);
	REQUIRE_AOFFSET(struct voluta_xattr_node, xe, 64);
	REQUIRE_AOFFSET64(struct voluta_lnk_value, lv_value, 64);
}

static void static_assert_defs_consistency(void)
{
	REQUIRE_EQ(CHAR_BIT, 8);
	REQUIRE_ZERO(VOLUTA_SEG_SIZE % VOLUTA_KB_SIZE);
	REQUIRE_EQ(8 * VOLUTA_BK_SIZE, VOLUTA_SEG_SIZE);
	REQUIRE_LT(VOLUTA_DIR_HTREE_DEPTH_MAX, VOLUTA_HASH256_LEN);
	REQUIRE_LT(VOLUTA_DIR_HTREE_INDEX_MAX, INT32_MAX);
	REQUIRE_LT(VOLUTA_DIR_HTREE_INDEX_MAX, VOLUTA_DIR_HTREE_INDEX_NULL);
	REQUIRE_GT(VOLUTA_DIR_ENTRIES_MAX, VOLUTA_LINK_MAX);
	REQUIRE_LT(VOLUTA_XATTR_VALUE_MAX, VOLUTA_XATTR_NODE_SIZE);
	REQUIRE_GT(VOLUTA_FILE_SIZE_MAX, 4 * VOLUTA_TERA);
	REQUIRE_EQ(VOLUTA_AG_SIZE, 4 * VOLUTA_MEGA);
	REQUIRE_EQ(VOLUTA_USP_SIZE, 2 * VOLUTA_GIGA);
	REQUIRE_EQ(VOLUTA_VOLUME_SIZE_MAX, 8 * VOLUTA_TERA);
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
	static_assert_persistent_types_members();
	static_assert_persistent_types_alignment();
	static_assert_defs_consistency();
	static_assert_external_constants();
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t voluta_persistent_size(enum voluta_vtype vtype)
{
	size_t psz;

	switch (vtype) {
	case VOLUTA_VTYPE_SUPER:
		psz = sizeof(struct voluta_super_block);
		break;
	case VOLUTA_VTYPE_USMAP:
		psz = sizeof(struct voluta_uspace_map);
		break;
	case VOLUTA_VTYPE_AGMAP:
		psz = sizeof(struct voluta_agroup_map);
		break;
	case VOLUTA_VTYPE_ITNODE:
		psz = sizeof(struct voluta_itable_tnode);
		break;
	case VOLUTA_VTYPE_INODE:
		psz = sizeof(struct voluta_inode);
		break;
	case VOLUTA_VTYPE_XANODE:
		psz = sizeof(struct voluta_xattr_node);
		break;
	case VOLUTA_VTYPE_HTNODE:
		psz = sizeof(struct voluta_dir_htree_node);
		break;
	case VOLUTA_VTYPE_RTNODE:
		psz = sizeof(struct voluta_radix_tnode);
		break;
	case VOLUTA_VTYPE_SYMVAL:
		psz = sizeof(struct voluta_lnk_value);
		break;
	case VOLUTA_VTYPE_DATA:
		psz = sizeof(struct voluta_data_block);
		break;
	case VOLUTA_VTYPE_NONE:
	default:
		psz = 0;
		break;
	}
	return psz;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static uint32_t hdr_magic(const struct voluta_header *hdr)
{
	return le32_to_cpu(hdr->h_magic);
}

static void hdr_set_magic(struct voluta_header *hdr, uint32_t magic)
{
	hdr->h_magic = cpu_to_le32(magic);
}

static size_t hdr_size(const struct voluta_header *hdr)
{
	return le32_to_cpu(hdr->h_size);
}

static size_t hdr_payload_size(const struct voluta_header *hdr)
{
	return hdr_size(hdr) - sizeof(*hdr);
}

static void hdr_set_size(struct voluta_header *hdr, size_t size)
{
	hdr->h_size = cpu_to_le32((uint32_t)size);
}

static enum voluta_vtype hdr_vtype(const struct voluta_header *hdr)
{
	return le16_to_cpu(hdr->h_vtype);
}

static void hdr_set_vtype(struct voluta_header *hdr, enum voluta_vtype vtype)
{
	hdr->h_vtype = cpu_to_le16((uint16_t)vtype);
}

static uint32_t hdr_csum(const struct voluta_header *hdr)
{
	return le32_to_cpu(hdr->h_csum);
}

static void hdr_set_csum(struct voluta_header *hdr, uint32_t csum)
{
	hdr->h_csum = cpu_to_le32(csum);
}

static const void *hdr_payload(const struct voluta_header *hdr)
{
	return hdr + 1;
}

static struct voluta_header *hdr_of(const struct voluta_view *view)
{
	const struct voluta_header *hdr = &view->u.hdr;

	return unconst(hdr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_header *v_hdr_of(const struct voluta_vnode_info *vi)
{
	return hdr_of(vi->view);
}

static const struct voluta_data_block *
v_db_of(const struct voluta_vnode_info *vi)
{
	return vi->u.db;
}

static uint32_t calc_meta_chekcsum(const struct voluta_header *hdr,
				   const struct voluta_crypto *crypto)
{
	uint32_t csum = 0;
	const void *payload = hdr_payload(hdr);
	const size_t pl_size = hdr_payload_size(hdr);

	voluta_assert_le(pl_size, VOLUTA_SEG_SIZE - VOLUTA_HEADER_SIZE);

	voluta_crc32_of(crypto, payload, pl_size, &csum);
	return csum;
}

static uint32_t calc_data_checksum(const struct voluta_data_block *db,
				   const struct voluta_crypto *crypto)
{
	uint32_t csum = 0;

	voluta_crc32_of(crypto, db->dat, sizeof(db->dat), &csum);
	return csum;
}

uint32_t voluta_calc_chekcsum(const struct voluta_vnode_info *vi)
{
	uint32_t csum;
	const struct voluta_crypto *crypto = v_crypto_of(vi);

	if (vaddr_isdata(v_vaddr_of(vi))) {
		csum = calc_data_checksum(v_db_of(vi), crypto);
	} else {
		csum = calc_meta_chekcsum(v_hdr_of(vi), crypto);
	}
	return csum;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int verify_hdr(const struct voluta_view *view, enum voluta_vtype vtype)
{
	const struct voluta_header *hdr = hdr_of(view);
	const size_t hsz = hdr_size(hdr);
	const size_t psz = voluta_persistent_size(vtype);

	if (vtype_isdata(vtype)) {
		return 0;
	}
	if (hdr_magic(hdr) != VOLUTA_MAGIC) {
		return -EFSCORRUPTED;
	}
	if (hdr_vtype(hdr) != vtype) {
		return -EFSCORRUPTED;
	}
	if (hsz != psz) {
		return -EFSCORRUPTED;
	}

	return 0;
}

static int verify_checksum(const struct voluta_view *view,
			   const struct voluta_crypto *crypto)
{
	uint32_t csum;
	const struct voluta_header *hdr = hdr_of(view);

	csum = calc_meta_chekcsum(hdr, crypto);
	return (csum == hdr_csum(hdr)) ? 0 : -EFSCORRUPTED;
}

int voluta_verify_ino(ino_t ino)
{
	return !ino_isnull(ino) ? 0 : -EFSCORRUPTED;
}

int voluta_verify_off(loff_t off)
{
	return (off_isnull(off) || (off >= 0)) ? 0 : -EFSCORRUPTED;
}

static int verify_sub(const struct voluta_view *view, enum voluta_vtype vtype)
{
	int err;

	switch (vtype) {
	case VOLUTA_VTYPE_SUPER:
		err = voluta_verify_super_block(&view->u.sb);
		break;
	case VOLUTA_VTYPE_USMAP:
		err = voluta_verify_uspace_map(&view->u.usm);
		break;
	case VOLUTA_VTYPE_AGMAP:
		err = voluta_verify_agroup_map(&view->u.agm);
		break;
	case VOLUTA_VTYPE_ITNODE:
		err = voluta_verify_itnode(&view->u.itn, false);
		break;
	case VOLUTA_VTYPE_INODE:
		err = voluta_verify_inode(&view->u.inode);
		break;
	case VOLUTA_VTYPE_XANODE:
		err = voluta_verify_xattr_node(&view->u.xan);
		break;
	case VOLUTA_VTYPE_HTNODE:
		err = voluta_verify_dir_htree_node(&view->u.htn);
		break;
	case VOLUTA_VTYPE_RTNODE:
		err = voluta_verify_radix_tnode(&view->u.rtn);
		break;
	case VOLUTA_VTYPE_SYMVAL:
		err = voluta_verify_lnk_value(&view->u.lnv);
		break;
	case VOLUTA_VTYPE_DATA:
		err = 0;
		break;
	case VOLUTA_VTYPE_NONE:
	default:
		err = -EFSCORRUPTED;
		break;
	}
	return err;
}

static int verify_view(const struct voluta_view *view, enum voluta_vtype vtype,
		       const struct voluta_crypto *crypto)
{
	int err;

	if (vtype_isdata(vtype)) {
		return 0;
	}
	err = verify_hdr(view, vtype);
	if (err) {
		return err;
	}
	err = verify_checksum(view, crypto);
	if (err) {
		return err;
	}
	err = verify_sub(view, vtype);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_verify_meta(const struct voluta_vnode_info *vi)
{
	const struct voluta_vaddr *vaddr = v_vaddr_of(vi);
	const struct voluta_crypto *crypto = v_crypto_of(vi);

	return verify_view(vi->view, vaddr->vtype, crypto);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void stamp_hdr(struct voluta_header *hdr,
		      enum voluta_vtype vtype, size_t size)
{
	hdr_set_magic(hdr, VOLUTA_MAGIC);
	hdr_set_size(hdr, size);
	hdr_set_vtype(hdr, vtype);
	hdr_set_csum(hdr, 0);
	hdr->h_flags = 0;
}

void voluta_stamp_view(struct voluta_view *view, enum voluta_vtype vtype)
{
	struct voluta_header *hdr = hdr_of(view);
	const size_t size = voluta_persistent_size(vtype);

	stamp_hdr(hdr, vtype, size);
}

void voluta_seal_meta(const struct voluta_vnode_info *vi)
{
	uint32_t csum;
	struct voluta_header *hdr = hdr_of(vi->view);

	csum = voluta_calc_chekcsum(vi);
	hdr_set_csum(hdr, csum);
}

