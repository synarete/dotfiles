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


static void op_try_relax(struct voluta_sb_info *sbi, time_t now)
{
	size_t rate;
	size_t nops;
	size_t diff;
	size_t nrelax = 0;
	const struct voluta_qalloc *qal = sbi->s_qalloc;
	struct voluta_oper_stat *op_st = &sbi->s_opstat;

	/* Low-activity case */
	nops = op_st->op_count - op_st->op_count_mark;
	if ((nops > 1) && (now > op_st->op_time)) {
		diff = (size_t)(now - op_st->op_time);
		rate = nops / diff;

		if (now > (op_st->op_time + 1)) {
			nrelax += min(16, diff);
		}
		if (rate && (rate < 10)) {
			nrelax += 1;
		}
	}

	/* Memory pressure case */
	if ((qal->st.npages_used * 4) > qal->st.npages) {
		nrelax += 1;
	}
	if ((qal->st.npages_used * 2) > qal->st.npages) {
		nrelax += 1;
	}

	if (nrelax) {
		voluta_cache_relax(sbi->s_cache, nrelax);
		op_st->op_count_mark = op_st->op_count;
	}
}

static void op_start(struct voluta_sb_info *sbi,
		     const struct voluta_oper *op_const, bool realtime)
{
	const time_t now = time(NULL);
	struct voluta_oper *op = unconst(op_const);

	/* XXX crap, remove after fuseq */
	voluta_ts_gettime(&op->xtime, realtime);

	op_try_relax(sbi, now);
	sbi->s_opstat.op_time = now;
	sbi->s_opstat.op_count++;
}

static void op_finish(struct voluta_sb_info *sbi,
		      const struct voluta_oper *op)
{
	struct voluta_oper *op2 = unconst(op);

	/* XXX crap, remove after fuseq */
	op2->xtime.tv_sec = 0;

	unused(sbi);
}

static int voluta_op_flush(struct voluta_sb_info *sbi)
{
	return voluta_commit_dirty(sbi, 0);
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

int voluta_fs_forget(struct voluta_sb_info *sbi,
		     const struct voluta_oper *op,
		     ino_t ino, size_t nlookup)
{
	int err;

	op_start(sbi, op, false);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_do_forget_inode(sbi, ino, nlookup);
	ok_or_goto_out(err);

out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_statfs(struct voluta_sb_info *sbi,
		     const struct voluta_oper *op,
		     ino_t ino, struct statvfs *stvfs)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, false);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_statvfs(op, ii, stvfs);
	ok_or_goto_out(err);

out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_lookup(struct voluta_sb_info *sbi,
		     const struct voluta_oper *op, ino_t parent,
		     const char *name, struct stat *out_stat)
{
	int err;
	struct voluta_qstr qstr;
	struct voluta_inode_info *ii = NULL;
	struct voluta_inode_info *dir_ii = NULL;

	op_start(sbi, op, false);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, parent, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_do_lookup(op, dir_ii, &qstr, &ii);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op, ii, out_stat);
	ok_or_goto_out(err);

out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_getattr(struct voluta_sb_info *sbi,
		      const struct voluta_oper *op,
		      ino_t ino, struct stat *out_stat)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, false);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op, ii, out_stat);
	ok_or_goto_out(err);

out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_access(struct voluta_sb_info *sbi,
		     const struct voluta_oper *op, ino_t ino, int mode)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, false);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_access(op, ii, mode);
	ok_or_goto_out(err);

out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_mkdir(struct voluta_sb_info *sbi,
		    const struct voluta_oper *op, ino_t parent,
		    const char *name, mode_t mode, struct stat *out_stat)
{
	int err;
	struct voluta_qstr qstr;
	struct voluta_inode_info *ii = NULL;
	struct voluta_inode_info *dir_ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, parent, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_do_mkdir(op, dir_ii, &qstr, mode, &ii);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_rmdir(struct voluta_sb_info *sbi,
		    const struct voluta_oper *op,
		    ino_t parent, const char *name)
{
	int err;
	struct voluta_qstr qstr;
	struct voluta_inode_info *dir_ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, parent, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_do_rmdir(op, dir_ii, &qstr);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_symlink(struct voluta_sb_info *sbi,
		      const struct voluta_oper *op, ino_t parent,
		      const char *name, const char *symval,
		      struct stat *out_stat)
{
	int err;
	struct voluta_str str;
	struct voluta_qstr qstr;
	struct voluta_inode_info *ii = NULL;
	struct voluta_inode_info *dir_ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, parent, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = symval_to_str(symval, &str);
	ok_or_goto_out(err);

	err = voluta_do_symlink(op, dir_ii, &qstr, &str, &ii);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_readlink(struct voluta_sb_info *sbi,
		       const struct voluta_oper *op,
		       ino_t ino, char *ptr, size_t lim)
{
	int err;
	struct voluta_buf buf;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, true);
	buf_init(&buf, ptr, lim);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_readlink(op, ii, &buf);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_unlink(struct voluta_sb_info *sbi,
		     const struct voluta_oper *op,
		     ino_t parent, const char *name)
{
	int err;
	struct voluta_qstr qstr;
	struct voluta_inode_info *dir_ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, parent, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_do_unlink(op, dir_ii, &qstr);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_link(struct voluta_sb_info *sbi,
		   const struct voluta_oper *op, ino_t ino, ino_t parent,
		   const char *name, struct stat *out_stat)
{
	int err;
	struct voluta_qstr qstr;
	struct voluta_inode_info *ii = NULL;
	struct voluta_inode_info *dir_ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, parent, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_do_link(op, dir_ii, &qstr, ii);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_opendir(struct voluta_sb_info *sbi,
		      const struct voluta_oper *op, ino_t ino)
{
	int err;
	struct voluta_inode_info *dir_ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_do_opendir(op, dir_ii);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_releasedir(struct voluta_sb_info *sbi,
			 const struct voluta_oper *op, ino_t ino, int o_flags)
{
	int err;
	struct voluta_inode_info *dir_ii = NULL;

	unused(o_flags);
	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_do_releasedir(op, dir_ii);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_readdir(struct voluta_sb_info *sbi,
		      const struct voluta_oper *op, ino_t ino,
		      struct voluta_readdir_ctx *rd_ctx)
{
	int err;
	struct voluta_inode_info *dir_ii = NULL;

	op_start(sbi, op, false);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_do_readdir(op, dir_ii, rd_ctx);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_readdirplus(struct voluta_sb_info *sbi,
			  const struct voluta_oper *op, ino_t ino,
			  struct voluta_readdir_ctx *rd_ctx)
{
	int err;
	struct voluta_inode_info *dir_ii = NULL;

	op_start(sbi, op, false);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_do_readdirplus(op, dir_ii, rd_ctx);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_fsyncdir(struct voluta_sb_info *sbi,
		       const struct voluta_oper *op, ino_t ino, bool datasync)
{
	int err;
	struct voluta_inode_info *dir_ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_do_fsyncdir(op, dir_ii, datasync);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_chmod(struct voluta_sb_info *sbi,
		    const struct voluta_oper *op, ino_t ino, mode_t mode,
		    const struct stat *st, struct stat *out_stat)
{
	int err;
	struct voluta_itimes itimes;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	stat_to_itimes(st, &itimes);
	err = voluta_do_chmod(op, ii, mode, &itimes);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_chown(struct voluta_sb_info *sbi,
		    const struct voluta_oper *op, ino_t ino, uid_t uid,
		    gid_t gid, const struct stat *st, struct stat *out_stat)
{
	int err;
	struct voluta_itimes itimes;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	stat_to_itimes(st, &itimes);
	err = voluta_do_chown(op, ii, uid, gid, &itimes);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_utimens(struct voluta_sb_info *sbi,
		      const struct voluta_oper *op, ino_t ino,
		      const struct stat *times, struct stat *out_stat)
{
	int err;
	struct voluta_itimes itimes;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	stat_to_itimes(times, &itimes);
	err = voluta_do_utimens(op, ii, &itimes);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_truncate(struct voluta_sb_info *sbi,
		       const struct voluta_oper *op, ino_t ino, loff_t len,
		       struct stat *out_stat)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_truncate(op, ii, len);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_create(struct voluta_sb_info *sbi,
		     const struct voluta_oper *op, ino_t parent,
		     const char *name, int o_flags, mode_t mode,
		     struct stat *out_stat)
{
	int err;
	struct voluta_qstr qstr;
	struct voluta_inode_info *ii = NULL;
	struct voluta_inode_info *dir_ii = NULL;

	unused(o_flags); /* XXX use me */
	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, parent, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_do_create(op, dir_ii, &qstr, mode, &ii);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_open(struct voluta_sb_info *sbi,
		   const struct voluta_oper *op, ino_t ino, int o_flags)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_open(op, ii, o_flags);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_mknod(struct voluta_sb_info *sbi, const struct voluta_oper *op,
		    ino_t parent, const char *name, mode_t mode, dev_t rdev,
		    struct stat *out_stat)
{
	int err;
	struct voluta_qstr qstr;
	struct voluta_inode_info *ii = NULL;
	struct voluta_inode_info *dir_ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, parent, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_do_mknod(op, dir_ii, &qstr, mode, rdev, &ii);
	ok_or_goto_out(err);

	err = voluta_do_getattr(op, ii, out_stat);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_release(struct voluta_sb_info *sbi,
		      const struct voluta_oper *op,
		      ino_t ino, int o_flags, bool flush)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	/* TODO: useme */
	unused(flush);
	unused(o_flags);

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_release(op, ii);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_flush(struct voluta_sb_info *sbi,
		    const struct voluta_oper *op, ino_t ino)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_flush(op, ii);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_fsync(struct voluta_sb_info *sbi,
		    const struct voluta_oper *op,
		    ino_t ino, bool datasync)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_fsync(op, ii, datasync);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_rename(struct voluta_sb_info *sbi,
		     const struct voluta_oper *op, ino_t parent,
		     const char *name, ino_t newparent,
		     const char *newname, int flags)
{
	int err;
	struct voluta_qstr qstr;
	struct voluta_qstr newqstr;
	struct voluta_inode_info *dir_ii = NULL;
	struct voluta_inode_info *newdir_ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, parent, &dir_ii);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, newparent, &newdir_ii);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(dir_ii, name, &qstr);
	ok_or_goto_out(err);

	err = voluta_name_to_qstr(dir_ii, newname, &newqstr);
	ok_or_goto_out(err);

	err = voluta_do_rename(op, dir_ii, &qstr,
			       newdir_ii, &newqstr, flags);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_read(struct voluta_sb_info *sbi,
		   const struct voluta_oper *op, ino_t ino, void *buf,
		   size_t len, loff_t off, size_t *out_len)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, false);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_read(op, ii, buf, len, off, out_len);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_read_iter(struct voluta_sb_info *sbi,
			const struct voluta_oper *op, ino_t ino,
			struct voluta_rw_iter *rwi)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, false);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_read_iter(op, ii, rwi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_write(struct voluta_sb_info *sbi,
		    const struct voluta_oper *op, ino_t ino,
		    const void *buf, size_t len, off_t off, size_t *out_len)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_write(op, ii, buf, len, off, out_len);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_write_iter(struct voluta_sb_info *sbi,
			 const struct voluta_oper *op, ino_t ino,
			 struct voluta_rw_iter *rwi)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_write_iter(op, ii, rwi);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_fallocate(struct voluta_sb_info *sbi,
			const struct voluta_oper *op, ino_t ino,
			int mode, loff_t offset, loff_t length)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_fallocate(op, ii, mode, offset, length);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_lseek(struct voluta_sb_info *sbi,
		    const struct voluta_oper *op, ino_t ino,
		    loff_t off, int whence, loff_t *out_off)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_lseek(op, ii, off, whence, out_off);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_setxattr(struct voluta_sb_info *sbi,
		       const struct voluta_oper *op, ino_t ino,
		       const char *name, const void *value,
		       size_t size, int flags)
{
	int err;
	struct voluta_str str;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_name_to_str(ii, name, &str);
	ok_or_goto_out(err);

	err = voluta_do_setxattr(op, ii, &str, value, size, flags);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_getxattr(struct voluta_sb_info *sbi,
		       const struct voluta_oper *op, ino_t ino,
		       const char *name, void *buf, size_t size,
		       size_t *out_size)
{
	int err;
	struct voluta_str str;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_name_to_str(ii, name, &str);
	ok_or_goto_out(err);

	err = voluta_do_getxattr(op, ii, &str, buf, size, out_size);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_listxattr(struct voluta_sb_info *sbi,
			const struct voluta_oper *op, ino_t ino,
			struct voluta_listxattr_ctx *lxa_ctx)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, false);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_listxattr(op, ii, lxa_ctx);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_removexattr(struct voluta_sb_info *sbi,
			  const struct voluta_oper *op,
			  ino_t ino, const char *name)
{
	int err;
	struct voluta_str str;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, true);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_name_to_str(ii, name, &str);
	ok_or_goto_out(err);

	err = voluta_do_removexattr(op, ii, &str);
	ok_or_goto_out(err);

	err = voluta_op_flush(sbi);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_statx(struct voluta_sb_info *sbi,
		    const struct voluta_oper *op, ino_t ino,
		    struct statx *out_stx)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, false);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_statx(op, ii, out_stx);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_fiemap(struct voluta_sb_info *sbi,
		     const struct voluta_oper *op,
		     ino_t ino, struct fiemap *fm)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, false);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_fiemap(op, ii, fm);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}

int voluta_fs_query(struct voluta_sb_info *sbi,
		    const struct voluta_oper *op, ino_t ino,
		    struct voluta_query *out_qry)
{
	int err;
	struct voluta_inode_info *ii = NULL;

	op_start(sbi, op, false);

	err = voluta_authorize(sbi, op);
	ok_or_goto_out(err);

	err = voluta_stage_inode(sbi, ino, &ii);
	ok_or_goto_out(err);

	err = voluta_do_query(op, ii, out_qry);
	ok_or_goto_out(err);
out:
	op_finish(sbi, op);
	return err;
}
