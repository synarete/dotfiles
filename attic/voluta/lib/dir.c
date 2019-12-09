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
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include "voluta-lib.h"


struct voluta_packed_aligned voluta_dirent_view {
	struct voluta_dir_entry de;
	uint8_t  de_name[VOLUTA_NAME_MAX + 1];
};

struct voluta_dirent_info {
	struct voluta_vnode_info *vi;
	struct voluta_dir_entry *de;
	const struct voluta_dir_entry *de_last;
	struct voluta_ino_dt ino_dt;
};

struct voluta_dir_tnode_info {
	const struct voluta_inode_info *dir_ii;
	struct voluta_dir_tlink *parent_dtl;
	struct voluta_vnode_info *parent_vi;
	struct voluta_vaddr parent_vaddr;
	struct voluta_vaddr tnode_vaddr;
	loff_t  node_index;
	size_t  tree_depth;
	size_t  child_ord;
	loff_t  base_doff;
};

struct voluta_dir_ctx {
	struct voluta_env *env;
	struct voluta_sb_info *sbi;
	struct voluta_inode_info  *dir_ii;
	struct voluta_inode_info  *parent_dir_ii;
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

static loff_t dir_i_size_of(blkcnt_t nbk)
{
	const size_t dh_size = sizeof(struct voluta_dir_inode);
	const size_t dtn_size = sizeof(struct voluta_dir_tnode);

	return (loff_t)dh_size + (nbk * (loff_t)dtn_size);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t hash_to_child_ord(const struct voluta_hash256 *hash, size_t depth)
{
	size_t child_ord, dh1, dh2;
	const size_t fanout = VOLUTA_DIRNODE_NCHILDS;

	VOLUTA_STATICASSERT_LT(VOLUTA_DIRDEPTH_MAX, ARRAY_SIZE(hash->hash));

	voluta_assert_gt(depth, 0);
	voluta_assert_le(depth, VOLUTA_DIRDEPTH_MAX);
	voluta_assert_lt(depth, ARRAY_SIZE(hash->hash));

	dh1 = hash->hash[2 * depth - 1];
	dh2 = hash->hash[2 * depth - 2];
	child_ord = (dh1 ^ dh2) % fanout;

	return child_ord;
}

static loff_t dtnode_index_to_offset(loff_t node_index)
{
	loff_t doff = 0;
	const loff_t nents_per_head = VOLUTA_DIRHEAD_NENTS;
	const loff_t nents_per_node = VOLUTA_DIRNODE_NENTS;

	if (node_index > 0) {
		doff = nents_per_head + ((node_index - 1) * nents_per_node);
	}
	return doff;
}

static loff_t offset_to_dtnode_index(loff_t doff)
{
	loff_t node_index = 0;
	const loff_t nents_per_head = VOLUTA_DIRHEAD_NENTS;
	const loff_t nents_per_node = VOLUTA_DIRNODE_NENTS;

	voluta_assert_ge(doff, 0);

	if (doff >= nents_per_head) {
		node_index = ((doff - nents_per_head) / nents_per_node) + 1;
	}
	return node_index;
}

static void dtnode_index_to_parent(loff_t node_index,
				   loff_t *parent_index, size_t *child_ord)
{
	const loff_t fanout = VOLUTA_DIRNODE_NCHILDS;

	*parent_index = (node_index - 1) / fanout;
	*child_ord = (size_t)(node_index - (*parent_index * fanout) - 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_dirent_view *
dirent_view_of(const struct voluta_dir_entry *de)
{
	return voluta_container_of(de, struct voluta_dirent_view, de);
}

static bool dir_entry_isvalid(const struct voluta_dir_entry *de)
{
	return (safe_dttoif(de->de_dtype) != 0);
}

static bool dir_entry_isactive(const struct voluta_dir_entry *de)
{
	return (de->de_nlen > 0) && ino_isvalid(de->de_ino);
}

static const char *dir_entry_name(const struct voluta_dir_entry *de)
{
	const struct voluta_dirent_view *dview = dirent_view_of(de);

	return (const char *)(dview->de_name);
}

static bool dir_entry_has_name(const struct voluta_dir_entry *de,
			       const struct voluta_qstr *name)
{
	int cmp;
	const char *de_name;

	if (!dir_entry_isactive(de)) {
		return false;
	}
	if (name->str.len != de->de_nlen) {
		return false;
	}
	de_name = dir_entry_name(de);
	cmp = memcmp(de_name, name->str.str, de->de_nlen);
	return (cmp == 0);
}

static size_t nlen_to_nents(size_t nlen)
{
	const size_t ent_size = sizeof(struct voluta_dir_entry);
	const size_t name_nents = (nlen + ent_size - 1) / ent_size;

	return 1 + name_nents;
}

static size_t dir_entry_nents(const struct voluta_dir_entry *de)
{
	return de->de_nents;
}

static void dir_entry_set_nents(struct voluta_dir_entry *de, size_t nents)
{
	voluta_assert_le(nents, VOLUTA_DIRNODE_NENTS);
	voluta_assert_gt(nents, 0);

	de->de_nents = (uint16_t)nents;
}

static size_t dir_entry_nprev(const struct voluta_dir_entry *de)
{
	voluta_assert_lt(de->de_nprev, VOLUTA_DIRNODE_NENTS);

	return de->de_nprev;
}

static void dir_entry_set_nprev(struct voluta_dir_entry *de, size_t nprev)
{
	voluta_assert_lt(nprev, VOLUTA_DIRNODE_NENTS);

	de->de_nprev = (uint16_t)nprev;
}

static struct voluta_dir_entry *
dir_entry_next(const struct voluta_dir_entry *de)
{
	const size_t step = dir_entry_nents(de);

	voluta_assert_gt(step, 0);
	return (struct voluta_dir_entry *)(de + step);
}

static struct voluta_dir_entry *
dir_entry_next_safe(const struct voluta_dir_entry *de,
		    const struct voluta_dir_entry *end)
{
	struct voluta_dir_entry *next = dir_entry_next(de);

	voluta_assert(next <= end);
	return (next < end) ? next : NULL;
}

static struct voluta_dir_entry *
dir_entry_prev_safe(const struct voluta_dir_entry *de)
{
	const size_t step = dir_entry_nprev(de);

	return step ? (struct voluta_dir_entry *)(de - step) : NULL;
}

static void
dir_entry_assign(struct voluta_dir_entry *de, size_t nents,
		 const struct voluta_qstr *name, ino_t ino, mode_t dt)
{
	struct voluta_dirent_view *dview = dirent_view_of(de);

	de->de_ino = ino;
	de->de_nlen = (uint16_t)name->str.len;
	de->de_dtype = (uint8_t)dt;
	dir_entry_set_nents(de, nents);
	memcpy(dview->de_name, name->str.str, de->de_nlen);
}

static void dir_entry_reset(struct voluta_dir_entry *de,
			    size_t nents, size_t nprev)
{
	voluta_assert_gt(nents, 0);

	memset(de + 1, 0, (nents - 1) * sizeof(*de));
	de->de_ino = VOLUTA_INO_NULL;
	de->de_nlen = 0;
	de->de_dtype = 0;
	dir_entry_set_nents(de, nents);
	dir_entry_set_nprev(de, nprev);
}

static struct voluta_dir_entry *
dir_entry_unconst(const struct voluta_dir_entry *de)
{
	return (struct voluta_dir_entry *)de;
}

static loff_t
dir_entry_doffset(const struct voluta_dir_entry *de,
		  const struct voluta_dir_entry *beg, loff_t base_off)
{
	const loff_t doff = (loff_t)(de - beg);

	voluta_assert_ge(de, beg);
	voluta_assert_le(doff, VOLUTA_DIRNODE_NENTS);

	return base_off + doff;
}

static const struct voluta_dir_entry *
dir_entry_search(const struct voluta_dir_entry *de,
		 const struct voluta_dir_entry *end,
		 const struct voluta_qstr *name)
{
	const struct voluta_dir_entry *itr;

	for (itr = de; itr < end; itr = dir_entry_next(itr)) {
		if (dir_entry_has_name(itr, name)) {
			return itr;
		}
	}
	return NULL;
}

static const struct voluta_dir_entry *
dir_entry_scan(const struct voluta_dir_entry *de,
	       const struct voluta_dir_entry *end,
	       loff_t base_offset, loff_t pos)
{
	loff_t doff;
	const struct voluta_dir_entry *itr;

	for (itr = de; itr < end; itr = dir_entry_next(itr)) {
		if (dir_entry_isactive(itr)) {
			voluta_assert(dir_entry_isvalid(itr));
			doff = dir_entry_doffset(itr, de, base_offset);
			if (doff >= pos) {
				return itr;
			}
		}
	}
	return NULL;
}

static struct voluta_dir_entry *
dir_entry_insert_at(struct voluta_dir_entry *de,
		    const struct voluta_dir_entry *end, size_t nents,
		    const struct voluta_qstr *name, ino_t ino, mode_t dt)
{
	size_t ncurr, nents_new;
	struct voluta_dir_entry *next_new, *next_old;

	ncurr = dir_entry_nents(de);
	voluta_assert(nents <= ncurr);

	next_old = dir_entry_next_safe(de, end);

	dir_entry_assign(de, nents, name, ino, dt);
	if (nents < ncurr) {
		nents_new = ncurr - nents;
		next_new = dir_entry_next(de);
		dir_entry_reset(next_new, nents_new, nents);
		if (next_old != NULL) {
			voluta_assert(dir_entry_isactive(next_old));
			dir_entry_set_nprev(next_old, nents_new);
		}
	}
	return de;
}

static struct voluta_dir_entry *
dir_entry_insert(struct voluta_dir_entry *beg,
		 const struct voluta_dir_entry *end,
		 const struct voluta_qstr *name, ino_t ino, mode_t dt)
{
	size_t nwant, nents;
	struct voluta_dir_entry *itr = beg, *res = NULL;

	nwant = nlen_to_nents(name->str.len);
	while ((itr < end) && !res) {
		nents = dir_entry_nents(itr);
		if (!dir_entry_isactive(itr) && (nwant <= nents)) {
			res = dir_entry_insert_at(itr, end,
						  nwant, name, ino, dt);
		}
		itr = dir_entry_next(itr);
	}
	return res;
}

static void dir_entry_remove(struct voluta_dir_entry *de,
			     const struct voluta_dir_entry *end)
{
	size_t nents = dir_entry_nents(de);
	size_t nents_prev = dir_entry_nprev(de);
	struct voluta_dir_entry *next, *prev, *dent = de;

	next = dir_entry_next_safe(de, end);
	if (next != NULL) {
		if (!dir_entry_isactive(next)) {
			nents += dir_entry_nents(next);
			next = dir_entry_next_safe(next, end);
		}
	}
	prev = dir_entry_prev_safe(de);
	if (prev != NULL) {
		if (!dir_entry_isactive(prev)) {
			nents += dir_entry_nents(prev);
			nents_prev = dir_entry_nprev(prev);
			dent = prev;
		}
	}

	dir_entry_reset(dent, nents, nents_prev);
	if (next != NULL) {
		dir_entry_set_nprev(next, nents);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void dir_tlink_reset(struct voluta_dir_tlink *dtl)
{
	dtl->dl_parent_off = VOLUTA_OFF_NULL;
	dtl->dl_base_doff = 0;
	dtl->dl_node_index = -1;
	dtl->dl_tree_depth = 0;
	dtl->dl_child_ord = 0;
	dtl->dl_flags = 0;
	for (size_t i = 0; i < ARRAY_SIZE(dtl->dl_child_off); ++i) {
		dtl->dl_child_off[i] = VOLUTA_OFF_NULL;
	}
}

static void dir_tlink_setup(struct voluta_dir_tlink *dtl,
			    const struct voluta_dir_tnode_info *dti)
{
	dir_tlink_reset(dtl);
	dtl->dl_parent_off = dti->parent_vaddr.off;
	dtl->dl_base_doff = dti->base_doff;
	dtl->dl_node_index = (int32_t)dti->node_index;
	dtl->dl_tree_depth = (uint16_t)dti->tree_depth;
	dtl->dl_child_ord = (uint8_t)dti->child_ord;
}

static void dir_tlink_set_child(struct voluta_dir_tlink *dtl,
				size_t child_ord, loff_t child_off)
{
	voluta_assert_lt(child_ord, ARRAY_SIZE(dtl->dl_child_off));
	voluta_assert_eq(dtl->dl_child_off[child_ord],
			 VOLUTA_OFF_NULL);
	voluta_assert_ne(child_off, VOLUTA_OFF_NULL);

	dtl->dl_child_off[child_ord] = child_off;
}

static loff_t
dir_tlink_get_child(const struct voluta_dir_tlink *dtl, size_t child_ord)
{
	voluta_assert_lt(child_ord, ARRAY_SIZE(dtl->dl_child_off));

	return dtl->dl_child_off[child_ord];
}

static loff_t
dir_tlink_child_index(const struct voluta_dir_tlink *dtl, size_t child_ord)
{
	const loff_t fanout = VOLUTA_DIRNODE_NCHILDS;

	voluta_assert_lt(dtl->dl_node_index, VOLUTA_DIRNODE_MAX);

	return (fanout * dtl->dl_node_index) + (loff_t)child_ord + 1;
}

static size_t dir_tlink_child_depth(const struct voluta_dir_tlink *dtl)
{
	const size_t node_depth = dtl->dl_tree_depth;

	return node_depth + 1;
}

static bool dir_tlink_isleaf(const struct voluta_dir_tlink *dtl)
{
	for (size_t i = 0; i < ARRAY_SIZE(dtl->dl_child_off); ++i) {
		if (!off_isnull(dtl->dl_child_off[i])) {
			return false;
		}
	}
	return true;
}

static loff_t dir_tlink_base_doff(const struct voluta_dir_tlink *dtl)
{
	return dtl->dl_base_doff;
}

static loff_t dir_tlink_node_index(const struct voluta_dir_tlink *dtl)
{
	return dtl->dl_node_index;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void reset_tnode_info(struct voluta_dir_tnode_info *dti,
			     const struct voluta_inode_info *dir_ii)
{
	memset(dti, 0, sizeof(*dti));
	dti->dir_ii = dir_ii;
	vaddr_reset(&dti->parent_vaddr);
	vaddr_reset(&dti->tnode_vaddr);
}

static void dir_head_setup(struct voluta_dir_head *dh)
{
	struct voluta_dir_tnode_info dti;

	reset_tnode_info(&dti, NULL);
	dir_tlink_setup(&dh->d_tlink, &dti);
	dh->d_last_index = 0;
	dh->d_ndents = 0;
	dh->d_name_mask = 0;
	dh->d_flags = 0;
	dh->d_class = VOLUTA_DIRCLASS_V1;
	dir_entry_reset(dh->de, ARRAY_SIZE(dh->de), 0);
}

static struct voluta_dir_entry *
dir_head_begin(const struct voluta_dir_head *dh)
{
	return (struct voluta_dir_entry *)dh->de;
}

static const struct voluta_dir_entry *
dir_head_end(const struct voluta_dir_head *dh)
{
	return dh->de + ARRAY_SIZE(dh->de);
}

static const struct voluta_dir_entry *
dir_head_search(const struct voluta_dir_head *dh,
		const struct voluta_qstr *name)
{
	return dir_entry_search(dir_head_begin(dh), dir_head_end(dh), name);
}

static const struct voluta_dir_entry *
dir_head_scan(const struct voluta_dir_head *dh, loff_t pos)
{
	return dir_entry_scan(dir_head_begin(dh), dir_head_end(dh), 0, pos);
}

static struct voluta_dir_entry *
dir_head_insert(struct voluta_dir_head *dh,
		const struct voluta_qstr *name, ino_t ino, mode_t dt)
{
	return dir_entry_insert(dir_head_begin(dh),
				dir_head_end(dh), name, ino, dt);
}

static loff_t dir_head_next_offset(const struct voluta_dir_head *dh)
{
	return (loff_t)ARRAY_SIZE(dh->de);
}

static loff_t dir_head_resolve_offset(const struct voluta_dir_head *dh,
				      const struct voluta_dir_entry *de)
{
	return dir_entry_doffset(de, dir_head_begin(dh), 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void dir_tnode_setup(struct voluta_dir_tnode *dtn,
			    const struct voluta_dir_tnode_info *dti)
{
	voluta_assert(ino_isvalid(i_ino_of(dti->dir_ii)));
	voluta_assert_le(dti->node_index, VOLUTA_DIRNODE_MAX);

	dir_tlink_setup(&dtn->dt_tlink, dti);
	dtn->dt_ino = i_ino_of(dti->dir_ii);
	dir_entry_reset(dtn->de, ARRAY_SIZE(dtn->de), 0);
}

static struct voluta_dir_entry *
dir_tnode_begin(const struct voluta_dir_tnode *dtn)
{
	return (struct voluta_dir_entry *)dtn->de;
}

static const struct voluta_dir_entry *
dir_tnode_end(const struct voluta_dir_tnode *dtn)
{
	return dtn->de + ARRAY_SIZE(dtn->de);
}

static void dir_tnode_reset_dirents(struct voluta_dir_tnode *dtn)
{
	dir_entry_reset(dtn->de, ARRAY_SIZE(dtn->de), 0);
}

static const struct voluta_dir_entry *
dir_tnode_search(const struct voluta_dir_tnode *dtn,
		 const struct voluta_qstr *name)
{
	return dir_entry_search(dir_tnode_begin(dtn), dir_tnode_end(dtn), name);
}

static const struct voluta_dir_entry *
dir_tnode_scan(const struct voluta_dir_tnode *dtn, loff_t pos)
{
	return dir_entry_scan(dir_tnode_begin(dtn), dir_tnode_end(dtn),
			      dir_tlink_base_doff(&dtn->dt_tlink), pos);
}

static struct voluta_dir_entry *
dir_tnode_insert(struct voluta_dir_tnode *dtn,
		 const struct voluta_qstr *name, ino_t ino, mode_t dt)
{
	return dir_entry_insert(dir_tnode_begin(dtn),
				dir_tnode_end(dtn), name, ino, dt);
}

static loff_t dir_tnode_next_offset(const struct voluta_dir_tnode *dtn)
{
	loff_t tree_index, next_off, base_off;
	const loff_t nents_per_node = ARRAY_SIZE(dtn->de);
	const struct voluta_dir_tlink *tlink = &dtn->dt_tlink;

	base_off = dir_tlink_base_doff(tlink);
	tree_index = dir_tlink_node_index(tlink);
	next_off = dtnode_index_to_offset(tree_index + 1);
	voluta_assert_eq(base_off + nents_per_node, next_off);

	return base_off + nents_per_node;
}

static loff_t dir_tnode_resolve_offset(const struct voluta_dir_tnode *dtn,
				       const struct voluta_dir_entry *de)
{
	loff_t base_off;
	const struct voluta_dir_entry *beg = dir_tnode_begin(dtn);

	base_off = dir_tlink_base_doff(&dtn->dt_tlink);
	return dir_entry_doffset(de, beg, base_off);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_dir_inode *
inode_to_dirinode(const struct voluta_inode *inode)
{
	const struct voluta_dir_inode *di;

	di = voluta_container_of(inode, struct voluta_dir_inode, d_i);
	return (struct voluta_dir_inode *)di;
}

static struct voluta_dir_inode *
dir_inode_of(const struct voluta_inode_info *dir_ii)
{
	struct voluta_inode *inode = dir_ii->inode;

	return voluta_container_of(inode, struct voluta_dir_inode, d_i);
}

static struct voluta_dir_head *
dir_head_of(const struct voluta_inode_info *dir_ii)
{
	struct voluta_dir_inode *di = dir_inode_of(dir_ii);

	return &di->d_h;
}

static struct voluta_dir_tlink *
dir_tlink_of(const struct voluta_inode_info *dir_ii)
{
	struct voluta_dir_head *dh = dir_head_of(dir_ii);

	return &dh->d_tlink;
}

static struct voluta_dir_tlink *
dir_tlink_of_tnode(const struct voluta_vnode_info *vi)
{
	return &vi->u.dtn->dt_tlink;
}

static ino_t dir_ino_of(const struct voluta_vnode_info *vi)
{
	return vi->u.dtn->dt_ino;
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


static struct voluta_inode_info *unconst_ii(const struct voluta_inode_info *ii)
{
	return (struct voluta_inode_info *)ii;
}

static struct voluta_vnode_info *unconst_vi(const struct voluta_vnode_info *vi)
{
	return (struct voluta_vnode_info *)vi;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void dir_setup(struct voluta_inode_info *dir_ii)
{
	struct voluta_dir_inode *di = dir_inode_of(dir_ii);

	di->d_i.i_size = (uint64_t)dir_i_size_of(0);
	di->d_i.i_blocks = 0;
	dir_head_setup(&di->d_h);
}

size_t voluta_dir_ndentries(const struct voluta_inode_info *dir_ii)
{
	const struct voluta_dir_head *dh = dir_head_of(dir_ii);

	return dh->d_ndents;
}

static void dir_inc_ndents(struct voluta_inode_info *dir_ii)
{
	struct voluta_dir_head *dh = dir_head_of(dir_ii);

	dh->d_ndents += 1;
	i_dirtify(dir_ii);
}

static void dir_dec_ndents(struct voluta_inode_info *dir_ii)
{
	struct voluta_dir_head *dh = dir_head_of(dir_ii);

	voluta_assert_ge(dh->d_ndents, 0);
	dh->d_ndents -= 1;
	i_dirtify(dir_ii);
}

static bool dir_has_tree(const struct voluta_inode_info *dir_ii)
{
	return !dir_tlink_isleaf(dir_tlink_of(dir_ii));
}

static loff_t dir_last_index(const struct voluta_inode_info *dir_ii)
{
	const struct voluta_dir_inode *di = dir_inode_of(dir_ii);

	return di->d_h.d_last_index;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void dirent_info_setup(struct voluta_dirent_info *dei,
			      const struct voluta_vnode_info *vi,
			      const struct voluta_dir_entry *de,
			      const struct voluta_dir_entry *de_last)
{
	dei->vi = unconst_vi(vi);
	dei->de = dir_entry_unconst(de);
	dei->de_last = de_last;
	dei->ino_dt.ino = de->de_ino;
	dei->ino_dt.dt = de->de_dtype;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int search_dir_tnode(const struct voluta_vnode_info *vi,
			    const struct voluta_qstr *name,
			    struct voluta_dirent_info *out_dei)
{
	const struct voluta_dir_entry *de;

	de = dir_tnode_search(vi->u.dtn, name);
	if (de == NULL) {
		return -ENOENT;
	}
	dirent_info_setup(out_dei, vi, de, dir_tnode_end(vi->u.dtn));
	return 0;
}

static int stage_dir_tnode(const struct voluta_dir_ctx *dir_ctx,
			   const struct voluta_dir_tnode_info *dti,
			   struct voluta_vnode_info **out_vi)
{
	int err;
	ino_t dir_ino, ino;
	struct voluta_vnode_info *vi;
	const struct voluta_dir_tlink *dtl;

	if (vaddr_isnull(&dti->tnode_vaddr)) {
		return -ENOENT;
	}
	err = voluta_stage_vnode(dir_ctx->sbi, &dti->tnode_vaddr, &vi);
	if (err) {
		return err;
	}
	dir_ino = i_ino_of(dir_ctx->dir_ii);
	ino = dir_ino_of(vi);
	if (ino != dir_ino) {
		voluta_assert_eq(ino, dir_ino);
		return -EFSCORRUPTED;
	}

	dtl = dir_tlink_of_tnode(vi);
	/* XXX return -EFSCORRUPTED; */
	voluta_assert_eq(dtl->dl_tree_depth, dti->tree_depth);
	voluta_assert_eq(dtl->dl_node_index, dti->node_index);

	*out_vi = vi;
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void setup_tnode_vaddr_by(struct voluta_dir_tnode_info *dti,
				 const struct voluta_vnode_info *vi)
{
	vaddr_clone(v_vaddr_of(vi), &dti->tnode_vaddr);
}

static void setup_ref_vaddrs(struct voluta_dir_tnode_info *dti)
{
	loff_t tnode_off;
	const enum voluta_vtype vtype = VOLUTA_VTYPE_DTNODE;

	tnode_off = dir_tlink_get_child(dti->parent_dtl, dti->child_ord);
	voluta_vaddr_by_off(&dti->tnode_vaddr, vtype, tnode_off);

	if (dti->parent_vi != NULL) {
		vaddr_clone(v_vaddr_of(dti->parent_vi), &dti->parent_vaddr);
	}
}

static void setup_by_ord(struct voluta_dir_tnode_info *dti,
			 const struct voluta_inode_info *dir_ii,
			 unsigned long child_ord)
{
	reset_tnode_info(dti, dir_ii);
	dti->parent_dtl = dir_tlink_of(dir_ii);
	dti->parent_vi = NULL;
	dti->tree_depth = 1;
	dti->child_ord = child_ord;
	dti->node_index = dir_tlink_child_index(dti->parent_dtl, child_ord);
	dti->base_doff = dtnode_index_to_offset(dti->node_index);
	setup_ref_vaddrs(dti);
}

static void setup_by_hash(struct voluta_dir_tnode_info *dti,
			  const struct voluta_inode_info *dir_ii,
			  const struct voluta_hash256 *hash)
{
	size_t child_ord;

	child_ord = hash_to_child_ord(hash, 1);
	setup_by_ord(dti, dir_ii, child_ord);
}

static void update_by_ord(struct voluta_dir_tnode_info *dti,
			  struct voluta_vnode_info *parent_vi, size_t child_ord)
{
	vaddr_clone(v_vaddr_of(parent_vi), &dti->parent_vaddr);
	dti->parent_dtl = dir_tlink_of_tnode(parent_vi);
	dti->parent_vi = parent_vi;
	dti->tree_depth = dir_tlink_child_depth(dti->parent_dtl);
	dti->child_ord = child_ord;
	dti->node_index = dir_tlink_child_index(dti->parent_dtl,
						dti->child_ord);
	dti->base_doff = dtnode_index_to_offset(dti->node_index);
	setup_ref_vaddrs(dti);
}

static void update_by_hash(struct voluta_dir_tnode_info *dti,
			   struct voluta_vnode_info *parent_vi,
			   const struct voluta_hash256 *hash)
{
	size_t child_ord;
	const struct voluta_dir_tlink *dtl = dir_tlink_of_tnode(parent_vi);

	child_ord = hash_to_child_ord(hash, dtl->dl_tree_depth);
	update_by_ord(dti, parent_vi, child_ord);
}

static void assign_parent_link(const struct voluta_dir_tnode_info *dti)
{
	dir_tlink_set_child(dti->parent_dtl,
			    dti->child_ord, dti->tnode_vaddr.off);

	if (dti->parent_vi != NULL) {
		v_dirtify(dti->parent_vi);
	} else {
		i_dirtify(dti->dir_ii);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int lookup_by_tree(struct voluta_dir_ctx *dir_ctx,
			  struct voluta_dirent_info *dei)
{
	int err = -ENOENT;
	struct voluta_dir_tnode_info dti;
	struct voluta_vnode_info *vi;
	const struct voluta_qstr *name = dir_ctx->name;
	const size_t depth_max = VOLUTA_DIRDEPTH_MAX;

	setup_by_hash(&dti, dir_ctx->dir_ii, &name->hash);
	while (dti.tree_depth <= depth_max) {
		if (vaddr_isnull(&dti.tnode_vaddr)) {
			break;
		}
		err = stage_dir_tnode(dir_ctx, &dti, &vi);
		if (err) {
			break;
		}
		err = search_dir_tnode(vi, name, dei);
		if (!err || (err != -ENOENT)) {
			break;
		}
		update_by_hash(&dti, vi, &name->hash);
	}
	return err;
}

static int lookup_by_head(struct voluta_dir_ctx *dir_ctx,
			  struct voluta_dirent_info *dei)
{
	const struct voluta_dir_entry *de;
	const struct voluta_dir_head *dh = dir_head_of(dir_ctx->dir_ii);

	de = dir_head_search(dh, dir_ctx->name);
	if (de == NULL) {
		return -ENOENT;
	}
	dirent_info_setup(dei, NULL, de, dir_head_end(dh));
	return 0;
}

static int lookup_by_name(struct voluta_dir_ctx *dir_ctx,
			  struct voluta_dirent_info *dei)
{
	int err;

	err = lookup_by_head(dir_ctx, dei);
	if (!err || (err != -ENOENT)) {
		return err;
	}
	err = lookup_by_tree(dir_ctx, dei);
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

int voluta_resolve_by_name(struct voluta_env *env,
			   const struct voluta_inode_info *dir_ii,
			   const struct voluta_qstr *name,
			   struct voluta_ino_dt *out_idt)
{
	int err;
	struct voluta_dirent_info dei;
	struct voluta_dir_ctx dir_ctx = {
		.env = env,
		.sbi = sbi_of(env),
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

int voluta_lookup_by_dname(struct voluta_env *env,
			   const struct voluta_inode_info *dir_ii,
			   const struct voluta_qstr *name, ino_t *out_ino)
{
	int err;
	struct voluta_ino_dt ino_dt;

	err = voluta_resolve_by_name(env, dir_ii, name, &ino_dt);
	if (err) {
		return err;
	}
	if (ino_dt.dt != DT_DIR) {
		return -ENOTDIR;
	}
	*out_ino = ino_dt.ino;
	return 0;
}

int voluta_lookup_by_iname(struct voluta_env *env,
			   const struct voluta_inode_info *dir_ii,
			   const struct voluta_qstr *name, ino_t *out_ino)
{
	int err;
	struct voluta_ino_dt ino_dt;

	err = voluta_resolve_by_name(env, dir_ii, name, &ino_dt);
	if (err) {
		return err;
	}
	if (ino_dt.dt == DT_DIR) {
		return -EISDIR;
	}
	*out_ino = ino_dt.ino;
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Conversion of internal <--> external directory offsets: shift by two for
 * the special "." and ".." pseudo entries.
 */
static loff_t doff_xtoi(loff_t doff)
{
	return doff - 2;
}

static loff_t doff_itox(loff_t doff)
{
	return doff + 2;
}

static loff_t last_tindex_of(const struct voluta_dir_ctx *dir_ctx)
{
	return dir_last_index(dir_ctx->dir_ii);
}

static loff_t start_tindex_of(const struct voluta_dir_ctx *dir_ctx)
{
	loff_t doff, tree_index;

	doff = doff_xtoi(dir_ctx->readdir_ctx->pos);
	tree_index = offset_to_dtnode_index(doff);
	return tree_index;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int new_dir_tnode(const struct voluta_dir_ctx *dir_ctx,
			 struct voluta_vnode_info **out_vi)
{
	const enum voluta_vtype vtype = VOLUTA_VTYPE_DTNODE;

	return voluta_new_vnode(sbi_of(dir_ctx->env), vtype, out_vi);
}

static int del_dir_tnode(const struct voluta_dir_ctx *dir_ctx,
			 struct voluta_vnode_info *vi)
{
	return voluta_del_vnode(dir_ctx->sbi, vi);
}

static void update_dir_space(const struct voluta_dir_ctx *dir_ctx,
			     loff_t tree_index, bool new_node)
{
	loff_t last_index;
	struct voluta_iattr attr;
	struct voluta_inode_info *dir_ii = dir_ctx->dir_ii;
	struct voluta_dir_inode *di = dir_inode_of(dir_ii);

	attr_reset(&attr);
	last_index = last_tindex_of(dir_ctx);
	if (new_node) {
		attr.ia_blocks = i_blocks_of(dir_ii) + 1;
		if (last_index < tree_index) {
			di->d_h.d_last_index = tree_index;
		}
	} else {
		attr.ia_blocks = i_blocks_of(dir_ii) - 1;
		if (last_index == tree_index) {
			di->d_h.d_last_index -= 1;
		}
	}

	attr.ia_size = dir_i_size_of(dir_last_index(dir_ii));
	attr.ia_flags = VOLUTA_ATTR_BLOCKS | VOLUTA_ATTR_SIZE;
	i_update_attr(dir_ctx->env, dir_ii, &attr);
}

static int create_dir_tnode(const struct voluta_dir_ctx *dir_ctx,
			    const struct voluta_dir_tnode_info *dti,
			    struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi;
	struct voluta_inode_info *dir_ii = dir_ctx->dir_ii;

	voluta_assert_lt(i_blocks_of(dir_ii), VOLUTA_DIRNODE_MAX);

	err = new_dir_tnode(dir_ctx, &vi);
	if (err) {
		return err;
	}
	dir_tnode_setup(vi->u.dtn, dti);
	update_dir_space(dir_ctx, dti->node_index, true);

	v_dirtify(vi);
	*out_vi = vi;
	return 0;
}

static int discard_dir_tnode(const struct voluta_dir_ctx *dir_ctx,
			     struct voluta_vnode_info *vi)
{
	int err;
	loff_t node_index;

	voluta_assert_le(i_blocks_of(dir_ctx->dir_ii), VOLUTA_DIRNODE_MAX);
	voluta_assert_ge(i_blocks_of(dir_ctx->dir_ii), 0);

	node_index = dir_tlink_node_index(&vi->u.dtn->dt_tlink);
	dir_tnode_reset_dirents(vi->u.dtn);
	err = del_dir_tnode(dir_ctx, vi);
	if (err) {
		return err;
	}
	update_dir_space(dir_ctx, node_index, false);
	return 0;
}

static int create_dir_tnode_child(const struct voluta_dir_ctx *dir_ctx,
				  struct voluta_dir_tnode_info *dti,
				  struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi = NULL;

	err = create_dir_tnode(dir_ctx, dti, &vi);
	if (err) {
		return err;
	}
	setup_tnode_vaddr_by(dti, vi);
	assign_parent_link(dti);
	*out_vi = vi;
	return 0;
}

static int stage_or_create_child_at(const struct voluta_dir_ctx *dir_ctx,
				    struct voluta_dir_tnode_info *dti,
				    struct voluta_vnode_info **out_vi)
{
	int err;
	const size_t depth_max = VOLUTA_DIRDEPTH_MAX;

	if (!vaddr_isnull(&dti->tnode_vaddr)) {
		return stage_dir_tnode(dir_ctx, dti, out_vi);
	}
	if (dti->tree_depth >= depth_max) {
		return -ENOSPC;
	}
	err = create_dir_tnode_child(dir_ctx, dti, out_vi);
	if (err) {
		return err;
	}
	return 0;
}

static int try_add_dirent(struct voluta_vnode_info *vi,
			  const struct voluta_qstr *name,
			  const struct voluta_inode_info *ii)
{
	struct voluta_dir_entry *de;
	struct voluta_dirent_info dei;

	de = dir_tnode_insert(vi->u.dtn, name, i_ino_of(ii), dtype_of(ii));
	if (de == NULL) {
		return -ENOSPC;
	}
	dirent_info_setup(&dei, vi, de, dir_tnode_end(vi->u.dtn));
	v_dirtify(vi);
	return 0;
}

static int add_to_tree(struct voluta_dir_ctx *dir_ctx)
{
	int err;
	size_t depth;
	struct voluta_dir_tnode_info dti;
	struct voluta_vnode_info *vi;
	const struct voluta_qstr *name = dir_ctx->name;
	const size_t depth_max = VOLUTA_DIRDEPTH_MAX;

	setup_by_hash(&dti, dir_ctx->dir_ii, &name->hash);
	depth = dti.tree_depth;

	while (depth < depth_max) {
		err = stage_or_create_child_at(dir_ctx, &dti, &vi);
		if (err) {
			return err;
		}
		err = try_add_dirent(vi, name, dir_ctx->ii);
		if (!err) {
			return 0;
		}
		update_by_hash(&dti, vi, &name->hash);
		voluta_assert_eq(depth + 1, dti.tree_depth);
		depth = dti.tree_depth;
	}
	return -ENOSPC;
}

static void post_add_dentry(const struct voluta_dir_ctx *dir_ctx)
{
	struct voluta_iattr attr;
	struct voluta_inode_info *ii = dir_ctx->ii;
	struct voluta_inode_info *dir_ii = dir_ctx->dir_ii;

	attr_reset(&attr);
	attr.ia_ino = i_ino_of(ii);
	attr.ia_nlink = i_nlink_of(ii) + 1;
	attr.ia_parent_ino = i_ino_of(dir_ii);
	attr.ia_flags |= VOLUTA_ATTR_NLINK | VOLUTA_ATTR_PARENT;
	i_update_attr(dir_ctx->env, ii, &attr);

	attr_reset(&attr);
	attr.ia_ino = i_ino_of(dir_ii);
	if (i_isdir(ii)) {
		attr.ia_nlink = i_nlink_of(dir_ii) + 1;
		attr.ia_flags |= VOLUTA_ATTR_NLINK;
	}
	i_update_attr(dir_ctx->env, dir_ii, &attr);
}

static void post_remove_dentry(const struct voluta_dir_ctx *dir_ctx)
{
	struct voluta_iattr attr;
	struct voluta_inode_info *ii = dir_ctx->ii;
	struct voluta_inode_info *dir_ii = dir_ctx->dir_ii;

	attr_reset(&attr);
	attr.ia_ino = i_ino_of(ii);
	attr.ia_nlink = i_nlink_of(ii) - 1;
	attr.ia_flags |= VOLUTA_ATTR_NLINK;
	if (i_parent_ino_of(ii) == i_ino_of(dir_ii)) {
		attr.ia_parent_ino = VOLUTA_INO_NULL;
		attr.ia_flags |= VOLUTA_ATTR_PARENT;
	}
	i_update_attr(dir_ctx->env, ii, &attr);

	attr_reset(&attr);
	attr.ia_ino = i_ino_of(dir_ii);
	if (i_isdir(ii)) {
		attr.ia_nlink = i_nlink_of(dir_ii) - 1;
		attr.ia_flags |= VOLUTA_ATTR_NLINK;
	}
	i_update_attr(dir_ctx->env, dir_ii, &attr);
}

static int add_to_head(struct voluta_dir_ctx *dir_ctx)
{
	struct voluta_dir_entry *de;
	struct voluta_dir_head *dh = dir_head_of(dir_ctx->dir_ii);
	const struct voluta_qstr *name = dir_ctx->name;
	const struct voluta_inode_info *ii = dir_ctx->ii;

	de = dir_head_insert(dh, name, i_ino_of(ii), dtype_of(ii));
	if (de == NULL) {
		return -ENOSPC;
	}
	i_dirtify(dir_ctx->dir_ii);
	return 0;
}

static int add_dentry(struct voluta_dir_ctx *dir_ctx)
{
	int err;
	struct voluta_inode_info *dir_ii = dir_ctx->dir_ii;

	err = add_to_head(dir_ctx);
	if (!err) {
		goto out;
	}
	err = add_to_tree(dir_ctx);
	if (err) {
		return err;
	}
out:
	dir_inc_ndents(dir_ii);
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
	if (!(ndents < VOLUTA_DIRENT_MAX)) {
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

int voluta_add_dentry(struct voluta_env *env,
		      struct voluta_inode_info *dir_ii,
		      const struct voluta_qstr *name,
		      struct voluta_inode_info *ii)
{
	int err;
	struct voluta_dir_ctx dir_ctx = {
		.env = env,
		.sbi = sbi_of(env),
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

static int require_raccess(struct voluta_env *env,
			   const struct voluta_inode_info *ii)
{
	return voluta_do_access(env, ii, R_OK);
}

static int require_rxaccess(struct voluta_env *env,
			    const struct voluta_inode_info *ii)
{
	return voluta_do_access(env, ii, R_OK | X_OK);
}

static int require_parent(struct voluta_env *env,
			  const struct voluta_inode_info *dir_ii,
			  struct voluta_inode_info **out_parent_ii)
{
	int err;
	ino_t parent_ino;

	parent_ino = i_parent_ino_of(dir_ii);
	if (!ino_isvalid(parent_ino)) {
		return -EFSCORRUPTED;
	}
	err = voluta_stage_inode(sbi_of(env), parent_ino, out_parent_ii);
	if (err) {
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool isinrange(const struct voluta_dir_ctx *dir_ctx)
{
	loff_t doff = doff_xtoi(dir_ctx->readdir_ctx->pos);

	return (doff >= 0) && (doff < VOLUTA_DIROFF_MAX);
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
	const char *name = dir_entry_name(de);

	dir_ctx->readdir_ctx->pos = doff_itox(doff);
	return emit(dir_ctx, name, de->de_nlen, de->de_ino, de->de_dtype);
}

static bool emit_ii(struct voluta_dir_ctx *dir_ctx, const char *name,
		    size_t nlen, const struct voluta_inode_info *ii)
{
	const ino_t xino = i_xino_of(ii);
	const mode_t mode = i_mode_of(ii);

	return emit(dir_ctx, name, nlen, xino, IFTODT(mode));
}

static bool stopped(const struct voluta_dir_ctx *dir_ctx)
{
	return !dir_ctx->keep_iter;
}

static bool has_head_pos(struct voluta_dir_ctx *dir_ctx)
{
	const loff_t pos = doff_xtoi(dir_ctx->readdir_ctx->pos);

	return (pos >= 0) && (pos < VOLUTA_DIRHEAD_NENTS);
}

static int iterate_head(struct voluta_dir_ctx *dir_ctx)
{
	bool ok;
	loff_t pos, off;
	const struct voluta_dir_entry *de;
	const struct voluta_dir_head *dh = dir_head_of(dir_ctx->dir_ii);

	while (has_head_pos(dir_ctx) && !stopped(dir_ctx)) {
		pos = doff_xtoi(dir_ctx->readdir_ctx->pos);
		de = dir_head_scan(dh, pos);
		if (!de) {
			off = dir_head_next_offset(dh);
			dir_ctx->readdir_ctx->pos = doff_itox(off);
			break;
		}
		off = dir_head_resolve_offset(dh, de);
		ok = emit_dirent(dir_ctx, de, off);
		if (!ok) {
			break;
		}
		dir_ctx->readdir_ctx->pos += 1;
	}
	return 0;
}

static int iterate_dir_tnode(struct voluta_dir_ctx *dir_ctx,
			     const struct voluta_vnode_info *vi)
{
	bool ok;
	loff_t pos, off;
	const struct voluta_dir_entry *de;

	while (!stopped(dir_ctx)) {
		pos = doff_xtoi(dir_ctx->readdir_ctx->pos);
		de = dir_tnode_scan(vi->u.dtn, pos);
		if (!de) {
			off = dir_tnode_next_offset(vi->u.dtn);
			dir_ctx->readdir_ctx->pos = doff_itox(off);
			break;
		}
		off = dir_tnode_resolve_offset(vi->u.dtn, de);
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

static int stage_dir_node_at(struct voluta_dir_ctx *dir_ctx, loff_t node_index,
			     struct voluta_vnode_info **out_vi)
{
	int err;
	size_t child_ord;
	loff_t parent_index;
	struct voluta_vnode_info *parent_vi;
	struct voluta_dir_tnode_info dti;

	dtnode_index_to_parent(node_index, &parent_index, &child_ord);
	setup_by_ord(&dti, dir_ctx->dir_ii, child_ord);
	if (parent_index > 0) {
		err = stage_dir_node_at(dir_ctx, parent_index, &parent_vi);
		if (err) {
			return err;
		}
		update_by_ord(&dti, parent_vi, child_ord);
	}
	return stage_dir_tnode(dir_ctx, &dti, out_vi);
}

static int iterate_tree(struct voluta_dir_ctx *dir_ctx)
{
	int err = 0;
	loff_t node_index, last_index;
	struct voluta_vnode_info *vi;

	last_index = last_tindex_of(dir_ctx);
	node_index = start_tindex_of(dir_ctx);
	voluta_assert_gt(node_index, 0);

	while (node_index <= last_index) {
		err = stage_dir_node_at(dir_ctx, node_index++, &vi);
		if (err == -ENOENT) {
			err = 0;
			continue;
		}
		if (err) {
			break;
		}
		err = iterate_dir_tnode(dir_ctx, vi);
		if (err || stopped(dir_ctx)) {
			break;
		}
	}
	return err;
}

static int iterate_dir(struct voluta_dir_ctx *dir_ctx)
{
	int err;

	err = iterate_head(dir_ctx);
	if (err || stopped(dir_ctx)) {
		return err;
	}
	if (!dir_has_tree(dir_ctx->dir_ii)) {
		return readdir_eos(dir_ctx);
	}
	err = iterate_tree(dir_ctx);
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
		ok = emit_ii(dir_ctx, "..", 2, dir_ctx->parent_dir_ii);
		dir_ctx->readdir_ctx->pos = 2;
	}
	if (ok && !isinrange(dir_ctx)) {
		err = readdir_eos(dir_ctx);
	}
	if (ok && !stopped(dir_ctx)) {
		err = iterate_dir(dir_ctx);
	}

	attr_reset(&attr);
	attr.ia_flags |= VOLUTA_ATTR_ATIME | VOLUTA_ATTR_LAZY;
	attr.ia_ino = i_ino_of(dir_ctx->dir_ii);
	i_update_attr(dir_ctx->env, dir_ctx->dir_ii, &attr);

	return err;
}

int voluta_do_readdir(struct voluta_env *env,
		      const struct voluta_inode_info *dir_ii,
		      struct voluta_readdir_ctx *readdir_ctx)
{
	int err;
	struct voluta_dir_ctx dir_ctx = {
		.env = env,
		.sbi = sbi_of(env),
		.readdir_ctx = readdir_ctx,
		.dir_ii = unconst_ii(dir_ii),
		.keep_iter = true
	};

	err = check_dir_io(dir_ii);
	if (err) {
		return err;
	}
	err = require_rxaccess(env, dir_ii);
	if (err) {
		return err;
	}
	err = require_parent(env, dir_ii, &dir_ctx.parent_dir_ii);
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
			       const struct voluta_dir_tnode_info *dti)
{
	int err;
	struct voluta_vnode_info *vi;
	struct voluta_dir_tnode_info child_dti;
	const size_t fanout = VOLUTA_DIRNODE_NCHILDS;

	if (vaddr_isnull(&dti->tnode_vaddr)) {
		return 0;
	}
	err = stage_dir_tnode(dir_ctx, dti, &vi);
	if (err) {
		return err;
	}
	reset_tnode_info(&child_dti, dir_ctx->dir_ii);
	for (size_t child_ord = 0; child_ord < fanout; ++child_ord) {
		update_by_ord(&child_dti, vi, child_ord);
		err = discard_recursively(dir_ctx, &child_dti);
		if (err) {
			return err;
		}
	}
	err = discard_dir_tnode(dir_ctx, vi);
	if (err) {
		return err;
	}
	return 0;
}

static int drop_tree(struct voluta_dir_ctx *dir_ctx)
{
	int err = 0;
	struct voluta_dir_tnode_info dti;
	const size_t fanout = VOLUTA_DIRNODE_NCHILDS;

	for (size_t child_ord = 0; child_ord < fanout; ++child_ord) {
		setup_by_ord(&dti, dir_ctx->dir_ii, child_ord);
		err = discard_recursively(dir_ctx, &dti);
		if (err) {
			break;
		}
	}
	return err;
}

static int finalize_tree(struct voluta_dir_ctx *dir_ctx)
{
	int err;
	struct voluta_inode_info *dir_ii = dir_ctx->dir_ii;

	err = drop_tree(dir_ctx);
	if (err) {
		return err;
	}
	dir_setup(dir_ii);
	i_dirtify(dir_ii);
	return 0;
}

int voluta_drop_dir(struct voluta_env *env, struct voluta_inode_info *dir_ii)
{
	struct voluta_dir_ctx dir_ctx = {
		.env = env,
		.sbi = sbi_of(env),
		.dir_ii = dir_ii,
	};

	if (!dir_has_tree(dir_ii)) {
		return 0;
	}
	return finalize_tree(&dir_ctx);
}

static int remove_dentry(struct voluta_dir_ctx *dir_ctx,
			 const struct voluta_dirent_info *dei)
{
	size_t ndents;
	struct voluta_inode_info *dir_ii = dir_ctx->dir_ii;

	dir_entry_remove(dei->de, dei->de_last);
	if (dei->vi == NULL) {
		goto out_ok;
	}
	v_dirtify(dei->vi);

	/* TODO: Remove nodes and squeeze-up tree for unused meta-data */
	ndents = voluta_dir_ndentries(dir_ii);
	if (ndents > 1) {
		goto out_ok;
	}
	return finalize_tree(dir_ctx);

out_ok:
	dir_dec_ndents(dir_ii);
	return 0;
}

int voluta_remove_dentry(struct voluta_env *env,
			 struct voluta_inode_info *dir_ii,
			 const struct voluta_qstr *name)
{
	int err;
	struct voluta_dirent_info dei;
	struct voluta_dir_ctx dir_ctx = {
		.env = env,
		.sbi = sbi_of(env),
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

int voluta_do_opendir(struct voluta_env *env, struct voluta_inode_info *dir_ii)
{
	int err;

	err = require_dir(dir_ii);
	if (err) {
		return err;
	}
	err = require_raccess(env, dir_ii);
	if (err) {
		return err;
	}
	err = voluta_inc_fileref(env, dir_ii);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_do_releasedir(struct voluta_env *env,
			 struct voluta_inode_info *dir_ii)
{
	int err;

	err = check_dir_io(dir_ii);
	if (err) {
		return err;
	}
	err = voluta_do_access(env, dir_ii, F_OK); /* XXX */
	if (err) {
		return err;
	}
	dir_ii->vi.ce.ce_refcnt--;

	/* TODO: Shrink sparse dir-tree upon last close */

	return 0;
}

int voluta_do_fsyncdir(struct voluta_env *env,
		       struct voluta_inode_info *dir_ii, bool dsync)
{
	int err;

	err = check_dir_io(dir_ii);
	if (err) {
		return err;
	}
	/* TODO */
	(void)env;
	(void)dsync;
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_setup_new_dir(struct voluta_inode_info *dir_ii)
{
	dir_setup(dir_ii);
	dir_ii->inode->i_nlink = 1;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int ok_or_unclean(long val, long val_min, long val_max)
{
	return ((val >= val_min) && (val <= val_max)) ? 0 : -EFSCORRUPTED;
}

static int verify_base_doff(loff_t base_doff)
{
	return ok_or_unclean(base_doff, 0, VOLUTA_DIROFF_MAX);
}

static int verify_node_index(loff_t node_index)
{
	return ok_or_unclean(node_index, 0, VOLUTA_DIRNODE_MAX);
}

static int verify_tree_depth(size_t tree_depth)
{
	return ok_or_unclean((long)tree_depth, 0, VOLUTA_DIRDEPTH_MAX);
}

static int verify_child_ord(size_t child_ord)
{
	return ok_or_unclean((long)child_ord, 0, VOLUTA_DIRNODE_NCHILDS);
}

static int verify_dir_tlink(const struct voluta_dir_tlink *tlink)
{
	int err;
	loff_t expected_base_doff;

	err = voluta_verify_off(tlink->dl_parent_off);
	if (err) {
		return err;
	}
	err = verify_base_doff(tlink->dl_base_doff);
	if (err) {
		return err;
	}
	err = verify_node_index(tlink->dl_node_index);
	if (err) {
		return err;
	}
	err = verify_tree_depth(tlink->dl_tree_depth);
	if (err) {
		return err;
	}
	err = verify_child_ord(tlink->dl_child_ord);
	if (err) {
		return err;
	}
	for (size_t i = 0; i < ARRAY_SIZE(tlink->dl_child_off); ++i) {
		err = voluta_verify_off(tlink->dl_child_off[i]);
		if (err) {
			return err;
		}
	}
	expected_base_doff = dtnode_index_to_offset(tlink->dl_node_index);
	if (tlink->dl_base_doff != expected_base_doff) {
		return -EFSCORRUPTED;
	}
	return 0;
}

int voluta_verify_dir_inode(const struct voluta_inode *inode)
{
	int err;
	struct voluta_dir_inode *di = inode_to_dirinode(inode);
	const struct voluta_dir_head *dh = &di->d_h;

	/* TODO: Check more */
	if (dh->d_class != VOLUTA_DIRCLASS_V1) {
		return -EFSCORRUPTED;
	}
	err = verify_node_index(dh->d_last_index);
	if (err) {
		return err;
	}
	err = verify_dir_tlink(&dh->d_tlink);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_verify_dir_tnode(const struct voluta_dir_tnode *dtn)
{
	int err;

	err = voluta_verify_ino(dtn->dt_ino);
	if (err) {
		return err;
	}
	err = verify_dir_tlink(&dtn->dt_tlink);
	if (err) {
		return err;
	}
	return 0;
}
