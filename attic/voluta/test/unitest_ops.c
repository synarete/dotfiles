/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * This file is part of voluta.
 *
 * Copyright (C) 2019 Shachar Sharon
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
#include "unitest.h"

static struct voluta_env *ctx_of(struct voluta_ut_ctx *ut_ctx)
{
	return ut_ctx->env;
}

int voluta_ut_load(struct voluta_ut_ctx *ut_ctx)
{
	voluta_env_setparams(ut_ctx->env, "/", NULL,  ut_ctx->volume_size);
	return voluta_env_load(ut_ctx->env);
}

int voluta_ut_statfs(struct voluta_ut_ctx *ut_ctx, ino_t ino,
		     struct statvfs *st)
{
	return voluta_fs_statfs(ctx_of(ut_ctx), ino, st);
}

int voluta_ut_getattr(struct voluta_ut_ctx *ut_ctx, ino_t ino, struct stat *st)
{
	return voluta_fs_getattr(ctx_of(ut_ctx), ino, st);
}

int voluta_ut_utimens(struct voluta_ut_ctx *ut_ctx, ino_t ino,
		      const struct stat *utimes, struct stat *st)
{
	return voluta_fs_utimens(ctx_of(ut_ctx), ino, utimes, st);
}

int voluta_ut_access(struct voluta_ut_ctx *ut_ctx, ino_t ino, int mode)
{
	return voluta_fs_access(ctx_of(ut_ctx), ino, mode);
}

int voluta_ut_lookup(struct voluta_ut_ctx *ut_ctx, ino_t parent,
		     const char *name, struct stat *out_stat)
{
	return voluta_fs_lookup(ctx_of(ut_ctx), parent, name, out_stat);
}

int voluta_ut_mkdir(struct voluta_ut_ctx *ut_ctx, ino_t parent,
		    const char *name, mode_t mode, struct stat *out)
{
	return voluta_fs_mkdir(ctx_of(ut_ctx), parent,
			       name, mode | S_IFDIR, out);
}

int voluta_ut_rmdir(struct voluta_ut_ctx *ut_ctx, ino_t parent,
		    const char *name)
{
	return voluta_fs_rmdir(ctx_of(ut_ctx), parent, name);
}

int voluta_ut_opendir(struct voluta_ut_ctx *ut_ctx, ino_t ino)
{
	return voluta_fs_opendir(ctx_of(ut_ctx), ino);
}

int voluta_ut_releasedir(struct voluta_ut_ctx *ut_ctx, ino_t ino)
{
	return voluta_fs_releasedir(ctx_of(ut_ctx), ino);
}

int voluta_ut_fsyncdir(struct voluta_ut_ctx *ut_ctx, ino_t ino, bool datasync)
{
	return voluta_fs_fsyncdir(ctx_of(ut_ctx), ino, datasync);
}

static struct voluta_ut_readdir_ctx *
ut_readdir_ctx_of(struct voluta_readdir_ctx *ptr)
{
	return voluta_container_of(ptr, struct voluta_ut_readdir_ctx,
				   readdir_ctx);
}

static int filldir(struct voluta_readdir_ctx *readdir_ctx,
		   const struct voluta_readdir_entry_info *rdei)
{
	size_t ndents_max;
	struct dirent64 *dent;
	struct voluta_ut_readdir_ctx *ut_readdir_ctx;

	ut_readdir_ctx = ut_readdir_ctx_of(readdir_ctx);
	ndents_max = VOLUTA_ARRAY_SIZE(ut_readdir_ctx->dents);

	if ((rdei->off < 0) || !rdei->name || !rdei->name_len) {
		return -EINVAL;
	}
	if (ut_readdir_ctx->ndents >= ndents_max) {
		return -EINVAL;
	}
	dent = &ut_readdir_ctx->dents[ut_readdir_ctx->ndents++];

	ut_assert(rdei->name_len < sizeof(dent->d_name));
	memcpy(dent->d_name, rdei->name, rdei->name_len);
	dent->d_name[rdei->name_len] = '\0';
	dent->d_reclen = (uint16_t)rdei->name_len;
	dent->d_ino = rdei->ino;
	dent->d_type = (uint8_t)rdei->dt;
	dent->d_off = rdei->off;
	return 0;
}

int voluta_ut_readdir(struct voluta_ut_ctx *ut_ctx, ino_t ino,
		      loff_t doff, struct voluta_ut_readdir_ctx *ut_readdir_ctx)
{
	struct voluta_readdir_ctx *readdir_ctx = &ut_readdir_ctx->readdir_ctx;

	ut_readdir_ctx->ndents = 0;
	readdir_ctx->pos = doff;
	readdir_ctx->actor = filldir;
	return voluta_fs_readdir(ctx_of(ut_ctx), ino, readdir_ctx);
}

int voluta_ut_symlink(struct voluta_ut_ctx *ut_ctx, ino_t parent,
		      const char *name, const char *symval, struct stat *out)
{
	return voluta_fs_symlink(ctx_of(ut_ctx), parent, name, symval, out);
}

int voluta_ut_readlink(struct voluta_ut_ctx *ut_ctx,
		       ino_t ino, char *ptr, size_t len)
{
	return voluta_fs_readlink(ctx_of(ut_ctx), ino, ptr, len);
}

int voluta_ut_link(struct voluta_ut_ctx *ut_ctx, ino_t ino, ino_t parent,
		   const char *name, struct stat *out)
{
	return voluta_fs_link(ctx_of(ut_ctx), ino, parent, name, out);
}

int voluta_ut_unlink(struct voluta_ut_ctx *ut_ctx, ino_t parent,
		     const char *name)
{
	return voluta_fs_unlink(ctx_of(ut_ctx), parent, name);
}

int voluta_ut_rename(struct voluta_ut_ctx *ut_ctx, ino_t parent,
		     const char *name,
		     ino_t newparent, const char *newname, int flags)
{
	return voluta_fs_rename(ctx_of(ut_ctx), parent, name,
				newparent, newname, flags);
}

int voluta_ut_create(struct voluta_ut_ctx *ut_ctx, ino_t parent,
		     const char *name,
		     mode_t mode, struct stat *out_stat)
{
	return voluta_fs_create(ctx_of(ut_ctx), parent, name, mode, out_stat);
}

int voluta_ut_mknod(struct voluta_ut_ctx *ut_ctx, ino_t parent,
		    const char *name,
		    mode_t mode, dev_t rdev, struct stat *out_stat)
{
	return voluta_fs_mknod(ctx_of(ut_ctx), parent,
			       name, mode, rdev, out_stat);
}

int voluta_ut_open(struct voluta_ut_ctx *ut_ctx, ino_t ino, int flags)
{
	return voluta_fs_open(ctx_of(ut_ctx), ino, flags);
}

int voluta_ut_release(struct voluta_ut_ctx *ut_ctx, ino_t ino)
{
	return voluta_fs_release(ctx_of(ut_ctx), ino);
}

int voluta_ut_fsync(struct voluta_ut_ctx *ut_ctx, ino_t ino, bool datasync)
{
	return voluta_fs_fsync(ctx_of(ut_ctx), ino, datasync);
}

int voluta_ut_read(struct voluta_ut_ctx *ut_ctx, ino_t ino, void *buf,
		   size_t len, loff_t off, size_t *out_len)
{
	return voluta_fs_read(ctx_of(ut_ctx), ino, buf, len, off, out_len);
}

int voluta_ut_write(struct voluta_ut_ctx *ut_ctx, ino_t ino, const void *buf,
		    size_t len, off_t off, size_t *out_len)
{
	return voluta_fs_write(ctx_of(ut_ctx), ino, buf, len, off, out_len);
}

int voluta_ut_truncate(struct voluta_ut_ctx *ut_ctx, ino_t ino, loff_t length)
{
	struct stat st;

	return voluta_fs_truncate(ctx_of(ut_ctx), ino, length, &st);
}

int voluta_ut_fallocate(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			int mode, loff_t offset, loff_t len)
{
	return voluta_fs_fallocate(ctx_of(ut_ctx), ino, mode, offset, len);
}

int voluta_ut_setxattr(struct voluta_ut_ctx *ut_ctx, ino_t ino,
		       const char *name, const void *value, size_t size, int f)
{
	return voluta_fs_setxattr(ctx_of(ut_ctx), ino, name, value, size, f);
}

int voluta_ut_getxattr(struct voluta_ut_ctx *ut_ctx, ino_t ino,
		       const char *name, void *buf, size_t size, size_t *out)
{
	return voluta_fs_getxattr(ctx_of(ut_ctx), ino, name, buf, size, out);
}

int voluta_ut_removexattr(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			  const char *name)
{
	return voluta_fs_removexattr(ctx_of(ut_ctx), ino, name);
}

static struct voluta_ut_listxattr_ctx *
ut_listxattr_ctx_of(struct voluta_listxattr_ctx *ptr)
{
	return voluta_container_of(ptr, struct voluta_ut_listxattr_ctx,
				   listxattr_ctx);
}

static int fillxent(struct voluta_listxattr_ctx *listxattr_ctx,
		    const char *name, size_t nlen)
{
	char *xname;
	size_t limit;
	struct voluta_ut_listxattr_ctx *ut_listxattr_ctx;

	ut_listxattr_ctx = ut_listxattr_ctx_of(listxattr_ctx);

	limit = sizeof(ut_listxattr_ctx->names);
	if (ut_listxattr_ctx->count == limit) {
		return -EINVAL;
	}
	xname = voluta_ut_strndup(ut_listxattr_ctx->ut_ctx, name, nlen);
	ut_listxattr_ctx->names[ut_listxattr_ctx->count++] = xname;
	return 0;
}

int voluta_ut_listxattr(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			struct voluta_ut_listxattr_ctx *ut_listxattr_ctx)
{
	struct voluta_listxattr_ctx *listxattr_ctx =
			&ut_listxattr_ctx->listxattr_ctx;

	memset(ut_listxattr_ctx, 0, sizeof(*ut_listxattr_ctx));
	ut_listxattr_ctx->ut_ctx = ut_ctx;
	ut_listxattr_ctx->listxattr_ctx.actor = fillxent;

	return voluta_fs_listxattr(ctx_of(ut_ctx), ino, listxattr_ctx);
}


