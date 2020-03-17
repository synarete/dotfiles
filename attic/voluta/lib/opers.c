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
#include "voluta-lib.h"


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
	struct voluta_cache *cache = &env->cache;
	const time_t last = env->opi.xtime.start;

	env->opi.xtime.start = time(NULL);
	voluta_cache_next_cycle(cache);
	voluta_cache_relax(cache, env->opi.xtime.start > (last + 8));

	if (with_xtime_now) {
		current_realtime_clock(&env->opi.xtime.now);
	}
}

static void voluta_op_finish(struct voluta_env *env)
{
	env->opi.xtime.now.tv_sec = 0;
	env->opi.xtime.now.tv_nsec = 0;
}

static int voluta_op_flush(struct voluta_env *env)
{
	return voluta_commit_dirtyq(&env->sbi, 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void stat_times_to_attr(const struct stat *times,
			       struct voluta_iattr *attr)
{
	iattr_init(attr, NULL);
	ts_copy(&attr->ia_atime, &times->st_atim);
	ts_copy(&attr->ia_mtime, &times->st_mtim);
	ts_copy(&attr->ia_ctime, &times->st_ctim);
	/* Birth _must_not_ be set from outside */
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_fs_forget(struct voluta_env *env, ino_t ino)
{
	int err;

	voluta_op_start(env, false);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_do_evict_inode(sbi_of(env), ino);
	ok_or_goto_out(err);

out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_statfs(struct voluta_env *env, ino_t ino, struct statvfs *stvfs)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	voluta_op_start(env, false);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_statvfs(sbi_of(env), stvfs);
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

	voluta_op_start(env, false);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), parent_ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(sbi_of(env), dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_do_lookup(sbi_of(env), opi_of(env), dir_ii, &qstr, &ii);
	ok_or_goto_out(err);

	err = voluta_do_getattr(env, ii, out_stat);
	ok_or_goto_out(err);

out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_getattr(struct voluta_env *env, ino_t ino, struct stat *out_stat)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	voluta_op_start(env, false);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_getattr(env, ii, out_stat);
	ok_or_goto_out(err);

out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_access(struct voluta_env *env, ino_t ino, int mode)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	voluta_op_start(env, false);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_access(opi_of(env), ii, mode);
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

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), parent_ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(sbi_of(env), dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_do_mkdir(env, dir_ii, &qstr, mode, &ii);
	ok_or_goto_out(err);

	err = voluta_do_getattr(env, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
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

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), parent_ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(sbi_of(env), dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_do_rmdir(env, dir_ii, &qstr);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_symlink(struct voluta_env *env, ino_t parent_ino,
		      const char *name, const char *symval, struct stat *out_st)
{
	int err;
	struct voluta_str str;
	struct voluta_qstr qstr;
	struct voluta_inode_info *ii = NULL;
	struct voluta_inode_info *dir_ii = NULL;

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), parent_ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(sbi_of(env), dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_symval_to_str(symval, &str);
	ok_or_goto_out(err);

	err = voluta_do_symlink(env, dir_ii, &qstr, &str, &ii);
	ok_or_goto_out(err);

	err = voluta_do_getattr(env, ii, out_st);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_readlink(struct voluta_env *env, ino_t ino, char *ptr, size_t lim)
{
	int err;
	struct voluta_buf buf;
	struct voluta_inode_info *ii = NULL;

	voluta_buf_init(&buf, ptr, lim);
	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_readlink(sbi_of(env), ii, &buf);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_unlink(struct voluta_env *env, ino_t parent_ino, const char *name)
{
	int err;
	struct voluta_qstr qstr;
	struct voluta_inode_info *dir_ii = NULL;

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), parent_ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(sbi_of(env), dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_do_unlink(env, dir_ii, &qstr);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
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

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), parent_ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(sbi_of(env), dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_do_link(env, dir_ii, &qstr, ii);
	ok_or_goto_out(err);

	err = voluta_do_getattr(env, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_opendir(struct voluta_env *env, ino_t ino)
{
	int err;
	struct voluta_inode_info *dir_ii = NULL;

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_do_opendir(sbi_of(env), opi_of(env), dir_ii);
	ok_or_goto_out(err);

out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_releasedir(struct voluta_env *env, ino_t ino)
{
	int err;
	struct voluta_inode_info *dir_ii = NULL;

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_do_releasedir(sbi_of(env), opi_of(env), dir_ii);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_readdir(struct voluta_env *env, ino_t ino,
		      struct voluta_readdir_ctx *readdir_ctx)
{
	int err;
	struct voluta_inode_info *dir_ii = NULL;

	voluta_op_start(env, false);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_do_readdir(sbi_of(env), opi_of(env), dir_ii, readdir_ctx);
	ok_or_goto_out(err);

out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_fsyncdir(struct voluta_env *env, ino_t ino, int datasync)
{
	int err;
	struct voluta_inode_info *dir_ii = NULL;

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_do_fsyncdir(sbi_of(env), opi_of(env), dir_ii, datasync);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_chmod(struct voluta_env *env, ino_t ino, mode_t mode,
		    const struct stat *times, struct stat *out_stat)
{
	int err;
	struct voluta_iattr attr;
	struct voluta_inode_info *ii = NULL;

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	stat_times_to_attr(times, &attr);
	err = voluta_do_chmod(sbi_of(env), opi_of(env), ii, mode, &attr);
	ok_or_goto_out(err);

	err = voluta_do_getattr(env, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_chown(struct voluta_env *env, ino_t ino, uid_t uid, gid_t gid,
		    const struct stat *times, struct stat *out_stat)
{
	int err;
	struct voluta_iattr attr;
	struct voluta_inode_info *ii = NULL;

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	stat_times_to_attr(times, &attr);
	err = voluta_do_chown(env, ii, uid, gid, &attr);
	ok_or_goto_out(err);

	err = voluta_do_getattr(env, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_utimens(struct voluta_env *env, ino_t ino,
		      const struct stat *times, struct stat *out_stat)
{
	int err;
	struct voluta_iattr attr;
	struct voluta_inode_info *ii = NULL;

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	stat_times_to_attr(times, &attr);
	err = voluta_do_utimens(opi_of(env), ii, &attr);
	ok_or_goto_out(err);

	err = voluta_do_getattr(env, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
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

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_truncate(env, ii, len);
	ok_or_goto_out(err);

	err = voluta_do_getattr(env, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
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

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), parent, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(sbi_of(env), dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_do_create(env, dir_ii, &qstr, mode, &ii);
	ok_or_goto_out(err);

	err = voluta_do_getattr(env, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_open(struct voluta_env *env, ino_t ino, int flags)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_open(env, ii, flags);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
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

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), parent, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(sbi_of(env), dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_do_mknod(env, dir_ii, &qstr, mode, rdev, &ii);
	ok_or_goto_out(err);

	err = voluta_do_getattr(env, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_release(struct voluta_env *env, ino_t ino)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_release(env, ii);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_flush(struct voluta_env *env, ino_t ino)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_flush(env, ii);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_fsync(struct voluta_env *env, ino_t ino, int datasync)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_fsync(env, ii, datasync);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
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
	struct voluta_qstr qstr, newqstr;
	struct voluta_inode_info *dir_ii = NULL;
	struct voluta_inode_info *newdir_ii = NULL;

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), parent_ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), newparent_ino, &newdir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(sbi_of(env), dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(sbi_of(env), dir_ii, newname, &newqstr);
	ok_or_goto_out(err);

	err = voluta_do_rename(env, dir_ii, &qstr, newdir_ii, &newqstr, flags);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
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

	voluta_op_start(env, false);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_read(env, ii, buf, len, off, out_len);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
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

	voluta_op_start(env, false);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_read_iter(env, ii, rwi);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
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

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_write(sbi_of(env), opi_of(env), ii, buf, len, off, out);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
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

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_write_iter(sbi_of(env), opi_of(env), ii, rwi);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
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

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_fallocate(env, ii, mode, offset, length);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
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

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	err = voluta_name_to_str(sbi_of(env), ii, name, &str);
	ok_or_goto_out(err);

	err = voluta_do_setxattr(sbi_of(env), opi_of(env),
				 ii, &str, value, size, flags);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
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

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	err = voluta_name_to_str(sbi_of(env), ii, name, &str);
	ok_or_goto_out(err);

	err = voluta_do_getxattr(sbi_of(env), opi_of(env),
				 ii, &str, buf, size, out_size);
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

	voluta_op_start(env, false);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_listxattr(sbi_of(env), opi_of(env), ii, listxattr_ctx);
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

	voluta_op_start(env, true);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	err = voluta_name_to_str(sbi_of(env), ii, name, &str);
	ok_or_goto_out(err);

	err = voluta_do_removexattr(sbi_of(env), opi_of(env), ii, &str);
	ok_or_goto_out(err);

	err = voluta_op_flush(env);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

int voluta_fs_statx(struct voluta_env *env, ino_t ino, struct statx *out_statx)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	voluta_op_start(env, false);

	err = voluta_authorize(env);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi_of(env), ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_statx(env, ii, out_statx);
	ok_or_goto_out(err);
out:
	voluta_op_finish(env);
	return err;
}

