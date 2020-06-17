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
#include <sys/statvfs.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include "libvoluta.h"


#define ok_or_goto_out(err_) \
	do { if ((err_) != 0) goto out; } while (0)



static void current_realtime_clock(struct timespec *ts)
{
	int err;
	const clockid_t clk = CLOCK_REALTIME;

	err = voluta_sys_clock_gettime(clk, ts);
	if (err) {
		voluta_panic("clk=%ld err=%d", (long)clk, err);
	}
}

static void voluta_op_start(struct voluta_env *env, bool with_xtime_now)
{
	struct voluta_cache *cache = env->cache;
	const time_t last = env->op_ctx.xtime.start;

	env->op_ctx.xtime.start = time(NULL);
	voluta_cache_relax(cache, env->op_ctx.xtime.start > (last + 8));

	if (with_xtime_now) {
		current_realtime_clock(&env->op_ctx.xtime.now);
	}
}

static void voluta_op_finish(struct voluta_env *env)
{
	struct voluta_oper_ctx *op_ctx = &env->op_ctx;

	op_ctx->xtime.now.tv_sec = 0;
	op_ctx->xtime.now.tv_nsec = 0;
}

static int voluta_op_flush(struct voluta_sb_info *sbi)
{
	return voluta_commit_dirty(sbi, 0);
}

static const struct voluta_oper_ctx *op_ctx_of(struct voluta_env *env)
{
	return &env->op_ctx;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void stat_to_itimes(const struct stat *times,
			   struct voluta_itimes *itimes)
{
	ts_copy(&itimes->atime, &times->st_atim);
	ts_copy(&itimes->mtime, &times->st_mtim);
	ts_copy(&itimes->ctime, &times->st_ctim);
	/* Birth _must_not_ be set from outside */
}

static int symval_to_str(const char *symval, struct voluta_str *str)
{
	size_t symlen;

	symlen = strnlen(symval, VOLUTA_SYMLNK_MAX + 1);
	if (symlen == 0) {
		return -EINVAL;
	}
	if (symlen > VOLUTA_SYMLNK_MAX) {
		return -ENAMETOOLONG;
	}
	str->str = symval;
	str->len = symlen;
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_fs_forget(struct voluta_env *env, ino_t ino)
{
	int err;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, false);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_do_evict_inode(sbi, ino);
	ok_or_goto_out(err);

out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_statfs(struct voluta_env *env, ino_t ino, struct statvfs *stvfs)
{
	int err;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, false);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_statvfs(op_ctx, ii, stvfs);
	ok_or_goto_out(err);

out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_lookup(struct voluta_env *env, ino_t parent_ino,
		     const char *name, struct stat *out_stat)
{
	int err;
	struct voluta_qstr qstr;
	struct voluta_inode_info *ii = NULL;
	struct voluta_inode_info *dir_ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, false);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), parent_ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_do_lookup(op_ctx, dir_ii, &qstr, &ii);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op_ctx, ii, out_stat);
	ok_or_goto_out(err);

out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_getattr(struct voluta_env *env, ino_t ino, struct stat *out_stat)
{
	int err;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, false);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op_ctx, ii, out_stat);
	ok_or_goto_out(err);

out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_access(struct voluta_env *env, ino_t ino, int mode)
{
	int err;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, false);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_access(op_ctx, ii, mode);
	ok_or_goto_out(err);

out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_mkdir(struct voluta_env *env, ino_t parent_ino,
		    const char *name, mode_t mode, struct stat *out_stat)
{
	int err;
	struct voluta_qstr qstr;
	struct voluta_inode_info *ii = NULL;
	struct voluta_inode_info *dir_ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, parent_ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_do_mkdir(op_ctx, dir_ii, &qstr, mode, &ii);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op_ctx, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_rmdir(struct voluta_env *env, ino_t parent_ino, const char *name)
{
	int err;
	struct voluta_qstr qstr;
	struct voluta_inode_info *dir_ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), parent_ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_do_rmdir(op_ctx, dir_ii, &qstr);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_symlink(struct voluta_env *env, ino_t parent_ino,
		      const char *name, const char *symval, struct stat *out)
{
	int err;
	struct voluta_str str;
	struct voluta_qstr qstr;
	struct voluta_inode_info *ii = NULL;
	struct voluta_inode_info *dir_ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, parent_ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = symval_to_str(symval, &str);
	ok_or_goto_out(err);

	err = voluta_do_symlink(op_ctx, dir_ii, &qstr, &str, &ii);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op_ctx, ii, out);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_readlink(struct voluta_env *env, ino_t ino,
		       char *ptr, size_t lim)
{
	int err;
	struct voluta_buf buf;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	buf_init(&buf, ptr, lim);
	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_readlink(op_ctx, ii, &buf);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_unlink(struct voluta_env *env, ino_t parent_ino,
		     const char *name)
{
	int err;
	struct voluta_qstr qstr;
	struct voluta_inode_info *dir_ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, parent_ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_do_unlink(op_ctx, dir_ii, &qstr);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_link(struct voluta_env *env, ino_t ino, ino_t parent_ino,
		   const char *name, struct stat *out_stat)
{
	int err;
	struct voluta_qstr qstr;
	struct voluta_inode_info *ii = NULL;
	struct voluta_inode_info *dir_ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, parent_ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_do_link(op_ctx, dir_ii, &qstr, ii);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op_ctx, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_opendir(struct voluta_env *env, ino_t ino)
{
	int err;
	struct voluta_inode_info *dir_ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_do_opendir(op_ctx, dir_ii);
	ok_or_goto_out(err);

out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_releasedir(struct voluta_env *env, ino_t ino)
{
	int err;
	struct voluta_inode_info *dir_ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_do_releasedir(op_ctx, dir_ii);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_readdir(struct voluta_env *env, ino_t ino,
		      struct voluta_readdir_ctx *rd_ctx)
{
	int err;
	struct voluta_inode_info *dir_ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, false);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_do_readdir(op_ctx, dir_ii, rd_ctx);
	ok_or_goto_out(err);

out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_fsyncdir(struct voluta_env *env, ino_t ino, int datasync)
{
	int err;
	struct voluta_inode_info *dir_ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_do_fsyncdir(op_ctx, dir_ii, datasync);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_chmod(struct voluta_env *env, ino_t ino, mode_t mode,
		    const struct stat *st, struct stat *out_stat)
{
	int err;
	struct voluta_itimes itimes;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	stat_to_itimes(st, &itimes);
	err = voluta_do_chmod(op_ctx, ii, mode, &itimes);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op_ctx, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_chown(struct voluta_env *env, ino_t ino, uid_t uid, gid_t gid,
		    const struct stat *st, struct stat *out_stat)
{
	int err;
	struct voluta_itimes itimes;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	stat_to_itimes(st, &itimes);
	err = voluta_do_chown(op_ctx, ii, uid, gid, &itimes);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op_ctx, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_utimens(struct voluta_env *env, ino_t ino,
		      const struct stat *times, struct stat *out_stat)
{
	int err;
	struct voluta_itimes itimes;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	stat_to_itimes(times, &itimes);
	err = voluta_do_utimens(op_ctx, ii, &itimes);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op_ctx, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_truncate(struct voluta_env *env, ino_t ino, loff_t len,
		       struct stat *out_stat)
{
	int err;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_truncate(op_ctx, ii, len);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op_ctx, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_create(struct voluta_env *env, ino_t parent,
		     const char *name, mode_t mode, struct stat *out_stat)
{
	int err;
	struct voluta_qstr qstr;
	struct voluta_inode_info *ii = NULL;
	struct voluta_inode_info *dir_ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, parent, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_do_create(op_ctx, dir_ii, &qstr, mode, &ii);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op_ctx, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_open(struct voluta_env *env, ino_t ino, int flags)
{
	int err;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_open(op_ctx, ii, flags);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_mknod(struct voluta_env *env, ino_t parent, const char *name,
		    mode_t mode, dev_t rdev, struct stat *out_stat)
{
	int err;
	struct voluta_qstr qstr;
	struct voluta_inode_info *ii = NULL;
	struct voluta_inode_info *dir_ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, parent, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_do_mknod(op_ctx, dir_ii, &qstr, mode, rdev, &ii);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op_ctx, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_release(struct voluta_env *env, ino_t ino)
{
	int err;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_release(op_ctx, ii);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_flush(struct voluta_env *env, ino_t ino)
{
	int err;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_flush(op_ctx, ii);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_fsync(struct voluta_env *env, ino_t ino, int datasync)
{
	int err;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_fsync(op_ctx, ii, datasync);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_rename(struct voluta_env *env, ino_t parent_ino,
		     const char *name, ino_t newparent_ino,
		     const char *newname, int flags)
{
	int err;
	struct voluta_qstr qstr;
	struct voluta_qstr newqstr;
	struct voluta_inode_info *dir_ii = NULL;
	struct voluta_inode_info *newdir_ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, parent_ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, newparent_ino, &newdir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(dir_ii, newname, &newqstr);
	ok_or_goto_out(err);

	err = voluta_do_rename(op_ctx, dir_ii, &qstr,
			       newdir_ii, &newqstr, flags);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_read(struct voluta_env *env, ino_t ino, void *buf,
		   size_t len, loff_t off, size_t *out_len)
{
	int err;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, false);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_read(op_ctx, ii, buf, len, off, out_len);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_read_iter(struct voluta_env *env, ino_t ino,
			struct voluta_rw_iter *rwi)
{
	int err;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, false);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_read_iter(op_ctx, ii, rwi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_write(struct voluta_env *env, ino_t ino, const void *buf,
		    size_t len, off_t off, size_t *out)
{
	int err;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_write(op_ctx, ii, buf, len, off, out);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_write_iter(struct voluta_env *env, ino_t ino,
			 struct voluta_rw_iter *rwi)
{
	int err;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_write_iter(op_ctx, ii, rwi);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_fallocate(struct voluta_env *env, ino_t ino,
			int mode, loff_t offset, loff_t length)
{
	int err;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_fallocate(op_ctx, ii, mode, offset, length);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_lseek(struct voluta_env *env, ino_t ino,
		    loff_t off, int whence, loff_t *out_off)
{
	int err;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_lseek(op_ctx, ii, off, whence, out_off);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_setxattr(struct voluta_env *env, ino_t ino, const char *name,
		       const char *value, size_t size, int flags)
{
	int err;
	struct voluta_str str;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_name_to_str(ii, name, &str);
	ok_or_goto_out(err);

	err = voluta_do_setxattr(op_ctx, ii, &str, value, size, flags);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_getxattr(struct voluta_env *env, ino_t ino, const char *name,
		       void *buf, size_t size, size_t *out_size)
{
	int err;
	struct voluta_str str;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_name_to_str(ii, name, &str);
	ok_or_goto_out(err);

	err = voluta_do_getxattr(op_ctx, ii, &str, buf, size, out_size);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_listxattr(struct voluta_env *env, ino_t ino,
			struct voluta_listxattr_ctx *listxattr_ctx)
{
	int err;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, false);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_listxattr(op_ctx, ii, listxattr_ctx);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_removexattr(struct voluta_env *env, ino_t ino, const char *name)
{
	int err;
	struct voluta_str str;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, true);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_name_to_str(ii, name, &str);
	ok_or_goto_out(err);

	err = voluta_do_removexattr(op_ctx, ii, &str);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_statx(struct voluta_env *env, ino_t ino, struct statx *out_statx)
{
	int err;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, false);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_statx(op_ctx, ii, out_statx);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_fiemap(struct voluta_env *env, ino_t ino, struct fiemap *fm)
{
	int err;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, false);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_fiemap(op_ctx, ii, fm);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_inquiry(struct voluta_env *env, ino_t ino,
		      struct voluta_inquiry *out_inq)
{
	int err;
	struct voluta_inode_info *ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);
	const struct voluta_oper_ctx *op_ctx = op_ctx_of(env);

	voluta_op_start(env, false);

	err = voluta_authorize(sbi, op_ctx);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_inquiry(op_ctx, ii, out_inq);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}
