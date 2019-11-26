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
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include "voluta-lib.h"

static const struct voluta_ucred *ucred_of(const struct voluta_env *env)
{
	return &env->ucred;
}

static bool isowner(const struct voluta_env *env,
		    const struct voluta_inode_info *ii)
{
	const struct voluta_ucred *ucred = ucred_of(env);

	return uid_eq(ucred->uid, i_uid_of(ii));
}

static int require_dir(const struct voluta_inode_info *ii)
{
	return i_isdir(ii) ? 0 : -ENOTDIR;
}

static int require_notdir(const struct voluta_inode_info *ii)
{
	return i_isdir(ii) ? -EISDIR : 0;
}

static int require_reg_or_fifo(const struct voluta_inode_info *ii)
{
	if (i_isdir(ii)) {
		return -EISDIR;
	}
	if (!i_isreg(ii) && !i_isfifo(ii)) {
		return -EINVAL;
	}
	return 0;
}

static int require_fileref(const struct voluta_env *env,
			   const struct voluta_inode_info *ii)
{
	int err = 0;
	const size_t open_max = 1U << 16; /* XXX */
	const size_t open_cur = i_refcnt_of(ii);
	const struct voluta_ucred *ucred = ucred_of(env);

	if ((open_cur >= open_max) && !uid_eq(ucred->uid, 0)) {
		err = -ENFILE;
	}
	return err;
}

static bool has_sticky_bit(const struct voluta_inode_info *dir_ii)
{
	const mode_t mode = i_mode_of(dir_ii);

	return ((mode & S_ISVTX) == S_ISVTX);
}

static int check_sticky(const struct voluta_env *env,
			const struct voluta_inode_info *dir_ii,
			const struct voluta_inode_info *ii)
{

	if (!has_sticky_bit(dir_ii)) {
		return 0; /* No sticky-bit, we're fine */
	}
	if (isowner(env, dir_ii)) {
		return 0;
	}
	if (ii && isowner(env, ii)) {
		return 0;
	}
	/* TODO: Check CAP_FOWNER */
	return -EPERM;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int new_inode(struct voluta_env *env, mode_t mode,
		     ino_t parent_ino, struct voluta_inode_info **out_ii)
{
	return voluta_new_inode(env, mode, parent_ino, out_ii);
}

static int new_dir_inode(struct voluta_env *env, mode_t mode, ino_t parent_ino,
			 struct voluta_inode_info **out_ii)
{
	const mode_t ifmt = S_IFMT;

	return new_inode(env, (mode & ~ifmt) | S_IFDIR, parent_ino, out_ii);
}

static int new_reg_inode(struct voluta_env *env, mode_t mode, ino_t parent_ino,
			 struct voluta_inode_info **out_ii)
{
	const mode_t ifmt = S_IFMT;

	return new_inode(env, (mode & ~ifmt) | S_IFREG, parent_ino, out_ii);
}

static int new_lnk_inode(struct voluta_env *env, ino_t parent_ino,
			 struct voluta_inode_info **out_ii)
{
	const mode_t mode = S_IFLNK | S_IRWXU | S_IRWXG | S_IRWXO;

	return voluta_new_inode(env, mode, parent_ino, out_ii);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_authorize(const struct voluta_env *env)
{
	const struct voluta_ucred *ucred = ucred_of(env);

	if (uid_eq(ucred->uid, env->sbi.s_owner)) {
		return 0;
	}
	if (uid_eq(ucred->uid, 0)) {
		return 0;
	}
	/* TODO: Check CAP_SYS_ADMIN */
	return -EPERM;
}

int voluta_do_access(struct voluta_env *env,
		     const struct voluta_inode_info *ii, int mode)
{
	mode_t rwx = 0;
	const struct voluta_ucred *ucred = ucred_of(env);
	const uid_t uid = ucred->uid;
	const gid_t gid = ucred->gid;
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

static int require_waccess(struct voluta_env *env,
			   const struct voluta_inode_info *ii)
{
	return voluta_do_access(env, ii, W_OK);
}

static int require_xaccess(struct voluta_env *env,
			   const struct voluta_inode_info *ii)
{
	return voluta_do_access(env, ii, X_OK);
}

int voluta_do_lookup(struct voluta_env *env,
		     const struct voluta_inode_info *dir_ii,
		     const struct voluta_qstr *qstr,
		     struct voluta_inode_info **out_ii)
{
	int err;
	struct voluta_ino_dt ino_dt;
	struct voluta_inode_info *ii = NULL;

	err = require_dir(dir_ii);
	if (err) {
		return err;
	}
	err = require_xaccess(env, dir_ii);
	if (err) {
		return err;
	}
	err = voluta_resolve_by_name(env, dir_ii, qstr, &ino_dt);
	if (err) {
		return err;
	}
	err = voluta_stage_inode(env, ino_dt.ino, &ii);
	if (err) {
		return err;
	}
	*out_ii = ii;
	return 0;
}

static int check_create(struct voluta_env *env,
			const struct voluta_inode_info *dir_ii, mode_t mode)
{
	int err;
	size_t nkbs;

	err = require_dir(dir_ii);
	if (err) {
		return err;
	}
	if (S_ISDIR(mode)) {
		return -EISDIR;
	}
	if (S_ISLNK(mode)) {
		return -EINVAL;
	}
	err = voluta_nkbs_of(VOLUTA_VTYPE_INODE, mode, &nkbs);
	if (err) {
		return err;
	}
	err = require_waccess(env, dir_ii);
	if (err) {
		return err;
	}
	return 0;
}

static int require_nodent(struct voluta_env *env,
			  const struct voluta_inode_info *dir_ii,
			  const struct voluta_qstr *name)
{
	int err;
	struct voluta_ino_dt ino_dt;

	err = voluta_resolve_by_name(env, dir_ii, name, &ino_dt);
	if (err == 0) {
		return -EEXIST;
	}
	if (err != -ENOENT) {
		return err;
	}
	return 0;
}

int voluta_do_create(struct voluta_env *env, struct voluta_inode_info *dir_ii,
		     const struct voluta_qstr *name, mode_t mode,
		     struct voluta_inode_info **out_ii)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	err = check_create(env, dir_ii, mode);
	if (err) {
		return err;
	}
	err = require_nodent(env, dir_ii, name);
	if (err) {
		return err;
	}
	err = voluta_check_add_dentry(dir_ii, name);
	if (err) {
		return err;
	}
	err = require_fileref(env, ii);
	if (err) {
		return err;
	}
	err = new_reg_inode(env, mode, i_ino_of(dir_ii), &ii);
	if (err) {
		return err;
	}
	err = voluta_add_dentry(env, dir_ii, name, ii);
	if (err) {
		/* TODO: Proper cleanups */
		return err;
	}
	voluta_inc_fileref(env, ii);
	i_update_times(env, dir_ii, VOLUTA_ATTR_RMCTIME);

	*out_ii = ii;
	return 0;
}

static int check_mknod(struct voluta_env *env,
		       const struct voluta_inode_info *dir_ii,
		       mode_t mode, dev_t dev)
{
	int err;

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
	err = require_waccess(env, dir_ii);
	if (err) {
		return err;
	}
	return 0;
}

static int setup_special(struct voluta_inode_info *ii, dev_t rdev)
{
	struct voluta_inode *inode = ii->inode;

	if (!i_isfifo(ii) && !i_issock(ii)) {
		return -ENOTSUP;
	}
	inode->i_rdev_major = major(rdev);
	inode->i_rdev_minor = minor(rdev);
	return 0;
}

int voluta_do_mknod(struct voluta_env *env, struct voluta_inode_info *dir_ii,
		    const struct voluta_qstr *name, mode_t mode, dev_t dev,
		    struct voluta_inode_info **out_ii)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	err = require_dir(dir_ii);
	if (err) {
		return err;
	}
	err = check_mknod(env, dir_ii, mode, dev);
	if (err) {
		return err;
	}
	err = require_nodent(env, dir_ii, name);
	if (err) {
		return err;
	}
	err = voluta_check_add_dentry(dir_ii, name);
	if (err) {
		return err;
	}
	err = new_inode(env, mode, i_ino_of(dir_ii), &ii);
	if (err) {
		return err;
	}
	err = setup_special(ii, dev);
	if (err) {
		/* TODO: Proper cleanups */
		return err;
	}
	err = voluta_add_dentry(env, dir_ii, name, ii);
	if (err) {
		/* TODO: Proper cleanups */
		return err;
	}
	i_update_times(env, dir_ii, VOLUTA_ATTR_RMCTIME);

	*out_ii = ii;
	return 0;
}

static int check_open(struct voluta_env *env,
		      const struct voluta_inode_info *ii, int flags)
{
	int err;

	if (flags & O_DIRECTORY) {
		return -EISDIR;
	}
	if (flags & (O_CREAT | O_EXCL)) {
		return -EEXIST; /* XXX ? */
	}
	err = require_fileref(env, ii);
	if (err) {
		return err;
	}
	return 0;
}

static int oflags_to_access_mask(int flags)
{
	int access_mask = 0;

	if ((flags & O_RDWR) == O_RDWR) {
		access_mask = R_OK | W_OK;
	} else if ((flags & O_WRONLY) == O_WRONLY) {
		access_mask = W_OK;
	} else if ((flags & O_RDONLY) == O_RDONLY) {
		access_mask = R_OK;
	}
	if ((flags & O_TRUNC) == O_TRUNC) {
		access_mask |= W_OK;
	}
	if ((flags & O_APPEND) == O_APPEND) {
		access_mask |= W_OK;
	}
	return access_mask;
}

static int post_open(struct voluta_env *env, struct voluta_inode_info *ii,
		     int flags)
{
	int err;

	if ((flags & O_TRUNC) && i_isreg(ii)) {
		err = voluta_do_truncate(env, ii, 0);
	} else {
		err = 0;
	}
	return err;
}

int voluta_open(struct voluta_env *env, struct voluta_inode_info *ii, int flags)
{
	int err, mask;

	err = require_reg_or_fifo(ii);
	if (err) {
		return err;
	}
	err = check_open(env, ii, flags);
	if (err) {
		return err;
	}
	mask = oflags_to_access_mask(flags);
	err = voluta_do_access(env, ii, mask);
	if (err) {
		return err;
	}
	err = voluta_inc_fileref(env, ii);
	if (err) {
		return err;
	}
	err = post_open(env, ii, flags);
	if (err) {
		return err;
	}
	return 0;
}

static int drop_ispecific(struct voluta_env *env, struct voluta_inode_info *ii)
{
	int err;

	if (i_isdir(ii)) {
		err = voluta_drop_dir(env, ii);
	} else if (i_isreg(ii)) {
		err = voluta_drop_reg(env, ii);
	} else if (i_islnk(ii)) {
		err = voluta_drop_symlink(env, ii);
	} else {
		err = 0;
	}
	return err;
}

static int update_or_drop_unlinked(struct voluta_env *env,
				   struct voluta_inode_info *ii)
{
	int err;
	const nlink_t nlink = i_nlink_of(ii);

	if (!i_isdir(ii) && (nlink || i_refcnt_of(ii))) {
		i_update_times(env, ii, VOLUTA_ATTR_CTIME);
		return 0;
	}
	err = voluta_drop_xattr(env, ii);
	if (err) {
		return err;
	}
	err = drop_ispecific(env, ii);
	if (err) {
		return err;
	}
	err = voluta_del_inode(env, ii);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_do_unlink(struct voluta_env *env, struct voluta_inode_info *dir_ii,
		     const struct voluta_qstr *name)
{
	int err;
	ino_t ino;
	struct voluta_inode_info *ii;

	err = require_dir(dir_ii);
	if (err) {
		return err;
	}
	err = require_waccess(env, dir_ii);
	if (err) {
		return err;
	}
	err = voluta_lookup_by_iname(env, dir_ii, name, &ino);
	if (err) {
		return err;
	}
	err = voluta_stage_inode(env, ino, &ii);
	if (err) {
		return err;
	}
	err = require_notdir(ii);
	if (err) {
		return err;
	}
	err = check_sticky(env, dir_ii, ii);
	if (err) {
		return err;
	}
	err = voluta_remove_dentry(env, dir_ii, name);
	if (err) {
		return err;
	}
	err = update_or_drop_unlinked(env, ii);
	if (err) {
		return err;
	}
	i_update_times(env, dir_ii, VOLUTA_ATTR_MCTIME);
	return 0;
}

static int require_nomlink(const struct voluta_inode_info *ii)
{
	const size_t link_max = VOLUTA_LINK_MAX;

	return (i_nlink_of(ii) < link_max) ? 0 : -EMLINK;
}

static int
check_link(struct voluta_env *env, struct voluta_inode_info *dir_ii,
	   const struct voluta_qstr *name, struct voluta_inode_info *ii)
{
	int err;

	err = require_dir(dir_ii);
	if (err) {
		return err;
	}
	err = require_notdir(ii);
	if (err) {
		return err;
	}
	err = require_waccess(env, dir_ii);
	if (err) {
		return err;
	}
	err = require_nodent(env, dir_ii, name);
	if (err) {
		return err;
	}
	err = require_notdir(ii);
	if (err) {
		return err;
	}
	err = require_nomlink(ii);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_do_link(struct voluta_env *env, struct voluta_inode_info *dir_ii,
		   const struct voluta_qstr *name, struct voluta_inode_info *ii)
{
	int err;

	err = check_link(env, dir_ii, name, ii);
	if (err) {
		return err;
	}
	err = voluta_add_dentry(env, dir_ii, name, ii);
	if (err) {
		/* TODO: Proper cleanups */
		return err;
	}
	i_update_times(env, dir_ii, VOLUTA_ATTR_MCTIME);
	i_update_times(env, ii, VOLUTA_ATTR_CTIME);

	return 0;
}

int voluta_inc_fileref(const struct voluta_env *env,
		       struct voluta_inode_info *ii)
{
	int err;

	err = require_fileref(env, ii);
	if (!err) {
		ii->vi.ce.ce_refcnt += 1;
	}
	return err;
}

int voluta_do_release(struct voluta_env *env, struct voluta_inode_info *ii)
{
	int err;

	if (!i_refcnt_of(ii)) {
		return -EBADF;
	}
	if (i_isdir(ii)) {
		return -EINVAL;
	}
	ii->vi.ce.ce_refcnt -= 1;
	err = update_or_drop_unlinked(env, ii);
	if (err) {
		return err;
	}
	return 0;
}

static int check_mkdir(struct voluta_env *env,
		       struct voluta_inode_info *dir_ii,
		       const struct voluta_qstr *name)
{
	int err;

	err = require_dir(dir_ii);
	if (err) {
		return err;
	}
	err = require_waccess(env, dir_ii);
	if (err) {
		return err;
	}
	err = require_nodent(env, dir_ii, name);
	if (err) {
		return err;
	}
	err = require_nomlink(dir_ii);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_do_mkdir(struct voluta_env *env, struct voluta_inode_info *dir_ii,
		    const struct voluta_qstr *name, mode_t mode,
		    struct voluta_inode_info **out_ii)
{
	int err;
	struct voluta_inode_info *ii;

	err = check_mkdir(env, dir_ii, name);
	if (err) {
		return err;
	}
	err = voluta_check_add_dentry(dir_ii, name);
	if (err) {
		return err;
	}
	err = new_dir_inode(env, mode, i_ino_of(dir_ii), &ii);
	if (err) {
		return err;
	}
	err = voluta_add_dentry(env, dir_ii, name, ii);
	if (err) {
		/* TODO: Poreper cleanups */
		return err;
	}
	i_update_times(env, dir_ii, VOLUTA_ATTR_RMCTIME);

	*out_ii = ii;
	return 0;
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

static int check_rmdir(const struct voluta_env *env,
		       const struct voluta_inode_info *parent_ii,
		       const struct voluta_inode_info *dir_ii)
{
	int err;

	err = require_dir(dir_ii);
	if (err) {
		return err;
	}
	if (!dir_isempty(dir_ii)) {
		return -ENOTEMPTY;
	}
	if (i_isrootd(dir_ii)) {
		return -EBUSY;
	}
	/* TODO: FIXME; ext4 allow */
	if (i_refcnt_of(dir_ii)) {
		return -EBUSY;
	}
	err = check_sticky(env, parent_ii, dir_ii);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_do_rmdir(struct voluta_env *env, struct voluta_inode_info *dir_ii,
		    const struct voluta_qstr *name)
{
	int err;
	ino_t ino;
	struct voluta_inode_info *ii = NULL;

	err = require_dir(dir_ii);
	if (err) {
		return err;
	}
	err = require_waccess(env, dir_ii);
	if (err) {
		return err;
	}
	err = voluta_lookup_by_dname(env, dir_ii, name, &ino);
	if (err) {
		return err;
	}
	err = voluta_stage_inode(env, ino, &ii);
	if (err) {
		return err;
	}
	err = check_rmdir(env, dir_ii, ii);
	if (err) {
		return err;
	}
	err = voluta_remove_dentry(env, dir_ii, name);
	if (err) {
		return err;
	}
	err = update_or_drop_unlinked(env, ii);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_symval_to_str(const char *symval, struct voluta_str *str)
{
	size_t len;

	len = strnlen(symval, VOLUTA_SYMLNK_MAX + 1);
	if (len > VOLUTA_SYMLNK_MAX) {
		return -ENAMETOOLONG;
	}
	str->len = len;
	str->str = symval;
	return 0;
}

int voluta_do_symlink(struct voluta_env *env,
		      struct voluta_inode_info *dir_ii,
		      const struct voluta_qstr *name,
		      const struct voluta_str *linkpath,
		      struct voluta_inode_info **out_ii)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	err = require_dir(dir_ii);
	if (err) {
		return err;
	}
	err = require_nodent(env, dir_ii, name);
	if (err) {
		return err;
	}
	err = voluta_check_add_dentry(dir_ii, name);
	if (err) {
		return err;
	}
	err = new_lnk_inode(env, i_ino_of(dir_ii), &ii);
	if (err) {
		return err;
	}
	err = voluta_setup_symlink(env, ii, linkpath);
	if (err) {
		/* TODO: Cleanups */
		return err;
	}
	err = voluta_add_dentry(env, dir_ii, name, ii);
	if (err) {
		/* TODO: Poreper cleanups */
		return err;
	}
	i_update_times(env, dir_ii, VOLUTA_ATTR_RMCTIME);

	*out_ii = ii;
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct voluta_rename_ctx {
	struct voluta_env *env;
	struct voluta_inode_info *dir_ii;
	const struct voluta_qstr *name;
	struct voluta_inode_info *ii;
	struct voluta_inode_info *newdir_ii;
	const struct voluta_qstr *newname;
	struct voluta_inode_info *old_ii;
	int flags;
};


static int rename_move(const struct voluta_rename_ctx *rename_ctx)
{
	int err;
	struct voluta_env *env = rename_ctx->env;
	struct voluta_inode_info *dir_ii = rename_ctx->dir_ii;
	struct voluta_inode_info *newdir_ii = rename_ctx->newdir_ii;
	struct voluta_inode_info *ii = rename_ctx->ii;
	const struct voluta_qstr *name = rename_ctx->name;
	const struct voluta_qstr *newname = rename_ctx->newname;

	err = voluta_check_add_dentry(newdir_ii, newname);
	if (err) {
		return err;
	}
	err = voluta_remove_dentry(env, dir_ii, name);
	if (err) {
		return err;
	}
	err = voluta_add_dentry(env, newdir_ii, newname, ii);
	if (err) {
		return err;
	}
	return 0;
}

static int rename_unlink(const struct voluta_rename_ctx *rename_ctx)
{
	struct voluta_env *env = rename_ctx->env;
	struct voluta_inode_info *dir_ii = rename_ctx->dir_ii;
	const struct voluta_qstr *name = rename_ctx->name;

	return voluta_do_unlink(env, dir_ii, name);
}

static int rename_replace(const struct voluta_rename_ctx *rename_ctx)
{
	int err;
	struct voluta_env *env = rename_ctx->env;
	struct voluta_inode_info *dir_ii = rename_ctx->dir_ii;
	struct voluta_inode_info *newdir_ii = rename_ctx->newdir_ii;
	struct voluta_inode_info *ii = rename_ctx->ii;
	struct voluta_inode_info *old_ii = rename_ctx->old_ii;
	const struct voluta_qstr *name = rename_ctx->name;
	const struct voluta_qstr *newname = rename_ctx->newname;

	err = voluta_remove_dentry(env, dir_ii, name);
	if (err) {
		return err;
	}
	err = voluta_remove_dentry(env, newdir_ii, newname);
	if (err) {
		return err;
	}
	err = voluta_add_dentry(env, newdir_ii, newname, ii);
	if (err) {
		return err;
	}
	err = update_or_drop_unlinked(env, old_ii);
	if (err) {
		return err;
	}
	return 0;
}

static int rename_exchange(const struct voluta_rename_ctx *rename_ctx)
{
	int err;
	struct voluta_env *env = rename_ctx->env;
	struct voluta_inode_info *dir_ii = rename_ctx->dir_ii;
	struct voluta_inode_info *newdir_ii = rename_ctx->newdir_ii;
	struct voluta_inode_info *ii = rename_ctx->ii;
	struct voluta_inode_info *old_ii = rename_ctx->old_ii;
	const struct voluta_qstr *name = rename_ctx->name;
	const struct voluta_qstr *newname = rename_ctx->newname;

	err = voluta_remove_dentry(env, dir_ii, name);
	if (err) {
		return err;
	}
	err = voluta_remove_dentry(env, newdir_ii, newname);
	if (err) {
		return err;
	}
	err = voluta_add_dentry(env, newdir_ii, newname, ii);
	if (err) {
		return err;
	}
	err = voluta_add_dentry(env, dir_ii, name, old_ii);
	if (err) {
		return err;
	}
	return 0;
}

static int rename_safely(const struct voluta_rename_ctx *rename_ctx)
{
	int err;
	struct voluta_env *env = rename_ctx->env;
	struct voluta_inode_info *dir_ii = rename_ctx->dir_ii;
	struct voluta_inode_info *newdir_ii = rename_ctx->newdir_ii;
	struct voluta_inode_info *ii = rename_ctx->ii;
	struct voluta_inode_info *old_ii = rename_ctx->old_ii;
	const int flags = rename_ctx->flags;

	if (old_ii == NULL) {
		err = rename_move(rename_ctx);
	} else if (ii == old_ii) {
		err = rename_unlink(rename_ctx);
		ii = old_ii = NULL;
	} else if (flags & RENAME_EXCHANGE) {
		err = rename_exchange(rename_ctx);
	} else {
		err = rename_replace(rename_ctx);
		old_ii = NULL;
	}
	i_update_times(env, dir_ii, VOLUTA_ATTR_MCTIME);
	i_update_times(env, newdir_ii, VOLUTA_ATTR_MCTIME);
	i_update_times(env, ii, VOLUTA_ATTR_CTIME);
	i_update_times(env, old_ii, VOLUTA_ATTR_CTIME);
	return err;
}

static int check_rename_flags(const struct voluta_rename_ctx *rename_ctx)
{
	const int flags = rename_ctx->flags;
	const struct voluta_inode_info *old_ii = rename_ctx->old_ii;

	if (flags & RENAME_WHITEOUT) {
		return -EINVAL;
	}
	if (flags & ~(RENAME_NOREPLACE | RENAME_EXCHANGE)) {
		return -EINVAL;
	}
	if ((flags & RENAME_NOREPLACE) && old_ii) {
		return -EEXIST;
	}
	if ((flags & RENAME_EXCHANGE) && !old_ii) {
		return -ENOENT;
	}
	return 0;
}

static int check_rename(const struct voluta_rename_ctx *rename_ctx)
{
	int err;
	struct voluta_env *env = rename_ctx->env;
	const struct voluta_inode_info *dir_ii = rename_ctx->dir_ii;
	const struct voluta_inode_info *ii = rename_ctx->ii;
	const struct voluta_inode_info *newdir_ii = rename_ctx->newdir_ii;
	const struct voluta_inode_info *old_ii = rename_ctx->old_ii;
	const int flags = rename_ctx->flags;

	err = check_rename_flags(rename_ctx);
	if (err) {
		return err;
	}
	err = require_waccess(env, dir_ii);
	if (err) {
		return err;
	}
	err = require_waccess(env, newdir_ii);
	if (err) {
		return err;
	}
	err = check_sticky(env, dir_ii, ii);
	if (err) {
		return err;
	}
	err = check_sticky(env, newdir_ii, old_ii);
	if (err) {
		return err;
	}
	if (flags & RENAME_EXCHANGE) {
		if (!ii) {
			return -EINVAL;
		}
		if ((ii != old_ii) && (i_isdir(ii) != i_isdir(old_ii))) {
			if (i_isdir(old_ii)) {
				err = require_nomlink(newdir_ii);
			} else  {
				err = require_nomlink(dir_ii);
			}
		}
	} else if (old_ii && i_isdir(old_ii) && (old_ii != ii)) {
		if (ii) {
			err = check_rmdir(env, dir_ii, old_ii);
		} else {
			err = require_nomlink(newdir_ii);
		}
	}
	return err;
}

int voluta_do_rename(struct voluta_env *env,
		     struct voluta_inode_info *dir_ii,
		     const struct voluta_qstr *name,
		     struct voluta_inode_info *newdir_ii,
		     const struct voluta_qstr *newname, int flags)
{
	int err;
	struct voluta_rename_ctx rename_ctx = {
		.env = env,
		.dir_ii = dir_ii,
		.name = name,
		.ii = NULL,
		.newdir_ii = newdir_ii,
		.newname = newname,
		.old_ii = NULL,
		/* TODO: Use flags (RENAME_EXCHANGE|RENAME_NOREPLACE) */
		.flags = flags
	};

	err = voluta_do_lookup(env, dir_ii, name, &rename_ctx.ii);
	if (err) {
		return err;
	}
	err = voluta_do_lookup(env, newdir_ii, newname, &rename_ctx.old_ii);
	if (err && (err != -ENOENT)) {
		return err;
	}
	err = check_rename(&rename_ctx);
	if (err) {
		return err;
	}
	err = rename_safely(&rename_ctx);
	if (err) {
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

union voluta_utf32_name_buf {
	char dat[4 * (VOLUTA_NAME_MAX + 1)];
	uint32_t utf32[VOLUTA_NAME_MAX + 1];
} voluta_aligned64;


static int name_to_hash(const struct voluta_env *env, const char *name,
			size_t name_len, struct voluta_hash256 *out_hash)
{
	int err;
	char *in, *out;
	size_t len, ret, outlen, datlen;
	const struct voluta_crypto *crypto = &env->crypto;
	union voluta_utf32_name_buf outbuf;

	in = (char *)name;
	len = name_len;
	out = outbuf.dat;
	outlen = sizeof(outbuf.dat);
	ret = iconv(env->iconv_handle, &in, &len, &out, &outlen);
	if ((ret != 0) || len || (outlen % 4)) {
		return errno ? -errno : -EINVAL;
	}
	datlen = sizeof(outbuf.dat) - outlen;
	err = voluta_sha256_of(crypto, outbuf.dat, datlen, out_hash);
	if (err) {
		return err;
	}
	return 0;
}

static int require_valid_name(const char *name, size_t len)
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

int voluta_name_to_str(struct voluta_env *env,
		       const struct voluta_inode_info *ii,
		       const char *name, struct voluta_str *str)
{
	int err;
	size_t len;

	len = strnlen(name, VOLUTA_NAME_MAX + 1);
	err = require_valid_name(name, len);
	if (err) {
		return err;
	}
	str->str = name;
	str->len = len;

	/* TODO: if ii is dir, Use flags to verify names (+xattr-names) */
	voluta_unused(env);
	voluta_unused(ii);
	return 0;
}

int voluta_name_to_qstr(struct voluta_env *env,
			const struct voluta_inode_info *dir_ii,
			const char *name, struct voluta_qstr *qstr)
{
	int err;

	err = require_dir(dir_ii);
	if (err) {
		return err;
	}
	err = voluta_name_to_str(env, dir_ii, name, &qstr->str);
	if (err) {
		return err;
	}
	err = name_to_hash(env, qstr->str.str, qstr->str.len, &qstr->hash);
	if (err) {
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fsblkcnt_t bytes_to_fsblkcnt(size_t nbytes)
{
	return nbytes / VOLUTA_BK_SIZE;
}

int voluta_do_statvfs(struct voluta_env *env, struct statvfs *stvfs)
{
	const struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_sp_stat *sps = &sbi->s_sp_stat;

	memset(stvfs, 0, sizeof(*stvfs));
	stvfs->f_bsize = VOLUTA_BK_SIZE;
	stvfs->f_frsize = VOLUTA_BK_SIZE;
	stvfs->f_namemax = VOLUTA_NAME_MAX;

	stvfs->f_blocks = bytes_to_fsblkcnt(sps->sp_nbytes);
	stvfs->f_bfree = bytes_to_fsblkcnt(sps->sp_nbytes -
					   (sps->sp_nmeta + sps->sp_ndata));
	stvfs->f_bavail = stvfs->f_bfree;
	stvfs->f_files = (sps->sp_nbytes / 2);
	stvfs->f_ffree = stvfs->f_files - sps->sp_nfiles;
	stvfs->f_favail = stvfs->f_ffree;

	return 0;
}

