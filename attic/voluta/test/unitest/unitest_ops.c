/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * This file is part of voluta.
 *
 * Copyright (C) 2020 Shachar Sharon
 *
 * Voluta is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Voluta is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/xattr.h>
#include <linux/falloc.h>
#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include <limits.h>
#include "unitest.h"

static struct voluta_sb_info *sbi(struct ut_env *ut_env)
{
	return ut_env->fs_env->sbi;
}

static const struct voluta_oper *op(struct ut_env *ut_env)
{
	struct voluta_oper *oper = &ut_env->oper;

	oper->ucred.uid = getuid();
	oper->ucred.gid = getgid();
	oper->ucred.pid = getpid();
	oper->ucred.umask = 0002;
	oper->unique = ut_env->unique_count++;
	voluta_ts_gettime(&oper->xtime, true);

	return oper;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int ut_statfs(struct ut_env *ut_env,
		     ino_t ino, struct statvfs *st)
{
	return voluta_fs_statfs(sbi(ut_env), op(ut_env), ino, st);
}

static int ut_statx(struct ut_env *ut_env, ino_t ino, struct statx *stx)
{
	return voluta_fs_statx(sbi(ut_env), op(ut_env), ino, stx);
}

static int ut_access(struct ut_env *ut_env, ino_t ino, int mode)
{
	return voluta_fs_access(sbi(ut_env), op(ut_env), ino, mode);
}

static int ut_getattr(struct ut_env *ut_env, ino_t ino, struct stat *st)
{
	return voluta_fs_getattr(sbi(ut_env), op(ut_env), ino, st);
}

static int ut_lookup(struct ut_env *ut_env, ino_t parent,
		     const char *name, struct stat *st)
{
	return voluta_fs_lookup(sbi(ut_env), op(ut_env), parent, name, st);
}

static int ut_utimens(struct ut_env *ut_env, ino_t ino,
		      const struct stat *utimes, struct stat *st)
{
	return voluta_fs_utimens(sbi(ut_env), op(ut_env), ino, utimes, st);
}

static int ut_mkdir(struct ut_env *ut_env, ino_t parent,
		    const char *name, mode_t mode, struct stat *out_st)
{
	return voluta_fs_mkdir(sbi(ut_env), op(ut_env),
			       parent, name, mode | S_IFDIR, out_st);
}

static int ut_rmdir(struct ut_env *ut_env, ino_t parent, const char *name)
{
	return voluta_fs_rmdir(sbi(ut_env), op(ut_env), parent, name);
}

static int ut_opendir(struct ut_env *ut_env, ino_t ino)
{
	return voluta_fs_opendir(sbi(ut_env), op(ut_env), ino);
}

static int ut_releasedir(struct ut_env *ut_env, ino_t ino)
{
	return voluta_fs_releasedir(sbi(ut_env), op(ut_env), ino, 0);
}

static int ut_fsyncdir(struct ut_env *ut_env, ino_t ino, bool datasync)
{
	return voluta_fs_fsyncdir(sbi(ut_env), op(ut_env), ino, datasync);
}


static int ut_symlink(struct ut_env *ut_env, ino_t parent,
		      const char *name, const char *val, struct stat *out_st)
{
	return voluta_fs_symlink(sbi(ut_env), op(ut_env),
				 parent, name, val, out_st);
}

static int ut_readlink(struct ut_env *ut_env,
		       ino_t ino, char *buf, size_t len)
{
	return voluta_fs_readlink(sbi(ut_env), op(ut_env), ino, buf, len);
}

static int ut_link(struct ut_env *ut_env, ino_t ino, ino_t parent,
		   const char *name, struct stat *out_st)
{
	return voluta_fs_link(sbi(ut_env), op(ut_env),
			      ino, parent, name, out_st);
}

static int ut_unlink(struct ut_env *ut_env, ino_t parent, const char *name)
{
	return voluta_fs_unlink(sbi(ut_env), op(ut_env), parent, name);
}

static int ut_create(struct ut_env *ut_env, ino_t parent,
		     const char *name, mode_t mode, struct stat *out_st)
{
	return voluta_fs_create(sbi(ut_env), op(ut_env),
				parent, name, 0, mode, out_st);
}

static int ut_open(struct ut_env *ut_env, ino_t ino, int flags)
{
	return voluta_fs_open(sbi(ut_env), op(ut_env), ino, flags);
}

static int ut_release(struct ut_env *ut_env, ino_t ino)
{
	return voluta_fs_release(sbi(ut_env), op(ut_env), ino, 0, false);
}

static int ut_truncate(struct ut_env *ut_env, ino_t ino,
		       loff_t length, struct stat *out_st)
{
	return voluta_fs_truncate(sbi(ut_env), op(ut_env),
				  ino, length, out_st);
}

static int ut_fsync(struct ut_env *ut_env, ino_t ino, bool datasync)
{
	return voluta_fs_fsync(sbi(ut_env), op(ut_env), ino, datasync);
}

static int ut_rename(struct ut_env *ut_env, ino_t parent,
		     const char *name, ino_t newparent,
		     const char *newname, int flags)
{
	return voluta_fs_rename(sbi(ut_env), op(ut_env), parent, name,
				newparent, newname, flags);
}

static int ut_fiemap(struct ut_env *ut_env, ino_t ino, struct fiemap *fm)
{
	return voluta_fs_fiemap(sbi(ut_env), op(ut_env), ino, fm);
}

static int ut_lseek(struct ut_env *ut_env, ino_t ino,
		    loff_t off, int whence, loff_t *out)
{
	return voluta_fs_lseek(sbi(ut_env), op(ut_env), ino, off, whence, out);
}

static int ut_query(struct ut_env *ut_env, ino_t ino,
		    struct voluta_query *out_qry)
{
	return voluta_fs_query(sbi(ut_env), op(ut_env), ino, out_qry);
}

static int ut_read(struct ut_env *ut_env, ino_t ino, void *buf,
		   size_t len, loff_t off, size_t *out_len)
{
	return voluta_fs_read(sbi(ut_env), op(ut_env),
			      ino, buf, len, off, out_len);
}

static int ut_write(struct ut_env *ut_env, ino_t ino, const void *buf,
		    size_t len, off_t off, size_t *out_len)
{
	return voluta_fs_write(sbi(ut_env), op(ut_env),
			       ino, buf, len, off, out_len);
}

static int ut_fallocate(struct ut_env *ut_env, ino_t ino,
			int mode, loff_t offset, loff_t len)
{
	return voluta_fs_fallocate(sbi(ut_env), op(ut_env),
				   ino, mode, offset, len);
}

static struct ut_readdir_ctx *ut_readdir_ctx_of(struct voluta_readdir_ctx *ptr)
{
	return ut_container_of(ptr, struct ut_readdir_ctx, rd_ctx);
}

static int filldir(struct voluta_readdir_ctx *rd_ctx,
		   const struct voluta_readdir_info *rdi)
{
	size_t ndents_max;
	struct ut_dirent_info *dei;
	struct ut_readdir_ctx *ut_rd_ctx;

	ut_rd_ctx = ut_readdir_ctx_of(rd_ctx);
	ndents_max = UT_ARRAY_SIZE(ut_rd_ctx->dei);

	if ((rdi->off < 0) || !rdi->namelen) {
		return -EINVAL;
	}
	if (ut_rd_ctx->nde >= ndents_max) {
		return -EINVAL;
	}
	dei = &ut_rd_ctx->dei[ut_rd_ctx->nde++];

	ut_assert(rdi->namelen < sizeof(dei->de.d_name));
	memcpy(dei->de.d_name, rdi->name, rdi->namelen);
	dei->de.d_name[rdi->namelen] = '\0';
	dei->de.d_reclen = (uint16_t)rdi->namelen;
	dei->de.d_ino = rdi->ino;
	dei->de.d_type = (uint8_t)rdi->dt;
	dei->de.d_off = rdi->off;
	if (ut_rd_ctx->plus) {
		memcpy(&dei->attr, &rdi->attr, sizeof(dei->attr));
	}
	return 0;
}

static int ut_readdir(struct ut_env *ut_env, ino_t ino, loff_t doff,
		      struct ut_readdir_ctx *ut_rd_ctx)
{
	struct voluta_readdir_ctx *rd_ctx = &ut_rd_ctx->rd_ctx;

	ut_rd_ctx->nde = 0;
	ut_rd_ctx->plus = 0;
	rd_ctx->pos = doff;
	rd_ctx->actor = filldir;
	return voluta_fs_readdir(sbi(ut_env), op(ut_env), ino, rd_ctx);
}

static int ut_readdirplus(struct ut_env *ut_env, ino_t ino, loff_t doff,
			  struct ut_readdir_ctx *ut_rd_ctx)
{
	struct voluta_readdir_ctx *rd_ctx = &ut_rd_ctx->rd_ctx;

	ut_rd_ctx->nde = 0;
	ut_rd_ctx->plus = 1;
	rd_ctx->pos = doff;
	rd_ctx->actor = filldir;
	return voluta_fs_readdirplus(sbi(ut_env), op(ut_env), ino, rd_ctx);
}

static int ut_setxattr(struct ut_env *ut_env, ino_t ino,
		       const char *name, const void *value,
		       size_t size, int flags)
{
	return voluta_fs_setxattr(sbi(ut_env), op(ut_env),
				  ino, name, value, size, flags);
}

static int ut_getxattr(struct ut_env *ut_env, ino_t ino,
		       const char *name, void *buf,
		       size_t size, size_t *out_size)
{
	return voluta_fs_getxattr(sbi(ut_env), op(ut_env),
				  ino, name, buf, size, out_size);
}

static int ut_removexattr(struct ut_env *ut_env, ino_t ino, const char *name)
{
	return voluta_fs_removexattr(sbi(ut_env), op(ut_env), ino, name);
}

static struct ut_listxattr_ctx *
ut_listxattr_ctx_of(struct voluta_listxattr_ctx *ptr)
{
	return ut_container_of(ptr, struct ut_listxattr_ctx,
			       lxa_ctx);
}

static int fillxent(struct voluta_listxattr_ctx *lxa_ctx,
		    const char *name, size_t nlen)
{
	char *xname;
	size_t limit;
	struct ut_listxattr_ctx *ut_lxa_ctx;

	ut_lxa_ctx = ut_listxattr_ctx_of(lxa_ctx);

	limit = sizeof(ut_lxa_ctx->names);
	if (ut_lxa_ctx->count == limit) {
		return -EINVAL;
	}
	xname = ut_strndup(ut_lxa_ctx->ut_env, name, nlen);
	ut_lxa_ctx->names[ut_lxa_ctx->count++] = xname;
	return 0;
}

static int ut_listxattr(struct ut_env *ut_env, ino_t ino,
			struct ut_listxattr_ctx *ut_lxa_ctx)
{
	struct voluta_listxattr_ctx *lxa_ctx = &ut_lxa_ctx->lxa_ctx;

	memset(ut_lxa_ctx, 0, sizeof(*ut_lxa_ctx));
	ut_lxa_ctx->ut_env = ut_env;
	ut_lxa_ctx->lxa_ctx.actor = fillxent;

	return voluta_fs_listxattr(sbi(ut_env), op(ut_env), ino, lxa_ctx);
}

static int ut_reload(struct ut_env *ut_env)
{
	return voluta_fs_env_reload(ut_env->fs_env);
}

/*: : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : :*/

#define ut_assert_status(err_, status_) \
	ut_assert_eq(err_, -abs(status_))

void ut_access_ok(struct ut_env *ut_env, ino_t ino, int mode)
{
	int err;

	err = ut_access(ut_env, ino, mode);
	ut_assert_ok(err);
}

void ut_statfs_ok(struct ut_env *ut_env, ino_t ino, struct statvfs *st)
{
	int err;

	err = ut_statfs(ut_env, ino, st);
	ut_assert_ok(err);
}

void ut_statfs_rootd(struct ut_env *ut_env, struct statvfs *st)
{
	ut_statfs_ok(ut_env, VOLUTA_INO_ROOT, st);
}

static void ut_assert_sane_statx(const struct statx *stx)
{
	ut_assert_gt(stx->stx_blksize, 0);
	ut_assert_gt(stx->stx_btime.tv_sec, 0);
	ut_assert_le(stx->stx_btime.tv_sec, stx->stx_ctime.tv_sec);
	ut_assert_le(stx->stx_btime.tv_sec, stx->stx_mtime.tv_sec);
}

void ut_statx_exists(struct ut_env *ut_env, ino_t ino, struct statx *stx)
{
	int err;

	err = ut_statx(ut_env, ino, stx);
	ut_assert_ok(err);
	ut_assert_sane_statx(stx);
}

void ut_getattr_ok(struct ut_env *ut_env, ino_t ino, struct stat *st)
{
	int err;

	err = ut_getattr(ut_env, ino, st);
	ut_assert_ok(err);
	ut_assert_eq(ino, st->st_ino);
}

void ut_getattr_noent(struct ut_env *ut_env, ino_t ino)
{
	int err;
	struct stat st;

	err = ut_getattr(ut_env, ino, &st);
	ut_assert_err(err, -ENOENT);
}

void ut_getattr_file(struct ut_env *ut_env, ino_t ino, struct stat *st)
{
	ut_getattr_ok(ut_env, ino, st);
	ut_assert(S_ISREG(st->st_mode));
}

void ut_getattr_lnk(struct ut_env *ut_env, ino_t ino, struct stat *st)
{
	ut_getattr_ok(ut_env, ino, st);
	ut_assert(S_ISLNK(st->st_mode));
}

void ut_getattr_dir(struct ut_env *ut_env, ino_t ino, struct stat *st)
{
	ut_getattr_ok(ut_env, ino, st);
	ut_assert(S_ISDIR(st->st_mode));
}

void ut_getattr_dirsize(struct ut_env *ut_env, ino_t ino, loff_t size)
{
	struct stat st;

	ut_getattr_dir(ut_env, ino, &st);
	ut_assert_ge(st.st_size, size);
	if (!size) {
		ut_assert_eq(st.st_size, VOLUTA_DIR_EMPTY_SIZE);
	}
}

void ut_utimens_atime(struct ut_env *ut_env, ino_t ino,
		      const struct timespec *atime)
{
	int err;
	struct stat st, uts;

	memset(&uts, 0, sizeof(uts));
	uts.st_atim.tv_sec = atime->tv_sec;
	uts.st_atim.tv_nsec = atime->tv_nsec;
	uts.st_mtim.tv_nsec = UTIME_OMIT;
	uts.st_ctim.tv_sec = atime->tv_sec;
	uts.st_ctim.tv_nsec = atime->tv_nsec;

	err = ut_utimens(ut_env, ino, &uts, &st);
	ut_assert_ok(err);
	ut_assert_eq(ino, st.st_ino);
	ut_assert_eq(st.st_atim.tv_sec, atime->tv_sec);
	ut_assert_eq(st.st_atim.tv_nsec, atime->tv_nsec);
}

void ut_utimens_mtime(struct ut_env *ut_env, ino_t ino,
		      const struct timespec *mtime)
{
	int err;
	struct stat st, uts;

	memset(&uts, 0, sizeof(uts));
	uts.st_mtim.tv_sec = mtime->tv_sec;
	uts.st_mtim.tv_nsec = mtime->tv_nsec;
	uts.st_atim.tv_nsec = UTIME_OMIT;
	uts.st_ctim.tv_sec = mtime->tv_sec;
	uts.st_ctim.tv_nsec = mtime->tv_nsec;

	err = ut_utimens(ut_env, ino, &uts, &st);
	ut_assert_ok(err);
	ut_assert_eq(ino, st.st_ino);
	ut_assert_eq(st.st_mtim.tv_sec, mtime->tv_sec);
	ut_assert_eq(st.st_mtim.tv_nsec, mtime->tv_nsec);
}

static void ut_lookup_status(struct ut_env *ut_env, ino_t parent,
			     const char *name, struct stat *out_st, int status)
{
	int err;

	err = ut_lookup(ut_env, parent, name, out_st);
	ut_assert_status(err, status);
}

void ut_lookup_ok(struct ut_env *ut_env, ino_t parent,
		  const char *name, struct stat *out_st)
{
	ut_lookup_status(ut_env, parent, name, out_st, 0);
}

void ut_lookup_ino(struct ut_env *ut_env, ino_t parent,
		   const char *name, ino_t *out_ino)
{
	struct stat st;

	ut_lookup_ok(ut_env, parent, name, &st);
	*out_ino = st.st_ino;
}

void ut_lookup_noent(struct ut_env *ut_env, ino_t ino, const char *name)
{
	ut_lookup_status(ut_env, ino, name, NULL, -ENOENT);
}

void ut_lookup_exists(struct ut_env *ut_env, ino_t parent,
		      const char *name, ino_t ino, mode_t mode)
{
	struct stat st;

	ut_lookup_ok(ut_env, parent, name, &st);
	ut_assert_eq(ino, st.st_ino);
	ut_assert_eq(mode, st.st_mode & mode);
}

void ut_lookup_dir(struct ut_env *ut_env, ino_t parent,
		   const char *name, ino_t dino)
{
	ut_lookup_exists(ut_env, parent, name, dino, S_IFDIR);
}

void ut_lookup_file(struct ut_env *ut_env, ino_t parent,
		    const char *name, ino_t ino)
{
	ut_lookup_exists(ut_env, parent, name, ino, S_IFREG);
}

void ut_lookup_lnk(struct ut_env *ut_env, ino_t parent,
		   const char *name, ino_t ino)
{
	ut_lookup_exists(ut_env, parent, name, ino, S_IFLNK);
}


static void ut_mkdir_status(struct ut_env *ut_env, ino_t parent,
			    const char *name, struct stat *out_st, int status)
{
	int err;

	err = ut_mkdir(ut_env, parent, name, 0700, out_st);
	ut_assert_status(err, status);
}

void ut_mkdir_ok(struct ut_env *ut_env, ino_t parent,
		 const char *name, struct stat *out_st)
{
	int err;
	ino_t dino;
	struct stat st;

	ut_mkdir_status(ut_env, parent, name, out_st, 0);

	dino = out_st->st_ino;
	ut_assert_ne(dino, parent);
	ut_assert_ne(dino, VOLUTA_INO_NULL);

	err = ut_getattr(ut_env, dino, &st);
	ut_assert_ok(err);
	ut_assert_eq(st.st_ino, dino);
	ut_assert_eq(st.st_nlink, 2);

	err = ut_lookup(ut_env, parent, name, &st);
	ut_assert_ok(err);
	ut_assert_eq(st.st_ino, dino);

	err = ut_getattr(ut_env, parent, &st);
	ut_assert_ok(err);
	ut_assert_eq(st.st_ino, parent);
	ut_assert_gt(st.st_nlink, 2);
	ut_assert_gt(st.st_size, 0);
}

void ut_mkdir_oki(struct ut_env *ut_env, ino_t parent,
		  const char *name, ino_t *out_ino)
{
	struct stat st;

	ut_mkdir_ok(ut_env, parent, name, &st);
	*out_ino = st.st_ino;
}

void ut_mkdir_err(struct ut_env *ut_env, ino_t parent,
		  const char *name, int err)
{
	ut_mkdir_status(ut_env, parent, name, NULL, err);
}

void ut_mkdir_at_root(struct ut_env *ut_env,
		      const char *name, ino_t *out_ino)
{
	ut_mkdir_oki(ut_env, VOLUTA_INO_ROOT, name, out_ino);
}


static void ut_rmdir_status(struct ut_env *ut_env,
			    ino_t parent, const char *name, int status)
{
	int err;

	err = ut_rmdir(ut_env, parent, name);
	ut_assert_status(err, status);
}

void ut_rmdir_ok(struct ut_env *ut_env, ino_t parent, const char *name)
{
	struct stat st;

	ut_lookup_ok(ut_env, parent, name, &st);
	ut_rmdir_status(ut_env, parent, name, 0);
	ut_lookup_noent(ut_env, parent, name);
	ut_getattr_ok(ut_env, parent, &st);
}

void ut_rmdir_err(struct ut_env *ut_env, ino_t parent,
		  const char *name, int err)
{
	ut_rmdir_status(ut_env, parent, name, err);
}

void ut_rmdir_at_root(struct ut_env *ut_env, const char *name)
{
	ut_rmdir_ok(ut_env, VOLUTA_INO_ROOT, name);
}


static void ut_require_dir(struct ut_env *ut_env, ino_t dino)
{
	int err;
	struct stat st;

	err = ut_getattr(ut_env, dino, &st);
	ut_assert_ok(err);
	ut_assert(S_ISDIR(st.st_mode));
}

static void ut_opendir_status(struct ut_env *ut_env, ino_t ino, int status)
{
	int err;

	err = ut_opendir(ut_env, ino);
	ut_assert_status(err, status);
}

void ut_opendir_ok(struct ut_env *ut_env, ino_t ino)
{
	ut_require_dir(ut_env, ino);
	ut_opendir_status(ut_env, ino, 0);
}

void ut_opendir_err(struct ut_env *ut_env, ino_t ino, int err)
{
	ut_opendir_status(ut_env, ino, err);
}

static void ut_releasedir_status(struct ut_env *ut_env, ino_t ino, int status)
{
	int err;

	err = ut_releasedir(ut_env, ino);
	ut_assert_status(err, status);
}

void ut_releasedir_ok(struct ut_env *ut_env, ino_t ino)
{
	ut_require_dir(ut_env, ino);
	ut_releasedir_status(ut_env, ino, 0);
}

void ut_releasedir_err(struct ut_env *ut_env, ino_t ino, int err)
{
	ut_releasedir_status(ut_env, ino, err);
}

void ut_fsyncdir_ok(struct ut_env *ut_env, ino_t ino)
{
	int err;

	err = ut_fsyncdir(ut_env, ino, true);
	ut_assert_ok(err);
}

void ut_readdir_ok(struct ut_env *ut_env, ino_t ino, loff_t doff,
		   struct ut_readdir_ctx *ut_rd_ctx)
{
	int err;

	err = ut_readdir(ut_env, ino, doff, ut_rd_ctx);
	ut_assert_ok(err);
}

void ut_readdirplus_ok(struct ut_env *ut_env, ino_t ino, loff_t doff,
		       struct ut_readdir_ctx *ut_rd_ctx)
{
	int err;

	err = ut_readdirplus(ut_env, ino, doff, ut_rd_ctx);
	ut_assert_ok(err);
}

static void ut_link_status(struct ut_env *ut_env, ino_t ino,
			   ino_t parent, const char *name,
			   struct stat *out_st, int status)
{
	int err;

	err = ut_link(ut_env, ino, parent, name, out_st);
	ut_assert_status(err, status);
}

void ut_link_ok(struct ut_env *ut_env, ino_t ino,
		ino_t parent, const char *name, struct stat *out_st)
{
	nlink_t nlink1;
	nlink_t nlink2;
	struct stat st;

	ut_lookup_noent(ut_env, parent, name);
	ut_getattr_ok(ut_env, ino, &st);
	nlink1 = st.st_nlink;

	ut_link_status(ut_env, ino, parent, name, out_st, 0);
	ut_assert_eq(out_st->st_ino, ino);
	ut_assert_gt(out_st->st_nlink, 1);

	ut_lookup_ok(ut_env, parent, name, &st);
	ut_getattr_ok(ut_env, ino, &st);
	nlink2 = st.st_nlink;
	ut_assert_eq(nlink1 + 1, nlink2);
}

void ut_link_err(struct ut_env *ut_env, ino_t ino,
		 ino_t parent, const char *name, int err)
{
	ut_link_status(ut_env, ino, parent, name, NULL, err);
}


static void ut_unlink_status(struct ut_env *ut_env,
			     ino_t parent, const char *name, int status)
{
	int err;

	err = ut_unlink(ut_env, parent, name);
	ut_assert_status(err, status);
}

void ut_unlink_ok(struct ut_env *ut_env, ino_t parent, const char *name)
{
	ut_unlink_status(ut_env, parent, name, 0);
	ut_lookup_noent(ut_env, parent, name);
}

void ut_unlink_err(struct ut_env *ut_env,
		   ino_t parent, const char *name, int err)
{
	ut_unlink_status(ut_env, parent, name, err);
}

void ut_unlink_file(struct ut_env *ut_env,
		    ino_t parent, const char *name)
{
	ino_t ino;
	struct stat st;

	ut_lookup_ino(ut_env, parent, name, &ino);
	ut_getattr_file(ut_env, ino, &st);
	ut_unlink_ok(ut_env, parent, name);
}

static void ut_rename_ok(struct ut_env *ut_env, ino_t parent,
			 const char *name, ino_t newparent,
			 const char *newname, int flags)
{
	int err;

	err = ut_rename(ut_env, parent, name, newparent, newname, flags);
	ut_assert_ok(err);
}

void ut_rename_move(struct ut_env *ut_env, ino_t parent, const char *name,
		    ino_t newparent, const char *newname)
{
	struct stat st;

	ut_lookup_ok(ut_env, parent, name, &st);
	ut_lookup_noent(ut_env, newparent, newname);
	ut_rename_ok(ut_env, parent, name, newparent, newname, 0);
	ut_lookup_noent(ut_env, parent, name);
	ut_lookup_ok(ut_env, newparent, newname, &st);
}

void ut_rename_replace(struct ut_env *ut_env, ino_t parent, const char *name,
		       ino_t newparent, const char *newname)
{
	struct stat st;

	ut_lookup_ok(ut_env, parent, name, &st);
	ut_lookup_ok(ut_env, newparent, newname, &st);
	ut_rename_ok(ut_env, parent, name, newparent, newname, 0);
	ut_lookup_noent(ut_env, parent, name);
	ut_lookup_ok(ut_env, newparent, newname, &st);
}

void ut_rename_exchange(struct ut_env *ut_env, ino_t parent, const char *name,
			ino_t newparent, const char *newname)
{
	struct stat st1;
	struct stat st2;
	struct stat st3;
	struct stat st4;
	const int flags = RENAME_EXCHANGE;

	ut_lookup_ok(ut_env, parent, name, &st1);
	ut_assert_gt(st1.st_nlink, 0);
	ut_lookup_ok(ut_env, newparent, newname, &st2);
	ut_assert_gt(st2.st_nlink, 0);
	ut_rename_ok(ut_env, parent, name, newparent, newname, flags);
	ut_lookup_ok(ut_env, parent, name, &st3);
	ut_lookup_ok(ut_env, newparent, newname, &st4);
	ut_assert_eq(st1.st_ino, st4.st_ino);
	ut_assert_eq(st1.st_mode, st4.st_mode);
	ut_assert_eq(st1.st_nlink, st4.st_nlink);
	ut_assert_eq(st2.st_ino, st3.st_ino);
	ut_assert_eq(st2.st_mode, st3.st_mode);
	ut_assert_eq(st2.st_nlink, st3.st_nlink);
}

void ut_symlink_ok(struct ut_env *ut_env, ino_t parent,
		   const char *name, const char *value, ino_t *out_ino)
{
	int err;
	struct stat st;

	err = ut_lookup(ut_env, parent, name, &st);
	ut_assert_err(err, -ENOENT);

	err = ut_symlink(ut_env, parent, name, value, &st);
	ut_assert_ok(err);
	ut_assert_ne(st.st_ino, parent);

	ut_readlink_expect(ut_env, st.st_ino, value);

	*out_ino = st.st_ino;
}

void ut_readlink_expect(struct ut_env *ut_env,
			ino_t ino, const char *value)
{
	int err;
	char *lnk;
	const size_t lsz = VOLUTA_PATH_MAX;

	lnk = ut_zalloc(ut_env, lsz);
	err = ut_readlink(ut_env, ino, lnk, lsz);
	ut_assert_ok(err);
	ut_assert_eqs(value, lnk);
}

static void ut_create_status(struct ut_env *ut_env, ino_t parent,
			     const char *name, mode_t mode,
			     struct stat *out_st, int status)
{
	int err;

	err = ut_create(ut_env, parent, name, mode, out_st);
	ut_assert_status(err, status);
}

void ut_create_ok(struct ut_env *ut_env, ino_t parent,
		  const char *name, mode_t mode, struct stat *out_st)
{
	ut_create_status(ut_env, parent, name, mode, out_st, 0);
}

static void ut_create_new(struct ut_env *ut_env, ino_t parent,
			  const char *name, mode_t mode, ino_t *out_ino)
{
	ino_t ino;
	struct stat st;
	struct statvfs stv[2];

	ut_statfs_ok(ut_env, parent, &stv[0]);
	ut_create_ok(ut_env, parent, name, mode, &st);

	ino = st.st_ino;
	ut_assert_ne(ino, parent);
	ut_assert_ne(ino, VOLUTA_INO_NULL);
	ut_assert_eq(st.st_nlink, 1);
	ut_assert_eq(st.st_mode & S_IFMT, mode & S_IFMT);

	ut_getattr_ok(ut_env, parent, &st);
	ut_assert_eq(st.st_ino, parent);
	ut_assert_gt(st.st_size, 0);

	ut_statfs_ok(ut_env, ino, &stv[1]);
	ut_assert_eq(stv[1].f_ffree + 1, stv[0].f_ffree);
	ut_assert_le(stv[1].f_bfree, stv[0].f_bfree);

	*out_ino = ino;
}

void ut_create_file(struct ut_env *ut_env, ino_t parent,
		    const char *name, ino_t *out_ino)
{
	ut_create_new(ut_env, parent, name, S_IFREG | 0600, out_ino);
}

void ut_create_special(struct ut_env *ut_env, ino_t parent,
		       const char *name, mode_t mode, ino_t *out_ino)
{
	ut_assert(S_ISFIFO(mode) || S_ISSOCK(mode));
	ut_create_new(ut_env, parent, name, mode, out_ino);
}

void ut_create_noent(struct ut_env *ut_env, ino_t parent, const char *name)
{
	ut_create_status(ut_env, parent, name, S_IFREG | 0600, NULL, -ENOENT);
}

void ut_release_ok(struct ut_env *ut_env, ino_t ino)
{
	int err;

	err = ut_release(ut_env, ino);
	ut_assert_ok(err);
}

void ut_release_file(struct ut_env *ut_env, ino_t ino)
{
	struct stat st;

	ut_getattr_file(ut_env, ino, &st);
	ut_release_ok(ut_env, ino);
}

void ut_fsync_ok(struct ut_env *ut_env, ino_t ino, bool datasync)
{
	int err;

	err = ut_fsync(ut_env, ino, datasync);
	ut_assert_ok(err);
}

void ut_create_only(struct ut_env *ut_env, ino_t parent,
		    const char *name, ino_t *out_ino)
{
	ino_t ino;
	struct stat st;

	ut_create_ok(ut_env, parent, name, S_IFREG | 0600, &st);
	ino = st.st_ino;
	ut_assert_ne(ino, parent);
	ut_assert_ne(ino, VOLUTA_INO_NULL);

	ut_release_ok(ut_env, ino);
	ut_lookup_ok(ut_env, parent, name, &st);
	ut_assert_eq(ino, st.st_ino);

	*out_ino = ino;
}

void ut_open_rdonly(struct ut_env *ut_env, ino_t ino)
{
	int err;

	err = ut_open(ut_env, ino, O_RDONLY);
	ut_assert_ok(err);
}

void ut_open_rdwr(struct ut_env *ut_env, ino_t ino)
{
	int err;

	err = ut_open(ut_env, ino, O_RDWR);
	ut_assert_ok(err);
}

void ut_remove_file(struct ut_env *ut_env, ino_t parent,
		    const char *name, ino_t ino)
{
	struct statvfs stv[2];

	ut_statfs_ok(ut_env, ino, &stv[0]);
	ut_release_ok(ut_env, ino);
	ut_unlink_ok(ut_env, parent, name);
	ut_unlink_err(ut_env, parent, name, -ENOENT);
	ut_statfs_ok(ut_env, parent, &stv[1]);
	ut_assert_eq(stv[0].f_ffree + 1, stv[1].f_ffree);
}

void ut_remove_link(struct ut_env *ut_env,
		    ino_t parent, const char *name)
{
	struct stat st;

	ut_lookup_ok(ut_env, parent, name, &st);
	ut_unlink_ok(ut_env, parent, name);
	ut_unlink_err(ut_env, parent, name, -ENOENT);
}

void ut_write_ok(struct ut_env *ut_env, ino_t ino,
		 const void *buf, size_t bsz, loff_t off)
{
	int err;
	size_t nwr;

	err = ut_write(ut_env, ino, buf, bsz, off, &nwr);
	ut_assert_ok(err);
	ut_assert_eq(nwr, bsz);
}

void ut_write_nospc(struct ut_env *ut_env, ino_t ino,
		    const void *buf, size_t bsz,
		    loff_t off, size_t *out_nwr)
{
	int err;

	*out_nwr = 0;
	err = ut_write(ut_env, ino, buf, bsz, off, out_nwr);
	if (err) {
		ut_assert_status(err, -ENOSPC);
	}
}

void ut_write_read(struct ut_env *ut_env, ino_t ino,
		   const void *buf, size_t bsz, loff_t off)
{
	ut_write_ok(ut_env, ino, buf, bsz, off);
	ut_read_verify(ut_env, ino, buf, bsz, off);
}

void ut_write_read1(struct ut_env *ut_env, ino_t ino, loff_t off)
{
	const uint8_t dat[] = { 1 };

	ut_write_read(ut_env, ino, dat, 1, off);
}

void ut_write_read_str(struct ut_env *ut_env, ino_t ino,
		       const char *str, loff_t off)
{
	ut_write_read(ut_env, ino, str, strlen(str), off);
}

void ut_read_verify(struct ut_env *ut_env, ino_t ino,
		    const void *buf, size_t bsz, loff_t off)
{
	void *dat;
	char tmp[512];

	dat = (bsz > sizeof(tmp)) ? ut_randbuf(ut_env, bsz) : tmp;
	ut_read_ok(ut_env, ino, dat, bsz, off);
	ut_assert_eqm(buf, dat, bsz);
}

void ut_read_verify_str(struct ut_env *ut_env, ino_t ino,
			const char *str, loff_t off)
{
	ut_read_verify(ut_env, ino, str, strlen(str), off);
}


void ut_read_ok(struct ut_env *ut_env, ino_t ino,
		void *buf, size_t bsz, loff_t off)
{
	int err;
	size_t nrd;

	err = ut_read(ut_env, ino, buf, bsz, off, &nrd);
	ut_assert_ok(err);
	ut_assert_eq(nrd, bsz);
}

void ut_read_zero(struct ut_env *ut_env, ino_t ino, loff_t off)
{
	uint8_t zero[] = { 0 };

	if (off >= 0) {
		ut_read_verify(ut_env, ino, zero, 1, off);
	}
}

void ut_read_zeros(struct ut_env *ut_env,
		   ino_t ino, loff_t off, size_t len)
{
	const void *zeros = ut_zerobuf(ut_env, len);

	ut_read_verify(ut_env, ino, zeros, len, off);
}

void ut_trunacate_file(struct ut_env *ut_env,
		       ino_t ino, loff_t off)
{
	int err;
	size_t nrd;
	uint8_t buf[1] = { 0 };
	struct stat st;

	err = ut_truncate(ut_env, ino, off, &st);
	ut_assert_ok(err);
	ut_assert_eq(off, st.st_size);

	err = ut_read(ut_env, ino, buf, 1, off, &nrd);
	ut_assert_ok(err);
	ut_assert_eq(nrd, 0);
	ut_assert_eq(buf[0], 0);
}

void ut_fallocate_reserve(struct ut_env *ut_env, ino_t ino,
			  loff_t offset, loff_t len)
{
	int err;
	struct stat st;

	err = ut_fallocate(ut_env, ino, 0, offset, len);
	ut_assert_ok(err);

	err = ut_getattr(ut_env, ino, &st);
	ut_assert_ok(err);
	ut_assert_ge(st.st_size, offset + len);
}

void ut_fallocate_punch_hole(struct ut_env *ut_env, ino_t ino,
			     loff_t offset, loff_t len)
{
	int err, mode;
	loff_t isize;
	struct stat st;

	err = ut_getattr(ut_env, ino, &st);
	ut_assert_ok(err);
	isize = st.st_size;

	mode = FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE;
	err = ut_fallocate(ut_env, ino, mode, offset, len);
	ut_assert_ok(err);

	err = ut_getattr(ut_env, ino, &st);
	ut_assert_ok(err);
	ut_assert_eq(st.st_size, isize);
}

static void ut_setgetxattr(struct ut_env *ut_env, ino_t ino,
			   const struct ut_keyval *kv, int flags)
{
	int err;

	err = ut_setxattr(ut_env, ino,
			  kv->name, kv->value, kv->size, flags);
	ut_assert_ok(err);

	ut_getxattr_value(ut_env, ino, kv);
}

void ut_setxattr_create(struct ut_env *ut_env, ino_t ino,
			const struct ut_keyval *kv)
{
	ut_setgetxattr(ut_env, ino, kv, XATTR_CREATE);
}

void ut_setxattr_replace(struct ut_env *ut_env, ino_t ino,
			 const struct ut_keyval *kv)
{
	ut_setgetxattr(ut_env, ino, kv, XATTR_REPLACE);
}

void ut_setxattr_rereplace(struct ut_env *ut_env, ino_t ino,
			   const struct ut_keyval *kv)
{
	ut_setgetxattr(ut_env, ino, kv, 0);
}

void ut_setxattr_all(struct ut_env *ut_env, ino_t ino,
		     const struct ut_kvl *kvl)
{
	const struct ut_keyval *kv;

	for (size_t i = 0; i < kvl->count; ++i) {
		kv = kvl->list[i];
		ut_setxattr_create(ut_env, ino, kv);
		ut_getxattr_value(ut_env, ino, kv);
	}
}


void ut_getxattr_value(struct ut_env *ut_env, ino_t ino,
		       const struct ut_keyval *kv)
{
	int err;
	size_t vsz;
	void *val;

	vsz = 0;
	err = ut_getxattr(ut_env, ino, kv->name, NULL, 0, &vsz);
	ut_assert_ok(err);
	ut_assert_eq(vsz, kv->size);

	val = ut_randbuf(ut_env, vsz);
	err = ut_getxattr(ut_env, ino, kv->name, val, vsz, &vsz);
	ut_assert_ok(err);
	ut_assert_eqm(val, kv->value, kv->size);
}

void ut_getxattr_nodata(struct ut_env *ut_env, ino_t ino,
			const struct ut_keyval *kv)

{
	int err;
	size_t bsz = 0;
	char buf[256] = "";

	err = ut_getxattr(ut_env, ino, kv->name,
			  buf, sizeof(buf), &bsz);
	ut_assert_err(err, -ENODATA);
	ut_assert_eq(bsz, 0);
}

void ut_removexattr_ok(struct ut_env *ut_env, ino_t ino,
		       const struct ut_keyval *kv)
{
	int err;

	err = ut_removexattr(ut_env, ino, kv->name);
	ut_assert_ok(err);

	err = ut_removexattr(ut_env, ino, kv->name);
	ut_assert_err(err, -ENODATA);
}


static struct ut_keyval *
kvl_search(const struct ut_kvl *kvl, const char *name)
{
	struct ut_keyval *kv;

	for (size_t i = 0; i < kvl->count; ++i) {
		kv = kvl->list[i];
		if (strcmp(name, kv->name) != 0) {
			return kv;
		}
	}
	return NULL;
}

void ut_listxattr_ok(struct ut_env *ut_env, ino_t ino,
		     const struct ut_kvl *kvl)
{
	int err;
	const char *name;
	const struct ut_keyval *kv;
	struct ut_listxattr_ctx ut_listxattr_ctx;

	err = ut_listxattr(ut_env, ino, &ut_listxattr_ctx);
	ut_assert_ok(err);
	ut_assert_eq(ut_listxattr_ctx.count, kvl->count);

	for (size_t i = 0; i < ut_listxattr_ctx.count; ++i) {
		name = ut_listxattr_ctx.names[i];
		ut_assert_not_null(name);
		kv = kvl_search(kvl, name);
		ut_assert_not_null(kv);
	}
}

void ut_removexattr_all(struct ut_env *ut_env, ino_t ino,
			const struct ut_kvl *kvl)
{
	const struct ut_keyval *kv;

	for (size_t i = 0; i < kvl->count; ++i) {
		kv = kvl->list[i];
		ut_removexattr_ok(ut_env, ino, kv);
	}
}

void ut_query_ok(struct ut_env *ut_env, ino_t ino,
		 struct voluta_query *out_qry)
{
	int err;

	err = ut_query(ut_env, ino, out_qry);
	ut_assert_ok(err);
}

void ut_fiemap_ok(struct ut_env *ut_env, ino_t ino, struct fiemap *fm)
{
	int err;

	err = ut_fiemap(ut_env, ino, fm);
	ut_assert_ok(err);
	ut_assert_lt(fm->fm_mapped_extents, UINT_MAX / 2);
	if (fm->fm_extent_count) {
		ut_assert_le(fm->fm_mapped_extents, fm->fm_extent_count);
	}
}

static void ut_lseek_ok(struct ut_env *ut_env, ino_t ino,
			loff_t off, int whence, loff_t *out_off)
{
	int err;
	struct stat st;

	ut_getattr_ok(ut_env, ino, &st);

	*out_off = -1;
	err = ut_lseek(ut_env, ino, off, whence, out_off);
	ut_assert_ok(err);
	ut_assert_ge(*out_off, 0);
	ut_assert_le(*out_off, st.st_size);
}

void ut_lseek_data(struct ut_env *ut_env,
		   ino_t ino, loff_t off, loff_t *out_off)
{
	ut_lseek_ok(ut_env, ino, off, SEEK_DATA, out_off);
}

void ut_lseek_hole(struct ut_env *ut_env,
		   ino_t ino, loff_t off, loff_t *out_off)
{
	ut_lseek_ok(ut_env, ino, off, SEEK_HOLE, out_off);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void ut_write_dvec(struct ut_env *ut_env, ino_t ino,
		   const struct ut_dvec *dvec)
{
	ut_write_read(ut_env, ino, dvec->dat,
		      dvec->len, dvec->off);
}

void ut_read_dvec(struct ut_env *ut_env, ino_t ino,
		  const struct ut_dvec *dvec)
{
	void *dat = ut_zerobuf(ut_env, dvec->len);

	ut_read_ok(ut_env, ino, dat, dvec->len, dvec->off);
	ut_assert_eqm(dat, dvec->dat, dvec->len);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void ut_sync_drop(struct ut_env *ut_env)
{
	int err;

	err = voluta_fs_env_sync_drop(ut_env->fs_env);
	ut_assert_ok(err);
}

void ut_drop_caches_fully(struct ut_env *ut_env)
{
	struct voluta_stats st;

	ut_sync_drop(ut_env);

	/* Expects only super-block */
	voluta_fs_env_stats(ut_env->fs_env, &st);
	ut_assert_eq(st.ncache_segs, 1);
	ut_assert_eq(st.ncache_vnodes, 1);
	ut_assert_eq(st.ncache_inodes, 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void ut_assert_eq_ts(const struct timespec *ts1, const struct timespec *ts2)
{
	ut_assert_eq(ts1->tv_sec, ts2->tv_sec);
	ut_assert_eq(ts1->tv_nsec, ts2->tv_nsec);
}

void ut_assert_eq_stat(const struct stat *st1, const struct stat *st2)
{
	ut_assert_eq(st1->st_ino, st2->st_ino);
	ut_assert_eq(st1->st_nlink, st2->st_nlink);
	ut_assert_eq(st1->st_uid, st2->st_uid);
	ut_assert_eq(st1->st_gid, st2->st_gid);
	ut_assert_eq(st1->st_mode, st2->st_mode);
	ut_assert_eq(st1->st_size, st2->st_size);
	ut_assert_eq(st1->st_blocks, st2->st_blocks);
	ut_assert_eq(st1->st_blksize, st2->st_blksize);
	ut_assert_eq_ts(&st1->st_mtim, &st2->st_mtim);
	ut_assert_eq_ts(&st1->st_ctim, &st2->st_ctim);
}

void ut_assert_eq_statvfs(const struct statvfs *s1, const struct statvfs *s2)
{
	ut_assert_eq(s1->f_bsize, s2->f_bsize);
	ut_assert_eq(s1->f_frsize, s2->f_frsize);
	ut_assert_eq(s1->f_blocks, s2->f_blocks);
	ut_assert_eq(s1->f_bfree, s2->f_bfree);
	ut_assert_eq(s1->f_bavail, s2->f_bavail);
	ut_assert_eq(s1->f_files, s2->f_files);
	ut_assert_eq(s1->f_ffree, s2->f_ffree);
	ut_assert_eq(s1->f_favail, s2->f_favail);
}

void ut_assert_valid_statvfs(const struct statvfs *stv)
{
	ut_assert_le(stv->f_bfree, stv->f_blocks);
	ut_assert_le(stv->f_bavail, stv->f_blocks);
	ut_assert_le(stv->f_ffree, stv->f_files);
	ut_assert_le(stv->f_favail, stv->f_files);
}

void ut_reload_ok(struct ut_env *ut_env, ino_t ino)
{
	int err;
	struct stat st[2];
	struct statvfs fsst[2];

	ut_statfs_ok(ut_env, ino, &fsst[0]);
	ut_getattr_ok(ut_env, ino, &st[0]);
	err = ut_reload(ut_env);
	ut_assert_ok(err);
	ut_statfs_ok(ut_env, ino, &fsst[1]);
	ut_getattr_ok(ut_env, ino, &st[1]);

	ut_assert_eq_statvfs(&fsst[0], &fsst[1]);
	ut_assert_eq_stat(&st[0], &st[1]);
}
