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
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include "voluta-lib.h"

#define ITNODE_ROOT_DEPTH 1

/* TODO: Move to common */
static const struct voluta_vaddr *
vaddr_of_vi(const struct voluta_vnode_info *vi)
{
	return &vi->vaddr;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool itable_entry_isfree(const struct voluta_itable_entry *ite)
{
	return (ite->ino == VOLUTA_INO_NULL);
}

static bool
itable_entry_has_ino(const struct voluta_itable_entry *ite, ino_t ino)
{
	return (ite->ino == ino);
}

static void itable_entry_to_iaddr(const struct voluta_itable_entry *ite,
				  struct voluta_iaddr *iaddr)
{
	voluta_iaddr_setup2(iaddr, ite->ino, ite->off, ite->len);
}

static void itable_entry_setup(struct voluta_itable_entry *ite,
			       const struct voluta_iaddr *iaddr)
{
	ite->ino = iaddr->ino;
	ite->off = iaddr->vaddr.off;
	ite->len = (uint16_t)iaddr->vaddr.len;
	ite->pad = 0;
}

static void itable_entry_reset(struct voluta_itable_entry *ite)
{
	ite->ino = VOLUTA_INO_NULL;
	ite->off = VOLUTA_OFF_NULL;
	ite->len = 0;
	ite->pad = 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
itable_tnode_init(struct voluta_itable_tnode *itn,
		  const struct voluta_vaddr *parent_vaddr, size_t depth)
{
	itn->it_parent_off =
		parent_vaddr ? parent_vaddr->off : VOLUTA_OFF_NULL;

	itn->it_depth = (uint16_t)depth;
	itn->it_nitents = 0;
	itn->it_nchilds = 0;

	for (size_t i = 0; i < ARRAY_SIZE(itn->it_child_off); ++i) {
		itn->it_child_off[i] = VOLUTA_OFF_NULL;
	}
	for (size_t j = 0; j < ARRAY_SIZE(itn->ite); ++j) {
		itable_entry_reset(&itn->ite[j]);
	}
}

static size_t itable_tnode_depth(const struct voluta_itable_tnode *itn)
{
	return itn->it_depth;
}

static struct voluta_itable_entry *
itable_tnode_at(const struct voluta_itable_tnode *itn, size_t pos)
{
	const struct voluta_itable_entry *ite = &itn->ite[pos];

	return (struct voluta_itable_entry *)ite;
}

static bool itable_tnode_isfull(const struct voluta_itable_tnode *itn)
{
	return (itn->it_nitents == ARRAY_SIZE(itn->ite));
}

static bool itable_tnode_isempty(const struct voluta_itable_tnode *itn)
{
	return (itn->it_nitents == 0);
}

static const struct voluta_itable_entry *
itable_tnode_find_first(const struct voluta_itable_tnode *itn)
{
	const struct voluta_itable_entry *ite;
	const size_t nents = ARRAY_SIZE(itn->ite);

	if (itable_tnode_isempty(itn)) {
		return NULL;
	}
	for (size_t i = 0; i < nents; ++i) {
		ite = itable_tnode_at(itn, i);
		if (!itable_entry_isfree(ite)) {
			return ite;
		}
	}
	return NULL;
}

static struct voluta_itable_entry *
itable_tnode_lookup(const struct voluta_itable_tnode *itn, ino_t ino)
{
	const struct voluta_itable_entry *ite;
	const size_t nents = ARRAY_SIZE(itn->ite);

	if (itable_tnode_isempty(itn)) {
		return NULL;
	}
	for (size_t i = 0; i < nents; ++i) {
		ite = itable_tnode_at(itn, (ino + i) % nents);
		if (itable_entry_has_ino(ite, ino)) {
			return (struct voluta_itable_entry *)ite;
		}
	}
	return NULL;
}

static struct voluta_itable_entry *
itable_tnode_insert(struct voluta_itable_tnode *itn,
		    const struct voluta_iaddr *iaddr)
{
	struct voluta_itable_entry *ite;
	const size_t nents = ARRAY_SIZE(itn->ite);

	if (itable_tnode_isfull(itn)) {
		return NULL;
	}
	for (size_t i = 0; i < nents; ++i) {
		ite = itable_tnode_at(itn, (iaddr->ino + i) % nents);
		if (itable_entry_isfree(ite)) {
			itable_entry_setup(ite, iaddr);
			itn->it_nitents++;
			return ite;
		}
	}
	return NULL;
}

static struct voluta_itable_entry *
itable_tnode_remove(struct voluta_itable_tnode *itn, ino_t ino)
{
	struct voluta_itable_entry *ite;

	ite = itable_tnode_lookup(itn, ino);
	if (ite) {
		itable_entry_reset(ite);
		itn->it_nitents--;
	}
	return ite;
}

static size_t
itable_tnode_child_slot(const struct voluta_itable_tnode *itn, ino_t ino)
{
	size_t slot, depth, shift;
	const size_t nchilds_max = ARRAY_SIZE(itn->it_child_off);

	depth = itable_tnode_depth(itn);
	shift = VOLUTA_ITNODE_SHIFT * (depth - 1);
	slot = (ino >> shift) % nchilds_max;
	return slot;
}

static void itable_tnode_get_child(const struct voluta_itable_tnode *itn,
				   ino_t ino, struct voluta_vaddr *out_vaddr)
{
	loff_t off;
	const size_t slot = itable_tnode_child_slot(itn, ino);

	off = itn->it_child_off[slot];
	if (!off_isnull(off)) {
		voluta_vaddr_by_off(out_vaddr, VOLUTA_VTYPE_ITNODE, off);
	} else {
		voluta_vaddr_reset(out_vaddr);
	}
}

static void itable_tnode_set_child(struct voluta_itable_tnode *itn,
				   ino_t ino, const struct voluta_vaddr *vaddr)
{
	const size_t slot = itable_tnode_child_slot(itn, ino);

	voluta_assert(off_isnull(itn->it_child_off[slot]));
	voluta_assert_lt(itn->it_nchilds, ARRAY_SIZE(itn->it_child_off));

	itn->it_child_off[slot] = vaddr->off;
	itn->it_nchilds++;
}

static void itable_tnode_clear_child(struct voluta_itable_tnode *itn, ino_t ino)
{
	const size_t slot = itable_tnode_child_slot(itn, ino);

	voluta_assert(!off_isnull(itn->it_child_off[slot]));
	voluta_assert_gt(itn->it_nchilds, 0);

	itn->it_child_off[slot] = VOLUTA_OFF_NULL;
	itn->it_nchilds--;
}

static bool itable_tnode_isleaf(const struct voluta_itable_tnode *itn)
{
	return (itn->it_nchilds == 0);
}

static bool itable_tnode_isroot(const struct voluta_itable_tnode *itn)
{
	return (itn->it_depth == ITNODE_ROOT_DEPTH);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_itable_init(struct voluta_itable *itable)
{
	voluta_vaddr_reset(&itable->it_tree_vaddr);
	itable->it_ino_rootd = VOLUTA_INO_NULL;
	itable->it_ino_apex = VOLUTA_INO_ROOT + 1;
	itable->it_ninodes = 0;
	itable->it_ninodes_max = 1UL << 31; /* XXX */
}

void voluta_itable_fini(struct voluta_itable *itable)
{
	voluta_vaddr_reset(&itable->it_tree_vaddr);
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

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_itable *itable_of(const struct voluta_sb_info *sbi)
{
	const struct voluta_itable *itable = &sbi->s_itable;

	return (struct voluta_itable *)itable;
}

static struct voluta_itable *itable_of1(const struct voluta_env *env)
{
	return itable_of(&env->sbi);
}

static int stage_itnode(struct voluta_env *env,
			const struct voluta_vaddr *vaddr,
			struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *itnode_vi;

	if (vaddr_isnull(vaddr)) {
		return -ENOENT;
	}
	err = voluta_stage_vnode(sbi_of(env), vaddr, &itnode_vi);
	if (err) {
		return err;
	}
	*out_vi = itnode_vi;
	return 0;
}

static int stage_child(struct voluta_env *env,
		       const struct voluta_vnode_info *vi,
		       ino_t ino, struct voluta_vnode_info **out_child_vi)
{
	struct voluta_vaddr child_vaddr;

	itable_tnode_get_child(vi->u.itn, ino, &child_vaddr);
	return stage_itnode(env, &child_vaddr, out_child_vi);
}

static int stage_itable_root(struct voluta_env *env,
			     struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi;
	struct voluta_itable *it = itable_of1(env);

	err = stage_itnode(env, &it->it_tree_vaddr, &vi);
	if (err) {
		return err;
	}
	voluta_assert_eq(itable_tnode_depth(vi->u.itn), ITNODE_ROOT_DEPTH);
	*out_vi = vi;
	return 0;
}

static int resolve_iaddr(const struct voluta_itable_entry *itent,
			 struct voluta_iaddr *out_iaddr)
{
	if (itent == NULL) {
		return -ENOENT;
	}
	itable_entry_to_iaddr(itent, out_iaddr);
	return 0;
}

static int lookup_at(const struct voluta_vnode_info *vi,
		     ino_t ino, struct voluta_iaddr *out_iaddr)
{
	const struct voluta_itable_entry *ite;

	ite = itable_tnode_lookup(vi->u.itn, ino);
	return resolve_iaddr(ite, out_iaddr);
}

static int lookup_recursive(struct voluta_env *env,
			    const struct voluta_vnode_info *vi,
			    ino_t ino, struct voluta_iaddr *out_iaddr)
{
	int err;
	struct voluta_vnode_info *child_vi;

	err = lookup_at(vi, ino, out_iaddr);
	if (!err) {
		return 0;
	}
	err = stage_child(env, vi, ino, &child_vi);
	if (err) {
		return err;
	}
	err = lookup_recursive(env, child_vi, ino, out_iaddr);
	if (err) {
		return err;
	}
	return 0;
}

static int lookup_iref(struct voluta_env *env, ino_t ino,
		       struct voluta_iaddr *out_iaddr)
{
	int err;
	struct voluta_vnode_info *vi = NULL;

	err = stage_itable_root(env, &vi);
	if (err) {
		return err;
	}
	err = lookup_recursive(env, vi, ino, out_iaddr);
	if (err) {
		return err;
	}
	return 0;
}

static int find_first_at(const struct voluta_vnode_info *vi,
			 struct voluta_iaddr *out_iaddr)
{
	const struct voluta_itable_entry *ite =
		itable_tnode_find_first(vi->u.itn);

	return resolve_iaddr(ite, out_iaddr);
}

static int new_itnode(struct voluta_env *env, const struct voluta_vaddr *vaddr,
		      size_t depth, struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi;

	err = voluta_new_vnode(sbi_of(env), VOLUTA_VTYPE_ITNODE, &vi);
	if (err) {
		return err;
	}
	itable_tnode_init(vi->u.itn, vaddr, depth);
	v_dirtify(vi);
	*out_vi = vi;
	return 0;
}

static int del_itnode(struct voluta_env *env,
		      struct voluta_vnode_info *itnode_vi)
{
	voluta_assert_eq(itnode_vi->bki->b_vaddr.vtype, VOLUTA_VTYPE_ITNODE);

	v_undirtify(itnode_vi);
	return voluta_del_vnode(env, itnode_vi);
}

static int create_itable_root(struct voluta_env *env,
			      struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *itnode_vi;
	struct voluta_itable *itable = itable_of1(env);

	err = new_itnode(env, NULL, ITNODE_ROOT_DEPTH, &itnode_vi);
	if (err) {
		return err;
	}
	vaddr_clone(vaddr_of_vi(itnode_vi), &itable->it_tree_vaddr);
	*out_vi = itnode_vi;
	return 0;
}

static int create_child(struct voluta_env *env, struct voluta_vnode_info *vi,
			ino_t ino, struct voluta_vnode_info **out_child_vi)
{
	int err;
	size_t depth, depth_max = 16;
	struct voluta_vnode_info *child_vi;

	depth = itable_tnode_depth(vi->u.itn);
	if (depth >= depth_max) {
		return -ENOSPC;
	}
	err = new_itnode(env, vaddr_of_vi(vi), depth + 1, &child_vi);
	if (err) {
		return err;
	}
	itable_tnode_set_child(vi->u.itn, ino, vaddr_of_vi(child_vi));
	v_dirtify(vi);
	*out_child_vi = child_vi;
	return 0;
}

static int stage_or_create_itable_root(struct voluta_env *env,
				       struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *it_root_vi = NULL;
	const struct voluta_itable *itable = itable_of1(env);

	if (!vaddr_isnull(&itable->it_tree_vaddr)) {
		err = stage_itable_root(env, &it_root_vi);
	} else {
		err = create_itable_root(env, &it_root_vi);
	}
	if (err) {
		return err;
	}

	voluta_assert_eq(it_root_vi->vaddr.ag_index, 1);
	*out_vi = it_root_vi;
	return 0;
}

static int require_child(struct voluta_env *env, struct voluta_vnode_info *vi,
			 ino_t ino, struct voluta_vnode_info **out_child_vi)
{
	int err;
	struct voluta_vaddr child_vaddr;

	itable_tnode_get_child(vi->u.itn, ino, &child_vaddr);
	if (!vaddr_isnull(&child_vaddr)) {
		err = stage_itnode(env, &child_vaddr, out_child_vi);
	} else {
		err = create_child(env, vi, ino, out_child_vi);
	}
	return err;
}

static int try_insert_at(struct voluta_env *env,
			 struct voluta_vnode_info *vi,
			 const struct voluta_iaddr *iaddr)
{
	struct voluta_itable_entry *itent;
	struct voluta_itable *itable = itable_of1(env);

	itent = itable_tnode_insert(vi->u.itn, iaddr);
	if (itent == NULL) {
		return -ENOSPC;
	}
	itable_add_ino(itable, iaddr->ino);
	v_dirtify(vi);
	return 0;
}

static int insert_recursive(struct voluta_env *env,
			    struct voluta_vnode_info *itnode_vi,
			    const struct voluta_iaddr *iaddr)
{
	int err;
	struct voluta_vnode_info *child_vi = NULL;

	err = try_insert_at(env, itnode_vi, iaddr);
	if (!err) {
		return 0;
	}
	err = require_child(env, itnode_vi, iaddr->ino, &child_vi);
	if (err) {
		return err;
	}
	err = insert_recursive(env, child_vi, iaddr);
	if (err) {
		return err;
	}
	return 0;
}

static int insert_iref(struct voluta_env *env, const struct voluta_iaddr *iaddr)
{
	int err;
	struct voluta_vnode_info *root_itnode_vi;

	err = stage_or_create_itable_root(env, &root_itnode_vi);
	if (err) {
		return err;
	}
	err = insert_recursive(env, root_itnode_vi, iaddr);
	if (err) {
		return err;
	}
	return 0;
}

static int try_remove_at(struct voluta_env *env,
			 struct voluta_vnode_info *vi, ino_t ino)
{
	struct voluta_itable_entry *ite;

	ite = itable_tnode_remove(vi->u.itn, ino);
	if (ite == NULL) {
		return -ENOENT;
	}
	itable_remove_ino(itable_of1(env), ino);
	v_dirtify(vi);
	return 0;
}

static int prune_leaf_of(struct voluta_env *env,
			 struct voluta_vnode_info *parent_vi,
			 struct voluta_vnode_info *child_vi, ino_t ino)
{
	int err;

	if (!itable_tnode_isempty(child_vi->u.itn)) {
		return 0;
	}
	if (!itable_tnode_isleaf(child_vi->u.itn)) {
		return 0;
	}
	err = del_itnode(env, child_vi);
	if (err) {
		return err;
	}
	itable_tnode_clear_child(parent_vi->u.itn, ino);
	v_dirtify(parent_vi);
	return 0;
}

static int remove_recursive(struct voluta_env *env,
			    struct voluta_vnode_info *itnode_vi, ino_t ino)
{
	int err;
	struct voluta_vnode_info *child_vi = NULL;

	err = try_remove_at(env, itnode_vi, ino);
	if (!err) {
		return 0;
	}
	err = stage_child(env, itnode_vi, ino, &child_vi);
	if (err) {
		return err;
	}
	err = remove_recursive(env, child_vi, ino);
	if (err) {
		return err;
	}
	err = prune_leaf_of(env, itnode_vi, child_vi, ino);
	if (err) {
		return err;
	}
	return 0;
}

static int remove_iref(struct voluta_env *env, ino_t ino)
{
	int err;
	struct voluta_vnode_info *root_itnode_vi;

	err = stage_itable_root(env, &root_itnode_vi);
	if (err) {
		return err;
	}
	err = remove_recursive(env, root_itnode_vi, ino);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_acquire_ino(struct voluta_env *env, struct voluta_iaddr *iaddr)
{
	int err;
	struct voluta_itable *itable = itable_of1(env);

	err = itable_next_ino(itable, &iaddr->ino);
	if (err) {
		return err;
	}
	err = insert_iref(env, iaddr);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_real_ino(const struct voluta_env *env, ino_t ino, ino_t *out_ino)
{
	const struct voluta_itable *itable = itable_of1(env);

	if ((ino < VOLUTA_INO_ROOT) || (ino > VOLUTA_INO_MAX)) {
		return -EINVAL;
	}
	if (ino == VOLUTA_INO_ROOT) {
		if (itable->it_ino_rootd == VOLUTA_INO_NULL) {
			return -ENOENT;
		}
		*out_ino = itable->it_ino_rootd;
	} else {
		*out_ino = ino;
	}
	return 0;
}

int voluta_discard_ino(struct voluta_env *env, ino_t xino)
{
	int err;
	ino_t ino;

	err = voluta_real_ino(env, xino, &ino);
	if (err) {
		return err;
	}
	err = remove_iref(env, ino);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_resolve_ino(struct voluta_env *env, ino_t xino,
		       struct voluta_iaddr *out_iaddr)
{
	int err;
	ino_t ino;

	err = voluta_real_ino(env, xino, &ino);
	if (err) {
		return err;
	}
	err = lookup_iref(env, ino, out_iaddr);
	if (err) {
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool has_itnode_at(const struct voluta_bk_info *uber_bki,
			  const struct voluta_vaddr *vaddr)
{
	enum voluta_vtype vtype;

	vtype = voluta_vtype_at(uber_bki, vaddr);
	return (vtype == VOLUTA_VTYPE_ITNODE);
}

static int stage_itnode_at(struct voluta_env *env,
			   const struct voluta_vaddr *vaddr,
			   struct voluta_vnode_info **out_itnode_vi)
{
	int err;
	struct voluta_bk_info *uber_bki;

	err = voluta_stage_uber_of(&env->sbi, vaddr, &uber_bki);
	if (err) {
		return err;
	}
	if (!has_itnode_at(uber_bki, vaddr)) {
		return -ENOENT;
	}
	err = stage_itnode(env, vaddr, out_itnode_vi);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_reload_itable(struct voluta_env *env,
			 const struct voluta_vaddr *vaddr)
{
	int err;
	struct voluta_vnode_info *root_vi;
	struct voluta_itable *itable = itable_of1(env);

	err = stage_itnode_at(env, vaddr, &root_vi);
	if (err) {
		return err;
	}
	if (!itable_tnode_isroot(root_vi->u.itn)) {
		return -ENOENT;
	}
	voluta_vaddr_clone(vaddr, &itable->it_tree_vaddr);

	/* XXX: Hack, FIXME */
	voluta_assert_eq(itable->it_ninodes, 0);
	itable->it_ninodes = 1;

	return 0;
}

void voluta_bind_root_ino(struct voluta_env *env,
			  const struct voluta_inode_info *root_ii)
{
	struct voluta_itable *itable = itable_of1(env);

	itable->it_ino_rootd = i_ino_of(root_ii);

	if (itable->it_ino_apex < itable->it_ino_rootd) {
		itable->it_ino_apex = itable->it_ino_rootd;
	}
}

int voluta_find_root_ino(struct voluta_env *env, struct voluta_iaddr *out_iaddr)
{
	int err;
	struct voluta_vnode_info *vi;
	struct voluta_itable *itable = itable_of1(env);

	voluta_assert(!vaddr_isnull(&itable->it_tree_vaddr));
	voluta_assert_eq(itable->it_ino_rootd, VOLUTA_INO_NULL);

	err = stage_itable_root(env, &vi);
	if (err) {
		return err;
	}
	/* TODO: Find root ino by explicit flag */
	err = find_first_at(vi, out_iaddr);
	if (err) {
		return err;
	}
	voluta_assert_ne(out_iaddr->ino, VOLUTA_INO_NULL);
	return 0;
}

int voluta_format_itable(struct voluta_env *env)
{
	int err;
	struct voluta_vnode_info *vi;
	struct voluta_itable *itable = itable_of1(env);

	voluta_assert(vaddr_isnull(&itable->it_tree_vaddr));

	err = stage_or_create_itable_root(env, &vi);
	if (err) {
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int verify_itent(const struct voluta_itable_entry *itent)
{
	int err;

	err = voluta_verify_off(itent->off);
	if (err) {
		return err;
	}
	if (itent->len > VOLUTA_BK_SIZE) {
		return -EFSCORRUPTED;
	}
	return 0;
}

static int verify_itnode_entries(const struct voluta_itable_tnode *itnode)
{
	int err;
	size_t count = 0;
	const struct voluta_itable_entry *itent;
	const size_t nite = ARRAY_SIZE(itnode->ite);

	for (size_t i = 0; (i < nite); ++i) {
		itent = &itnode->ite[i];
		err = verify_itent(itent);
		if (err) {
			return err;
		}
		if (!off_isnull(itent->off)) {
			count++;
		}
	}
	return voluta_verify_count(count, itnode->it_nitents);
}

static int verify_itnode_childs(const struct voluta_itable_tnode *itnode)
{
	int err;
	loff_t off;
	size_t nchilds = 0;
	const size_t nchilds_max = ARRAY_SIZE(itnode->it_child_off);

	for (size_t i = 0; i < nchilds_max; ++i) {
		off = itnode->it_child_off[i];
		err = voluta_verify_off(off);
		if (err) {
			return err;
		}
		if (!off_isnull(off)) {
			nchilds++;
		}
	}
	return voluta_verify_count(nchilds, itnode->it_nchilds);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_verify_itnode(const struct voluta_itable_tnode *itnode)
{
	int err;

	err = voluta_verify_off(itnode->it_parent_off);
	if (err) {
		return err;
	}
	err = verify_itnode_entries(itnode);
	if (err) {
		return err;
	}
	err = verify_itnode_childs(itnode);
	if (err) {
		return err;
	}
	return 0;
}
