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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "libvoluta.h"

/*
 * TODO-0001: Support setxflags/getxflags ioctls
 *
 * Have support for xflags attributes per inode. Follow XFS' extended flags
 * per inode. At minimum, have support for S_IMMUTABLE of inode. That is, an
 * inode which can not be modified or removed.
 *
 * See kernel's 'xfs_ioc_getxflags/xfs_ioc_setxflags'
 */

/*
 * TODO-0002: Track meta-blocks per inode
 *
 * For each inode (+ entire file-system) track number on meta-blocks.
 * Especially important for deep/sparse dir/file inodes.
 */

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ts_to_cpu(const struct voluta_timespec *vts, struct timespec *ts)
{
	if (ts != NULL) {
		ts->tv_sec = (time_t)le64_to_cpu(vts->t_sec);
		ts->tv_nsec = (long)le64_to_cpu(vts->t_nsec);
	}
}

static void cpu_to_ts(const struct timespec *ts, struct voluta_timespec *vts)
{
	if (ts != NULL) {
		vts->t_sec = cpu_to_le64((uint64_t)ts->tv_sec);
		vts->t_nsec = cpu_to_le64((uint64_t)ts->tv_nsec);
	}
}

static void assign_ts(struct timespec *ts, const struct timespec *other)
{
	ts->tv_sec = other->tv_sec;
	ts->tv_nsec = other->tv_nsec;
}

static void assign_statx_ts(struct statx_timestamp *stx_ts,
			    const struct timespec *ts)
{
	stx_ts->tv_sec = ts->tv_sec;
	stx_ts->tv_nsec = (uint32_t)ts->tv_nsec;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static ino_t inode_ino(const struct voluta_inode *inode)
{
	return cpu_to_ino(inode->i_ino);
}

static void inode_set_ino(struct voluta_inode *inode, ino_t ino)
{
	inode->i_ino = ino_to_cpu(ino);
}

static ino_t inode_parent_ino(const struct voluta_inode *inode)
{
	return cpu_to_ino(inode->i_parent_ino);
}

static void inode_set_parent_ino(struct voluta_inode *inode, ino_t ino)
{
	inode->i_parent_ino = cpu_to_ino(ino);
}

static uid_t inode_uid(const struct voluta_inode *inode)
{
	return le32_to_cpu(inode->i_uid);
}

static void inode_set_uid(struct voluta_inode *inode, uid_t uid)
{
	inode->i_uid = cpu_to_le32(uid);
}

static gid_t inode_gid(const struct voluta_inode *inode)
{
	return le32_to_cpu(inode->i_gid);
}

static void inode_set_gid(struct voluta_inode *inode, uid_t gid)
{
	inode->i_gid = cpu_to_le32(gid);
}

static mode_t inode_mode(const struct voluta_inode *inode)
{
	return le32_to_cpu(inode->i_mode);
}

static void inode_set_mode(struct voluta_inode *inode, mode_t mode)
{
	inode->i_mode = cpu_to_le32(mode);
}

static loff_t inode_size(const struct voluta_inode *inode)
{
	return off_to_cpu(inode->i_size);
}

static void inode_set_size(struct voluta_inode *inode, loff_t off)
{
	inode->i_size = cpu_to_off(off);
}

static blkcnt_t inode_blocks(const struct voluta_inode *inode)
{
	return (blkcnt_t)le64_to_cpu(inode->i_blocks);
}

static void inode_set_blocks(struct voluta_inode *inode, blkcnt_t blocks)
{
	inode->i_blocks = cpu_to_le64((uint64_t)blocks);
}

static nlink_t inode_nlink(const struct voluta_inode *inode)
{
	return le64_to_cpu(inode->i_nlink);
}

static void inode_set_nlink(struct voluta_inode *inode, nlink_t nlink)
{
	inode->i_nlink = cpu_to_le64(nlink);
}

static long inode_revision(const struct voluta_inode *inode)
{
	return (long)le64_to_cpu(inode->i_revision);
}

static void inode_set_revision(struct voluta_inode *inode, long r)
{
	inode->i_revision = cpu_to_le64((uint64_t)r);
}

static void inode_inc_revision(struct voluta_inode *inode)
{
	inode_set_revision(inode, inode_revision(inode) + 1);
}

static enum voluta_inode_flags inode_flags(const struct voluta_inode *inode)
{
	return le32_to_cpu(inode->i_flags);
}

static void inode_set_flags(struct voluta_inode *inode,
			    enum voluta_inode_flags flags)
{
	inode->i_flags = cpu_to_le32(flags);
}

static bool inode_has_flags(struct voluta_inode *inode,
			    enum voluta_inode_flags mask)
{
	return (inode_flags(inode) & mask) == mask;
}

static unsigned int inode_rdev_major(const struct voluta_inode *inode)
{
	return le32_to_cpu(inode->i_rdev_major);
}

static unsigned int inode_rdev_minor(const struct voluta_inode *inode)
{
	return le32_to_cpu(inode->i_rdev_minor);
}

static void inode_set_rdev(struct voluta_inode *inode,
			   unsigned int maj, unsigned int min)
{
	inode->i_rdev_major = cpu_to_le32(maj);
	inode->i_rdev_minor = cpu_to_le32(min);
}

static void inode_btime(const struct voluta_inode *inode, struct timespec *ts)
{
	ts_to_cpu(&inode->i_t.btime, ts);
}

static void inode_set_btime(struct voluta_inode *inode,
			    const struct timespec *ts)
{
	cpu_to_ts(ts, &inode->i_t.btime);
}

static void inode_atime(const struct voluta_inode *inode, struct timespec *ts)
{
	ts_to_cpu(&inode->i_t.atime, ts);
}

static void inode_set_atime(struct voluta_inode *inode,
			    const struct timespec *ts)
{
	cpu_to_ts(ts, &inode->i_t.atime);
}

static void inode_mtime(const struct voluta_inode *inode, struct timespec *ts)
{
	ts_to_cpu(&inode->i_t.mtime, ts);
}

static void inode_set_mtime(struct voluta_inode *inode,
			    const struct timespec *ts)
{
	cpu_to_ts(ts, &inode->i_t.mtime);
}

static void inode_ctime(const struct voluta_inode *inode, struct timespec *ts)
{
	ts_to_cpu(&inode->i_t.ctime, ts);
}

static void inode_set_ctime(struct voluta_inode *inode,
			    const struct timespec *ts)
{
	cpu_to_ts(ts, &inode->i_t.ctime);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

ino_t voluta_xino_of(const struct voluta_inode_info *ii)
{
	return i_isrootd(ii) ? VOLUTA_INO_ROOT : i_ino_of(ii);
}

ino_t voluta_parent_ino_of(const struct voluta_inode_info *ii)
{
	return inode_parent_ino(ii->inode);
}

uid_t voluta_uid_of(const struct voluta_inode_info *ii)
{
	return inode_uid(ii->inode);
}

gid_t voluta_gid_of(const struct voluta_inode_info *ii)
{
	return inode_gid(ii->inode);
}

mode_t voluta_mode_of(const struct voluta_inode_info *ii)
{
	return inode_mode(ii->inode);
}

nlink_t voluta_nlink_of(const struct voluta_inode_info *ii)
{
	return inode_nlink(ii->inode);
}

loff_t voluta_isize_of(const struct voluta_inode_info *ii)
{
	return inode_size(ii->inode);
}

blkcnt_t voluta_blocks_of(const struct voluta_inode_info *ii)
{
	return inode_blocks(ii->inode);
}

static dev_t i_rdev_of(const struct voluta_inode_info *ii)
{
	const struct voluta_inode *inode = ii->inode;

	return makedev(inode_rdev_major(inode), inode_rdev_minor(inode));
}

static unsigned int i_rdev_major_of(const struct voluta_inode_info *ii)
{
	return inode_rdev_major(ii->inode);
}

static unsigned int i_rdev_minor_of(const struct voluta_inode_info *ii)
{
	return inode_rdev_minor(ii->inode);
}

bool voluta_isdir(const struct voluta_inode_info *ii)
{
	return S_ISDIR(i_mode_of(ii));
}

bool voluta_isreg(const struct voluta_inode_info *ii)
{
	return S_ISREG(i_mode_of(ii));
}

bool voluta_islnk(const struct voluta_inode_info *ii)
{
	return S_ISLNK(i_mode_of(ii));
}

bool voluta_isfifo(const struct voluta_inode_info *ii)
{
	return S_ISFIFO(i_mode_of(ii));
}

bool voluta_issock(const struct voluta_inode_info *ii)
{
	return S_ISSOCK(i_mode_of(ii));
}

static ino_t rootd_ino(const struct voluta_sb_info *sbi)
{
	return sbi->s_itable.it_ino_rootd;
}

bool voluta_isrootd(const struct voluta_inode_info *ii)
{
	const struct voluta_sb_info *sbi = i_sbi_of(ii);

	return i_isdir(ii) && (i_ino_of(ii) == rootd_ino(sbi));
}

void voluta_fixup_rootd(struct voluta_inode_info *ii)
{
	struct voluta_inode *inode = ii->inode;

	inode_set_parent_ino(inode, i_ino_of(ii));
	inode_set_nlink(inode, 2);
	inode_set_flags(inode, VOLUTA_INODEF_ROOTD | VOLUTA_INODEF_NAME_UTF8);
}

bool voluta_inode_hasflag(const struct voluta_inode_info *ii,
			  enum voluta_inode_flags mask)
{
	return inode_has_flags(ii->inode, mask);
}

bool voluta_has_iname_utf8(const struct voluta_inode_info *ii)
{
	return voluta_inode_hasflag(ii, VOLUTA_INODEF_NAME_UTF8);
}

bool voluta_is_rootd(const struct voluta_inode_info *ii)
{
	return i_isdir(ii) && inode_has_flags(ii->inode, VOLUTA_INODEF_ROOTD);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_setup_ispecial(struct voluta_inode_info *ii, dev_t rdev)
{
	if (!i_isfifo(ii) && !i_issock(ii)) {
		return -ENOTSUP;
	}
	inode_set_rdev(ii->inode, major(rdev), minor(rdev));
	i_dirtify(ii);
	return 0;
}

/*
 * TODO-0008: Per-inode extra accounting
 *
 * Track number of meta-data bytes allocated per inode.
 *
 *
 * TODO-0010: Store timezone in inode
 */
static void fill_inode(struct voluta_inode *inode, ino_t ino, mode_t mode,
		       const struct voluta_ucred *ucred, ino_t parent_ino)
{
	inode_set_ino(inode, ino);
	inode_set_parent_ino(inode, parent_ino);
	inode_set_uid(inode, ucred->uid);
	inode_set_gid(inode, ucred->gid);
	inode_set_mode(inode, mode & ~ucred->umask);
	inode_set_flags(inode, VOLUTA_INODEF_NAME_UTF8);
	inode_set_size(inode, 0);
	inode_set_blocks(inode, 0);
	inode_set_nlink(inode, 0);
	inode_set_revision(inode, 0);
	voluta_uuid_generate(&inode->i_uuid);
}

void voluta_setup_new_inode(struct voluta_inode_info *ii,
			    const struct voluta_oper_ctx *op_ctx,
			    mode_t mode, ino_t parent_ino)
{
	fill_inode(ii->inode, i_ino_of(ii), mode, &op_ctx->ucred, parent_ino);
	voluta_setup_inode_xattr(ii);
	if (i_isdir(ii)) {
		voluta_setup_new_dir(ii);
	} else if (i_isreg(ii)) {
		voluta_setup_new_reg(ii);
	} else if (i_islnk(ii)) {
		voluta_setup_new_symlnk(ii);
	} else if (i_isfifo(ii) || i_issock(ii)) {
		voluta_setup_ispecial(ii, 0);
	}
	update_itimes(op_ctx, ii, VOLUTA_IATTR_TIMES);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void itimes_setup(struct voluta_itimes *itimes)
{
	ts_now(&itimes->atime);
	ts_now(&itimes->ctime);
	ts_now(&itimes->mtime);
	ts_now(&itimes->btime);
}

static void itimes_copy(struct voluta_itimes *itimes,
			const struct voluta_itimes *other)
{
	ts_copy(&itimes->atime, &other->atime);
	ts_copy(&itimes->ctime, &other->ctime);
	ts_copy(&itimes->mtime, &other->mtime);
	ts_copy(&itimes->btime, &other->btime);
}

static void iattr_set_times(struct voluta_iattr *iattr,
			    const struct voluta_itimes *itimes)
{
	itimes_copy(&iattr->ia_t, itimes);
}

void voluta_iattr_setup(struct voluta_iattr *iattr, ino_t ino)
{
	voluta_memzero(iattr, sizeof(*iattr));
	itimes_setup(&iattr->ia_t);
	iattr->ia_ino = ino;
}

static void iattr_setup3(struct voluta_iattr *iattr, ino_t ino,
			 const struct voluta_itimes *itimes)
{
	iattr_setup(iattr, ino);
	iattr_set_times(iattr, itimes);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static mode_t itype_of(mode_t mode)
{
	return S_IFMT & mode;
}

static bool uid_isnull(uid_t uid)
{
	const uid_t uid_none = (uid_t)(-1);

	return uid_eq(uid, uid_none);
}

static bool gid_isnull(gid_t gid)
{
	const gid_t gid_none = (gid_t)(-1);

	return gid_eq(gid, gid_none);
}

static bool isowner(const struct voluta_ucred *ucred,
		    const struct voluta_inode_info *ii)
{
	return uid_eq(ucred->uid, i_uid_of(ii));
}

static bool capable_fsetid(const struct voluta_ucred *ucred)
{
	/* TODO: CAP_SYS_ADMIN */
	return uid_isroot(ucred->uid);
}

static bool capable_chown(const struct voluta_ucred *ucred)
{
	/* TODO: CAP_CHOWN */
	return uid_isroot(ucred->uid);
}

static bool capable_fowner(const struct voluta_ucred *ucred)
{
	/* TODO: CAP_FOWNER */
	return uid_isroot(ucred->uid);
}

static bool has_itype(const struct voluta_inode_info *ii, mode_t mode)
{
	const mode_t imode = i_mode_of(ii);

	return (itype_of(imode) == itype_of(mode));
}

static int check_raccess(const struct voluta_oper_ctx *op_ctx,
			 const struct voluta_inode_info *ii)
{
	return voluta_do_access(op_ctx, ii, R_OK);
}

static int check_waccess(const struct voluta_oper_ctx *op_ctx,
			 const struct voluta_inode_info *ii)
{
	return voluta_do_access(op_ctx, ii, W_OK);
}

static int check_xaccess_parent(const struct voluta_oper_ctx *op_ctx,
				const struct voluta_inode_info *ii)
{
	int err;
	ino_t parent_ino;
	struct voluta_inode_info *parent_ii = NULL;

	if (!i_isdir(ii) || i_isrootd(ii)) {
		return 0;
	}
	parent_ino = i_parent_ino_of(ii);
	err = voluta_stage_inode(i_sbi_of(ii), parent_ino, &parent_ii);
	if (err) {
		return err;
	}
	if (!i_isdir(parent_ii)) {
		return -EFSCORRUPTED; /* XXX */
	}
	err = voluta_do_access(op_ctx, parent_ii, X_OK);
	if (err) {
		return err;
	}
	return 0;
}

static void i_unset_mode(struct voluta_inode_info *ii, mode_t mask)
{
	inode_set_mode(ii->inode, i_mode_of(ii) & ~mask);
	i_dirtify(ii);
}

static void i_kill_priv(const struct voluta_oper_ctx *op_ctx,
			struct voluta_inode_info *ii)
{
	mode_t mode;
	mode_t mask = 0;
	const struct voluta_ucred *ucred = &op_ctx->ucred;

	if (i_isreg(ii)) {
		mode = i_mode_of(ii);

		if (mode & S_ISUID) {
			mask |= S_ISUID;
		}
		if ((mode & S_ISGID) && (mode & S_IXGRP)) {
			mask |= S_ISGID;
		}
		if (!capable_fsetid(ucred)) {
			i_unset_mode(ii, mask);
		}
	}
}

static mode_t new_mode_of(const struct voluta_inode_info *ii, mode_t mask)
{
	const mode_t fmt_mask = S_IFMT;

	return (i_mode_of(ii) & fmt_mask) | (mask & ~fmt_mask);
}

static int check_chmod(const struct voluta_oper_ctx *op_ctx,
		       struct voluta_inode_info *ii, mode_t mode)
{
	const struct voluta_ucred *ucred = &op_ctx->ucred;

	/* TODO: Check chmod allowed and allow root (or CAP_FOWNER) */
	if (!capable_fowner(ucred) && !isowner(ucred, ii)) {
		return -EPERM;
	}
	/* Must not change inode type */
	if (itype_of(mode) && !has_itype(ii, mode)) {
		return -EPERM;
	}
	return 0;
}

static void update_times_attr(const struct voluta_oper_ctx *op_ctx,
			      struct voluta_inode_info *ii,
			      enum voluta_iattr_flags attr_flags,
			      const struct voluta_itimes *itimes)
{
	struct voluta_iattr iattr;

	iattr_setup(&iattr, i_ino_of(ii));
	memcpy(&iattr.ia_t, itimes, sizeof(iattr.ia_t));
	iattr.ia_flags = attr_flags;
	update_iattrs(op_ctx, ii, &iattr);
}

/*
 * TODO-0013: Allow file-sealing
 *
 * Support special mode for file as read-only permanently (immutable).
 */
static int do_chmod(const struct voluta_oper_ctx *op_ctx,
		    struct voluta_inode_info *ii, mode_t mode,
		    const struct voluta_itimes *itimes)
{
	int err;
	struct voluta_iattr iattr;

	err = check_chmod(op_ctx, ii, mode);
	if (err) {
		return err;
	}
	err = check_xaccess_parent(op_ctx, ii);
	if (err) {
		return err;
	}

	iattr_setup3(&iattr, i_ino_of(ii), itimes);
	iattr.ia_mode = new_mode_of(ii, mode);
	iattr.ia_flags |= VOLUTA_IATTR_MODE | VOLUTA_IATTR_CTIME;
	update_iattrs(op_ctx, ii, &iattr);

	return 0;
}

int voluta_do_chmod(const struct voluta_oper_ctx *op_ctx,
		    struct voluta_inode_info *ii, mode_t mode,
		    const struct voluta_itimes *itimes)
{
	int err;

	i_incref(ii);
	err = do_chmod(op_ctx, ii, mode, itimes);
	i_decref(ii);

	return err;
}

static int check_chown_uid(const struct voluta_oper_ctx *op_ctx,
			   const struct voluta_inode_info *ii, uid_t uid)
{
	const struct voluta_ucred *ucred = &op_ctx->ucred;

	if (uid_eq(uid, i_uid_of(ii))) {
		return 0;
	}
	if (capable_chown(ucred)) {
		return 0;
	}
	return -EPERM;
}

static int check_chown_gid(const struct voluta_oper_ctx *op_ctx,
			   const struct voluta_inode_info *ii, gid_t gid)
{
	const struct voluta_ucred *ucred = &op_ctx->ucred;

	if (gid_eq(gid, i_gid_of(ii))) {
		return 0;
	}
	if (isowner(ucred, ii)) {
		return 0;
	}
	if (capable_chown(ucred)) {
		return 0;
	}
	return -EPERM;
}

static int check_chown(const struct voluta_oper_ctx *op_ctx,
		       const struct voluta_inode_info *ii,
		       uid_t uid, gid_t gid)
{
	int err = 0;

	if (!uid_isnull(uid)) {
		err = check_chown_uid(op_ctx, ii, uid);
	}
	if (!gid_isnull(gid) && !err) {
		err = check_chown_gid(op_ctx, ii, gid);
	}
	return err;
}

static int do_chown(const struct voluta_oper_ctx *op_ctx,
		    struct voluta_inode_info *ii, uid_t uid, gid_t gid,
		    const struct voluta_itimes *itimes)
{
	int err;
	bool chown_uid = !uid_isnull(uid);
	bool chown_gid = !gid_isnull(gid);
	struct voluta_iattr iattr;

	if (!chown_uid && !chown_gid) {
		return 0; /* no-op */
	}
	err = check_chown(op_ctx, ii, uid, gid);
	if (err) {
		return err;
	}
	iattr_setup3(&iattr, i_ino_of(ii), itimes);
	if (chown_uid) {
		iattr.ia_uid = uid;
		iattr.ia_flags |= VOLUTA_IATTR_UID;
	}
	if (chown_gid) {
		iattr.ia_gid = gid;
		iattr.ia_flags |= VOLUTA_IATTR_GID;
	}
	iattr.ia_flags |= VOLUTA_IATTR_KILL_PRIV | VOLUTA_IATTR_CTIME;
	update_iattrs(op_ctx, ii, &iattr);
	return 0;
}

int voluta_do_chown(const struct voluta_oper_ctx *op_ctx,
		    struct voluta_inode_info *ii, uid_t uid, gid_t gid,
		    const struct voluta_itimes *itimes)
{
	int err;

	i_incref(ii);
	err = do_chown(op_ctx, ii, uid, gid, itimes);
	i_decref(ii);

	return err;
}

static bool is_utime_now(const struct timespec *tv)
{
	return (tv->tv_nsec == UTIME_NOW);
}

static bool is_utime_omit(const struct timespec *tv)
{
	return (tv->tv_nsec == UTIME_OMIT);
}

static int check_utimens(const struct voluta_oper_ctx *op_ctx,
			 const struct voluta_inode_info *ii)
{
	int err;

	if (isowner(&op_ctx->ucred, ii)) {
		return 0;
	}
	/* TODO: check VOLUTA_CAPF_FOWNER */
	/* TODO: Follow "Permissions requirements" in UTIMENSAT(2) */
	err = check_waccess(op_ctx, ii);
	if (err) {
		return err;
	}
	return 0;
}

static int do_utimens(const struct voluta_oper_ctx *op_ctx,
		      struct voluta_inode_info *ii,
		      const struct voluta_itimes *itimes)
{
	int err;
	const struct timespec *ctime = &itimes->ctime;
	const struct timespec *atime = &itimes->atime;
	const struct timespec *mtime = &itimes->mtime;

	err = check_utimens(op_ctx, ii);
	if (err) {
		return err;
	}
	if (is_utime_now(atime)) {
		update_itimes(op_ctx, ii, VOLUTA_IATTR_ATIME);
	} else if (!is_utime_omit(atime)) {
		update_times_attr(op_ctx, ii, VOLUTA_IATTR_ATIME, itimes);
	}
	if (is_utime_now(mtime)) {
		update_itimes(op_ctx, ii, VOLUTA_IATTR_MTIME);
	} else if (!is_utime_omit(mtime)) {
		update_times_attr(op_ctx, ii, VOLUTA_IATTR_MTIME, itimes);
	}
	if (!is_utime_omit(ctime)) {
		update_times_attr(op_ctx, ii, VOLUTA_IATTR_CTIME, itimes);
	}
	return 0;
}


int voluta_do_utimens(const struct voluta_oper_ctx *op_ctx,
		      struct voluta_inode_info *ii,
		      const struct voluta_itimes *itimes)
{
	int err;

	i_incref(ii);
	err = do_utimens(op_ctx, ii, itimes);
	i_decref(ii);

	return err;
}

static int check_parent_dir(const struct voluta_inode_info *ii)
{
	int err;
	ino_t parent_ino;
	struct voluta_inode_info *parent_ii = NULL;

	if (!i_isdir(ii) || i_isrootd(ii)) {
		return 0;
	}
	parent_ino = i_parent_ino_of(ii);
	if (ino_isnull(parent_ino)) {
		return ii->i_nopen ? 0 : -ENOENT;
	}
	err = voluta_stage_inode(i_sbi_of(ii), parent_ino, &parent_ii);
	if (err) {
		return err;
	}
	if (!i_isdir(parent_ii)) {
		return -EFSCORRUPTED; /* XXX */
	}
	return 0;
}

/*
 * TODO-0004: Submit a patch to Linux kernel which support readdir of
 * multiple pages, possible using 'st_blksize' as hint.
 *
 * As of glibc-2.28 'opendir' uses 'st_blksize' as a hint to for size
 * of internal allocated buffer of 'DIR', which in turn passed to
 * 'getdents' system call. Unfortunately, currently FUSE chops readdir
 * into single page iterations.
 */
static blksize_t stat_blksize_of(const struct voluta_inode_info *ii)
{
	return i_isdir(ii) ? VOLUTA_BK_SIZE : VOLUTA_DS_SIZE;
}

static blkcnt_t stat_blocks_of(const struct voluta_inode_info *ii)
{
	blkcnt_t nbytes;
	const blkcnt_t align = 512;
	const blkcnt_t ds_size = (blkcnt_t)(VOLUTA_DS_SIZE);

	nbytes = ds_size * i_blocks_of(ii);
	return (nbytes + align - 1) / align;
}

static void i_stat(const struct voluta_inode_info *ii, struct stat *st)
{
	voluta_memzero(st, sizeof(*st));
	st->st_ino = i_xino_of(ii);
	st->st_mode = i_mode_of(ii);
	st->st_nlink = i_nlink_of(ii);
	st->st_uid = i_uid_of(ii);
	st->st_gid = i_gid_of(ii);
	st->st_rdev = i_rdev_of(ii);
	st->st_size = i_size_of(ii);
	st->st_blocks = stat_blocks_of(ii);
	st->st_blksize = stat_blksize_of(ii);
	assign_ts(&st->st_atim, &ii->i_atime_lazy);
	inode_mtime(ii->inode, &st->st_mtim);
	inode_ctime(ii->inode, &st->st_ctim);
}

static void i_statx(const struct voluta_inode_info *ii, struct statx *stx)
{
	struct timespec ts;

	voluta_memzero(stx, sizeof(*stx));
	stx->stx_mask = STATX_ALL;
	stx->stx_blksize = VOLUTA_BK_SIZE;
	stx->stx_nlink = (uint32_t)i_nlink_of(ii);
	stx->stx_uid = i_uid_of(ii);
	stx->stx_gid = i_gid_of(ii);
	stx->stx_mode = (uint16_t)i_mode_of(ii);
	stx->stx_ino = i_xino_of(ii);
	stx->stx_size = (uint64_t)i_size_of(ii);
	stx->stx_blocks = (uint64_t)stat_blocks_of(ii);
	stx->stx_blksize = (uint32_t)stat_blksize_of(ii);
	stx->stx_rdev_minor =  i_rdev_minor_of(ii);
	stx->stx_rdev_major =  i_rdev_major_of(ii);

	stx->stx_attributes_mask = STATX_ATTR_IMMUTABLE | STATX_ATTR_ENCRYPTED;
	stx->stx_attributes = STATX_ATTR_ENCRYPTED;

	inode_btime(ii->inode, &ts);
	assign_statx_ts(&stx->stx_btime, &ts);
	assign_statx_ts(&stx->stx_atime, &ii->i_atime_lazy);
	inode_mtime(ii->inode, &ts);
	assign_statx_ts(&stx->stx_mtime, &ts);
	inode_ctime(ii->inode, &ts);
	assign_statx_ts(&stx->stx_ctime, &ts);
}

/*
 * TODO-0016: Support strict-access mode
 *
 * Have special mode where only root & self may read inode's attributes.
 */
static int check_getattr(const struct voluta_oper_ctx *op_ctx,
			 const struct voluta_inode_info *ii)
{
	int err;

	voluta_unused(op_ctx);

	err = check_parent_dir(ii);
	if (err) {
		return err;
	}
	return 0;
}

static int do_getattr(const struct voluta_oper_ctx *op_ctx,
		      const struct voluta_inode_info *ii,
		      struct stat *out_st)
{
	int err;

	err = check_getattr(op_ctx, ii);
	if (err) {
		return err;
	}
	i_stat(ii, out_st);
	return 0;
}

int voluta_do_getattr(const struct voluta_oper_ctx *op_ctx,
		      const struct voluta_inode_info *ii,
		      struct stat *out_st)
{
	int err;

	i_incref(ii);
	err = do_getattr(op_ctx, ii, out_st);
	i_decref(ii);

	return err;
}

static int do_statx(const struct voluta_oper_ctx *op_ctx,
		    const struct voluta_inode_info *ii,
		    struct statx *out_stx)
{
	int err;

	err = check_getattr(op_ctx, ii);
	if (err) {
		return err;
	}
	i_statx(ii, out_stx);
	return 0;
}

int voluta_do_statx(const struct voluta_oper_ctx *op_ctx,
		    const struct voluta_inode_info *ii,
		    struct statx *out_stx)
{
	int err;

	i_incref(ii);
	err = do_statx(op_ctx, ii, out_stx);
	i_decref(ii);

	return err;
}

static void fill_inquiry(struct voluta_inquiry *inq)
{
	inq->version = VOLUTA_VERSION;
	/* TODO: fill more */
}

static int do_inquiry(const struct voluta_oper_ctx *op_ctx,
		      const struct voluta_inode_info *ii,
		      struct voluta_inquiry *out_inq)
{
	int err;

	err = check_raccess(op_ctx, ii);
	if (err) {
		return err;
	}
	fill_inquiry(out_inq);
	return 0;
}

int voluta_do_inquiry(const struct voluta_oper_ctx *op_ctx,
		      const struct voluta_inode_info *ii,
		      struct voluta_inquiry *out_inq)
{
	int err;

	i_incref(ii);
	err = do_inquiry(op_ctx, ii, out_inq);
	i_decref(ii);

	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct timespec *time_now(const struct voluta_oper_ctx *op_ctx)
{
	return &op_ctx->xtime.now;
}

static const struct timespec *
timespec_of(const struct voluta_oper_ctx *op_ctx, const struct timespec *ts_in)
{
	const struct timespec *ts = ts_in;

	voluta_assert_not_null(ts_in);
	if (ts_in->tv_nsec == UTIME_NOW) {
		ts = time_now(op_ctx);
	} else if (ts_in->tv_nsec == UTIME_OMIT) {
		ts = NULL;
	}
	return ts;
}

static void i_update_atime(struct voluta_inode_info *ii,
			   const struct timespec *atime)
{
	if (atime != NULL) {
		memcpy(&ii->i_atime_lazy, atime, sizeof(ii->i_atime_lazy));
	}
}

static void update_inode_attr(struct voluta_inode_info *ii,
			      const struct voluta_oper_ctx *op_ctx,
			      enum voluta_iattr_flags attr_flags,
			      const struct voluta_iattr *iattr)
{
	long flags = (long)attr_flags;
	struct voluta_inode *inode;
	const struct timespec *ts;

	if (ii == NULL) {
		return; /* e.g., rename */
	}
	if (flags & (VOLUTA_IATTR_LAZY | VOLUTA_IATTR_ATIME)) {
		ts = timespec_of(op_ctx, &iattr->ia_t.atime);
		i_update_atime(ii, ts);
		flags &= ~VOLUTA_IATTR_ATIME;
	}
	flags &= ~VOLUTA_IATTR_LAZY;
	if (!flags) {
		return;
	}
	inode = ii->inode;
	if (flags & VOLUTA_IATTR_PARENT) {
		inode_set_parent_ino(inode, iattr->ia_parent_ino);
	}
	if (flags & VOLUTA_IATTR_SIZE) {
		inode_set_size(inode, iattr->ia_size);
	}
	if (flags & VOLUTA_IATTR_BLOCKS) {
		inode_set_blocks(inode, iattr->ia_blocks);
	}
	if (flags & VOLUTA_IATTR_NLINK) {
		voluta_assert_lt(iattr->ia_nlink, UINT_MAX);
		inode_set_nlink(inode, iattr->ia_nlink);
	}
	if (flags & VOLUTA_IATTR_MODE) {
		inode_set_mode(inode, iattr->ia_mode);
	}
	if (flags & VOLUTA_IATTR_UID) {
		inode_set_uid(inode, iattr->ia_uid);
	}
	if (flags & VOLUTA_IATTR_GID) {
		inode_set_gid(inode, iattr->ia_gid);
	}
	if (flags & VOLUTA_IATTR_BTIME) {
		ts = timespec_of(op_ctx, &iattr->ia_t.btime);
		inode_set_btime(inode, ts);
	}
	if (flags & VOLUTA_IATTR_MTIME) {
		ts = timespec_of(op_ctx, &iattr->ia_t.mtime);
		inode_set_mtime(inode, ts);
	}
	if (flags & VOLUTA_IATTR_CTIME) {
		ts = timespec_of(op_ctx, &iattr->ia_t.ctime);
		inode_set_ctime(inode, ts);
	}
	if (flags & VOLUTA_IATTR_ATIME) {
		ts = timespec_of(op_ctx, &iattr->ia_t.atime);
		inode_set_atime(inode, ts);
		voluta_refresh_atime(ii, true);
	} else if (flags & VOLUTA_IATTR_TIMES) {
		voluta_refresh_atime(ii, false);
	}
	if (flags & VOLUTA_IATTR_KILL_PRIV) {
		i_kill_priv(op_ctx, ii);
	}
	inode_inc_revision(inode);
	i_dirtify(ii);
}

void voluta_update_iattrs(const struct voluta_oper_ctx *op_ctx,
			  struct voluta_inode_info *ii,
			  const struct voluta_iattr *iattr)
{
	update_inode_attr(ii, op_ctx, iattr->ia_flags, iattr);
}

void voluta_update_itimes(const struct voluta_oper_ctx *op_ctx,
			  struct voluta_inode_info *ii,
			  enum voluta_iattr_flags attr_flags)
{
	struct voluta_iattr iattr;
	const enum voluta_iattr_flags mask = VOLUTA_IATTR_TIMES;

	iattr_setup(&iattr, i_ino_of(ii));
	update_inode_attr(ii, op_ctx, attr_flags & mask, &iattr);
}

void voluta_refresh_atime(struct voluta_inode_info *ii, bool to_volatile)
{
	if (to_volatile) {
		inode_atime(ii->inode, &ii->i_atime_lazy);
	} else {
		inode_set_atime(ii->inode, &ii->i_atime_lazy);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int verify_inode_specific(const struct voluta_inode *inode)
{
	int err;
	const mode_t mode = inode_mode(inode);

	if (S_ISDIR(mode)) {
		err = voluta_verify_dir_inode(inode);
	} else {
		/* TODO: ALL type */
		err = 0;
	}
	return err;
}

int voluta_verify_inode(const struct voluta_inode *inode)
{
	int err;

	err = voluta_verify_ino(inode_ino(inode));
	if (err) {
		return err;
	}
	err = voluta_verify_inode_xattr(inode);
	if (err) {
		return err;
	}
	err = verify_inode_specific(inode);
	if (err) {
		return err;
	}
	return err;
}

