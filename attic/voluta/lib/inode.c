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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "voluta-lib.h"

/*
 * ISSUE-0001 (TODO): Support setxflags/getxflags ioctls
 *
 * Have support for xflags attributes per inode. Follow XFS' extended flags
 * per inode. At minimum, have support for S_IMMUTABLE of inode. That is, an
 * inode which can not be modified or removed.
 *
 * See kernel's 'xfs_ioc_getxflags/xfs_ioc_setxflags'
 */

/*
 * ISSUE-0002 (TODO): Track meta-blocks per inode
 *
 * For each inode (+ entire file-system) track number on meta-blocks.
 * Especially important for deep/sparse dir/file inodes.
 */


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const struct voluta_vaddr *
voluta_vaddr_of_vi(const struct voluta_vnode_info *vi)
{
	return &vi->vaddr;
}

const struct voluta_vaddr *
voluta_vaddr_of_ii(const struct voluta_inode_info *ii)
{
	return voluta_vaddr_of_vi(&ii->vi);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

ino_t voluta_ino_of(const struct voluta_inode_info *ii)
{
	return ii->ino;
}

ino_t voluta_xino_of(const struct voluta_inode_info *ii)
{
	return i_isrootd(ii) ? VOLUTA_INO_ROOT : i_ino_of(ii);
}

ino_t voluta_parent_ino_of(const struct voluta_inode_info *ii)
{
	return ii->inode->i_parent_ino;
}

uid_t voluta_uid_of(const struct voluta_inode_info *ii)
{
	return ii->inode->i_uid;
}

gid_t voluta_gid_of(const struct voluta_inode_info *ii)
{
	return ii->inode->i_gid;
}

mode_t voluta_mode_of(const struct voluta_inode_info *ii)
{
	return ii->inode->i_mode;
}

nlink_t voluta_nlink_of(const struct voluta_inode_info *ii)
{
	return ii->inode->i_nlink;
}

loff_t voluta_isize_of(const struct voluta_inode_info *ii)
{
	return (loff_t)(ii->inode->i_size);
}

blkcnt_t voluta_blocks_of(const struct voluta_inode_info *ii)
{
	return (blkcnt_t)(ii->inode->i_blocks);
}

dev_t voluta_rdev_of(const struct voluta_inode_info *ii)
{
	return makedev(ii->inode->i_rdev_major, ii->inode->i_rdev_minor);
}

static unsigned int i_rdev_major_of(const struct voluta_inode_info *ii)
{
	return ii->inode->i_rdev_major;
}

static unsigned int i_rdev_minor_of(const struct voluta_inode_info *ii)
{
	return ii->inode->i_rdev_minor;
}

size_t voluta_nxattr_of(const struct voluta_inode_info *ii)
{
	return ii->inode->i_nxattr;
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

static struct voluta_sb_info *i_sbi(const struct voluta_inode_info *ii)
{
	return ii->vi.bki->b_sbi;
}

static ino_t rootd_ino(const struct voluta_sb_info *sbi)
{
	return sbi->s_itable.it_ino_rootd;
}

bool voluta_isrootd(const struct voluta_inode_info *ii)
{
	const struct voluta_sb_info *sbi = i_sbi(ii);

	return i_isdir(ii) && (i_ino_of(ii) == rootd_ino(sbi));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void assign_timespec(struct voluta_timespec *vts,
			    const struct timespec *ts)
{
	if (ts != NULL) {
		vts->sec = (uint64_t)ts->tv_sec;
		vts->nsec = (uint64_t)ts->tv_nsec;
	}
}

static void extern_timespec(const struct voluta_timespec *vts,
			    struct timespec *ts)
{
	ts->tv_sec = (time_t)vts->sec;
	ts->tv_nsec = (long)vts->nsec;
}

static void extern_timespec_x(const struct voluta_timespec *vts,
			      struct statx_timestamp *stx_ts)
{
	stx_ts->tv_sec = (int64_t)vts->sec;
	stx_ts->tv_nsec = (uint32_t)vts->nsec;
}

static void setup_new_special(struct voluta_inode_info *ii)
{
	ii->inode->i_rdev_major = 0; /* XXX */
	ii->inode->i_rdev_minor = 0; /* XXX */
}

static void setup_new_fifo(struct voluta_inode_info *ii)
{
	setup_new_special(ii);
}

static void setup_new_sock(struct voluta_inode_info *ii)
{
	setup_new_special(ii);
}


static void fill_new_inode(struct voluta_inode_info *ii, ino_t ino, mode_t mode,
			   const struct voluta_ucred *ucred, ino_t parent_ino)
{
	struct voluta_inode *inode = ii->inode;

	inode->i_ino = (uint64_t)ino;
	inode->i_parent_ino = (uint64_t)parent_ino;
	inode->i_size = 0;
	inode->i_blocks = 0;
	inode->i_nlink = 0;
	inode->i_version = 0;
	inode->i_revision = 0;
	inode->i_mode = (uint32_t)(mode & ~ucred->umask);
	inode->i_flags = 0;
	inode->i_uid = (uint32_t)ucred->uid;
	inode->i_gid = (uint32_t)ucred->gid;
	voluta_uuid_generate(&inode->i_uuid);

	for (size_t i = 0; i < ARRAY_SIZE(inode->i_xattr_off); ++i) {
		inode->i_xattr_off[i] = VOLUTA_OFF_NULL;
	}
}

static void zero_stamp_inode(struct voluta_inode_info *ii)
{
	voluta_assert_eq(ii->vi.vaddr.vtype, VOLUTA_VTYPE_INODE);
	voluta_stamp_vnode(&ii->vi);
}

int voluta_setup_new_inode(struct voluta_env *env, struct voluta_inode_info *ii,
			   mode_t mode, ino_t parent_ino)
{
	zero_stamp_inode(ii);
	fill_new_inode(ii, i_ino_of(ii), mode, &env->ucred, parent_ino);

	if (i_isdir(ii)) {
		voluta_setup_new_dir(ii);
	} else if (i_isreg(ii)) {
		voluta_setup_new_reg(ii);
	} else if (i_islnk(ii)) {
		voluta_setup_new_symlnk(ii);
	} else if (i_isfifo(ii)) {
		setup_new_fifo(ii);
	} else if (i_issock(ii)) {
		setup_new_sock(ii);
	} else {
		return -ENOTSUP;
	}
	i_update_times(env, ii, VOLUTA_ATTR_TIMES);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

bool voluta_uid_eq(uid_t uid1, uid_t uid2)
{
	return (uid1 == uid2);
}

bool voluta_gid_eq(gid_t gid1, gid_t gid2)
{
	return (gid1 == gid2);
}

bool voluta_uid_isroot(uid_t uid)
{
	return uid_eq(uid, 0);
}

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

static int require_xaccess_parent_dir(struct voluta_env *env,
				      const struct voluta_inode_info *ii)
{
	int err;
	ino_t parent_ino;
	struct voluta_inode_info *parent_ii = NULL;

	if (!i_isdir(ii) || i_isrootd(ii)) {
		return 0;
	}
	parent_ino = i_parent_ino_of(ii);
	err = voluta_stage_inode(sbi_of(env), parent_ino, &parent_ii);
	if (err) {
		return err;
	}
	if (!i_isdir(parent_ii)) {
		return -EFSCORRUPTED; /* XXX */
	}
	err = voluta_do_access(env, parent_ii, X_OK);
	if (err) {
		return err;
	}
	return 0;
}

static void i_unset_mode(struct voluta_inode_info *ii, mode_t mask)
{
	struct voluta_inode *inode = ii->inode;

	inode->i_mode = i_mode_of(ii) & ~mask;
	i_dirtify(ii);
}

static void i_kill_priv(struct voluta_env *env, struct voluta_inode_info *ii)
{
	mode_t mask, mode;
	const struct voluta_ucred *ucred = &env->ucred;

	if (i_isreg(ii)) {
		mask = 0;
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

static void change_mode_of(struct voluta_inode_info *ii, mode_t mask)
{
	const mode_t fmt_mask = S_IFMT;
	struct voluta_inode *inode = ii->inode;

	inode->i_mode = (i_mode_of(ii) & fmt_mask) | (mask & ~fmt_mask);
}

static int check_chmod(struct voluta_env *env, struct voluta_inode_info *ii,
		       mode_t mode)
{
	const struct voluta_ucred *ucred = &env->ucred;

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

static void i_update_times_attr(struct voluta_env *env,
				struct voluta_inode_info *ii,
				enum voluta_attr_flags attr_flags,
				const struct voluta_iattr *ts_attr)
{
	struct voluta_iattr attr;

	memcpy(&attr, ts_attr, sizeof(attr));
	attr.ia_flags = attr_flags;
	voluta_update_iattr(env, ii, &attr);
}

int voluta_do_chmod(struct voluta_env *env, struct voluta_inode_info *ii,
		    mode_t mode, const struct voluta_iattr *attr)
{
	int err;

	err = check_chmod(env, ii, mode);
	if (err) {
		return err;
	}
	err = require_xaccess_parent_dir(env, ii);
	if (err) {
		return err;
	}
	change_mode_of(ii, mode);
	i_update_times_attr(env, ii, VOLUTA_ATTR_CTIME, attr);
	return 0;
}

static int check_chown_uid(struct voluta_env *env, uid_t uid,
			   const struct voluta_inode_info *ii)
{
	const struct voluta_ucred *ucred = &env->ucred;

	if (uid_eq(uid, i_uid_of(ii))) {
		return 0;
	}
	if (capable_chown(ucred)) {
		return 0;
	}
	return -EPERM;
}

static int check_chown_gid(struct voluta_env *env, gid_t gid,
			   const struct voluta_inode_info *ii)
{
	const struct voluta_ucred *ucred = &env->ucred;

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

int voluta_do_chown(struct voluta_env *env, struct voluta_inode_info *ii,
		    uid_t uid, gid_t gid, const struct voluta_iattr *attr)
{
	int err, chown_uid = 0, chown_gid = 0;
	enum voluta_attr_flags attr_flags;
	struct voluta_inode *inode = ii->inode;

	if (!uid_isnull(uid)) {
		err = check_chown_uid(env, uid, ii);
		if (err) {
			return err;
		}
		chown_uid = 1;
	}
	if (!gid_isnull(gid)) {
		err = check_chown_gid(env, gid, ii);
		if (err) {
			return err;
		}
		chown_gid = 1;
	}
	if (!chown_uid && !chown_gid) {
		return 0; /* no-op */
	}
	if (chown_uid) {
		inode->i_uid = uid;
	}
	if (chown_gid) {
		inode->i_gid = gid;
	}
	attr_flags = VOLUTA_ATTR_KILL_PRIV | VOLUTA_ATTR_CTIME;
	i_update_times_attr(env, ii, attr_flags, attr);
	return 0;
}

static bool is_utime_now(const struct timespec *tv)
{
	return (tv->tv_nsec == UTIME_NOW);
}

static bool is_utime_omit(const struct timespec *tv)
{
	return (tv->tv_nsec == UTIME_OMIT);
}

static int check_utimens(struct voluta_env *env,
			 const struct voluta_inode_info *ii)
{
	int err;
	const struct voluta_ucred *ucred = &env->ucred;

	if (isowner(ucred, ii)) {
		return 0;
	}
	/* TODO: check VOLUTA_CAPF_FOWNER */
	/* TODO: Follow "Permissions requirements" in UTIMENSAT(2) */
	err = voluta_do_access(env, ii, W_OK);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_do_utimens(struct voluta_env *env, struct voluta_inode_info *ii,
		      const struct voluta_iattr *attr)
{
	int err;
	const struct timespec *ctime = &attr->ia_ctime;
	const struct timespec *atime = &attr->ia_atime;
	const struct timespec *mtime = &attr->ia_mtime;

	err = check_utimens(env, ii);
	if (err) {
		return err;
	}
	if (is_utime_now(atime)) {
		i_update_times(env, ii, VOLUTA_ATTR_ATIME);
	} else if (!is_utime_omit(atime)) {
		i_update_times_attr(env, ii, VOLUTA_ATTR_ATIME, attr);
	}
	if (is_utime_now(mtime)) {
		i_update_times(env, ii, VOLUTA_ATTR_MTIME);
	} else if (!is_utime_omit(mtime)) {
		i_update_times_attr(env, ii, VOLUTA_ATTR_MTIME, attr);
	}
	if (!is_utime_omit(ctime)) {
		i_update_times_attr(env, ii, VOLUTA_ATTR_CTIME, attr);
	}
	return 0;
}

static blkcnt_t i_s_blocks_of(const struct voluta_inode_info *ii)
{
	blkcnt_t nbytes;
	const blkcnt_t align = 512;
	const blkcnt_t bk_size = (blkcnt_t)(VOLUTA_BK_SIZE);

	nbytes = bk_size * i_blocks_of(ii);
	return (nbytes + align - 1) / align;
}

static int require_parent_dir(struct voluta_env *env,
			      const struct voluta_inode_info *ii)
{
	int err;
	ino_t parent_ino;
	struct voluta_inode_info *parent_ii = NULL;

	if (!i_isdir(ii) || i_isrootd(ii)) {
		return 0;
	}
	parent_ino = i_parent_ino_of(ii);
	if (!parent_ino) {
		return -EFSCORRUPTED; /* XXX */
	}
	err = voluta_stage_inode(sbi_of(env), parent_ino, &parent_ii);
	if (err) {
		return err;
	}
	if (!i_isdir(parent_ii)) {
		return -EFSCORRUPTED; /* XXX */
	}
	return 0;
}

static void i_extern_attrs(const struct voluta_inode_info *ii, struct stat *st)
{
	const struct voluta_itimes *itimes = &ii->inode->i_times;

	memset(st, 0, sizeof(*st));
	st->st_ino = i_xino_of(ii);
	st->st_mode = i_mode_of(ii);
	st->st_nlink = i_nlink_of(ii);
	st->st_uid = i_uid_of(ii);
	st->st_gid = i_gid_of(ii);
	st->st_rdev = i_rdev_of(ii);
	st->st_size = i_size_of(ii);
	st->st_blksize = VOLUTA_BK_SIZE;
	st->st_blocks = i_s_blocks_of(ii);
	extern_timespec(&itimes->atime, &st->st_atim);
	extern_timespec(&itimes->mtime, &st->st_mtim);
	extern_timespec(&itimes->ctime, &st->st_ctim);
}

int voluta_do_getattr(struct voluta_env *env,
		      const struct voluta_inode_info *ii,
		      struct stat *out_stat)
{
	int err;

	err = require_parent_dir(env, ii);
	if (err) {
		return err;
	}
	i_extern_attrs(ii, out_stat);
	return 0;
}

static void i_extern_attrs_x(const struct voluta_inode_info *ii,
			     struct statx *stx)
{
	const struct voluta_itimes *itimes = &ii->inode->i_times;

	memset(stx, 0, sizeof(*stx));
	stx->stx_mask = STATX_ALL;
	stx->stx_blksize = VOLUTA_BK_SIZE;
	stx->stx_attributes = 0; /* TODO: support STATX_ATTR_IMMUTABLE */
	stx->stx_nlink = (uint32_t)i_nlink_of(ii);
	stx->stx_uid = i_uid_of(ii);
	stx->stx_gid = i_gid_of(ii);
	stx->stx_mode = (uint16_t)i_mode_of(ii);
	stx->stx_ino = i_xino_of(ii);
	stx->stx_size = (uint64_t)i_size_of(ii);
	stx->stx_blocks = (uint64_t)i_s_blocks_of(ii);
	stx->stx_attributes_mask = STATX_ATTR_IMMUTABLE;
	extern_timespec_x(&itimes->atime, &stx->stx_atime);
	extern_timespec_x(&itimes->btime, &stx->stx_btime);
	extern_timespec_x(&itimes->ctime, &stx->stx_ctime);
	extern_timespec_x(&itimes->mtime, &stx->stx_mtime);
	stx->stx_rdev_minor =  i_rdev_minor_of(ii);
	stx->stx_rdev_major =  i_rdev_major_of(ii);
}

int voluta_do_statx(struct voluta_env *env, const struct voluta_inode_info *ii,
		    struct statx *out_statx)
{
	int err;

	err = require_parent_dir(env, ii);
	if (err) {
		return err;
	}
	i_extern_attrs_x(ii, out_statx);

	return 0;
}


int voluta_do_evict_inode(struct voluta_sb_info *sbi, ino_t ino)
{
	int err;
	struct voluta_inode_info *ii;

	err = voluta_real_ino(sbi, ino, &ino);
	if (err) {
		return err;
	}
	err = voluta_cache_lookup_ii(sbi->s_cache, ino, &ii);
	if (err) {
		return 0; /* not cached, ok */
	}
	if (!voluta_isevictable_ii(ii)) {
		return -EPERM; /* XXX may happen when (nlookup > 0) */
	}
	voulta_cache_forget_ii(sbi->s_cache, ii);
	return 0;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct timespec *time_now(struct voluta_env *env)
{
	int err;
	const clockid_t clk = CLOCK_REALTIME;
	struct timespec *ts = &env->xtime.now;

	if (ts->tv_sec == 0) {
		err = voluta_sys_clock_gettime(clk, ts);
		if (err) {
			voluta_panic("clk=%ld err=%d", (long)clk, err);
		}
	}
	return ts;
}

static const struct timespec *
timespec_from_attr(struct voluta_env *env, const struct timespec *ts_in)
{
	const struct timespec *ts = ts_in;

	voluta_assert_not_null(ts_in);
	if (ts_in->tv_nsec == UTIME_NOW) {
		ts = time_now(env);
	} else if (ts_in->tv_nsec == UTIME_OMIT) {
		ts = NULL;
	}
	return ts;
}

static void
update_inode_attr(struct voluta_env *env, struct voluta_inode_info *ii,
		  enum voluta_attr_flags flags, const struct voluta_iattr *attr)
{
	struct voluta_inode *inode;
	const struct timespec *ts;

	voluta_assert_not_null(attr);
	if ((ii == NULL) || (flags == 0)) {
		return; /* e.g., rename */
	}
	inode = ii->inode;
	if (flags & VOLUTA_ATTR_PARENT) {
		inode->i_parent_ino = attr->ia_parent_ino;
	}
	if (flags & VOLUTA_ATTR_SIZE) {
		inode->i_size = (uint64_t)(attr->ia_size);
	}
	if (flags & VOLUTA_ATTR_NLINK) {
		inode->i_nlink = attr->ia_nlink;
	}
	if (flags & VOLUTA_ATTR_BLOCKS) {
		inode->i_blocks = (uint64_t)(attr->ia_blocks);
	}
	if (flags & VOLUTA_ATTR_BTIME) {
		ts = timespec_from_attr(env, &attr->ia_btime);
		assign_timespec(&inode->i_times.btime, ts);
	}
	if (flags & VOLUTA_ATTR_ATIME) {
		ts = timespec_from_attr(env, &attr->ia_atime);
		assign_timespec(&inode->i_times.atime, ts);
	}
	if (flags & VOLUTA_ATTR_MTIME) {
		ts = timespec_from_attr(env, &attr->ia_mtime);
		assign_timespec(&inode->i_times.mtime, ts);
	}
	if (flags & VOLUTA_ATTR_CTIME) {
		ts = timespec_from_attr(env, &attr->ia_ctime);
		assign_timespec(&inode->i_times.ctime, ts);
	}
	if (flags & VOLUTA_ATTR_KILL_PRIV) {
		i_kill_priv(env, ii);
	}
	if (flags & VOLUTA_ATTR_REVISION) {
		inode->i_revision += 1;
	}
	if (flags & VOLUTA_ATTR_LAZY) {
		return; /* When update atime in memory only ("lazytime") */
	}
	inode->i_version += 1;
	i_dirtify(ii);
}

void voluta_update_iattr(struct voluta_env *env,
			 struct voluta_inode_info *ii,
			 const struct voluta_iattr *attr)
{
	update_inode_attr(env, ii, attr->ia_flags, attr);
}

void voluta_update_itimes(struct voluta_env *env, struct voluta_inode_info *ii,
			  enum voluta_attr_flags attr_flags)
{
	const struct voluta_iattr iattr = {
		.ia_btime.tv_nsec = UTIME_NOW,
		.ia_atime.tv_nsec = UTIME_NOW,
		.ia_mtime.tv_nsec = UTIME_NOW,
		.ia_ctime.tv_nsec = UTIME_NOW,
	};
	const enum voluta_attr_flags mask =
		VOLUTA_ATTR_TIMES | VOLUTA_ATTR_REVISION | VOLUTA_ATTR_VERSION;

	update_inode_attr(env, ii, attr_flags & mask, &iattr);
}

void voluta_iattr_reset(struct voluta_iattr *iattr)
{
	memset(iattr, 0, sizeof(*iattr));
	iattr->ia_btime.tv_nsec = UTIME_NOW;
	iattr->ia_atime.tv_nsec = UTIME_NOW;
	iattr->ia_mtime.tv_nsec = UTIME_NOW;
	iattr->ia_ctime.tv_nsec = UTIME_NOW;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_verify_inode(const struct voluta_inode *inode)
{
	int err;
	const mode_t mode = inode->i_mode;

	err = voluta_verify_ino(inode->i_ino);
	if (err) {
		return err;
	}
	if (S_ISDIR(mode)) {
		if (inode->i_hdr.size != sizeof(struct voluta_dir_inode)) {
			return -EFSCORRUPTED;
		}
		err = voluta_verify_dir_inode(inode);
	} else {
		/* TODO: ALL type */
		err = 0;
	}
	return err;
}

