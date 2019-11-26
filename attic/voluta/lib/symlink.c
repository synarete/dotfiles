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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "voluta-lib.h"

static struct voluta_lnk_symval *symval_of(const struct voluta_vnode_info *vi)
{
	voluta_assert_eq(vi->bki->b_vaddr.vtype, VOLUTA_VTYPE_SYMVAL);

	return &vi->lview->lnk_symval;
}

static void *symval_databuf(struct voluta_lnk_symval *symval)
{
	return (void *)symval->lv_value_tail;
}

static void symval_init(struct voluta_lnk_symval *symval, ino_t parent_ino,
			const char *value, size_t length)
{
	voluta_assert_gt(length, 0);
	voluta_assert_lt(length, sizeof(symval->lv_value_tail));

	symval->lv_parent_ino = parent_ino;
	memcpy(symval_databuf(symval), value, length);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t linklen_of(const struct voluta_inode_info *lnk_ii)
{
	return (size_t)i_size_of(lnk_ii);
}

static void parts_length(size_t linklen, size_t *headlen, size_t *taillen)
{
	size_t headsz;
	const struct voluta_inode *inode = NULL;

	headsz = sizeof(inode->i_s.lr.lnk_head);
	if (linklen < headsz) {
		*headlen = linklen;
		*taillen = 0;
	} else {
		*headlen = (headsz - 1);
		*taillen = (linklen - *headlen);
	}
}

static loff_t tail_offset_of(const struct voluta_inode_info *lnk_ii)
{
	const struct voluta_inode *inode = lnk_ii->inode;

	return inode->i_s.lr.lnk_tail_off;
}

static int tail_vaddr_of(const struct voluta_inode_info *lnk_ii,
			 struct voluta_vaddr *out_vaddr)
{
	const loff_t off = tail_offset_of(lnk_ii);

	if (off_isnull(off)) {
		return -ENOENT;
	}
	voluta_vaddr_by_off(out_vaddr, VOLUTA_VTYPE_SYMVAL, off);
	return 0;
}

static void *head_value_of(const struct voluta_inode_info *lnk_ii)
{
	struct voluta_inode *inode = lnk_ii->inode;

	return inode->i_s.lr.lnk_head;
}

static void *tail_value_of(const struct voluta_vnode_info *symval_vi)
{
	voluta_assert_not_null(symval_vi);

	return symval_databuf(symval_of(symval_vi));
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

static void copy_linkpath_to(const void *head, size_t headlen,
			     const void *tail, size_t taillen,
			     const struct voluta_buf *buf)
{
	size_t count, datlen = buf->len;
	char *datbuf = buf->ptr;

	if (!buf->len || !buf->ptr) {
		return;
	}
	count = (headlen < datlen) ? headlen : (datlen - 1);
	memcpy(datbuf, head, count);
	datbuf[count] = '\0';

	if (!tail || !taillen) {
		return;
	}
	datbuf += count;
	datlen -= count;
	count = (taillen < datlen) ? taillen : (datlen - 1);
	memcpy(datbuf, tail, count);
	datbuf[count] = '\0';
}

static int stage_symval(struct voluta_sb_info *sbi,
			const struct voluta_vaddr *vaddr,
			struct voluta_vnode_info **out_symval_vi)
{
	return voluta_stage_vnode(sbi, vaddr, out_symval_vi);
}

static int extern_symval(struct voluta_sb_info *sbi,
			 const struct voluta_inode_info *lnk_ii,
			 const struct voluta_buf *buf)
{
	int err;
	size_t linklen, headlen, taillen;
	struct voluta_vaddr tail_vaddr;
	struct voluta_vnode_info *symval_vi;
	void const *head, *tail = NULL;

	head = head_value_of(lnk_ii);
	linklen = linklen_of(lnk_ii);
	parts_length(linklen, &headlen, &taillen);

	err = tail_vaddr_of(lnk_ii, &tail_vaddr);
	if (err == -ENOENT) {
		goto out; /* head only */
	}
	err = stage_symval(sbi, &tail_vaddr, &symval_vi);
	if (err) {
		return err;
	}
	tail = tail_value_of(symval_vi);
out:
	copy_linkpath_to(head, headlen, tail, taillen, buf);
	return 0;
}

int voluta_do_readlink(struct voluta_env *env,
		       const struct voluta_inode_info *lnk_ii,
		       struct voluta_buf *buf)
{
	int err;
	struct voluta_sb_info *sbi = sbi_of(env);

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

static int new_symval_node(struct voluta_env *env, ino_t parent_ino,
			   const char *val, size_t len,
			   struct voluta_vnode_info **symval_vi)
{
	int err;

	err = voluta_new_vnode(sbi_of(env), VOLUTA_VTYPE_SYMVAL, symval_vi);
	if (err) {
		return err;
	}
	symval_init(symval_of(*symval_vi), parent_ino, val, len);
	return 0;
}

static int del_symval_node(struct voluta_env *env,
			   struct voluta_vnode_info *symval_vi)
{
	return voluta_del_vnode(env, symval_vi);
}

static loff_t symval_addr_of(const struct voluta_vnode_info *symval_vi)
{
	const struct voluta_vaddr *vaddr;

	vaddr = v_vaddr_of(symval_vi);
	return vaddr->off;
}

static int assign_symval(struct voluta_env *env,
			 struct voluta_inode_info *lnk_ii,
			 const struct voluta_str *symval)
{
	int err;
	const char *val;
	size_t headlen, taillen;
	struct voluta_vnode_info *symval_vi;
	struct voluta_inode *inode = lnk_ii->inode;

	parts_length(symval->len, &headlen, &taillen);
	memcpy(head_value_of(lnk_ii), symval->str, headlen);
	if (taillen == 0) {
		return 0; /* head only */
	}
	val = symval->str + headlen;
	err = new_symval_node(env, i_ino_of(lnk_ii), val, taillen, &symval_vi);
	if (err) {
		return err;
	}
	inode->i_s.lr.lnk_tail_off = symval_addr_of(symval_vi);
	i_dirtify(lnk_ii);
	return 0;
}

static loff_t length_of(const struct voluta_str *symval)
{
	return (loff_t)symval->len;
}

int voluta_setup_symlink(struct voluta_env *env,
			 struct voluta_inode_info *lnk_ii,
			 const struct voluta_str *symval)
{
	int err;
	struct voluta_iattr attr;

	attr_reset(&attr);
	attr.ia_ino = i_ino_of(lnk_ii);
	attr.ia_size = length_of(symval);

	err = require_symlnk(lnk_ii);
	if (err) {
		return err;
	}
	err = assign_symval(env, lnk_ii, symval);
	if (err) {
		return err;
	}
	attr.ia_flags = VOLUTA_ATTR_RMCTIME | VOLUTA_ATTR_SIZE;
	i_update_attr(env, lnk_ii, &attr);

	return 0;
}

static int drop_symval(struct voluta_env *env, struct voluta_inode_info *lnk_ii)
{
	int err;
	struct voluta_vaddr tail_vaddr;
	struct voluta_vnode_info *symval_vi;

	err = tail_vaddr_of(lnk_ii, &tail_vaddr);
	if (err == -ENOENT) {
		return 0;
	}
	/* TODO: Free by vaddr only */
	err = stage_symval(sbi_of(env), &tail_vaddr, &symval_vi);
	if (err) {
		return err;
	}
	err = del_symval_node(env, symval_vi);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_drop_symlink(struct voluta_env *env,
			struct voluta_inode_info *lnk_ii)
{
	int err;

	err = require_symlnk(lnk_ii);
	if (err) {
		return err;
	}
	err = drop_symval(env, lnk_ii);
	if (err) {
		return err;
	}
	return 0;
}

void voluta_setup_new_symlnk(struct voluta_inode_info *lnk_ii)
{
	struct voluta_inode *inode = lnk_ii->inode;

	memset(inode->i_s.lr.lnk_head, 0, sizeof(inode->i_s.lr.lnk_head));
	inode->i_s.lr.lnk_tail_off = VOLUTA_OFF_NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_verify_symval(const struct voluta_lnk_symval *symval)
{
	/* TODO: more check */
	return voluta_verify_ino(symval->lv_parent_ino);
}

