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
#include <sys/xattr.h>
#include <linux/falloc.h>
#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include <limits.h>
#include "unitest.h"


void voluta_ut_statfs_ok(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			 struct statvfs *st)
{
	int err;

	err = voluta_ut_statfs(ut_ctx, ino, st);
	ut_assert_ok(err);
}

void voluta_ut_statfs_rootd(struct voluta_ut_ctx *ut_ctx, struct statvfs *st)
{
	voluta_ut_statfs_ok(ut_ctx, VOLUTA_INO_ROOT, st);
}

static void ut_assert_sane_statx(const struct statx *stx)
{
	ut_assert_gt(stx->stx_blksize, 0);
	ut_assert_gt(stx->stx_btime.tv_sec, 0);
	ut_assert_le(stx->stx_btime.tv_sec, stx->stx_ctime.tv_sec);
	ut_assert_le(stx->stx_btime.tv_sec, stx->stx_mtime.tv_sec);
}

void voluta_ut_statx_exists(struct voluta_ut_ctx *ut_ctx,
			    ino_t ino, struct statx *stx)
{
	int err;

	err = voluta_ut_statx(ut_ctx, ino, stx);
	ut_assert_ok(err);
	ut_assert_sane_statx(stx);
}

void voluta_ut_getattr_exists(struct voluta_ut_ctx *ut_ctx,
			      ino_t ino, struct stat *st)
{
	int err;

	err = voluta_ut_getattr(ut_ctx, ino, st);
	ut_assert_ok(err);
}

void voluta_ut_getattr_file(struct voluta_ut_ctx *ut_ctx,
			    ino_t ino, struct stat *st)
{
	voluta_ut_getattr_exists(ut_ctx, ino, st);
	ut_assert(S_ISREG(st->st_mode));
}

void voluta_ut_getattr_dirsize(struct voluta_ut_ctx *ut_ctx,
			       ino_t ino, loff_t size)
{
	int err;
	struct stat st;

	err = voluta_ut_getattr(ut_ctx, ino, &st);
	ut_assert_ok(err);
	ut_assert(S_ISDIR(st.st_mode));
	ut_assert_ge(st.st_size, size);
	if (!size) {
		ut_assert_eq(st.st_size, VOLUTA_DIR_EMPTY_SIZE);
	}
}

void voluta_ut_utimens_atime(struct voluta_ut_ctx *ut_ctx, ino_t ino,
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

	err = voluta_ut_utimens(ut_ctx, ino, &uts, &st);
	ut_assert_ok(err);
	ut_assert_eq(ino, st.st_ino);
	ut_assert_eq(st.st_atim.tv_sec, atime->tv_sec);
	ut_assert_eq(st.st_atim.tv_nsec, atime->tv_nsec);
}

void voluta_ut_utimens_mtime(struct voluta_ut_ctx *ut_ctx, ino_t ino,
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

	err = voluta_ut_utimens(ut_ctx, ino, &uts, &st);
	ut_assert_ok(err);
	ut_assert_eq(ino, st.st_ino);
	ut_assert_eq(st.st_mtim.tv_sec, mtime->tv_sec);
	ut_assert_eq(st.st_mtim.tv_nsec, mtime->tv_nsec);
}

void voluta_ut_lookup_noent(struct voluta_ut_ctx *ut_ctx,
			    ino_t ino, const char *name)
{
	int err;
	struct stat st;

	err = voluta_ut_lookup(ut_ctx, ino, name, &st);
	ut_assert_err(err, -ENOENT);
}

void voluta_ut_mkdir_ok(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			const char *name, ino_t *out_ino)
{
	int err;
	ino_t dino;
	struct stat st;
	const mode_t mode = 0700;

	err = voluta_ut_mkdir(ut_ctx, parent_ino, name, mode, &st);
	ut_assert_ok(err);

	dino = st.st_ino;
	ut_assert_ne(dino, parent_ino);
	ut_assert_ne(dino, VOLUTA_INO_NULL);

	err = voluta_ut_getattr(ut_ctx, dino, &st);
	ut_assert_ok(err);
	ut_assert_eq(st.st_ino, dino);
	ut_assert_eq(st.st_nlink, 2);

	err = voluta_ut_lookup(ut_ctx, parent_ino, name, &st);
	ut_assert_ok(err);
	ut_assert_eq(st.st_ino, dino);

	err = voluta_ut_getattr(ut_ctx, parent_ino, &st);
	ut_assert_ok(err);
	ut_assert_eq(st.st_ino, parent_ino);
	ut_assert_gt(st.st_nlink, 2);
	ut_assert_gt(st.st_size, 0);

	*out_ino = dino;
}

void voluta_ut_mkdir_at_root(struct voluta_ut_ctx *ut_ctx,
			     const char *name, ino_t *out_ino)
{
	voluta_ut_mkdir_ok(ut_ctx, VOLUTA_INO_ROOT, name, out_ino);
}


void voluta_ut_mkdir_fail(struct voluta_ut_ctx *ut_ctx,
			  ino_t parent_ino, const char *name)
{
	int err;
	struct stat st;

	err = voluta_ut_mkdir(ut_ctx, parent_ino, name, 0700, &st);
	ut_assert_err(err, -EEXIST);
}

void voluta_ut_rmdir_ok(struct voluta_ut_ctx *ut_ctx,
			ino_t parent_ino, const char *name)
{
	int err;
	ino_t child_ino;
	struct stat st;

	err = voluta_ut_lookup(ut_ctx, parent_ino, name, &st);
	ut_assert_ok(err);
	child_ino = st.st_ino;

	err = voluta_ut_rmdir(ut_ctx, parent_ino, name);
	ut_assert_ok(err);

	err = voluta_ut_lookup(ut_ctx, parent_ino, name, &st);
	ut_assert_err(err, -ENOENT);

	err = voluta_ut_getattr(ut_ctx, child_ino, &st);
	ut_assert_err(err, -ENOENT);

	err = voluta_ut_getattr(ut_ctx, parent_ino, &st);
	ut_assert_ok(err);
}

void voluta_ut_rmdir_at_root(struct voluta_ut_ctx *ut_ctx, const char *name)
{
	voluta_ut_rmdir_ok(ut_ctx, VOLUTA_INO_ROOT, name);
}

void voluta_ut_rmdir_enotempty(struct voluta_ut_ctx *ut_ctx,
			       ino_t parent_ino, const char *name)
{
	int err;

	err = voluta_ut_rmdir(ut_ctx, parent_ino, name);
	ut_assert_err(err, -ENOTEMPTY);
}

static void ut_require_dir(struct voluta_ut_ctx *ut_ctx, ino_t dino)
{
	int err;
	struct stat st;

	err = voluta_ut_getattr(ut_ctx, dino, &st);
	ut_assert_ok(err);
	ut_assert(S_ISDIR(st.st_mode));
}

void voluta_ut_opendir_ok(struct voluta_ut_ctx *ut_ctx, ino_t dino)
{
	int err;

	ut_require_dir(ut_ctx, dino);
	err = voluta_ut_opendir(ut_ctx, dino);
	ut_assert_ok(err);
}

void voluta_ut_releasedir_ok(struct voluta_ut_ctx *ut_ctx, ino_t dino)
{
	int err;

	ut_require_dir(ut_ctx, dino);
	err = voluta_ut_releasedir(ut_ctx, dino);
	ut_assert_ok(err);
}

void voluta_ut_lookup_ok(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			 const char *name, ino_t *out_ino)
{
	int err;
	struct stat st;

	err = voluta_ut_lookup(ut_ctx, parent_ino, name, &st);
	ut_assert_ok(err);

	*out_ino = st.st_ino;
}

void voluta_ut_lookup_exists(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			     const char *name, ino_t ino, mode_t mode)
{
	int err;
	struct stat st;

	err = voluta_ut_lookup(ut_ctx, parent_ino, name, &st);
	ut_assert_ok(err);
	ut_assert_eq(ino, st.st_ino);
	ut_assert_eq(mode, st.st_mode & mode);
}

void voluta_ut_lookup_dir(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			  const char *name, ino_t dino)
{
	voluta_ut_lookup_exists(ut_ctx, parent_ino, name, dino, S_IFDIR);
}

void voluta_ut_lookup_reg(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			  const char *name, ino_t ino)
{
	voluta_ut_lookup_exists(ut_ctx, parent_ino, name, ino, S_IFREG);
}

void voluta_ut_lookup_lnk(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			  const char *name, ino_t ino)
{
	voluta_ut_lookup_exists(ut_ctx, parent_ino, name, ino, S_IFLNK);
}


void voluta_ut_link_new(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			ino_t parent_ino, const char *name)
{
	int err;
	nlink_t nlink1;
	nlink_t nlink2;
	struct stat st;

	voluta_ut_lookup_noent(ut_ctx, parent_ino, name);
	voluta_ut_getattr_exists(ut_ctx, ino, &st);
	nlink1 = st.st_nlink;

	err = voluta_ut_link(ut_ctx, ino, parent_ino, name, &st);
	ut_assert_ok(err);
	ut_assert_eq(st.st_ino, ino);
	ut_assert_gt(st.st_nlink, 1);

	err = voluta_ut_lookup(ut_ctx, parent_ino, name, &st);
	ut_assert_ok(err);

	voluta_ut_getattr_exists(ut_ctx, ino, &st);
	nlink2 = st.st_nlink;
	ut_assert_eq(nlink1 + 1, nlink2);
}

void voluta_ut_unlink_exists(struct voluta_ut_ctx *ut_ctx,
			     ino_t parent_ino, const char *name)
{
	int err;
	struct stat st;

	err = voluta_ut_unlink(ut_ctx, parent_ino, name);
	ut_assert_ok(err);

	err = voluta_ut_lookup(ut_ctx, parent_ino, name, &st);
	ut_assert_err(err, -ENOENT);
}

void voluta_ut_rename_move(struct voluta_ut_ctx *ut_ctx,
			   ino_t parent_ino, const char *name,
			   ino_t newparent_ino, const char *newname)
{
	int err;
	struct stat st;

	err = voluta_ut_lookup(ut_ctx, parent_ino, name, &st);
	ut_assert_ok(err);

	err = voluta_ut_lookup(ut_ctx, newparent_ino, newname, &st);
	ut_assert_err(err, -ENOENT);

	err = voluta_ut_rename(ut_ctx, parent_ino, name,
			       newparent_ino, newname, 0);
	ut_assert_ok(err);

	err = voluta_ut_lookup(ut_ctx, parent_ino, name, &st);
	ut_assert_err(err, -ENOENT);

	err = voluta_ut_lookup(ut_ctx, newparent_ino, newname, &st);
	ut_assert_ok(err);
}

void voluta_ut_rename_replace(struct voluta_ut_ctx *ut_ctx,
			      ino_t parent_ino, const char *name,
			      ino_t newparent_ino, const char *newname)
{
	int err;
	struct stat st;

	err = voluta_ut_lookup(ut_ctx, parent_ino, name, &st);
	ut_assert_ok(err);

	err = voluta_ut_lookup(ut_ctx, newparent_ino, newname, &st);
	ut_assert_ok(err);

	err = voluta_ut_rename(ut_ctx, parent_ino, name,
			       newparent_ino, newname, 0);
	ut_assert_ok(err);

	err = voluta_ut_lookup(ut_ctx, parent_ino, name, &st);
	ut_assert_err(err, -ENOENT);

	err = voluta_ut_lookup(ut_ctx, newparent_ino, newname, &st);
	ut_assert_ok(err);
}

void voluta_ut_rename_exchange(struct voluta_ut_ctx *ut_ctx,
			       ino_t parent_ino, const char *name,
			       ino_t newparent_ino, const char *newname)
{
	int err;
	struct stat st1;
	struct stat st2;
	struct stat st3;
	struct stat st4;

	err = voluta_ut_lookup(ut_ctx, parent_ino, name, &st1);
	ut_assert_ok(err);
	ut_assert_gt(st1.st_nlink, 0);

	err = voluta_ut_lookup(ut_ctx, newparent_ino, newname, &st2);
	ut_assert_ok(err);
	ut_assert_gt(st2.st_nlink, 0);

	err = voluta_ut_rename(ut_ctx, parent_ino, name,
			       newparent_ino, newname, RENAME_EXCHANGE);
	ut_assert_ok(err);

	err = voluta_ut_lookup(ut_ctx, parent_ino, name, &st3);
	ut_assert_ok(err);

	err = voluta_ut_lookup(ut_ctx, newparent_ino, newname, &st4);
	ut_assert_ok(err);

	ut_assert_eq(st1.st_ino, st4.st_ino);
	ut_assert_eq(st1.st_nlink, st4.st_nlink);
	ut_assert_eq(st2.st_ino, st3.st_ino);
	ut_assert_eq(st2.st_nlink, st3.st_nlink);
}

void voluta_ut_create_symlink(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			      const char *name, const char *value, ino_t *out)
{
	int err;
	ino_t sino;
	char *lnk;
	struct stat st;
	const size_t lsz = VOLUTA_PATH_MAX;

	err = voluta_ut_lookup(ut_ctx, parent_ino, name, &st);
	ut_assert_err(err, -ENOENT);

	err = voluta_ut_symlink(ut_ctx, parent_ino, name, value, &st);
	sino = st.st_ino;
	ut_assert_ok(err);
	ut_assert_ne(sino, parent_ino);

	lnk = voluta_ut_zalloc(ut_ctx, lsz);
	err = voluta_ut_readlink(ut_ctx, sino, lnk, lsz);
	ut_assert_ok(err);
	ut_assert_eqs(value, lnk);

	*out = sino;
}

void voluta_ut_readlink_expect(struct voluta_ut_ctx *ut_ctx,
			       ino_t ino, const char *symval)
{
	int err;
	char *lnk;
	const size_t lsz = VOLUTA_PATH_MAX;

	lnk = voluta_ut_zalloc(ut_ctx, lsz);
	err = voluta_ut_readlink(ut_ctx, ino, lnk, lsz);
	ut_assert_ok(err);
	ut_assert_eqs(symval, lnk);
}

static void ut_create(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
		      const char *name, mode_t mode, ino_t *out_ino)
{
	int err;
	ino_t ino;
	struct stat st;

	err = voluta_ut_create(ut_ctx, parent_ino, name, mode, &st);
	ut_assert_ok(err);

	ino = st.st_ino;
	ut_assert_ne(ino, parent_ino);
	ut_assert_ne(ino, VOLUTA_INO_NULL);

	err = voluta_ut_getattr(ut_ctx, parent_ino, &st);
	ut_assert_ok(err);
	ut_assert_eq(st.st_ino, parent_ino);
	ut_assert_gt(st.st_size, 0);

	*out_ino = ino;
}

void voluta_ut_create_file(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			   const char *name, ino_t *out_ino)
{
	ut_create(ut_ctx, parent_ino, name, S_IFREG | 0600, out_ino);
}

void voluta_ut_create_special(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			      const char *name, mode_t mode, ino_t *out_ino)
{
	ut_assert(S_ISFIFO(mode) || S_ISSOCK(mode));
	ut_create(ut_ctx, parent_ino, name, mode, out_ino);
}

void voluta_ut_release_file(struct voluta_ut_ctx *ut_ctx, ino_t ino)
{
	int err;
	struct stat st;

	err = voluta_ut_getattr(ut_ctx, ino, &st);
	ut_assert_ok(err);

	err = voluta_ut_release(ut_ctx, ino);
	ut_assert_ok(err);
}

void voluta_ut_fsync_file(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			  bool datasync)
{
	int err;

	err = voluta_ut_fsync(ut_ctx, ino, datasync);
	ut_assert_ok(err);
}

void voluta_ut_create_only(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			   const char *name, ino_t *out_ino)
{
	int err;
	ino_t ino;
	struct stat st;

	err = voluta_ut_create(ut_ctx, parent_ino, name, S_IFREG | 0600, &st);
	ut_assert_ok(err);

	ino = st.st_ino;
	ut_assert_ne(ino, parent_ino);
	ut_assert_ne(ino, VOLUTA_INO_NULL);

	err = voluta_ut_release(ut_ctx, ino);
	ut_assert_ok(err);

	err = voluta_ut_lookup(ut_ctx, parent_ino, name, &st);
	ut_assert_ok(err);
	ut_assert_eq(ino, st.st_ino);

	*out_ino = ino;
}

void voluta_ut_open_rdonly(struct voluta_ut_ctx *ut_ctx, ino_t ino)
{
	int err;

	err = voluta_ut_open(ut_ctx, ino, O_RDONLY);
	ut_assert_ok(err);
}

void voluta_ut_open_rdwr(struct voluta_ut_ctx *ut_ctx, ino_t ino)
{
	int err;

	err = voluta_ut_open(ut_ctx, ino, O_RDWR);
	ut_assert_ok(err);
}

void voluta_ut_remove_file(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			   const char *name, ino_t ino)
{
	int err;

	err = voluta_ut_release(ut_ctx, ino);
	ut_assert_ok(err);

	err = voluta_ut_unlink(ut_ctx, parent_ino, name);
	ut_assert_ok(err);

	err = voluta_ut_unlink(ut_ctx, parent_ino, name);
	ut_assert_err(err, -ENOENT);
}

void voluta_ut_remove_link(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			   const char *name)
{
	int err;
	struct stat st;

	err = voluta_ut_lookup(ut_ctx, parent_ino, name, &st);
	ut_assert_ok(err);

	err = voluta_ut_unlink(ut_ctx, parent_ino, name);
	ut_assert_ok(err);

	err = voluta_ut_unlink(ut_ctx, parent_ino, name);
	ut_assert_err(err, -ENOENT);
}

void voluta_ut_write_read(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			  const void *buf, size_t bsz, loff_t off)
{
	int err;
	size_t nwr;

	err = voluta_ut_write(ut_ctx, ino, buf, bsz, off, &nwr);
	ut_assert_ok(err);
	ut_assert_eq(nwr, bsz);

	voluta_ut_read_verify(ut_ctx, ino, buf, bsz, off);
}

void voluta_ut_write_read_str(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			      const char *str, loff_t off)
{
	voluta_ut_write_read(ut_ctx, ino, str, strlen(str), off);
}

void voluta_ut_read_verify(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			   const void *buf, size_t bsz, loff_t off)
{
	void *dat;
	char tmp[512];

	dat = (bsz > sizeof(tmp)) ? ut_randbuf(ut_ctx, bsz) : tmp;
	voluta_ut_read_only(ut_ctx, ino, dat, bsz, off);
	ut_assert_eqm(buf, dat, bsz);
}

void voluta_ut_read_verify_str(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			       const char *str, loff_t off)
{
	voluta_ut_read_verify(ut_ctx, ino, str, strlen(str), off);
}


void voluta_ut_read_only(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			 void *buf, size_t bsz, loff_t off)
{
	int err;
	size_t nrd;

	err = voluta_ut_read(ut_ctx, ino, buf, bsz, off, &nrd);
	ut_assert_ok(err);
	ut_assert_eq(nrd, bsz);
}

void voluta_ut_read_zero(struct voluta_ut_ctx *ut_ctx, ino_t ino, loff_t off)
{
	uint8_t zero[] = { 0 };

	if (off >= 0) {
		voluta_ut_read_verify(ut_ctx, ino, zero, 1, off);
	}
}

void voluta_ut_read_zeros(struct voluta_ut_ctx *ut_ctx,
			  ino_t ino, loff_t off, size_t len)
{
	const void *zeros = voluta_ut_zerobuf(ut_ctx, len);

	voluta_ut_read_verify(ut_ctx, ino, zeros, len, off);
}

void voluta_ut_trunacate_file(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			      loff_t len)
{
	int err;
	size_t nrd;
	uint8_t buf[1] = { 0 };
	struct stat st;

	err = voluta_ut_truncate(ut_ctx, ino, len);
	ut_assert_ok(err);

	err = voluta_ut_getattr(ut_ctx, ino, &st);
	ut_assert_ok(err);
	ut_assert_eq(len, st.st_size);

	err = voluta_ut_read(ut_ctx, ino, buf, 1, len, &nrd);
	ut_assert_ok(err);
	ut_assert_eq(nrd, 0);
	ut_assert_eq(buf[0], 0);
}

void voluta_ut_fallocate_reserve(struct voluta_ut_ctx *ut_ctx, ino_t ino,
				 loff_t offset, loff_t len)
{
	int err;
	struct stat st;

	err = voluta_ut_fallocate(ut_ctx, ino, 0, offset, len);
	ut_assert_ok(err);

	err = voluta_ut_getattr(ut_ctx, ino, &st);
	ut_assert_ok(err);
	ut_assert_ge(st.st_size, offset + len);
}

void voluta_ut_fallocate_punch_hole(struct voluta_ut_ctx *ut_ctx, ino_t ino,
				    loff_t offset, loff_t len)
{
	int err, mode;
	loff_t isize;
	struct stat st;

	err = voluta_ut_getattr(ut_ctx, ino, &st);
	ut_assert_ok(err);
	isize = st.st_size;

	mode = FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE;
	err = voluta_ut_fallocate(ut_ctx, ino, mode, offset, len);
	ut_assert_ok(err);

	err = voluta_ut_getattr(ut_ctx, ino, &st);
	ut_assert_ok(err);
	ut_assert_eq(st.st_size, isize);
}

static void ut_setgetxattr(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			   const struct voluta_ut_keyval *kv, int flags)
{
	int err;

	err = voluta_ut_setxattr(ut_ctx, ino,
				 kv->name, kv->value, kv->size, flags);
	ut_assert_ok(err);

	voluta_ut_getxattr_value(ut_ctx, ino, kv);
}

void voluta_ut_setxattr_create(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			       const struct voluta_ut_keyval *kv)
{
	ut_setgetxattr(ut_ctx, ino, kv, XATTR_CREATE);
}

void voluta_ut_setxattr_replace(struct voluta_ut_ctx *ut_ctx, ino_t ino,
				const struct voluta_ut_keyval *kv)
{
	ut_setgetxattr(ut_ctx, ino, kv, XATTR_REPLACE);
}

void voluta_ut_setxattr_rereplace(struct voluta_ut_ctx *ut_ctx, ino_t ino,
				  const struct voluta_ut_keyval *kv)
{
	ut_setgetxattr(ut_ctx, ino, kv, 0);
}

void voluta_ut_setxattr_all(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			    const struct voluta_ut_kvl *kvl)
{
	const struct voluta_ut_keyval *kv;

	for (size_t i = 0; i < kvl->count; ++i) {
		kv = kvl->list[i];
		voluta_ut_setxattr_create(ut_ctx, ino, kv);
		voluta_ut_getxattr_value(ut_ctx, ino, kv);
	}
}


void voluta_ut_getxattr_value(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			      const struct voluta_ut_keyval *kv)
{
	int err;
	size_t vsz;
	void *val;

	vsz = 0;
	err = voluta_ut_getxattr(ut_ctx, ino, kv->name, NULL, 0, &vsz);
	ut_assert_ok(err);
	ut_assert_eq(vsz, kv->size);

	val = ut_randbuf(ut_ctx, vsz);
	err = voluta_ut_getxattr(ut_ctx, ino, kv->name, val, vsz, &vsz);
	ut_assert_ok(err);
	ut_assert_eqm(val, kv->value, kv->size);
}

void voluta_ut_getxattr_nodata(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			       const struct voluta_ut_keyval *kv)

{
	int err;
	size_t bsz = 0;
	char buf[256] = "";

	err = voluta_ut_getxattr(ut_ctx, ino, kv->name,
				 buf, sizeof(buf), &bsz);
	ut_assert_err(err, -ENODATA);
	ut_assert_eq(bsz, 0);
}

void voluta_ut_removexattr_exists(struct voluta_ut_ctx *ut_ctx, ino_t ino,
				  const struct voluta_ut_keyval *kv)
{
	int err;

	err = voluta_ut_removexattr(ut_ctx, ino, kv->name);
	ut_assert_ok(err);

	err = voluta_ut_removexattr(ut_ctx, ino, kv->name);
	ut_assert_err(err, -ENODATA);
}


static struct voluta_ut_keyval *
kvl_search(const struct voluta_ut_kvl *kvl, const char *name)
{
	struct voluta_ut_keyval *kv;

	for (size_t i = 0; i < kvl->count; ++i) {
		kv = kvl->list[i];
		if (strcmp(name, kv->name) != 0) {
			return kv;
		}
	}
	return NULL;
}

void voluta_ut_listxattr_exists(struct voluta_ut_ctx *ut_ctx, ino_t ino,
				const struct voluta_ut_kvl *kvl)
{
	int err;
	const char *name;
	const struct voluta_ut_keyval *kv;
	struct voluta_ut_listxattr_ctx ut_listxattr_ctx;

	err = voluta_ut_listxattr(ut_ctx, ino, &ut_listxattr_ctx);
	ut_assert_ok(err);
	ut_assert_eq(ut_listxattr_ctx.count, kvl->count);

	for (size_t i = 0; i < ut_listxattr_ctx.count; ++i) {
		name = ut_listxattr_ctx.names[i];
		ut_assert_notnull(name);
		kv = kvl_search(kvl, name);
		ut_assert_notnull(kv);
	}
}

void voluta_ut_removexattr_all(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			       const struct voluta_ut_kvl *kvl)
{
	const struct voluta_ut_keyval *kv;

	for (size_t i = 0; i < kvl->count; ++i) {
		kv = kvl->list[i];
		voluta_ut_removexattr_exists(ut_ctx, ino, kv);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_ut_write_iobuf(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			   const struct voluta_ut_iobuf *iobuf)
{
	voluta_ut_write_read(ut_ctx, ino, iobuf->buf,
			     iobuf->len, iobuf->off);
}

void voluta_ut_read_iobuf(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			  const struct voluta_ut_iobuf *iobuf)
{
	void *dat = voluta_ut_zerobuf(ut_ctx, iobuf->len);

	voluta_ut_read_only(ut_ctx, ino, dat, iobuf->len, iobuf->off);
	ut_assert_eqm(dat, iobuf->buf, iobuf->len);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_ut_drop_caches(struct voluta_ut_ctx *ut_ctx)
{
	voluta_env_drop_caches(ut_ctx->env);
}

void voluta_ut_drop_caches_fully(struct voluta_ut_ctx *ut_ctx)
{
	struct voluta_cache_stat cst;

	voluta_ut_drop_caches(ut_ctx);
	voluta_env_cache_stats(ut_ctx->env, &cst);
	ut_assert_eq(cst.nblocks, 0);
	ut_assert_eq(cst.nvnodes, 0);
	ut_assert_eq(cst.ninodes, 0);
}
