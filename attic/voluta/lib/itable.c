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
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include "libvoluta.h"

#define ITNODE_ROOT_DEPTH 1

static int lookup_iref(struct voluta_sb_info *sbi,
		       struct voluta_vnode_info *itn_vi, ino_t ino,
		       struct voluta_iaddr *out_iaddr);

static int insert_iref(struct voluta_sb_info *sbi,
		       struct voluta_vnode_info *itn_vi,
		       const struct voluta_iaddr *iaddr);

static int remove_iref(struct voluta_sb_info *sbi,
		       struct voluta_vnode_info *itn_vi, ino_t ino);

static int scan_subtree(struct voluta_sb_info *sbi,
			struct voluta_vnode_info *itn_vi);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void iaddr_setup(struct voluta_iaddr *iaddr, ino_t ino, loff_t off)
{
	vaddr_by_off(&iaddr->vaddr, VOLUTA_VTYPE_INODE, off);
	iaddr->ino = ino;
}

static void vaddr_of_itnode(struct voluta_vaddr *vaddr, loff_t off)
{
	vaddr_by_off(vaddr, VOLUTA_VTYPE_ITNODE, off);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static ino_t ite_ino(const struct voluta_itable_entry *ite)
{
	return ino_to_cpu(ite->ino);
}

static void ite_set_ino(struct voluta_itable_entry *ite, ino_t ino)
{
	ite->ino = cpu_to_ino(ino);
}

static loff_t ite_off(const struct voluta_itable_entry *ite)
{
	return off_to_cpu(ite->off);
}

static void ite_set_off(struct voluta_itable_entry *ite, loff_t off)
{
	ite->off = cpu_to_off(off);
}

static bool ite_isfree(const struct voluta_itable_entry *ite)
{
	return ino_isnull(ite_ino(ite));
}

static bool ite_has_ino(const struct voluta_itable_entry *ite, ino_t ino)
{
	return (ite_ino(ite) == ino);
}

static void ite_setup(struct voluta_itable_entry *ite, ino_t ino, loff_t off)
{
	ite_set_ino(ite, ino);
	ite_set_off(ite, off);
}

static void ite_reset(struct voluta_itable_entry *ite)
{
	ite_set_ino(ite, VOLUTA_INO_NULL);
	ite_set_off(ite, VOLUTA_OFF_NULL);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static loff_t itn_parent_off(const struct voluta_itable_tnode *itn)
{
	return off_to_cpu(itn->it_parent_off);
}

static void itn_set_parent_off(struct voluta_itable_tnode *itn, loff_t off)
{
	itn->it_parent_off = cpu_to_off(off);
}

static size_t itn_depth(const struct voluta_itable_tnode *itn)
{
	return le16_to_cpu(itn->it_depth);
}

static void itn_set_depth(struct voluta_itable_tnode *itn, size_t depth)
{
	itn->it_depth = cpu_to_le16((uint16_t)depth);
}

static size_t itn_nents(const struct voluta_itable_tnode *itn)
{
	return le16_to_cpu(itn->it_nents);
}

static void itn_set_nents(struct voluta_itable_tnode *itn, size_t nents)
{
	itn->it_nents = cpu_to_le16((uint16_t)nents);
}

static void itn_inc_nents(struct voluta_itable_tnode *itn)
{
	itn_set_nents(itn, itn_nents(itn) + 1);
}

static void itn_dec_nents(struct voluta_itable_tnode *itn)
{
	itn_set_nents(itn, itn_nents(itn) - 1);
}

static size_t itn_nchilds(const struct voluta_itable_tnode *itn)
{
	return le16_to_cpu(itn->it_nchilds);
}

static size_t itn_nchilds_max(const struct voluta_itable_tnode *itn)
{
	return ARRAY_SIZE(itn->it_child);
}

static void itn_set_nchilds(struct voluta_itable_tnode *itn, size_t nchilds)
{
	voluta_assert_le(nchilds, itn_nchilds_max(itn));
	itn->it_nchilds = cpu_to_le16((uint16_t)nchilds);
}

static void itn_inc_nchilds(struct voluta_itable_tnode *itn)
{
	itn_set_nchilds(itn, itn_nchilds(itn) + 1);
}

static void itn_dec_nchilds(struct voluta_itable_tnode *itn)
{
	itn_set_nchilds(itn, itn_nchilds(itn) - 1);
}

static loff_t itn_child_at(const struct voluta_itable_tnode *itn, size_t slot)
{
	voluta_assert_lt(slot, itn_nchilds_max(itn));

	return off_to_cpu(itn->it_child[slot]);
}

static void itn_set_child_at(struct voluta_itable_tnode *itn,
			     size_t slot, loff_t off)
{
	voluta_assert_lt(slot, itn_nchilds_max(itn));

	itn->it_child[slot] = cpu_to_off(off);
}

static void itn_clear_child_at(struct voluta_itable_tnode *itn, size_t slot)
{
	itn_set_child_at(itn, slot, VOLUTA_OFF_NULL);
}

static size_t itn_child_slot(const struct voluta_itable_tnode *itn, ino_t ino)
{
	size_t slot;
	size_t shift;
	const size_t depth = itn_depth(itn);

	shift = VOLUTA_ITNODE_SHIFT * (depth - 1);
	slot = (ino >> shift) % itn_nchilds_max(itn);
	return slot;
}

static size_t itn_nents_max(const struct voluta_itable_tnode *itn)
{
	return ARRAY_SIZE(itn->ite);
}

static struct voluta_itable_entry *
itn_entry_at(const struct voluta_itable_tnode *itn, size_t slot)
{
	const struct voluta_itable_entry *ite = &itn->ite[slot];

	return unconst(ite);
}

static void itn_init(struct voluta_itable_tnode *itn,
		     loff_t parent_off, size_t depth)
{
	const size_t nents_max = itn_nents_max(itn);
	const size_t nchilds_max = itn_nchilds_max(itn);

	itn_set_parent_off(itn, parent_off);
	itn_set_depth(itn, depth);
	itn_set_nents(itn, 0);
	itn_set_nchilds(itn, 0);

	for (size_t i = 0; i < nchilds_max; ++i) {
		itn_clear_child_at(itn, i);
	}
	for (size_t j = 0; j < nents_max; ++j) {
		ite_reset(itn_entry_at(itn, j));
	}
}

static bool itn_isfull(const struct voluta_itable_tnode *itn)
{
	return (itn_nents(itn) == itn_nents_max(itn));
}

static bool itn_isempty(const struct voluta_itable_tnode *itn)
{
	return (itn_nents(itn) == 0);
}

static const struct voluta_itable_entry *
itn_find_next(const struct voluta_itable_tnode *itn,
	      const struct voluta_itable_entry *from)
{
	size_t slot_beg;
	const struct voluta_itable_entry *ite;
	const size_t nents_max = itn_nents_max(itn);

	if (itn_isempty(itn)) {
		return NULL;
	}
	slot_beg = (from != NULL) ? (size_t)(from - itn->ite) : 0;
	for (size_t i = slot_beg; i < nents_max; ++i) {
		ite = itn_entry_at(itn, i);
		if (!ite_isfree(ite)) {
			return ite;
		}
	}
	return NULL;
}

static size_t itn_slot_by_ino(const struct voluta_itable_tnode *itn, ino_t ino)
{
	return ino % itn_nents_max(itn);
}

static struct voluta_itable_entry *
itn_lookup(const struct voluta_itable_tnode *itn, ino_t ino)
{
	size_t slot;
	const struct voluta_itable_entry *ite;

	if (!itn_isempty(itn)) {
		slot = itn_slot_by_ino(itn, ino);
		ite = itn_entry_at(itn, slot);
		if (ite_has_ino(ite, ino)) {
			return unconst(ite);
		}
	}
	return NULL;
}

static struct voluta_itable_entry *
itn_insert(struct voluta_itable_tnode *itn, ino_t ino, loff_t off)
{
	size_t slot;
	struct voluta_itable_entry *ite;

	if (!itn_isfull(itn)) {
		slot = itn_slot_by_ino(itn, ino);
		ite = itn_entry_at(itn, slot);
		if (ite_isfree(ite)) {
			ite_setup(ite, ino, off);
			itn_inc_nents(itn);
			return ite;
		}
	}
	return NULL;
}

static struct voluta_itable_entry *
itn_remove(struct voluta_itable_tnode *itn, ino_t ino)
{
	struct voluta_itable_entry *ite;

	ite = itn_lookup(itn, ino);
	if (ite) {
		ite_reset(ite);
		itn_dec_nents(itn);
	}
	return ite;
}

static void itn_set_child(struct voluta_itable_tnode *itn,
			  ino_t ino, loff_t off)
{
	const size_t slot = itn_child_slot(itn, ino);

	itn_set_child_at(itn, slot, off);
	itn_inc_nchilds(itn);
}

static void itn_clear_child(struct voluta_itable_tnode *itn, ino_t ino)
{
	const size_t slot = itn_child_slot(itn, ino);

	itn_clear_child_at(itn, slot);
	itn_dec_nchilds(itn);
}

static bool itn_isleaf(const struct voluta_itable_tnode *itn)
{
	return (itn_nchilds(itn) == 0);
}

static bool itn_isroot(const struct voluta_itable_tnode *itn)
{
	return (itn_depth(itn) == ITNODE_ROOT_DEPTH);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void itable_init(struct voluta_itable *itable)
{
	vaddr_reset(&itable->it_root_vaddr);
	itable->it_ino_rootd = VOLUTA_INO_NULL;
	itable->it_ino_apex = VOLUTA_INO_ROOT + 1;
	itable->it_ninodes = 0;
	itable->it_ninodes_max = ULONG_MAX / 2;
}

static void itable_fini(struct voluta_itable *itable)
{
	vaddr_reset(&itable->it_root_vaddr);
	itable->it_ino_rootd = VOLUTA_INO_NULL;
	itable->it_ino_apex = VOLUTA_INO_NULL;
	itable->it_ninodes = 0;
	itable->it_ninodes_max = 0;
}

static int itable_next_ino(struct voluta_itable *itable, ino_t *out_ino)
{
	if (itable->it_ninodes >= itable->it_ninodes_max) {
		return -ENOSPC;
	}
	itable->it_ino_apex += 1;
	*out_ino = itable->it_ino_apex;
	return 0;
}

static void itable_add_ino(struct voluta_itable *itable, ino_t ino)
{
	itable->it_ninodes++;
	if (itable->it_ino_apex < ino) {
		itable->it_ino_apex = ino;
	}
}

static void itable_remove_ino(struct voluta_itable *itable, ino_t ino)
{
	voluta_assert_gt(itable->it_ninodes, 0);
	voluta_assert_ge(itable->it_ino_apex, ino);

	itable->it_ninodes--;
}

static void itable_parse_itn(struct voluta_itable *itable,
			     const struct voluta_itable_tnode *itn)
{
	ino_t ino;
	const struct voluta_itable_entry *ite;

	ite = itn_find_next(itn, NULL);
	while (ite != NULL) {
		ino = ite_ino(ite);
		itable_add_ino(itable, ino);
		ite = itn_find_next(itn, ite + 1);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_itable *itable_of(const struct voluta_sb_info *sbi)
{
	const struct voluta_itable *itable = &sbi->s_itable;

	return unconst(itable);
}

static int stage_itnode(struct voluta_sb_info *sbi,
			const struct voluta_vaddr *vaddr,
			struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *itn_vi;

	if (vaddr_isnull(vaddr)) {
		return -ENOENT;
	}
	err = voluta_stage_vnode(sbi, vaddr, NULL, &itn_vi);
	if (err) {
		return err;
	}
	*out_vi = itn_vi;
	return 0;
}

static void resolve_child_at(const struct voluta_vnode_info *vi, size_t slot,
			     struct voluta_vaddr *out_vaddr)
{
	const loff_t off = itn_child_at(vi->u.itn, slot);

	vaddr_of_itnode(out_vaddr, off);
}

static void resolve_child(const struct voluta_vnode_info *vi, ino_t ino,
			  struct voluta_vaddr *out_vaddr)
{
	const size_t slot = itn_child_slot(vi->u.itn, ino);

	resolve_child_at(vi, slot, out_vaddr);
}

static int stage_child(struct voluta_sb_info *sbi,
		       const struct voluta_vnode_info *itn_vi, ino_t ino,
		       struct voluta_vnode_info **out_child_vi)
{
	struct voluta_vaddr child_vaddr;

	resolve_child(itn_vi, ino, &child_vaddr);
	return stage_itnode(sbi, &child_vaddr, out_child_vi);
}

static int stage_itroot(struct voluta_sb_info *sbi,
			struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *itroot_vi;
	const struct voluta_itable *itable = itable_of(sbi);

	err = stage_itnode(sbi, &itable->it_root_vaddr, &itroot_vi);
	if (err) {
		return err;
	}
	if (!itn_isroot(itroot_vi->u.itn)) {
		return -EFSCORRUPTED;
	}
	*out_vi = itroot_vi;
	return 0;
}

static void iaddr_by_ite(struct voluta_iaddr *iaddr,
			 const struct voluta_itable_entry *ite)
{
	iaddr_setup(iaddr, ite_ino(ite), ite_off(ite));
}

static int lookup_at(const struct voluta_vnode_info *itn_vi,
		     ino_t ino, struct voluta_iaddr *out_iaddr)
{
	const struct voluta_itable_entry *ite;

	ite = itn_lookup(itn_vi->u.itn, ino);
	if (ite == NULL) {
		return -ENOENT;
	}
	iaddr_by_ite(out_iaddr, ite);
	return 0;
}

static int do_lookup_iref(struct voluta_sb_info *sbi,
			  struct voluta_vnode_info *itn_vi, ino_t ino,
			  struct voluta_iaddr *out_iaddr)
{
	int err;
	struct voluta_vnode_info *child_vi;

	err = lookup_at(itn_vi, ino, out_iaddr);
	if (!err) {
		return 0;
	}
	err = stage_child(sbi, itn_vi, ino, &child_vi);
	if (err) {
		return err;
	}
	err = lookup_iref(sbi, child_vi, ino, out_iaddr);
	if (err) {
		return err;
	}
	return 0;
}

static int lookup_iref(struct voluta_sb_info *sbi,
		       struct voluta_vnode_info *itn_vi, ino_t ino,
		       struct voluta_iaddr *out_iaddr)
{
	int err;

	v_incref(itn_vi);
	err = do_lookup_iref(sbi, itn_vi, ino, out_iaddr);
	v_decref(itn_vi);

	return err;
}

static int lookup_iaddr_of(struct voluta_sb_info *sbi, ino_t ino,
			   struct voluta_iaddr *out_iaddr)
{
	int err;
	struct voluta_vnode_info *itroot_vi = NULL;

	err = stage_itroot(sbi, &itroot_vi);
	if (err) {
		return err;
	}
	err = lookup_iref(sbi, itroot_vi, ino, out_iaddr);
	if (err) {
		return err;
	}
	return 0;
}

static int new_itnode(struct voluta_sb_info *sbi,
		      const struct voluta_vaddr *parent_vaddr,
		      size_t depth, struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *itn_vi;

	err = voluta_new_vnode(sbi, NULL, VOLUTA_VTYPE_ITNODE, &itn_vi);
	if (err) {
		return err;
	}
	itn_init(itn_vi->u.itn, parent_vaddr->off, depth);
	v_dirtify(itn_vi);
	*out_vi = itn_vi;
	return 0;
}

static int del_itnode(struct voluta_sb_info *sbi,
		      struct voluta_vnode_info *itn_vi)
{
	return voluta_del_vnode(sbi, itn_vi);
}

static int create_itroot(struct voluta_sb_info *sbi,
			 struct voluta_vnode_info **out_vi)
{
	struct voluta_vaddr vaddr;

	vaddr_of_itnode(&vaddr, VOLUTA_OFF_NULL);
	return new_itnode(sbi, &vaddr, ITNODE_ROOT_DEPTH, out_vi);
}

static void bind_child(struct voluta_vnode_info *vi, ino_t ino,
		       struct voluta_vnode_info *child_vi)
{
	const struct voluta_vaddr *vaddr = v_vaddr_of(child_vi);

	itn_set_child(vi->u.itn, ino, vaddr->off);
	v_dirtify(vi);
}

static int create_itchild(struct voluta_sb_info *sbi,
			  struct voluta_vnode_info *vi, ino_t ino,
			  struct voluta_vnode_info **out_child_vi)
{
	int err;
	size_t depth, depth_max = 16;
	struct voluta_vnode_info *child_vi;

	depth = itn_depth(vi->u.itn);
	if (depth >= depth_max) {
		return -ENOSPC;
	}
	err = new_itnode(sbi, v_vaddr_of(vi), depth + 1, &child_vi);
	if (err) {
		return err;
	}
	bind_child(vi, ino, child_vi);
	*out_child_vi = child_vi;
	return 0;
}

static int require_child(struct voluta_sb_info *sbi,
			 struct voluta_vnode_info *vi, ino_t ino,
			 struct voluta_vnode_info **out_child_vi)
{
	int err;
	struct voluta_vaddr child_vaddr;

	resolve_child(vi, ino, &child_vaddr);
	if (!vaddr_isnull(&child_vaddr)) {
		err = stage_itnode(sbi, &child_vaddr, out_child_vi);
	} else {
		err = create_itchild(sbi, vi, ino, out_child_vi);
	}
	return err;
}

static int try_insert_at(struct voluta_sb_info *sbi,
			 struct voluta_vnode_info *itn_vi,
			 const struct voluta_iaddr *iaddr)
{
	struct voluta_itable_entry *ite;
	struct voluta_itable *itable = itable_of(sbi);

	ite = itn_insert(itn_vi->u.itn, iaddr->ino, iaddr->vaddr.off);
	if (ite == NULL) {
		return -ENOSPC;
	}
	itable_add_ino(itable, iaddr->ino);
	v_dirtify(itn_vi);
	return 0;
}

static int do_insert_iref(struct voluta_sb_info *sbi,
			  struct voluta_vnode_info *itn_vi,
			  const struct voluta_iaddr *iaddr)
{
	int err;
	struct voluta_vnode_info *child_vi = NULL;

	err = try_insert_at(sbi, itn_vi, iaddr);
	if (!err) {
		return 0;
	}
	err = require_child(sbi, itn_vi, iaddr->ino, &child_vi);
	if (err) {
		return err;
	}
	err = insert_iref(sbi, child_vi, iaddr);
	if (err) {
		return err;
	}
	return 0;
}

static int insert_iref(struct voluta_sb_info *sbi,
		       struct voluta_vnode_info *itn_vi,
		       const struct voluta_iaddr *iaddr)
{
	int err;

	v_incref(itn_vi);
	err = do_insert_iref(sbi, itn_vi, iaddr);
	v_decref(itn_vi);

	return err;
}

static int try_remove_at(struct voluta_sb_info *sbi,
			 struct voluta_vnode_info *vi, ino_t ino)
{
	struct voluta_itable_entry *ite;

	ite = itn_remove(vi->u.itn, ino);
	if (ite == NULL) {
		return -ENOENT;
	}
	itable_remove_ino(itable_of(sbi), ino);
	v_dirtify(vi);
	return 0;
}

static int prune_if_empty_leaf(struct voluta_sb_info *sbi,
			       struct voluta_vnode_info *parent_vi,
			       struct voluta_vnode_info *child_vi, ino_t ino)
{
	int err;

	if (!itn_isempty(child_vi->u.itn)) {
		return 0;
	}
	if (!itn_isleaf(child_vi->u.itn)) {
		return 0;
	}
	if (itn_isroot(child_vi->u.itn)) {
		return 0;
	}
	err = del_itnode(sbi, child_vi);
	if (err) {
		return err;
	}
	itn_clear_child(parent_vi->u.itn, ino);
	v_dirtify(parent_vi);
	return 0;
}

static int do_remove_iref(struct voluta_sb_info *sbi,
			  struct voluta_vnode_info *itn_vi, ino_t ino)
{
	int err;
	struct voluta_vnode_info *child_vi = NULL;

	err = try_remove_at(sbi, itn_vi, ino);
	if (!err) {
		return 0;
	}
	err = stage_child(sbi, itn_vi, ino, &child_vi);
	if (err) {
		return err;
	}
	err = remove_iref(sbi, child_vi, ino);
	if (err) {
		return err;
	}
	err = prune_if_empty_leaf(sbi, itn_vi, child_vi, ino);
	if (err) {
		return err;
	}
	return 0;
}

static int remove_iref(struct voluta_sb_info *sbi,
		       struct voluta_vnode_info *itn_vi, ino_t ino)
{
	int err;

	v_incref(itn_vi);
	err = do_remove_iref(sbi, itn_vi, ino);
	v_decref(itn_vi);

	return err;
}

static int remove_itentry(struct voluta_sb_info *sbi, ino_t ino)
{
	int err;
	struct voluta_vnode_info *itroot_vi;

	err = stage_itroot(sbi, &itroot_vi);
	if (err) {
		return err;
	}
	err = remove_iref(sbi, itroot_vi, ino);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_acquire_ino(struct voluta_sb_info *sbi, struct voluta_iaddr *iaddr)
{
	int err;
	struct voluta_vnode_info *itroot_vi;
	struct voluta_itable *itable = itable_of(sbi);

	err = itable_next_ino(itable, &iaddr->ino);
	if (err) {
		return err;
	}
	err = stage_itroot(sbi, &itroot_vi);
	if (err) {
		return err;
	}
	err = insert_iref(sbi, itroot_vi, iaddr);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_real_ino(const struct voluta_sb_info *sbi,
		    ino_t ino, ino_t *out_ino)
{
	const struct voluta_itable *itable = itable_of(sbi);

	if ((ino < VOLUTA_INO_ROOT) || (ino > VOLUTA_INO_MAX)) {
		return -EINVAL;
	}
	if (ino == VOLUTA_INO_ROOT) {
		if (ino_isnull(itable->it_ino_rootd)) {
			return -ENOENT;
		}
		*out_ino = itable->it_ino_rootd;
	} else {
		*out_ino = ino;
	}
	return 0;
}

int voluta_discard_ino(struct voluta_sb_info *sbi, ino_t xino)
{
	int err;
	ino_t ino;

	err = voluta_real_ino(sbi, xino, &ino);
	if (err) {
		return err;
	}
	err = remove_itentry(sbi, ino);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_resolve_ino(struct voluta_sb_info *sbi, ino_t xino,
		       struct voluta_iaddr *out_iaddr)
{
	int err;
	ino_t ino;

	err = voluta_real_ino(sbi, xino, &ino);
	if (err) {
		return err;
	}
	err = lookup_iaddr_of(sbi, ino, out_iaddr);
	if (err) {
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_create_itable(struct voluta_sb_info *sbi)
{
	int err;
	struct voluta_vnode_info *itroot_vi;
	struct voluta_itable *itable = itable_of(sbi);

	voluta_assert(vaddr_isnull(&itable->it_root_vaddr));
	voluta_assert_eq(itable->it_ino_rootd, VOLUTA_INO_NULL);

	err = create_itroot(sbi, &itroot_vi);
	if (err) {
		return err;
	}
	vaddr_clone(v_vaddr_of(itroot_vi), &itable->it_root_vaddr);
	return 0;
}

static int reload_itable_root(struct voluta_sb_info *sbi,
			      const struct voluta_vaddr *vaddr)
{
	int err;
	struct voluta_vnode_info *root_vi;
	struct voluta_itable *itable = itable_of(sbi);

	err = stage_itnode(sbi, vaddr, &root_vi);
	if (err) {
		return err;
	}
	if (!itn_isroot(root_vi->u.itn)) {
		return -ENOENT;
	}
	vaddr_clone(vaddr, &itable->it_root_vaddr);
	return 0;
}

static void scan_entries_of(struct voluta_sb_info *sbi,
			    const struct voluta_vnode_info *itn_vi)
{
	struct voluta_itable *itable = itable_of(sbi);

	itable_parse_itn(itable, itn_vi->u.itn);
}

static int scan_subtree_at(struct voluta_sb_info *sbi,
			   const struct voluta_vaddr *vaddr)
{
	int err;
	struct voluta_vnode_info *itn_vi;

	if (vaddr_isnull(vaddr)) {
		return 0;
	}
	err = stage_itnode(sbi, vaddr, &itn_vi);
	if (err) {
		return err;
	}
	err = scan_subtree(sbi, itn_vi);
	if (err) {
		return err;
	}
	return 0;
}

static int do_scan_subtree(struct voluta_sb_info *sbi,
			   const struct voluta_vnode_info *itn_vi)
{
	int err = 0;
	struct voluta_vaddr vaddr;
	const size_t nchilds_max = itn_nchilds_max(itn_vi->u.itn);

	scan_entries_of(sbi, itn_vi);
	for (size_t i = 0; (i < nchilds_max) && !err; ++i) {
		resolve_child_at(itn_vi, i, &vaddr);
		err = scan_subtree_at(sbi, &vaddr);
	}
	return err;
}

static int scan_subtree(struct voluta_sb_info *sbi,
			struct voluta_vnode_info *itn_vi)
{
	int err;

	v_incref(itn_vi);
	err = do_scan_subtree(sbi, itn_vi);
	v_decref(itn_vi);

	return err;
}

int voluta_reload_itable(struct voluta_sb_info *sbi,
			 const struct voluta_vaddr *vaddr)
{
	int err;
	struct voluta_vnode_info *itroot_vi;

	err = reload_itable_root(sbi, vaddr);
	if (err) {
		return err;
	}
	err = stage_itroot(sbi, &itroot_vi);
	if (err) {
		return err;
	}
	err = scan_subtree(sbi, itroot_vi);
	if (err) {
		return err;
	}
	return 0;
}

static bool ino_set_isfull(const struct voluta_ino_set *ino_set)
{
	return (ino_set->cnt >= ARRAY_SIZE(ino_set->ino));
}

static void ino_set_append(struct voluta_ino_set *ino_set, ino_t ino)
{
	ino_set->ino[ino_set->cnt++] = ino;
}

static void fill_ino_set(const struct voluta_vnode_info *itn_vi,
			 struct voluta_ino_set *ino_set)
{
	const struct voluta_itable_entry *ite;
	const struct voluta_itable_tnode *itn = itn_vi->u.itn;

	ino_set->cnt = 0;
	ite = itn_find_next(itn, NULL);
	while (ite != NULL) {
		if (ino_set_isfull(ino_set)) {
			break;
		}
		ino_set_append(ino_set, ite_ino(ite));
		ite = itn_find_next(itn, ite + 1);
	}
}

int voluta_parse_itable_top(struct voluta_sb_info *sbi,
			    struct voluta_ino_set *ino_set)
{
	int err;
	struct voluta_vnode_info *vi;
	struct voluta_itable *itable = itable_of(sbi);

	err = stage_itnode(sbi, &itable->it_root_vaddr, &vi);
	if (err) {
		return err;
	}
	fill_ino_set(vi, ino_set);
	return 0;
}

void voluta_bind_root_ino(struct voluta_sb_info *sbi, ino_t ino)
{
	struct voluta_itable *itable = itable_of(sbi);

	itable->it_ino_rootd = ino;
	if (itable->it_ino_apex < itable->it_ino_rootd) {
		itable->it_ino_apex = itable->it_ino_rootd;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_init_itable(struct voluta_sb_info *sbi)
{
	itable_init(&sbi->s_itable);
	return 0;
}

void voluta_fini_itable(struct voluta_sb_info *sbi)
{
	itable_fini(&sbi->s_itable);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int verify_itable_entry(const struct voluta_itable_entry *ite)
{
	return voluta_verify_off(ite_off(ite));
}

static int verify_count(size_t count, size_t expected)
{
	return (count == expected) ? 0 : -EFSCORRUPTED;
}

static int verify_itnode_entries(const struct voluta_itable_tnode *itn)
{
	int err;
	size_t count = 0;
	const struct voluta_itable_entry *ite;
	const size_t nents_max = itn_nents_max(itn);

	for (size_t i = 0; (i < nents_max); ++i) {
		ite = itn_entry_at(itn, i);
		err = verify_itable_entry(ite);
		if (err) {
			return err;
		}
		if (!off_isnull(ite_off(ite))) {
			count++;
		}
	}
	return verify_count(count, itn_nents(itn));
}

static int verify_itnode_childs(const struct voluta_itable_tnode *itn)
{
	int err;
	loff_t off;
	size_t nchilds = 0;
	const size_t nchilds_max = itn_nchilds_max(itn);

	for (size_t slot = 0; slot < nchilds_max; ++slot) {
		off = itn_child_at(itn, slot);
		err = voluta_verify_off(off);
		if (err) {
			return err;
		}
		if (!off_isnull(off)) {
			nchilds++;
		}
	}
	return verify_count(nchilds, itn_nchilds(itn));
}

int voluta_verify_itnode(const struct voluta_itable_tnode *itn, bool full)
{
	int err;

	err = voluta_verify_off(itn_parent_off(itn));
	if (err || !full) {
		return err;
	}
	err = verify_itnode_entries(itn);
	if (err) {
		return err;
	}
	err = verify_itnode_childs(itn);
	if (err) {
		return err;
	}
	return 0;
}
