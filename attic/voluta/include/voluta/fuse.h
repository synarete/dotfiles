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
#ifndef VOLUTA_FUSE_H_
#define VOLUTA_FUSE_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

/* File-system top-level interfaces */
struct statx;
struct voluta_sb_info;
struct voluta_oper;
struct voluta_rw_iter;
struct voluta_readdir_ctx;
struct voluta_readdir_info;
struct voluta_listxattr_ctx;

typedef int (*voluta_filldir_fn)(struct voluta_readdir_ctx *rd_ctx,
				 const struct voluta_readdir_info *rdi);

typedef int (*voluta_fillxattr_fn)(struct voluta_listxattr_ctx *lxa_ctx,
				   const char *name, size_t name_len);

struct voluta_readdir_info {
	struct stat attr;
	const char *name;
	size_t namelen;
	ino_t ino;
	loff_t off;
	mode_t dt;
	int pad;
};

struct voluta_readdir_ctx {
	voluta_filldir_fn actor;
	loff_t pos;
};

struct voluta_listxattr_ctx {
	voluta_fillxattr_fn actor;
};

typedef int (*voluta_rw_iter_fn)(struct voluta_rw_iter *rwi,
				 const struct voluta_memref *mref);

struct voluta_rw_iter {
	voluta_rw_iter_fn actor;
	loff_t off;
	size_t len;
};

/* Current operation state */
struct voluta_oper {
	struct voluta_ucred ucred;
	struct timespec     xtime;
	unsigned long       unique;
	int                 opcode;
};

/* FUSE-bridge machinery */


struct voluta_mntparams {
	const char *path;
	uint64_t flags;
	size_t max_read;
	mode_t rootmode;
	uid_t  user_id;
	gid_t  group_id;
};

struct voluta_mntctl {
	char mc_path[VOLUTA_PATH_MAX];
	struct voluta_mntmsg    mc_mmsg;
	struct voluta_socket    mc_asock;
	struct voluta_sockaddr  mc_peer;
	struct voluta_mntparams mc_params;
	pid_t mc_pid;
	uid_t mc_uid;
	gid_t mc_gid;
	int   mc_active;
	int   mc_fuse_fd;
	int   mc_pad;
};

struct voluta_mntsrv {
	struct voluta_socket    ms_lsock;
	struct voluta_mntctl    ms_ctl;
};

struct voluta_ms_env {
	struct voluta_mntsrv    mntsrv;
	int active;
	int pad;
};

struct voluta_fuseq_ctx;
union voluta_fuseq_in;
struct voluta_fuseq_inb;
struct voluta_fuseq_outb;

typedef void (*voluta_fuseq_hook)(struct voluta_fuseq_ctx *, ino_t,
				  const union voluta_fuseq_in *);

struct voluta_opdesc {
	voluta_fuseq_hook hook;
	const char       *name;
	int               code;
	int               realtime;
};

struct voluta_fuseq_databuf {
	uint8_t buf[VOLUTA_IO_SIZE_MAX];
};

struct voluta_fuseq_pathbuf {
	char path[VOLUTA_PATH_MAX];
};

struct voluta_fuseq_xattrbuf {
	char value[VOLUTA_XATTR_VALUE_MAX];
};

struct voluta_fuseq_diter {
	char   buf[8 * VOLUTA_UKILO];
	struct voluta_readdir_ctx rd_ctx;
	size_t bsz;
	size_t len;
	size_t ndes;
	char   de_name[VOLUTA_NAME_MAX + 1];
	struct stat de_attr;
	loff_t de_off;
	size_t de_nlen;
	ino_t  de_ino;
	mode_t de_dt;
	int    plus;
};

struct voluta_fuseq_xiter {
	struct voluta_listxattr_ctx lxa;
	size_t cnt;
	const char *beg;
	const char *end;
	char *cur;
	char buf[64 * VOLUTA_UKILO];
};

struct voluta_fuseq_ctx {
	const struct voluta_opdesc *opdsc;
	struct voluta_fuseq        *fq;
	struct voluta_sb_info      *sbi;
	struct voluta_fuseq_inb    *inb;
	struct voluta_fuseq_outb   *outb;
	struct voluta_oper          op;
};

struct voluta_fuseq_pipe {
	int pipe[2];
	size_t size;
	int can_grow;
	int pad;
};

struct voluta_fuseq_conn {
	int     proto_major;
	int     proto_minor;
	int     cap_kern;
	int     cap_want;
	size_t  pagesize;
	size_t  buffsize;
	size_t  max_write;
	size_t  max_read;
	size_t  max_readahead;
	size_t  max_background;
	size_t  congestion_threshold;
	size_t  time_gran;
};

struct voluta_fuseq {
	struct voluta_fuseq_ctx  fqc;
	struct voluta_sb_info   *fq_sbi;
	struct voluta_qalloc    *fq_qal;
	struct voluta_fuseq_conn fq_conn;
	struct voluta_fuseq_pipe fq_pipe;
	struct voluta_socket     fq_sock;
	int fq_fuse_fd;
	int fq_status;
	int fq_got_init;
	int fq_got_destroy;
	int fq_deny_others;
	int fq_active;
};


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* mntclnt */
void voluta_mntclnt_init(struct voluta_socket *sock);

void voluta_mntclnt_fini(struct voluta_socket *sock);

int voluta_mntclnt_connect(struct voluta_socket *sock);

int voluta_mntclnt_disconnect(struct voluta_socket *sock);

int voluta_mntclnt_mount(struct voluta_socket *sock,
			 const struct voluta_mntparams *mntp,
			 int *out_status, int *out_fd);

int voluta_mntclnt_umount(struct voluta_socket *sock, int *out_status);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* opers */
int voluta_fs_forget(struct voluta_sb_info *sbi,
		     const struct voluta_oper *op, ino_t ino, size_t nlookup);

int voluta_fs_statfs(struct voluta_sb_info *sbi,
		     const struct voluta_oper *op, ino_t ino,
		     struct statvfs *stvfs);

int voluta_fs_lookup(struct voluta_sb_info *sbi,
		     const struct voluta_oper *op, ino_t parent,
		     const char *name, struct stat *out_stat);

int voluta_fs_getattr(struct voluta_sb_info *sbi,
		      const struct voluta_oper *op,
		      ino_t ino, struct stat *out_stat);

int voluta_fs_mkdir(struct voluta_sb_info *sbi,
		    const struct voluta_oper *op, ino_t parent,
		    const char *name, mode_t mode, struct stat *out_stat);

int voluta_fs_rmdir(struct voluta_sb_info *sbi,
		    const struct voluta_oper *op,
		    ino_t parent, const char *name);

int voluta_fs_access(struct voluta_sb_info *sbi,
		     const struct voluta_oper *op, ino_t ino, int mode);

int voluta_fs_chmod(struct voluta_sb_info *sbi,
		    const struct voluta_oper *op, ino_t ino, mode_t mode,
		    const struct stat *st, struct stat *out_stat);

int voluta_fs_chown(struct voluta_sb_info *sbi,
		    const struct voluta_oper *op, ino_t ino, uid_t uid,
		    gid_t gid, const struct stat *st, struct stat *out_stat);

int voluta_fs_truncate(struct voluta_sb_info *sbi,
		       const struct voluta_oper *op, ino_t ino, loff_t len,
		       struct stat *out_stat);

int voluta_fs_utimens(struct voluta_sb_info *sbi,
		      const struct voluta_oper *op, ino_t ino,
		      const struct stat *times, struct stat *out_stat);

int voluta_fs_symlink(struct voluta_sb_info *sbi,
		      const struct voluta_oper *op, ino_t parent,
		      const char *name, const char *symval,
		      struct stat *out_stat);

int voluta_fs_readlink(struct voluta_sb_info *sbi,
		       const struct voluta_oper *op,
		       ino_t ino, char *ptr, size_t lim);

int voluta_fs_unlink(struct voluta_sb_info *sbi,
		     const struct voluta_oper *op,
		     ino_t parent, const char *name);

int voluta_fs_link(struct voluta_sb_info *sbi,
		   const struct voluta_oper *op, ino_t ino, ino_t parent,
		   const char *name, struct stat *out_stat);

int voluta_fs_rename(struct voluta_sb_info *sbi,
		     const struct voluta_oper *op, ino_t parent,
		     const char *name, ino_t newparent,
		     const char *newname, int flags);

int voluta_fs_opendir(struct voluta_sb_info *sbi,
		      const struct voluta_oper *op, ino_t ino);

int voluta_fs_releasedir(struct voluta_sb_info *sbi,
			 const struct voluta_oper *op, ino_t ino, int o_flags);

int voluta_fs_readdir(struct voluta_sb_info *sbi,
		      const struct voluta_oper *op, ino_t ino,
		      struct voluta_readdir_ctx *rd_ctx);

int voluta_fs_readdirplus(struct voluta_sb_info *sbi,
			  const struct voluta_oper *op, ino_t ino,
			  struct voluta_readdir_ctx *rd_ctx);

int voluta_fs_fsyncdir(struct voluta_sb_info *sbi,
		       const struct voluta_oper *op, ino_t ino, bool datasync);

int voluta_fs_create(struct voluta_sb_info *sbi,
		     const struct voluta_oper *op, ino_t parent,
		     const char *name, int o_flags, mode_t mode,
		     struct stat *out_stat);

int voluta_fs_open(struct voluta_sb_info *sbi,
		   const struct voluta_oper *op, ino_t ino, int o_flags);

int voluta_fs_mknod(struct voluta_sb_info *sbi, const struct voluta_oper *op,
		    ino_t parent, const char *name, mode_t mode, dev_t rdev,
		    struct stat *out_stat);

int voluta_fs_release(struct voluta_sb_info *sbi,
		      const struct voluta_oper *op,
		      ino_t ino, int o_flags, bool flush);

int voluta_fs_flush(struct voluta_sb_info *sbi,
		    const struct voluta_oper *op, ino_t ino);

int voluta_fs_fsync(struct voluta_sb_info *sbi,
		    const struct voluta_oper *op,
		    ino_t ino, bool datasync);

int voluta_fs_getxattr(struct voluta_sb_info *sbi,
		       const struct voluta_oper *op, ino_t ino,
		       const char *name, void *buf, size_t size,
		       size_t *out_size);

int voluta_fs_setxattr(struct voluta_sb_info *sbi,
		       const struct voluta_oper *op, ino_t ino,
		       const char *name, const void *value,
		       size_t size, int flags);

int voluta_fs_listxattr(struct voluta_sb_info *sbi,
			const struct voluta_oper *op, ino_t ino,
			struct voluta_listxattr_ctx *lxa_ctx);

int voluta_fs_removexattr(struct voluta_sb_info *sbi,
			  const struct voluta_oper *op,
			  ino_t ino, const char *name);

int voluta_fs_fallocate(struct voluta_sb_info *sbi,
			const struct voluta_oper *op, ino_t ino,
			int mode, loff_t offset, loff_t length);

int voluta_fs_lseek(struct voluta_sb_info *sbi,
		    const struct voluta_oper *op, ino_t ino,
		    loff_t off, int whence, loff_t *out_off);

int voluta_fs_read(struct voluta_sb_info *sbi,
		   const struct voluta_oper *op, ino_t ino, void *buf,
		   size_t len, loff_t off, size_t *out_len);

int voluta_fs_read_iter(struct voluta_sb_info *sbi,
			const struct voluta_oper *op, ino_t ino,
			struct voluta_rw_iter *rwi);

int voluta_fs_write(struct voluta_sb_info *sbi,
		    const struct voluta_oper *op, ino_t ino,
		    const void *buf, size_t len, off_t off, size_t *out_len);

int voluta_fs_write_iter(struct voluta_sb_info *sbi,
			 const struct voluta_oper *op, ino_t ino,
			 struct voluta_rw_iter *rwi);

int voluta_fs_statx(struct voluta_sb_info *sbi,
		    const struct voluta_oper *op, ino_t ino,
		    struct statx *out_stx);

int voluta_fs_fiemap(struct voluta_sb_info *sbi,
		     const struct voluta_oper *op, ino_t ino,
		     struct fiemap *fm);

int voluta_fs_query(struct voluta_sb_info *sbi,
		    const struct voluta_oper *op, ino_t ino,
		    struct voluta_query *out_qry);


#endif /* VOLUTA_FUSE_H_ */




