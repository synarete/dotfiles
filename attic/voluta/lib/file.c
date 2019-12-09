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
#include <linux/falloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "voluta-lib.h"


struct voluta_radix_leaf_info {
	struct voluta_vaddr vaddr;
	struct voluta_inode_info *ii;
	struct voluta_vnode_info *parent_vi;
	size_t parent_slot;
	loff_t file_pos;
};

struct voluta_file_ctx {
	struct voluta_env *env;
	struct voluta_sb_info *sbi;
	struct voluta_inode_info *ii;
	struct voluta_rw_iter *rwi;
	loff_t  start;
	size_t  len;
	loff_t  beg;
	loff_t  off;
	loff_t  end;
	int     fl_mode;
	bool    read;
	bool    write;
	bool    trunc;
	bool    falloc;
};

/* Local functions forward declarations */
static bool fl_keep_size(const struct voluta_file_ctx *file_ctx);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void off_to_int56(loff_t off, struct voluta_int56 *nn)
{
	uint64_t uv = off_isnull(off) ? (~0ULL << 8) : (uint64_t)off;

	voluta_assert_eq(uv & 0xff, 0);
	nn->b[0] = (uint8_t)(uv >> 8);
	nn->b[1] = (uint8_t)(uv >> 16);
	nn->b[2] = (uint8_t)(uv >> 24);
	nn->b[3] = (uint8_t)(uv >> 32);
	nn->b[4] = (uint8_t)(uv >> 40);
	nn->b[5] = (uint8_t)(uv >> 48);
	nn->b[6] = (uint8_t)(uv >> 56);
}

static loff_t int56_to_off(const struct voluta_int56 *nn)
{
	uint64_t uv = 0;

	uv |= ((uint64_t)nn->b[0]) << 8;
	uv |= ((uint64_t)nn->b[1]) << 16;
	uv |= ((uint64_t)nn->b[2]) << 24;
	uv |= ((uint64_t)nn->b[3]) << 32;
	uv |= ((uint64_t)nn->b[4]) << 40;
	uv |= ((uint64_t)nn->b[5]) << 48;
	uv |= ((uint64_t)nn->b[6]) << 56;

	return (uv == (~0ULL << 8)) ? VOLUTA_OFF_NULL : (loff_t)uv;
}
/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static loff_t off_align(loff_t off, loff_t align)
{
	return (off / align) * align;
}

static loff_t off_end(loff_t off, size_t len)
{
	return off + (loff_t)len;
}

static loff_t off_min(loff_t off1, loff_t off2)
{
	return (off1 < off2) ? off1 : off2;
}

static loff_t off_max(loff_t off1, loff_t off2)
{
	return (off1 > off2) ? off1 : off2;
}

static loff_t off_diff(loff_t beg, loff_t end)
{
	return end - beg;
}

static size_t off_len(loff_t beg, loff_t end)
{
	return (size_t)off_diff(beg, end);
}

static loff_t off_to_df(loff_t off)
{
	return off_align(off, VOLUTA_DF_SIZE);
}

static loff_t off_to_df_up(loff_t off)
{
	return off_align(off + VOLUTA_DF_SIZE - 1, VOLUTA_DF_SIZE);
}

static loff_t off_to_next_df(loff_t off)
{
	return off_to_df(off + VOLUTA_DF_SIZE);
}

static loff_t off_in_df(loff_t off)
{
	return off % VOLUTA_DF_SIZE;
}

static bool off_is_df_start(loff_t off)
{
	return (off_to_df(off) == off);
}

static size_t len_to_next_df(loff_t off)
{
	loff_t off_next = off_to_next_df(off);

	return (size_t)(off_next - off);
}

static size_t len_within_df(loff_t off, loff_t end)
{
	const loff_t off_next = off_to_next_df(off);

	return (size_t)((off_next < end) ? (off_next - off) : (end - off));
}

static size_t off_len_ndfs(loff_t off, size_t len)
{
	const loff_t beg = off_to_df(off);
	const loff_t end = off_to_df_up(off_end(off, len));

	return (size_t)(end - beg) / VOLUTA_DF_SIZE;
}

static bool off_is_inrange(loff_t off, loff_t beg, loff_t end)
{
	return (beg <= off) && (off < end);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool vaddr_isleaf(const struct voluta_vaddr *vaddr)
{
	voluta_assert((vaddr->vtype == VOLUTA_VTYPE_DATA) ||
		      (vaddr->vtype == VOLUTA_VTYPE_RTNODE));

	return (vaddr->vtype == VOLUTA_VTYPE_DATA);
}

static void vaddr_of_leaf(struct voluta_vaddr *vaddr, loff_t off)
{
	voluta_vaddr_by_off(vaddr, VOLUTA_VTYPE_DATA, off);
}

static void vaddr_of_tnode(struct voluta_vaddr *vaddr, loff_t off)
{
	voluta_vaddr_by_off(vaddr, VOLUTA_VTYPE_RTNODE, off);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void zero_range(struct voluta_data_frg *df, loff_t off, size_t len)
{
	voluta_assert(len <= sizeof(df->dat));
	voluta_assert((size_t)off + len <= sizeof(df->dat));

	memset(&df->dat[off], 0, len);
}

static void zero_headn(struct voluta_data_frg *df, size_t n)
{
	zero_range(df, 0, n);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t radix_tnode_nslots(const struct voluta_radix_tnode *rtn)
{
	return ARRAY_SIZE(rtn->r_child_off);
}

static loff_t radix_tnode_end(const struct voluta_radix_tnode *rtn)
{
	return rtn->r_end;
}

static size_t radix_tnode_span(const struct voluta_radix_tnode *rtn)
{
	return off_len(rtn->r_beg, rtn->r_end);
}

static size_t radix_tnode_height(const struct voluta_radix_tnode *rtn)
{
	return rtn->r_height;
}

static bool radix_tnode_isbottom(const struct voluta_radix_tnode *rtn)
{
	const size_t height = radix_tnode_height(rtn);

	voluta_assert_ge(height, 2);
	voluta_assert_le(height, 5);

	return (height == 2);
}

static size_t radix_tnode_nbytes_per_slot(const struct voluta_radix_tnode *rtn)
{
	return radix_tnode_span(rtn) / radix_tnode_nslots(rtn);
}

static size_t
radix_tnode_slot_by_offset(const struct voluta_radix_tnode *rtn, loff_t off)
{
	loff_t roff;
	size_t span, slot, nslots;

	voluta_assert_lt(rtn->r_beg, rtn->r_end);
	voluta_assert_ge(off, rtn->r_beg);
	voluta_assert_lt(off, rtn->r_end);
	voluta_assert_ge(radix_tnode_height(rtn), 0);

	span = radix_tnode_span(rtn);
	nslots = radix_tnode_nslots(rtn);

	roff = off_diff(rtn->r_beg, off);
	slot = ((size_t)roff * nslots) / span;
	voluta_assert_lt(slot, nslots);
	voluta_assert_ge((size_t)roff * nslots, roff);

	return slot;
}

static size_t
radix_tnode_height_by_offset(const struct voluta_radix_tnode *rtn, loff_t off)
{
	loff_t xoff;
	bool loop = true;
	size_t height = 1;
	const size_t nslots = radix_tnode_nslots(rtn);

	xoff = off / VOLUTA_BK_SIZE;
	while (loop) {
		++height;
		xoff /= (loff_t)nslots;
		loop = (xoff > 0);
	}
	return height;
}

static loff_t
radix_tnode_child_at(const struct voluta_radix_tnode *rtn, size_t slot)
{
	voluta_assert_lt(slot, ARRAY_SIZE(rtn->r_child_off));
	return int56_to_off(&rtn->r_child_off[slot]);
}

static loff_t
radix_tnode_get_child_off(const struct voluta_radix_tnode *rtn, loff_t off)
{
	const size_t slot = radix_tnode_slot_by_offset(rtn, off);

	return radix_tnode_child_at(rtn, slot);
}

static void
radix_tnode_resolve_child(const struct voluta_radix_tnode *rtn,
			  loff_t child_off, struct voluta_vaddr *vaddr)
{
	if (radix_tnode_isbottom(rtn)) {
		vaddr_of_leaf(vaddr, child_off);
	} else {
		vaddr_of_tnode(vaddr, child_off);
	}
}

static void radix_tnode_get_child(const struct voluta_radix_tnode *rtn,
				  loff_t file_pos, struct voluta_vaddr *vaddr)
{
	const loff_t off = radix_tnode_get_child_off(rtn, file_pos);

	radix_tnode_resolve_child(rtn, off, vaddr);
}

static void radix_tnode_get_child_at(const struct voluta_radix_tnode *rtn,
				     size_t slot, struct voluta_vaddr *vaddr)
{
	const loff_t off = radix_tnode_child_at(rtn, slot);

	radix_tnode_resolve_child(rtn, off, vaddr);
}

static void
radix_tnode_set_child_at(struct voluta_radix_tnode *rtn, size_t slot,
			 const struct voluta_vaddr *vaddr)
{
	off_to_int56(vaddr->off, &rtn->r_child_off[slot]);
}

static void
radix_tnode_set_child(struct voluta_radix_tnode *rtn, loff_t file_pos,
		      const struct voluta_vaddr *vaddr)
{
	const size_t slot = radix_tnode_slot_by_offset(rtn, file_pos);

	radix_tnode_set_child_at(rtn, slot, vaddr);
}

static void
radix_tnode_reset_child_at(struct voluta_radix_tnode *rtn, size_t slot)
{
	off_to_int56(VOLUTA_OFF_NULL, &rtn->r_child_off[slot]);
}

static void radix_tnode_clear_childs(struct voluta_radix_tnode *rtn)
{
	const size_t nslots_max = radix_tnode_nslots(rtn);

	for (size_t slot = 0; slot < nslots_max; ++slot) {
		radix_tnode_reset_child_at(rtn, slot);
	}
}

static bool
radix_tnode_isinrange(const struct voluta_radix_tnode *rtn, loff_t file_pos)
{
	return off_is_inrange(file_pos, rtn->r_beg, rtn->r_end);
}

static size_t
radix_tnode_span_by_height(const struct voluta_radix_tnode *rtn, size_t height)
{
	size_t nslots, nbytes = VOLUTA_BK_SIZE;

	voluta_assert_le(height, 5);
	voluta_assert_ge(height, 2);

	/* TODO: Use SHIFT instead of loop */
	nslots = radix_tnode_nslots(rtn);
	for (size_t i = 2; i <= height; ++i) {
		nbytes *= nslots;
	}
	return nbytes ? nbytes : LONG_MAX; /* make clang-scan happy */
}

static void
radix_tnode_calc_range(const struct voluta_radix_tnode *rtn,
		       loff_t off, size_t height, loff_t *beg, loff_t *end)
{
	const loff_t span = (loff_t)radix_tnode_span_by_height(rtn, height);

	*beg = off_align(off, span);
	*end = *beg + span;
}

static loff_t
radix_tnode_offset_at(const struct voluta_radix_tnode *rtn, size_t slot)
{
	loff_t next_off;
	const size_t nbps = radix_tnode_nbytes_per_slot(rtn);

	voluta_assert_lt(rtn->r_beg, rtn->r_end);
	voluta_assert_gt(radix_tnode_height(rtn), 0);
	voluta_assert_lt(slot, radix_tnode_nslots(rtn));

	next_off = off_end(rtn->r_beg, slot * nbps);
	return off_to_df(next_off);
}

static loff_t
radix_tnode_next_offset(const struct voluta_radix_tnode *rtn, size_t slot)
{
	size_t nbps;
	loff_t soff;

	soff = radix_tnode_offset_at(rtn, slot);
	nbps = radix_tnode_nbytes_per_slot(rtn);
	return off_end(soff, nbps);
}

static void radix_tnode_init(struct voluta_radix_tnode *rtn,
			     ino_t ino, loff_t off, size_t height)
{
	loff_t beg, end;

	radix_tnode_calc_range(rtn, off, height, &beg, &end);
	radix_tnode_clear_childs(rtn);
	rtn->r_beg = beg;
	rtn->r_end = end;
	rtn->r_height = (uint8_t)height;
	rtn->r_flags = 0;
	rtn->r_ino = ino;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_reg_inode *reg_inode_of(const struct voluta_inode_info *ii)
{
	struct voluta_inode *inode = ii->inode;

	return voluta_container_of(inode, struct voluta_reg_inode, r_i);
}

static struct voluta_tree_ref *file_tree_ref(const struct voluta_inode_info *ii)
{
	struct voluta_reg_inode *ri = reg_inode_of(ii);

	return &ri->r_i.i_s.tr;
}

static void file_tree_setup(struct voluta_inode_info *ii)
{
	struct voluta_tree_ref *tree_ref = file_tree_ref(ii);

	tree_ref->t_root_off = VOLUTA_OFF_NULL;
	tree_ref->t_height = 0;
}

static size_t file_tree_height(const struct voluta_inode_info *ii)
{
	const struct voluta_tree_ref *tree_ref = file_tree_ref(ii);

	return tree_ref->t_height;
}

static void file_tree_root(const struct voluta_inode_info *ii,
			   struct voluta_vaddr *vaddr)
{
	const size_t height = file_tree_height(ii);
	const struct voluta_tree_ref *tree_ref = file_tree_ref(ii);

	if (height >= 2) {
		vaddr_of_tnode(vaddr, tree_ref->t_root_off);
	} else if (height == 1) {
		vaddr_of_leaf(vaddr, tree_ref->t_root_off);
	} else {
		vaddr_reset(vaddr);
	}
}

static void file_tree_update(struct voluta_inode_info *ii,
			     const struct voluta_vaddr *vaddr, size_t height)
{
	struct voluta_tree_ref *tree_ref = file_tree_ref(ii);

	tree_ref->t_root_off = vaddr->off;
	tree_ref->t_height = (uint32_t)height;
}


static bool file_is_leaf_mode(const struct voluta_inode_info *ii)
{
	return (file_tree_height(ii) == 1);
}

static bool file_is_tree_mode(const struct voluta_inode_info *ii)
{
	return (file_tree_height(ii) >= 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void init_radix_leaf_info(struct voluta_radix_leaf_info *rli,
				 struct voluta_inode_info *ii, loff_t pos)
{
	vaddr_reset(&rli->vaddr);
	rli->ii = ii;
	rli->parent_vi = NULL;
	rli->parent_slot = 0;
	rli->file_pos = pos;
}

static void set_radix_leaf_info(struct voluta_radix_leaf_info *rli,
				struct voluta_vnode_info *parent_vi,
				size_t slot, loff_t pos,
				const struct voluta_vaddr *vaddr)
{
	loff_t leaf_off = radix_tnode_offset_at(parent_vi->u.rtn, slot);

	vaddr_clone(vaddr, &rli->vaddr);
	rli->parent_vi = parent_vi;
	rli->parent_slot = slot;
	rli->file_pos = off_max(leaf_off, pos);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int require_reg(const struct voluta_file_ctx *file_ctx)
{
	if (i_isdir(file_ctx->ii)) {
		return -EISDIR;
	}
	if (!i_isreg(file_ctx->ii)) {
		return -EINVAL;
	}
	return 0;
}

static int check_file_io(const struct voluta_file_ctx *file_ctx)
{
	int err;
	size_t ndfs;
	const loff_t filesize_max = VOLUTA_FILESIZE_MAX;
	const size_t iosize_max = VOLUTA_IO_SIZE_MAX;
	const size_t ndfs_max = VOLUTA_IO_NDF_MAX;

	err = require_reg(file_ctx);
	if (err) {
		return err;
	}
	if (!file_ctx->trunc && !i_refcnt_of(file_ctx->ii)) {
		return -EBADF;
	}
	if (file_ctx->start < 0) {
		return -EINVAL;
	}
	if (file_ctx->beg > filesize_max) {
		return -EFBIG;
	}
	if ((file_ctx->end > filesize_max) && !file_ctx->trunc) {
		return -EFBIG;
	}
	if (file_ctx->read || file_ctx->write) {
		if (file_ctx->len > iosize_max) {
			return -EINVAL;
		}
		ndfs = off_len_ndfs(file_ctx->beg, file_ctx->len);
		if (ndfs_max < ndfs) {
			return -EINVAL;
		}
		if (!file_ctx->rwi) {
			return -EINVAL;
		}
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int iter_recursive(struct voluta_file_ctx *,
			  struct voluta_vnode_info *,
			  struct voluta_radix_leaf_info *);

static bool has_more_io(const struct voluta_file_ctx *file_ctx)
{
	return (file_ctx->off < file_ctx->end);
}

static bool has_head_leaf_io(const struct voluta_file_ctx *file_ctx)
{
	return (file_ctx->off < VOLUTA_BK_SIZE);
}

static bool has_more_leaf_io(const struct voluta_file_ctx *file_ctx)
{
	return has_more_io(file_ctx) && has_head_leaf_io(file_ctx);
}

static size_t io_length(const struct voluta_file_ctx *file_ctx)
{
	voluta_assert(file_ctx->beg <= file_ctx->off);

	return (size_t)(file_ctx->off - file_ctx->beg);
}

static void post_io_update(const struct voluta_file_ctx *file_ctx,
			   size_t *len, bool kill_priv)
{
	struct voluta_iattr attr;
	struct voluta_env *env = file_ctx->env;
	struct voluta_inode_info *ii = file_ctx->ii;
	const loff_t isz = i_size_of(ii);
	const loff_t off = file_ctx->off;

	attr_reset(&attr);
	attr.ia_ino = i_ino_of(ii);

	*len = io_length(file_ctx);
	if (file_ctx->falloc) {
		attr.ia_flags |= VOLUTA_ATTR_RMCTIME;
		if (!fl_keep_size(file_ctx)) {
			attr.ia_flags |= VOLUTA_ATTR_SIZE;
			attr.ia_size = off_max(off, isz);
		}
	} else if (file_ctx->write) {
		attr.ia_flags |= VOLUTA_ATTR_RMCTIME | VOLUTA_ATTR_SIZE;
		attr.ia_size = off_max(off, isz);
		if (kill_priv && (*len > 0)) {
			attr.ia_flags |= VOLUTA_ATTR_KILL_PRIV;
		}
	} else if (file_ctx->read) {
		attr.ia_flags |= VOLUTA_ATTR_ATIME | VOLUTA_ATTR_LAZY;
	} else if (file_ctx->trunc) {
		attr.ia_flags |= VOLUTA_ATTR_RMCTIME |
				 VOLUTA_ATTR_SIZE | VOLUTA_ATTR_KILL_PRIV;
		attr.ia_size = file_ctx->beg;
	}
	i_update_attr(env, ii, &attr);
}

static int stage_data_leaf(const struct voluta_file_ctx *file_ctx,
			   const struct voluta_vaddr *vaddr,
			   struct voluta_vnode_info **out_vi)
{
	int err;

	voluta_assert_eq(vaddr->vtype, VOLUTA_VTYPE_DATA);
	if (vaddr_isnull(vaddr)) {
		return -ENOENT;
	}
	err = voluta_stage_vnode(file_ctx->sbi, vaddr, out_vi);
	if (err) {
		return err;
	}
	return 0;
}

static int stage_radix_tnode(const struct voluta_file_ctx *file_ctx,
			     const struct voluta_vaddr *vaddr,
			     struct voluta_vnode_info **out_vi)
{
	return vaddr_isnull(vaddr) ? -ENOENT :
	       voluta_stage_vnode(file_ctx->sbi, vaddr, out_vi);
}

static int stage_file_tree_root(const struct voluta_file_ctx *file_ctx,
				struct voluta_vnode_info **out_vi)
{
	struct voluta_vaddr root_vaddr;

	file_tree_root(file_ctx->ii, &root_vaddr);
	return stage_radix_tnode(file_ctx, &root_vaddr, out_vi);
}

static int stage_file_head_leaf(const struct voluta_file_ctx *file_ctx,
				struct voluta_vnode_info **out_vi)
{
	struct voluta_vaddr head_leaf_vaddr;

	file_tree_root(file_ctx->ii, &head_leaf_vaddr);
	return stage_data_leaf(file_ctx, &head_leaf_vaddr, out_vi);
}

static void advance_to(struct voluta_file_ctx *file_ctx, loff_t off)
{
	file_ctx->off = off_max(file_ctx->off, off);
}

static void advance_to_slot(struct voluta_file_ctx *file_ctx,
			    const struct voluta_radix_tnode *rtn, size_t slot)
{
	advance_to(file_ctx, radix_tnode_offset_at(rtn, slot));
}

static void advance_to_next(struct voluta_file_ctx *file_ctx,
			    const struct voluta_radix_tnode *rtn, size_t slot)
{
	advance_to(file_ctx, radix_tnode_next_offset(rtn, slot));
}

static int iter_to_data(struct voluta_file_ctx *file_ctx,
			struct voluta_vnode_info *parent_vi,
			struct voluta_radix_leaf_info *rli)
{
	size_t slot, start_slot, nslots_max;
	const loff_t off = file_ctx->off;
	struct voluta_vaddr vaddr;

	start_slot = radix_tnode_slot_by_offset(parent_vi->u.rtn, off);
	nslots_max = radix_tnode_nslots(parent_vi->u.rtn);
	for (slot = start_slot; slot < nslots_max; ++slot) {
		advance_to_slot(file_ctx, parent_vi->u.rtn, slot);

		radix_tnode_get_child_at(parent_vi->u.rtn, slot, &vaddr);
		if (!vaddr_isnull(&vaddr)) {
			set_radix_leaf_info(rli, parent_vi, slot, off, &vaddr);
			return 0;
		}
	}
	return -ENOENT;
}

static int iter_recursive_at(struct voluta_file_ctx *file_ctx,
			     struct voluta_vnode_info *parent_vi,
			     size_t slot, struct voluta_radix_leaf_info *rli)
{
	int err;
	struct voluta_vaddr vaddr;
	struct voluta_vnode_info *vi;

	radix_tnode_get_child_at(parent_vi->u.rtn, slot, &vaddr);
	if (vaddr_isnull(&vaddr)) {
		return -ENOENT;
	}
	err = stage_radix_tnode(file_ctx, &vaddr, &vi);
	if (err) {
		return err;
	}
	err = iter_recursive(file_ctx, vi, rli);
	if (err) {
		return err;
	}
	return 0;
}

static int iter_recursive(struct voluta_file_ctx *file_ctx,
			  struct voluta_vnode_info *parent_vi,
			  struct voluta_radix_leaf_info *rli)
{
	int err = -ENOENT;
	size_t slot, start_slot, nslots_max;
	const loff_t file_pos = file_ctx->off;

	if (!radix_tnode_isinrange(parent_vi->u.rtn, file_pos)) {
		return -ENOENT;
	}
	if (radix_tnode_isbottom(parent_vi->u.rtn)) {
		return iter_to_data(file_ctx, parent_vi, rli);
	}
	start_slot = radix_tnode_slot_by_offset(parent_vi->u.rtn, file_pos);
	nslots_max = radix_tnode_nslots(parent_vi->u.rtn);
	for (slot = start_slot; slot < nslots_max; ++slot) {
		err = iter_recursive_at(file_ctx, parent_vi, slot, rli);
		if (err != -ENOENT) {
			break;
		}
		advance_to_next(file_ctx, parent_vi->u.rtn, slot);
	}
	return err;
}

static int iterate_tree_map(struct voluta_file_ctx *file_ctx,
			    struct voluta_radix_leaf_info *rli)
{
	int err;
	struct voluta_vnode_info *root_vi;

	err = stage_file_tree_root(file_ctx, &root_vi);
	if (err) {
		return err;
	}
	init_radix_leaf_info(rli, file_ctx->ii, file_ctx->off);
	err = iter_recursive(file_ctx, root_vi, rli);
	if (err) {
		return err;
	}
	return 0;
}

static int fetch_by_tree_map(const struct voluta_file_ctx *file_ctx,
			     struct voluta_vnode_info **out_vi)
{
	int err;
	size_t height;
	struct voluta_vaddr vaddr;
	struct voluta_vnode_info *vi;

	if (!file_is_tree_mode(file_ctx->ii)) {
		return -ENOENT;
	}
	err = stage_file_tree_root(file_ctx, &vi);
	if (err) {
		return err;
	}
	if (!radix_tnode_isinrange(vi->u.rtn, file_ctx->off)) {
		return -ENOENT;
	}
	height = file_tree_height(file_ctx->ii);
	while (height--) {
		radix_tnode_get_child(vi->u.rtn, file_ctx->off, &vaddr);
		if (vaddr_isleaf(&vaddr)) {
			return stage_data_leaf(file_ctx, &vaddr, out_vi);
		}
		err = stage_radix_tnode(file_ctx, &vaddr, &vi);
		if (err) {
			return err;
		}
	}
	return -EFSCORRUPTED;
}

static void advance_nbytes(struct voluta_file_ctx *file_ctx, size_t len)
{
	advance_to(file_ctx, off_end(file_ctx->off, len));
}

static void advance_to_next_bk(struct voluta_file_ctx *file_ctx)
{
	advance_nbytes(file_ctx, len_to_next_df(file_ctx->off));
}

static void advance_to_next_pos(struct voluta_file_ctx *file_ctx)
{
	advance_nbytes(file_ctx, len_within_df(file_ctx->off, file_ctx->end));
}

static int memref_of(const struct voluta_file_ctx *file_ctx,
		     const void *ptr, size_t len, struct voluta_qmref *qmr)
{
	return voluta_memref(file_ctx->sbi->s_qmal, ptr, len, qmr);
}

static void *membuf_of(struct voluta_file_ctx *file_ctx,
		       const struct voluta_vnode_info *vi)
{
	void *mem;
	const struct voluta_cache *cache = file_ctx->sbi->s_cache;

	if (vi != NULL) {
		mem = vi->u.df->dat;
	} else {
		mem = cache->null_bk;
	}
	return mem;
}

static int call_rw_actor(struct voluta_file_ctx *file_ctx,
			 const struct voluta_vnode_info *vi, size_t *out_len)
{
	int err;
	loff_t cnt;
	size_t len;
	uint8_t *buf;
	struct voluta_qmref qmr = {
		.fd = -1,
	};

	cnt = off_in_df(file_ctx->off);
	len = len_within_df(file_ctx->off, file_ctx->end);
	buf = membuf_of(file_ctx, vi);
	err = memref_of(file_ctx, buf + cnt, len, &qmr);
	if (!err) {
		err = file_ctx->rwi->actor(file_ctx->rwi, &qmr);
	}
	*out_len = len;
	return err;
}

static int export_to_user(struct voluta_file_ctx *file_ctx,
			  const struct voluta_vnode_info *vi, size_t *out_len)
{
	return call_rw_actor(file_ctx, vi, out_len);
}

static int import_from_user(struct voluta_file_ctx *file_ctx,
			    const struct voluta_vnode_info *vi, size_t *out_len)
{
	return call_rw_actor(file_ctx, vi, out_len);
}

static int read_by_tree_map(struct voluta_file_ctx *file_ctx)
{
	int err;
	size_t len;
	struct voluta_vnode_info *vi;

	while (has_more_io(file_ctx)) {
		vi = NULL;
		err = fetch_by_tree_map(file_ctx, &vi);
		if (err && (err != -ENOENT)) {
			return err;
		}
		err = export_to_user(file_ctx, vi, &len);
		if (err) {
			return err;
		}
		advance_nbytes(file_ctx, len);
	}
	return 0;
}

static int read_by_head_leaf(struct voluta_file_ctx *file_ctx)
{
	int err;
	size_t len;
	struct voluta_vnode_info *vi = NULL;

	if (!file_is_leaf_mode(file_ctx->ii)) {
		return 0;
	}
	if (!has_more_leaf_io(file_ctx)) {
		return 0;
	}
	err = stage_file_head_leaf(file_ctx, &vi);
	if (err && (err != -ENOENT)) {
		return err;
	}
	err = export_to_user(file_ctx, vi, &len);
	if (err) {
		return err;
	}
	advance_nbytes(file_ctx, len);
	return 0;
}

static int read_data(struct voluta_file_ctx *file_ctx)
{
	int err;

	err = read_by_head_leaf(file_ctx);
	if (err) {
		return err;
	}
	err = read_by_tree_map(file_ctx);
	if (err) {
		return err;
	}
	return 0;
}

struct voluta_read_iter {
	struct voluta_rw_iter rwi;
	uint8_t *dat;
	size_t dat_len;
	size_t dat_max;
};

static struct voluta_read_iter *read_iter_of(const struct voluta_rw_iter *rwi)
{
	return container_of(rwi, struct voluta_read_iter, rwi);
}

static int read_iter_actor(struct voluta_rw_iter *rwi,
			   const struct voluta_qmref *mb)
{
	struct voluta_read_iter *ri = read_iter_of(rwi);

	if ((mb->fd > 0) && (mb->off < 0)) {
		return -EINVAL;
	}
	if ((ri->dat_len + mb->len) > ri->dat_max) {
		return -EINVAL;
	}
	memcpy(ri->dat + ri->dat_len, mb->mem, mb->len);
	ri->dat_len += mb->len;
	return 0;
}

static void setup_read_iter(struct voluta_read_iter *ri,
			    void *buf, size_t len, loff_t off)
{
	ri->rwi.actor = read_iter_actor;
	ri->rwi.len = len;
	ri->rwi.off = off;
	ri->dat = buf;
	ri->dat_len = 0;
	ri->dat_max = len;
}

int voluta_do_read(struct voluta_env *env, struct voluta_inode_info *ii,
		   void *buf, size_t len, loff_t off, size_t *out_len)
{
	int err;
	struct voluta_read_iter ri = {
		.dat_len = 0
	};

	setup_read_iter(&ri, buf, len, off);
	err = voluta_do_read_iter(env, ii, &ri.rwi);
	*out_len = ri.dat_len;
	return err;
}

static loff_t rw_iter_end(const struct voluta_rw_iter *rwi)
{
	return off_end(rwi->off, rwi->len);
}

int voluta_do_read_iter(struct voluta_env *env, struct voluta_inode_info *ii,
			struct voluta_rw_iter *rwi)
{
	int err;
	size_t len = 0;
	loff_t end, isz;
	struct voluta_file_ctx file_ctx = {
		.env = env,
		.sbi = &env->sbi,
		.rwi = rwi,
		.ii = ii,
		.start = rwi->off,
		.len = rwi->len,
		.beg = rwi->off,
		.off = rwi->off,
		.end = rw_iter_end(rwi),
		.read = true,
	};

	err = check_file_io(&file_ctx);
	if (err) {
		return err;
	}

	isz = i_size_of(ii);
	end = rw_iter_end(rwi);
	file_ctx.end = off_min(end, isz);

	err = read_data(&file_ctx);
	post_io_update(&file_ctx, &len, false);
	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int alloc_data_leaf_space(const struct voluta_file_ctx *file_ctx,
				 struct voluta_vaddr *out_vaddr)
{
	return voluta_new_vspace(file_ctx->sbi, VOLUTA_VTYPE_DATA,
				 VOLUTA_NKBS_IN_BK, out_vaddr);
}

static int new_data_leaf(const struct voluta_file_ctx *file_ctx,
			 struct voluta_vnode_info **out_vi)
{
	return voluta_new_vnode(file_ctx->sbi, VOLUTA_VTYPE_DATA, out_vi);
}

static int new_radix_tnode(const struct voluta_file_ctx *file_ctx,
			   struct voluta_vnode_info **out_vi)
{
	return voluta_new_vnode(file_ctx->sbi, VOLUTA_VTYPE_RTNODE, out_vi);
}

static int create_radix_tnode(const struct voluta_file_ctx *file_ctx,
			      loff_t off, size_t height,
			      struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi;

	err = new_radix_tnode(file_ctx, &vi);
	if (err) {
		return err;
	}
	radix_tnode_init(vi->u.rtn, i_ino_of(file_ctx->ii), off, height);
	v_dirtify(vi);
	*out_vi = vi;
	return 0;
}

static int create_root_node(const struct voluta_file_ctx *file_ctx,
			    size_t height, struct voluta_vnode_info **out_vi)
{
	voluta_assert_gt(height, 0);

	return create_radix_tnode(file_ctx, 0, height, out_vi);
}

static int create_bind_node(const struct voluta_file_ctx *file_ctx,
			    struct voluta_vnode_info *parent_vi,
			    struct voluta_vnode_info **out_vi)
{
	int err;
	size_t height;
	struct voluta_vnode_info *vi;

	height = radix_tnode_height(parent_vi->u.rtn);
	err = create_radix_tnode(file_ctx, file_ctx->off, height - 1, &vi);
	if (err) {
		return err;
	}
	radix_tnode_set_child(parent_vi->u.rtn, file_ctx->off, v_vaddr_of(vi));
	v_dirtify(parent_vi);
	*out_vi = vi;
	return 0;
}

static void zero_data_frg(const struct voluta_vnode_info *vi)
{
	struct voluta_data_frg *df = vi->u.df;

	memset(df, 0, sizeof(*df));
}

static int create_data_leaf(const struct voluta_file_ctx *file_ctx,
			    struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi = NULL;
	struct voluta_inode_info *ii = file_ctx->ii;
	struct voluta_iattr attr = { .ia_blocks = 0 };

	err = new_data_leaf(file_ctx, &vi);
	if (err) {
		return err;
	}
	zero_data_frg(vi);
	v_dirtify(vi);

	attr.ia_blocks = i_blocks_of(ii) + 1;
	attr.ia_flags = VOLUTA_ATTR_BLOCKS;
	i_update_attr(file_ctx->env, ii, &attr);
	*out_vi = vi;
	return 0;
}

static int create_bind_leaf(const struct voluta_file_ctx *file_ctx,
			    struct voluta_vnode_info *parent_vi,
			    struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi = NULL;

	err = create_data_leaf(file_ctx, &vi);
	if (err) {
		return err;
	}
	radix_tnode_set_child(parent_vi->u.rtn, file_ctx->off, &vi->vaddr);
	v_dirtify(parent_vi);
	*out_vi = vi;
	return 0;
}

static void set_new_root(const struct voluta_file_ctx *file_ctx,
			 const struct voluta_vaddr *vaddr, size_t height)
{
	struct voluta_inode_info *ii = file_ctx->ii;

	file_tree_update(ii, vaddr, height);
	i_dirtify(ii);
}

static void bind_sub_tree(struct voluta_file_ctx *file_ctx,
			  struct voluta_vnode_info *vi)
{
	size_t height;
	struct voluta_vaddr root_vaddr;

	file_tree_root(file_ctx->ii, &root_vaddr);
	radix_tnode_set_child_at(vi->u.rtn, 0, &root_vaddr);
	v_dirtify(vi);

	height = radix_tnode_height(vi->u.rtn);
	set_new_root(file_ctx, v_vaddr_of(vi), height);
}

static int create_tree_levels(struct voluta_file_ctx *file_ctx)
{
	int err;
	size_t new_height, cur_height;
	struct voluta_vnode_info *vi;

	new_height = radix_tnode_height_by_offset(NULL, file_ctx->off);
	cur_height = voluta_max(1, file_tree_height(file_ctx->ii));
	while (new_height > cur_height) {
		err = create_root_node(file_ctx, ++cur_height, &vi);
		if (err) {
			return err;
		}
		bind_sub_tree(file_ctx, vi);
	}
	return 0;
}

static int stage_or_create_leaf(const struct voluta_file_ctx *file_ctx,
				struct voluta_vnode_info *parent_vi,
				struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vaddr vaddr;

	radix_tnode_get_child(parent_vi->u.rtn, file_ctx->off, &vaddr);
	if (!vaddr_isnull(&vaddr)) {
		return stage_data_leaf(file_ctx, &vaddr, out_vi);
	}
	err = create_bind_leaf(file_ctx, parent_vi, out_vi);
	if (err) {
		return err;
	}
	return 0;
}

static int stage_or_create_node(const struct voluta_file_ctx *file_ctx,
				struct voluta_vnode_info *parent_vi,
				struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vaddr vaddr;

	radix_tnode_get_child(parent_vi->u.rtn, file_ctx->off, &vaddr);

	if (!vaddr_isnull(&vaddr)) {
		err = stage_radix_tnode(file_ctx, &vaddr, out_vi);
	} else {
		err = create_bind_node(file_ctx, parent_vi, out_vi);
	}
	return err;
}

static int stage_or_create_tree_path(struct voluta_file_ctx *file_ctx,
				     struct voluta_vnode_info **out_vi)
{
	int err;
	size_t level, height;
	struct voluta_vnode_info *tnode_vi;

	level = height = file_tree_height(file_ctx->ii);
	err = stage_file_tree_root(file_ctx, &tnode_vi);
	while (!err && (level > 0)) {
		if (radix_tnode_isbottom(tnode_vi->u.rtn)) {
			return stage_or_create_leaf(file_ctx, tnode_vi, out_vi);
		}
		err = stage_or_create_node(file_ctx, tnode_vi, &tnode_vi);
		level -= 1;
	}
	return err ? err : -EIO;
}

static int stage_or_create_tree_map(struct voluta_file_ctx *file_ctx,
				    struct voluta_vnode_info **out_vi)
{
	int err;

	err = create_tree_levels(file_ctx);
	if (err) {
		return err;
	}
	err = stage_or_create_tree_path(file_ctx, out_vi);
	if (err) {
		return err;
	}
	return 0;
}

static int write_by_tree_map(struct voluta_file_ctx *file_ctx)
{
	int err;
	size_t len;
	struct voluta_vnode_info *vi;

	while (has_more_io(file_ctx)) {
		vi = NULL;
		err = stage_or_create_tree_map(file_ctx, &vi);
		if (err) {
			return err;
		}
		err = import_from_user(file_ctx, vi, &len);
		if (err) {
			return err;
		}
		advance_nbytes(file_ctx, len);
		v_dirtify(vi);
	}
	return 0;
}

static int stage_or_create_head_leaf(struct voluta_file_ctx *file_ctx,
				     struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi;

	if (file_is_leaf_mode(file_ctx->ii)) {
		return stage_file_head_leaf(file_ctx, out_vi);
	}
	err = create_data_leaf(file_ctx, &vi);
	if (err) {
		return err;
	}
	set_new_root(file_ctx, v_vaddr_of(vi), 1);
	*out_vi = vi;
	return 0;
}

static int write_by_head_leaf(struct voluta_file_ctx *file_ctx)
{
	int err;
	size_t len;
	struct voluta_vnode_info *vi;

	if (!file_is_leaf_mode(file_ctx->ii)) {
		return 0;
	}
	if (!has_more_leaf_io(file_ctx)) {
		return 0;
	}
	err = stage_or_create_head_leaf(file_ctx, &vi);
	if (err) {
		return err;
	}
	err = import_from_user(file_ctx, vi, &len);
	if (err) {
		return err;
	}
	advance_nbytes(file_ctx, len);
	v_dirtify(vi);

	return 0;
}

static int write_data(struct voluta_file_ctx *file_ctx)
{
	int err;

	err = write_by_head_leaf(file_ctx);
	if (err) {
		return err;
	}
	err = write_by_tree_map(file_ctx);
	if (err) {
		return err;
	}
	return 0;
}

struct voluta_write_iter {
	struct voluta_rw_iter rwi;
	const uint8_t *dat;
	size_t dat_len;
	size_t dat_max;
};


static struct voluta_write_iter *write_iter_of(const struct voluta_rw_iter *rwi)
{
	return container_of(rwi, struct voluta_write_iter, rwi);
}

static int write_iter_actor(struct voluta_rw_iter *rwi,
			    const struct voluta_qmref *mb)
{
	struct voluta_write_iter *wi = write_iter_of(rwi);

	if ((mb->fd > 0) && (mb->off < 0)) {
		return -EINVAL;
	}
	if ((wi->dat_len + mb->len) > wi->dat_max) {
		return -EINVAL;
	}
	memcpy(mb->mem, wi->dat + wi->dat_len, mb->len);
	wi->dat_len += mb->len;
	return 0;
}

static void setup_write_iter(struct voluta_write_iter *wi,
			     const void *buf, size_t len, loff_t off)
{
	wi->rwi.actor = write_iter_actor;
	wi->rwi.len = len;
	wi->rwi.off = off;
	wi->dat = buf;
	wi->dat_len = 0;
	wi->dat_max = len;
}

int voluta_do_write(struct voluta_env *env, struct voluta_inode_info *ii,
		    const void *buf, size_t len, loff_t off, size_t *out_len)
{
	int err;
	struct voluta_write_iter wi = {
		.dat_len = 0
	};

	setup_write_iter(&wi, buf, len, off);
	err = voluta_do_write_iter(env, ii, &wi.rwi);
	*out_len = wi.dat_len;
	return err;
}

int voluta_do_write_iter(struct voluta_env *env,
			 struct voluta_inode_info *ii,
			 struct voluta_rw_iter *rwi)
{
	int err;
	size_t len = 0;
	struct voluta_file_ctx file_ctx = {
		.env = env,
		.sbi = &env->sbi,
		.rwi = rwi,
		.ii = ii,
		.start = rwi->off,
		.len = rwi->len,
		.beg = rwi->off,
		.off = rwi->off,
		.end = rw_iter_end(rwi),
		.write = true
	};

	err = check_file_io(&file_ctx);
	if (err) {
		return err;
	}
	err = write_data(&file_ctx);
	post_io_update(&file_ctx, &len, err == 0);
	return err;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_do_flush(struct voluta_env *env, struct voluta_inode_info *ii)
{
	/* TODO: Report if errors with this ii */
	voluta_unused(env);
	voluta_unused(ii);
	return 0;
}

int voluta_do_fsync(struct voluta_env *env,
		    struct voluta_inode_info *ii, bool datasync)
{
	int err;
	struct voluta_file_ctx file_ctx = {
		.env = env,
		.sbi = &env->sbi,
		.ii = ii,
		.start = 0,
		.len = 1,
		.trunc = false
	};

	err = check_file_io(&file_ctx);
	if (err) {
		return err;
	}
	err = voluta_vdi_sync(file_ctx.sbi->s_vdi, !datasync);
	if (err) {
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int drop_recursive(struct voluta_file_ctx *, struct voluta_vnode_info *);

static int discard_data_leaf(const struct voluta_file_ctx *file_ctx,
			     const struct voluta_vaddr *vaddr)
{
	int err;
	struct voluta_iattr attr;
	struct voluta_inode_info *ii = file_ctx->ii;

	voluta_assert_eq(vaddr->vtype, VOLUTA_VTYPE_DATA);

	if (vaddr_isnull(vaddr)) {
		return 0;
	}
	err = voluta_del_vnode_at(file_ctx->sbi, vaddr);
	if (err) {
		return err;
	}
	attr_reset(&attr);
	attr.ia_ino = i_ino_of(ii);
	attr.ia_blocks = i_blocks_of(ii) - 1;
	attr.ia_flags = VOLUTA_ATTR_BLOCKS;
	i_update_attr(file_ctx->env, ii, &attr);
	return 0;
}

static int del_file_tnode(const struct voluta_file_ctx *file_ctx,
			  struct voluta_vnode_info *vi)
{
	return voluta_del_vnode(file_ctx->sbi, vi);
}

static int drop_subtree(struct voluta_file_ctx *file_ctx,
			const struct voluta_vaddr *vaddr)
{
	int err;
	struct voluta_vnode_info *vi;

	if (vaddr_isnull(vaddr)) {
		return 0;
	}
	err = stage_radix_tnode(file_ctx, vaddr, &vi);
	if (err) {
		return err;
	}
	err = drop_recursive(file_ctx, vi);
	if (err) {
		return err;
	}
	err = del_file_tnode(file_ctx, vi);
	if (err) {
		return err;
	}
	return 0;
}

static int
drop_subtree_at(struct voluta_file_ctx *file_ctx,
		const struct voluta_vnode_info *parent_vi, size_t slot)
{
	int err;
	struct voluta_vaddr vaddr;

	radix_tnode_get_child_at(parent_vi->u.rtn, slot, &vaddr);
	if (vaddr_isleaf(&vaddr)) {
		err = discard_data_leaf(file_ctx, &vaddr);
	} else {
		err = drop_subtree(file_ctx, &vaddr);
	}
	return err;
}

static int drop_recursive(struct voluta_file_ctx *file_ctx,
			  struct voluta_vnode_info *vi)
{
	int err;
	size_t slot, nslots_max;

	nslots_max = radix_tnode_nslots(vi->u.rtn);
	for (slot = 0; slot < nslots_max; ++slot) {
		err = drop_subtree_at(file_ctx, vi, slot);
		if (err) {
			return err;
		}
	}
	return 0;
}

static int drop_by_tree_map(struct voluta_file_ctx *file_ctx)
{
	int err;
	struct voluta_vnode_info *vi = NULL;

	if (!file_is_tree_mode(file_ctx->ii)) {
		return 0;
	}
	err = stage_file_tree_root(file_ctx, &vi);
	if (err) {
		return err;
	}
	err = drop_recursive(file_ctx, vi);
	if (err) {
		return err;
	}
	err = del_file_tnode(file_ctx, vi);
	if (err) {
		return err;
	}
	return 0;
}

static int drop_by_head_leaf(struct voluta_file_ctx *file_ctx)
{
	int err;
	struct voluta_vaddr vaddr;

	if (!file_is_leaf_mode(file_ctx->ii)) {
		return 0;
	}
	file_tree_root(file_ctx->ii, &vaddr);
	err = discard_data_leaf(file_ctx, &vaddr);
	if (err) {
		return err;
	}
	return 0;
}

static void reset_tree_mappings(struct voluta_inode_info *ii)
{
	file_tree_setup(ii);
	i_dirtify(ii);
}

static int drop_meta_and_data(struct voluta_file_ctx *file_ctx)
{
	int err;
	struct voluta_inode_info *ii = file_ctx->ii;

	err = drop_by_head_leaf(file_ctx);
	if (err) {
		return err;
	}
	err = drop_by_tree_map(file_ctx);
	if (err) {
		return err;
	}
	reset_tree_mappings(ii);
	return 0;
}

int voluta_drop_reg(struct voluta_env *env, struct voluta_inode_info *ii)
{
	struct voluta_file_ctx file_ctx = {
		.env = env,
		.sbi = &env->sbi,
		.ii = ii
	};

	return drop_meta_and_data(&file_ctx);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int resolve_discard_end(struct voluta_file_ctx *file_ctx)
{
	int err;
	loff_t tree_end;
	struct voluta_vnode_info *vi;

	if (file_is_leaf_mode(file_ctx->ii)) {
		file_ctx->end = off_max(file_ctx->off, VOLUTA_BK_SIZE);
		return 0;
	}
	if (!file_is_tree_mode(file_ctx->ii)) {
		file_ctx->end = 0;
		return 0;
	}
	err = stage_file_tree_root(file_ctx, &vi);
	if (err) {
		return err;
	}
	tree_end = radix_tnode_end(vi->u.rtn);
	file_ctx->end = off_max(tree_end, file_ctx->off);
	return 0;
}

static int discard_partial(const struct voluta_file_ctx *file_ctx,
			   const struct voluta_radix_leaf_info *rli)
{
	int err;
	loff_t off;
	size_t len;
	struct voluta_vnode_info *vi;

	err = stage_data_leaf(file_ctx, &rli->vaddr, &vi);
	if (err) {
		return err;
	}
	off = rli->file_pos;
	len = len_within_df(off, file_ctx->end);
	voluta_assert(len < VOLUTA_BK_SIZE);

	if (off_is_df_start(off)) {
		zero_headn(vi->u.df, len);
	} else {
		zero_range(vi->u.df, off_in_df(off), len);
	}
	v_dirtify(vi);
	return 0;
}

static void clear_subtree_mappings(struct voluta_vnode_info *vi, size_t slot)
{
	radix_tnode_reset_child_at(vi->u.rtn, slot);
	v_dirtify(vi);
}

static int discard_entire(struct voluta_file_ctx *file_ctx,
			  const struct voluta_radix_leaf_info *rli)
{
	int err;

	err = discard_data_leaf(file_ctx, &rli->vaddr);
	if (err) {
		return err;
	}
	if (rli->parent_vi != NULL) {
		clear_subtree_mappings(rli->parent_vi, rli->parent_slot);
	} else {
		reset_tree_mappings(file_ctx->ii);
	}
	return 0;
}

static int discard_data_at(struct voluta_file_ctx *file_ctx,
			   const struct voluta_radix_leaf_info *rli)
{
	int err = 0;
	size_t len, bk_size = VOLUTA_BK_SIZE;

	if (vaddr_isnull(&rli->vaddr)) {
		return 0;
	}
	len = len_within_df(rli->file_pos, file_ctx->end);
	if (len < bk_size) {
		err = discard_partial(file_ctx, rli);
	} else {
		err = discard_entire(file_ctx, rli);
	}
	return err;
}

static int discard_by_tree_map(struct voluta_file_ctx *file_ctx)
{
	int err;
	struct voluta_radix_leaf_info rli;
	struct voluta_sb_info *sbi = &file_ctx->env->sbi;

	if (!file_is_tree_mode(file_ctx->ii)) {
		return 0;
	}
	while (has_more_io(file_ctx)) {
		err = voluta_commit_dirtyq(sbi, 0); /* TODO: needed ? */
		if (err) {
			return err;
		}
		err = iterate_tree_map(file_ctx, &rli);
		if (err == -ENOENT) {
			break;
		} else if (err) {
			return err;
		}
		err = discard_data_at(file_ctx, &rli);
		if (err) {
			return err;
		}
		/* TODO: need to skip large holes */
		advance_to_next_bk(file_ctx);
	}
	return 0;
}

static int discard_by_head_leaf(struct voluta_file_ctx *file_ctx)
{
	int err;
	struct voluta_radix_leaf_info rli = {
		.ii = file_ctx->ii,
		.parent_vi = NULL
	};

	if (!file_is_leaf_mode(file_ctx->ii)) {
		return 0;
	}
	if (!has_head_leaf_io(file_ctx)) {
		return 0;
	}
	file_tree_root(file_ctx->ii, &rli.vaddr);
	err = discard_data_at(file_ctx, &rli);
	if (err) {
		return err;
	}
	advance_to_next_bk(file_ctx);
	return 0;
}

static int discard_data(struct voluta_file_ctx *file_ctx)
{
	int err;

	err = discard_by_head_leaf(file_ctx);
	if (err) {
		return err;
	}
	err = discard_by_tree_map(file_ctx);
	if (err) {
		return err;
	}
	return 0;
}

static int discard_unused_meta(struct voluta_file_ctx *file_ctx)
{
	int err = 0;

	if (file_ctx->beg == 0) {
		err = drop_meta_and_data(file_ctx);
	}
	return err;
}

int voluta_do_truncate(struct voluta_env *env,
		       struct voluta_inode_info *ii, loff_t off)
{
	int err;
	size_t len = 0;
	struct voluta_file_ctx file_ctx = {
		.env = env,
		.sbi = &env->sbi,
		.ii = ii,
		.start = off,
		.len = 0,
		.beg = off,
		.off = off,
		.end = off,
		.trunc = true
	};

	err = check_file_io(&file_ctx);
	if (err) {
		return err;
	}
	err = resolve_discard_end(&file_ctx);
	if (err) {
		return err;
	}
	err = discard_data(&file_ctx);
	if (err) {
		return err;
	}
	err = discard_unused_meta(&file_ctx);
	if (err) {
		return err;
	}
	post_io_update(&file_ctx, &len, false);
	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int
_seek_notsupp(const struct voluta_inode_info *ii, loff_t off, loff_t *out_off)
{
	(void)ii;
	(void)off;
	*out_off = -1;
	return -ENOTSUP;
}

static int _seek_data(struct voluta_inode_info *ii, loff_t off,
		      loff_t *out_off)
{
	return _seek_notsupp(ii, off, out_off);
}

static int _seek_hole(struct voluta_inode_info *ii, loff_t off,
		      loff_t *out_off)
{
	return _seek_notsupp(ii, off, out_off);
}

int voluta_lseek(struct voluta_inode_info *rii,
		 loff_t offset, int whence, loff_t *out_off)
{
	if (whence == SEEK_DATA) {
		return _seek_data(rii, offset, out_off);
	}
	if (whence == SEEK_HOLE) {
		return _seek_hole(rii, offset, out_off);
	}

	return -ENOTSUP;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool fl_reserve_range(const struct voluta_file_ctx *file_ctx)
{
	const int fl_mask = FALLOC_FL_KEEP_SIZE;

	return (file_ctx->fl_mode & ~fl_mask) == 0;
}

static bool fl_keep_size(const struct voluta_file_ctx *file_ctx)
{
	const int fl_mask = FALLOC_FL_KEEP_SIZE;

	return (file_ctx->fl_mode & fl_mask) == fl_mask;
}

static bool fl_punch_hole(const struct voluta_file_ctx *file_ctx)
{
	const int fl_mask = FALLOC_FL_PUNCH_HOLE;

	return (file_ctx->fl_mode & fl_mask) == fl_mask;
}

static int check_fl_mode(const struct voluta_file_ctx *file_ctx)
{
	int err, fl_mode = file_ctx->fl_mode;

	if ((fl_mode & FALLOC_FL_PUNCH_HOLE) &&
	    !(fl_mode & FALLOC_FL_KEEP_SIZE)) {
		return -EINVAL;
	}
	if (fl_mode & FALLOC_FL_KEEP_SIZE) {
		fl_mode &= ~FALLOC_FL_KEEP_SIZE;
	}
	if (!fl_mode) {
		err = 0;
	} else if (fl_mode & FALLOC_FL_PUNCH_HOLE) {
		err = 0;
	} else if (fl_mode & FALLOC_FL_COLLAPSE_RANGE) {
		err = -ENOTSUP;
	} else if (fl_mode & FALLOC_FL_INSERT_RANGE) {
		err = -ENOTSUP;
	} else if (fl_mode & FALLOC_FL_UNSHARE_RANGE) {
		err = -ENOTSUP;
	} else if (fl_mode & FALLOC_FL_NO_HIDE_STALE) {
		err = -ENOTSUP;
	} else {
		err = -ENOTSUP;
	}
	return err;
}

static int reserve_data_leaf(const struct voluta_file_ctx *file_ctx,
			     struct voluta_vaddr *out_vaddr)
{
	int err;
	struct voluta_inode_info *ii = file_ctx->ii;
	struct voluta_iattr attr = { .ia_blocks = 0 };

	err = alloc_data_leaf_space(file_ctx, out_vaddr);
	if (err) {
		return err;
	}
	attr.ia_blocks = i_blocks_of(ii) + 1;
	attr.ia_flags = VOLUTA_ATTR_BLOCKS;
	i_update_attr(file_ctx->env, ii, &attr);
	return 0;
}

static int reserve_bind_leaf(const struct voluta_file_ctx *file_ctx,
			     struct voluta_vnode_info *parent_vi)
{
	int err;
	struct voluta_vaddr vaddr;

	radix_tnode_get_child(parent_vi->u.rtn, file_ctx->off, &vaddr);
	if (!vaddr_isnull(&vaddr)) {
		return 0;
	}
	err = reserve_data_leaf(file_ctx, &vaddr);
	if (err) {
		return err;
	}
	radix_tnode_set_child(parent_vi->u.rtn, file_ctx->off, &vaddr);
	v_dirtify(parent_vi);
	return 0;
}

static int reserve_leaf(struct voluta_file_ctx *file_ctx)
{
	int err;
	size_t level, height;
	struct voluta_vnode_info *vi;

	level = height = file_tree_height(file_ctx->ii);
	err = stage_file_tree_root(file_ctx, &vi);
	if (err) {
		return err;
	}
	while (level > 0) {
		if (radix_tnode_isbottom(vi->u.rtn)) {
			return reserve_bind_leaf(file_ctx, vi);
		}
		err = stage_or_create_node(file_ctx, vi, &vi);
		if (err) {
			return err;
		}
		level--;
	}
	return -EFSCORRUPTED;
}

static int reserve_by_tree_map(struct voluta_file_ctx *file_ctx)
{
	int err;

	err = create_tree_levels(file_ctx);
	if (err) {
		return err;
	}
	err = reserve_leaf(file_ctx);
	if (err) {
		return err;
	}
	return 0;
}

static int fallocate_reserve_by_tree_map(struct voluta_file_ctx *file_ctx)
{
	int err;

	while (has_more_io(file_ctx)) {
		err = reserve_by_tree_map(file_ctx);
		if (err) {
			return err;
		}
		advance_to_next_pos(file_ctx);
	}
	return 0;
}

static int fallocate_reserve_by_head_leaf(struct voluta_file_ctx *file_ctx)
{
	int err;
	struct voluta_vnode_info *vi;

	if (!has_more_leaf_io(file_ctx)) {
		return 0;
	}
	if (file_is_tree_mode(file_ctx->ii)) {
		return 0;
	}
	err = stage_or_create_head_leaf(file_ctx, &vi);
	if (err) {
		return err;
	}
	advance_to_next_pos(file_ctx);
	return 0;
}

static int fallocate_reserve(struct voluta_file_ctx *file_ctx)
{
	int err;

	err = fallocate_reserve_by_head_leaf(file_ctx);
	if (err) {
		return err;
	}
	err = fallocate_reserve_by_tree_map(file_ctx);
	if (err) {
		return err;
	}
	return 0;
}

static int fallocate_punch_hole(struct voluta_file_ctx *file_ctx)
{
	return discard_data(file_ctx);
}

static int do_fallocate(struct voluta_file_ctx *file_ctx)
{
	int err;

	if (fl_reserve_range(file_ctx)) {
		err = fallocate_reserve(file_ctx);
	} else if (fl_punch_hole(file_ctx)) {
		err = fallocate_punch_hole(file_ctx);
	} else {
		err = -ENOTSUP;
	}
	return err;
}

int voluta_do_fallocate(struct voluta_env *env, struct voluta_inode_info *ii,
			int mode, loff_t off, loff_t length)
{
	int err;
	size_t len = (size_t)length;
	struct voluta_file_ctx file_ctx = {
		.env = env,
		.sbi = &env->sbi,
		.ii = ii,
		.start = off,
		.len = len,
		.beg = off,
		.off = off,
		.end = off_end(off, len),
		.falloc = true,
		.fl_mode = mode,
	};

	err = check_file_io(&file_ctx);
	if (err) {
		return err;
	}
	err = check_fl_mode(&file_ctx);
	if (err) {
		return err;
	}
	err = do_fallocate(&file_ctx);
	if (err) {
		return err;
	}
	post_io_update(&file_ctx, &len, false);
	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_setup_new_reg(struct voluta_inode_info *ii)
{
	reset_tree_mappings(ii);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_verify_radix_tnode(const struct voluta_radix_tnode *rtn)
{
	int err;
	size_t height, span, spbh;

	err = voluta_verify_ino(rtn->r_ino);
	if (err) {
		return err;
	}
	if ((rtn->r_beg < 0) || (rtn->r_end < 0)) {
		return -EFSCORRUPTED;
	}
	if (rtn->r_beg >= rtn->r_end) {
		return -EFSCORRUPTED;
	}
	if (rtn->r_flags != 0) {
		return -EFSCORRUPTED;
	}
	height = radix_tnode_height(rtn);
	if ((height <= 1) || (height > 7)) {
		return -EFSCORRUPTED;
	}
	span = radix_tnode_span(rtn);
	spbh = radix_tnode_span_by_height(rtn, height);
	if (span != spbh) {
		return -EFSCORRUPTED;
	}
	return 0;
}
