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
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include "libvoluta.h"


static bool i_hasflag(const struct voluta_inode_info *ii,
		      enum voluta_inode_flags mask)
{
	return voluta_inode_hasflag(ii, mask);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool isowner(const struct voluta_oper_ctx *op_ctx,
		    const struct voluta_inode_info *ii)
{
	return uid_eq(op_ctx->ucred.uid, i_uid_of(ii));
}

static int check_isdir(const struct voluta_inode_info *ii)
{
	return i_isdir(ii) ? 0 : -ENOTDIR;
}

static int check_notdir(const struct voluta_inode_info *ii)
{
	return i_isdir(ii) ? -EISDIR : 0;
}

static int check_opened(const struct voluta_inode_info *ii)
{
	return !ii->i_nopen ? -EBADF : 0;
}

static int check_reg_or_fifo(const struct voluta_inode_info *ii)
{
	if (i_isdir(ii)) {
		return -EISDIR;
	}
	if (!i_isreg(ii) && !i_isfifo(ii)) {
		return -EINVAL;
	}
	return 0;
}

static int check_open_limit(const struct voluta_inode_info *ii)
{
	const int i_open_max = INT_MAX / 2;
	const struct voluta_sb_info *sbi = i_sbi_of(ii);

	if (!ii->i_nopen && !(sbi->s_iopen < sbi->s_iopen_limit)) {
		return -ENFILE;
	}
	if (ii->i_nopen >= i_open_max) {
		return -ENFILE;
	}
	return 0;
}

static void update_nopen(struct voluta_inode_info *ii, int n)
{
	struct voluta_sb_info *sbi = i_sbi_of(ii);

	voluta_assert_ge(ii->i_nopen + n, 0);
	voluta_assert_lt(ii->i_nopen + n, INT_MAX);

	if ((n > 0) && (ii->i_nopen == 0)) {
		sbi->s_iopen++;
	} else if ((n < 0) && (ii->i_nopen == 1)) {
		sbi->s_iopen--;
	}
	ii->i_nopen += n;
}

static bool has_sticky_bit(const struct voluta_inode_info *dir_ii)
{
	const mode_t mode = i_mode_of(dir_ii);

	return ((mode & S_ISVTX) == S_ISVTX);
}

static int check_sticky(const struct voluta_oper_ctx *op_ctx,
			const struct voluta_inode_info *dir_ii,
			const struct voluta_inode_info *ii)
{

	if (!has_sticky_bit(dir_ii)) {
		return 0; /* No sticky-bit, we're fine */
	}
	if (isowner(op_ctx, dir_ii)) {
		return 0;
	}
	if (ii && isowner(op_ctx, ii)) {
		return 0;
	}
	/* TODO: Check CAP_FOWNER */
	return -EPERM;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int new_inode(const struct voluta_oper_ctx *op_ctx,
		     const struct voluta_inode_info *parent_dir,
		     mode_t mode, struct voluta_inode_info **out_ii)
{
	const ino_t parent_ino = i_ino_of(parent_dir);
	struct voluta_sb_info *sbi = i_sbi_of(parent_dir);

	return voluta_new_inode(sbi, op_ctx, mode, parent_ino, out_ii);
}

static int new_dir_inode(const struct voluta_oper_ctx *op_ctx,
			 const struct voluta_inode_info *parent_dir,
			 mode_t mode, struct voluta_inode_info **out_ii)
{
	const mode_t ifmt = S_IFMT;
	const mode_t dir_mode = (mode & ~ifmt) | S_IFDIR;

	return new_inode(op_ctx, parent_dir, dir_mode, out_ii);
}

static int new_reg_inode(const struct voluta_oper_ctx *op_ctx,
			 const struct voluta_inode_info *parent_dir,
			 mode_t mode, struct voluta_inode_info **out_ii)
{
	const mode_t ifmt = S_IFMT;
	const mode_t reg_mode = (mode & ~ifmt) | S_IFREG;

	return new_inode(op_ctx, parent_dir, reg_mode, out_ii);
}

static int new_lnk_inode(const struct voluta_oper_ctx *op_ctx,
			 const struct voluta_inode_info *parent_dir,
			 struct voluta_inode_info **out_ii)
{
	const mode_t lnk_mode = S_IRWXU | S_IRWXG | S_IRWXO | S_IFLNK;

	return new_inode(op_ctx, parent_dir, lnk_mode, out_ii);
}

static int del_inode(struct voluta_inode_info *ii)
{
	return voluta_del_inode(i_sbi_of(ii), ii);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_authorize(const struct voluta_sb_info *sbi,
		     const struct voluta_oper_ctx *op_ctx)
{
	const struct voluta_ucred *ucred = &op_ctx->ucred;

	if (uid_eq(ucred->uid, sbi->s_owner)) {
		return 0;
	}
	if (uid_eq(ucred->uid, 0)) {
		return 0;
	}
	/* TODO: Check CAP_SYS_ADMIN */
	return -EPERM;
}

static int do_access(const struct voluta_oper_ctx *op_ctx,
		     const struct voluta_inode_info *ii, int mode)
{
	mode_t rwx = 0;
	const uid_t uid = op_ctx->ucred.uid;
	const gid_t gid = op_ctx->ucred.gid;
	const uid_t i_uid = i_uid_of(ii);
	const gid_t i_gid = i_gid_of(ii);
	const mode_t i_mode = i_mode_of(ii);
	const mode_t mask = (mode_t)mode;

	if (uid_isroot(uid)) {
		rwx |= R_OK | W_OK;
		if (S_ISREG(i_mode)) {
			if (i_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
				rwx |= X_OK;
			}
		} else {
			rwx |= X_OK;
		}
	} else if (uid_eq(uid, i_uid)) {
		/* Owner permissions */
		if (i_mode & S_IRUSR) {
			rwx |= R_OK;
		}
		if (i_mode & S_IWUSR) {
			rwx |= W_OK;
		}
		if (i_mode & S_IXUSR) {
			rwx |= X_OK;
		}
	} else if (gid_eq(gid, i_gid)) {
		/* Group permissions */
		if (i_mode & S_IRGRP) {
			rwx |= R_OK;
		}
		if (i_mode & S_IWGRP) {
			rwx |= W_OK;
		}
		if (i_mode & S_IXGRP) {
			rwx |= X_OK;
		}
		/* TODO: Check for supplementary groups */
	} else {
		/* Other permissions */
		if (i_mode & S_IROTH) {
			rwx |= R_OK;
		}
		if (i_mode & S_IWOTH) {
			rwx |= W_OK;
		}
		if (i_mode & S_IXOTH) {
			rwx |= X_OK;
		}
	}
	return ((rwx & mask) == mask) ? 0 : -EACCES;
}

int voluta_do_access(const struct voluta_oper_ctx *op_ctx,
		     const struct voluta_inode_info *ii, int mode)
{
	int err;

	i_incref(ii);
	err = do_access(op_ctx, ii, mode);
	i_decref(ii);

	return err;
}

static int check_waccess(const struct voluta_oper_ctx *op_ctx,
			 const struct voluta_inode_info *ii)
{
	return voluta_do_access(op_ctx, ii, W_OK);
}

static int check_xaccess(const struct voluta_oper_ctx *op_ctx,
			 const struct voluta_inode_info *ii)
{
	return voluta_do_access(op_ctx, ii, X_OK);
}

static int check_raccess(const struct voluta_oper_ctx *op_ctx,
			 const struct voluta_inode_info *ii)
{
	return voluta_do_access(op_ctx, ii, R_OK);
}

static int check_dir_waccess(const struct voluta_oper_ctx *op_ctx,
			     const struct voluta_inode_info *ii)
{
	int err;

	err = check_isdir(ii);
	if (err) {
		return err;
	}
	err = check_waccess(op_ctx, ii);
	if (err) {
		return err;
	}
	return 0;
}

static int check_dir_and_name(const struct voluta_inode_info *ii,
			      const struct voluta_qstr *name)
{
	int err;

	err = check_isdir(ii);
	if (err) {
		return err;
	}
	if (name->str.len == 0) {
		return -EINVAL;
	}
	if (name->str.len > VOLUTA_NAME_MAX) {
		return -ENAMETOOLONG;
	}
	return 0;
}

static int check_lookup(const struct voluta_oper_ctx *op_ctx,
			const struct voluta_inode_info *dir_ii,
			const struct voluta_qstr *name)
{
	int err;

	err = check_dir_and_name(dir_ii, name);
	if (err) {
		return err;
	}
	err = check_xaccess(op_ctx, dir_ii);
	if (err) {
		return err;
	}
	return 0;
}

static int lookup_by_name(const struct voluta_oper_ctx *op_ctx,
			  const struct voluta_inode_info *dir_ii,
			  const struct voluta_qstr *name, ino_t *out_ino)
{
	int err;
	struct voluta_ino_dt ino_dt;

	err = voluta_lookup_dentry(op_ctx, dir_ii, name, &ino_dt);
	if (err) {
		return err;
	}
	*out_ino = ino_dt.ino;
	return 0;
}

static int stage_by_child_ino(const struct voluta_inode_info *dir_ii,
			      ino_t ino, struct voluta_inode_info **out_ii)
{
	return voluta_stage_inode(i_sbi_of(dir_ii), ino, out_ii);
}

static int stage_by_name(const struct voluta_oper_ctx *op_ctx,
			 const struct voluta_inode_info *dir_ii,
			 const struct voluta_qstr *name,
			 struct voluta_inode_info **out_ii)
{
	int err;
	ino_t ino;

	err = lookup_by_name(op_ctx, dir_ii, name, &ino);
	if (err) {
		return err;
	}
	err = stage_by_child_ino(dir_ii, ino, out_ii);
	if (err) {
		return err;
	}
	return 0;
}

static int do_lookup(const struct voluta_oper_ctx *op_ctx,
		     const struct voluta_inode_info *dir_ii,
		     const struct voluta_qstr *name,
		     struct voluta_inode_info **out_ii)
{
	int err;

	err = check_lookup(op_ctx, dir_ii, name);
	if (err) {
		return err;
	}
	err = stage_by_name(op_ctx, dir_ii, name, out_ii);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_do_lookup(const struct voluta_oper_ctx *op_ctx,
		     const struct voluta_inode_info *dir_ii,
		     const struct voluta_qstr *name,
		     struct voluta_inode_info **out_ii)
{
	int err;

	i_incref(dir_ii);
	err = do_lookup(op_ctx, dir_ii, name, out_ii);
	i_decref(dir_ii);

	return err;
}

static int check_create_mode(mode_t mode)
{
	if (S_ISDIR(mode)) {
		return -EISDIR;
	}
	if (S_ISLNK(mode)) {
		return -EINVAL;
	}
	if (!S_ISREG(mode) && !S_ISFIFO(mode) && !S_ISSOCK(mode)) {
		return -ENOTSUP;
	}
	return 0;
}

static int check_nodent(const struct voluta_oper_ctx *op_ctx,
			const struct voluta_inode_info *dir_ii,
			const struct voluta_qstr *name)
{
	int err;
	ino_t ino;

	err = lookup_by_name(op_ctx, dir_ii, name, &ino);
	if (err == 0) {
		return -EEXIST;
	}
	return (err == -ENOENT) ? 0 : err;
}

static int check_add_dentry(const struct voluta_inode_info *dir_ii,
			    const struct voluta_qstr *name)
{
	int err;
	size_t ndents;
	const size_t ndents_max = VOLUTA_DIR_ENTRIES_MAX;

	err = check_dir_and_name(dir_ii, name);
	if (err) {
		return err;
	}
	ndents = voluta_dir_ndentries(dir_ii);
	if (!(ndents < ndents_max)) {
		return -EMLINK;
	}
	/* Special case for directory which is still held by open fd */
	if (i_nlink_of(dir_ii) < 2) {
		return -ENOENT;
	}
	return 0;
}

static int check_dir_can_add(const struct voluta_oper_ctx *op_ctx,
			     const struct voluta_inode_info *dir_ii,
			     const struct voluta_qstr *name)
{
	int err;

	err = check_dir_waccess(op_ctx, dir_ii);
	if (err) {
		return err;
	}
	err = check_nodent(op_ctx, dir_ii, name);
	if (err) {
		return err;
	}
	err = check_add_dentry(dir_ii, name);
	if (err) {
		return err;
	}
	return 0;
}

static int check_create(const struct voluta_oper_ctx *op_ctx,
			const struct voluta_inode_info *dir_ii,
			const struct voluta_qstr *name, mode_t mode)
{
	int err;

	err = check_dir_can_add(op_ctx, dir_ii, name);
	if (err) {
		return err;
	}
	err = check_create_mode(mode);
	if (err) {
		return err;
	}
	return 0;
}

static int do_add_dentry(const struct voluta_oper_ctx *op_ctx,
			 struct voluta_inode_info *dir_ii,
			 const struct voluta_qstr *name,
			 struct voluta_inode_info *ii,
			 bool del_upon_failure)
{
	int err;

	err = voluta_add_dentry(op_ctx, dir_ii, name, ii);
	if (err && del_upon_failure) {
		del_inode(ii);
	}
	return err;
}

static int do_create(const struct voluta_oper_ctx *op_ctx,
		     struct voluta_inode_info *dir_ii,
		     const struct voluta_qstr *name, mode_t mode,
		     struct voluta_inode_info **out_ii)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	err = check_create(op_ctx, dir_ii, name, mode);
	if (err) {
		return err;
	}
	err = new_reg_inode(op_ctx, dir_ii, mode, &ii);
	if (err) {
		return err;
	}
	err = do_add_dentry(op_ctx, dir_ii, name, ii, true);
	if (err) {
		return err;
	}
	update_nopen(ii, 1);
	update_itimes(op_ctx, dir_ii, VOLUTA_IATTR_MCTIME);

	*out_ii = ii;
	return 0;
}

int voluta_do_create(const struct voluta_oper_ctx *op_ctx,
		     struct voluta_inode_info *dir_ii,
		     const struct voluta_qstr *name, mode_t mode,
		     struct voluta_inode_info **out_ii)
{
	int err;

	i_incref(dir_ii);
	err = do_create(op_ctx, dir_ii, name, mode, out_ii);
	i_decref(dir_ii);

	return err;
}

static int check_mknod(const struct voluta_oper_ctx *op_ctx,
		       const struct voluta_inode_info *dir_ii,
		       const struct voluta_qstr *name,
		       mode_t mode, dev_t dev)
{
	int err;

	err = check_dir_can_add(op_ctx, dir_ii, name);
	if (err) {
		return err;
	}
	if (S_ISDIR(mode)) {
		return -EISDIR;
	}
	if (S_ISLNK(mode)) {
		return -EINVAL;
	}
	if (!S_ISFIFO(mode) && !S_ISSOCK(mode)) {
		return -ENOTSUP;
	}
	if (dev != 0) {
		return -EINVAL; /* XXX see man 3p mknod */
	}
	return 0;
}

static int create_special_inode(const struct voluta_oper_ctx *op_ctx,
				struct voluta_inode_info *dir_ii,
				mode_t mode, dev_t dev,
				struct voluta_inode_info **out_ii)
{
	int err;

	err = new_inode(op_ctx, dir_ii, mode, out_ii);
	if (err) {
		return err;
	}
	err = voluta_setup_ispecial(*out_ii, dev);
	if (err) {
		del_inode(*out_ii);
		return err;
	}
	return 0;
}

static int do_mknod(const struct voluta_oper_ctx *op_ctx,
		    struct voluta_inode_info *dir_ii,
		    const struct voluta_qstr *name,
		    mode_t mode, dev_t dev,
		    struct voluta_inode_info **out_ii)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	err = check_mknod(op_ctx, dir_ii, name, mode, dev);
	if (err) {
		return err;
	}
	err = create_special_inode(op_ctx, dir_ii, mode, dev, &ii);
	if (err) {
		return err;
	}
	err = do_add_dentry(op_ctx, dir_ii, name, ii, true);
	if (err) {
		return err;
	}
	update_itimes(op_ctx, dir_ii, VOLUTA_IATTR_MCTIME);

	*out_ii = ii;
	return 0;
}

int voluta_do_mknod(const struct voluta_oper_ctx *op_ctx,
		    struct voluta_inode_info *dir_ii,
		    const struct voluta_qstr *name,
		    mode_t mode, dev_t dev,
		    struct voluta_inode_info **out_ii)
{
	int err;

	i_incref(dir_ii);
	err = do_mknod(op_ctx, dir_ii, name, mode, dev, out_ii);
	i_decref(dir_ii);

	return err;
}

static int o_flags_to_rwx_mask(int o_flags)
{
	int mask = 0;

	if ((o_flags & O_RDWR) == O_RDWR) {
		mask = R_OK | W_OK;
	} else if ((o_flags & O_WRONLY) == O_WRONLY) {
		mask = W_OK;
	} else if ((o_flags & O_RDONLY) == O_RDONLY) {
		mask = R_OK;
	}
	if ((o_flags & O_TRUNC) == O_TRUNC) {
		mask |= W_OK;
	}
	if ((o_flags & O_APPEND) == O_APPEND) {
		mask |= W_OK;
	}
	return mask;
}

static int check_open(const struct voluta_oper_ctx *op_ctx,
		      const struct voluta_inode_info *ii, int o_flags)
{
	int err;
	int mask;

	err = check_reg_or_fifo(ii);
	if (err) {
		return err;
	}
	if (o_flags & O_DIRECTORY) {
		return -EISDIR;
	}
	if (o_flags & (O_CREAT | O_EXCL)) {
		return -EEXIST; /* XXX ? */
	}
	mask = o_flags_to_rwx_mask(o_flags);
	err = voluta_do_access(op_ctx, ii, mask);
	if (err) {
		return err;
	}
	err = check_open_limit(ii);
	if (err) {
		return err;
	}
	return 0;
}

static int post_open(const struct voluta_oper_ctx *op_ctx,
		     struct voluta_inode_info *ii, int o_flags)
{
	int err = 0;

	if ((o_flags & O_TRUNC) && i_isreg(ii)) {
		err = voluta_do_truncate(op_ctx, ii, 0);
	}
	return err;
}

static int do_open(const struct voluta_oper_ctx *op_ctx,
		   struct voluta_inode_info *ii, int o_flags)
{
	int err;

	err = check_open(op_ctx, ii, o_flags);
	if (err) {
		return err;
	}
	err = post_open(op_ctx, ii, o_flags);
	if (err) {
		return err;
	}
	update_nopen(ii, 1);
	return 0;
}

int voluta_do_open(const struct voluta_oper_ctx *op_ctx,
		   struct voluta_inode_info *ii, int o_flags)
{
	int err;

	i_incref(ii);
	err = do_open(op_ctx, ii, o_flags);
	i_decref(ii);

	return err;
}

static int drop_ispecific(struct voluta_inode_info *ii)
{
	int err;

	if (i_isdir(ii)) {
		err = voluta_drop_dir(ii);
	} else if (i_isreg(ii)) {
		err = voluta_drop_reg(ii);
	} else if (i_islnk(ii)) {
		err = voluta_drop_symlink(ii);
	} else {
		err = 0;
	}
	return err;
}

static int drop_unlinked(struct voluta_inode_info *ii)
{
	int err;

	err = voluta_drop_xattr(ii);
	if (err) {
		return err;
	}
	err = drop_ispecific(ii);
	if (err) {
		return err;
	}
	err = del_inode(ii);
	if (err) {
		return err;
	}
	return 0;
}

static bool i_isdropable(const struct voluta_inode_info *ii)
{
	bool res = false;
	const nlink_t nlink = i_nlink_of(ii);

	if ((ii->i_nopen == 0) && !i_refcnt(ii)) {
		res = i_isdir(ii) ? (nlink <= 1) : (nlink < 1);
	}
	return res;
}

static int try_prune_inode(const struct voluta_oper_ctx *op_ctx,
			   struct voluta_inode_info *ii, bool update_ctime)
{
	int err = 0;

	if (i_isdropable(ii)) {
		err = drop_unlinked(ii);
	} else if (update_ctime) {
		update_itimes(op_ctx, ii, VOLUTA_IATTR_CTIME);
	}
	return err;
}

static int do_remove_dentry(const struct voluta_oper_ctx *op_ctx,
			    struct voluta_inode_info *dir_ii,
			    const struct voluta_qstr *name,
			    struct voluta_inode_info *ii)
{
	int err;

	i_incref(ii);
	err = voluta_remove_dentry(op_ctx, dir_ii, name);
	i_decref(ii);

	return !err ? try_prune_inode(op_ctx, ii, true) : err;
}

static int check_prepare_unlink(const struct voluta_oper_ctx *op_ctx,
				struct voluta_inode_info *dir_ii,
				const struct voluta_qstr *name,
				struct voluta_inode_info **out_ii)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	err = check_dir_waccess(op_ctx, dir_ii);
	if (err) {
		return err;
	}
	err = stage_by_name(op_ctx, dir_ii, name, &ii);
	if (err) {
		return err;
	}
	err = check_sticky(op_ctx, dir_ii, ii);
	if (err) {
		return err;
	}
	err = check_notdir(ii);
	if (err) {
		return err;
	}
	*out_ii = ii;
	return 0;
}

static int do_unlink(const struct voluta_oper_ctx *op_ctx,
		     struct voluta_inode_info *dir_ii,
		     const struct voluta_qstr *name)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	err = check_prepare_unlink(op_ctx, dir_ii, name, &ii);
	if (err) {
		return err;
	}
	err = do_remove_dentry(op_ctx, dir_ii, name, ii);
	if (err) {
		return err;
	}
	update_itimes(op_ctx, dir_ii, VOLUTA_IATTR_MCTIME);
	return 0;
}

int voluta_do_unlink(const struct voluta_oper_ctx *op_ctx,
		     struct voluta_inode_info *dir_ii,
		     const struct voluta_qstr *name)
{
	int err;

	i_incref(dir_ii);
	err = do_unlink(op_ctx, dir_ii, name);
	i_decref(dir_ii);

	return err;
}

static int check_nomlink(const struct voluta_inode_info *ii)
{
	const size_t link_max = VOLUTA_LINK_MAX;

	return (i_nlink_of(ii) < link_max) ? 0 : -EMLINK;
}

static int check_link(const struct voluta_oper_ctx *op_ctx,
		      const struct voluta_inode_info *dir_ii,
		      const struct voluta_qstr *name,
		      const struct voluta_inode_info *ii)
{
	int err;

	err = check_dir_waccess(op_ctx, dir_ii);
	if (err) {
		return err;
	}
	err = check_notdir(ii);
	if (err) {
		return err;
	}
	err = check_nodent(op_ctx, dir_ii, name);
	if (err) {
		return err;
	}
	err = check_nomlink(ii);
	if (err) {
		return err;
	}
	return 0;
}

static int do_link(const struct voluta_oper_ctx *op_ctx,
		   struct voluta_inode_info *dir_ii,
		   const struct voluta_qstr *name,
		   struct voluta_inode_info *ii)
{
	int err;

	err = check_link(op_ctx, dir_ii, name, ii);
	if (err) {
		return err;
	}
	err = do_add_dentry(op_ctx, dir_ii, name, ii, false);
	if (err) {
		return err;
	}
	update_itimes(op_ctx, dir_ii, VOLUTA_IATTR_MCTIME);
	update_itimes(op_ctx, ii, VOLUTA_IATTR_CTIME);

	return 0;
}

int voluta_do_link(const struct voluta_oper_ctx *op_ctx,
		   struct voluta_inode_info *dir_ii,
		   const struct voluta_qstr *name,
		   struct voluta_inode_info *ii)
{
	int err;

	i_incref(dir_ii);
	i_incref(ii);
	err = do_link(op_ctx, dir_ii, name, ii);
	i_decref(ii);
	i_decref(dir_ii);

	return err;
}

static int check_release(const struct voluta_inode_info *ii)
{
	int err;

	err = check_notdir(ii);
	if (err) {
		return err;
	}
	err = check_opened(ii);
	if (err) {
		return err;
	}
	return 0;
}

static int do_release(struct voluta_inode_info *ii)
{
	int err;

	err = check_release(ii);
	if (err) {
		return err;
	}
	update_nopen(ii, -1);
	return 0;
}

int voluta_do_release(const struct voluta_oper_ctx *op_ctx,
		      struct voluta_inode_info *ii)
{
	int err;

	i_incref(ii);
	err = do_release(ii);
	i_decref(ii);

	return !err ? try_prune_inode(op_ctx, ii, false) : err;
}

static int check_mkdir(const struct voluta_oper_ctx *op_ctx,
		       struct voluta_inode_info *dir_ii,
		       const struct voluta_qstr *name)
{
	int err;

	err = check_dir_can_add(op_ctx, dir_ii, name);
	if (err) {
		return err;
	}
	err = check_nomlink(dir_ii);
	if (err) {
		return err;
	}
	return 0;
}

static int do_mkdir(const struct voluta_oper_ctx *op_ctx,
		    struct voluta_inode_info *dir_ii,
		    const struct voluta_qstr *name, mode_t mode,
		    struct voluta_inode_info **out_ii)
{
	int err;
	struct voluta_inode_info *ii;

	err = check_mkdir(op_ctx, dir_ii, name);
	if (err) {
		return err;
	}
	err = new_dir_inode(op_ctx, dir_ii, mode, &ii);
	if (err) {
		return err;
	}
	err = do_add_dentry(op_ctx, dir_ii, name, ii, true);
	if (err) {
		return err;
	}
	update_itimes(op_ctx, dir_ii, VOLUTA_IATTR_MCTIME);

	*out_ii = ii;
	return 0;
}

int voluta_do_mkdir(const struct voluta_oper_ctx *op_ctx,
		    struct voluta_inode_info *dir_ii,
		    const struct voluta_qstr *name, mode_t mode,
		    struct voluta_inode_info **out_ii)
{
	int err;

	i_incref(dir_ii);
	err = do_mkdir(op_ctx, dir_ii, name, mode, out_ii);
	i_decref(dir_ii);

	return err;
}

static bool dir_isempty(const struct voluta_inode_info *dir_ii)
{
	if (i_nlink_of(dir_ii) > 2) {
		return false;
	}
	if (voluta_dir_ndentries(dir_ii)) {
		return false;
	}
	return true;
}

static int check_rmdir_child(const struct voluta_oper_ctx *op_ctx,
			     const struct voluta_inode_info *parent_ii,
			     const struct voluta_inode_info *dir_ii)
{
	int err;

	err = check_isdir(dir_ii);
	if (err) {
		return err;
	}
	if (!dir_isempty(dir_ii)) {
		return -ENOTEMPTY;
	}
	if (i_isrootd(dir_ii)) {
		return -EBUSY;
	}
	err = check_sticky(op_ctx, parent_ii, dir_ii);
	if (err) {
		return err;
	}
	return 0;
}

static int check_prepare_rmdir(const struct voluta_oper_ctx *op_ctx,
			       struct voluta_inode_info *dir_ii,
			       const struct voluta_qstr *name,
			       struct voluta_inode_info **out_ii)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	err = check_dir_waccess(op_ctx, dir_ii);
	if (err) {
		return err;
	}
	err = stage_by_name(op_ctx, dir_ii, name, &ii);
	if (err) {
		return err;
	}
	err = check_rmdir_child(op_ctx, dir_ii, ii);
	if (err) {
		return err;
	}
	*out_ii = ii;
	return 0;
}

static int do_rmdir(const struct voluta_oper_ctx *op_ctx,
		    struct voluta_inode_info *dir_ii,
		    const struct voluta_qstr *name)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	err = check_prepare_rmdir(op_ctx, dir_ii, name, &ii);
	if (err) {
		return err;
	}
	err = do_remove_dentry(op_ctx, dir_ii, name, ii);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_do_rmdir(const struct voluta_oper_ctx *op_ctx,
		    struct voluta_inode_info *dir_ii,
		    const struct voluta_qstr *name)
{
	int err;

	i_incref(dir_ii);
	err = do_rmdir(op_ctx, dir_ii, name);
	i_decref(dir_ii);

	return err;
}

static int create_lnk_inode(const struct voluta_oper_ctx *op_ctx,
			    const struct voluta_inode_info *dir_ii,
			    const struct voluta_str *linkpath,
			    struct voluta_inode_info **out_ii)
{
	int err;

	err = new_lnk_inode(op_ctx, dir_ii, out_ii);
	if (err) {
		return err;
	}
	err = voluta_setup_symlink(op_ctx, *out_ii, linkpath);
	if (err) {
		del_inode(*out_ii);
		return err;
	}
	return 0;
}

static int check_symval(const struct voluta_str *symval)
{
	if (symval->len == 0) {
		return -EINVAL;
	}
	if (symval->len > VOLUTA_SYMLNK_MAX) {
		return -ENAMETOOLONG;
	}
	return 0;
}

static int check_symlink(const struct voluta_oper_ctx *op_ctx,
			 struct voluta_inode_info *dir_ii,
			 const struct voluta_qstr *name,
			 const struct voluta_str *symval)
{
	int err;

	err = check_dir_can_add(op_ctx, dir_ii, name);
	if (err) {
		return err;
	}
	err = check_symval(symval);
	if (err) {
		return err;
	}
	return 0;
}

static int do_symlink(const struct voluta_oper_ctx *op_ctx,
		      struct voluta_inode_info *dir_ii,
		      const struct voluta_qstr *name,
		      const struct voluta_str *symval,
		      struct voluta_inode_info **out_ii)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	err = check_symlink(op_ctx, dir_ii, name, symval);
	if (err) {
		return err;
	}
	err = create_lnk_inode(op_ctx, dir_ii, symval, &ii);
	if (err) {
		return err;
	}
	err = do_add_dentry(op_ctx, dir_ii, name, ii, true);
	if (err) {
		return err;
	}
	update_itimes(op_ctx, dir_ii, VOLUTA_IATTR_MCTIME);

	*out_ii = ii;
	return 0;
}

int voluta_do_symlink(const struct voluta_oper_ctx *op_ctx,
		      struct voluta_inode_info *dir_ii,
		      const struct voluta_qstr *name,
		      const struct voluta_str *symval,
		      struct voluta_inode_info **out_ii)
{
	int err;

	i_incref(dir_ii);
	err = do_symlink(op_ctx, dir_ii, name, symval, out_ii);
	i_decref(dir_ii);

	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int check_opendir(const struct voluta_oper_ctx *op_ctx,
			 struct voluta_inode_info *dir_ii)
{
	int err;

	err = check_isdir(dir_ii);
	if (err) {
		return err;
	}
	err = check_raccess(op_ctx, dir_ii);
	if (err) {
		return err;
	}
	err = check_open_limit(dir_ii);
	if (err) {
		return err;
	}
	return 0;
}

static int do_opendir(const struct voluta_oper_ctx *op_ctx,
		      struct voluta_inode_info *dir_ii)
{
	int err;

	err = check_opendir(op_ctx, dir_ii);
	if (err) {
		return err;
	}
	update_nopen(dir_ii, 1);
	return 0;
}

int voluta_do_opendir(const struct voluta_oper_ctx *op_ctx,
		      struct voluta_inode_info *dir_ii)
{
	int err;

	i_incref(dir_ii);
	err = do_opendir(op_ctx, dir_ii);
	i_decref(dir_ii);

	return err;
}

static int check_releasedir(const struct voluta_oper_ctx *op_ctx,
			    const struct voluta_inode_info *dir_ii)
{
	int err;

	err = check_isdir(dir_ii);
	if (err) {
		return err;
	}
	err = check_opened(dir_ii);
	if (err) {
		return err;
	}
	voluta_unused(op_ctx);
	return 0;
}

/*
 * TODO-0017: Shrink sparse dir-tree upon last close
 *
 * Try to shrink sparse dir hash-tree upon last close. Note that we should
 * not do so while dir is held open, as it may corrupt active readdir.
 */
static int do_releasedir(const struct voluta_oper_ctx *op_ctx,
			 struct voluta_inode_info *dir_ii)
{
	int err;

	err = check_releasedir(op_ctx, dir_ii);
	if (err) {
		return err;
	}
	update_nopen(dir_ii, -1);

	return 0;
}

int voluta_do_releasedir(const struct voluta_oper_ctx *op_ctx,
			 struct voluta_inode_info *dir_ii)
{
	int err;

	i_incref(dir_ii);
	err = do_releasedir(op_ctx, dir_ii);
	i_decref(dir_ii);

	return !err ? try_prune_inode(op_ctx, dir_ii, false) : err;
}

static int check_fsyncdir(const struct voluta_oper_ctx *op_ctx,
			  const struct voluta_inode_info *dir_ii)
{
	int err;

	err = check_isdir(dir_ii);
	if (err) {
		return err;
	}
	err = check_opened(dir_ii);
	if (err) {
		return err;
	}
	voluta_unused(op_ctx);
	return 0;
}

static int do_fsyncdir(const struct voluta_oper_ctx *op_ctx,
		       struct voluta_inode_info *dir_ii, bool dsync)
{
	int err;

	err = check_fsyncdir(op_ctx, dir_ii);
	if (err) {
		return err;
	}
	/* TODO */
	voluta_unused(dsync);
	return 0;
}

int voluta_do_fsyncdir(const struct voluta_oper_ctx *op_ctx,
		       struct voluta_inode_info *dir_ii, bool dsync)
{
	int err;

	i_incref(dir_ii);
	err = do_fsyncdir(op_ctx, dir_ii, dsync);
	i_decref(dir_ii);

	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct voluta_dentry_ref {
	struct voluta_inode_info *dir_ii;
	const struct voluta_qstr *name;
	struct voluta_inode_info *ii;
};

static int check_add_dentry_at(const struct voluta_dentry_ref *dref)
{
	return check_add_dentry(dref->dir_ii, dref->name);
}

static int do_add_dentry_at(const struct voluta_oper_ctx *op_ctx,
			    struct voluta_dentry_ref *dref,
			    struct voluta_inode_info *ii)

{
	int err;

	err = do_add_dentry(op_ctx, dref->dir_ii, dref->name, ii, false);
	if (!err) {
		dref->ii = ii;
	}
	return err;
}

static int do_remove_dentry_at(const struct voluta_oper_ctx *op_ctx,
			       struct voluta_dentry_ref *dref)
{
	int err;

	err = do_remove_dentry(op_ctx, dref->dir_ii, dref->name, dref->ii);
	if (!err) {
		dref->ii = NULL;
	}
	return err;
}

static int do_rename_move(const struct voluta_oper_ctx *op_ctx,
			  struct voluta_dentry_ref *cur_dref,
			  struct voluta_dentry_ref *new_dref)
{
	int err;
	struct voluta_inode_info *ii = cur_dref->ii;

	err = check_add_dentry_at(new_dref);
	if (err) {
		return err;
	}
	err = do_remove_dentry_at(op_ctx, cur_dref);
	if (err) {
		return err;
	}
	err = do_add_dentry_at(op_ctx, new_dref, ii);
	if (err) {
		return err;
	}
	return 0;
}

static int rename_move(const struct voluta_oper_ctx *op_ctx,
		       struct voluta_dentry_ref *cur_dref,
		       struct voluta_dentry_ref *new_dref)
{
	int err;
	struct voluta_inode_info *ii = cur_dref->ii;

	i_incref(ii);
	err = do_rename_move(op_ctx, cur_dref, new_dref);
	i_decref(ii);

	return err;
}

static int rename_unlink(const struct voluta_oper_ctx *op_ctx,
			 struct voluta_dentry_ref *dref)
{
	return do_remove_dentry_at(op_ctx, dref);
}

static int do_rename_replace(const struct voluta_oper_ctx *op_ctx,
			     struct voluta_dentry_ref *cur_dref,
			     struct voluta_dentry_ref *new_dref)
{
	int err;
	struct voluta_inode_info *ii = cur_dref->ii;

	err = do_remove_dentry_at(op_ctx, cur_dref);
	if (err) {
		return err;
	}
	err = do_remove_dentry_at(op_ctx, new_dref);
	if (err) {
		return err;
	}
	err = do_add_dentry_at(op_ctx, new_dref, ii);
	if (err) {
		return err;
	}
	return 0;
}

static int rename_replace(const struct voluta_oper_ctx *op_ctx,
			  struct voluta_dentry_ref *cur_dref,
			  struct voluta_dentry_ref *new_dref)
{
	int err;
	struct voluta_inode_info *ii = cur_dref->ii;

	i_incref(ii);
	err = do_rename_replace(op_ctx, cur_dref, new_dref);
	i_decref(ii);

	return err;
}

static int do_rename_exchange(const struct voluta_oper_ctx *op_ctx,
			      struct voluta_dentry_ref *dref1,
			      struct voluta_dentry_ref *dref2)
{
	int err;
	struct voluta_inode_info *ii1 = dref1->ii;
	struct voluta_inode_info *ii2 = dref2->ii;

	err = do_remove_dentry_at(op_ctx, dref1);
	if (err) {
		return err;
	}
	err = do_remove_dentry_at(op_ctx, dref2);
	if (err) {
		return err;
	}
	err = do_add_dentry_at(op_ctx, dref2, ii1);
	if (err) {
		return err;
	}
	err = do_add_dentry_at(op_ctx, dref1, ii2);
	if (err) {
		return err;
	}
	return 0;
}

static int rename_exchange(const struct voluta_oper_ctx *op_ctx,
			   struct voluta_dentry_ref *dref1,
			   struct voluta_dentry_ref *dref2)
{
	int err;
	struct voluta_inode_info *ii1 = dref1->ii;
	struct voluta_inode_info *ii2 = dref2->ii;

	i_incref(ii1);
	i_incref(ii2);
	err = do_rename_exchange(op_ctx, dref1, dref2);
	i_decref(ii2);
	i_decref(ii1);

	return err;
}

static int rename_specific(const struct voluta_oper_ctx *op_ctx,
			   struct voluta_dentry_ref *cur_dref,
			   struct voluta_dentry_ref *new_dref, int flags)
{
	int err;

	if (new_dref->ii == NULL) {
		err = rename_move(op_ctx, cur_dref, new_dref);
	} else if (cur_dref->ii == new_dref->ii) {
		err = rename_unlink(op_ctx, cur_dref);
	} else if (flags & RENAME_EXCHANGE) {
		err = rename_exchange(op_ctx, cur_dref, new_dref);
	} else {
		err = rename_replace(op_ctx, cur_dref, new_dref);
	}
	update_itimes(op_ctx, cur_dref->dir_ii, VOLUTA_IATTR_MCTIME);
	update_itimes(op_ctx, new_dref->dir_ii, VOLUTA_IATTR_MCTIME);
	return err;
}

static int check_rename_exchange(const struct voluta_dentry_ref *cur_dref,
				 const struct voluta_dentry_ref *new_dref)
{
	int err = 0;
	const struct voluta_inode_info *ii = cur_dref->ii;
	const struct voluta_inode_info *old_ii = new_dref->ii;

	if (ii == NULL) {
		return -EINVAL;
	}
	if ((ii != old_ii) && (i_isdir(ii) != i_isdir(old_ii))) {
		if (i_isdir(old_ii)) {
			err = check_nomlink(new_dref->dir_ii);
		} else  {
			err = check_nomlink(cur_dref->dir_ii);
		}
	}
	return err;
}

static int check_rename(const struct voluta_oper_ctx *op_ctx,
			const struct voluta_dentry_ref *cur_dref,
			const struct voluta_dentry_ref *new_dref, int flags)
{
	int err = 0;
	const struct voluta_inode_info *ii = cur_dref->ii;
	const struct voluta_inode_info *old_ii = new_dref->ii;
	const bool old_exists = (old_ii != NULL);

	if (flags & RENAME_WHITEOUT) {
		return -EINVAL;
	}
	if (flags & ~(RENAME_NOREPLACE | RENAME_EXCHANGE)) {
		return -EINVAL;
	}
	if ((flags & RENAME_NOREPLACE) && old_exists) {
		return -EEXIST;
	}
	if ((flags & RENAME_EXCHANGE) && !old_exists) {
		return -ENOENT;
	}
	if (flags & RENAME_EXCHANGE) {
		return check_rename_exchange(cur_dref, new_dref);
	}
	if (old_exists && i_isdir(old_ii) && (old_ii != ii)) {
		err = (ii == NULL) ? check_nomlink(new_dref->dir_ii) :
		      check_rmdir_child(op_ctx, cur_dref->dir_ii, old_ii);
	}
	return err;
}

static int check_stage_rename_at(const struct voluta_oper_ctx *op_ctx,
				 struct voluta_dentry_ref *dref, bool new_de)
{
	int err;

	err = check_dir_waccess(op_ctx, dref->dir_ii);
	if (err) {
		return err;
	}
	err = stage_by_name(op_ctx, dref->dir_ii, dref->name, &dref->ii);
	if (err) {
		return ((err == -ENOENT) && new_de) ? 0 : err;
	}
	err = check_sticky(op_ctx, dref->dir_ii, dref->ii);
	if (err) {
		return err;
	}
	return 0;
}

static int do_rename(const struct voluta_oper_ctx *op_ctx,
		     struct voluta_inode_info *dir_ii,
		     const struct voluta_qstr *name,
		     struct voluta_inode_info *newdir_ii,
		     const struct voluta_qstr *newname, int flags)
{
	int err;
	struct voluta_dentry_ref cur_dref = {
		.dir_ii = dir_ii,
		.name = name,
	};
	struct voluta_dentry_ref new_dref = {
		.dir_ii = newdir_ii,
		.name = newname,
	};

	err = check_stage_rename_at(op_ctx, &cur_dref, false);
	if (err) {
		return err;
	}
	err = check_stage_rename_at(op_ctx, &new_dref, true);
	if (err) {
		return err;
	}
	err = check_rename(op_ctx, &cur_dref, &new_dref, flags);
	if (err) {
		return err;
	}
	err = rename_specific(op_ctx, &cur_dref, &new_dref, flags);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_do_rename(const struct voluta_oper_ctx *op_ctx,
		     struct voluta_inode_info *dir_ii,
		     const struct voluta_qstr *name,
		     struct voluta_inode_info *newdir_ii,
		     const struct voluta_qstr *newname, int flags)
{
	int err;

	i_incref(dir_ii);
	i_incref(newdir_ii);
	err = do_rename(op_ctx, dir_ii, name, newdir_ii, newname, flags);
	i_decref(newdir_ii);
	i_decref(dir_ii);

	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int do_statvfs(const struct voluta_oper_ctx *op_ctx,
		      const struct voluta_inode_info *ii,
		      struct statvfs *out_stvfs)
{
	const struct voluta_sb_info *sbi = i_sbi_of(ii);

	voluta_unused(op_ctx);
	voluta_statvfs_of(sbi, out_stvfs);
	return 0;
}

int voluta_do_statvfs(const struct voluta_oper_ctx *op_ctx,
		      struct voluta_inode_info *ii, struct statvfs *out_stvfs)
{
	int err;

	i_incref(ii);
	err = do_statvfs(op_ctx, ii, out_stvfs);
	i_decref(ii);

	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

union voluta_utf32_name_buf {
	char dat[4 * (VOLUTA_NAME_MAX + 1)];
	uint32_t utf32[VOLUTA_NAME_MAX + 1];
} voluta_aligned64;


static int check_utf8_name(const struct voluta_sb_info *sbi,
			   const char *name, size_t name_len)
{
	union voluta_utf32_name_buf outbuf;
	char *in = (char *)name;
	char *out = outbuf.dat;
	size_t len = name_len;
	size_t outlen = sizeof(outbuf.dat);
	size_t datlen;
	size_t ret;

	ret = iconv(sbi->s_namei_iconv, &in, &len, &out, &outlen);
	if ((ret != 0) || len || (outlen % 4)) {
		return errno ? -errno : -EINVAL;
	}
	datlen = sizeof(outbuf.dat) - outlen;
	if (datlen == 0) {
		return -EINVAL;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static uint64_t hash256_to_u64(const struct voluta_hash256 *hash)
{
	uint64_t v = 0;

	VOLUTA_STATICASSERT_GT(sizeof(hash->hash), sizeof(v));
	for (size_t i = 0; i < 8; ++i) {
		v = (v << 8) | (uint64_t)(hash->hash[i]);
	}
	return v;
}

static const struct voluta_crypto *
i_crypto_of(const struct voluta_inode_info *ii)
{
	return v_crypto_of(i_vi(ii));
}

static int name_to_hash(const struct voluta_inode_info *dir_ii,
			const char *name, size_t name_len, uint64_t *out_hash)
{
	int err;
	struct voluta_hash256 sha256;

	if (!voluta_dir_hasflag(dir_ii, VOLUTA_DIRF_HASH_SHA256)) {
		return -EINVAL;
	}
	err = voluta_sha256_of(i_crypto_of(dir_ii), name, name_len, &sha256);
	if (err) {
		return err;
	}
	*out_hash = hash256_to_u64(&sha256);
	return 0;
}

static int check_valid_name(const char *name, size_t len)
{
	if (len == 0) {
		return -EINVAL;
	}
	if (len > VOLUTA_NAME_MAX) {
		return -ENAMETOOLONG;
	}
	if (memchr(name, '/', len)) {
		return -EINVAL;
	}
	if (name[len] != '\0') {
		return -EINVAL;
	}
	return 0;
}

static int check_valid_encoding(const struct voluta_inode_info *ii,
				const char *name, size_t name_len)
{
	return i_hasflag(ii, VOLUTA_INODEF_NAME_UTF8) ?
	       check_utf8_name(i_sbi_of(ii), name, name_len) : -EINVAL;
}

int voluta_name_to_str(const struct voluta_inode_info *ii,
		       const char *name, struct voluta_str *str)
{
	int err;
	size_t len;

	len = strnlen(name, VOLUTA_NAME_MAX + 1);
	err = check_valid_name(name, len);
	if (err) {
		return err;
	}
	err = check_valid_encoding(ii, name, len);
	if (err) {
		return err;
	}
	str->str = name;
	str->len = len;
	return 0;
}

int voluta_name_to_qstr(const struct voluta_inode_info *dir_ii,
			const char *name, struct voluta_qstr *qstr)
{
	int err;
	struct voluta_str *str = &qstr->str;

	err = check_isdir(dir_ii);
	if (err) {
		return err;
	}
	err = voluta_name_to_str(dir_ii, name, str);
	if (err) {
		return err;
	}
	err = name_to_hash(dir_ii, str->str, str->len, &qstr->hash);
	if (err) {
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_inode_info *
lookup_cached_ii(struct voluta_sb_info *sbi, ino_t ino)
{
	return voluta_cache_lookup_ii(sbi->s_cache, ino);
}

static void forget_cached_ii(struct voluta_inode_info *ii)
{
	struct voluta_sb_info *sbi = i_sbi_of(ii);

	voulta_cache_forget_ii(sbi->s_cache, ii);
}

int voluta_do_evict_inode(struct voluta_sb_info *sbi, ino_t xino)
{
	int err;
	ino_t ino;
	struct voluta_inode_info *ii;

	err = voluta_real_ino(sbi, xino, &ino);
	if (!err) {
		ii = lookup_cached_ii(sbi, ino);
		if (ii && voluta_isevictable_ii(ii)) {
			forget_cached_ii(ii);
		}
	}
	return err;
}


