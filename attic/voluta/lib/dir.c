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
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include "voluta-lib.h"


#define HTREE_FANOUT            VOLUTA_DIR_HTNODE_NCHILDS
#define HTREE_DEPTH_MAX         VOLUTA_DIR_HTREE_DEPTH_MAX
#define HTREE_NODE_MAX          VOLUTA_DIR_HTREE_NODE_MAX
#define HTREE_INDEX_MAX         VOLUTA_DIR_HTREE_NODE_MAX
#define HTREE_INDEX_NULL        UINT32_MAX
#define HTREE_INDEX_ROOT        0



struct voluta_dir_entry_view {
	struct voluta_dir_entry de;
	uint8_t de_name[VOLUTA_NAME_MAX + 1];
} voluta_packed;

struct voluta_dir_entry_info {
	struct voluta_vnode_info *vi;
	struct voluta_dir_entry  *de;
	struct voluta_ino_dt  ino_dt;
};

struct voluta_dir_ctx {
	struct voluta_sb_info     *sbi;
	const struct voluta_oper_info *opi;
	struct voluta_inode_info  *dir_ii;
	struct voluta_inode_info  *parent_ii;
	struct voluta_inode_info  *ii;
	struct voluta_readdir_ctx *readdir_ctx;
	const struct voluta_qstr  *name;
	bool  keep_iter;
};


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool ino_isvalid(ino_t ino)
{
	/* TODO: Check ino max */
	return (ino > VOLUTA_INO_NULL);
}

static mode_t safe_dttoif(mode_t dt)
{
	mode_t mode;

	switch (dt) {
	case DT_UNKNOWN:
	case DT_FIFO:
	case DT_CHR:
	case DT_DIR:
	case DT_BLK:
	case DT_REG:
	case DT_LNK:
	case DT_SOCK:
	case DT_WHT:
		mode = DTTOIF(dt);
		break;
	default:
		mode = 0;
		break;
	}
	return mode;
}

static void vaddr_of_htnode(struct voluta_vaddr *vaddr, loff_t off)
{
	vaddr_by_off(vaddr, VOLUTA_VTYPE_HTNODE, off);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static loff_t make_doffset(size_t node_index, size_t sloti)
{
	return (loff_t)(((node_index + 1) << 16) | (sloti << 2));
}

static size_t doffset_to_index(loff_t doff)
{
	const loff_t base_doff = make_doffset(0, 0);

	return (doff >= base_doff) ?
	       (size_t)((doff >> 16) & 0xFFFFFFFFL) - 1 : HTREE_INDEX_ROOT;
}

static bool index_isnull(size_t index)
{
	return (index == HTREE_INDEX_NULL);
}

static bool index_isvalid(size_t index)
{
	VOLUTA_STATICASSERT_GT(HTREE_INDEX_NULL, VOLUTA_DIR_HTREE_NODE_MAX);

	return (index < HTREE_INDEX_MAX);
}

static size_t index_to_parent(size_t index)
{
	return (index - 1) / HTREE_FANOUT;
}

static size_t index_to_child_ord(size_t index)
{
	const size_t parent_index = index_to_parent(index);

	return (index - (parent_index * HTREE_FANOUT) - 1);
}

static size_t parent_to_child_index(size_t parent_index, size_t child_ord)
{
	return (parent_index * HTREE_FANOUT) + child_ord + 1;
}

static size_t index_to_depth(size_t index)
{
	size_t depth = 0;

	/* TODO: use shift operations */
	while (index > 0) {
		depth++;
		index = index_to_parent(index);
	}
	return depth;
}

static bool depth_isvalid(size_t depth)
{
	return (depth <= HTREE_DEPTH_MAX);
}

static bool index_valid_depth(size_t index)
{
	return index_isvalid(index) && depth_isvalid(index_to_depth(index));
}

static size_t hash_to_child_ord(uint64_t hash, size_t depth)
{
	voluta_assert_gt(depth, 0);
	voluta_assert_lt(depth, sizeof(hash));
	voluta_assert_le(depth, HTREE_DEPTH_MAX);

	return (hash >> (8 * (depth - 1))) % HTREE_FANOUT;
}

static size_t hash_to_child_index(uint64_t hash, size_t parent_index)
{
	const size_t depth = index_to_depth(parent_index);
	const size_t child_ord = hash_to_child_ord(hash, depth + 1);

	return parent_to_child_index(parent_index, child_ord);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_dir_entry_view *
de_view_of(const struct voluta_dir_entry *de)
{
	const struct voluta_dir_entry_view *de_view;

	de_view = container_of(de, struct voluta_dir_entry_view, de);
	return (struct voluta_dir_entry_view *)de_view;
}

static mode_t de_dtype(const struct voluta_dir_entry *de)
{
	return de->de_dtype;
}

static void de_set_dtype(struct voluta_dir_entry *de, mode_t dt)
{
	de->de_dtype = (uint8_t)dt;
}

static bool de_isvalid(const struct voluta_dir_entry *de)
{
	return (safe_dttoif(de_dtype(de)) != 0);
}

static ino_t de_ino(const struct voluta_dir_entry *de)
{
	return ino_to_cpu(de->de_ino);
}

static void de_set_ino(struct voluta_dir_entry *de, ino_t ino)
{
	de->de_ino = cpu_to_ino(ino);
}

static size_t de_name_len(const struct voluta_dir_entry *de)
{
	return le16_to_cpu(de->de_name_len);
}

static void de_set_name_len(struct voluta_dir_entry *de, size_t nlen)
{
	de->de_name_len = cpu_to_le16((uint16_t)nlen);
}

static bool de_isactive(const struct voluta_dir_entry *de)
{
	return (de_name_len(de) > 0) && ino_isvalid(de_ino(de));
}

static const char *de_name(const struct voluta_dir_entry *de)
{
	const struct voluta_dir_entry_view *de_view = de_view_of(de);

	return (const char *)(de_view->de_name);
}

static bool de_has_name_len(const struct voluta_dir_entry *de, size_t nlen)
{
	return (de_name_len(de) == nlen);
}

static bool de_has_name(const struct voluta_dir_entry *de,
			const struct voluta_str *name)
{
	bool ret = false;

	if (de_isactive(de) && de_has_name_len(de, name->len)) {
		ret = !memcmp(de_name(de), name->str, name->len);
	}
	return ret;
}

static size_t de_nents_for_name(const struct voluta_dir_entry *de, size_t nlen)
{
	const size_t de_size = sizeof(*de);
	const size_t name_ndes = (nlen + de_size - 1) / de_size;

	return 1 + name_ndes;
}

static size_t de_nents(const struct voluta_dir_entry *de)
{
	return le16_to_cpu(de->de_nents);
}

static void de_set_nents(struct voluta_dir_entry *de, size_t nents)
{
	de->de_nents = cpu_to_le16((uint16_t)nents);
}

static size_t de_nprev(const struct voluta_dir_entry *de)
{
	return le16_to_cpu(de->de_nprev);
}

static void de_set_nprev(struct voluta_dir_entry *de, size_t nprev)
{
	de->de_nprev = cpu_to_le16((uint16_t)nprev);
}

static struct voluta_dir_entry *de_next(const struct voluta_dir_entry *de)
{
	const size_t step = de_nents(de);

	voluta_assert_gt(step, 0);
	return (struct voluta_dir_entry *)(de + step);
}

static struct voluta_dir_entry *
de_next_safe(const struct voluta_dir_entry *de,
	     const struct voluta_dir_entry *end)
{
	struct voluta_dir_entry *next = de_next(de);

	voluta_assert(next <= end);
	return (next < end) ? next : NULL;
}

static struct voluta_dir_entry *
de_prev_safe(const struct voluta_dir_entry *de)
{
	const size_t step = de_nprev(de);

	return step ? (struct voluta_dir_entry *)(de - step) : NULL;
}

static void de_assign(struct voluta_dir_entry *de, size_t nents,
		      const struct voluta_str *name, ino_t ino, mode_t dt)
{
	struct voluta_dir_entry_view *de_view  = de_view_of(de);

	de_set_ino(de, ino);
	de_set_name_len(de, name->len);
	de_set_dtype(de, dt);
	de_set_nents(de, nents);
	memcpy(de_view->de_name, name->str, name->len);
}

static void de_reset(struct voluta_dir_entry *de, size_t nents, size_t nprev)
{
	voluta_memzero(de + 1, (nents - 1) * sizeof(*de));
	de_set_ino(de, VOLUTA_INO_NULL);
	de_set_name_len(de, 0);
	de_set_dtype(de, 0);
	de_set_nents(de, nents);
	de_set_nprev(de, nprev);
}

static void de_reset_arr(struct voluta_dir_entry *de, size_t nents)
{
	de_reset(de, nents, 0);
}

static size_t de_slot(const struct voluta_dir_entry *de,
		      const struct voluta_dir_entry *beg)
{
	const ptrdiff_t slot = (de - beg);

	voluta_assert_ge(de, beg);
	voluta_assert_lt(slot, VOLUTA_DIR_HTNODE_NENTS);

	return (size_t)slot;
}

static loff_t de_doffset(const struct voluta_dir_entry *de,
			 const struct voluta_dir_entry *beg, size_t node_index)
{
	return make_doffset(node_index, de_slot(de, beg));
}

static const struct voluta_dir_entry *
de_search(const struct voluta_dir_entry *de,
	  const struct voluta_dir_entry *end,
	  const struct voluta_str *name)
{
	const struct voluta_dir_entry *itr = de;

	while (itr < end) {
		if (de_has_name(itr, name)) {
			return itr;
		}
		itr = de_next(itr);
	}
	return NULL;
}

static const struct voluta_dir_entry *
de_scan(const struct voluta_dir_entry *de, const struct voluta_dir_entry *beg,
	const struct voluta_dir_entry *end, size_t node_index, loff_t pos)
{
	loff_t doff;
	const struct voluta_dir_entry *itr = de;

	while (itr < end) {
		if (de_isactive(itr)) {
			voluta_assert(de_isvalid(itr));
			doff = de_doffset(itr, beg, node_index);
			if (doff >= pos) {
				return itr;
			}
		}
		itr = de_next(itr);
	}
	return NULL;
}

static const struct voluta_dir_entry *
de_insert_at(struct voluta_dir_entry *de, const struct voluta_dir_entry *end,
	     size_t nents, const struct voluta_str *name, ino_t ino, mode_t dt)
{
	size_t nents_new;
	struct voluta_dir_entry *next_new;
	struct voluta_dir_entry *next_old;
	const size_t ncurr = de_nents(de);

	next_old = de_next_safe(de, end);
	de_assign(de, nents, name, ino, dt);
	if (nents < ncurr) {
		nents_new = ncurr - nents;
		next_new = de_next(de);
		de_reset(next_new, nents_new, nents);
		if (next_old != NULL) {
			voluta_assert(de_isactive(next_old));
			de_set_nprev(next_old, nents_new);
		}
	}
	return de;
}

static bool de_may_insert(const struct voluta_dir_entry *de, size_t nwant)
{
	size_t nents;

	if (de_isactive(de)) {
		return false;
	}
	nents = de_nents(de);
	if (nwant > nents) {
		return false;
	}
	/* avoid single-dentry holes, as it is useless */
	if ((nents - nwant) == 1) {
		return false;
	}
	return true;
}

static const struct voluta_dir_entry *
de_insert(struct voluta_dir_entry *beg, const struct voluta_dir_entry *end,
	  const struct voluta_str *name, ino_t ino, mode_t dt)
{
	struct voluta_dir_entry *itr = beg;
	const size_t nwant = de_nents_for_name(itr, name->len);

	while (itr < end) {
		if (de_may_insert(itr, nwant)) {
			return de_insert_at(itr, end, nwant, name, ino, dt);
		}
		itr = de_next(itr);
	}
	return NULL;
}

static void de_remove(struct voluta_dir_entry *de,
		      const struct voluta_dir_entry *end)
{
	size_t nents = de_nents(de);
	size_t nents_prev = de_nprev(de);
	struct voluta_dir_entry *next;
	struct voluta_dir_entry *prev;
	struct voluta_dir_entry *dent = de;

	next = de_next_safe(de, end);
	if (next != NULL) {
		if (!de_isactive(next)) {
			nents += de_nents(next);
			next = de_next_safe(next, end);
		}
	}
	prev = de_prev_safe(de);
	if (prev != NULL) {
		if (!de_isactive(prev)) {
			nents += de_nents(prev);
			nents_prev = de_nprev(prev);
			dent = prev;
		}
	}

	de_reset(dent, nents, nents_prev);
	if (next != NULL) {
		de_set_nprev(next, nents);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static ino_t htn_ino(const struct voluta_dir_htree_node *htn)
{
	return ino_to_cpu(htn->dh_ino);
}

static void htn_set_ino(struct voluta_dir_htree_node *htn, ino_t ino)
{
	htn->dh_ino = cpu_to_ino(ino);
}

static loff_t htn_parent(const struct voluta_dir_htree_node *htn)
{
	return off_to_cpu(htn->dh_parent);
}

static void htn_set_parent(struct voluta_dir_htree_node *htn, loff_t parent)
{
	htn->dh_parent = cpu_to_off(parent);
}

static size_t htn_node_index(const struct voluta_dir_htree_node *htn)
{
	return le32_to_cpu(htn->dh_node_index);
}

static void htn_set_node_index(struct voluta_dir_htree_node *htn, size_t index)
{
	htn->dh_node_index = cpu_to_le32((uint32_t)index);
}

static loff_t htn_child(const struct voluta_dir_htree_node *htn, size_t ord)
{
	return off_to_cpu(htn->dh_child[ord]);
}

static void htn_set_child(struct voluta_dir_htree_node *htn,
			  size_t child_ord, loff_t child_off)
{
	htn->dh_child[child_ord] = cpu_to_off(child_off);
}

static void htn_reset_childs(struct voluta_dir_htree_node *htn)
{
	for (size_t i = 0; i < ARRAY_SIZE(htn->dh_child); ++i) {
		htn_set_child(htn, i, VOLUTA_OFF_NULL);
	}
}

static void htn_setup(struct voluta_dir_htree_node *htn,
		      ino_t ino, size_t node_index, loff_t parent_off)
{
	voluta_assert_le(node_index, HTREE_NODE_MAX);

	htn_set_ino(htn, ino);
	htn_set_parent(htn, parent_off);
	htn_set_node_index(htn, node_index);
	htn_reset_childs(htn);
	de_reset_arr(htn->de, ARRAY_SIZE(htn->de));
	htn->dh_flags = 0;
}

static size_t htn_child_ord(const struct voluta_dir_htree_node *htn)
{
	return index_to_child_ord(htn_node_index(htn));
}

static size_t htn_depth(const struct voluta_dir_htree_node *htn)
{
	const size_t index = htn_node_index(htn);

	return index_to_depth(index);
}

static struct voluta_dir_entry *
htn_begin(const struct voluta_dir_htree_node *htn)
{
	return (struct voluta_dir_entry *)htn->de;
}

static const struct voluta_dir_entry *
htn_end(const struct voluta_dir_htree_node *htn)
{
	return htn->de + ARRAY_SIZE(htn->de);
}

static void htn_reset_des(struct voluta_dir_htree_node *htn)
{
	de_reset(htn->de, ARRAY_SIZE(htn->de), 0);
}

static const struct voluta_dir_entry *
htn_search(const struct voluta_dir_htree_node *htn,
	   const struct voluta_qstr *name)
{
	return de_search(htn_begin(htn), htn_end(htn), &name->str);
}

static const struct voluta_dir_entry *
htn_scan(const struct voluta_dir_htree_node *htn,
	 const struct voluta_dir_entry *hint, loff_t pos)
{
	const struct voluta_dir_entry *beg = htn_begin(htn);
	const struct voluta_dir_entry *end = htn_end(htn);
	const size_t node_index = htn_node_index(htn);

	return de_scan(hint ? hint : beg, beg, end, node_index, pos);
}

static const struct voluta_dir_entry *
htn_insert(struct voluta_dir_htree_node *htn,
	   const struct voluta_qstr *name, ino_t ino, mode_t dt)
{
	return de_insert(htn_begin(htn), htn_end(htn), &name->str, ino, dt);
}

static loff_t htn_next_doffset(const struct voluta_dir_htree_node *htn)
{
	const size_t node_index = htn_node_index(htn);

	return make_doffset(node_index + 1, 0);
}

static loff_t htn_resolve_doffset(const struct voluta_dir_htree_node *htn,
				  const struct voluta_dir_entry *de)
{
	const size_t index = htn_node_index(htn);
	const size_t sloti = de_slot(de, htn_begin(htn));

	return make_doffset(index, sloti);
}

static void htn_child_by_ord(const struct voluta_dir_htree_node *htn,
			     size_t ord, struct voluta_vaddr *out_vaddr)
{
	const loff_t off = htn_child(htn, ord);

	vaddr_of_htnode(out_vaddr, off);
}

static void htn_child_by_hash(const struct voluta_dir_htree_node *htn,
			      uint64_t hash, struct voluta_vaddr *out_vaddr)
{
	const size_t ord = hash_to_child_ord(hash, htn_depth(htn) + 1);

	htn_child_by_ord(htn, ord, out_vaddr);
}

static void htn_parent_addr(const struct voluta_dir_htree_node *htn,
			    struct voluta_vaddr *out_vaddr)
{
	vaddr_of_htnode(out_vaddr, htn_parent(htn));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static ino_t dir_ino_of(const struct voluta_vnode_info *vi)
{
	return htn_ino(vi->u.htn);
}

static mode_t dtype_of(const struct voluta_inode_info *ii)
{
	return IFTODT(i_mode_of(ii));
}

static int require_dir(const struct voluta_inode_info *ii)
{
	return i_isdir(ii) ? 0 : -ENOTDIR;
}

static int check_dir_io(const struct voluta_inode_info *ii)
{
	int err;

	err = require_dir(ii);
	if (err) {
		return err;
	}
	if (!i_refcnt_of(ii)) {
		return -EBADF;
	}
	return 0;
}

static struct voluta_inode *unconst_inode(const struct voluta_inode *inode)
{
	return (struct voluta_inode *)inode;
}

static struct voluta_inode_info *unconst_ii(const struct voluta_inode_info *ii)
{
	return (struct voluta_inode_info *)ii;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_dir_ispec *dis_of(const struct voluta_inode *inode)
{
	struct voluta_inode *dir_inode = unconst_inode(inode);

	return &dir_inode->i_s.d;
}

static uint64_t dis_ndents(const struct voluta_dir_ispec *dis)
{
	return le64_to_cpu(dis->d_ndents);
}

static void dis_set_ndents(struct voluta_dir_ispec *dis, size_t n)
{
	dis->d_ndents = cpu_to_le64(n);
}

static void dis_inc_ndents(struct voluta_dir_ispec *dis)
{
	dis_set_ndents(dis, dis_ndents(dis) + 1);
}

static void dis_dec_ndents(struct voluta_dir_ispec *dis)
{
	dis_set_ndents(dis, dis_ndents(dis) - 1);
}

static size_t dis_last_index(const struct voluta_dir_ispec *dis)
{
	return le32_to_cpu(dis->d_last_index);
}

static void dis_set_last_index(struct voluta_dir_ispec *dis, size_t index)
{
	dis->d_last_index = cpu_to_le32((uint32_t)index);
}

static void dis_update_last_index(struct voluta_dir_ispec *dis,
				  size_t alt_index, bool add)
{
	const size_t cur_index = dis_last_index(dis);

	if (add && (cur_index < alt_index)) {
		dis_set_last_index(dis, alt_index);
	} else if (!add && cur_index && (cur_index == alt_index)) {
		dis_set_last_index(dis, cur_index - 1);
	}
}

static loff_t dis_htree_root(const struct voluta_dir_ispec *dis)
{
	return off_to_cpu(dis->d_root);
}

static void dis_set_htree_root(struct voluta_dir_ispec *dis, loff_t off)
{
	dis->d_root = cpu_to_off(off);
}

static enum voluta_dir_flags dis_flags(struct voluta_dir_ispec *dis)
{
	return le32_to_cpu(dis->d_flags);
}

static void dis_set_flags(struct voluta_dir_ispec *dis,
			  enum voluta_dir_flags f)
{
	dis->d_flags = cpu_to_le32((uint32_t)f);
}

static void dis_setup(struct voluta_dir_ispec *dis)
{
	dis_set_htree_root(dis, VOLUTA_OFF_NULL);
	dis_set_last_index(dis, HTREE_INDEX_NULL);
	dis_set_ndents(dis, 0);
	dis_set_flags(dis, VOLUTA_DIRF_HASH_SHA256);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_dir_ispec *
dir_ispec_of(const struct voluta_inode_info *dir_ii)
{
	return dis_of(dir_ii->inode);
}

static void dir_setup(struct voluta_inode_info *dir_ii)
{
	struct voluta_iattr attr = {
		.ia_flags = VOLUTA_ATTR_SIZE | VOLUTA_ATTR_BLOCKS,
		.ia_size = VOLUTA_DIR_EMPTY_SIZE,
		.ia_blocks = 0
	};

	dis_setup(dis_of(dir_ii->inode));
	voluta_update_iattr(dir_ii, NULL, &attr);
}

static uint64_t dir_ndents(const struct voluta_inode_info *dir_ii)
{
	return dis_ndents(dir_ispec_of(dir_ii));
}

static void dir_inc_ndents(struct voluta_inode_info *dir_ii)
{
	dis_inc_ndents(dir_ispec_of(dir_ii));
	i_dirtify(dir_ii);
}

static void dir_dec_ndents(struct voluta_inode_info *dir_ii)
{
	dis_dec_ndents(dir_ispec_of(dir_ii));
	i_dirtify(dir_ii);
}

static loff_t dir_htree_root(const struct voluta_inode_info *dir_ii)
{
	return dis_htree_root(dir_ispec_of(dir_ii));
}

static void dir_htree_root_addr(const struct voluta_inode_info *dir_ii,
				struct voluta_vaddr *out_vaddr)
{
	vaddr_of_htnode(out_vaddr, dir_htree_root(dir_ii));
}

static bool dir_has_htree(const struct voluta_inode_info *dir_ii)
{
	return !off_isnull(dir_htree_root(dir_ii));
}

static void dir_set_htree_root(struct voluta_inode_info *dir_ii,
			       const struct voluta_vaddr *vaddr)
{
	struct voluta_dir_ispec *dis = dir_ispec_of(dir_ii);

	dis_set_htree_root(dis, vaddr->off);
	dis_set_last_index(dis, HTREE_INDEX_ROOT);
}

static size_t dir_last_index(const struct voluta_inode_info *dir_ii)
{
	return dis_last_index(dir_ispec_of(dir_ii));
}

static loff_t dir_recalc_isize(const struct voluta_inode_info *dir_ii)
{
	const size_t last_index = dir_last_index(dir_ii);

	return dir_has_htree(dir_ii) ?
	       make_doffset(last_index + 1, 0) : VOLUTA_DIR_EMPTY_SIZE;
}

static void dir_update_last_index(struct voluta_inode_info *dir_ii,
				  size_t alt_index, bool add)
{
	dis_update_last_index(dir_ispec_of(dir_ii), alt_index, add);
}

size_t voluta_dir_ndentries(const struct voluta_inode_info *dir_ii)
{
	return dir_ndents(dir_ii);
}

bool voluta_dir_hasflag(const struct voluta_inode_info *dir_ii,
			enum voluta_dir_flags mask)
{
	enum voluta_dir_flags flags;

	flags = dis_flags(dir_ispec_of(dir_ii));
	return ((flags & mask) == mask);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_vnode_info *unconst_vi(const struct voluta_vnode_info *vi)
{
	return (struct voluta_vnode_info *)vi;
}

static struct voluta_dir_entry *unconst_de(const struct voluta_dir_entry *de)
{
	return (struct voluta_dir_entry *)de;
}

static void dei_setup(struct voluta_dir_entry_info *dei,
		      const struct voluta_vnode_info *vi,
		      const struct voluta_dir_entry *de)
{
	dei->vi = unconst_vi(vi);
	dei->de = unconst_de(de);
	dei->ino_dt.ino = de_ino(de);
	dei->ino_dt.dt = de_dtype(de);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void child_addr_by_hash(const struct voluta_vnode_info *parent_vi,
			       uint64_t hash, struct voluta_vaddr *out_vaddr)
{
	htn_child_by_hash(parent_vi->u.htn, hash, out_vaddr);
}

static void child_addr_by_ord(const struct voluta_vnode_info *parent_vi,
			      size_t ord, struct voluta_vaddr *out_vaddr)
{
	htn_child_by_ord(parent_vi->u.htn, ord, out_vaddr);
}

static int search_htnode(const struct voluta_vnode_info *vi,
			 const struct voluta_qstr *name,
			 struct voluta_dir_entry_info *out_dei)
{
	const struct voluta_dir_entry *de;

	de = htn_search(vi->u.htn, name);
	if (de == NULL) {
		return -ENOENT;
	}
	dei_setup(out_dei, vi, de);
	return 0;
}

static int stage_htnode(const struct voluta_dir_ctx *dir_ctx,
			const struct voluta_vaddr *vaddr,
			struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi;

	if (vaddr_isnull(vaddr)) {
		return -ENOENT;
	}
	err = voluta_stage_vnode(dir_ctx->sbi, vaddr, &vi);
	if (err) {
		return err;
	}
	if (dir_ino_of(vi) != i_ino_of(dir_ctx->dir_ii)) {
		return -EFSCORRUPTED;
	}
	*out_vi = vi;
	return 0;
}

static int stage_child(const struct voluta_dir_ctx *dir_ctx,
		       const struct voluta_vaddr *vaddr,
		       struct voluta_vnode_info **out_vi)
{
	/* TODO: check depth */
	return stage_htnode(dir_ctx, vaddr, out_vi);
}

static int new_htnode(const struct voluta_dir_ctx *dir_ctx,
		      struct voluta_vnode_info **out_vi)
{
	return voluta_new_vnode(dir_ctx->sbi, VOLUTA_VTYPE_HTNODE, out_vi);
}

static int del_htnode(const struct voluta_dir_ctx *dir_ctx,
		      struct voluta_vnode_info *vi)
{
	return voluta_del_vnode(dir_ctx->sbi, vi);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t last_node_index_of(const struct voluta_dir_ctx *dir_ctx)
{
	return dir_last_index(dir_ctx->dir_ii);
}

static size_t curr_node_index_of(const struct voluta_dir_ctx *dir_ctx)
{
	return doffset_to_index(dir_ctx->readdir_ctx->pos);
}

static void update_size_blocks(const struct voluta_dir_ctx *dir_ctx,
			       size_t node_index, bool new_node)
{
	struct voluta_iattr attr;
	struct voluta_inode_info *dir_ii = dir_ctx->dir_ii;
	const blkcnt_t blkcnt = i_blocks_of(dir_ii);

	iattr_init(&attr, dir_ii);
	attr.ia_blocks = new_node ? blkcnt + 1 : blkcnt - 1;
	attr.ia_size = dir_recalc_isize(dir_ii);
	attr.ia_flags = VOLUTA_ATTR_SIZE | VOLUTA_ATTR_BLOCKS;
	i_update_attr(dir_ii, dir_ctx->opi, &attr);
	dir_update_last_index(dir_ii, node_index, new_node);
	i_dirtify(dir_ii);
}

static int create_htnode(const struct voluta_dir_ctx *dir_ctx,
			 const struct voluta_vaddr *parent, size_t node_index,
			 struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi;
	struct voluta_inode_info *dir_ii = dir_ctx->dir_ii;

	err = new_htnode(dir_ctx, &vi);
	if (err) {
		return err;
	}
	htn_setup(vi->u.htn, i_ino_of(dir_ii), node_index, parent->off);
	update_size_blocks(dir_ctx, node_index, true);
	v_dirtify(vi);
	*out_vi = vi;
	return 0;
}

static int stage_htree_root(struct voluta_dir_ctx *dir_ctx,
			    struct voluta_vnode_info **out_vi)
{
	struct voluta_vaddr vaddr;

	dir_htree_root_addr(dir_ctx->dir_ii, &vaddr);
	return stage_htnode(dir_ctx, &vaddr, out_vi);
}

static int create_htree_root(struct voluta_dir_ctx *dir_ctx,
			     struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vaddr vaddr;
	struct voluta_vnode_info *vi;

	vaddr_of_htnode(&vaddr, VOLUTA_OFF_NULL);
	err = create_htnode(dir_ctx, &vaddr, HTREE_INDEX_ROOT, &vi);
	if (err) {
		return err;
	}
	dir_set_htree_root(dir_ctx->dir_ii, v_vaddr_of(vi));
	i_dirtify(dir_ctx->dir_ii);

	*out_vi = vi;
	return 0;
}

static int stage_or_create_root(struct voluta_dir_ctx *dir_ctx,
				struct voluta_vnode_info **out_vi)
{
	int err;

	if (dir_has_htree(dir_ctx->dir_ii)) {
		err = stage_htree_root(dir_ctx, out_vi);
	} else {
		err = create_htree_root(dir_ctx, out_vi);
	}
	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int lookup_by_tree(struct voluta_dir_ctx *dir_ctx,
			  const struct voluta_vnode_info *root_vi,
			  struct voluta_dir_entry_info *dei)
{
	int err;
	size_t depth;
	struct voluta_vaddr vaddr;
	struct voluta_vnode_info *child_vi;
	const struct voluta_vnode_info *vi = root_vi;
	const struct voluta_qstr *name = dir_ctx->name;

	depth = htn_depth(vi->u.htn);
	while (depth_isvalid(depth)) {
		err = search_htnode(vi, name, dei);
		if (!err) {
			return 0;
		}
		if (err != -ENOENT) {
			return err;
		}
		child_addr_by_hash(vi, name->hash, &vaddr);
		if (vaddr_isnull(&vaddr)) {
			break;
		}
		err = stage_child(dir_ctx, &vaddr, &child_vi);
		if (err) {
			return err;
		}
		vi = child_vi;
		voluta_assert_eq(depth + 1, htn_depth(vi->u.htn));
		depth = htn_depth(vi->u.htn);
	}
	return -ENOENT;
}

static int lookup_by_name(struct voluta_dir_ctx *dir_ctx,
			  struct voluta_dir_entry_info *dei)
{
	int err = -ENOENT;
	struct voluta_vnode_info *root_vi;

	if (!dir_has_htree(dir_ctx->dir_ii)) {
		return -ENOENT;
	}
	err = stage_htree_root(dir_ctx, &root_vi);
	if (err) {
		return err;
	}
	err = lookup_by_tree(dir_ctx, root_vi, dei);
	if (err) {
		return err;
	}
	return 0;
}

static int check_dir_and_name(const struct voluta_dir_ctx *dir_ctx)
{
	if (!i_isdir(dir_ctx->dir_ii)) {
		return -ENOTDIR;
	}
	if (dir_ctx->name->str.len == 0) {
		return -EINVAL;
	}
	if (dir_ctx->name->str.len > VOLUTA_NAME_MAX) {
		return -ENAMETOOLONG;
	}
	return 0;
}

int voluta_resolve_by_name(struct voluta_sb_info *sbi,
			   const struct voluta_oper_info *opi,
			   const struct voluta_inode_info *dir_ii,
			   const struct voluta_qstr *name,
			   struct voluta_ino_dt *out_idt)
{
	int err;
	struct voluta_dir_entry_info dei;
	struct voluta_dir_ctx dir_ctx = {
		.sbi = sbi,
		.opi = opi,
		.dir_ii = unconst_ii(dir_ii),
		.name = name
	};

	err = check_dir_and_name(&dir_ctx);
	if (err) {
		return err;
	}
	err = lookup_by_name(&dir_ctx, &dei);
	if (err) {
		return err;
	}
	out_idt->ino = dei.ino_dt.ino;
	out_idt->dt = dei.ino_dt.dt;
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int create_child(const struct voluta_dir_ctx *dir_ctx,
			const struct voluta_vaddr *parent, size_t index,
			struct voluta_vnode_info **out_vi)
{
	int err;

	if (!index_valid_depth(index)) {
		return -ENOSPC;
	}
	err = create_htnode(dir_ctx, parent, index, out_vi);
	if (err) {
		return err;
	}
	v_dirtify(*out_vi);
	return 0;
}

static void set_parent_link(struct voluta_vnode_info *parent_vi,
			    const struct voluta_vaddr *vaddr, size_t index)
{
	const size_t child_ord = index_to_child_ord(index);

	htn_set_child(parent_vi->u.htn, child_ord, vaddr->off);
	v_dirtify(parent_vi);
}

static int create_link_child(const struct voluta_dir_ctx *dir_ctx,
			     struct voluta_vnode_info *parent_vi,
			     struct voluta_vnode_info **out_vi)
{
	int err;
	size_t parent_index;
	size_t child_index;
	struct voluta_vnode_info *vi;
	const struct voluta_qstr *name = dir_ctx->name;

	parent_index = htn_node_index(parent_vi->u.htn);
	child_index = hash_to_child_index(name->hash, parent_index);
	err = create_child(dir_ctx, v_vaddr_of(parent_vi), child_index, &vi);
	if (err) {
		return err;
	}
	set_parent_link(parent_vi, v_vaddr_of(vi), child_index);
	*out_vi = vi;
	return 0;
}

static int stage_or_create_child(const struct voluta_dir_ctx *dir_ctx,
				 struct voluta_vnode_info *parent_vi,
				 struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vaddr vaddr;
	const struct voluta_qstr *name = dir_ctx->name;

	child_addr_by_hash(parent_vi, name->hash, &vaddr);
	if (!vaddr_isnull(&vaddr)) {
		err = stage_child(dir_ctx, &vaddr, out_vi);
	} else {
		err = create_link_child(dir_ctx, parent_vi, out_vi);
	}
	return err;
}

static int discard_htnode(const struct voluta_dir_ctx *dir_ctx,
			  struct voluta_vnode_info *vi)
{
	int err;
	const size_t node_index = htn_node_index(vi->u.htn);

	voluta_assert_le(i_blocks_of(dir_ctx->dir_ii), HTREE_NODE_MAX);
	voluta_assert_ge(i_blocks_of(dir_ctx->dir_ii), 0);

	htn_reset_des(vi->u.htn);
	err = del_htnode(dir_ctx, vi);
	if (err) {
		return err;
	}
	update_size_blocks(dir_ctx, node_index, false);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void post_add_dentry(const struct voluta_dir_ctx *dir_ctx)
{
	struct voluta_iattr attr;
	struct voluta_inode_info *ii = dir_ctx->ii;
	struct voluta_inode_info *dir_ii = dir_ctx->dir_ii;

	iattr_init(&attr, ii);
	attr.ia_nlink = i_nlink_of(ii) + 1;
	attr.ia_parent_ino = i_ino_of(dir_ii);
	attr.ia_flags |= VOLUTA_ATTR_NLINK | VOLUTA_ATTR_PARENT;
	i_update_attr(ii, dir_ctx->opi, &attr);

	iattr_init(&attr, dir_ii);
	if (i_isdir(ii)) {
		attr.ia_nlink = i_nlink_of(dir_ii) + 1;
		attr.ia_flags |= VOLUTA_ATTR_NLINK;
	}
	i_update_attr(dir_ii, dir_ctx->opi, &attr);
}

static void post_remove_dentry(const struct voluta_dir_ctx *dir_ctx)
{
	struct voluta_iattr attr;
	struct voluta_inode_info *ii = dir_ctx->ii;
	struct voluta_inode_info *dir_ii = dir_ctx->dir_ii;

	iattr_init(&attr, ii);
	attr.ia_nlink = i_nlink_of(ii) - 1;
	attr.ia_flags |= VOLUTA_ATTR_NLINK;
	if (i_parent_ino_of(ii) == i_ino_of(dir_ii)) {
		attr.ia_parent_ino = VOLUTA_INO_NULL;
		attr.ia_flags |= VOLUTA_ATTR_PARENT;
	}
	i_update_attr(ii, dir_ctx->opi, &attr);

	iattr_init(&attr, dir_ii);
	if (i_isdir(ii)) {
		attr.ia_nlink = i_nlink_of(dir_ii) - 1;
		attr.ia_flags |= VOLUTA_ATTR_NLINK;
	}
	i_update_attr(dir_ii, dir_ctx->opi, &attr);
}

static int add_to_htnode(const struct voluta_dir_ctx *dir_ctx,
			 struct voluta_vnode_info *vi)
{
	const struct voluta_dir_entry *de;
	const struct voluta_inode_info *ii = dir_ctx->ii;

	de = htn_insert(vi->u.htn, dir_ctx->name, i_ino_of(ii), dtype_of(ii));
	if (de == NULL) {
		return -ENOSPC;
	}
	v_dirtify(vi);
	return 0;
}

static int add_to_tree(struct voluta_dir_ctx *dir_ctx,
		       struct voluta_vnode_info *root_vi)
{
	int err;
	size_t depth;
	struct voluta_vnode_info *vi = root_vi;

	depth = htn_depth(vi->u.htn);
	while (depth_isvalid(depth)) {
		err = add_to_htnode(dir_ctx, vi);
		if (!err) {
			return 0;
		}
		err = stage_or_create_child(dir_ctx, vi, &vi);
		if (err) {
			return err;
		}
		voluta_assert_eq(depth + 1, htn_depth(vi->u.htn));
		depth = htn_depth(vi->u.htn);
	}
	return -ENOSPC;
}

static int add_dentry(struct voluta_dir_ctx *dir_ctx)
{
	int err;
	struct voluta_vnode_info *root_vi;

	err = stage_or_create_root(dir_ctx, &root_vi);
	if (err) {
		return err;
	}
	err = add_to_tree(dir_ctx, root_vi);
	if (err) {
		return err;
	}
	dir_inc_ndents(dir_ctx->dir_ii);
	return 0;
}

static int check_add_dentry(const struct voluta_dir_ctx *dir_ctx)
{
	int err;
	size_t ndents;
	const struct voluta_inode_info *dir_ii = dir_ctx->dir_ii;

	err = check_dir_and_name(dir_ctx);
	if (err) {
		return err;
	}
	ndents = voluta_dir_ndentries(dir_ii);
	if (!(ndents < VOLUTA_DIR_ENTRIES_MAX)) {
		return -EMLINK;
	}
	/* Special case for remove-directory which is still held by open fd */
	/* TODO: FIXME */
	if (!i_nlink_of(dir_ii)) {
		return -ENOENT;
	}
	return 0;
}

int voluta_check_add_dentry(const struct voluta_inode_info *dir_ii,
			    const struct voluta_qstr *name)
{
	struct voluta_dir_ctx dir_ctx = {
		.dir_ii = unconst_ii(dir_ii),
		.name = name
	};

	return check_add_dentry(&dir_ctx);
}

int voluta_add_dentry(struct voluta_sb_info *sbi,
		      const struct voluta_oper_info *opi,
		      struct voluta_inode_info *dir_ii,
		      const struct voluta_qstr *name,
		      struct voluta_inode_info *ii)
{
	int err;
	struct voluta_dir_ctx dir_ctx = {
		.sbi = sbi,
		.opi = opi,
		.dir_ii = dir_ii,
		.ii = ii,
		.name = name,
	};

	err = check_add_dentry(&dir_ctx);
	if (err) {
		return err;
	}
	err = add_dentry(&dir_ctx);
	if (err) {
		return err;
	}
	post_add_dentry(&dir_ctx);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int require_raccess(const struct voluta_dir_ctx *dir_ctx)
{
	return voluta_do_access(dir_ctx->opi, dir_ctx->dir_ii, R_OK);
}

static int require_rxaccess(const struct voluta_dir_ctx *dir_ctx)
{
	return voluta_do_access(dir_ctx->opi, dir_ctx->dir_ii, R_OK | X_OK);
}

static int require_parent(struct voluta_dir_ctx *dir_ctx)
{
	int err;
	ino_t parent_ino;

	parent_ino = i_parent_ino_of(dir_ctx->dir_ii);
	if (!ino_isvalid(parent_ino)) {
		return -EFSCORRUPTED;
	}
	err = voluta_stage_inode(dir_ctx->sbi, parent_ino, &dir_ctx->parent_ii);
	if (err) {
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool index_inrange(const struct voluta_dir_ctx *dir_ctx, size_t index)
{
	const size_t last = last_node_index_of(dir_ctx);

	return (last != HTREE_INDEX_NULL) ? (index <= last) : false;
}

static bool inrange(const struct voluta_dir_ctx *dir_ctx)
{
	const loff_t doff = dir_ctx->readdir_ctx->pos;

	return (doff >= 0) && index_inrange(dir_ctx, doffset_to_index(doff));
}

static bool stopped(const struct voluta_dir_ctx *dir_ctx)
{
	return !dir_ctx->keep_iter;
}

static bool emit(struct voluta_dir_ctx *dir_ctx, const char *name,
		 size_t nlen, ino_t ino, mode_t dt)
{
	int err;
	struct voluta_readdir_ctx *readdir_ctx = dir_ctx->readdir_ctx;
	struct voluta_readdir_entry_info rdei = {
		.name = name,
		.name_len = nlen,
		.ino = ino,
		.dt = dt,
		.off = readdir_ctx->pos
	};

	err = readdir_ctx->actor(readdir_ctx, &rdei);
	dir_ctx->keep_iter = (err == 0);
	return dir_ctx->keep_iter;
}

static bool emit_dirent(struct voluta_dir_ctx *dir_ctx,
			const struct voluta_dir_entry *de, loff_t doff)
{
	dir_ctx->readdir_ctx->pos = doff;
	return emit(dir_ctx, de_name(de),
		    de_name_len(de), de_ino(de), de_dtype(de));
}

static bool emit_ii(struct voluta_dir_ctx *dir_ctx, const char *name,
		    size_t nlen, const struct voluta_inode_info *ii)
{
	const ino_t xino = i_xino_of(ii);
	const mode_t mode = i_mode_of(ii);

	return emit(dir_ctx, name, nlen, xino, IFTODT(mode));
}

static int iterate_htnode(struct voluta_dir_ctx *dir_ctx,
			  const struct voluta_vnode_info *vi)
{
	bool ok;
	loff_t off;
	const struct voluta_dir_entry *de = NULL;

	while (!stopped(dir_ctx)) {
		de = htn_scan(vi->u.htn, de, dir_ctx->readdir_ctx->pos);
		if (!de) {
			off = htn_next_doffset(vi->u.htn);
			dir_ctx->readdir_ctx->pos = off;
			break;
		}
		off = htn_resolve_doffset(vi->u.htn, de);
		ok = emit_dirent(dir_ctx, de, off);
		if (!ok) {
			break;
		}
		dir_ctx->readdir_ctx->pos += 1;
	}
	return 0;
}

static int readdir_eos(struct voluta_dir_ctx *dir_ctx)
{
	dir_ctx->readdir_ctx->pos = -1;
	emit(dir_ctx, "", 0, VOLUTA_INO_NULL, 0);

	dir_ctx->keep_iter = false;
	return 0;
}

static int stage_child_by_ord(struct voluta_dir_ctx *dir_ctx,
			      const struct voluta_vnode_info *vi, size_t ord,
			      struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vaddr vaddr;

	if (ord >= HTREE_FANOUT) {
		return -ENOENT;
	}
	child_addr_by_ord(vi, ord, &vaddr);
	err = stage_htnode(dir_ctx, &vaddr, out_vi);
	if (err) {
		return err;
	}
	return 0;
}

static int stage_htnode_at(struct voluta_dir_ctx *dir_ctx,
			   const struct voluta_vnode_info *root_vi,
			   size_t index, struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi;
	const size_t depth = index_to_depth(index);
	size_t child_ord[HTREE_DEPTH_MAX];

	voluta_assert_lt(depth, ARRAY_SIZE(child_ord));
	if (depth >= ARRAY_SIZE(child_ord)) {
		return -ENOENT;
	}
	for (size_t d = depth; d > 0; --d) {
		child_ord[d - 1] = index_to_child_ord(index);
		index = index_to_parent(index);
	}
	vi = unconst_vi(root_vi);
	for (size_t i = 0; i < depth; ++i) {
		err = stage_child_by_ord(dir_ctx, vi, child_ord[i], &vi);
		if (err) {
			return err;
		}
	}
	*out_vi = vi;
	return 0;
}

static int next_htnode(struct voluta_dir_ctx *dir_ctx,
		       const struct voluta_vnode_info *vi, size_t *out_index)
{
	int err;
	size_t ord;
	size_t next_index;
	size_t curr_index;
	struct voluta_vaddr vaddr;
	struct voluta_vnode_info *parent_vi;

	curr_index = htn_node_index(vi->u.htn);
	next_index = curr_index + 1;

	htn_parent_addr(vi->u.htn, &vaddr);
	if (vaddr_isnull(&vaddr)) {
		*out_index = next_index;
		return 0;
	}
	err = stage_htnode(dir_ctx, &vaddr, &parent_vi);
	if (err) {
		return err;
	}
	ord = htn_child_ord(vi->u.htn);
	while (++ord < HTREE_FANOUT) {
		child_addr_by_ord(parent_vi, ord, &vaddr);
		if (!vaddr_isnull(&vaddr)) {
			break;
		}
		next_index += 1;
	}
	*out_index = next_index;
	return 0;
}

static int iterate_htnodes(struct voluta_dir_ctx *dir_ctx,
			   const struct voluta_vnode_info *root_vi)
{
	int err = 0;
	size_t index;
	struct voluta_vnode_info *vi = NULL;

	index = curr_node_index_of(dir_ctx);
	while (!err && !stopped(dir_ctx)) {
		if (!index_inrange(dir_ctx, index)) {
			return readdir_eos(dir_ctx);
		}
		err = stage_htnode_at(dir_ctx, root_vi, index, &vi);
		if (err == -ENOENT) {
			index++;
			err = 0;
			continue;
		}
		if (err) {
			break;
		}
		err = iterate_htnode(dir_ctx, vi);
		if (err) {
			break;
		}
		err = next_htnode(dir_ctx, vi, &index);
	}
	return err;
}

static int iterate_htree(struct voluta_dir_ctx *dir_ctx)
{
	int err;
	struct voluta_vnode_info *root_vi;
	const size_t start_index = curr_node_index_of(dir_ctx);

	if (!index_inrange(dir_ctx, start_index)) {
		return readdir_eos(dir_ctx);
	}
	err = stage_htree_root(dir_ctx, &root_vi);
	if (err) {
		return err;
	}
	err = iterate_htnodes(dir_ctx, root_vi);
	if (err) {
		return err;
	}
	return 0;
}

static int iterate_dir(struct voluta_dir_ctx *dir_ctx)
{
	int err;

	if (!dir_has_htree(dir_ctx->dir_ii)) {
		return readdir_eos(dir_ctx);
	}
	err = iterate_htree(dir_ctx);
	if (err || stopped(dir_ctx)) {
		return err;
	}
	return readdir_eos(dir_ctx);
}

static int readdir_emit(struct voluta_dir_ctx *dir_ctx)
{
	int err = 0;
	bool ok = true;
	struct voluta_iattr attr;

	if (dir_ctx->readdir_ctx->pos == 0) {
		ok = emit_ii(dir_ctx, ".", 1, dir_ctx->dir_ii);
		dir_ctx->readdir_ctx->pos = 1;
	}
	if (ok && (dir_ctx->readdir_ctx->pos == 1)) {
		ok = emit_ii(dir_ctx, "..", 2, dir_ctx->parent_ii);
		dir_ctx->readdir_ctx->pos = 2;
	}
	if (ok && !inrange(dir_ctx)) {
		err = readdir_eos(dir_ctx);
	}
	if (ok && !stopped(dir_ctx)) {
		err = iterate_dir(dir_ctx);
	}

	iattr_init(&attr, dir_ctx->dir_ii);
	attr.ia_flags |= VOLUTA_ATTR_ATIME | VOLUTA_ATTR_LAZY;
	i_update_attr(dir_ctx->dir_ii, dir_ctx->opi, &attr);

	return err;
}

int voluta_do_readdir(struct voluta_sb_info *sbi,
		      const struct voluta_oper_info *opi,
		      const struct voluta_inode_info *dir_ii,
		      struct voluta_readdir_ctx *readdir_ctx)
{
	int err;
	struct voluta_dir_ctx dir_ctx = {
		.sbi = sbi,
		.opi = opi,
		.readdir_ctx = readdir_ctx,
		.dir_ii = unconst_ii(dir_ii),
		.keep_iter = true
	};

	err = check_dir_io(dir_ii);
	if (err) {
		return err;
	}
	err = require_rxaccess(&dir_ctx);
	if (err) {
		return err;
	}
	err = require_parent(&dir_ctx);
	if (err) {
		return err;
	}
	err = readdir_emit(&dir_ctx);
	if (err) {
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int discard_recursively(struct voluta_dir_ctx *dir_ctx,
			       const struct voluta_vaddr *vaddr)
{
	int err;
	struct voluta_vnode_info *vi;
	struct voluta_vaddr child_vaddr;

	if (vaddr_isnull(vaddr)) {
		return 0;
	}
	err = stage_htnode(dir_ctx, vaddr, &vi);
	if (err) {
		return err;
	}
	for (size_t ord = 0; ord < HTREE_FANOUT; ++ord) {
		htn_child_by_ord(vi->u.htn, ord, &child_vaddr);
		err = discard_recursively(dir_ctx, &child_vaddr);
		if (err) {
			return err;
		}
	}
	err = discard_htnode(dir_ctx, vi);
	if (err) {
		return err;
	}
	return 0;
}

static int drop_htree(struct voluta_dir_ctx *dir_ctx,
		      struct voluta_vnode_info *root_vi)
{
	int err = 0;
	struct voluta_vaddr child_vaddr;

	for (size_t ord = 0; ord < HTREE_FANOUT; ++ord) {
		htn_child_by_ord(root_vi->u.htn, ord, &child_vaddr);
		err = discard_recursively(dir_ctx, &child_vaddr);
		if (err) {
			break;
		}
	}
	err = discard_htnode(dir_ctx, root_vi);
	if (err) {
		return err;
	}
	return err;
}

static int finalize_htree(struct voluta_dir_ctx *dir_ctx)
{
	int err;
	struct voluta_vnode_info *root_vi;
	struct voluta_inode_info *dir_ii = dir_ctx->dir_ii;

	err = stage_htree_root(dir_ctx, &root_vi);
	if (err) {
		return err;
	}
	err = drop_htree(dir_ctx, root_vi);
	if (err) {
		return err;
	}
	dir_setup(dir_ii);
	i_dirtify(dir_ii);
	return 0;
}

int voluta_drop_dir(struct voluta_sb_info *sbi,
		    struct voluta_inode_info *dir_ii)
{
	struct voluta_dir_ctx dir_ctx = {
		.sbi = sbi,
		.dir_ii = dir_ii,
	};

	if (!dir_has_htree(dir_ii)) {
		return 0;
	}
	return finalize_htree(&dir_ctx);
}


/*
 * TODO-0005: Squeeze htree
 *
 * Currently, we drop htree only when its empty. Try to squeeze it up and
 * remove empty leaf nodes.
 */
static int remove_dentry(struct voluta_dir_ctx *dir_ctx,
			 const struct voluta_dir_entry_info *dei)
{
	struct voluta_inode_info *dir_ii = dir_ctx->dir_ii;

	voluta_assert_not_null(dei->vi);
	de_remove(dei->de, htn_end(dei->vi->u.htn));
	v_dirtify(dei->vi);

	dir_dec_ndents(dir_ii);

	return !dir_ndents(dir_ii) ? finalize_htree(dir_ctx) : 0;
}

int voluta_remove_dentry(struct voluta_sb_info *sbi,
			 const struct voluta_oper_info *opi,
			 struct voluta_inode_info *dir_ii,
			 const struct voluta_qstr *name)
{
	int err;
	struct voluta_dir_entry_info dei;
	struct voluta_dir_ctx dir_ctx = {
		.sbi = sbi,
		.opi = opi,
		.dir_ii = unconst_ii(dir_ii),
		.ii = NULL,
		.name = name
	};

	err = check_dir_and_name(&dir_ctx);
	if (err) {
		return err;
	}
	err = lookup_by_name(&dir_ctx, &dei);
	if (err) {
		return err;
	}
	err = voluta_stage_inode(dir_ctx.sbi, dei.ino_dt.ino, &dir_ctx.ii);
	if (err) {
		return err;
	}
	err = remove_dentry(&dir_ctx, &dei);
	if (err) {
		return err;
	}
	post_remove_dentry(&dir_ctx);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_do_opendir(struct voluta_sb_info *sbi,
		      const struct voluta_oper_info *opi,
		      struct voluta_inode_info *dir_ii)
{
	int err;
	struct voluta_dir_ctx dir_ctx = {
		.sbi = sbi,
		.opi = opi,
		.dir_ii = dir_ii,
	};

	err = require_dir(dir_ii);
	if (err) {
		return err;
	}
	err = require_raccess(&dir_ctx);
	if (err) {
		return err;
	}
	err = voluta_inc_fileref(opi, dir_ii);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_do_releasedir(struct voluta_sb_info *sbi,
			 const struct voluta_oper_info *opi,
			 struct voluta_inode_info *dir_ii)
{
	int err;

	err = check_dir_io(dir_ii);
	if (err) {
		return err;
	}
	err = voluta_do_access(opi, dir_ii, F_OK); /* XXX */
	if (err) {
		return err;
	}
	dir_ii->vi.ce.ce_refcnt--;

	/* TODO: Shrink sparse dir-tree upon last close */
	voluta_unused(sbi);

	return 0;
}

int voluta_do_fsyncdir(struct voluta_sb_info *sbi,
		       const struct voluta_oper_info *opi,
		       struct voluta_inode_info *dir_ii, bool dsync)
{
	int err;

	err = check_dir_io(dir_ii);
	if (err) {
		return err;
	}
	/* TODO */
	voluta_unused(sbi);
	voluta_unused(opi);
	voluta_unused(dsync);
	(void)sbi;
	(void)dsync;
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_setup_new_dir(struct voluta_inode_info *dir_ii)
{
	struct voluta_iattr attr = {
		.ia_flags = VOLUTA_ATTR_NLINK,
		.ia_nlink = 1
	};

	dir_setup(dir_ii);
	voluta_update_iattr(dir_ii, NULL, &attr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int verify_node_index(size_t node_index, bool has_tree)
{
	int err;

	if (has_tree) {
		err = index_isvalid(node_index) ? 0 : -EFSCORRUPTED;
	} else {
		err = index_isnull(node_index) ? 0 : -EFSCORRUPTED;
	}
	return err;
}

static int verify_dir_root(const struct voluta_inode *inode)
{
	int err;
	loff_t root_off;
	const struct voluta_dir_ispec *dis = dis_of(inode);

	root_off = dis_htree_root(dis);
	err = voluta_verify_off(root_off);
	if (err) {
		return err;
	}
	err = verify_node_index(dis_last_index(dis), !off_isnull(root_off));
	if (err) {
		return err;
	}
	return 0;
}

int voluta_verify_dir_inode(const struct voluta_inode *inode)
{
	int err;

	/* TODO: Check more */
	err = verify_dir_root(inode);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_verify_dir_htree_node(const struct voluta_dir_htree_node *htn)
{
	int err;

	err = voluta_verify_ino(htn_ino(htn));
	if (err) {
		return err;
	}
	err = voluta_verify_off(htn_parent(htn));
	if (err) {
		return err;
	}
	err = verify_node_index(htn_node_index(htn), true);
	if (err) {
		return err;
	}
	for (size_t i = 0; i < ARRAY_SIZE(htn->dh_child); ++i) {
		err = voluta_verify_off(htn_child(htn, i));
		if (err) {
			return err;
		}
	}
	return 0;
}
