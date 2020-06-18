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
#include "libvoluta.h"


struct voluta_symval_info {
	struct voluta_str head;
	struct voluta_str parts[VOLUTA_SYMLNK_NPARTS];
	size_t nparts;
};

struct voluta_symlink_ctx {
	const struct voluta_oper_ctx *op_ctx;
	struct voluta_sb_info    *sbi;
	struct voluta_inode_info *lnk_ii;
	const struct voluta_str *symval;
};

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const char *next_part(const char *val, size_t len)
{
	return (val != NULL) ? (val + len) : NULL;
}

static size_t head_size(size_t len)
{
	return min(len, VOLUTA_SYMLNK_HEAD_MAX);
}

static size_t part_size(size_t len)
{
	return min(len, VOLUTA_SYMLNK_PART_MAX);
}

static int setup_symval_info(struct voluta_symval_info *svi,
			     const char *val, size_t len)
{
	size_t rem;
	struct voluta_str *str;

	voluta_memzero(svi, sizeof(*svi));
	svi->nparts = 0;

	str = &svi->head;
	str->len = head_size(len);
	str->str = val;

	val = next_part(val, str->len);
	rem = len - str->len;
	while (rem > 0) {
		if (svi->nparts == ARRAY_SIZE(svi->parts)) {
			return -ENAMETOOLONG;
		}
		str = &svi->parts[svi->nparts++];
		str->len = part_size(rem);
		str->str = val;

		val = next_part(val, str->len);
		rem -= str->len;
	}
	return 0;
}

static void vaddr_of_symval(struct voluta_vaddr *vaddr, loff_t off)
{
	vaddr_by_off(vaddr, VOLUTA_VTYPE_SYMVAL, off);
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

static struct voluta_lnk_ispec *
lnk_ispec_of(const struct voluta_inode_info *ii)
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

static int check_symlnk(const struct voluta_symlink_ctx *sl_ctx)
{
	if (i_isdir(sl_ctx->lnk_ii)) {
		return -EISDIR;
	}
	if (!i_islnk(sl_ctx->lnk_ii)) {
		return -EINVAL;
	}
	return 0;
}

static void append_symval(struct voluta_buf *buf, const void *p, size_t n)
{
	if (p && n) {
		buf_append(buf, p, n);
		buf_seteos(buf);
	}
}

static int stage_symval(const struct voluta_symlink_ctx *sl_ctx,
			const struct voluta_vaddr *vaddr,
			struct voluta_vnode_info **out_vi)
{
	return voluta_stage_vnode(sl_ctx->sbi, vaddr, sl_ctx->lnk_ii, out_vi);
}

static int extern_symval_head(const struct voluta_symlink_ctx *sl_ctx,
			      const struct voluta_symval_info *svi,
			      struct voluta_buf *buf)
{
	const struct voluta_inode_info *lnk_ii = sl_ctx->lnk_ii;

	append_symval(buf, lnk_value_head(lnk_ii), svi->head.len);
	return 0;
}

static int extern_symval_parts(const struct voluta_symlink_ctx *sl_ctx,
			       const struct voluta_symval_info *svi,
			       struct voluta_buf *buf)
{
	int err;
	struct voluta_vaddr vaddr;
	struct voluta_vnode_info *vi;
	const struct voluta_inode_info *lnk_ii = sl_ctx->lnk_ii;

	for (size_t i = 0; i < svi->nparts; ++i) {
		err = lnk_get_value_part(lnk_ii, i, &vaddr);
		if (err) {
			return err;
		}
		err = stage_symval(sl_ctx, &vaddr, &vi);
		if (err) {
			return err;
		}
		v_incref(vi);
		append_symval(buf, symval_of(vi), svi->parts[i].len);
		v_decref(vi);
	}
	return 0;
}


static int extern_symval(const struct voluta_symlink_ctx *sl_ctx,
			 struct voluta_buf *buf)
{
	int err;
	size_t len;
	struct voluta_symval_info svi;
	const struct voluta_inode_info *lnk_ii = sl_ctx->lnk_ii;

	len = lnk_value_length(lnk_ii);
	err = setup_symval_info(&svi, NULL, len);
	if (err) {
		return err;
	}
	err = extern_symval_head(sl_ctx, &svi, buf);
	if (err) {
		return err;
	}
	err = extern_symval_parts(sl_ctx, &svi, buf);
	if (err) {
		return err;
	}
	return 0;
}

static int readlink_of(const struct voluta_symlink_ctx *sl_ctx,
		       struct voluta_buf *buf)
{
	int err;

	err = check_symlnk(sl_ctx);
	if (err) {
		return err;
	}
	err = extern_symval(sl_ctx, buf);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_do_readlink(const struct voluta_oper_ctx *op_ctx,
		       struct voluta_inode_info *lnk_ii,
		       struct voluta_buf *buf)
{
	int err;
	struct voluta_symlink_ctx sl_ctx = {
		.op_ctx = op_ctx,
		.sbi = i_sbi_of(lnk_ii),
		.lnk_ii = lnk_ii,
	};

	i_incref(lnk_ii);
	err = readlink_of(&sl_ctx, buf);
	i_decref(lnk_ii);

	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int new_symval(const struct voluta_symlink_ctx *sl_ctx,
		      struct voluta_vnode_info **out_vi)
{
	return voluta_new_vnode(sl_ctx->sbi, sl_ctx->lnk_ii,
				VOLUTA_VTYPE_SYMVAL, out_vi);
}

static int del_symval_node_at(const struct voluta_symlink_ctx *sl_ctx,
			      const struct voluta_vaddr *vaddr)

{
	return voluta_del_vnode_at(sl_ctx->sbi, vaddr);
}

static int create_symval(struct voluta_symlink_ctx *sl_ctx,
			 const struct voluta_str *str,
			 struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi;
	const ino_t parent_ino = i_ino_of(sl_ctx->lnk_ii);

	err = new_symval(sl_ctx, &vi);
	if (err) {
		return err;
	}
	lnv_init(vi->u.lnv, parent_ino, str->str, str->len);
	*out_vi = vi;
	return 0;
}

static int assign_symval_head(struct voluta_symlink_ctx *sl_ctx,
			      const struct voluta_symval_info *svi)
{
	struct voluta_inode_info *lnk_ii = sl_ctx->lnk_ii;

	lnk_assign_value_head(lnk_ii, svi->head.str, svi->head.len);
	i_dirtify(lnk_ii);
	return 0;
}

static int assign_symval_parts(struct voluta_symlink_ctx *sl_ctx,
			       const struct voluta_symval_info *svi)
{
	int err;
	struct voluta_vnode_info *vi;

	for (size_t i = 0; i < svi->nparts; ++i) {
		err = create_symval(sl_ctx, &svi->parts[i], &vi);
		if (err) {
			return err;
		}
		lnk_set_value_part(sl_ctx->lnk_ii, i, v_vaddr_of(vi));
		i_dirtify(sl_ctx->lnk_ii);
	}
	return 0;
}

static int assign_symval(struct voluta_symlink_ctx *sl_ctx)
{
	int err;
	struct voluta_symval_info svi;
	const struct voluta_str *symval = sl_ctx->symval;

	err = setup_symval_info(&svi, symval->str, symval->len);
	if (err) {
		return err;
	}
	err = assign_symval_head(sl_ctx, &svi);
	if (err) {
		return err;
	}
	err = assign_symval_parts(sl_ctx, &svi);
	if (err) {
		return err;
	}
	return 0;
}

static loff_t length_of(const struct voluta_str *symval)
{
	return (loff_t)symval->len;
}

static void post_symlink(const struct voluta_symlink_ctx *sl_ctx)
{
	struct voluta_iattr iattr;
	struct voluta_inode_info *lnk_ii = sl_ctx->lnk_ii;

	iattr_setup(&iattr, i_ino_of(lnk_ii));
	iattr.ia_size = length_of(sl_ctx->symval);
	iattr.ia_flags = VOLUTA_IATTR_MCTIME | VOLUTA_IATTR_SIZE;
	update_iattrs(sl_ctx->op_ctx, lnk_ii, &iattr);
}

static int do_symlink(struct voluta_symlink_ctx *sl_ctx)
{
	int err;

	err = check_symlnk(sl_ctx);
	if (err) {
		return err;
	}
	err = assign_symval(sl_ctx);
	if (err) {
		return err;
	}
	post_symlink(sl_ctx);
	return 0;
}

int voluta_setup_symlink(const struct voluta_oper_ctx *op_ctx,
			 struct voluta_inode_info *lnk_ii,
			 const struct voluta_str *symval)
{
	int err;
	struct voluta_symlink_ctx sl_ctx = {
		.op_ctx = op_ctx,
		.sbi = i_sbi_of(lnk_ii),
		.lnk_ii = lnk_ii,
		.symval = symval
	};

	i_incref(lnk_ii);
	err = do_symlink(&sl_ctx);
	i_decref(lnk_ii);

	return err;
}

static int drop_symval(const struct voluta_symlink_ctx *sl_ctx)
{
	int err;
	struct voluta_vaddr vaddr;

	for (size_t i = 0; i < VOLUTA_SYMLNK_NPARTS; ++i) {
		err = lnk_get_value_part(sl_ctx->lnk_ii, i, &vaddr);
		if (err == -ENOENT) {
			break;
		}
		err = del_symval_node_at(sl_ctx, &vaddr);
		if (err) {
			return err;
		}
	}
	return 0;
}

int voluta_drop_symlink(struct voluta_inode_info *lnk_ii)
{
	int err;
	struct voluta_symlink_ctx sl_ctx = {
		.sbi = i_sbi_of(lnk_ii),
		.lnk_ii = lnk_ii,
	};

	i_incref(lnk_ii);
	err = drop_symval(&sl_ctx);
	i_decref(lnk_ii);

	return err;
}

void voluta_setup_new_symlnk(struct voluta_inode_info *lnk_ii)
{
	struct voluta_lnk_ispec *lis = lnk_ispec_of(lnk_ii);

	lis_init(lis);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_verify_lnk_value(const struct voluta_lnk_value *lnv)
{
	int err;
	ino_t parent_ino;

	parent_ino = lnv_parent_ino(lnv);
	err = voluta_verify_ino(parent_ino);
	if (err) {
		return err;
	}
	return 0;
}

