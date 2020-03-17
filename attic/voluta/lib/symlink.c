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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "voluta-lib.h"


struct voluta_symval_info {
	size_t head_len;
	size_t nparts;
	size_t part_len[VOLUTA_SYMLNK_NPARTS];
};

static int setup_symval_info(struct voluta_symval_info *svi, size_t len)
{
	size_t psz, rem;
	const size_t head_max = VOLUTA_SYMLNK_HEAD_MAX;
	const size_t part_max = VOLUTA_SYMLNK_PART_MAX;

	voluta_memzero(svi, sizeof(*svi));

	if (len <= head_max) {
		svi->head_len = len;
		rem = 0;
	} else {
		svi->head_len = head_max;
		rem = len - head_max;
	}

	svi->nparts = 0;
	while (rem > 0) {
		if (svi->nparts == ARRAY_SIZE(svi->part_len)) {
			return -ENAMETOOLONG;
		}
		psz = voluta_min(rem, part_max);
		svi->part_len[svi->nparts++] = psz;
		rem -= psz;
	}
	return 0;
}

static void vaddr_of_symval(struct voluta_vaddr *vaddr, loff_t off)
{
	voluta_vaddr_by_off(vaddr, VOLUTA_VTYPE_SYMVAL, off);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static ino_t lnv_parent_ino(const struct voluta_lnk_value *lnv)
{
	return ino_to_cpu(lnv->lv_parent_ino);
}

static void lnv_set_parent_ino(struct voluta_lnk_value *lnv, ino_t parent_ino)
{
	lnv->lv_parent_ino = cpu_to_ino(parent_ino);
}

static void lnv_set_length(struct voluta_lnk_value *lnv, size_t length)
{
	lnv->lv_length = cpu_to_le16((uint16_t)length);
}

static const void *lnv_value(const struct voluta_lnk_value *lnv)
{
	return lnv->lv_value;
}

static void lnv_set_value(struct voluta_lnk_value *lnv,
			  const void *value, size_t length)
{
	voluta_assert_le(length, sizeof(lnv->lv_value));
	memcpy(lnv->lv_value, value, length);
}

static void lnv_init(struct voluta_lnk_value *lnv, ino_t parent_ino,
		     const char *value, size_t length)
{
	lnv_set_parent_ino(lnv, parent_ino);
	lnv_set_length(lnv, length);
	lnv_set_value(lnv, value, length);
}

static const void *symval_of(const struct voluta_vnode_info *vi)
{
	return lnv_value(vi->u.lnv);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const void *lis_head_value(const struct voluta_lnk_ispec *lis)
{
	return lis->l_head;
}

static void lis_set_head_value(struct voluta_lnk_ispec *lis,
			       const void *value, size_t length)
{
	voluta_assert_le(length, sizeof(lis->l_head));
	memcpy(lis->l_head, value, length);
}

static loff_t lis_tail_part(const struct voluta_lnk_ispec *lis, size_t slot)
{
	voluta_assert_lt(slot, ARRAY_SIZE(lis->l_tail));
	return off_to_cpu(lis->l_tail[slot]);
}

static void lis_set_tail_part(struct voluta_lnk_ispec *lis,
			      size_t slot, loff_t off)
{
	voluta_assert_lt(slot, ARRAY_SIZE(lis->l_tail));
	lis->l_tail[slot] = cpu_to_off(off);
}

static void lis_reset_tail_part(struct voluta_lnk_ispec *lis, size_t slot)
{
	lis_set_tail_part(lis, slot, VOLUTA_OFF_NULL);
}

static void lis_init(struct voluta_lnk_ispec *lis)
{
	voluta_memzero(lis->l_head, sizeof(lis->l_head));
	for (size_t slot = 0; slot < ARRAY_SIZE(lis->l_tail); ++slot) {
		lis_reset_tail_part(lis, slot);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_lnk_ispec *lnk_ispec_of(const struct voluta_inode_info *ii)
{
	struct voluta_inode *inode = ii->inode;

	return &inode->i_s.l;
}

static size_t lnk_value_length(const struct voluta_inode_info *lnk_ii)
{
	return (size_t)i_size_of(lnk_ii);
}

static const void *lnk_value_head(const struct voluta_inode_info *lnk_ii)
{
	return lis_head_value(lnk_ispec_of(lnk_ii));
}

static void lnk_assign_value_head(const struct voluta_inode_info *lnk_ii,
				  const void *val, size_t len)
{
	lis_set_head_value(lnk_ispec_of(lnk_ii), val, len);
}

static int lnk_get_value_part(const struct voluta_inode_info *lnk_ii,
			      size_t slot, struct voluta_vaddr *out_vaddr)
{
	loff_t off;
	const struct voluta_lnk_ispec *lis = lnk_ispec_of(lnk_ii);

	off = lis_tail_part(lis, slot);
	vaddr_of_symval(out_vaddr, off);

	return !off_isnull(off) ? 0 : -ENOENT;
}

static void lnk_set_value_part(struct voluta_inode_info *lnk_ii, size_t slot,
			       const struct voluta_vaddr *vaddr)
{
	struct voluta_lnk_ispec *lis = lnk_ispec_of(lnk_ii);

	lis_set_tail_part(lis, slot, vaddr->off);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int require_symlnk(const struct voluta_inode_info *ii)
{
	if (i_isdir(ii)) {
		return -EISDIR;
	}
	if (!i_islnk(ii)) {
		return -EINVAL;
	}
	return 0;
}

static void append_symval(struct voluta_buf *buf, const void *p, size_t n)
{
	if (p && n) {
		voluta_buf_append(buf, p, n);
		voluta_buf_seteos(buf);
	}
}

static int stage_symval(struct voluta_sb_info *sbi,
			const struct voluta_vaddr *vaddr,
			struct voluta_vnode_info **out_symval_vi)
{
	return voluta_stage_vnode(sbi, vaddr, out_symval_vi);
}

static int extern_symval(struct voluta_sb_info *sbi,
			 const struct voluta_inode_info *lnk_ii,
			 struct voluta_buf *buf)
{
	int err;
	size_t link_len;
	struct voluta_vaddr vaddr;
	struct voluta_vnode_info *symval_vi;
	struct voluta_symval_info svi;

	link_len = lnk_value_length(lnk_ii);
	err = setup_symval_info(&svi, link_len);
	if (err) {
		return err;
	}
	append_symval(buf, lnk_value_head(lnk_ii), svi.head_len);
	for (size_t i = 0; i < svi.nparts; ++i) {
		err = lnk_get_value_part(lnk_ii, i, &vaddr);
		if (err) {
			return err;
		}
		err = stage_symval(sbi, &vaddr, &symval_vi);
		if (err) {
			return err;
		}
		append_symval(buf, symval_of(symval_vi), svi.part_len[i]);
	}
	return 0;
}

int voluta_do_readlink(struct voluta_sb_info *sbi,
		       const struct voluta_inode_info *lnk_ii,
		       struct voluta_buf *buf)
{
	int err;

	err = require_symlnk(lnk_ii);
	if (err) {
		return err;
	}
	err = extern_symval(sbi, lnk_ii, buf);
	if (err) {
		return err;
	}
	return 0;
}

static int new_symval_node(struct voluta_sb_info *sbi, ino_t parent_ino,
			   const char *val, size_t len,
			   struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi;

	err = voluta_new_vnode(sbi, VOLUTA_VTYPE_SYMVAL, &vi);
	if (err) {
		return err;
	}
	lnv_init(vi->u.lnv, parent_ino, val, len);
	*out_vi = vi;
	return 0;
}

static int new_symval_node_at(struct voluta_sb_info *sbi, ino_t parent_ino,
			      const char *val, size_t len,
			      struct voluta_vaddr *out_vaddr)
{
	int err;
	struct voluta_vnode_info *symval_vi;

	err = new_symval_node(sbi, parent_ino, val, len, &symval_vi);
	if (err) {
		return err;
	}
	vaddr_clone(v_vaddr_of(symval_vi), out_vaddr);
	return 0;
}


static int del_symval_node_at(struct voluta_sb_info *sbi,
			      const struct voluta_vaddr *vaddr)

{
	return voluta_del_vnode_at(sbi, vaddr);
}

static int assign_symval(struct voluta_sb_info *sbi,
			 struct voluta_inode_info *lnk_ii,
			 const struct voluta_str *symval)
{
	int err;
	size_t part_len;
	const char *val;
	struct voluta_vaddr vaddr;
	struct voluta_symval_info svi;
	const ino_t ino = i_ino_of(lnk_ii);

	err = setup_symval_info(&svi, symval->len);
	if (err) {
		return err;
	}
	val = symval->str;
	lnk_assign_value_head(lnk_ii, val, svi.head_len);
	i_dirtify(lnk_ii);
	val += svi.head_len;

	for (size_t i = 0; i < svi.nparts; ++i) {
		part_len = svi.part_len[i];
		err = new_symval_node_at(sbi, ino, val, part_len, &vaddr);
		if (err) {
			return err;
		}
		lnk_set_value_part(lnk_ii, i, &vaddr);
		i_dirtify(lnk_ii);
		val += part_len;
	}
	return 0;
}

static loff_t length_of(const struct voluta_str *symval)
{
	return (loff_t)symval->len;
}

int voluta_setup_symlink(struct voluta_sb_info *sbi,
			 const struct voluta_oper_info *opi,
			 struct voluta_inode_info *lnk_ii,
			 const struct voluta_str *symval)
{
	int err;
	struct voluta_iattr attr;

	err = require_symlnk(lnk_ii);
	if (err) {
		return err;
	}
	err = assign_symval(sbi, lnk_ii, symval);
	if (err) {
		return err;
	}

	iattr_init(&attr, lnk_ii);
	attr.ia_size = length_of(symval);
	attr.ia_flags = VOLUTA_ATTR_MCTIME | VOLUTA_ATTR_SIZE;
	i_update_attr(lnk_ii, opi, &attr);
	return 0;
}

static int drop_symval(struct voluta_sb_info *sbi,
		       struct voluta_inode_info *lnk_ii)
{
	int err;
	struct voluta_vaddr vaddr;

	for (size_t i = 0; i < VOLUTA_SYMLNK_NPARTS; ++i) {
		err = lnk_get_value_part(lnk_ii, i, &vaddr);
		if (err == -ENOENT) {
			break;
		}
		err = del_symval_node_at(sbi, &vaddr);
		if (err) {
			return err;
		}
	}
	return 0;
}

int voluta_drop_symlink(struct voluta_sb_info *sbi,
			struct voluta_inode_info *lnk_ii)
{
	int err;

	err = require_symlnk(lnk_ii);
	if (err) {
		return err;
	}
	err = drop_symval(sbi, lnk_ii);
	if (err) {
		return err;
	}
	return 0;
}

void voluta_setup_new_symlnk(struct voluta_inode_info *lnk_ii)
{
	lis_init(lnk_ispec_of(lnk_ii));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_verify_lnk_value(const struct voluta_lnk_value *lnv)
{
	int err;

	err = voluta_verify_ino(lnv_parent_ino(lnv));
	if (err) {
		return err;
	}
	return 0;
}

