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
	{ .prefix = (p_), .ns = (n_), .flags = (f_) }


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

static size_t xe_aligned_size(size_t size)
{
	const size_t align = sizeof(struct voluta_xattr_entry);

	return (align * div_round_up(size, align));
}

static size_t xe_calc_payload_nents(size_t name_len, size_t value_size)
{
	const size_t payload_size =
		xe_aligned_size(name_len) + xe_aligned_size(value_size);

	return payload_size / sizeof(struct voluta_xattr_entry);
}

static size_t xe_calc_nents(size_t name_len, size_t value_size)
{
	return 1 + xe_calc_payload_nents(name_len, value_size);
}

static size_t xe_calc_nents_of(const struct voluta_str *name,
			       const struct voluta_buf *value)
{
	return xe_calc_nents(name->len, value->len);
}

static size_t xe_diff(const struct voluta_xattr_entry *beg,
		      const struct voluta_xattr_entry *end)
{
	voluta_assert(end >= beg);

	return (size_t)(end - beg);
}

static struct voluta_xattr_entry *
xe_unconst(const struct voluta_xattr_entry *xe)
{
	return (struct voluta_xattr_entry *)xe;
}

static struct voluta_xentry_view *
xe_view_of(const struct voluta_xattr_entry *xe)
{
	return voluta_container_of(xe, struct voluta_xentry_view, xe);
}

static size_t xe_name_len(const struct voluta_xattr_entry *xe)
{
	return le16_to_cpu(xe->xe_name_len);
}

static void xe_set_name_len(struct voluta_xattr_entry *xe, size_t name_len)
{
	xe->xe_name_len = cpu_to_le16((uint16_t)name_len);
}

static size_t xe_value_size(const struct voluta_xattr_entry *xe)
{
	return le16_to_cpu(xe->xe_value_size);
}

static void xe_set_value_size(struct voluta_xattr_entry *xe, size_t value_size)
{
	xe->xe_value_size = cpu_to_le16((uint16_t)value_size);
}

static char *xe_name(const struct voluta_xattr_entry *xe)
{
	struct voluta_xentry_view *xeview = xe_view_of(xe);

	return (char *)xeview->xe_data;
}

static void *xe_value(const struct voluta_xattr_entry *xe)
{
	struct voluta_xentry_view *xeview = xe_view_of(xe);

	return xeview->xe_data + xe_aligned_size(xe_name_len(xe));
}

static bool xe_has_name(const struct voluta_xattr_entry *xe,
			const struct voluta_str *name)
{
	return (name->len == xe_name_len(xe)) &&
	       !memcmp(xe_name(xe), name->str, name->len);
}

static size_t xe_nents(const struct voluta_xattr_entry *xe)
{
	return xe_calc_nents(xe_name_len(xe), xe_value_size(xe));
}

static struct voluta_xattr_entry *xe_next(const struct voluta_xattr_entry *xe)
{
	const struct voluta_xattr_entry *next = xe + xe_nents(xe);

	voluta_assert(xe != next);
	return xe_unconst(next);
}

static void xe_assign(struct voluta_xattr_entry *xe,
		      const struct voluta_str *name,
		      const struct voluta_buf *value)
{
	xe_set_name_len(xe, name->len);
	xe_set_value_size(xe, value->len);
	memcpy(xe_name(xe), name->str, name->len);
	memcpy(xe_value(xe), value->buf, value->len);
}

static void xe_reset(struct voluta_xattr_entry *xe, size_t cnt)
{
	voluta_memzero(xe, cnt * sizeof(*xe));
}

static void xe_squeeze(struct voluta_xattr_entry *xe,
		       const struct voluta_xattr_entry *last)
{
	const struct voluta_xattr_entry *next = xe_next(xe);

	memmove(xe, next, xe_diff(next, last) * sizeof(*xe));
}

static void xe_copy_value(const struct voluta_xattr_entry *xe,
			  struct voluta_buf *buf)
{
	const void *value = xe_value(xe);
	const size_t size = xe_value_size(xe);

	voluta_buf_append(buf, value, size);
}

static struct voluta_xattr_entry *
xe_search(const struct voluta_xattr_entry *itr,
	  const struct voluta_xattr_entry *end,
	  const struct voluta_str *name)
{
	while (itr < end) {
		if (xe_has_name(itr, name)) {
			return (struct voluta_xattr_entry *)itr;
		}
		itr = xe_next(itr);
	}
	return NULL;
}

static struct voluta_xattr_entry *
xe_append(struct voluta_xattr_entry *xe,
	  const struct voluta_xattr_entry *end,
	  const struct voluta_str *name,
	  const struct voluta_buf *value)
{
	const size_t nfree = xe_diff(xe, end);
	const size_t nents = xe_calc_nents_of(name, value);

	if (nfree < nents) {
		return NULL;
	}
	xe_assign(xe, name, value);
	return xe;
}

static int xe_verify(const struct voluta_xattr_entry *xe,
		     const struct voluta_xattr_entry *end)
{
	size_t nents;
	const struct voluta_xattr_entry *itr = xe;

	while (itr < end) {
		nents = xe_nents(xe);
		if (!nents || ((xe + nents) > end)) {
			return -EFSCORRUPTED;
		}
		itr += nents;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static ino_t xan_ino(const struct voluta_xattr_node *xan)
{
	return ino_to_cpu(xan->xa_ino);
}

static void xan_set_ino(struct voluta_xattr_node *xan, ino_t ino)
{
	xan->xa_ino = cpu_to_ino(ino);
}

static size_t xan_nents(const struct voluta_xattr_node *xan)
{
	return le16_to_cpu(xan->xa_nents);
}

static void xan_set_nents(struct voluta_xattr_node *xan, size_t n)
{
	voluta_assert_le(n, ARRAY_SIZE(xan->xe));
	xan->xa_nents = cpu_to_le16((uint16_t)n);
}

static void xan_inc_nents(struct voluta_xattr_node *xan, size_t n)
{
	xan_set_nents(xan, xan_nents(xan) + n);
}

static void xan_dec_nents(struct voluta_xattr_node *xan, size_t n)
{
	voluta_assert_gt(xan_nents(xan), 0);

	xan_set_nents(xan, xan_nents(xan) - n);
}

static void xan_setup(struct voluta_xattr_node *xan, ino_t ino)
{
	xan_set_ino(xan, ino);
	xan_set_nents(xan, 0);
	xe_reset(xan->xe, ARRAY_SIZE(xan->xe));
}

static struct voluta_xattr_entry *
xan_begin(const struct voluta_xattr_node *xan)
{
	return xe_unconst(xan->xe);
}

static const struct voluta_xattr_entry *
xan_end(const struct voluta_xattr_node *xan)
{
	return xan->xe + ARRAY_SIZE(xan->xe);
}

static struct voluta_xattr_entry *
xan_last(const struct voluta_xattr_node *xan)
{
	return xe_unconst(xan->xe) + xan_nents(xan);
}

static struct voluta_xattr_entry *
xan_search(const struct voluta_xattr_node *xan, const struct voluta_str *str)
{
	struct voluta_xattr_entry *xe = NULL;
	const size_t nmin = xe_calc_nents(str->len, 0);

	if (xan_nents(xan) >= nmin) {
		xe = xe_search(xan_begin(xan), xan_last(xan), str);
	}
	return xe;
}

static struct voluta_xattr_entry *
xan_insert(struct voluta_xattr_node *xan,
	   const struct voluta_str *name, const struct voluta_buf *value)
{
	struct voluta_xattr_entry *xe;

	xe = xe_append(xan_last(xan), xan_end(xan), name, value);
	if (xe != NULL) {
		xan_inc_nents(xan, xe_nents(xe));
	}
	return xe;
}

static void xan_remove(struct voluta_xattr_node *xan,
		       struct voluta_xattr_entry *xe)
{
	const size_t nents = xe_nents(xe);

	voluta_assert(xe >= xan_begin(xan));
	voluta_assert(xe < xan_end(xan));

	xe_squeeze(xe, xan_last(xan));
	xan_dec_nents(xan, nents);
}

static int xan_verify(const struct voluta_xattr_node *xan)
{
	return xe_verify(xan_begin(xan), xan_end(xan));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_inode_xattr *
inode_xattr_of(const struct voluta_inode *inode)
{
	const struct voluta_inode_xattr *ixa = &inode->i_x;

	return (struct voluta_inode_xattr *)ixa;
}

static struct voluta_inode_xattr *
ixa_of(const struct voluta_inode_info *ii)
{
	return inode_xattr_of(ii->inode);
}

static size_t ixa_nents(const struct voluta_inode_xattr *ixa)
{
	return le16_to_cpu(ixa->xa_nents);
}

static void ixa_set_nents(struct voluta_inode_xattr *ixa, size_t n)
{
	voluta_assert_le(n, ARRAY_SIZE(ixa->xe));
	ixa->xa_nents = cpu_to_le16((uint16_t)n);
}

static void ixa_inc_nents(struct voluta_inode_xattr *ixa, size_t n)
{
	ixa_set_nents(ixa, ixa_nents(ixa) + n);
}

static void ixa_dec_nents(struct voluta_inode_xattr *ixa, size_t n)
{
	ixa_set_nents(ixa, ixa_nents(ixa) - n);
}

static loff_t ixa_off(const struct voluta_inode_xattr *ixa, size_t slot)
{
	return off_to_cpu(ixa->xa_off[slot]);
}

static void ixa_set_off(struct voluta_inode_xattr *ixa, size_t slot, loff_t off)
{
	ixa->xa_off[slot] = cpu_to_off(off);
}

static void ixa_unset_off(struct voluta_inode_xattr *ixa, size_t slot)
{
	ixa_set_off(ixa, slot, VOLUTA_OFF_NULL);
}

static size_t ixa_nslots_max(const struct voluta_inode_xattr *ixa)
{
	return ARRAY_SIZE(ixa->xa_off);
}

static void ixa_reset_slots(struct voluta_inode_xattr *ixa)
{
	const size_t nslots = ixa_nslots_max(ixa);

	for (size_t slot = 0; slot < nslots; ++slot) {
		ixa_unset_off(ixa, slot);
	}
}

static void ixa_setup(struct voluta_inode_xattr *ixa)
{
	ixa_set_nents(ixa, 0);
	ixa_reset_slots(ixa);
	xe_reset(ixa->xe, ARRAY_SIZE(ixa->xe));
}

static struct voluta_xattr_entry *
ixa_begin(const struct voluta_inode_xattr *ixa)
{
	return xe_unconst(ixa->xe);
}

static const struct voluta_xattr_entry *
ixa_end(const struct voluta_inode_xattr *ixa)
{
	return ixa->xe + ARRAY_SIZE(ixa->xe);
}

static struct voluta_xattr_entry *
ixa_last(const struct voluta_inode_xattr *ixa)
{
	return xe_unconst(ixa->xe) + ixa_nents(ixa);
}

static struct voluta_xattr_entry *
ixa_search(const struct voluta_inode_xattr *ixa,
	   const struct voluta_str *str)
{
	struct voluta_xattr_entry *xe = NULL;
	const size_t nmin = xe_calc_nents(str->len, 0);

	if (ixa_nents(ixa) >= nmin) {
		xe = xe_search(ixa_begin(ixa), ixa_last(ixa), str);
	}
	return xe;
}

static struct voluta_xattr_entry *
ixa_insert(struct voluta_inode_xattr *ixa,
	   const struct voluta_str *name,
	   const struct voluta_buf *value)
{
	struct voluta_xattr_entry *xe;

	xe = xe_append(ixa_last(ixa), ixa_end(ixa), name, value);
	if (xe != NULL) {
		ixa_inc_nents(ixa, xe_nents(xe));
	}
	return xe;
}

static void ixa_remove(struct voluta_inode_xattr *ixa,
		       struct voluta_xattr_entry *xe)
{
	const size_t nents = xe_nents(xe);

	voluta_assert(xe >= ixa_begin(ixa));
	voluta_assert(xe < ixa_end(ixa));

	xe_squeeze(xe, ixa_last(ixa));
	ixa_dec_nents(ixa, nents);
}

static int ixa_verify(const struct voluta_inode_xattr *ixa)
{
	return xe_verify(ixa_begin(ixa), ixa_end(ixa));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_xattr_node *xan_of(const struct voluta_vnode_info *vi)
{
	return vi->u.xan;
}

static ino_t xa_ino_of(const struct voluta_vnode_info *vi)
{
	return xan_ino(xan_of(vi));
}

static size_t xa_nslots_max(const struct voluta_inode_info *ii)
{
	return ixa_nslots_max(ixa_of(ii));
}

static void xa_unset_at(struct voluta_inode_info *ii, size_t sloti)
{
	ixa_unset_off(ixa_of(ii), sloti);
}

static void xa_get_at(const struct voluta_inode_info *ii, size_t sloti,
		      struct voluta_vaddr *out_vaddr)
{
	const loff_t off = ixa_off(ixa_of(ii), sloti);

	voluta_vaddr_by_off(out_vaddr, VOLUTA_VTYPE_XANODE, off);
}

static void xa_set_at(const struct voluta_inode_info *ii, size_t sloti,
		      const struct voluta_vaddr *vaddr)
{
	ixa_set_off(ixa_of(ii), sloti, vaddr->off);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_setup_inode_xattr(struct voluta_inode_info *ii)
{
	ixa_setup(ixa_of(ii));
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct voluta_xattr_ctx {
	struct voluta_sb_info *sbi;
	const struct voluta_oper_info *opi;
	struct voluta_listxattr_ctx *lsxa_ctx;
	struct voluta_inode_info *ii;
	const struct voluta_str *name;
	struct voluta_buf value;
	size_t size;
	int flags;
	bool keep_iter;
};

struct voluta_xentry_info {
	struct voluta_inode_info *ii;
	struct voluta_vnode_info *vi;
	struct voluta_xattr_entry *xentry;
};

static void setup_xattr_node(struct voluta_vnode_info *vi, ino_t ino)
{
	xan_setup(xan_of(vi), ino);
}

static int stage_xattr_node(const struct voluta_xattr_ctx *xa_ctx,
			    const struct voluta_vaddr *vaddr,
			    struct voluta_vnode_info **out_vi)
{
	int err;
	ino_t ino, xattr_ino;
	struct voluta_vnode_info *vi;

	err = voluta_stage_vnode(xa_ctx->sbi, vaddr, &vi);
	if (err) {
		return err;
	}
	ino = i_ino_of(xa_ctx->ii);
	xattr_ino = xa_ino_of(vi);
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

static const struct voluta_xattr_prefix *
search_prefix(const struct voluta_str *name)
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

static int check_xattr(const struct voluta_xattr_ctx *xa_ctx)
{
	int err;
	const struct voluta_inode_info *ii = xa_ctx->ii;

	if (!i_isreg(ii) && !i_isdir(ii) && !i_islnk(ii)) {
		return -EINVAL;
	}
	err = check_xattr_name(xa_ctx->name);
	if (err) {
		return err;
	}
	if (xa_ctx->size > VOLUTA_XATTR_VALUE_MAX) {
		return -EINVAL;
	}
	if (!is_valid_xflags(xa_ctx->flags)) {
		return -ENOTSUP;
	}
	return 0;
}

static int lookup_xentry_in_xan(const struct voluta_xattr_ctx *xa_ctx,
				const struct voluta_vaddr *vaddr,
				struct voluta_xentry_info *xei)
{
	int err;
	struct voluta_xattr_entry *xe;
	struct voluta_vnode_info *vi;

	if (vaddr_isnull(vaddr)) {
		return -ENOENT;
	}
	err = stage_xattr_node(xa_ctx, vaddr, &vi);
	if (err) {
		return err;
	}
	xe = xan_search(xan_of(vi), xa_ctx->name);
	if (xe == NULL) {
		return -ENOENT;
	}
	xei->vi = vi;
	xei->xentry = xe;
	return 0;
}

static int lookup_xentry_at_xan(struct voluta_xattr_ctx *xa_ctx,
				struct voluta_xentry_info *xei)
{
	int err = -ENOENT;
	struct voluta_vaddr vaddr;
	const struct voluta_inode_info *ii = xa_ctx->ii;

	for (size_t sloti = 0; sloti < xa_nslots_max(ii); ++sloti) {
		xa_get_at(ii, sloti, &vaddr);
		err = lookup_xentry_in_xan(xa_ctx, &vaddr, xei);
		if (err != -ENOENT) {
			break;
		}
	}
	return err;
}

static int lookup_xentry_at_ixa(struct voluta_xattr_ctx *xa_ctx,
				struct voluta_xentry_info *xei)
{
	struct voluta_xattr_entry *xe;
	struct voluta_inode_info *ii = xa_ctx->ii;

	xe = ixa_search(ixa_of(ii), xa_ctx->name);
	if (xe == NULL) {
		return -ENOENT;
	}
	xei->ii = ii;
	xei->xentry = xe;
	return 0;
}

static int lookup_xentry(struct voluta_xattr_ctx *xa_ctx,
			 struct voluta_xentry_info *xei)
{
	int err;

	err = lookup_xentry_at_ixa(xa_ctx, xei);
	if (err != -ENOENT) {
		goto out;
	}
	err = lookup_xentry_at_xan(xa_ctx, xei);
	if (err != -ENOENT) {
		goto out;
	}
out:
	return (err == -ENOENT) ? -ENOATTR : err;
}

static int do_getxattr(struct voluta_xattr_ctx *xa_ctx, size_t *out_size)
{
	int err;
	struct voluta_buf *buf = &xa_ctx->value;
	struct voluta_xentry_info xei = { .xentry = NULL };

	err = lookup_xentry(xa_ctx, &xei);
	if (err) {
		return err;
	}
	*out_size = xe_value_size(xei.xentry);
	if (buf->bsz == 0) {
		return 0;
	}
	if (buf->bsz < (buf->len + *out_size)) {
		return -ERANGE;
	}
	xe_copy_value(xei.xentry, buf);
	return 0;
}

int voluta_do_getxattr(struct voluta_sb_info *sbi,
		       const struct voluta_oper_info *opi,
		       struct voluta_inode_info *ii,
		       const struct voluta_str *name,
		       void *buf, size_t size, size_t *out_size)
{
	int err;
	struct voluta_xattr_ctx xa_ctx = {
		.sbi = sbi,
		.opi = opi,
		.ii = ii,
		.name = name,
		.value.buf = buf,
		.value.len = 0,
		.value.bsz = size
	};

	err = check_xattr(&xa_ctx);
	if (err) {
		return err;
	}
	err = voluta_do_access(opi, ii, R_OK);
	if (err) {
		return err;
	}
	err = do_getxattr(&xa_ctx, out_size);
	if (err) {
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void discard_xentry(const struct voluta_xentry_info *xei)
{
	struct voluta_inode_info *ii = xei->ii;
	struct voluta_vnode_info *vi = xei->vi;

	if (ii != NULL) {
		ixa_remove(ixa_of(ii), xei->xentry);
		i_dirtify(ii);
	} else if (vi != NULL) {
		xan_remove(xan_of(vi), xei->xentry);
		v_dirtify(vi);
	}
}

static int new_xattr_node(const struct voluta_xattr_ctx *xa_ctx,
			  struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vnode_info *vi;
	const struct voluta_inode_info *ii = xa_ctx->ii;

	err = voluta_new_vnode(xa_ctx->sbi, VOLUTA_VTYPE_XANODE, &vi);
	if (err) {
		return err;
	}
	setup_xattr_node(vi, i_ino_of(ii));
	*out_vi = vi;
	return 0;
}

static int del_xattr_node(const struct voluta_xattr_ctx *xa_ctx,
			  struct voluta_vnode_info *vi)
{
	return voluta_del_vnode(xa_ctx->sbi, vi);
}

static int
require_xattr_node(const struct voluta_xattr_ctx *xa_ctx,
		   size_t sloti, struct voluta_vnode_info **out_vi)
{
	int err;
	struct voluta_vaddr vaddr;
	struct voluta_vnode_info *vi;
	struct voluta_inode_info *ii = xa_ctx->ii;

	xa_get_at(ii, sloti, &vaddr);
	if (!vaddr_isnull(&vaddr)) {
		return stage_xattr_node(xa_ctx, &vaddr, out_vi);
	}
	err = new_xattr_node(xa_ctx, &vi);
	if (err) {
		return err;
	}
	xa_set_at(ii, sloti, v_vaddr_of(vi));
	i_dirtify(ii);
	*out_vi = vi;
	return 0;
}

static int try_insert_at(const struct voluta_xattr_ctx *xa_ctx,
			 struct voluta_vnode_info *vi,
			 struct voluta_xentry_info *xei)
{
	struct voluta_xattr_entry *xe;

	xe = xan_insert(xan_of(vi), xa_ctx->name, &xa_ctx->value);
	if (xe == NULL) {
		return -ENOSPC;
	}
	xei->vi = vi;
	xei->xentry = xe;
	v_dirtify(vi);
	return 0;
}

static int try_insert_at_xan(const struct voluta_xattr_ctx *xa_ctx,
			     struct voluta_xentry_info *xei)
{
	int err;
	struct voluta_vnode_info *vi;
	const size_t nslots_max = xa_nslots_max(xa_ctx->ii);

	for (size_t sloti = 0; sloti < nslots_max; ++sloti) {
		err = require_xattr_node(xa_ctx, sloti, &vi);
		if (err) {
			break;
		}
		err = try_insert_at(xa_ctx, vi, xei);
		if (!err) {
			break;
		}
	}
	return err;
}

static int try_insert_at_ixa(const struct voluta_xattr_ctx *xa_ctx,
			     struct voluta_xentry_info *xei)
{
	struct voluta_xattr_entry *xe;
	struct voluta_inode_info *ii = xa_ctx->ii;

	xe = ixa_insert(ixa_of(ii), xa_ctx->name, &xa_ctx->value);
	if (xe == NULL) {
		return -ENOSPC;
	}
	xei->ii = ii;
	xei->xentry = xe;
	i_dirtify(ii);
	return 0;
}

static int setxattr_create(struct voluta_xattr_ctx *xa_ctx,
			   struct voluta_xentry_info *xei)
{
	int err;

	if ((xa_ctx->flags == XATTR_CREATE) && xei->xentry) {
		return -EEXIST;
	}
	err = try_insert_at_ixa(xa_ctx, xei);
	if (err != -ENOSPC) {
		return err;
	}
	err = try_insert_at_xan(xa_ctx, xei);
	if (err) {
		return err;
	}
	return 0;
}

static int setxattr_replace(struct voluta_xattr_ctx *xa_ctx,
			    struct voluta_xentry_info *xei)
{
	int err;
	struct voluta_xentry_info xei_cur = {
		.vi = xei->vi,
		.ii = xei->ii,
		.xentry = xei->xentry
	};

	/* TODO: Try replace in-place */
	if ((xa_ctx->flags == XATTR_REPLACE) && !xei->xentry) {
		return -ENOATTR;
	}
	err = setxattr_create(xa_ctx, xei);
	if (!err) {
		discard_xentry(&xei_cur);
	}
	return err;
}

static int do_setxattr(struct voluta_xattr_ctx *xa_ctx)
{
	int err;
	struct voluta_xentry_info xei = { .xentry = NULL };

	err = lookup_xentry(xa_ctx, &xei);
	if (err && (err != -ENOATTR)) {
		return err;
	}
	if (xa_ctx->flags == XATTR_CREATE) {
		return setxattr_create(xa_ctx, &xei);
	}
	if (xa_ctx->flags == XATTR_REPLACE) {
		return setxattr_replace(xa_ctx, &xei);
	}
	if (xei.xentry) { /* implicit replace */
		xa_ctx->flags = XATTR_REPLACE;
		return setxattr_replace(xa_ctx, &xei);
	}
	/* by-default, create */
	return setxattr_create(xa_ctx, &xei);
}

static void post_add_xattr(struct voluta_xattr_ctx *xa_ctx)
{
	i_update_times(xa_ctx->ii, xa_ctx->opi, VOLUTA_ATTR_CTIME);
}

static void post_remove_xattr(struct voluta_xattr_ctx *xa_ctx)
{
	i_update_times(xa_ctx->ii, xa_ctx->opi, VOLUTA_ATTR_CTIME);
}

int voluta_do_setxattr(struct voluta_sb_info *sbi,
		       const struct voluta_oper_info *opi,
		       struct voluta_inode_info *ii,
		       const struct voluta_str *name,
		       const char *value, size_t size, int flags)
{
	int err;
	struct voluta_xattr_ctx xa_ctx = {
		.sbi = sbi,
		.opi = opi,
		.ii = ii,
		.name = name,
		.value.buf = (void *)value,
		.value.len = size,
		.value.bsz = size,
		.size = size,
		.flags = flags
	};

	err = check_xattr(&xa_ctx);
	if (err) {
		return err;
	}
	err = voluta_do_access(opi, ii, R_OK); /* TODO: is it? */
	if (err) {
		return err;
	}
	err = do_setxattr(&xa_ctx);
	if (err) {
		return err;
	}
	post_add_xattr(&xa_ctx);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* TODO: Delete node if empty */
static int do_removexattr(struct voluta_xattr_ctx *xa_ctx)
{
	int err;
	struct voluta_xentry_info xei = { .xentry = NULL };

	err = lookup_xentry(xa_ctx, &xei);
	if (err) {
		return err;
	}
	discard_xentry(&xei);
	return 0;
}

int voluta_do_removexattr(struct voluta_sb_info *sbi,
			  const struct voluta_oper_info *opi,
			  struct voluta_inode_info *ii,
			  const struct voluta_str *name)
{
	int err;
	struct voluta_xattr_ctx xa_ctx = {
		.sbi = sbi,
		.opi = opi,
		.ii = ii,
		.name = name
	};

	err = check_xattr(&xa_ctx);
	if (err) {
		return err;
	}
	err = voluta_do_access(opi, ii, W_OK);
	if (err) {
		return err;
	}
	err = do_removexattr(&xa_ctx);
	if (err) {
		return err;
	}
	post_remove_xattr(&xa_ctx);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int emit(struct voluta_xattr_ctx *xa_ctx, const char *name, size_t nlen)
{
	int err;
	struct voluta_listxattr_ctx *lsxa_ctx = xa_ctx->lsxa_ctx;

	err = lsxa_ctx->actor(lsxa_ctx, name, nlen);
	return err ? -ERANGE : 0;
}

static bool emit_xentry(struct voluta_xattr_ctx *xa_ctx,
			const struct voluta_xattr_entry *xe)
{
	return emit(xa_ctx, xe_name(xe), xe_name_len(xe));
}

static int emit_range(struct voluta_xattr_ctx *xa_ctx,
		      const struct voluta_xattr_entry *itr,
		      const struct voluta_xattr_entry *lst)
{
	int err = 0;

	while ((itr < lst) && !err) {
		err = emit_xentry(xa_ctx, itr);
		itr = xe_next(itr);
	}
	return err;
}

static int emit_ixa(struct voluta_xattr_ctx *xa_ctx)
{
	const struct voluta_inode_xattr *ixa = ixa_of(xa_ctx->ii);

	return emit_range(xa_ctx, ixa_begin(ixa), ixa_last(ixa));
}

static int emit_xan(struct voluta_xattr_ctx *xa_ctx,
		    const struct voluta_vnode_info *vi)
{
	const struct voluta_xattr_node *xan = xan_of(vi);

	return emit_range(xa_ctx, xan_begin(xan), xan_last(xan));
}

static int emit_xan_at(struct voluta_xattr_ctx *xa_ctx, size_t sloti)
{
	int err;
	struct voluta_vaddr vaddr;
	struct voluta_vnode_info *vi;

	xa_get_at(xa_ctx->ii, sloti, &vaddr);
	if (vaddr_isnull(&vaddr)) {
		return 0;
	}
	err = stage_xattr_node(xa_ctx, &vaddr, &vi);
	if (err) {
		return err;
	}
	err = emit_xan(xa_ctx, vi);
	if (err) {
		return err;
	}
	return 0;
}

static int emit_by_xan(struct voluta_xattr_ctx *xa_ctx)
{
	int err = 0;
	const size_t nslots_max = xa_nslots_max(xa_ctx->ii);

	for (size_t sloti = 0; sloti < nslots_max; ++sloti) {
		err = emit_xan_at(xa_ctx, sloti);
		if (err) {
			break;
		}
	}
	return err;
}

static int emit_by_ixa(struct voluta_xattr_ctx *xa_ctx)
{
	return emit_ixa(xa_ctx);
}

static int emit_xattr_names(struct voluta_xattr_ctx *xa_ctx)
{
	int err;

	err = emit_by_ixa(xa_ctx);
	if (err) {
		return err;
	}
	err = emit_by_xan(xa_ctx);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_do_listxattr(struct voluta_sb_info *sbi,
			const struct voluta_oper_info *opi,
			struct voluta_inode_info *ii,
			struct voluta_listxattr_ctx *lsxa)
{
	int err;
	struct voluta_xattr_ctx xa_ctx = {
		.sbi = sbi,
		.opi = opi,
		.ii = ii,
		.lsxa_ctx = lsxa,
		.keep_iter = true
	};

	err = check_xattr(&xa_ctx);
	if (err) {
		return err;
	}
	err = voluta_do_access(opi, ii, R_OK);
	if (err) {
		return err;
	}
	err = emit_xattr_names(&xa_ctx);
	if (err) {
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int drop_xan_at(struct voluta_xattr_ctx *xa_ctx, size_t sloti)
{
	int err;
	struct voluta_vaddr vaddr;
	struct voluta_vnode_info *vi;

	xa_get_at(xa_ctx->ii, sloti, &vaddr);
	if (vaddr_isnull(&vaddr)) {
		return 0;
	}
	/* TODO: del by addr only */
	err = stage_xattr_node(xa_ctx, &vaddr, &vi);
	if (err) {
		return err;
	}
	err = del_xattr_node(xa_ctx, vi);
	if (err) {
		return err;
	}
	xa_unset_at(xa_ctx->ii, sloti);
	return 0;
}

static int drop_xattr_slots(struct voluta_xattr_ctx *xa_ctx)
{
	int err;
	const size_t nslots_max = xa_nslots_max(xa_ctx->ii);

	for (size_t sloti = 0; sloti < nslots_max; ++sloti) {
		err = drop_xan_at(xa_ctx, sloti);
		if (err) {
			return err;
		}
	}
	return 0;
}

int voluta_drop_xattr(struct voluta_sb_info *sbi,
		      const struct voluta_oper_info *opi,
		      struct voluta_inode_info *ii)
{
	struct voluta_xattr_ctx xa_ctx = {
		.sbi = sbi,
		.opi = opi,
		.ii = ii,
	};

	return drop_xattr_slots(&xa_ctx);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_verify_inode_xattr(const struct voluta_inode *inode)
{
	const struct voluta_inode_xattr *ixa = inode_xattr_of(inode);

	/* TODO: check nodes offsets */

	return ixa_verify(ixa);
}

int voluta_verify_xattr_node(const struct voluta_xattr_node *xan)
{
	int err;

	err = voluta_verify_ino(xan_ino(xan));
	if (err) {
		return err;
	}
	err = xan_verify(xan);
	if (err) {
		return err;
	}
	return 0;
}

