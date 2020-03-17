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
	struct voluta_sb_info    *sbi;
	const struct voluta_oper_info *opi;
	struct voluta_inode_info *ii;
	struct voluta_rw_iter    *rwi;
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
	uint64_t uv = (uint64_t)off;

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

	return (loff_t)uv;
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

static loff_t off_to_ds(loff_t off)
{
	return off_align(off, VOLUTA_DS_SIZE);
}

static loff_t off_to_ds_up(loff_t off)
{
	return off_align(off + VOLUTA_DS_SIZE - 1, VOLUTA_DS_SIZE);
}

static loff_t off_to_next_ds(loff_t off)
{
	return off_to_ds(off + VOLUTA_DS_SIZE);
}

static loff_t off_in_ds(loff_t off)
{
	return off % VOLUTA_DS_SIZE;
}

static bool off_is_ds_start(loff_t off)
{
	return (off_to_ds(off) == off);
}

static size_t len_to_next_ds(loff_t off)
{
	loff_t off_next = off_to_next_ds(off);

	return (size_t)(off_next - off);
}

static size_t len_within_ds(loff_t off, loff_t end)
{
	const loff_t off_next = off_to_next_ds(off);

	return (size_t)((off_next < end) ? (off_next - off) : (end - off));
}

static size_t off_len_nds(loff_t off, size_t len)
{
	const loff_t beg = off_to_ds(off);
	const loff_t end = off_to_ds_up(off_end(off, len));

	return (size_t)(end - beg) / VOLUTA_DS_SIZE;
}

static bool off_is_inrange(loff_t off, loff_t beg, loff_t end)
{
	return (beg <= off) && (off < end);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void vaddr_of_leaf(struct voluta_vaddr *vaddr, loff_t off)
{
	voluta_vaddr_by_off(vaddr, VOLUTA_VTYPE_DATA, off);
}

static void vaddr_of_tnode(struct voluta_vaddr *vaddr, loff_t off)
{
	voluta_vaddr_by_off(vaddr, VOLUTA_VTYPE_RTNODE, off);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void zero_range(struct voluta_data_seg *ds, loff_t off, size_t len)
{
	voluta_assert(len <= sizeof(ds->dat));
	voluta_assert((size_t)off + len <= sizeof(ds->dat));

	voluta_memzero(ds->dat + off, len);
}

static void zero_headn(struct voluta_data_seg *ds, size_t n)
{
	zero_range(ds, 0, n);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static ino_t rtn_ino(const struct voluta_radix_tnode *rtn)
{
	return ino_to_cpu(rtn->r_ino);
}

static void rtn_set_ino(struct voluta_radix_tnode *rtn, ino_t ino)
{
	rtn->r_ino = cpu_to_ino(ino);
}

static loff_t rtn_beg(const struct voluta_radix_tnode *rtn)
{
	return off_to_cpu(rtn->r_beg);
}

static void rtn_set_beg(struct voluta_radix_tnode *rtn, loff_t beg)
{
	rtn->r_beg = cpu_to_off(beg);
}

static loff_t rtn_end(const struct voluta_radix_tnode *rtn)
{
	return off_to_cpu(rtn->r_end);
}

static void rtn_set_end(struct voluta_radix_tnode *rtn, loff_t end)
{
	rtn->r_end = cpu_to_off(end);
}

static size_t rtn_nchilds_max(const struct voluta_radix_tnode *rtn)
{
	return ARRAY_SIZE(rtn->r_child);
}

static size_t rtn_span(const struct voluta_radix_tnode *rtn)
{
	return off_len(rtn_beg(rtn), rtn_end(rtn));
}

static size_t rtn_height(const struct voluta_radix_tnode *rtn)
{
	return rtn->r_height;
}

static void rtn_set_height(struct voluta_radix_tnode *rtn, size_t height)
{
	voluta_assert_lt(height, 7);
	rtn->r_height = (uint8_t)height;
}

static bool rtn_isbottom(const struct voluta_radix_tnode *rtn)
{
	const size_t height = rtn_height(rtn);

	voluta_assert_ge(height, 2);
	voluta_assert_le(height, 5);

	return (height == 2);
}

static size_t rtn_nbytes_per_slot(const struct voluta_radix_tnode *rtn)
{
	return rtn_span(rtn) / rtn_nchilds_max(rtn);
}

static size_t
rtn_slot_by_file_pos(const struct voluta_radix_tnode *rtn, loff_t file_pos)
{
	loff_t roff;
	size_t slot;
	const size_t span = rtn_span(rtn);
	const size_t nslots = rtn_nchilds_max(rtn);

	roff = off_diff(rtn_beg(rtn), file_pos);
	slot = ((size_t)roff * nslots) / span;
	voluta_assert_lt(slot, nslots);
	voluta_assert_ge((size_t)roff * nslots, roff);

	return slot;
}

static size_t
rtn_height_by_offset(const struct voluta_radix_tnode *rtn, loff_t off)
{
	loff_t xoff;
	bool loop = true;
	size_t height = 1;
	const size_t nslots = rtn_nchilds_max(rtn);

	xoff = off / VOLUTA_DS_SIZE;
	while (loop) {
		++height;
		xoff /= (loff_t)nslots;
		loop = (xoff > 0);
	}
	return height;
}

static loff_t rtn_child_at(const struct voluta_radix_tnode *rtn, size_t slot)
{
	return int56_to_off(&rtn->r_child[slot]);
}

static void rtn_set_child_at(struct voluta_radix_tnode *rtn,
			     size_t slot, loff_t off)
{
	off_to_int56(off, &rtn->r_child[slot]);
}

static void rtn_reset_child_at(struct voluta_radix_tnode *rtn, size_t slot)
{
	rtn_set_child_at(rtn, slot, VOLUTA_OFF_NULL);
}

static void rtn_resolve_child(const struct voluta_radix_tnode *rtn,
			      loff_t child_off, struct voluta_vaddr *vaddr)
{
	if (rtn_isbottom(rtn)) {
		vaddr_of_leaf(vaddr, child_off);
	} else {
		vaddr_of_tnode(vaddr, child_off);
	}
}

static void rtn_child_by_slot(const struct voluta_radix_tnode *rtn,
			      size_t slot, struct voluta_vaddr *vaddr)
{
	rtn_resolve_child(rtn, rtn_child_at(rtn, slot), vaddr);
}

static void rtn_child_by_file_pos(const struct voluta_radix_tnode *rtn,
				  loff_t file_pos, struct voluta_vaddr *vaddr)
{
	const size_t slot = rtn_slot_by_file_pos(rtn, file_pos);

	rtn_child_by_slot(rtn, slot, vaddr);
}

static void
rtn_set_child_by_file_pos(struct voluta_radix_tnode *rtn,
			  loff_t file_pos, const struct voluta_vaddr *vaddr)
{
	const size_t slot = rtn_slot_by_file_pos(rtn, file_pos);

	rtn_set_child_at(rtn, slot, vaddr->off);
}

static bool rtn_isinrange(const struct voluta_radix_tnode *rtn, loff_t file_pos)
{
	return off_is_inrange(file_pos, rtn_beg(rtn), rtn_end(rtn));
}

static size_t
rtn_span_by_height(const struct voluta_radix_tnode *rtn, size_t height)
{
	size_t nbytes = VOLUTA_DS_SIZE;
	const size_t nslots = rtn_nchilds_max(rtn);

	voluta_assert_lt(height, 6);
	voluta_assert_gt(height, 1);

	/* TODO: Use SHIFT instead of loop */
	for (size_t i = 2; i <= height; ++i) {
		nbytes *= nslots;
	}
	return nbytes ? nbytes : LONG_MAX; /* make clang-scan happy */
}

static void rtn_calc_range(const struct voluta_radix_tnode *rtn,
			   loff_t off, size_t height, loff_t *beg, loff_t *end)
{
	const loff_t span = (loff_t)rtn_span_by_height(rtn, height);

	*beg = off_align(off, span);
	*end = *beg + span;
}

static loff_t rtn_offset_at(const struct voluta_radix_tnode *rtn, size_t slot)
{
	loff_t next_off;
	const size_t nbps = rtn_nbytes_per_slot(rtn);

	next_off = off_end(rtn_beg(rtn), slot * nbps);
	return off_to_ds(next_off);
}

static loff_t rtn_next_offset(const struct voluta_radix_tnode *rtn, size_t slot)
{
	size_t nbps;
	loff_t soff;

	soff = rtn_offset_at(rtn, slot);
	nbps = rtn_nbytes_per_slot(rtn);
	return off_end(soff, nbps);
}

static void rtn_clear_childs(struct voluta_radix_tnode *rtn)
{
	const size_t nslots_max = rtn_nchilds_max(rtn);

	for (size_t slot = 0; slot < nslots_max; ++slot) {
		rtn_reset_child_at(rtn, slot);
	}
}

static void rtn_init(struct voluta_radix_tnode *rtn,
		     ino_t ino, loff_t off, size_t height)
{
	loff_t beg, end;

	rtn_calc_range(rtn, off, height, &beg, &end);
	rtn_set_ino(rtn, ino);
	rtn_set_beg(rtn, beg);
	rtn_set_end(rtn, end);
	rtn_set_height(rtn, height);
	rtn_clear_childs(rtn);
	voluta_memzero(rtn->r_zeros, sizeof(rtn->r_zeros));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_reg_ispec *reg_ispec_of(const struct voluta_inode_info *ii)
{
	struct voluta_inode *inode = ii->inode;

	return &inode->i_s.r;
}

static void file_tree_setup(struct voluta_inode_info *ii)
{
	struct voluta_reg_ispec *ris = reg_ispec_of(ii);

	ris->r_tree_root_off = VOLUTA_OFF_NULL;
	ris->r_tree_height = 0;
}

static size_t file_tree_height(const struct voluta_inode_info *ii)
{
	const struct voluta_reg_ispec *ris = reg_ispec_of(ii);

	return ris->r_tree_height;
}

static void file_tree_root(const struct voluta_inode_info *ii,
			   struct voluta_vaddr *vaddr)
{
	const size_t height = file_tree_height(ii);
	const struct voluta_reg_ispec *ris = reg_ispec_of(ii);

	if (height >= 2) {
		vaddr_of_tnode(vaddr, ris->r_tree_root_off);
	} else if (height == 1) {
		vaddr_of_leaf(vaddr, ris->r_tree_root_off);
	} else {
		vaddr_reset(vaddr);
	}
}

static void file_tree_update(struct voluta_inode_info *ii,
			     const struct voluta_vaddr *vaddr, size_t height)
{
	struct voluta_reg_ispec *ris = reg_ispec_of(ii);

	ris->r_tree_root_off = vaddr->off;
	ris->r_tree_height = (uint32_t)height;
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

static void rli_init(struct voluta_radix_leaf_info *rli,
		     struct voluta_inode_info *ii, loff_t file_pos)
{
	vaddr_reset(&rli->vaddr);
	rli->ii = ii;
	rli->parent_vi = NULL;
	rli->parent_slot = 0;
	rli->file_pos = file_pos;
}

static void rli_set(struct voluta_radix_leaf_info *rli,
		    const struct voluta_vaddr *vaddr,
		    struct voluta_vnode_info *parent_vi,
		    size_t slot, loff_t file_pos)
{
	loff_t leaf_off = rtn_offset_at(parent_vi->u.rtn, slot);

	vaddr_clone(vaddr, &rli->vaddr);
	rli->parent_vi = parent_vi;
	rli->parent_slot = slot;
	rli->file_pos = off_max(leaf_off, file_pos);
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
	const loff_t filesize_max = VOLUTA_FILE_SIZE_MAX;
	const size_t iosize_max = VOLUTA_IO_SIZE_MAX;
	const size_t ndfs_max = VOLUTA_IO_NDS_MAX;

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
	if (file_ctx->write || file_ctx->falloc) {
		if (file_ctx->end > filesize_max) {
			return -EFBIG;
		}
	}
	if (file_ctx->read || file_ctx->write) {
		if (file_ctx->len > iosize_max) {
			return -EINVAL;
		}
		ndfs = off_len_nds(file_ctx->beg, file_ctx->len);
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

static int iter_recursive(struct voluta_file_ctx *file_ctx,
			  struct voluta_vnode_info *parent_vi,
			  struct voluta_radix_leaf_info *rli);

static bool has_more_io(const struct voluta_file_ctx *file_ctx)
{
	return (file_ctx->off < file_ctx->end);
}

static bool is_mapping_boundaries(const struct voluta_file_ctx *file_ctx)
{
	const loff_t mapping_size = (VOLUTA_DS_SIZE * VOLUTA_FILE_MAP_NCHILD);

	return ((file_ctx->off % mapping_size) == 0);
}

static bool has_head_leaf_io(const struct voluta_file_ctx *file_ctx)
{
	return (file_ctx->off < VOLUTA_DS_SIZE);
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
	struct voluta_inode_info *ii = file_ctx->ii;
	const loff_t isz = i_size_of(ii);
	const loff_t off = file_ctx->off;

	iattr_init(&attr, ii);

	*len = io_length(file_ctx);
	if (file_ctx->falloc) {
		attr.ia_flags |= VOLUTA_ATTR_MCTIME;
		if (!fl_keep_size(file_ctx)) {
			attr.ia_flags |= VOLUTA_ATTR_SIZE;
			attr.ia_size = off_max(off, isz);
		}
	} else if (file_ctx->write) {
		attr.ia_flags |= VOLUTA_ATTR_MCTIME | VOLUTA_ATTR_SIZE;
		attr.ia_size = off_max(off, isz);
		if (kill_priv && (*len > 0)) {
			attr.ia_flags |= VOLUTA_ATTR_KILL_PRIV;
		}
	} else if (file_ctx->read) {
		attr.ia_flags |= VOLUTA_ATTR_ATIME | VOLUTA_ATTR_LAZY;
	} else if (file_ctx->trunc) {
		attr.ia_flags |= VOLUTA_ATTR_MCTIME |
				 VOLUTA_ATTR_SIZE | VOLUTA_ATTR_KILL_PRIV;
		attr.ia_size = file_ctx->beg;
	}
	i_update_attr(ii, file_ctx->opi, &attr);
}

static int stage_data_leaf(const struct voluta_file_ctx *file_ctx,
			   const struct voluta_vaddr *vaddr,
			   struct voluta_vnode_info **out_vi)
{
	int err;

	*out_vi = NULL;
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

static int stage_tree_root(const struct voluta_file_ctx *file_ctx,
			   struct voluta_vnode_info **out_vi)
{
	struct voluta_vaddr root_vaddr;

	file_tree_root(file_ctx->ii, &root_vaddr);
	return stage_radix_tnode(file_ctx, &root_vaddr, out_vi);
}

static int stage_head_leaf(const struct voluta_file_ctx *file_ctx,
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
	advance_to(file_ctx, rtn_offset_at(rtn, slot));
}

static void advance_to_next(struct voluta_file_ctx *file_ctx,
			    const struct voluta_radix_tnode *rtn, size_t slot)
{
	advance_to(file_ctx, rtn_next_offset(rtn, slot));
}

static int iter_to_data(struct voluta_file_ctx *file_ctx,
			struct voluta_vnode_info *parent_vi,
			struct voluta_radix_leaf_info *rli)
{
	size_t start_slot;
	size_t nslots_max;
	const loff_t file_pos = file_ctx->off;
	struct voluta_vaddr vaddr;

	start_slot = rtn_slot_by_file_pos(parent_vi->u.rtn, file_pos);
	nslots_max = rtn_nchilds_max(parent_vi->u.rtn);
	for (size_t slot = start_slot; slot < nslots_max; ++slot) {
		advance_to_slot(file_ctx, parent_vi->u.rtn, slot);

		rtn_child_by_slot(parent_vi->u.rtn, slot, &vaddr);
		if (!vaddr_isnull(&vaddr)) {
			rli_set(rli, &vaddr, parent_vi, slot, file_pos);
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

	rtn_child_by_slot(parent_vi->u.rtn, slot, &vaddr);
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
	size_t start_slot;
	size_t nslots_max;
	const loff_t file_pos = file_ctx->off;

	if (!rtn_isinrange(parent_vi->u.rtn, file_pos)) {
		return -ENOENT;
	}
	if (rtn_isbottom(parent_vi->u.rtn)) {
		return iter_to_data(file_ctx, parent_vi, rli);
	}
	start_slot = rtn_slot_by_file_pos(parent_vi->u.rtn, file_pos);
	nslots_max = rtn_nchilds_max(parent_vi->u.rtn);
	for (size_t slot = start_slot; slot < nslots_max; ++slot) {
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

	err = stage_tree_root(file_ctx, &root_vi);
	if (err) {
		return err;
	}
	rli_init(rli, file_ctx->ii, file_ctx->off);
	err = iter_recursive(file_ctx, root_vi, rli);
	if (err) {
		return err;
	}
	return 0;
}

static void advance_by_nbytes(struct voluta_file_ctx *file_ctx, size_t len)
{
	advance_to(file_ctx, off_end(file_ctx->off, len));
}

static void advance_to_next_bk(struct voluta_file_ctx *file_ctx)
{
	advance_by_nbytes(file_ctx, len_to_next_ds(file_ctx->off));
}

static void advance_to_next_pos(struct voluta_file_ctx *file_ctx)
{
	const size_t len = len_within_ds(file_ctx->off, file_ctx->end);

	advance_by_nbytes(file_ctx, len);
}

static int memref_of(const struct voluta_file_ctx *file_ctx,
		     const void *ptr, size_t len, struct voluta_qmref *qmr)
{
	return voluta_memref(file_ctx->sbi->s_qalloc, ptr, len, qmr);
}

static void *membuf_of(struct voluta_file_ctx *file_ctx,
		       const struct voluta_vnode_info *vi)
{
	void *mem;
	const struct voluta_cache *cache = file_ctx->sbi->s_cache;

	if (vi != NULL) {
		mem = vi->u.ds->dat;
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

	cnt = off_in_ds(file_ctx->off);
	len = len_within_ds(file_ctx->off, file_ctx->end);
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

static void child_of_current_pos(const struct voluta_file_ctx *file_ctx,
				 const struct voluta_vnode_info *parent_vi,
				 struct voluta_vaddr *vaddr)
{
	if (parent_vi != NULL) {
		rtn_child_by_file_pos(parent_vi->u.rtn, file_ctx->off, vaddr);
	} else {
		vaddr_of_leaf(vaddr, VOLUTA_OFF_NULL);
	}
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
	err = stage_tree_root(file_ctx, &vi);
	if (err) {
		return err;
	}
	if (!rtn_isinrange(vi->u.rtn, file_ctx->off)) {
		return -ENOENT;
	}
	height = file_tree_height(file_ctx->ii);
	while (height--) {
		if (rtn_isbottom(vi->u.rtn)) {
			*out_vi = vi;
			return 0;
		}
		child_of_current_pos(file_ctx, vi, &vaddr);
		err = stage_radix_tnode(file_ctx, &vaddr, &vi);
		if (err) {
			return err;
		}
	}
	return -EFSCORRUPTED;
}

static int read_from_leaves(struct voluta_file_ctx *file_ctx,
			    struct voluta_vnode_info *parent_vi)
{
	int err;
	size_t len;
	bool next_mapping = false;
	struct voluta_vnode_info *vi;
	struct voluta_vaddr vaddr;

	while (has_more_io(file_ctx) && !next_mapping) {
		child_of_current_pos(file_ctx, parent_vi, &vaddr);
		err = stage_data_leaf(file_ctx, &vaddr, &vi);
		if (err && (err != -ENOENT)) {
			return err;
		}
		err = export_to_user(file_ctx, vi, &len);
		if (err) {
			return err;
		}
		advance_by_nbytes(file_ctx, len);
		next_mapping = is_mapping_boundaries(file_ctx);
	}
	return 0;
}

static int read_by_tree_map(struct voluta_file_ctx *file_ctx)
{
	int err;
	struct voluta_vnode_info *parent_vi;

	while (has_more_io(file_ctx)) {
		parent_vi = NULL;
		err = fetch_by_tree_map(file_ctx, &parent_vi);
		if (err && (err != -ENOENT)) {
			return err;
		}
		err = read_from_leaves(file_ctx, parent_vi);
		if (err) {
			return err;
		}
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
	err = stage_head_leaf(file_ctx, &vi);
	if (err && (err != -ENOENT)) {
		return err;
	}
	err = export_to_user(file_ctx, vi, &len);
	if (err) {
		return err;
	}
	advance_by_nbytes(file_ctx, len);
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

static void read_iter_setup(struct voluta_read_iter *ri,
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

	read_iter_setup(&ri, buf, len, off);
	err = voluta_do_read_iter(env, ii, &ri.rwi);
	*out_len = ri.dat_len;
	return err;
}

static loff_t rw_iter_end(const struct voluta_rw_iter *rwi)
{
	return off_end(rwi->off, rwi->len);
}

static void update_with_rw_iter(struct voluta_file_ctx *file_ctx,
				struct voluta_rw_iter *rwi)
{
	const loff_t end = rw_iter_end(rwi);
	const loff_t isz = i_size_of(file_ctx->ii);

	file_ctx->rwi = rwi;
	file_ctx->start = rwi->off;
	file_ctx->len = rwi->len;
	file_ctx->beg = rwi->off;
	file_ctx->off = rwi->off;
	if (!file_ctx->read) {
		file_ctx->end = end;
	} else {
		file_ctx->end = off_min(end, isz);
	}
}

int voluta_do_read_iter(struct voluta_env *env, struct voluta_inode_info *ii,
			struct voluta_rw_iter *rwi)
{
	int err;
	size_t len = 0;
	struct voluta_file_ctx file_ctx = {
		.sbi = &env->sbi,
		.opi = &env->opi,
		.ii = ii,
		.read = true,
	};

	update_with_rw_iter(&file_ctx, rwi);
	err = check_file_io(&file_ctx);
	if (err) {
		return err;
	}
	err = read_data(&file_ctx);
	post_io_update(&file_ctx, &len, false);
	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int alloc_data_leaf_space(const struct voluta_file_ctx *file_ctx,
				 struct voluta_vaddr *out_vaddr)
{
	return voluta_new_vspace(file_ctx->sbi, VOLUTA_VTYPE_DATA, out_vaddr);
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
	rtn_init(vi->u.rtn, i_ino_of(file_ctx->ii), off, height);
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
	const loff_t file_pos = file_ctx->off;

	height = rtn_height(parent_vi->u.rtn);
	err = create_radix_tnode(file_ctx, file_pos, height - 1, &vi);
	if (err) {
		return err;
	}
	rtn_set_child_by_file_pos(parent_vi->u.rtn, file_pos, v_vaddr_of(vi));
	v_dirtify(parent_vi);
	*out_vi = vi;
	return 0;
}

static void zero_data_seg(const struct voluta_vnode_info *vi)
{
	struct voluta_data_seg *ds = vi->u.ds;

	voluta_memzero(ds, sizeof(*ds));
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
	zero_data_seg(vi);
	v_dirtify(vi);

	attr.ia_blocks = i_blocks_of(ii) + 1;
	attr.ia_flags = VOLUTA_ATTR_BLOCKS;
	i_update_attr(ii, file_ctx->opi, &attr);
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
	rtn_set_child_by_file_pos(parent_vi->u.rtn, file_ctx->off, &vi->vaddr);
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
	rtn_set_child_at(vi->u.rtn, 0, root_vaddr.off);
	v_dirtify(vi);

	height = rtn_height(vi->u.rtn);
	set_new_root(file_ctx, v_vaddr_of(vi), height);
}

static int create_tree_levels(struct voluta_file_ctx *file_ctx)
{
	int err;
	size_t new_height, cur_height;
	struct voluta_vnode_info *vi;

	new_height = rtn_height_by_offset(NULL, file_ctx->off);
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

	child_of_current_pos(file_ctx, parent_vi, &vaddr);
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

	child_of_current_pos(file_ctx, parent_vi, &vaddr);

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
	size_t level;
	struct voluta_vnode_info *tnode_vi;

	level = file_tree_height(file_ctx->ii);
	err = stage_tree_root(file_ctx, &tnode_vi);
	while (!err && (level > 0)) {
		if (rtn_isbottom(tnode_vi->u.rtn)) {
			*out_vi = tnode_vi;
			return 0;
		}
		err = stage_or_create_node(file_ctx, tnode_vi, &tnode_vi);
		level -= 1;
	}
	return err ? err : -EFSCORRUPTED;
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

static int write_to_leaves(struct voluta_file_ctx *file_ctx,
			   struct voluta_vnode_info *tnode_vi)
{
	int err;
	size_t len;
	bool next_mapping = false;
	struct voluta_vnode_info *leaf_vi;

	while (has_more_io(file_ctx) && !next_mapping) {
		err = stage_or_create_leaf(file_ctx, tnode_vi, &leaf_vi);
		if (err) {
			return err;
		}
		err = import_from_user(file_ctx, leaf_vi, &len);
		if (err) {
			return err;
		}
		voluta_clear_unwritten(leaf_vi);
		v_dirtify(leaf_vi);

		advance_by_nbytes(file_ctx, len);
		next_mapping = is_mapping_boundaries(file_ctx);
	}
	return 0;
}

static int write_by_tree_map(struct voluta_file_ctx *file_ctx)
{
	int err;
	struct voluta_vnode_info *vi;

	while (has_more_io(file_ctx)) {
		err = stage_or_create_tree_map(file_ctx, &vi);
		if (err) {
			return err;
		}
		err = write_to_leaves(file_ctx, vi);
		if (err) {
			return err;
		}
	}
	return 0;
}

static int stage_or_create_head_leaf(struct voluta_file_ctx *file_ctx,
				     struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi;

	if (file_is_leaf_mode(file_ctx->ii)) {
		return stage_head_leaf(file_ctx, out_vi);
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
	struct voluta_vnode_info *hleaf_vi;

	if (file_is_tree_mode(file_ctx->ii)) {
		return 0;
	}
	if (!has_more_leaf_io(file_ctx)) {
		return 0;
	}
	err = stage_or_create_head_leaf(file_ctx, &hleaf_vi);
	if (err) {
		return err;
	}
	err = import_from_user(file_ctx, hleaf_vi, &len);
	if (err) {
		return err;
	}
	voluta_clear_unwritten(hleaf_vi);
	advance_by_nbytes(file_ctx, len);
	v_dirtify(hleaf_vi);

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

static void write_iter_setup(struct voluta_write_iter *wi,
			     const void *buf, size_t len, loff_t off)
{
	wi->rwi.actor = write_iter_actor;
	wi->rwi.len = len;
	wi->rwi.off = off;
	wi->dat = buf;
	wi->dat_len = 0;
	wi->dat_max = len;
}

int voluta_do_write(struct voluta_sb_info *sbi,
		    const struct voluta_oper_info *opi,
		    struct voluta_inode_info *ii,
		    const void *buf, size_t len,
		    loff_t off, size_t *out_len)
{
	int err;
	struct voluta_write_iter wi = {
		.dat_len = 0
	};

	write_iter_setup(&wi, buf, len, off);
	err = voluta_do_write_iter(sbi, opi, ii, &wi.rwi);
	*out_len = wi.dat_len;
	return err;
}

int voluta_do_write_iter(struct voluta_sb_info *sbi,
			 const struct voluta_oper_info *opi,
			 struct voluta_inode_info *ii,
			 struct voluta_rw_iter *rwi)
{
	int err;
	size_t len = 0;
	struct voluta_file_ctx file_ctx = {
		.sbi = sbi,
		.opi = opi,
		.ii = ii,
		.write = true
	};

	update_with_rw_iter(&file_ctx, rwi);
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
		.sbi = &env->sbi,
		.opi = &env->opi,
		.ii = ii,
		.start = 0,
		.len = 1,
		.trunc = false
	};

	err = check_file_io(&file_ctx);
	if (err) {
		return err;
	}
	/*
	 * For now, don't have sync all the way to underlying physical hw; it
	 * is too costly...
	 */
	if (env->tmpfs_mode) {
		err = voluta_pstore_sync(file_ctx.sbi->s_pstore, !datasync);
		if (err) {
			return err;
		}
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
	iattr_init(&attr, ii);
	attr.ia_blocks = i_blocks_of(ii) - 1;
	attr.ia_flags = VOLUTA_ATTR_BLOCKS;
	i_update_attr(ii, file_ctx->opi, &attr);
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

	rtn_child_by_slot(parent_vi->u.rtn, slot, &vaddr);
	if (rtn_isbottom(parent_vi->u.rtn)) {
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
	const size_t nslots_max = rtn_nchilds_max(vi->u.rtn);

	for (size_t slot = 0; slot < nslots_max; ++slot) {
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
	err = stage_tree_root(file_ctx, &vi);
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

	err = drop_by_head_leaf(file_ctx);
	if (err) {
		return err;
	}
	err = drop_by_tree_map(file_ctx);
	if (err) {
		return err;
	}
	reset_tree_mappings(file_ctx->ii);
	return 0;
}

int voluta_drop_reg(struct voluta_sb_info *sbi, struct voluta_inode_info *ii)
{
	struct voluta_file_ctx file_ctx = {
		.sbi = sbi,
		.ii = ii
	};

	return drop_meta_and_data(&file_ctx);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int resolve_truncate_end(struct voluta_file_ctx *file_ctx)
{
	int err;
	loff_t tree_end;
	struct voluta_vnode_info *vi;

	if (file_is_leaf_mode(file_ctx->ii)) {
		file_ctx->end = off_max(file_ctx->off, VOLUTA_DS_SIZE);
		return 0;
	}
	if (!file_is_tree_mode(file_ctx->ii)) {
		file_ctx->end = 0;
		return 0;
	}
	err = stage_tree_root(file_ctx, &vi);
	if (err) {
		return err;
	}
	tree_end = rtn_end(vi->u.rtn);
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
	len = len_within_ds(off, file_ctx->end);
	voluta_assert(len < VOLUTA_DS_SIZE);

	if (off_is_ds_start(off)) {
		zero_headn(vi->u.ds, len);
	} else {
		zero_range(vi->u.ds, off_in_ds(off), len);
	}
	v_dirtify(vi);
	return 0;
}

static void clear_subtree_mappings(struct voluta_vnode_info *vi, size_t slot)
{
	rtn_reset_child_at(vi->u.rtn, slot);
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
	size_t len;

	if (vaddr_isnull(&rli->vaddr)) {
		return 0;
	}
	len = len_within_ds(rli->file_pos, file_ctx->end);
	if (len < VOLUTA_DS_SIZE) {
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
	struct voluta_sb_info *sbi = file_ctx->sbi;

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
		}
		if (err) {
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
		.parent_vi = NULL
	};

	if (!file_is_leaf_mode(file_ctx->ii)) {
		return 0;
	}
	if (!has_head_leaf_io(file_ctx)) {
		return 0;
	}
	rli_init(&rli, file_ctx->ii, file_ctx->off);
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
		.sbi = &env->sbi,
		.opi = &env->opi,
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
	err = resolve_truncate_end(&file_ctx);
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
	const int fl_mode = file_ctx->fl_mode;
	const int fl_supported =
		FALLOC_FL_KEEP_SIZE | FALLOC_FL_PUNCH_HOLE;

	if ((fl_mode & FALLOC_FL_PUNCH_HOLE) &&
	    !(fl_mode & FALLOC_FL_KEEP_SIZE)) {
		return -EINVAL;
	}
	if (fl_mode & ~fl_supported) {
		return -ENOTSUP;
	}
	return 0;
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
	i_update_attr(ii, file_ctx->opi, &attr);
	return 0;
}

static int reserve_bind_leaf(const struct voluta_file_ctx *file_ctx,
			     struct voluta_vnode_info *parent_vi)
{
	int err;
	struct voluta_vaddr vaddr;

	child_of_current_pos(file_ctx, parent_vi, &vaddr);
	if (!vaddr_isnull(&vaddr)) {
		return 0;
	}
	err = reserve_data_leaf(file_ctx, &vaddr);
	if (err) {
		return err;
	}
	rtn_set_child_by_file_pos(parent_vi->u.rtn, file_ctx->off, &vaddr);
	v_dirtify(parent_vi);
	return 0;
}

static int reserve_bind_leaves(struct voluta_file_ctx *file_ctx,
			       struct voluta_vnode_info *parent_vi)
{
	int err;
	bool next_mapping = false;

	while (has_more_io(file_ctx) && !next_mapping) {
		err = reserve_bind_leaf(file_ctx, parent_vi);
		if (err) {
			return err;
		}
		advance_to_next_pos(file_ctx);
		next_mapping = is_mapping_boundaries(file_ctx);
	}
	return 0;
}

static int reserve_leaves(struct voluta_file_ctx *file_ctx)
{
	int err;
	size_t level;
	struct voluta_vnode_info *vi;
	const size_t height = file_tree_height(file_ctx->ii);

	err = stage_tree_root(file_ctx, &vi);
	if (err) {
		return err;
	}
	level = height;
	while (level > 0) {
		if (rtn_isbottom(vi->u.rtn)) {
			return reserve_bind_leaves(file_ctx, vi);
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
	err = reserve_leaves(file_ctx);
	if (err) {
		return err;
	}
	return 0;
}

static int fallocate_reserve_by_tree_map(struct voluta_file_ctx *file_ctx)
{
	int err = 0;

	while (has_more_io(file_ctx) && !err) {
		err = reserve_by_tree_map(file_ctx);
	}
	return err;
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
		.sbi = &env->sbi,
		.opi = &env->opi,
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

	err = voluta_verify_ino(rtn_ino(rtn));
	if (err) {
		return err;
	}
	if ((rtn_beg(rtn) < 0) || (rtn_end(rtn) < 0)) {
		return -EFSCORRUPTED;
	}
	if (rtn_beg(rtn) >= rtn_end(rtn)) {
		return -EFSCORRUPTED;
	}
	height = rtn_height(rtn);
	if ((height <= 1) || (height > 7)) {
		return -EFSCORRUPTED;
	}
	span = rtn_span(rtn);
	spbh = rtn_span_by_height(rtn, height);
	if (span != spbh) {
		return -EFSCORRUPTED;
	}
	return 0;
}
