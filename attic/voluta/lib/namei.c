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
#include "voluta-lib.h"

static const struct voluta_ucred *ucred_of(const struct voluta_env *env)
{
	return &env->opi.ucred;
}

static bool isowner(const struct voluta_oper_info *opi,
		    const struct voluta_inode_info *ii)
{
	return uid_eq(opi->ucred.uid, i_uid_of(ii));
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

static int require_fileref(const struct voluta_oper_info *opi,
			   const struct voluta_inode_info *ii)
{
	int err = 0;
	const size_t open_max = 1U << 16; /* XXX */
	const size_t open_cur = i_refcnt_of(ii);

	if ((open_cur >= open_max) && !uid_eq(opi->ucred.uid, 0)) {
		err = -ENFILE;
	}
	return err;
}

static bool has_sticky_bit(const struct voluta_inode_info *dir_ii)
{
	const mode_t mode = i_mode_of(dir_ii);

	return ((mode & S_ISVTX) == S_ISVTX);
}

static int check_sticky(const struct voluta_oper_info *opi,
			const struct voluta_inode_info *dir_ii,
			const struct voluta_inode_info *ii)
{

	if (!has_sticky_bit(dir_ii)) {
		return 0; /* No sticky-bit, we're fine */
	}
	if (isowner(opi, dir_ii)) {
		return 0;
	}
	if (ii && isowner(opi, ii)) {
		return 0;
	}
	/* TODO: Check CAP_FOWNER */
	return -EPERM;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int new_inode(struct voluta_sb_info *sbi,
		     const struct voluta_oper_info *opi,
		     mode_t mode, ino_t parent_ino,
		     struct voluta_inode_info **out_ii)
{
	return voluta_new_inode(sbi, opi, mode, parent_ino, out_ii);
}

static int new_dir_inode(struct voluta_sb_info *sbi,
			 const struct voluta_oper_info *opi,
			 mode_t mode, ino_t parent_ino,
			 struct voluta_inode_info **out_ii)
{
	const mode_t ifmt = S_IFMT;
	const mode_t imod = (mode & ~ifmt) | S_IFDIR;

	return new_inode(sbi, opi, imod, parent_ino, out_ii);
}

static int new_reg_inode(struct voluta_sb_info *sbi,
			 const struct voluta_oper_info *opi,
			 mode_t mode, ino_t parent_ino,
			 struct voluta_inode_info **out_ii)
{
	const mode_t ifmt = S_IFMT;
	const mode_t imod = (mode & ~ifmt) | S_IFREG;

	return new_inode(sbi, opi, imod, parent_ino, out_ii);
}

static int new_lnk_inode(struct voluta_sb_info *sbi,
			 const struct voluta_oper_info *opi, ino_t parent_ino,
			 struct voluta_inode_info **out_ii)
{
	const mode_t imod = S_IFLNK | S_IRWXU | S_IRWXG | S_IRWXO;

	return voluta_new_inode(sbi, opi, imod, parent_ino, out_ii);
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

int voluta_do_access(const struct voluta_oper_info *opi,
		     const struct voluta_inode_info *ii, int mode)
{
	mode_t rwx = 0;
	const uid_t uid = opi->ucred.uid;
	const gid_t gid = opi->ucred.gid;
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

static int require_waccess(const struct voluta_oper_info *opi,
			   const struct voluta_inode_info *ii)
{
	return voluta_do_access(opi, ii, W_OK);
}

static int require_xaccess(const struct voluta_oper_info *opi,
			   const struct voluta_inode_info *ii)
{
	return voluta_do_access(opi, ii, X_OK);
}

int voluta_do_lookup(struct voluta_sb_info *sbi,
		     const struct voluta_oper_info *opi,
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
	err = require_xaccess(opi, dir_ii);
	if (err) {
		return err;
	}
	err = voluta_resolve_by_name(sbi, opi, dir_ii, qstr, &ino_dt);
	if (err) {
		return err;
	}
	err = voluta_stage_inode(sbi, ino_dt.ino, &ii);
	if (err) {
		return err;
	}
	*out_ii = ii;
	return 0;
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

static int check_create(const struct voluta_oper_info *opi,
			const struct voluta_inode_info *dir_ii, mode_t mode)
{
	int err;

	err = require_dir(dir_ii);
	if (err) {
		return err;
	}
	err = check_create_mode(mode);
	if (err) {
		return err;
	}
	err = require_waccess(opi, dir_ii);
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
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_info *opi = opi_of(env);

	err = voluta_resolve_by_name(sbi, opi, dir_ii, name, &ino_dt);
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
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_info *opi = opi_of(env);

	err = check_create(opi, dir_ii, mode);
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
	err = require_fileref(opi, ii);
	if (err) {
		return err;
	}
	err = new_reg_inode(sbi, opi, mode, i_ino_of(dir_ii), &ii);
	if (err) {
		return err;
	}
	err = voluta_add_dentry(sbi, opi, dir_ii, name, ii);
	if (err) {
		/* TODO: Proper cleanups */
		return err;
	}
	voluta_inc_fileref(opi, ii);
	i_update_times(dir_ii, opi, VOLUTA_ATTR_MCTIME);

	*out_ii = ii;
	return 0;
}

static int check_mknod(const struct voluta_oper_info *opi,
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
	err = require_waccess(opi, dir_ii);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_do_mknod(struct voluta_env *env, struct voluta_inode_info *dir_ii,
		    const struct voluta_qstr *name, mode_t mode, dev_t dev,
		    struct voluta_inode_info **out_ii)
{
	int err;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_info *opi = opi_of(env);

	err = require_dir(dir_ii);
	if (err) {
		return err;
	}
	err = check_mknod(opi, dir_ii, mode, dev);
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
	err = new_inode(sbi, opi, mode, i_ino_of(dir_ii), &ii);
	if (err) {
		return err;
	}
	err = voluta_setup_ispecial(ii, dev);
	if (err) {
		/* TODO: Proper cleanups */
		return err;
	}
	err = voluta_add_dentry(sbi, opi, dir_ii, name, ii);
	if (err) {
		/* TODO: Proper cleanups */
		return err;
	}
	i_update_times(dir_ii, opi, VOLUTA_ATTR_MCTIME);

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
	err = require_fileref(&env->opi, ii);
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

int voluta_do_open(struct voluta_env *env,
		   struct voluta_inode_info *ii, int flags)
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
	err = voluta_do_access(&env->opi, ii, mask);
	if (err) {
		return err;
	}
	err = voluta_inc_fileref(&env->opi, ii);
	if (err) {
		return err;
	}
	err = post_open(env, ii, flags);
	if (err) {
		return err;
	}
	return 0;
}

static int drop_ispecific(struct voluta_sb_info *sbi,
			  struct voluta_inode_info *ii)
{
	int err;

	if (i_isdir(ii)) {
		err = voluta_drop_dir(sbi, ii);
	} else if (i_isreg(ii)) {
		err = voluta_drop_reg(sbi, ii);
	} else if (i_islnk(ii)) {
		err = voluta_drop_symlink(sbi, ii);
	} else {
		err = 0;
	}
	return err;
}

static int drop_unlinked(struct voluta_sb_info *sbi,
			 const struct voluta_oper_info *opi,
			 struct voluta_inode_info *ii)
{
	int err;

	err = voluta_drop_xattr(sbi, opi, ii);
	if (err) {
		return err;
	}
	err = drop_ispecific(sbi, ii);
	if (err) {
		return err;
	}
	err = voluta_del_inode(sbi, ii);
	if (err) {
		return err;
	}
	return 0;
}

static bool i_isdropable(const struct voluta_inode_info *ii)
{
	bool res;

	if (!i_isdir(ii)) {
		res = !i_nlink_of(ii) && !i_refcnt_of(ii);
	} else {
		res = (i_nlink_of(ii) <= 2);
	}
	return res;
}

static int try_drop_unlinked(struct voluta_sb_info *sbi,
			     const struct voluta_oper_info *opi,
			     struct voluta_inode_info *ii, bool update_ctime)
{
	int err = 0;

	if (i_isdropable(ii)) {
		err = drop_unlinked(sbi, opi, ii);
	} else if (update_ctime) {
		i_update_times(ii, opi, VOLUTA_ATTR_CTIME);
	}
	return err;
}

static int lookup_by_name(struct voluta_sb_info *sbi,
			  const struct voluta_oper_info *opi,
			  const struct voluta_inode_info *dir_ii,
			  const struct voluta_qstr *name,
			  bool expecting_dir, ino_t *out_ino)
{
	int err;
	struct voluta_ino_dt ino_dt;

	err = voluta_resolve_by_name(sbi, opi, dir_ii, name, &ino_dt);
	if (err) {
		return err;
	}
	if (expecting_dir) {
		if (ino_dt.dt != DT_DIR) {
			return -ENOTDIR;
		}
	} else {
		if (ino_dt.dt == DT_DIR) {
			return -EISDIR;
		}
	}
	*out_ino = ino_dt.ino;
	return 0;
}

static int lookup_by_dname(struct voluta_sb_info *sbi,
			   const struct voluta_oper_info *opi,
			   const struct voluta_inode_info *dir_ii,
			   const struct voluta_qstr *name, ino_t *out_ino)
{
	return lookup_by_name(sbi, opi, dir_ii, name, true, out_ino);
}

static int lookup_by_iname(struct voluta_sb_info *sbi,
			   const struct voluta_oper_info *opi,
			   const struct voluta_inode_info *dir_ii,
			   const struct voluta_qstr *name, ino_t *out_ino)
{
	return lookup_by_name(sbi, opi, dir_ii, name, false, out_ino);
}

int voluta_do_unlink(struct voluta_env *env, struct voluta_inode_info *dir_ii,
		     const struct voluta_qstr *name)
{
	int err;
	ino_t ino;
	struct voluta_inode_info *ii;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_info *opi = opi_of(env);

	err = require_dir(dir_ii);
	if (err) {
		return err;
	}
	err = require_waccess(opi, dir_ii);
	if (err) {
		return err;
	}
	err = lookup_by_iname(sbi, opi, dir_ii, name, &ino);
	if (err) {
		return err;
	}
	err = voluta_stage_inode(sbi, ino, &ii);
	if (err) {
		return err;
	}
	err = require_notdir(ii);
	if (err) {
		return err;
	}
	err = check_sticky(opi, dir_ii, ii);
	if (err) {
		return err;
	}
	err = voluta_remove_dentry(sbi, opi, dir_ii, name);
	if (err) {
		return err;
	}
	err = try_drop_unlinked(sbi, opi, ii, true);
	if (err) {
		return err;
	}
	i_update_times(dir_ii, opi, VOLUTA_ATTR_MCTIME);
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
	const struct voluta_oper_info *opi = &env->opi;

	err = require_dir(dir_ii);
	if (err) {
		return err;
	}
	err = require_notdir(ii);
	if (err) {
		return err;
	}
	err = require_waccess(opi, dir_ii);
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
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_info *opi = opi_of(env);

	err = check_link(env, dir_ii, name, ii);
	if (err) {
		return err;
	}
	err = voluta_add_dentry(sbi, opi, dir_ii, name, ii);
	if (err) {
		/* TODO: Proper cleanups */
		return err;
	}
	i_update_times(dir_ii, opi, VOLUTA_ATTR_MCTIME);
	i_update_times(ii, opi, VOLUTA_ATTR_CTIME);

	return 0;
}

int voluta_inc_fileref(const struct voluta_oper_info *opi,
		       struct voluta_inode_info *ii)
{
	int err;

	err = require_fileref(opi, ii);
	if (err) {
		return err;
	}
	ii->vi.ce.ce_refcnt += 1;
	return 0;
}

int voluta_do_release(struct voluta_env *env, struct voluta_inode_info *ii)
{
	int err;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_info *opi = opi_of(env);

	if (!i_refcnt_of(ii)) {
		return -EBADF;
	}
	if (i_isdir(ii)) {
		return -EINVAL;
	}
	ii->vi.ce.ce_refcnt -= 1;
	err = try_drop_unlinked(sbi, opi, ii, false);
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
	const struct voluta_oper_info *opi = &env->opi;

	err = require_dir(dir_ii);
	if (err) {
		return err;
	}
	err = require_waccess(opi, dir_ii);
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
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_info *opi = opi_of(env);

	err = check_mkdir(env, dir_ii, name);
	if (err) {
		return err;
	}
	err = voluta_check_add_dentry(dir_ii, name);
	if (err) {
		return err;
	}
	err = new_dir_inode(sbi, opi, mode, i_ino_of(dir_ii), &ii);
	if (err) {
		return err;
	}
	err = voluta_add_dentry(sbi, opi, dir_ii, name, ii);
	if (err) {
		/* TODO: Poreper cleanups */
		return err;
	}
	i_update_times(dir_ii, opi, VOLUTA_ATTR_MCTIME);

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

static int check_rmdir(const struct voluta_oper_info *opi,
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
	err = check_sticky(opi, parent_ii, dir_ii);
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
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_info *opi = opi_of(env);

	err = require_dir(dir_ii);
	if (err) {
		return err;
	}
	err = require_waccess(opi, dir_ii);
	if (err) {
		return err;
	}
	err = lookup_by_dname(sbi, opi, dir_ii, name, &ino);
	if (err) {
		return err;
	}
	err = voluta_stage_inode(sbi, ino, &ii);
	if (err) {
		return err;
	}
	err = check_rmdir(&env->opi, dir_ii, ii);
	if (err) {
		return err;
	}
	err = voluta_remove_dentry(sbi, opi, dir_ii, name);
	if (err) {
		return err;
	}
	err = try_drop_unlinked(sbi, opi, ii, false);
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
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_info *opi = opi_of(env);

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
	err = new_lnk_inode(sbi, opi, i_ino_of(dir_ii), &ii);
	if (err) {
		return err;
	}
	err = voluta_setup_symlink(sbi, &env->opi, ii, linkpath);
	if (err) {
		/* TODO: Cleanups */
		return err;
	}
	err = voluta_add_dentry(sbi, opi, dir_ii, name, ii);
	if (err) {
		/* TODO: Poreper cleanups */
		return err;
	}
	i_update_times(dir_ii, opi, VOLUTA_ATTR_MCTIME);

	*out_ii = ii;
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct voluta_rename_ctx {
	struct voluta_env *env;
	struct voluta_sb_info *sbi;
	const struct voluta_oper_info *opi;
	struct voluta_inode_info *dir_ii;
	const struct voluta_qstr *name;
	struct voluta_inode_info *ii;
	struct voluta_inode_info *newdir_ii;
	const struct voluta_qstr *newname;
	struct voluta_inode_info *old_ii;
	int flags;
};


static int rename_move(const struct voluta_rename_ctx *rc)
{
	int err;

	err = voluta_check_add_dentry(rc->newdir_ii, rc->newname);
	if (err) {
		return err;
	}
	err = voluta_remove_dentry(rc->sbi, rc->opi, rc->dir_ii, rc->name);
	if (err) {
		return err;
	}
	err = voluta_add_dentry(rc->sbi, rc->opi, rc->newdir_ii,
				rc->newname, rc->ii);
	if (err) {
		return err;
	}
	return 0;
}

static int rename_unlink(const struct voluta_rename_ctx *rc)
{
	return voluta_do_unlink(rc->env, rc->dir_ii, rc->name);
}

static int rename_replace(const struct voluta_rename_ctx *rc)
{
	int err;

	err = voluta_remove_dentry(rc->sbi, rc->opi, rc->dir_ii, rc->name);
	if (err) {
		return err;
	}
	err = voluta_remove_dentry(rc->sbi, rc->opi,
				   rc->newdir_ii, rc->newname);
	if (err) {
		return err;
	}
	err = voluta_add_dentry(rc->sbi, rc->opi,
				rc->newdir_ii, rc->newname, rc->ii);
	if (err) {
		return err;
	}
	err = try_drop_unlinked(rc->sbi, rc->opi, rc->old_ii, true);
	if (err) {
		return err;
	}
	return 0;
}

static int rename_exchange(const struct voluta_rename_ctx *rc)
{
	int err;

	err = voluta_remove_dentry(rc->sbi, rc->opi, rc->dir_ii, rc->name);
	if (err) {
		return err;
	}
	err = voluta_remove_dentry(rc->sbi, rc->opi,
				   rc->newdir_ii, rc->newname);
	if (err) {
		return err;
	}
	err = voluta_add_dentry(rc->sbi, rc->opi,
				rc->newdir_ii, rc->newname, rc->ii);
	if (err) {
		return err;
	}
	err = voluta_add_dentry(rc->sbi, rc->opi,
				rc->dir_ii, rc->name, rc->old_ii);
	if (err) {
		return err;
	}
	return 0;
}

static int do_rename(const struct voluta_rename_ctx *rc)
{
	int err;
	struct voluta_oper_info *opi = &rc->env->opi;
	struct voluta_inode_info *dir_ii = rc->dir_ii;
	struct voluta_inode_info *newdir_ii = rc->newdir_ii;
	struct voluta_inode_info *ii = rc->ii;
	struct voluta_inode_info *old_ii = rc->old_ii;

	if (old_ii == NULL) {
		err = rename_move(rc);
	} else if (ii == old_ii) {
		err = rename_unlink(rc);
		ii = old_ii = NULL;
	} else if (rc->flags & RENAME_EXCHANGE) {
		err = rename_exchange(rc);
	} else {
		err = rename_replace(rc);
		old_ii = NULL;
	}
	i_update_times(dir_ii, opi, VOLUTA_ATTR_MCTIME);
	i_update_times(newdir_ii, opi, VOLUTA_ATTR_MCTIME);
	i_update_times(ii, opi, VOLUTA_ATTR_CTIME);
	i_update_times(old_ii, opi, VOLUTA_ATTR_CTIME);
	return err;
}

static int check_rename_flags(const struct voluta_rename_ctx *rc)
{
	const int flags = rc->flags;
	const struct voluta_inode_info *old_ii = rc->old_ii;

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

static int check_rename(const struct voluta_rename_ctx *rc)
{
	int err;
	struct voluta_env *env = rc->env;
	const struct voluta_inode_info *dir_ii = rc->dir_ii;
	const struct voluta_inode_info *ii = rc->ii;
	const struct voluta_inode_info *newdir_ii = rc->newdir_ii;
	const struct voluta_inode_info *old_ii = rc->old_ii;

	err = check_rename_flags(rc);
	if (err) {
		return err;
	}
	err = require_waccess(rc->opi, dir_ii);
	if (err) {
		return err;
	}
	err = require_waccess(rc->opi, newdir_ii);
	if (err) {
		return err;
	}
	err = check_sticky(rc->opi, dir_ii, ii);
	if (err) {
		return err;
	}
	err = check_sticky(rc->opi, newdir_ii, old_ii);
	if (err) {
		return err;
	}
	if (rc->flags & RENAME_EXCHANGE) {
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
			err = check_rmdir(&env->opi, dir_ii, old_ii);
		} else {
			err = require_nomlink(newdir_ii);
		}
	}
	return err;
}

static int resolve_rename(struct voluta_rename_ctx *rc)
{
	int err;

	err = voluta_do_lookup(rc->sbi, rc->opi,
			       rc->dir_ii, rc->name, &rc->ii);
	if (err) {
		return err;
	}
	err = voluta_do_lookup(rc->sbi, rc->opi,
			       rc->newdir_ii, rc->newname, &rc->old_ii);
	if (err && (err != -ENOENT)) {
		return err;
	}
	return 0;
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
		.sbi = sbi_of(env),
		.opi = opi_of(env),
		.dir_ii = dir_ii,
		.name = name,
		.ii = NULL,
		.newdir_ii = newdir_ii,
		.newname = newname,
		.old_ii = NULL,
		/* TODO: Use flags (RENAME_EXCHANGE|RENAME_NOREPLACE) */
		.flags = flags
	};

	err = resolve_rename(&rename_ctx);
	if (err) {
		return err;
	}
	err = check_rename(&rename_ctx);
	if (err) {
		return err;
	}
	err = do_rename(&rename_ctx);
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


static int require_utf8_name(const struct voluta_sb_info *sbi,
			     const char *name, size_t name_len)
{
	char *in, *out;
	size_t len, ret, outlen, datlen;
	union voluta_utf32_name_buf outbuf;

	in = (char *)name;
	len = name_len;
	out = outbuf.dat;
	outlen = sizeof(outbuf.dat);
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

static int name_to_hash(const struct voluta_crypto *crypto,
			const struct voluta_inode_info *dir_ii,
			const char *name, size_t name_len, uint64_t *out_hash)
{
	int err = -EINVAL;
	struct voluta_hash256 sha256;

	if (voluta_dir_hasflag(dir_ii, VOLUTA_DIRF_HASH_SHA256)) {
		err = voluta_sha256_of(crypto, name, name_len, &sha256);
		*out_hash = hash256_to_u64(&sha256);
	}
	return err;
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


static int require_valid_encoding(const struct voluta_sb_info *sbi,
				  const struct voluta_inode_info *ii,
				  const char *name, size_t name_len)
{
	return voluta_has_iname_utf8(ii) ?
	       require_utf8_name(sbi, name, name_len) : -EINVAL;
}

int voluta_name_to_str(const struct voluta_sb_info *sbi,
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
	err = require_valid_encoding(sbi, ii, name, len);
	if (err) {
		return err;
	}
	str->str = name;
	str->len = len;
	return 0;
}

int voluta_name_to_qstr(const struct voluta_sb_info *sbi,
			const struct voluta_inode_info *dir_ii,
			const char *name, struct voluta_qstr *qstr)
{
	int err;
	struct voluta_str *str = &qstr->str;
	const struct voluta_crypto *crypto = sbi->s_cstore->crypto;

	err = require_dir(dir_ii);
	if (err) {
		return err;
	}
	err = voluta_name_to_str(sbi, dir_ii, name, str);
	if (err) {
		return err;
	}
	err = name_to_hash(crypto, dir_ii, str->str, str->len, &qstr->hash);
	if (err) {
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_do_statvfs(const struct voluta_sb_info *sbi, struct statvfs *stvfs)
{
	voluta_memzero(stvfs, sizeof(*stvfs));
	stvfs->f_namemax = VOLUTA_NAME_MAX;
	voluta_space_to_statvfs(sbi, stvfs);
	return 0;
}

