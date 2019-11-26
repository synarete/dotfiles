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
 *
 */
#ifndef VOLUTA_VOLUTA_H_
#define VOLUTA_VOLUTA_H_

#include <unistd.h>
#include <stdlib.h>
#include "xsyscall.h"
#include "voluta-defs.h"
#include "voluta-infra.h"


/* Types forward declarations */
struct iovec;
struct stat;
struct statvfs;
struct statx;

struct voluta_env;
struct voluta_rw_iter;
struct voluta_readdir_ctx;
struct voluta_readdir_entry_info;
struct voluta_listxattr_ctx;


struct voluta_ucred {
	uid_t uid;
	gid_t gid;
	pid_t pid;
	mode_t umask;
};


struct voluta_cache_stat {
	size_t nblocks;
	size_t ninodes;
	size_t nvnodes;
};


typedef int (*voluta_filldir_fn)(struct voluta_readdir_ctx *readdir_ctx,
				 const struct voluta_readdir_entry_info *rdei);

struct voluta_readdir_entry_info {
	const char *name;
	size_t name_len;
	ino_t ino;
	mode_t dt;
	loff_t off;
};

struct voluta_readdir_ctx {
	voluta_filldir_fn actor;
	loff_t pos;
};

typedef int (*voluta_fillxattr_fn)(struct voluta_listxattr_ctx *listxattr_ctx,
				   const char *name, size_t name_len);

struct voluta_listxattr_ctx {
	voluta_fillxattr_fn actor;
};

typedef int (*voluta_rw_iter_fn)(struct voluta_rw_iter *,
				 const struct voluta_qmref *);

struct voluta_rw_iter {
	voluta_rw_iter_fn actor;
	loff_t off;
	size_t len;
};


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_lib_init(void); /* TODO: have fini_lib */

int voluta_env_new(struct voluta_env **out_env);

void voluta_env_del(struct voluta_env *env);

int voluta_env_load(struct voluta_env *);

int voluta_env_exec(struct voluta_env *);

int voluta_env_term(struct voluta_env *);

void voluta_env_halt(struct voluta_env *env, int signum);

void voluta_env_drop_caches(struct voluta_env *env);

void voluta_env_setmode(struct voluta_env *env, bool pedantic);

void voluta_env_setcreds(struct voluta_env *env,
			 const struct voluta_ucred *ucred);

void voluta_env_setparams(struct voluta_env *env, const char *mntpoint,
			  const char *volpath, loff_t volsize);

size_t voluta_env_allocated_mem(const struct voluta_env *env);

void voluta_env_cache_stats(const struct voluta_env *env,
			    struct voluta_cache_stat *);

int voluta_env_setup_key(struct voluta_env *, const char *, const char *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_fs_format(struct voluta_env *env, const char *path, loff_t size);

int voluta_fs_forget(struct voluta_env *env, ino_t ino);

int voluta_fs_statfs(struct voluta_env *env, ino_t ino,
		     struct statvfs *out_statvfs);

int voluta_fs_lookup(struct voluta_env *env, ino_t parent_ino,
		     const char *name, struct stat *out_stat);

int voluta_fs_mkdir(struct voluta_env *env, ino_t parent_ino,
		    const char *name, mode_t mode, struct stat *out_stat);

int voluta_fs_rmdir(struct voluta_env *env, ino_t parent_ino, const char *name);

int voluta_fs_access(struct voluta_env *env, ino_t ino, int mode);

int voluta_fs_getattr(struct voluta_env *env, ino_t ino, struct stat *out_stat);

int voluta_fs_chmod(struct voluta_env *env, ino_t ino, mode_t mode,
		    const struct stat *times, struct stat *out_stat);

int voluta_fs_chown(struct voluta_env *env, ino_t ino, uid_t uid, gid_t gid,
		    const struct stat *times, struct stat *out_stat);

int voluta_fs_truncate(struct voluta_env *env, ino_t ino, loff_t len,
		       struct stat *out_stat);

int voluta_fs_utimens(struct voluta_env *env, ino_t ino,
		      const struct stat *times, struct stat *out_stat);

int voluta_fs_symlink(struct voluta_env *env, ino_t parent_ino,
		      const char *name,
		      const char *val, struct stat *out_stat);

int voluta_fs_readlink(struct voluta_env *env, ino_t ino, char *ptr,
		       size_t len);

int voluta_fs_link(struct voluta_env *env, ino_t ino, ino_t parent_ino,
		   const char *name, struct stat *out_stat);

int voluta_fs_unlink(struct voluta_env *env, ino_t parent_ino,
		     const char *name);

int voluta_fs_rename(struct voluta_env *env, ino_t parent_ino,
		     const char *name, ino_t newparent_ino,
		     const char *newname, int flags);

int voluta_fs_opendir(struct voluta_env *env, ino_t ino);

int voluta_fs_releasedir(struct voluta_env *env, ino_t ino);

int voluta_fs_readdir(struct voluta_env *env, ino_t ino,
		      struct voluta_readdir_ctx *readdir_ctx);

int voluta_fs_fsyncdir(struct voluta_env *env, ino_t ino, int datasync);

int voluta_fs_create(struct voluta_env *env, ino_t parent,
		     const char *name, mode_t mode, struct stat *out_stat);

int voluta_fs_open(struct voluta_env *env, ino_t ino, int flags);

int voluta_fs_mknod(struct voluta_env *env, ino_t parent, const char *name,
		    mode_t mode, dev_t rdev, struct stat *out_stat);

int voluta_fs_release(struct voluta_env *env, ino_t ino);

int voluta_fs_flush(struct voluta_env *env, ino_t ino);

int voluta_fs_fsync(struct voluta_env *env, ino_t ino, int datasync);

int voluta_fs_getxattr(struct voluta_env *env, ino_t ino, const char *name,
		       void *buf, size_t size, size_t *out_size);

int voluta_fs_setxattr(struct voluta_env *env, ino_t ino, const char *name,
		       const char *value, size_t size, int flags);

int voluta_fs_listxattr(struct voluta_env *env, ino_t ino,
			struct voluta_listxattr_ctx *listxattr_ctx);

int voluta_fs_removexattr(struct voluta_env *env, ino_t ino, const char *name);

int voluta_fs_fallocate(struct voluta_env *env, ino_t ino,
			int mode, loff_t offset, loff_t length);

int voluta_fs_read(struct voluta_env *env, ino_t ino, void *buf,
		   size_t len, loff_t off, size_t *out_len);

int voluta_fs_read_iter(struct voluta_env *env, ino_t ino,
			struct voluta_rw_iter *io_itr);


int voluta_fs_write(struct voluta_env *env, ino_t ino, const void *buf,
		    size_t len, off_t off, size_t *out_len);

int voluta_fs_write_iter(struct voluta_env *env, ino_t ino,
			 struct voluta_rw_iter *rwi);

int voluta_fs_statx(struct voluta_env *env, ino_t ino, struct statx *out_statx);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_vol_resolve_size(const char *path,
			    loff_t size_want, loff_t *out_size);

int voluta_vol_check_path(const char *path);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

extern const char voluta_version_string[];

#endif /* VOLUTA_VOLUTA_H_ */


