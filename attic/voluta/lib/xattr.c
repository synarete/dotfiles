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
#include <sys/types.h>
#include <sys/xattr.h>
#include <linux/xattr.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "voluta-lib.h"


#define XATTR_DATA_MAX \
	(VOLUTA_NAME_MAX + 1 + VOLUTA_XATTR_VALUE_MAX)

#define XATTRF_DISABLE 1

#define MKPREFIX(p_, n_, f_) \
	{ .prefix = p_, .ns = n_, .flags = f_ }


struct voluta_xentry_view {
	struct voluta_xattr_entry xe;
	uint8_t  xe_data[XATTR_DATA_MAX];
} voluta_packed;


struct voluta_xattr_prefix {
	const char *prefix;
	enum voluta_xattr_ns ns;
	int flags;
};


/*
 * TODO: For well-known xattr prefix, do not store the prefix-part as string
 * but as 'enum voluta_xattr_ns' value within 'xe_namespace'.
 */
static const struct voluta_xattr_prefix s_xattr_prefix[] = {
	MKPREFIX(XATTR_SECURITY_PREFIX, VOLUTA_XATTR_SECURITY, 0),
	MKPREFIX(XATTR_SYSTEM_PREFIX, VOLUTA_XATTR_SYSTEM, XATTRF_DISABLE),
	MKPREFIX(XATTR_TRUSTED_PREFIX, VOLUTA_XATTR_TRUSTED, 0),
	MKPREFIX(XATTR_USER_PREFIX, VOLUTA_XATTR_USER, 0),
};

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t div_round_up(size_t n, size_t d)
{
	return (n + d - 1) / d;
}

static struct voluta_xentry_view *
xentry_view_of(const struct voluta_xattr_entry *xe)
{
	return voluta_container_of(xe, struct voluta_xentry_view, xe);
}

static struct voluta_xattr_entry *
xattr_entry_unconst(const struct voluta_xattr_entry *xe)
{
	return (struct voluta_xattr_entry *)xe;
}

static bool xattr_entry_isactive(const struct voluta_xattr_entry *xe)
{
	return (xe->xe_name_len > 0) && (xe->xe_nentries > 0);
}

static size_t xe_aligned_size(size_t size)
{
	const size_t align = sizeof(struct voluta_xattr_entry);

	return align * div_round_up(size, align);
}

static char *xattr_entry_name(const struct voluta_xattr_entry *xe)
{
	struct voluta_xentry_view *xeview = xentry_view_of(xe);

	return (char *)xeview->xe_data;
}

static void *xattr_entry_value(const struct voluta_xattr_entry *xe)
{
	struct voluta_xentry_view *xeview = xentry_view_of(xe);

	return xeview->xe_data + xe_aligned_size(xe->xe_name_len);
}

static size_t xattr_entry_value_size(const struct voluta_xattr_entry *xe)
{
	return xe->xe_value_size;
}

static bool xattr_entry_has_name(const struct voluta_xattr_entry *xe,
				 const struct voluta_str *name)
{
	int cmp;
	const char *xe_name;

	if (!xattr_entry_isactive(xe)) {
		return false;
	}
	if (name->len != xe->xe_name_len) {
		return false;
	}
	xe_name = xattr_entry_name(xe);
	cmp = memcmp(xe_name, name->str, name->len);
	return (cmp == 0);
}

static size_t xe_nbytes_to_nents(size_t nbytes)
{
	return div_round_up(nbytes, sizeof(struct voluta_xattr_entry));
}

static size_t xe_name_value_to_nents(const struct voluta_str *name,
				     const struct voluta_buf *value)
{
	const size_t name_len = xe_aligned_size(name->len);
	const size_t value_sz = xe_aligned_size(value->len);

	return 1 + xe_nbytes_to_nents(name_len + value_sz);
}

static size_t xattr_entry_nents_used(const struct voluta_xattr_entry *xe)
{
	return xe->xe_nentries;
}

static struct voluta_xattr_entry *
xattr_entry_next(const struct voluta_xattr_entry *xe)
{
	const size_t step = xattr_entry_isactive(xe) ?
			    xattr_entry_nents_used(xe) : 1;

	return (struct voluta_xattr_entry *)(xe + step);
}

static void xattr_entry_assign(struct voluta_xattr_entry *xe,
			       const struct voluta_str *name,
			       const struct voluta_buf *value)
{
	const size_t nents = xe_name_value_to_nents(name, value);

	xe->xe_name_len = (uint16_t)name->len;
	xe->xe_value_size = (uint16_t)value->len;
	xe->xe_nentries = (uint16_t)nents;
	memcpy(xattr_entry_name(xe), name->str, name->len);
	memcpy(xattr_entry_value(xe), value->ptr, value->len);
}

static void xattr_entry_reset(struct voluta_xattr_entry *xe, size_t cnt)
{
	memset(xe, 0, cnt * sizeof(*xe));
}

static void xattr_entry_clear(struct voluta_xattr_entry *xe)
{
	xattr_entry_reset(xe, xattr_entry_nents_used(xe));
}

static void xattr_entry_copy_value(const struct voluta_xattr_entry *xe,
				   struct voluta_buf *buf)
{
	const size_t value_sz = xattr_entry_value_size(xe);
	const size_t size = voluta_min(value_sz, buf->len);

	memcpy(buf->ptr, xattr_entry_value(xe), size);
	buf->len = size;
}

static size_t
xattr_entry_count_nfree(const struct voluta_xattr_entry *itr,
			const struct voluta_xattr_entry *end)
{
	size_t count = 0;

	while (!xattr_entry_isactive(itr) && (itr++ < end)) {
		count++;
	}
	return count;
}

static struct voluta_xattr_entry *
xattr_entry_search(const struct voluta_xattr_entry *itr,
		   const struct voluta_xattr_entry *end,
		   const struct voluta_str *name)
{
	while (itr < end) {
		if (xattr_entry_has_name(itr, name)) {
			return (struct voluta_xattr_entry *)itr;
		}
		itr = xattr_entry_next(itr);
	}
	return NULL;
}

static struct voluta_xattr_entry *
xattr_entry_insert(struct voluta_xattr_entry *xe,
		   const struct voluta_xattr_entry *end,
		   const struct voluta_str *name,
		   const struct voluta_buf *value)
{
	size_t count, nents;
	struct voluta_xattr_entry *itr = xe;

	nents = xe_name_value_to_nents(name, value);
	while (itr < end) {
		count = xattr_entry_count_nfree(itr, end);
		if (count >= nents) {
			xattr_entry_assign(itr, name, value);
			return itr;
		} else if (count == 0) {
			count = xattr_entry_nents_used(itr);
		}
		itr += count;
	}
	return NULL;
}

static int xattr_entry_verify(const struct voluta_xattr_entry *xe,
			      const struct voluta_xattr_entry *end)
{
	size_t count, ntail;
	const struct voluta_xattr_entry *itr = xe;

	while (itr < end) {
		count = xattr_entry_count_nfree(itr, end);
		if (count == 0) {
			count = xattr_entry_nents_used(itr);
			ntail = (size_t)(end - itr);
			if (count > ntail) {
				return -EFSCORRUPTED;
			}
		}
		itr += count;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_xattr_entry *
inode_xe_begin(const struct voluta_inode *inode)
{
	return (struct voluta_xattr_entry *)inode->i_xe;
}

static const struct voluta_xattr_entry *
inode_xe_end(const struct voluta_inode *inode)
{
	return inode->i_xe + ARRAY_SIZE(inode->i_xe);
}

static struct voluta_xattr_entry *
inode_find_xentry(const struct voluta_inode *inode,
		  const struct voluta_str *str)
{
	const struct voluta_xattr_entry *beg = inode_xe_begin(inode);
	const struct voluta_xattr_entry *end = inode_xe_end(inode);

	return xattr_entry_search(beg, end, str);
}

static struct voluta_xattr_entry *
inode_insert_xentry(struct voluta_inode *inode,
		    const struct voluta_str *name,
		    const struct voluta_buf *value)
{
	struct voluta_xattr_entry *beg = inode_xe_begin(inode);
	const struct voluta_xattr_entry *end = inode_xe_end(inode);

	return xattr_entry_insert(beg, end, name, value);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_xattr_entry *
xattr_node_begin(const struct voluta_xattr_node *xan)
{
	return (struct voluta_xattr_entry *)xan->xe;
}

static const struct voluta_xattr_entry *
xattr_node_end(const struct voluta_xattr_node *xan)
{
	return xan->xe + ARRAY_SIZE(xan->xe);
}

static struct voluta_xattr_entry *
xattr_node_search(const struct voluta_xattr_node *xan,
		  const struct voluta_str *str)
{
	const struct voluta_xattr_entry *beg = xattr_node_begin(xan);
	const struct voluta_xattr_entry *end = xattr_node_end(xan);

	return xattr_entry_search(beg, end, str);
}

static struct voluta_xattr_entry *
xattr_node_insert(struct voluta_xattr_node *xan,
		  const struct voluta_str *name, const struct voluta_buf *value)
{
	struct voluta_xattr_entry *beg = xattr_node_begin(xan);
	const struct voluta_xattr_entry *end = xattr_node_end(xan);

	return xattr_entry_insert(beg, end, name, value);
}

static int xaddr_node_verify(const struct voluta_xattr_node *xan)
{
	const struct voluta_xattr_entry *beg = xattr_node_begin(xan);
	const struct voluta_xattr_entry *end = xattr_node_end(xan);

	return xattr_entry_verify(beg, end);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static ino_t xattr_ino_of(const struct voluta_vnode_info *vi)
{
	return vi->u.xan->xa_ino;
}

static void setup_xattr_node(struct voluta_vnode_info *vi, ino_t ino)
{
	struct voluta_xattr_node *xan = vi->u.xan;

	xan->xa_flags = 0;
	xan->xa_ino = ino;
	xattr_entry_reset(xan->xe, ARRAY_SIZE(xan->xe));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t xattr_nslots_max(const struct voluta_inode_info *ii)
{
	return ARRAY_SIZE(ii->inode->i_xattr_off);
}

static void xattr_unset_at(struct voluta_inode_info *ii, size_t sloti)
{
	struct voluta_inode *inode = ii->inode;

	inode->i_xattr_off[sloti] = VOLUTA_OFF_NULL;
}

static void xattr_get_at(const struct voluta_inode_info *ii, size_t sloti,
			 struct voluta_vaddr *out_vaddr)
{
	loff_t off;
	const struct voluta_inode *inode = ii->inode;

	off = inode->i_xattr_off[sloti];
	if (off_isnull(off)) {
		voluta_vaddr_reset(out_vaddr);
	} else {
		voluta_vaddr_by_off(out_vaddr, VOLUTA_VTYPE_XANODE, off);
	}
}

static void xattr_set_at(const struct voluta_inode_info *ii, size_t sloti,
			 const struct voluta_vaddr *vaddr)
{
	struct voluta_inode *inode = ii->inode;

	inode->i_xattr_off[sloti] = vaddr->off;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct voluta_xattr_ctx {
	struct voluta_env *env;
	struct voluta_sb_info *sbi;
	struct voluta_listxattr_ctx *listxattr_ctx;
	struct voluta_inode_info *ii;
	const struct voluta_str *name;
	struct voluta_buf value;
	size_t size;
	int flags;
	bool keep_iter;
};


struct voluta_xentry_info {
	struct voluta_vnode_info *xa_vi;
	struct voluta_xattr_entry *xentry;
};


static int stage_xattr_node(const struct voluta_xattr_ctx *xattr_ctx,
			    const struct voluta_vaddr *vaddr,
			    struct voluta_vnode_info **out_vi)
{
	int err;
	ino_t ino, xattr_ino;
	struct voluta_vnode_info *vi;

	err = voluta_stage_vnode(xattr_ctx->sbi, vaddr, &vi);
	if (err) {
		return err;
	}
	ino = i_ino_of(xattr_ctx->ii);
	xattr_ino = xattr_ino_of(vi);
	voluta_assert_eq(ino, xattr_ino);
	if (ino != xattr_ino) {
		return -EFSCORRUPTED;
	}
	*out_vi = vi;
	return 0;
}

static bool is_valid_xflags(int flags)
{
	return !flags || (flags == XATTR_CREATE) || (flags == XATTR_REPLACE);
}

static bool has_prefix(const struct voluta_xattr_prefix *xap,
		       const struct voluta_str *name)
{
	const size_t len = strlen(xap->prefix);

	return (name->len > len) && !strncmp(name->str, xap->prefix, len);
}

static const struct voluta_xattr_prefix *search_prefix(const struct voluta_str
		*name)
{
	const struct voluta_xattr_prefix *xap;

	for (size_t i = 0; i < ARRAY_SIZE(s_xattr_prefix); ++i) {
		xap = &s_xattr_prefix[i];
		if (has_prefix(xap, name)) {
			return xap;
		}
	}
	return NULL;
}

static int check_xattr_name(const struct voluta_str *name)
{
	const struct voluta_xattr_prefix *xap;

	if (!name) {
		return 0;
	}
	if (name->len > VOLUTA_NAME_MAX) {
		return -ENAMETOOLONG;
	}
	xap = search_prefix(name);
	if (xap && (xap->flags & XATTRF_DISABLE)) {
		return -EOPNOTSUPP;
	}
	return 0;
}

static int check_xattr(const struct voluta_xattr_ctx *xattr_ctx)
{
	int err;
	const struct voluta_inode_info *ii = xattr_ctx->ii;

	if (!i_isreg(ii) && !i_isdir(ii) && !i_islnk(ii)) {
		return -EINVAL;
	}
	err = check_xattr_name(xattr_ctx->name);
	if (err) {
		return err;
	}
	if (xattr_ctx->size > VOLUTA_XATTR_VALUE_MAX) {
		return -EINVAL;
	}
	if (!is_valid_xflags(xattr_ctx->flags)) {
		return -ENOTSUP;
	}
	return 0;
}

static int lookup_xentry_at(const struct voluta_xattr_ctx *xattr_ctx,
			    const struct voluta_vaddr *vaddr,
			    struct voluta_xentry_info *xei)
{
	int err;
	struct voluta_xattr_entry *xe;
	struct voluta_vnode_info *vi;

	if (vaddr_isnull(vaddr)) {
		return -ENOENT;
	}
	err = stage_xattr_node(xattr_ctx, vaddr, &vi);
	if (err) {
		return err;
	}
	xe = xattr_node_search(vi->u.xan, xattr_ctx->name);
	if (xe == NULL) {
		return -ENOENT;
	}
	xei->xa_vi = vi;
	xei->xentry = xe;
	return 0;
}

static int lookup_xentry_by_xanode(struct voluta_xattr_ctx *xattr_ctx,
				   struct voluta_xentry_info *xei)
{
	int err = -ENOENT;
	struct voluta_vaddr vaddr;
	const struct voluta_inode_info *ii = xattr_ctx->ii;

	for (size_t sloti = 0; sloti < xattr_nslots_max(ii); ++sloti) {
		xattr_get_at(ii, sloti, &vaddr);
		err = lookup_xentry_at(xattr_ctx, &vaddr, xei);
		if (err != -ENOENT) {
			break;
		}
	}
	return err;
}

static int lookup_xentry_by_inode(struct voluta_xattr_ctx *xattr_ctx,
				  struct voluta_xentry_info *xei)
{
	struct voluta_xattr_entry *xe;
	const struct voluta_inode_info *ii = xattr_ctx->ii;

	xe = inode_find_xentry(ii->inode, xattr_ctx->name);
	if (xe == NULL) {
		return -ENOENT;
	}
	xei->xa_vi = NULL;
	xei->xentry = xe;
	return 0;
}

static int lookup_xentry(struct voluta_xattr_ctx *xattr_ctx,
			 struct voluta_xentry_info *xei)
{
	int err = -ENOENT;
	const struct voluta_inode_info *ii = xattr_ctx->ii;

	if (!i_nxattr_of(ii)) {
		goto out;
	}
	err = lookup_xentry_by_inode(xattr_ctx, xei);
	if (err != -ENOENT) {
		goto out;
	}
	err = lookup_xentry_by_xanode(xattr_ctx, xei);
	if (err != -ENOENT) {
		goto out;
	}
out:
	return (err == -ENOENT) ? -ENOATTR : err;
}

static int do_getxattr(struct voluta_xattr_ctx *xattr_ctx, size_t *out_size)
{
	int err;
	struct voluta_buf *buf = &xattr_ctx->value;
	struct voluta_xentry_info xei = { .xentry = NULL };

	err = lookup_xentry(xattr_ctx, &xei);
	if (err) {
		return err;
	}
	*out_size = xattr_entry_value_size(xei.xentry);
	if (buf->len == 0) {
		return 0;
	}
	if (buf->len < *out_size) {
		return -ERANGE;
	}
	xattr_entry_copy_value(xei.xentry, buf);
	return 0;
}

int voluta_do_getxattr(struct voluta_env *env, struct voluta_inode_info *ii,
		       const struct voluta_str *name, void *buf,
		       size_t size, size_t *out_size)
{
	int err;
	struct voluta_xattr_ctx xattr_ctx = {
		.env = env,
		.sbi = sbi_of(env),
		.ii = ii,
		.name = name,
		.value.ptr = buf,
		.value.len = size
	};

	err = check_xattr(&xattr_ctx);
	if (err) {
		return err;
	}
	err = voluta_do_access(env, ii, R_OK);
	if (err) {
		return err;
	}
	err = do_getxattr(&xattr_ctx, out_size);
	if (err) {
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void discard_xentry(const struct voluta_xentry_info *xei)
{
	xattr_entry_clear(xei->xentry);
	if (xei->xa_vi) {
		v_dirtify(xei->xa_vi);
	}
}

static int new_xattr_node(const struct voluta_xattr_ctx *xattr_ctx,
			  struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi;
	const struct voluta_inode_info *ii = xattr_ctx->ii;

	err = voluta_new_vnode(xattr_ctx->sbi, VOLUTA_VTYPE_XANODE, &vi);
	if (err) {
		return err;
	}
	setup_xattr_node(vi, i_ino_of(ii));
	*out_vi = vi;
	return 0;
}

static int del_xattr_node(const struct voluta_xattr_ctx *xattr_ctx,
			  struct voluta_vnode_info *vi)
{
	return voluta_del_vnode(xattr_ctx->env, vi);
}

static int
require_xattr_node_at(const struct voluta_xattr_ctx *xattr_ctx, size_t sloti,
		      struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vaddr vaddr;
	struct voluta_vnode_info *vi;
	struct voluta_inode_info *ii = xattr_ctx->ii;

	xattr_get_at(ii, sloti, &vaddr);
	if (!vaddr_isnull(&vaddr)) {
		return stage_xattr_node(xattr_ctx, &vaddr, out_vi);
	}
	err = new_xattr_node(xattr_ctx, &vi);
	if (err) {
		return err;
	}
	xattr_set_at(ii, sloti, v_vaddr_of(vi));
	i_dirtify(ii);
	*out_vi = vi;
	return 0;
}

static int try_insert_at_xanode(const struct voluta_xattr_ctx *xattr_ctx,
				struct voluta_vnode_info *vi,
				struct voluta_xentry_info *xei)
{
	struct voluta_xattr_entry *xe;

	xe = xattr_node_insert(vi->u.xan, xattr_ctx->name, &xattr_ctx->value);
	if (xe == NULL) {
		return -ENOSPC;
	}
	xei->xa_vi = vi;
	xei->xentry = xe;
	v_dirtify(vi);
	return 0;
}

static int try_insert_at_inode(const struct voluta_xattr_ctx *xattr_ctx,
			       struct voluta_xentry_info *xei)
{
	struct voluta_xattr_entry *xe;
	const struct voluta_str *name = xattr_ctx->name;
	struct voluta_inode_info *ii = xattr_ctx->ii;

	xe = inode_insert_xentry(ii->inode, name, &xattr_ctx->value);
	if (xe == NULL) {
		return -ENOSPC;
	}
	xei->xa_vi = NULL;
	xei->xentry = xattr_entry_unconst(xe);
	i_dirtify(ii);
	return 0;
}

static int try_insert_by_xanode(const struct voluta_xattr_ctx *xattr_ctx,
				struct voluta_xentry_info *xei)
{
	int err;
	struct voluta_vnode_info *vi;
	const size_t nslots_max = xattr_nslots_max(xattr_ctx->ii);

	for (size_t sloti = 0; sloti < nslots_max; ++sloti) {
		err = require_xattr_node_at(xattr_ctx, sloti, &vi);
		if (err) {
			break;
		}
		err = try_insert_at_xanode(xattr_ctx, vi, xei);
		if (!err) {
			break;
		}
	}
	return err;
}

static int setxattr_create(struct voluta_xattr_ctx *xattr_ctx,
			   struct voluta_xentry_info *xei)
{
	int err;

	if ((xattr_ctx->flags == XATTR_CREATE) && xei->xentry) {
		return -EEXIST;
	}
	err = try_insert_at_inode(xattr_ctx, xei);
	if (err == -ENOSPC) {
		err = try_insert_by_xanode(xattr_ctx, xei);
	}
	return err;
}

static int setxattr_replace(struct voluta_xattr_ctx *xattr_ctx,
			    struct voluta_xentry_info *xei)
{
	int err;
	struct voluta_xentry_info xei_cur = {
		.xa_vi = xei->xa_vi,
		.xentry = xei->xentry
	};

	/* TODO: Try replace in-place */
	if ((xattr_ctx->flags == XATTR_REPLACE) && !xei->xentry) {
		return -ENOATTR;
	}
	err = setxattr_create(xattr_ctx, xei);
	if (!err) {
		discard_xentry(&xei_cur);
	}
	return err;
}

static int do_setxattr(struct voluta_xattr_ctx *xattr_ctx)
{
	int err;
	struct voluta_xentry_info xei = { .xentry = NULL };

	err = lookup_xentry(xattr_ctx, &xei);
	if (err && (err != -ENOATTR)) {
		return err;
	}
	if (xattr_ctx->flags == XATTR_CREATE) {
		err = setxattr_create(xattr_ctx, &xei);
	} else if (xattr_ctx->flags == XATTR_REPLACE) {
		err = setxattr_replace(xattr_ctx, &xei);
	} else if (xei.xentry) { /* implicit replace */
		xattr_ctx->flags = XATTR_REPLACE;
		err = setxattr_replace(xattr_ctx, &xei);
	} else {
		err = setxattr_create(xattr_ctx, &xei);
	}
	return err;
}

static void post_add_xattr(struct voluta_xattr_ctx *xattr_ctx)
{
	struct voluta_env *env = xattr_ctx->env;
	struct voluta_inode_info *ii = xattr_ctx->ii;

	if (xattr_ctx->flags != XATTR_REPLACE) {
		ii->inode->i_nxattr += 1;
	}
	i_update_times(env, ii, VOLUTA_ATTR_CTIME);
}

static void post_remove_xattr(struct voluta_xattr_ctx *xattr_ctx)
{
	struct voluta_env *env = xattr_ctx->env;
	struct voluta_inode_info *ii = xattr_ctx->ii;

	voluta_assert_gt(ii->inode->i_nxattr, 0);

	ii->inode->i_nxattr -= 1;
	i_update_times(env, ii, VOLUTA_ATTR_CTIME);
}

int voluta_do_setxattr(struct voluta_env *env, struct voluta_inode_info *ii,
		       const struct voluta_str *name, const char *value,
		       size_t size, int flags)
{
	int err;
	struct voluta_xattr_ctx xattr_ctx = {
		.env = env,
		.sbi = sbi_of(env),
		.ii = ii,
		.name = name,
		.value.ptr = (void *)value,
		.value.len = size,
		.size = size,
		.flags = flags
	};

	err = check_xattr(&xattr_ctx);
	if (err) {
		return err;
	}
	err = voluta_do_access(env, ii, R_OK); /* TODO: is it? */
	if (err) {
		return err;
	}
	err = do_setxattr(&xattr_ctx);
	if (err) {
		return err;
	}
	post_add_xattr(&xattr_ctx);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* TODO: Delete node if empty */
static int do_removexattr(struct voluta_xattr_ctx *xattr_ctx)
{
	int err;
	struct voluta_xentry_info xei = { .xentry = NULL };

	err = lookup_xentry(xattr_ctx, &xei);
	if (err) {
		return err;
	}
	discard_xentry(&xei);
	return 0;
}

int voluta_do_removexattr(struct voluta_env *env,
			  struct voluta_inode_info *ii,
			  const struct voluta_str *name)
{
	int err;
	struct voluta_xattr_ctx xattr_ctx = {
		.env = env,
		.sbi = sbi_of(env),
		.ii = ii,
		.name = name
	};

	err = check_xattr(&xattr_ctx);
	if (err) {
		return err;
	}
	err = voluta_do_access(env, ii, W_OK);
	if (err) {
		return err;
	}
	err = do_removexattr(&xattr_ctx);
	if (err) {
		return err;
	}
	post_remove_xattr(&xattr_ctx);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int emit(struct voluta_xattr_ctx *xattr_ctx, const char *name,
		size_t nlen)
{
	int err;
	struct voluta_listxattr_ctx *listxattr_ctx = xattr_ctx->listxattr_ctx;

	err = listxattr_ctx->actor(listxattr_ctx, name, nlen);
	return err ? -ERANGE : 0;
}

static bool emit_xentry(struct voluta_xattr_ctx *xattr_ctx,
			const struct voluta_xattr_entry *xe)
{
	return emit(xattr_ctx, xattr_entry_name(xe), xe->xe_name_len);
}

static int emit_range(struct voluta_xattr_ctx *xattr_ctx,
		      const struct voluta_xattr_entry *itr,
		      const struct voluta_xattr_entry *end)
{
	int err = 0;

	while ((itr < end) && !err) {
		if (xattr_entry_isactive(itr)) {
			err = emit_xentry(xattr_ctx, itr);
		}
		itr = xattr_entry_next(itr);
	}
	return err;
}

static int emit_inode(struct voluta_xattr_ctx *xattr_ctx)
{
	const struct voluta_inode_info *ii = xattr_ctx->ii;
	const struct voluta_xattr_entry *beg = inode_xe_begin(ii->inode);
	const struct voluta_xattr_entry *end = inode_xe_end(ii->inode);

	return emit_range(xattr_ctx, beg, end);
}

static int emit_xattr_node(struct voluta_xattr_ctx *xattr_ctx,
			   const struct voluta_vnode_info *vi)
{
	const struct voluta_xattr_entry *beg = xattr_node_begin(vi->u.xan);
	const struct voluta_xattr_entry *end = xattr_node_end(vi->u.xan);

	return emit_range(xattr_ctx, beg, end);
}

static int emit_xnode_at(struct voluta_xattr_ctx *xattr_ctx, size_t sloti)
{
	int err;
	struct voluta_vaddr vaddr;
	struct voluta_vnode_info *vi;
	struct voluta_inode_info *ii = xattr_ctx->ii;

	xattr_get_at(ii, sloti, &vaddr);
	if (vaddr_isnull(&vaddr)) {
		return 0;
	}
	err = stage_xattr_node(xattr_ctx, &vaddr, &vi);
	if (err) {
		return err;
	}
	err = emit_xattr_node(xattr_ctx, vi);
	if (err) {
		return err;
	}
	return 0;
}

static int emit_by_xanode(struct voluta_xattr_ctx *xattr_ctx)
{
	int err = 0;
	const size_t nslots_max = xattr_nslots_max(xattr_ctx->ii);

	for (size_t sloti = 0; sloti < nslots_max; ++sloti) {
		err = emit_xnode_at(xattr_ctx, sloti);
		if (err) {
			break;
		}
	}
	return err;
}

static int emit_xattr_names(struct voluta_xattr_ctx *xattr_ctx)
{
	int err;

	err = emit_inode(xattr_ctx);
	if (err) {
		return err;
	}
	err = emit_by_xanode(xattr_ctx);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_do_listxattr(struct voluta_env *env, struct voluta_inode_info *ii,
			struct voluta_listxattr_ctx *listxattr_ctx)
{
	int err;
	struct voluta_xattr_ctx xattr_ctx = {
		.env = env,
		.sbi = sbi_of(env),
		.ii = ii,
		.listxattr_ctx = listxattr_ctx,
		.keep_iter = true
	};

	err = check_xattr(&xattr_ctx);
	if (err) {
		return err;
	}
	err = voluta_do_access(env, ii, R_OK);
	if (err) {
		return err;
	}
	err = emit_xattr_names(&xattr_ctx);
	if (err) {
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int drop_xattr_node_at(struct voluta_xattr_ctx *xattr_ctx, size_t sloti)
{
	int err;
	struct voluta_vaddr vaddr;
	struct voluta_vnode_info *vi;
	struct voluta_inode_info *ii = xattr_ctx->ii;

	xattr_get_at(ii, sloti, &vaddr);
	if (vaddr_isnull(&vaddr)) {
		return 0;
	}
	/* TODO: del by addr only */
	err = stage_xattr_node(xattr_ctx, &vaddr, &vi);
	if (err) {
		return err;
	}
	err = del_xattr_node(xattr_ctx, vi);
	if (err) {
		return err;
	}
	xattr_unset_at(ii, sloti);
	return 0;
}

static int drop_xattr_slots(struct voluta_xattr_ctx *xattr_ctx)
{
	int err;
	const size_t nslots_max = xattr_nslots_max(xattr_ctx->ii);

	for (size_t sloti = 0; sloti < nslots_max; ++sloti) {
		err = drop_xattr_node_at(xattr_ctx, sloti);
		if (err) {
			return err;
		}
	}
	return 0;
}

int voluta_drop_xattr(struct voluta_env *env, struct voluta_inode_info *ii)
{
	struct voluta_xattr_ctx xattr_ctx = {
		.env = env,
		.sbi = sbi_of(env),
		.ii = ii,
	};

	return drop_xattr_slots(&xattr_ctx);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_verify_xattr_node(const struct voluta_xattr_node *xattr_node)
{
	int err;

	err = voluta_verify_ino(xattr_node->xa_ino);
	if (err) {
		return err;
	}
	err = xaddr_node_verify(xattr_node);
	if (err) {
		return err;
	}
	return 0;
}

