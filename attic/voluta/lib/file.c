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
#include <linux/fiemap.h>
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
#include "libvoluta.h"

#define STATICASSERT_NELEMS(x, y) \
	VOLUTA_STATICASSERT_EQ(VOLUTA_ARRAY_SIZE(x), y)

#define OP_READ         (1 << 0)
#define OP_WRITE        (1 << 1)
#define OP_TRUNC        (1 << 2)
#define OP_FALLOC       (1 << 3)
#define OP_FSYNC        (1 << 4)
#define OP_FIEMAP       (1 << 5)
#define OP_LSEEK        (1 << 6)


struct voluta_rchild_info {
	struct voluta_vnode_info *parent_vi;
	struct voluta_vaddr vaddr;
	size_t parent_slot;
	loff_t file_pos;
};

struct voluta_file_ctx {
	const struct voluta_oper_ctx *op_ctx;
	struct voluta_sb_info    *sbi;
	struct voluta_inode_info *ii;
	struct voluta_rw_iter    *rwi;
	struct fiemap *fm;
	loff_t  start;
	size_t  len;
	loff_t  beg;
	loff_t  off;
	loff_t  end;
	int     op;
	int     fl_mode;
	int     fm_flags;
	int     fm_stop;
	int     whence;
	int     pad;
};

/* Local functions forward declarations */
static bool fl_keep_size(const struct voluta_file_ctx *f_ctx);

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
	vaddr_by_off(vaddr, VOLUTA_VTYPE_DATA, off);
}

static void vaddr_of_tnode(struct voluta_vaddr *vaddr, loff_t off)
{
	vaddr_by_off(vaddr, VOLUTA_VTYPE_RTNODE, off);
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
	return ARRAY_SIZE(rtn->r_child_lo);
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
	voluta_assert_le(height, VOLUTA_FILE_HEIGHT_MAX);
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

	return slot;
}

static size_t
rtn_height_by_file_pos(const struct voluta_radix_tnode *rtn, loff_t off)
{
	size_t height = 1;
	loff_t xoff = off / VOLUTA_DS_SIZE;

	STATICASSERT_NELEMS(rtn->r_child_hi, VOLUTA_FILE_MAP_NCHILD);
	STATICASSERT_NELEMS(rtn->r_child_lo, VOLUTA_FILE_MAP_NCHILD);

	while (xoff > 0) {
		height += 1;
		xoff /= VOLUTA_FILE_MAP_NCHILD;
	}
	return height;
}

static loff_t rtn_child(const struct voluta_radix_tnode *rtn, size_t slot)
{
	const uint64_t hi = le16_to_cpu(rtn->r_child_hi[slot]);
	const uint64_t lo = le32_to_cpu(rtn->r_child_lo[slot]);

	return (loff_t)((hi << 40) | (lo << 8));
}

static void rtn_set_child(struct voluta_radix_tnode *rtn,
			  size_t slot, loff_t off)
{
	const uint16_t hi = (uint16_t)(off >> 40);
	const uint32_t lo = (uint32_t)(off >> 8);

	voluta_assert_eq(off & 0xFFL, 0);

	rtn->r_child_lo[slot] = cpu_to_le32(lo);
	rtn->r_child_hi[slot] = cpu_to_le16(hi);
}

static void rtn_reset_child(struct voluta_radix_tnode *rtn, size_t slot)
{
	rtn_set_child(rtn, slot, VOLUTA_OFF_NULL);
}

static bool rtn_isinrange(const struct voluta_radix_tnode *rtn,
			  loff_t file_pos)
{
	return off_is_inrange(file_pos, rtn_beg(rtn), rtn_end(rtn));
}

static size_t
rtn_span_by_height(const struct voluta_radix_tnode *rtn, size_t height)
{
	size_t nbytes = VOLUTA_DS_SIZE;
	const size_t nslots = rtn_nchilds_max(rtn);

	voluta_assert_le(height, VOLUTA_FILE_HEIGHT_MAX);
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

static loff_t rtn_file_pos(const struct voluta_radix_tnode *rtn, size_t slot)
{
	loff_t next_off;
	const size_t nbps = rtn_nbytes_per_slot(rtn);

	next_off = off_end(rtn_beg(rtn), slot * nbps);
	return off_to_ds(next_off);
}

static loff_t
rtn_next_file_pos(const struct voluta_radix_tnode *rtn, size_t slot)
{
	loff_t file_pos;
	const size_t nbps = rtn_nbytes_per_slot(rtn);

	file_pos = rtn_file_pos(rtn, slot);
	return off_end(file_pos, nbps);
}

static void rtn_clear_childs(struct voluta_radix_tnode *rtn)
{
	const size_t nslots_max = rtn_nchilds_max(rtn);

	for (size_t slot = 0; slot < nslots_max; ++slot) {
		rtn_reset_child(rtn, slot);
	}
}

static void rtn_init(struct voluta_radix_tnode *rtn,
		     ino_t ino, loff_t off, size_t height)
{
	loff_t beg = 0;
	loff_t end = 0;

	rtn_calc_range(rtn, off, height, &beg, &end);
	rtn_set_ino(rtn, ino);
	rtn_set_beg(rtn, beg);
	rtn_set_end(rtn, end);
	rtn_set_height(rtn, height);
	rtn_clear_childs(rtn);
	voluta_memzero(rtn->r_zeros, sizeof(rtn->r_zeros));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void rci_init(struct voluta_rchild_info *rci, loff_t file_pos)
{
	vaddr_of_leaf(&rci->vaddr, VOLUTA_OFF_NULL);
	rci->parent_vi = NULL;
	rci->parent_slot = UINT_MAX;
	rci->file_pos = file_pos;
}

static void rci_set(struct voluta_rchild_info *rci,
		    const struct voluta_vaddr *vaddr,
		    struct voluta_vnode_info *parent_vi,
		    size_t slot, loff_t file_pos)
{
	loff_t leaf_off = 0;

	if (parent_vi != NULL) {
		leaf_off = rtn_file_pos(parent_vi->u.rtn, slot);
	}
	vaddr_clone(vaddr, &rci->vaddr);
	rci->parent_vi = parent_vi;
	rci->parent_slot = slot;
	rci->file_pos = off_max(leaf_off, file_pos);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool isinrange(const struct voluta_vnode_info *vi, loff_t file_pos)
{
	return rtn_isinrange(vi->u.rtn, file_pos);
}

static bool isbottom(const struct voluta_vnode_info *vi)
{
	return rtn_isbottom(vi->u.rtn);
}

static size_t height_of(const struct voluta_vnode_info *vi)
{
	return rtn_height(vi->u.rtn);
}

static size_t nchilds_max(const struct voluta_vnode_info *vi)
{
	return rtn_nchilds_max(vi->u.rtn);
}

static size_t child_slot_of(const struct voluta_vnode_info *vi, loff_t off)
{
	return rtn_slot_by_file_pos(vi->u.rtn, off);
}

static void resolve_child_by_slot(const struct voluta_vnode_info *vi,
				  size_t slot, struct voluta_vaddr *vaddr)
{
	loff_t child_off;

	child_off = rtn_child(vi->u.rtn, slot);
	if (isbottom(vi)) {
		vaddr_of_leaf(vaddr, child_off);
	} else {
		vaddr_of_tnode(vaddr, child_off);
	}
}

static void assign_child_by_pos(struct voluta_vnode_info *vi, loff_t file_pos,
				const struct voluta_vaddr *vaddr)
{
	size_t child_slot;

	child_slot = child_slot_of(vi, file_pos);
	rtn_set_child(vi->u.rtn, child_slot, vaddr->off);
}

static void resolve_child_at(struct voluta_vnode_info *vi,
			     loff_t file_pos, size_t slot,
			     struct voluta_rchild_info *rci)
{
	struct voluta_vaddr vaddr;

	resolve_child_by_slot(vi, slot, &vaddr);
	rci_set(rci, &vaddr, vi, slot, file_pos);
}

static void resolve_child(struct voluta_vnode_info *vi, loff_t file_pos,
			  struct voluta_rchild_info *rci)
{
	size_t child_slot;

	if (vi != NULL) {
		child_slot = child_slot_of(vi, file_pos);
		resolve_child_at(vi, file_pos, child_slot, rci);
	} else {
		rci_init(rci, file_pos);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_reg_ispec *
reg_ispec_of(const struct voluta_inode_info *ii)
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

static int require_reg(const struct voluta_file_ctx *f_ctx)
{
	if (i_isdir(f_ctx->ii)) {
		return -EISDIR;
	}
	if (!i_isreg(f_ctx->ii)) {
		return -EINVAL;
	}
	return 0;
}

static int check_file_io(const struct voluta_file_ctx *f_ctx)
{
	int err;
	size_t nds;
	const loff_t filesize_max = VOLUTA_FILE_SIZE_MAX;
	const size_t iosize_max = VOLUTA_IO_SIZE_MAX;
	const size_t nds_max = VOLUTA_IO_NDS_MAX;

	err = require_reg(f_ctx);
	if (err) {
		return err;
	}
	if ((f_ctx->op & ~OP_TRUNC) && !f_ctx->ii->i_nopen) {
		return -EBADF;
	}
	if (f_ctx->start < 0) {
		return -EINVAL;
	}
	if (f_ctx->beg > filesize_max) {
		return -EFBIG;
	}
	if (f_ctx->op & (OP_WRITE | OP_FALLOC)) {
		if (f_ctx->end > filesize_max) {
			return -EFBIG;
		}
	}
	if (f_ctx->op & (OP_READ | OP_WRITE)) {
		if (f_ctx->len > iosize_max) {
			return -EINVAL;
		}
		nds = off_len_nds(f_ctx->beg, f_ctx->len);
		if (nds_max < nds) {
			return -EINVAL;
		}
		if (!f_ctx->rwi) {
			return -EINVAL;
		}
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int iter_recursive(struct voluta_file_ctx *f_ctx,
			  struct voluta_vnode_info *parent_vi,
			  struct voluta_rchild_info *rci);

static bool has_more_io(const struct voluta_file_ctx *f_ctx)
{
	return (f_ctx->off < f_ctx->end) && !f_ctx->fm_stop;
}

static bool has_more_head_io(const struct voluta_file_ctx *f_ctx)
{
	return has_more_io(f_ctx) && (f_ctx->off < VOLUTA_DS_SIZE);
}

static size_t io_length(const struct voluta_file_ctx *f_ctx)
{
	voluta_assert(f_ctx->beg <= f_ctx->off);

	return off_len(f_ctx->beg, f_ctx->off);
}

static bool is_mapping_boundaries(const struct voluta_file_ctx *f_ctx)
{
	const loff_t mapping_size = (VOLUTA_DS_SIZE * VOLUTA_FILE_MAP_NCHILD);

	return ((f_ctx->off % mapping_size) == 0);
}

static void post_io_update(const struct voluta_file_ctx *f_ctx, bool killprv)
{
	struct voluta_iattr iattr;
	struct voluta_inode_info *ii = f_ctx->ii;
	const loff_t isz = i_size_of(ii);
	const loff_t off = f_ctx->off;
	const size_t len = io_length(f_ctx);

	iattr_setup(&iattr, i_ino_of(ii));
	if (f_ctx->op & OP_FALLOC) {
		iattr.ia_flags |= VOLUTA_IATTR_MCTIME;
		if (!fl_keep_size(f_ctx)) {
			iattr.ia_flags |= VOLUTA_IATTR_SIZE;
			iattr.ia_size = off_max(off, isz);
		}
	} else if (f_ctx->op & OP_WRITE) {
		iattr.ia_flags |= VOLUTA_IATTR_MCTIME | VOLUTA_IATTR_SIZE;
		iattr.ia_size = off_max(off, isz);
		if (killprv && (len > 0)) {
			iattr.ia_flags |= VOLUTA_IATTR_KILL_PRIV;
		}
	} else if (f_ctx->op & OP_READ) {
		iattr.ia_flags |= VOLUTA_IATTR_ATIME | VOLUTA_IATTR_LAZY;
	} else if (f_ctx->op & OP_TRUNC) {
		iattr.ia_flags |= VOLUTA_IATTR_MCTIME |
				  VOLUTA_IATTR_SIZE | VOLUTA_IATTR_KILL_PRIV;
		iattr.ia_size = f_ctx->beg;
	}
	update_iattrs(f_ctx->op_ctx, ii, &iattr);
}

static int stage_data_leaf(const struct voluta_file_ctx *f_ctx,
			   const struct voluta_vaddr *vaddr,
			   struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi;

	*out_vi = NULL;
	if (vaddr_isnull(vaddr)) {
		return -ENOENT;
	}
	err = voluta_stage_data(f_ctx->sbi, vaddr, f_ctx->ii, &vi);
	if (err) {
		return err;
	}
	*out_vi = vi;
	return 0;
}

static int stage_data_leaf_of(const struct voluta_file_ctx *f_ctx,
			      const struct voluta_rchild_info *rci,
			      struct voluta_vnode_info **out_vi)
{
	return stage_data_leaf(f_ctx, &rci->vaddr, out_vi);
}

static int stage_radix_tnode(const struct voluta_file_ctx *f_ctx,
			     const struct voluta_vaddr *vaddr,
			     struct voluta_vnode_info **out_vi)
{
	int err = -ENOENT;

	if (!vaddr_isnull(vaddr)) {
		err = voluta_stage_vnode(f_ctx->sbi, vaddr, f_ctx->ii, out_vi);
	}
	return err;
}

static int stage_tree_root(const struct voluta_file_ctx *f_ctx,
			   struct voluta_vnode_info **out_vi)
{
	struct voluta_vaddr root_vaddr;

	file_tree_root(f_ctx->ii, &root_vaddr);
	return stage_radix_tnode(f_ctx, &root_vaddr, out_vi);
}

static int stage_head_leaf(const struct voluta_file_ctx *f_ctx,
			   struct voluta_vnode_info **out_vi)
{
	struct voluta_rchild_info rci;

	rci_init(&rci, f_ctx->off);
	file_tree_root(f_ctx->ii, &rci.vaddr);
	return stage_data_leaf_of(f_ctx, &rci, out_vi);
}

static void advance_to(struct voluta_file_ctx *f_ctx, loff_t off)
{
	f_ctx->off = off_max(f_ctx->off, off);
}

static void advance_to_slot(struct voluta_file_ctx *f_ctx,
			    const struct voluta_vnode_info *vi, size_t slot)
{
	advance_to(f_ctx, rtn_file_pos(vi->u.rtn, slot));
}

static void advance_to_next(struct voluta_file_ctx *f_ctx,
			    const struct voluta_vnode_info *vi, size_t slot)
{
	advance_to(f_ctx, rtn_next_file_pos(vi->u.rtn, slot));
}

static size_t iter_start_slot(const struct voluta_file_ctx *f_ctx,
			      const struct voluta_vnode_info *parent_vi)
{
	return child_slot_of(parent_vi, f_ctx->off);
}

static int iter_to_data(struct voluta_file_ctx *f_ctx,
			struct voluta_vnode_info *parent_vi,
			struct voluta_rchild_info *rci)
{
	size_t start_slot;
	const size_t nslots_max = nchilds_max(parent_vi);

	start_slot = iter_start_slot(f_ctx, parent_vi);
	for (size_t slot = start_slot; slot < nslots_max; ++slot) {
		advance_to_slot(f_ctx, parent_vi, slot);

		resolve_child_at(parent_vi, f_ctx->off, slot, rci);
		if (!vaddr_isnull(&rci->vaddr)) {
			return 0;
		}
	}
	return -ENOENT;
}

static int iter_recursive_at(struct voluta_file_ctx *f_ctx,
			     struct voluta_vnode_info *parent_vi,
			     size_t slot, struct voluta_rchild_info *rci)
{
	int err;
	struct voluta_vaddr vaddr;
	struct voluta_vnode_info *vi;

	resolve_child_by_slot(parent_vi, slot, &vaddr);
	if (vaddr_isnull(&vaddr)) {
		return -ENOENT;
	}
	err = stage_radix_tnode(f_ctx, &vaddr, &vi);
	if (err) {
		return err;
	}
	err = iter_recursive(f_ctx, vi, rci);
	if (err) {
		return err;
	}
	return 0;
}

static int do_iter_recursive(struct voluta_file_ctx *f_ctx,
			     struct voluta_vnode_info *parent_vi,
			     struct voluta_rchild_info *rci)
{
	int err = -ENOENT;
	size_t start_slot;
	const size_t nslots_max = nchilds_max(parent_vi);

	if (!isinrange(parent_vi, f_ctx->off)) {
		return -ENOENT;
	}
	if (isbottom(parent_vi)) {
		return iter_to_data(f_ctx, parent_vi, rci);
	}
	start_slot = child_slot_of(parent_vi, f_ctx->off);
	for (size_t slot = start_slot; slot < nslots_max; ++slot) {
		err = iter_recursive_at(f_ctx, parent_vi, slot, rci);
		if (err != -ENOENT) {
			break;
		}
		advance_to_next(f_ctx, parent_vi, slot);
	}
	return err;
}

static int iter_recursive(struct voluta_file_ctx *f_ctx,
			  struct voluta_vnode_info *parent_vi,
			  struct voluta_rchild_info *rci)
{
	int err;

	v_incref(parent_vi);
	err = do_iter_recursive(f_ctx, parent_vi, rci);
	v_decref(parent_vi);

	return err;
}

static int seek_data_by_tree_map(struct voluta_file_ctx *f_ctx,
				 struct voluta_rchild_info *rci)
{
	int err;
	struct voluta_vnode_info *root_vi;

	if (!file_is_tree_mode(f_ctx->ii)) {
		return -ENOENT;
	}
	err = stage_tree_root(f_ctx, &root_vi);
	if (err) {
		return err;
	}
	rci_init(rci, f_ctx->off);
	err = iter_recursive(f_ctx, root_vi, rci);
	if (err) {
		return err;
	}
	return 0;
}

static int seek_data_by_head_leaf(struct voluta_file_ctx *f_ctx,
				  struct voluta_rchild_info *rci)
{
	int err;
	struct voluta_vnode_info *vi = NULL;

	if (!file_is_leaf_mode(f_ctx->ii)) {
		return -ENOENT;
	}
	if (!has_more_head_io(f_ctx)) {
		return -ENOENT;
	}
	err = stage_head_leaf(f_ctx, &vi);
	if (err) {
		return err;
	}
	rci_set(rci, v_vaddr_of(vi), NULL, 0, f_ctx->off);
	return 0;
}

static void advance_by_nbytes(struct voluta_file_ctx *f_ctx, size_t len)
{
	advance_to(f_ctx, off_end(f_ctx->off, len));
}

static void advance_to_next_bk(struct voluta_file_ctx *f_ctx)
{
	advance_by_nbytes(f_ctx, len_to_next_ds(f_ctx->off));
}

static void advance_to_next_pos(struct voluta_file_ctx *f_ctx)
{
	const size_t len = len_within_ds(f_ctx->off, f_ctx->end);

	advance_by_nbytes(f_ctx, len);
}

static int memref_of(const struct voluta_file_ctx *f_ctx,
		     struct voluta_vnode_info *vi, void *ptr, size_t len,
		     struct voluta_memref *out_qmr)
{
	const struct voluta_qalloc *qal = f_ctx->sbi->s_qalloc;

	return voluta_memref(qal, ptr, len, vi, out_qmr);
}

static void *membuf_of(struct voluta_file_ctx *f_ctx,
		       const struct voluta_vnode_info *vi)
{
	const struct voluta_cache *cache = f_ctx->sbi->s_cache;

	return likely(vi != NULL) ? vi->u.ds->dat : cache->c_null_bk->bk;
}

static int call_rw_actor(struct voluta_file_ctx *f_ctx,
			 struct voluta_vnode_info *vi, size_t *out_len)
{
	int err;
	loff_t cnt;
	size_t len;
	uint8_t *buf;
	struct voluta_memref qmr = {
		.fd = -1,
	};

	cnt = off_in_ds(f_ctx->off);
	len = len_within_ds(f_ctx->off, f_ctx->end);
	buf = membuf_of(f_ctx, vi);
	err = memref_of(f_ctx, vi, buf + cnt, len, &qmr);
	if (!err) {
		err = f_ctx->rwi->actor(f_ctx->rwi, &qmr);
	}
	*out_len = len;
	return err;
}

static int export_to_user(struct voluta_file_ctx *f_ctx,
			  struct voluta_vnode_info *vi, size_t *out_sz)
{
	return call_rw_actor(f_ctx, vi, out_sz);
}

static int import_from_user(struct voluta_file_ctx *f_ctx,
			    struct voluta_vnode_info *vi, size_t *out_sz)
{
	return call_rw_actor(f_ctx, vi, out_sz);
}

static void child_of_current_pos(const struct voluta_file_ctx *f_ctx,
				 struct voluta_vnode_info *parent_vi,
				 struct voluta_rchild_info *rci)
{
	resolve_child(parent_vi, f_ctx->off, rci);
}

static int fetch_by_tree_map(const struct voluta_file_ctx *f_ctx,
			     struct voluta_vnode_info **out_vi)
{
	int err;
	size_t height;
	struct voluta_vnode_info *vi;
	struct voluta_rchild_info rci;

	if (!file_is_tree_mode(f_ctx->ii)) {
		return -ENOENT;
	}
	err = stage_tree_root(f_ctx, &vi);
	if (err) {
		return err;
	}
	if (!isinrange(vi, f_ctx->off)) {
		return -ENOENT;
	}
	height = file_tree_height(f_ctx->ii);
	while (height--) {
		if (isbottom(vi)) {
			*out_vi = vi;
			return 0;
		}
		child_of_current_pos(f_ctx, vi, &rci);
		err = stage_radix_tnode(f_ctx, &rci.vaddr, &vi);
		if (err) {
			return err;
		}
	}
	return -EFSCORRUPTED;
}

static int do_read_from_leaf(struct voluta_file_ctx *f_ctx,
			     struct voluta_vnode_info *leaf_vi, size_t *out_sz)
{
	return export_to_user(f_ctx, leaf_vi, out_sz);
}

static int read_from_leaf(struct voluta_file_ctx *f_ctx,
			  struct voluta_vnode_info *leaf_vi, size_t *out_sz)
{
	int err;

	v_incref(leaf_vi);
	err = do_read_from_leaf(f_ctx, leaf_vi, out_sz);
	v_decref(leaf_vi);

	return err;
}

static int do_read_from_leaves(struct voluta_file_ctx *f_ctx,
			       struct voluta_vnode_info *parent_vi)
{
	int err;
	size_t len;
	struct voluta_vnode_info *vi;
	struct voluta_rchild_info rci;

	while (has_more_io(f_ctx)) {
		child_of_current_pos(f_ctx, parent_vi, &rci);
		err = stage_data_leaf(f_ctx, &rci.vaddr, &vi);
		if (err && (err != -ENOENT)) {
			return err;
		}
		err = read_from_leaf(f_ctx, vi, &len);
		if (err) {
			return err;
		}
		advance_by_nbytes(f_ctx, len);
		if (is_mapping_boundaries(f_ctx)) {
			break;
		}
	}
	return 0;
}

static int read_from_leaves(struct voluta_file_ctx *f_ctx,
			    struct voluta_vnode_info *parent_vi)
{
	int err;

	v_incref(parent_vi);
	err = do_read_from_leaves(f_ctx, parent_vi);
	v_decref(parent_vi);

	return err;
}

static int read_by_tree_map(struct voluta_file_ctx *f_ctx)
{
	int err;
	struct voluta_vnode_info *parent_vi;

	while (has_more_io(f_ctx)) {
		parent_vi = NULL;
		err = fetch_by_tree_map(f_ctx, &parent_vi);
		if (err && (err != -ENOENT)) {
			return err;
		}
		err = read_from_leaves(f_ctx, parent_vi);
		if (err) {
			return err;
		}
	}
	return 0;
}

/*
 * TODO-0009: Data on inode for tiny files
 *
 * For very small files (< 400-bytes) place data within inode itself (a.k.a,
 * inline-data).
 */
static int read_by_head_leaf(struct voluta_file_ctx *f_ctx)
{
	int err;
	size_t len;
	struct voluta_vnode_info *vi = NULL;

	if (!file_is_leaf_mode(f_ctx->ii)) {
		return 0;
	}
	if (!has_more_head_io(f_ctx)) {
		return 0;
	}
	err = stage_head_leaf(f_ctx, &vi);
	if (err && (err != -ENOENT)) {
		return err;
	}
	err = export_to_user(f_ctx, vi, &len);
	if (err) {
		return err;
	}
	advance_by_nbytes(f_ctx, len);
	return 0;
}

static int read_data(struct voluta_file_ctx *f_ctx)
{
	int err;

	err = read_by_head_leaf(f_ctx);
	if (err) {
		return err;
	}
	err = read_by_tree_map(f_ctx);
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
			   const struct voluta_memref *qmr)
{
	struct voluta_read_iter *ri = read_iter_of(rwi);

	if ((qmr->fd > 0) && (qmr->off < 0)) {
		return -EINVAL;
	}
	if ((ri->dat_len + qmr->len) > ri->dat_max) {
		return -EINVAL;
	}
	memcpy(ri->dat + ri->dat_len, qmr->mem, qmr->len);
	ri->dat_len += qmr->len;
	return 0;
}

static loff_t rw_iter_end(const struct voluta_rw_iter *rwi)
{
	return off_end(rwi->off, rwi->len);
}

static void update_with_rw_iter(struct voluta_file_ctx *f_ctx,
				struct voluta_rw_iter *rwi)
{
	const loff_t end = rw_iter_end(rwi);
	const loff_t isz = i_size_of(f_ctx->ii);

	f_ctx->rwi = rwi;
	f_ctx->start = rwi->off;
	f_ctx->len = rwi->len;
	f_ctx->beg = rwi->off;
	f_ctx->off = rwi->off;
	if (f_ctx->op & OP_READ) {
		f_ctx->end = off_min(end, isz);
	} else {
		f_ctx->end = end;
	}
}

static int read_iter(struct voluta_file_ctx *f_ctx)
{
	int err;

	err = check_file_io(f_ctx);
	if (err) {
		return err;
	}
	err = read_data(f_ctx);
	post_io_update(f_ctx, false);
	return err;
}

int voluta_do_read_iter(const struct voluta_oper_ctx *op_ctx,
			struct voluta_inode_info *ii,
			struct voluta_rw_iter *rwi)
{
	int err;
	struct voluta_file_ctx f_ctx = {
		.op_ctx = op_ctx,
		.sbi = i_sbi_of(ii),
		.ii = ii,
		.op = OP_READ
	};

	update_with_rw_iter(&f_ctx, rwi);

	i_incref(ii);
	err = read_iter(&f_ctx);
	i_decref(ii);

	return err;
}

int voluta_do_read(const struct voluta_oper_ctx *op_ctx,
		   struct voluta_inode_info *ii,
		   void *buf, size_t len, loff_t off, size_t *out_len)
{
	int err;
	struct voluta_read_iter ri = {
		.dat_len = 0,
		.rwi.actor = read_iter_actor,
		.rwi.len = len,
		.rwi.off = off,
		.dat = buf,
		.dat_max = len
	};

	err = voluta_do_read_iter(op_ctx, ii, &ri.rwi);
	*out_len = ri.dat_len;
	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int alloc_data_leaf_space(const struct voluta_file_ctx *f_ctx,
				 struct voluta_vaddr *out_va)
{
	return voluta_alloc_vspace(f_ctx->sbi, VOLUTA_VTYPE_DATA, out_va);
}

static int new_data_leaf(const struct voluta_file_ctx *f_ctx,
			 struct voluta_vnode_info **out_vi)
{
	return voluta_new_vnode(f_ctx->sbi, f_ctx->ii,
				VOLUTA_VTYPE_DATA, out_vi);
}

static int new_radix_tnode(const struct voluta_file_ctx *f_ctx,
			   struct voluta_vnode_info **out_vi)
{
	return voluta_new_vnode(f_ctx->sbi, f_ctx->ii,
				VOLUTA_VTYPE_RTNODE, out_vi);
}

static int del_file_tnode(const struct voluta_file_ctx *f_ctx,
			  struct voluta_vnode_info *vi)
{
	return voluta_del_vnode(f_ctx->sbi, vi);
}

static int del_file_tnode_at(const struct voluta_file_ctx *f_ctx,
			     const struct voluta_vaddr *vaddr)
{
	return voluta_del_vnode_at(f_ctx->sbi, vaddr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int create_radix_tnode(const struct voluta_file_ctx *f_ctx,
			      loff_t off, size_t height,
			      struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi;

	err = new_radix_tnode(f_ctx, &vi);
	if (err) {
		return err;
	}
	rtn_init(vi->u.rtn, i_ino_of(f_ctx->ii), off, height);
	v_dirtify(vi);
	*out_vi = vi;
	return 0;
}

static int create_root_node(const struct voluta_file_ctx *f_ctx,
			    size_t height, struct voluta_vnode_info **out_vi)
{
	voluta_assert_gt(height, 0);

	return create_radix_tnode(f_ctx, 0, height, out_vi);
}

static int create_bind_node(const struct voluta_file_ctx *f_ctx,
			    struct voluta_vnode_info *parent_vi,
			    struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi;
	const loff_t file_pos = f_ctx->off;
	const size_t height = height_of(parent_vi);

	err = create_radix_tnode(f_ctx, file_pos, height - 1, &vi);
	if (err) {
		return err;
	}
	assign_child_by_pos(parent_vi, file_pos, v_vaddr_of(vi));
	v_dirtify(parent_vi);
	*out_vi = vi;
	return 0;
}

static void zero_data_seg(const struct voluta_vnode_info *vi)
{
	struct voluta_data_seg *ds = vi->u.ds;

	voluta_memzero(ds, sizeof(*ds));
}

static int create_data_leaf(const struct voluta_file_ctx *f_ctx,
			    struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_iattr iattr;
	struct voluta_vnode_info *vi = NULL;
	struct voluta_inode_info *ii = f_ctx->ii;

	err = new_data_leaf(f_ctx, &vi);
	if (err) {
		return err;
	}
	zero_data_seg(vi);
	v_dirtify(vi);

	iattr_setup(&iattr, i_ino_of(ii));
	iattr.ia_blocks = i_blocks_of(ii) + 1;
	iattr.ia_flags = VOLUTA_IATTR_BLOCKS;
	update_iattrs(f_ctx->op_ctx, ii, &iattr);
	*out_vi = vi;
	return 0;
}

static int create_bind_leaf(const struct voluta_file_ctx *f_ctx,
			    struct voluta_vnode_info *parent_vi,
			    struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi = NULL;

	err = create_data_leaf(f_ctx, &vi);
	if (err) {
		return err;
	}
	assign_child_by_pos(parent_vi, f_ctx->off, &vi->vaddr);
	v_dirtify(parent_vi);
	*out_vi = vi;
	return 0;
}

static void set_new_root(const struct voluta_file_ctx *f_ctx,
			 const struct voluta_vaddr *vaddr, size_t height)
{
	struct voluta_inode_info *ii = f_ctx->ii;

	file_tree_update(ii, vaddr, height);
	i_dirtify(ii);
}

static void bind_sub_tree(struct voluta_file_ctx *f_ctx,
			  struct voluta_vnode_info *vi)
{
	struct voluta_vaddr root_vaddr;

	file_tree_root(f_ctx->ii, &root_vaddr);
	rtn_set_child(vi->u.rtn, 0, root_vaddr.off);
	v_dirtify(vi);

	set_new_root(f_ctx, v_vaddr_of(vi), height_of(vi));
}

static size_t off_to_height(loff_t off)
{
	return rtn_height_by_file_pos(NULL, off);
}

static int create_tree_levels(struct voluta_file_ctx *f_ctx)
{
	int err;
	size_t cur_height;
	struct voluta_vnode_info *vi;
	const size_t new_height = off_to_height(f_ctx->off);

	cur_height = max(1, file_tree_height(f_ctx->ii));
	while (new_height > cur_height) {
		err = create_root_node(f_ctx, ++cur_height, &vi);
		if (err) {
			return err;
		}
		bind_sub_tree(f_ctx, vi);
	}
	return 0;
}

static int stage_or_create_leaf(const struct voluta_file_ctx *f_ctx,
				struct voluta_vnode_info *parent_vi,
				struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_rchild_info rci;

	child_of_current_pos(f_ctx, parent_vi, &rci);
	if (!vaddr_isnull(&rci.vaddr)) {
		err = stage_data_leaf(f_ctx, &rci.vaddr, out_vi);
	} else {
		err = create_bind_leaf(f_ctx, parent_vi, out_vi);
	}
	return err;
}

static int do_stage_or_create_node(const struct voluta_file_ctx *f_ctx,
				   struct voluta_vnode_info *parent_vi,
				   struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_rchild_info rci;

	child_of_current_pos(f_ctx, parent_vi, &rci);
	if (!vaddr_isnull(&rci.vaddr)) {
		err = stage_radix_tnode(f_ctx, &rci.vaddr, out_vi);
	} else {
		err = create_bind_node(f_ctx, parent_vi, out_vi);
	}
	return err;
}

static int stage_or_create_node(const struct voluta_file_ctx *f_ctx,
				struct voluta_vnode_info *parent_vi,
				struct voluta_vnode_info **out_vi)
{
	int err;

	v_incref(parent_vi);
	err = do_stage_or_create_node(f_ctx, parent_vi, out_vi);
	v_decref(parent_vi);

	return err;
}

static int stage_or_create_tree_path(struct voluta_file_ctx *f_ctx,
				     struct voluta_vnode_info **out_vi)
{
	int err;
	size_t level;
	struct voluta_vnode_info *vi;

	level = file_tree_height(f_ctx->ii);
	err = stage_tree_root(f_ctx, &vi);
	while (!err && (level > 0)) {
		if (isbottom(vi)) {
			*out_vi = vi;
			return 0;
		}
		err = stage_or_create_node(f_ctx, vi, &vi);
		level -= 1;
	}
	return err ? err : -EFSCORRUPTED;
}

static int stage_or_create_tree_map(struct voluta_file_ctx *f_ctx,
				    struct voluta_vnode_info **out_vi)
{
	int err;

	err = create_tree_levels(f_ctx);
	if (err) {
		return err;
	}
	err = stage_or_create_tree_path(f_ctx, out_vi);
	if (err) {
		return err;
	}
	return 0;
}

static void clear_unwritten_leaf(struct voluta_vnode_info *leaf_vi)
{
	voluta_clear_unwritten(leaf_vi);
}

static int do_write_to_leaf(struct voluta_file_ctx *f_ctx,
			    struct voluta_vnode_info *leaf_vi, size_t *out_sz)
{
	int err;
	struct voluta_vnode_info *agm_vi = leaf_vi->v_pvi;

	err = import_from_user(f_ctx, leaf_vi, out_sz);
	if (err) {
		return err;
	}
	clear_unwritten_leaf(leaf_vi);
	v_dirtify(leaf_vi);

	/* for data checksum */
	voluta_assert_eq(agm_vi->vaddr.vtype, VOLUTA_VTYPE_AGMAP);
	v_dirtify(agm_vi);

	return 0;
}

static int write_to_leaf(struct voluta_file_ctx *f_ctx,
			 struct voluta_vnode_info *leaf_vi, size_t *out_sz)
{
	int err;

	v_incref(leaf_vi);
	err = do_write_to_leaf(f_ctx, leaf_vi, out_sz);
	v_decref(leaf_vi);

	return err;
}

static int do_write_to_leaves(struct voluta_file_ctx *f_ctx,
			      struct voluta_vnode_info *parent_vi)
{
	int err;
	size_t len;
	bool next_mapping = false;
	struct voluta_vnode_info *vi;

	while (has_more_io(f_ctx) && !next_mapping) {
		err = stage_or_create_leaf(f_ctx, parent_vi, &vi);
		if (err) {
			return err;
		}
		err = write_to_leaf(f_ctx, vi, &len);
		if (err) {
			return err;
		}
		advance_by_nbytes(f_ctx, len);
		next_mapping = is_mapping_boundaries(f_ctx);
	}
	return 0;
}

static int write_to_leaves(struct voluta_file_ctx *f_ctx,
			   struct voluta_vnode_info *vi)
{
	int err;

	v_incref(vi);
	err = do_write_to_leaves(f_ctx, vi);
	v_decref(vi);

	return err;
}

static int write_by_tree_map(struct voluta_file_ctx *f_ctx)
{
	int err;
	struct voluta_vnode_info *vi;

	while (has_more_io(f_ctx)) {
		err = stage_or_create_tree_map(f_ctx, &vi);
		if (err) {
			return err;
		}
		err = write_to_leaves(f_ctx, vi);
		if (err) {
			return err;
		}
	}
	return 0;
}

static int stage_or_create_head_leaf(struct voluta_file_ctx *f_ctx,
				     struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi;

	if (file_is_leaf_mode(f_ctx->ii)) {
		return stage_head_leaf(f_ctx, out_vi);
	}
	err = create_data_leaf(f_ctx, &vi);
	if (err) {
		return err;
	}
	set_new_root(f_ctx, v_vaddr_of(vi), 1);
	*out_vi = vi;
	return 0;
}

static int write_by_head_leaf(struct voluta_file_ctx *f_ctx)
{
	int err;
	size_t len;
	struct voluta_vnode_info *hleaf_vi;

	if (file_is_tree_mode(f_ctx->ii)) {
		return 0;
	}
	if (!has_more_head_io(f_ctx)) {
		return 0;
	}
	err = stage_or_create_head_leaf(f_ctx, &hleaf_vi);
	if (err) {
		return err;
	}
	err = write_to_leaf(f_ctx, hleaf_vi, &len);
	if (err) {
		return err;
	}
	advance_by_nbytes(f_ctx, len);

	return 0;
}

static int write_data(struct voluta_file_ctx *f_ctx)
{
	int err;

	err = write_by_head_leaf(f_ctx);
	if (err) {
		return err;
	}
	err = write_by_tree_map(f_ctx);
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


static struct voluta_write_iter *
write_iter_of(const struct voluta_rw_iter *rwi)
{
	return container_of(rwi, struct voluta_write_iter, rwi);
}

static int write_iter_actor(struct voluta_rw_iter *rwi,
			    const struct voluta_memref *qmr)
{
	struct voluta_write_iter *wi = write_iter_of(rwi);

	if ((qmr->fd > 0) && (qmr->off < 0)) {
		return -EINVAL;
	}
	if ((wi->dat_len + qmr->len) > wi->dat_max) {
		return -EINVAL;
	}
	memcpy(qmr->mem, wi->dat + wi->dat_len, qmr->len);
	wi->dat_len += qmr->len;
	return 0;
}

static int write_iter(struct voluta_file_ctx *f_ctx)
{
	int err;

	err = check_file_io(f_ctx);
	if (err) {
		return err;
	}
	err = write_data(f_ctx);
	post_io_update(f_ctx, err == 0);

	return err;
}

int voluta_do_write_iter(const struct voluta_oper_ctx *op_ctx,
			 struct voluta_inode_info *ii,
			 struct voluta_rw_iter *rwi)
{
	int err;
	struct voluta_file_ctx f_ctx = {
		.sbi = i_sbi_of(ii),
		.op_ctx = op_ctx,
		.ii = ii,
		.op = OP_WRITE
	};

	update_with_rw_iter(&f_ctx, rwi);

	i_incref(ii);
	err = write_iter(&f_ctx);
	i_decref(ii);

	return err;
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

int voluta_do_write(const struct voluta_oper_ctx *op_ctx,
		    struct voluta_inode_info *ii,
		    const void *buf, size_t len,
		    loff_t off, size_t *out_len)
{
	int err;
	struct voluta_write_iter wi = {
		.dat_len = 0
	};

	write_iter_setup(&wi, buf, len, off);
	err = voluta_do_write_iter(op_ctx, ii, &wi.rwi);
	*out_len = wi.dat_len;
	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_do_flush(const struct voluta_oper_ctx *op_ctx,
		    struct voluta_inode_info *ii)
{
	/* TODO: Report if errors with this ii */
	voluta_unused(op_ctx);
	voluta_unused(ii);
	return 0;
}

static int do_fsync(struct voluta_file_ctx *f_ctx, bool datasync)
{
	int err;
	const int flags = datasync ? VOLUTA_F_SYNC : 0;

	err = check_file_io(f_ctx);
	if (err) {
		return err;
	}
	err = voluta_commit_dirty_of(f_ctx->ii, flags);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_do_fsync(const struct voluta_oper_ctx *op_ctx,
		    struct voluta_inode_info *ii, bool datasync)
{
	int err;
	struct voluta_file_ctx f_ctx = {
		.op_ctx = op_ctx,
		.sbi = i_sbi_of(ii),
		.ii = ii,
		.start = 0,
		.len = 1,
		.op = OP_FSYNC
	};

	i_incref(ii);
	err = do_fsync(&f_ctx, datasync);
	i_decref(ii);

	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int drop_del_subtree(struct voluta_file_ctx *f_ctx,
			    struct voluta_vnode_info *vi);

static int discard_data_leaf(const struct voluta_file_ctx *f_ctx,
			     const struct voluta_vaddr *vaddr)
{
	int err;
	struct voluta_iattr iattr;
	struct voluta_inode_info *ii = f_ctx->ii;

	voluta_assert_eq(vaddr->vtype, VOLUTA_VTYPE_DATA);

	if (vaddr_isnull(vaddr)) {
		return 0;
	}
	err = del_file_tnode_at(f_ctx, vaddr);
	if (err) {
		return err;
	}
	iattr_setup(&iattr, i_ino_of(ii));
	iattr.ia_blocks = i_blocks_of(ii) - 1;
	iattr.ia_flags = VOLUTA_IATTR_BLOCKS;
	update_iattrs(f_ctx->op_ctx, ii, &iattr);
	return 0;
}

static int drop_subtree(struct voluta_file_ctx *f_ctx,
			const struct voluta_vaddr *vaddr)
{
	int err;
	struct voluta_vnode_info *vi;

	if (vaddr_isnull(vaddr)) {
		return 0;
	}
	err = stage_radix_tnode(f_ctx, vaddr, &vi);
	if (err) {
		return err;
	}
	err = drop_del_subtree(f_ctx, vi);
	if (err) {
		return err;
	}
	return 0;
}

static int
drop_subtree_at(struct voluta_file_ctx *f_ctx,
		const struct voluta_vnode_info *parent_vi, size_t slot)
{
	int err;
	struct voluta_vaddr vaddr;

	resolve_child_by_slot(parent_vi, slot, &vaddr);
	if (isbottom(parent_vi)) {
		err = discard_data_leaf(f_ctx, &vaddr);
	} else {
		err = drop_subtree(f_ctx, &vaddr);
	}
	return err;
}

static int do_drop_recursive(struct voluta_file_ctx *f_ctx,
			     struct voluta_vnode_info *vi)
{
	int err;
	const size_t nslots_max = rtn_nchilds_max(vi->u.rtn);

	for (size_t slot = 0; slot < nslots_max; ++slot) {
		err = drop_subtree_at(f_ctx, vi, slot);
		if (err) {
			return err;
		}
	}
	return 0;
}

static int drop_recursive(struct voluta_file_ctx *f_ctx,
			  struct voluta_vnode_info *vi)
{
	int err;

	v_incref(vi);
	err = do_drop_recursive(f_ctx, vi);
	v_decref(vi);

	return err;
}

static int drop_del_subtree(struct voluta_file_ctx *f_ctx,
			    struct voluta_vnode_info *vi)
{
	int err;

	err = drop_recursive(f_ctx, vi);
	if (err) {
		return err;
	}
	err = del_file_tnode(f_ctx, vi);
	if (err) {
		return err;
	}
	return 0;
}

static int drop_by_tree_map(struct voluta_file_ctx *f_ctx)
{
	int err;
	struct voluta_vnode_info *vi;

	if (!file_is_tree_mode(f_ctx->ii)) {
		return 0;
	}
	err = stage_tree_root(f_ctx, &vi);
	if (err) {
		return err;
	}
	err = drop_del_subtree(f_ctx, vi);
	if (err) {
		return err;
	}
	return 0;
}

static int drop_by_head_leaf(struct voluta_file_ctx *f_ctx)
{
	int err;
	struct voluta_vaddr vaddr;

	if (!file_is_leaf_mode(f_ctx->ii)) {
		return 0;
	}
	file_tree_root(f_ctx->ii, &vaddr);
	err = discard_data_leaf(f_ctx, &vaddr);
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

static int drop_meta_and_data(struct voluta_file_ctx *f_ctx)
{
	int err;

	err = drop_by_head_leaf(f_ctx);
	if (err) {
		return err;
	}
	err = drop_by_tree_map(f_ctx);
	if (err) {
		return err;
	}
	reset_tree_mappings(f_ctx->ii);
	return 0;
}

int voluta_drop_reg(struct voluta_inode_info *ii)
{
	int err;
	struct voluta_file_ctx f_ctx = {
		.sbi = i_sbi_of(ii),
		.ii = ii
	};

	i_incref(ii);
	err = drop_meta_and_data(&f_ctx);
	i_decref(ii);

	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int resolve_truncate_end(struct voluta_file_ctx *f_ctx)
{
	int err;
	loff_t tree_end;
	struct voluta_vnode_info *vi;

	if (file_is_leaf_mode(f_ctx->ii)) {
		f_ctx->end = off_max(f_ctx->off, VOLUTA_DS_SIZE);
		return 0;
	}
	if (!file_is_tree_mode(f_ctx->ii)) {
		f_ctx->end = 0;
		return 0;
	}
	err = stage_tree_root(f_ctx, &vi);
	if (err) {
		return err;
	}
	tree_end = rtn_end(vi->u.rtn);
	f_ctx->end = off_max(tree_end, f_ctx->off);
	return 0;
}

static int do_discard_partial(const struct voluta_file_ctx *f_ctx,
			      const struct voluta_rchild_info *rci)
{
	int err;
	loff_t off;
	size_t len;
	struct voluta_vnode_info *vi;

	err = stage_data_leaf_of(f_ctx, rci, &vi);
	if (err) {
		return err;
	}
	off = rci->file_pos;
	len = len_within_ds(off, f_ctx->end);
	voluta_assert(len < VOLUTA_DS_SIZE);

	if (off_is_ds_start(off)) {
		zero_headn(vi->u.ds, len);
	} else {
		zero_range(vi->u.ds, off_in_ds(off), len);
	}
	v_dirtify(vi);
	return 0;
}

static int discard_partial(const struct voluta_file_ctx *f_ctx,
			   const struct voluta_rchild_info *rci)
{
	int err;

	v_incref(rci->parent_vi);
	err = do_discard_partial(f_ctx, rci);
	v_decref(rci->parent_vi);

	return err;
}

static void clear_subtree_mappings(struct voluta_vnode_info *vi, size_t slot)
{
	rtn_reset_child(vi->u.rtn, slot);
	v_dirtify(vi);
}

static int discard_data_leaf_at(struct voluta_file_ctx *f_ctx,
				const struct voluta_rchild_info *rci)
{
	int err;

	v_incref(rci->parent_vi);
	err = discard_data_leaf(f_ctx, &rci->vaddr);
	v_decref(rci->parent_vi);

	return err;
}

static int discard_entire(struct voluta_file_ctx *f_ctx,
			  const struct voluta_rchild_info *rci)
{
	int err;

	err = discard_data_leaf_at(f_ctx, rci);
	if (err) {
		return err;
	}
	if (rci->parent_vi != NULL) {
		clear_subtree_mappings(rci->parent_vi, rci->parent_slot);
	} else {
		reset_tree_mappings(f_ctx->ii);
	}
	return 0;
}

static int discard_data_at(struct voluta_file_ctx *f_ctx,
			   const struct voluta_rchild_info *rci)
{
	int err = 0;
	size_t len;

	if (vaddr_isnull(&rci->vaddr)) {
		return 0;
	}
	len = len_within_ds(rci->file_pos, f_ctx->end);
	if (len < VOLUTA_DS_SIZE) {
		err = discard_partial(f_ctx, rci);
	} else {
		err = discard_entire(f_ctx, rci);
	}
	return err;
}

static int discard_by_tree_map(struct voluta_file_ctx *f_ctx)
{
	int err;
	struct voluta_rchild_info rci;

	if (!file_is_tree_mode(f_ctx->ii)) {
		return 0;
	}
	while (has_more_io(f_ctx)) {
		err = seek_data_by_tree_map(f_ctx, &rci);
		if (err == -ENOENT) {
			break;
		}
		if (err) {
			return err;
		}
		err = discard_data_at(f_ctx, &rci);
		if (err) {
			return err;
		}
		advance_to_next_bk(f_ctx);
	}
	return 0;
}

static int discard_by_head_leaf(struct voluta_file_ctx *f_ctx)
{
	int err;
	struct voluta_rchild_info rci = {
		.parent_vi = NULL
	};

	if (!file_is_leaf_mode(f_ctx->ii)) {
		return 0;
	}
	if (!has_more_head_io(f_ctx)) {
		return 0;
	}
	rci_init(&rci, f_ctx->off);
	file_tree_root(f_ctx->ii, &rci.vaddr);
	err = discard_data_at(f_ctx, &rci);
	if (err) {
		return err;
	}
	advance_to_next_bk(f_ctx);
	return 0;
}

static int discard_data(struct voluta_file_ctx *f_ctx)
{
	int err;

	err = discard_by_head_leaf(f_ctx);
	if (err) {
		return err;
	}
	err = discard_by_tree_map(f_ctx);
	if (err) {
		return err;
	}
	return 0;
}

static int discard_unused_meta(struct voluta_file_ctx *f_ctx)
{
	int err = 0;

	if (f_ctx->beg == 0) {
		err = drop_meta_and_data(f_ctx);
	}
	return err;
}

static int do_truncate(struct voluta_file_ctx *f_ctx)
{
	int err;

	err = check_file_io(f_ctx);
	if (err) {
		return err;
	}
	err = resolve_truncate_end(f_ctx);
	if (err) {
		return err;
	}
	err = discard_data(f_ctx);
	if (err) {
		return err;
	}
	err = discard_unused_meta(f_ctx);
	if (err) {
		return err;
	}
	post_io_update(f_ctx, false);
	return err;
}

int voluta_do_truncate(const struct voluta_oper_ctx *op_ctx,
		       struct voluta_inode_info *ii, loff_t off)
{
	int err;
	const loff_t isz = i_size_of(ii);
	const size_t len = (off < isz) ? off_len(off, isz) : 0;
	struct voluta_file_ctx f_ctx = {
		.op_ctx = op_ctx,
		.sbi = i_sbi_of(ii),
		.ii = ii,
		.start = off,
		.len = len,
		.beg = off,
		.off = off,
		.end = off_end(off, len),
		.op = OP_TRUNC,
	};

	i_incref(ii);
	err = do_truncate(&f_ctx);
	i_decref(ii);

	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int lseek_data_leaf(struct voluta_file_ctx *f_ctx,
			   struct voluta_rchild_info *rci)
{
	int err;

	err = seek_data_by_head_leaf(f_ctx, rci);
	if (!err || (err != -ENOENT)) {
		return err;
	}
	err = seek_data_by_tree_map(f_ctx, rci);
	if (err) {
		return err;
	}
	return 0;
}

static int lseek_data(struct voluta_file_ctx *f_ctx)
{
	int err;
	struct voluta_rchild_info rci = {
		.parent_vi = NULL,
	};

	err = lseek_data_leaf(f_ctx, &rci);
	if (err == 0) {
		f_ctx->off = off_max(rci.file_pos, f_ctx->off);
	} else if (err == -ENOENT) {
		f_ctx->off = i_size_of(f_ctx->ii);
		err = 0;
	}
	return err;
}

static int lseek_notsupp(struct voluta_file_ctx *f_ctx)
{
	f_ctx->off = f_ctx->end;
	return -ENOTSUP;
}

static int lseek_hole(struct voluta_file_ctx *f_ctx)
{
	return lseek_notsupp(f_ctx);
}

static int check_lseek(const struct voluta_file_ctx *f_ctx)
{
	int err;
	const loff_t isz = i_size_of(f_ctx->ii);

	err = check_file_io(f_ctx);
	if (err) {
		return err;
	}
	if (f_ctx->off >= isz) {
		return -ENXIO;
	}
	return 0;
}

static int do_lseek(struct voluta_file_ctx *f_ctx)
{
	int err;

	err = check_lseek(f_ctx);
	if (err) {
		return err;
	}
	if (f_ctx->whence == SEEK_DATA) {
		err = lseek_data(f_ctx);
	} else if (f_ctx->whence == SEEK_HOLE) {
		err =  lseek_hole(f_ctx);
	} else {
		err = lseek_notsupp(f_ctx);
	}
	return err;
}

int voluta_do_lseek(const struct voluta_oper_ctx *op_ctx,
		    struct voluta_inode_info *ii,
		    loff_t off, int whence, loff_t *out_off)
{
	int err;
	struct voluta_file_ctx f_ctx = {
		.op_ctx = op_ctx,
		.sbi = i_sbi_of(ii),
		.ii = ii,
		.start = off,
		.len = 0,
		.beg = off,
		.off = off,
		.end = i_size_of(ii),
		.op = OP_LSEEK,
		.whence = whence
	};

	i_incref(ii);
	err = do_lseek(&f_ctx);
	i_decref(ii);

	*out_off = f_ctx.off;
	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool fl_reserve_range(const struct voluta_file_ctx *f_ctx)
{
	const int fl_mask = FALLOC_FL_KEEP_SIZE;

	return (f_ctx->fl_mode & ~fl_mask) == 0;
}

static bool fl_keep_size(const struct voluta_file_ctx *f_ctx)
{
	const int fl_mask = FALLOC_FL_KEEP_SIZE;

	return (f_ctx->fl_mode & fl_mask) == fl_mask;
}

static bool fl_punch_hole(const struct voluta_file_ctx *f_ctx)
{
	const int fl_mask = FALLOC_FL_PUNCH_HOLE;

	return (f_ctx->fl_mode & fl_mask) == fl_mask;
}

/*
 * TODO-0012: Proper handling for FALLOC_FL_KEEP_SIZE beyond file size
 *
 * See 'man 2 fallocate' for semantics details of FALLOC_FL_KEEP_SIZE
 * beyond end-of-file.
 */
static int check_fl_mode(const struct voluta_file_ctx *f_ctx)
{
	const int fl_mode = f_ctx->fl_mode;
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

static int reserve_data_leaf(const struct voluta_file_ctx *f_ctx,
			     struct voluta_vaddr *out_vaddr)
{
	int err;
	struct voluta_iattr iattr;
	struct voluta_inode_info *ii = f_ctx->ii;

	err = alloc_data_leaf_space(f_ctx, out_vaddr);
	if (err) {
		return err;
	}
	iattr_setup(&iattr, i_ino_of(ii));
	iattr.ia_blocks = i_blocks_of(ii) + 1;
	iattr.ia_flags = VOLUTA_IATTR_BLOCKS;
	update_iattrs(f_ctx->op_ctx, ii, &iattr);
	return 0;
}

static int reserve_bind_leaf(const struct voluta_file_ctx *f_ctx,
			     struct voluta_vnode_info *parent_vi)
{
	int err;
	struct voluta_rchild_info rci;

	child_of_current_pos(f_ctx, parent_vi, &rci);
	if (!vaddr_isnull(&rci.vaddr)) {
		return 0;
	}
	err = reserve_data_leaf(f_ctx, &rci.vaddr);
	if (err) {
		return err;
	}
	assign_child_by_pos(parent_vi, f_ctx->off, &rci.vaddr);
	v_dirtify(parent_vi);
	return 0;
}

static int reserve_bind_leaves(struct voluta_file_ctx *f_ctx,
			       struct voluta_vnode_info *parent_vi)
{
	int err;
	bool next_mapping = false;

	while (has_more_io(f_ctx) && !next_mapping) {
		err = reserve_bind_leaf(f_ctx, parent_vi);
		if (err) {
			return err;
		}
		advance_to_next_pos(f_ctx);
		next_mapping = is_mapping_boundaries(f_ctx);
	}
	return 0;
}

static int reserve_leaves(struct voluta_file_ctx *f_ctx)
{
	int err;
	size_t level;
	struct voluta_vnode_info *vi;
	const size_t height = file_tree_height(f_ctx->ii);

	err = stage_tree_root(f_ctx, &vi);
	if (err) {
		return err;
	}
	level = height;
	while (level > 0) {
		if (isbottom(vi)) {
			return reserve_bind_leaves(f_ctx, vi);
		}
		err = stage_or_create_node(f_ctx, vi, &vi);
		if (err) {
			return err;
		}
		level--;
	}
	return -EFSCORRUPTED;
}

static int reserve_by_tree_map(struct voluta_file_ctx *f_ctx)
{
	int err;

	err = create_tree_levels(f_ctx);
	if (err) {
		return err;
	}
	err = reserve_leaves(f_ctx);
	if (err) {
		return err;
	}
	return 0;
}

static int fallocate_reserve_by_tree_map(struct voluta_file_ctx *f_ctx)
{
	int err = 0;

	while (has_more_io(f_ctx) && !err) {
		err = reserve_by_tree_map(f_ctx);
	}
	return err;
}

static int fallocate_reserve_by_head_leaf(struct voluta_file_ctx *f_ctx)
{
	int err;
	struct voluta_vnode_info *vi;

	if (!has_more_head_io(f_ctx)) {
		return 0;
	}
	if (file_is_tree_mode(f_ctx->ii)) {
		return 0;
	}
	err = stage_or_create_head_leaf(f_ctx, &vi);
	if (err) {
		return err;
	}
	advance_to_next_pos(f_ctx);
	return 0;
}

static int fallocate_reserve(struct voluta_file_ctx *f_ctx)
{
	int err;

	err = fallocate_reserve_by_head_leaf(f_ctx);
	if (err) {
		return err;
	}
	err = fallocate_reserve_by_tree_map(f_ctx);
	if (err) {
		return err;
	}
	return 0;
}

static int fallocate_punch_hole(struct voluta_file_ctx *f_ctx)
{
	return discard_data(f_ctx);
}

static int do_fallocate_op(struct voluta_file_ctx *f_ctx)
{
	int err;

	if (fl_reserve_range(f_ctx)) {
		err = fallocate_reserve(f_ctx);
	} else if (fl_punch_hole(f_ctx)) {
		err = fallocate_punch_hole(f_ctx);
	} else {
		err = -ENOTSUP;
	}
	return err;
}

static int do_fallocate(struct voluta_file_ctx *f_ctx)
{
	int err;

	err = check_file_io(f_ctx);
	if (err) {
		return err;
	}
	err = check_fl_mode(f_ctx);
	if (err) {
		return err;
	}
	err = do_fallocate_op(f_ctx);
	if (err) {
		return err;
	}
	post_io_update(f_ctx, false);
	return err;
}

int voluta_do_fallocate(const struct voluta_oper_ctx *op_ctx,
			struct voluta_inode_info *ii,
			int mode, loff_t off, loff_t length)
{
	int err;
	const size_t len = (size_t)length;
	struct voluta_file_ctx f_ctx = {
		.op_ctx = op_ctx,
		.sbi = i_sbi_of(ii),
		.ii = ii,
		.start = off,
		.len = len,
		.beg = off,
		.off = off,
		.end = off_end(off, len),
		.op = OP_FALLOC,
		.fl_mode = mode,
	};

	i_incref(ii);
	err = do_fallocate(&f_ctx);
	i_decref(ii);

	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t nbytes_of(const struct voluta_vaddr *vaddr)
{
	return voluta_persistent_size(vaddr->vtype);
}

static bool emit_fiemap_ext(struct voluta_file_ctx *f_ctx,
			    const struct voluta_vaddr *vaddr, size_t *out_len)
{
	loff_t end;
	struct fiemap_extent *fm_ext;
	struct fiemap *fm = f_ctx->fm;
	const size_t len = nbytes_of(vaddr);

	end = off_min(off_end(f_ctx->off, len), f_ctx->end);
	*out_len = len_within_ds(f_ctx->off, end);
	if (*out_len == 0) {
		return true;
	}
	if (vaddr_isnull(vaddr)) {
		return false;
	}
	fm_ext = &fm->fm_extents[fm->fm_mapped_extents++];
	if (fm->fm_mapped_extents <= fm->fm_extent_count) {
		fm_ext->fe_flags = FIEMAP_EXTENT_DATA_ENCRYPTED;
		fm_ext->fe_logical = (uint64_t)(f_ctx->off);
		fm_ext->fe_physical = (uint64_t)(vaddr->off);
		fm_ext->fe_length = *out_len;
	}
	return (fm->fm_mapped_extents == fm->fm_extent_count);
}

static void emit_fiemap(struct voluta_file_ctx *f_ctx,
			const struct voluta_vaddr *vaddr, size_t *out_len)
{
	*out_len = 0;
	f_ctx->fm_stop = emit_fiemap_ext(f_ctx, vaddr, out_len);
}

static int do_fiemap_by_leaves(struct voluta_file_ctx *f_ctx,
			       struct voluta_vnode_info *parent_vi)
{
	size_t len;
	struct voluta_rchild_info rci;

	while (has_more_io(f_ctx)) {
		child_of_current_pos(f_ctx, parent_vi, &rci);
		emit_fiemap(f_ctx, &rci.vaddr, &len);
		advance_by_nbytes(f_ctx, len);
		if (is_mapping_boundaries(f_ctx)) {
			break;
		}
	}
	return 0;
}

static int fiemap_by_leaves(struct voluta_file_ctx *f_ctx,
			    struct voluta_vnode_info *parent_vi)
{
	int err;

	v_incref(parent_vi);
	err = do_fiemap_by_leaves(f_ctx, parent_vi);
	v_decref(parent_vi);

	return err;
}

static int fiemap_by_tree_map(struct voluta_file_ctx *f_ctx)
{
	int err;
	struct voluta_rchild_info rci = {
		.parent_vi = NULL
	};

	while (has_more_io(f_ctx)) {
		err = seek_data_by_tree_map(f_ctx, &rci);
		if (err == -ENOENT) {
			break;
		}
		if (err) {
			return err;
		}
		err = fiemap_by_leaves(f_ctx, rci.parent_vi);
		if (err) {
			return err;
		}
		/* TODO: need to skip large holes */
	}
	return 0;
}

static int fiemap_by_head_leaf(struct voluta_file_ctx *f_ctx)
{
	int err;
	size_t len;
	struct voluta_vnode_info *vi = NULL;

	if (!file_is_leaf_mode(f_ctx->ii)) {
		return 0;
	}
	if (!has_more_head_io(f_ctx)) {
		return 0;
	}
	err = stage_head_leaf(f_ctx, &vi);
	if (err && (err != -ENOENT)) {
		return err;
	}
	emit_fiemap(f_ctx, v_vaddr_of(vi), &len);
	advance_by_nbytes(f_ctx, len);
	return 0;
}

static int fiemap_data(struct voluta_file_ctx *f_ctx)
{
	int err;

	err = fiemap_by_head_leaf(f_ctx);
	if (err) {
		return err;
	}
	err = fiemap_by_tree_map(f_ctx);
	if (err) {
		return err;
	}
	return 0;
}

static int check_fm_flags(const struct voluta_file_ctx *f_ctx)
{
	const int fm_allowed =
		FIEMAP_FLAG_SYNC | FIEMAP_FLAG_XATTR | FIEMAP_FLAG_CACHE;

	if (f_ctx->fm_flags & ~fm_allowed) {
		return -ENOTSUP;
	}
	if (f_ctx->fm_flags & fm_allowed) {
		return -ENOTSUP;
	}
	return 0;
}

static int do_fiemap(struct voluta_file_ctx *f_ctx)
{
	int err;

	err = check_file_io(f_ctx);
	if (err) {
		return err;
	}
	err = check_fm_flags(f_ctx);
	if (err) {
		return err;
	}
	err = fiemap_data(f_ctx);
	if (err) {
		return err;
	}
	return 0;
}

static loff_t fiemap_end(const struct voluta_inode_info *ii,
			 loff_t off, size_t len)
{
	const loff_t end = off_end(off, len);
	const loff_t isz = i_size_of(ii);

	return off_min(end, isz);
}

int voluta_do_fiemap(const struct voluta_oper_ctx *op_ctx,
		     struct voluta_inode_info *ii, struct fiemap *fm)
{
	int err;
	const loff_t off = (loff_t)fm->fm_start;
	const size_t len = (size_t)fm->fm_length;
	struct voluta_file_ctx f_ctx = {
		.op_ctx = op_ctx,
		.sbi = i_sbi_of(ii),
		.ii = ii,
		.start = off,
		.len = len,
		.beg = off,
		.off = off,
		.end = fiemap_end(ii, off, len),
		.op = OP_FIEMAP,
		.fm = fm,
		.fm_flags = (int)(fm->fm_flags),
		.fm_stop = 0
	};

	fm->fm_mapped_extents = 0;

	i_incref(ii);
	err = do_fiemap(&f_ctx);
	i_decref(ii);

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
