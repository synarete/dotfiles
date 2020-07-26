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
#ifndef VOLUTA_CORE_H_
#define VOLUTA_CORE_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <pthread.h>
#include <time.h>
#include <gcrypt.h>
#include <liburing.h>
#include <iconv.h>
#include <uuid/uuid.h>
#include "voluta.h"
#include "macros.h"
#include "types.h"
#include "avl.h"
#include "list.h"
#include "logging.h"


/* XXX remove */
struct voluta_fuseq;


struct voluta_itimes {
	struct timespec btime;
	struct timespec atime;
	struct timespec mtime;
	struct timespec ctime;
};

struct voluta_iattr {
	enum voluta_iattr_flags ia_flags;
	mode_t          ia_mode;
	ino_t           ia_ino;
	ino_t           ia_parent;
	nlink_t         ia_nlink;
	uid_t           ia_uid;
	gid_t           ia_gid;
	dev_t           ia_rdev;
	loff_t          ia_size;
	blksize_t       ia_blksize;
	blkcnt_t        ia_blocks;
	struct voluta_itimes ia_t;
};

/* Logical-address within storage space */
struct voluta_vaddr {
	enum voluta_vtype vtype;
	unsigned int len;
	loff_t off;
	loff_t lba;
};

/* Inode-address */
struct voluta_iaddr {
	struct voluta_vaddr vaddr;
	ino_t ino;
};


/* Pool-based allocator */
struct voluta_mpool {
	struct voluta_qalloc *mp_qal;
	struct voluta_listq mp_bq;
	struct voluta_listq mp_vq;
	struct voluta_listq mp_iq;
};

/* Caching */
struct voluta_dirtyq {
	struct voluta_listq lq;
};

struct voluta_cache_elem {
	struct voluta_list_head ce_htb_lh;
	struct voluta_list_head ce_lru_lh;
	size_t ce_refcnt;
	long   ce_key;

};

struct voluta_seg_info {
	struct voluta_cache_elem seg_ce;
	struct voluta_segment *seg;
	loff_t   seg_lba;
	uint64_t seg_mask;
};

union voluta_vnode_u {
	struct voluta_data_block        *db;
	struct voluta_super_block       *sb;
	struct voluta_uspace_map        *usm;
	struct voluta_agroup_map        *agm;
	struct voluta_itable_tnode      *itn;
	struct voluta_inode             *inode;
	struct voluta_radix_tnode       *rtn;
	struct voluta_dir_htree_node    *htn;
	struct voluta_xattr_node        *xan;
	struct voluta_lnk_value         *lnv;
	void *p;
};

struct voluta_vnode_info {
	struct voluta_vaddr vaddr;
	struct voluta_cache_elem  v_ce;
	struct voluta_list_head   v_dq_lh;
	struct voluta_avl_node    v_dt_an;
	struct voluta_sb_info    *v_sbi;
	struct voluta_seg_info   *v_sgi;
	struct voluta_vnode_info *v_pvi;
	struct voluta_inode_info *v_pii;
	struct voluta_view *view;
	union voluta_vnode_u u;
	int v_dirty;
	int v_verify;
};

struct voluta_inode_info {
	struct voluta_inode *inode;
	struct voluta_vnode_info i_vi;
	struct voluta_dirtyq i_vdq;
	struct timespec i_atime_lazy;
	ino_t  i_ino;
	int    i_nsubs;
	int    i_nopen;
};

typedef long (*voulta_lhash_fn)(long);

struct voluta_lrumap {
	struct voluta_list_head lru;
	struct voluta_list_head *htbl;
	voulta_lhash_fn hash_fn;
	size_t hsize;
	size_t count;
};

struct voluta_cache {
	struct voluta_mpool    *c_mpool;
	struct voluta_qalloc   *c_qalloc;
	struct voluta_lrumap    c_slm;
	struct voluta_lrumap    c_ilm;
	struct voluta_lrumap    c_vlm;
	struct voluta_dirtyq    c_vdq;
	struct voluta_segment  *c_nil_seg;
	struct voluta_segment  *c_aux_seg;
};

struct voluta_space_stat {
	ssize_t ndata;
	ssize_t nmeta;
	ssize_t nfiles;
};

struct voluta_space_info {
	loff_t  sp_size;
	size_t  sp_usp_count;
	size_t  sp_usp_index_lo;
	size_t  sp_ag_count;
	ssize_t sp_used_meta;
	ssize_t sp_used_data;
	ssize_t sp_nfiles;
};

/* Persistent-storage I/O-control */
struct voluta_pstore {
	int    ps_fd;
	int    ps_flags;
	loff_t ps_size;
	size_t ps_usend;
	size_t ps_urecv;
	struct io_uring ps_uring;
};

struct voluta_itable {
	struct voluta_vaddr it_root_vaddr;
	ino_t  it_ino_rootd;
	ino_t  it_ino_apex;
	size_t it_ninodes;
	size_t it_ninodes_max;
};

struct voluta_ino_set {
	size_t cnt;
	ino_t ino[VOLUTA_ITNODE_NENTS];
};

struct voluta_oper_stat {
	time_t op_time;
	size_t op_count;
	size_t op_count_mark;
	/* TODO: Have counter per-operation */
};


/* Cryptographic interface with libgcrypt */
struct voluta_crypto {
	gcry_md_hd_t     md_hd;
	gcry_cipher_hd_t chiper_hd;
};

struct voluta_sb_info {
	struct voluta_iv_key  s_iv_key;
	struct voluta_uuid    s_fs_uuid;
	struct voluta_ucred   s_owner;
	struct voluta_qalloc *s_qalloc;
	struct voluta_cache  *s_cache;
	struct voluta_crypto *s_crypto;
	struct voluta_pstore *s_pstore;
	struct voluta_vnode_info *s_sb_vi;
	struct voluta_space_info s_spi;
	struct voluta_itable s_itable;
	struct voluta_oper_stat s_opstat;
	size_t s_iopen_limit;
	size_t s_iopen;
	iconv_t s_namei_iconv;
};

struct voluta_dirtyset {
	struct voluta_avl ds_avl;
	struct voluta_sb_info *ds_sbi;
	struct voluta_inode_info *ds_ii;
};

/* FUSE interface */
struct voluta_fusei {
	unsigned long magic;
	struct voluta_fs_env *fs_env;
	struct voluta_qalloc *qal;
	char *mount_point;
	char *mount_options;
	struct fuse_req *req;
	unsigned int conn_caps;
	unsigned int pad;
	size_t page_size;
	struct voluta_oper oper;
	struct voluta_ucred *ucred;
	struct fuse_session *session;
	union voluta_fusei_u *u;
	int err;
	bool session_loop_active;
	bool blkdev_mode;
	char pad2[2];
};


/* File-system environment context */
struct voluta_fs_env {
	struct voluta_params    params;
	struct voluta_qalloc    qalloc;
	struct voluta_mpool     mpool;
	struct voluta_cache    *cache;
	struct voluta_crypto   *crypto;
	struct voluta_pstore   *pstore;
	struct voluta_sb_info  *sbi;
	struct voluta_fusei    *fusei;
	struct voluta_fuseq    *fuseq;
	const char *volume_path;
	loff_t volume_size;
	int signum;
	int pad2[13];
};


enum voluta_flags {
	VOLUTA_F_SYNC   = 0x0001,
	VOLUTA_F_NOW    = 0x0002,
	VOLUTA_F_BLKDEV = 0x0004,
	VOLUTA_F_URING  = 0x0008,
};

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* super */
int voluta_sbi_init(struct voluta_sb_info *sbi,
		    struct voluta_cache *cache,
		    struct voluta_crypto *crypto,
		    struct voluta_pstore *pstore);

void voluta_sbi_fini(struct voluta_sb_info *sbi);

int voluta_sbi_reinit(struct voluta_sb_info *sbi);

void voluta_sbi_setowner(struct voluta_sb_info *sbi,
			 const struct voluta_ucred *cred);

bool voluta_vaddr_isnull(const struct voluta_vaddr *vaddr);

bool voluta_vaddr_isdata(const struct voluta_vaddr *vaddr);

void voluta_vaddr_reset(struct voluta_vaddr *vaddr);

void voluta_vaddr_clone(const struct voluta_vaddr *vaddr,
			struct voluta_vaddr *other);

void voluta_vaddr_by_off(struct voluta_vaddr *vaddr,
			 enum voluta_vtype vtype, loff_t off);


void voluta_clear_unwritten(const struct voluta_vnode_info *vi);

int voluta_prepare_space(struct voluta_sb_info *sbi,
			 const char *volume_path, loff_t size);

int voluta_format_super(struct voluta_sb_info *sbi, const char *name);

int voluta_bind_itable(struct voluta_sb_info *sbi,
		       struct voluta_vnode_info *it_root);

int voluta_format_usmaps(struct voluta_sb_info *sbi);

int voluta_format_agmaps(struct voluta_sb_info *sbi);

int voluta_format_itable(struct voluta_sb_info *sbi);

int voluta_load_super(struct voluta_sb_info *sbi);

int voluta_load_usmaps(struct voluta_sb_info *sbi);

int voluta_load_agmaps(struct voluta_sb_info *sbi);

int voluta_load_itable(struct voluta_sb_info *sbi);

void voluta_statvfs_of(const struct voluta_sb_info *sbi,
		       struct statvfs *out_stvfs);

int voluta_commit_dirty(struct voluta_sb_info *sbi, int flags);

int voluta_commit_dirty_of(struct voluta_inode_info *ii, int flags);

int voluta_commit_dirty_now(struct voluta_sb_info *sbi);

int voluta_drop_caches(struct voluta_sb_info *sbi);

int voluta_shut_super(struct voluta_sb_info *sbi);

int voluta_stage_inode(struct voluta_sb_info *sbi, ino_t xino,
		       struct voluta_inode_info **out_ii);

int voluta_stage_vnode(struct voluta_sb_info *sbi,
		       const struct voluta_vaddr *vaddr,
		       struct voluta_inode_info *pii,
		       struct voluta_vnode_info **out_vi);

int voluta_stage_data(struct voluta_sb_info *sbi,
		      const struct voluta_vaddr *vaddr,
		      struct voluta_inode_info *pii,
		      struct voluta_vnode_info **out_vi);

int voluta_new_inode(struct voluta_sb_info *sbi,
		     const struct voluta_oper *op,
		     mode_t mode, ino_t parent,
		     struct voluta_inode_info **out_ii);

int voluta_del_inode(struct voluta_sb_info *sbi, struct voluta_inode_info *ii);

int voluta_new_vnode(struct voluta_sb_info *sbi,
		     struct voluta_inode_info *pii,
		     enum voluta_vtype vtype,
		     struct voluta_vnode_info **out_vi);

int voluta_del_vnode(struct voluta_sb_info *sbi, struct voluta_vnode_info *vi);

int voluta_del_vnode_at(struct voluta_sb_info *sbi,
			const struct voluta_vaddr *vaddr);

int voluta_alloc_vspace(struct voluta_sb_info *sbi, enum voluta_vtype vtype,
			struct voluta_vaddr *out_vaddr);

void voluta_iv_key_of(const struct voluta_vnode_info *vi,
		      struct voluta_iv_key *out_iv_key);

int voluta_verify_super_block(const struct voluta_super_block *sb);

int voluta_verify_uspace_map(const struct voluta_uspace_map *usm);

int voluta_verify_agroup_map(const struct voluta_agroup_map *agm);

size_t voluta_nkbs_of(const struct voluta_vaddr *vaddr);


/* itable */
int voluta_init_itable(struct voluta_sb_info *sbi);

void voluta_fini_itable(struct voluta_sb_info *sbi);

int voluta_acquire_ino(struct voluta_sb_info *sbi, struct voluta_iaddr *iaddr);

int voluta_discard_ino(struct voluta_sb_info *sbi, ino_t ino);

int voluta_resolve_ino(struct voluta_sb_info *sbi,
		       ino_t ino, struct voluta_iaddr *iaddr);

int voluta_create_itable(struct voluta_sb_info *sbi);

void voluta_bind_root_ino(struct voluta_sb_info *sbi, ino_t ino);

int voluta_reload_itable(struct voluta_sb_info *sbi,
			 const struct voluta_vaddr *vaddr);

int voluta_parse_itable_top(struct voluta_sb_info *sbi,
			    struct voluta_ino_set *ino_set);

int voluta_real_ino(const struct voluta_sb_info *sbi,
		    ino_t ino, ino_t *out_ino);

int voluta_verify_itnode(const struct voluta_itable_tnode *itn, bool full);

/* namei */
int voluta_authorize(const struct voluta_sb_info *sbi,
		     const struct voluta_oper *op);

int voluta_name_to_str(const struct voluta_inode_info *ii,
		       const char *name, struct voluta_str *str);

int voluta_name_to_qstr(const struct voluta_inode_info *dir_ii,
			const char *name, struct voluta_qstr *qstr);

int voluta_do_forget_inode(struct voluta_sb_info *sbi,
			   ino_t xino, size_t nlookup);

int voluta_do_statvfs(const struct voluta_oper *op,
		      struct voluta_inode_info *ii, struct statvfs *out_stvfs);

int voluta_do_access(const struct voluta_oper *op,
		     const struct voluta_inode_info *ii, int mode);

int voluta_do_open(const struct voluta_oper *op,
		   struct voluta_inode_info *ii, int flags);

int voluta_do_release(const struct voluta_oper *op,
		      struct voluta_inode_info *ii);

int voluta_do_mkdir(const struct voluta_oper *op,
		    struct voluta_inode_info *dir_ii,
		    const struct voluta_qstr *name, mode_t mode,
		    struct voluta_inode_info **out_ii);

int voluta_do_rmdir(const struct voluta_oper *op,
		    struct voluta_inode_info *dir_ii,
		    const struct voluta_qstr *name);

int voluta_do_rename(const struct voluta_oper *op,
		     struct voluta_inode_info *dir_ii,
		     const struct voluta_qstr *name,
		     struct voluta_inode_info *newdir_ii,
		     const struct voluta_qstr *newname, int flags);

int voluta_do_symlink(const struct voluta_oper *op,
		      struct voluta_inode_info *dir_ii,
		      const struct voluta_qstr *name,
		      const struct voluta_str *linkpath,
		      struct voluta_inode_info **out_ii);

int voluta_do_link(const struct voluta_oper *op,
		   struct voluta_inode_info *dir_ii,
		   const struct voluta_qstr *name,
		   struct voluta_inode_info *ii);

int voluta_do_unlink(const struct voluta_oper *op,
		     struct voluta_inode_info *dir_ii,
		     const struct voluta_qstr *name);

int voluta_do_create(const struct voluta_oper *op,
		     struct voluta_inode_info *dir_ii,
		     const struct voluta_qstr *name, mode_t mode,
		     struct voluta_inode_info **out_ii);

int voluta_do_mknod(const struct voluta_oper *op,
		    struct voluta_inode_info *dir_ii,
		    const struct voluta_qstr *name, mode_t mode, dev_t dev,
		    struct voluta_inode_info **out_ii);

int voluta_do_lookup(const struct voluta_oper *op,
		     const struct voluta_inode_info *dir_ii,
		     const struct voluta_qstr *name,
		     struct voluta_inode_info **out_ii);

int voluta_do_opendir(const struct voluta_oper *op,
		      struct voluta_inode_info *dir_ii);

int voluta_do_releasedir(const struct voluta_oper *op,
			 struct voluta_inode_info *dir_ii);

int voluta_do_fsyncdir(const struct voluta_oper *op,
		       struct voluta_inode_info *dir_ii, bool dsync);

/* inode */
ino_t voluta_ino_of(const struct voluta_inode_info *ii);

ino_t voluta_parent_of(const struct voluta_inode_info *ii);

ino_t voluta_xino_of(const struct voluta_inode_info *ii);

uid_t voluta_uid_of(const struct voluta_inode_info *ii);

gid_t voluta_gid_of(const struct voluta_inode_info *ii);

mode_t voluta_mode_of(const struct voluta_inode_info *ii);

nlink_t voluta_nlink_of(const struct voluta_inode_info *ii);

loff_t voluta_isize_of(const struct voluta_inode_info *ii);

blkcnt_t voluta_blocks_of(const struct voluta_inode_info *ii);

bool voluta_isdir(const struct voluta_inode_info *ii);

bool voluta_isreg(const struct voluta_inode_info *ii);

bool voluta_isfifo(const struct voluta_inode_info *ii);

bool voluta_issock(const struct voluta_inode_info *ii);

bool voluta_islnk(const struct voluta_inode_info *ii);

bool voluta_isrootd(const struct voluta_inode_info *ii);

void voluta_fixup_rootd(struct voluta_inode_info *ii);

bool voluta_has_iname_utf8(const struct voluta_inode_info *ii);

bool voluta_inode_hasflag(const struct voluta_inode_info *ii,
			  enum voluta_inode_flags mask);

bool voluta_is_rootd(const struct voluta_inode_info *ii);

int voluta_do_getattr(const struct voluta_oper *op,
		      const struct voluta_inode_info *ii,
		      struct stat *out_st);

int voluta_do_statx(const struct voluta_oper *op,
		    const struct voluta_inode_info *ii,
		    struct statx *out_stx);

int voluta_do_query(const struct voluta_oper *op,
		    const struct voluta_inode_info *ii,
		    struct voluta_query *out_inq);

int voluta_do_chmod(const struct voluta_oper *op,
		    struct voluta_inode_info *ii, mode_t mode,
		    const struct voluta_itimes *itimes);

int voluta_do_chown(const struct voluta_oper *op,
		    struct voluta_inode_info *ii, uid_t uid, gid_t gid,
		    const struct voluta_itimes *itimes);

int voluta_do_utimens(const struct voluta_oper *op,
		      struct voluta_inode_info *ii,
		      const struct voluta_itimes *itimes);

int voluta_setup_ispecial(struct voluta_inode_info *ii, dev_t rdev);

int voluta_verify_inode(const struct voluta_inode *inode);

void voluta_update_itimes(const struct voluta_oper *op,
			  struct voluta_inode_info *ii,
			  enum voluta_iattr_flags attr_flags);

void voluta_update_iattrs(const struct voluta_oper *op,
			  struct voluta_inode_info *ii,
			  const struct voluta_iattr *attr);

void voluta_iattr_setup(struct voluta_iattr *iattr, ino_t ino);

void voluta_refresh_atime(struct voluta_inode_info *ii, bool to_volatile);


void voluta_setup_new_inode(struct voluta_inode_info *ii,
			    const struct voluta_oper *op,
			    mode_t mode, ino_t parent);

void voluta_stat_of(const struct voluta_inode_info *ii, struct stat *st);

/* dir */
size_t voluta_dir_ndentries(const struct voluta_inode_info *dir_ii);

bool voluta_dir_hasflag(const struct voluta_inode_info *dir_ii,
			enum voluta_dir_flags mask);

int voluta_verify_dir_inode(const struct voluta_inode *inode);

int voluta_verify_dir_htree_node(const struct voluta_dir_htree_node *htn);

void voluta_setup_new_dir(struct voluta_inode_info *dir_ii);

int voluta_lookup_dentry(const struct voluta_oper *op,
			 const struct voluta_inode_info *dir_ii,
			 const struct voluta_qstr *name,
			 struct voluta_ino_dt *out_idt);

int voluta_add_dentry(const struct voluta_oper *op,
		      struct voluta_inode_info *dir_ii,
		      const struct voluta_qstr *name,
		      struct voluta_inode_info *ii);

int voluta_remove_dentry(const struct voluta_oper *op,
			 struct voluta_inode_info *dir_ii,
			 const struct voluta_qstr *name);

int voluta_do_readdir(const struct voluta_oper *op,
		      struct voluta_inode_info *dir_ii,
		      struct voluta_readdir_ctx *rd_ctx);

int voluta_do_readdirplus(const struct voluta_oper *op,
			  struct voluta_inode_info *dir_ii,
			  struct voluta_readdir_ctx *rd_ctx);

int voluta_drop_dir(struct voluta_inode_info *dir_ii);

/* file */
void voluta_setup_new_reg(struct voluta_inode_info *ii);

int voluta_drop_reg(struct voluta_inode_info *ii);

int voluta_do_write(const struct voluta_oper *op,
		    struct voluta_inode_info *ii,
		    const void *buf, size_t len,
		    loff_t off, size_t *out_len);

int voluta_do_write_iter(const struct voluta_oper *op,
			 struct voluta_inode_info *ii,
			 struct voluta_rw_iter *rwi);


int voluta_do_read_iter(const struct voluta_oper *op,
			struct voluta_inode_info *ii,
			struct voluta_rw_iter *rwi);

int voluta_do_read(const struct voluta_oper *op,
		   struct voluta_inode_info *ii,
		   void *buf, size_t len, loff_t off, size_t *out_len);

int voluta_do_lseek(const struct voluta_oper *op,
		    struct voluta_inode_info *ii,
		    loff_t off, int whence, loff_t *out_off);

int voluta_do_fallocate(const struct voluta_oper *op,
			struct voluta_inode_info *ii,
			int mode, loff_t off, loff_t length);

int voluta_do_truncate(const struct voluta_oper *op,
		       struct voluta_inode_info *ii, loff_t off);

int voluta_do_fsync(const struct voluta_oper *op,
		    struct voluta_inode_info *ii, bool datasync);

int voluta_do_flush(const struct voluta_oper *op,
		    struct voluta_inode_info *ii);

int voluta_do_fiemap(const struct voluta_oper *op,
		     struct voluta_inode_info *ii, struct fiemap *fm);

int voluta_verify_radix_tnode(const struct voluta_radix_tnode *rtn);

/* symlink */
void voluta_setup_new_symlnk(struct voluta_inode_info *lnk_ii);

int voluta_drop_symlink(struct voluta_inode_info *lnk_ii);

int voluta_do_readlink(const struct voluta_oper *op,
		       struct voluta_inode_info *lnk_ii,
		       struct voluta_buf *buf);

int voluta_setup_symlink(const struct voluta_oper *op,
			 struct voluta_inode_info *lnk_ii,
			 const struct voluta_str *symval);

int voluta_verify_lnk_value(const struct voluta_lnk_value *lnv);

/* xattr */
void voluta_setup_inode_xattr(struct voluta_inode_info *ii);

int voluta_do_getxattr(const struct voluta_oper *op,
		       struct voluta_inode_info *ii,
		       const struct voluta_str *name,
		       void *buf, size_t size, size_t *out_size);

int voluta_do_setxattr(const struct voluta_oper *op,
		       struct voluta_inode_info *ii,
		       const struct voluta_str *name,
		       const void *value, size_t size, int flags);

int voluta_do_removexattr(const struct voluta_oper *op,
			  struct voluta_inode_info *ii,
			  const struct voluta_str *name);

int voluta_do_listxattr(const struct voluta_oper *op,
			struct voluta_inode_info *ii,
			struct voluta_listxattr_ctx *lsxa);

int voluta_drop_xattr(struct voluta_inode_info *ii);

int voluta_verify_inode_xattr(const struct voluta_inode *inode);

int voluta_verify_xattr_node(const struct voluta_xattr_node *xan);


/* volume */
int voluta_pstore_init(struct voluta_pstore *pstore);

void voluta_pstore_fini(struct voluta_pstore *pstore);

int voluta_pstore_create(struct voluta_pstore *pstore,
			 const char *path, loff_t size);

int voluta_pstore_open(struct voluta_pstore *pstore, const char *path);

void voluta_pstore_close(struct voluta_pstore *pstore);

int voluta_pstore_read(const struct voluta_pstore *pstore,
		       loff_t off, size_t bsz, void *buf);

int voluta_pstore_write(const struct voluta_pstore *pstore,
			loff_t off, size_t bsz, const void *buf);

int voluta_pstore_writev(const struct voluta_pstore *pstore, loff_t off,
			 size_t len, const struct iovec *iov, size_t cnt);

int voluta_pstore_flock(const struct voluta_pstore *pstore);

int voluta_pstore_funlock(const struct voluta_pstore *pstore);

bool voluta_pstore_exhuased(const struct voluta_pstore *pstore);


int voluta_flush_dirty(const struct voluta_sb_info *sbi,
		       const struct voluta_dirtyset *dset);

int voluta_collect_flushed(const struct voluta_sb_info *sbi, size_t count);

int voluta_sync_volume(const struct voluta_sb_info *sbi);

/* crypto */
int voluta_init_gcrypt(void);

void voluta_fill_random(void *buf, size_t len, bool very_strong);

void voluta_fill_random_ascii(char *str, size_t len);

int voluta_crypto_init(struct voluta_crypto *crypto);

void voluta_crypto_fini(struct voluta_crypto *crypto);

int voluta_derive_iv_key(const struct voluta_crypto *crypto,
			 const void *pass, const void *salt,
			 struct voluta_iv_key *iv_key);

int voluta_sha256_of(const struct voluta_crypto *crypto,
		     const void *buf, size_t bsz,
		     struct voluta_hash256 *out_hash);

int voluta_sha512_of(const struct voluta_crypto *crypto,
		     const void *buf, size_t bsz,
		     struct voluta_hash512 *out_hash);

int voluta_crc32_of(const struct voluta_crypto *crypto,
		    const void *buf, size_t bsz, uint32_t *out_crc32);

int voluta_encrypt_data(const struct voluta_crypto *crypto,
			const struct voluta_iv_key *iv_key,
			const void *in_dat, void *out_dat, size_t dat_len);

int voluta_decrypt_data(const struct voluta_crypto *crypto,
			const struct voluta_iv_key *iv_key,
			const void *in_dat, void *out_dat, size_t dat_len);


int voluta_decrypt_inplace(const struct voluta_vnode_info *vi);

int voluta_encrypt_intobuf(const struct voluta_vnode_info *vi,
			   void *buf, size_t bsz);

/* cache */
int voluta_cache_init(struct voluta_cache *cache, struct voluta_mpool *mpool);

void voluta_cache_fini(struct voluta_cache *cache);

void voluta_cache_relax(struct voluta_cache *cache, size_t cnt);

void voluta_cache_drop(struct voluta_cache *cache);

struct voluta_seg_info *
voluta_cache_lookup_seg(struct voluta_cache *cache, loff_t lba);

struct voluta_seg_info *
voluta_cache_spawn_seg(struct voluta_cache *cache, loff_t lba);

void voluta_cache_forget_seg(struct voluta_cache *cache,
			     struct voluta_seg_info *sgi);

void voluta_cache_try_forget_seg(struct voluta_cache *cache,
				 struct voluta_seg_info *sgi);

struct voluta_inode_info *
voluta_cache_spawn_ii(struct voluta_cache *cache,
		      const struct voluta_iaddr *iaddr);

void voulta_cache_forget_ii(struct voluta_cache *cache,
			    struct voluta_inode_info *ii);

struct voluta_inode_info *
voluta_cache_lookup_ii(struct voluta_cache *cache, ino_t ino);

struct voluta_vnode_info *
voluta_cache_lookup_vi(struct voluta_cache *cache,
		       const struct voluta_vaddr *vaddr);

struct voluta_vnode_info *
voluta_cache_spawn_vi(struct voluta_cache *cache,
		      const struct voluta_vaddr *vaddr);

void voulta_cache_forget_vi(struct voluta_cache *cache,
			    struct voluta_vnode_info *vi);

void voluta_dirtify_vi(struct voluta_vnode_info *vi);

void voluta_dirtify_ii(struct voluta_inode_info *ii);

void voluta_attach_to(struct voluta_vnode_info *vi,
		      struct voluta_seg_info *sgi,
		      struct voluta_vnode_info *pvi,
		      struct voluta_inode_info *pii);

void voluta_incref_of(struct voluta_vnode_info *vi);

void voluta_decref_of(struct voluta_vnode_info *vi);

size_t voluta_refcnt_of(const struct voluta_vnode_info *vi);

bool voluta_isevictable_ii(const struct voluta_inode_info *ii);


void voluta_mark_visible(const struct voluta_vnode_info *vi);

void voluta_mark_opaque_at(struct voluta_seg_info *sgi,
			   const struct voluta_vaddr *vaddr);

void voluta_mark_opaque(const struct voluta_vnode_info *vi);

bool voluta_is_visible(const struct voluta_vnode_info *vi);


void voluta_dirtyset_init(struct voluta_dirtyset *dset,
			  struct voluta_sb_info *sbi,
			  struct voluta_inode_info *ii);

void voluta_dirtyset_reset(struct voluta_dirtyset *dset);

struct voluta_vnode_info *
voluta_dirtyset_first(const struct voluta_dirtyset *dset);

struct voluta_vnode_info *
voluta_dirtyset_next(const struct voluta_dirtyset *dset,
		     const struct voluta_vnode_info *vi);

void voluta_dirtyset_inhabit(struct voluta_dirtyset *dset);

void voluta_dirtyset_purge(const struct voluta_dirtyset *dset);


/* persist */
void voluta_verify_persistent_format(void);

size_t voluta_persistent_size(enum voluta_vtype);

void voluta_stamp_view(struct voluta_view *view, enum voluta_vtype vtype);

int voluta_verify_meta(const struct voluta_vnode_info *vi);

uint32_t voluta_calc_chekcsum(const struct voluta_vnode_info *vi);

void voluta_seal_meta(const struct voluta_vnode_info *vi);

int voluta_verify_ino(ino_t ino);

int voluta_verify_off(loff_t off);

/* fusei */
int voluta_fusei_init(struct voluta_fusei *fusei,
		      struct voluta_fs_env *fs_env);

void voluta_fusei_fini(struct voluta_fusei *fusei);

int voluta_fusei_mount(struct voluta_fusei *fusei, const char *mntpoint);

int voluta_fusei_umount(struct voluta_fusei *fusei);

int voluta_fusei_session_loop(struct voluta_fusei *fusei);

void voluta_fusei_session_break(struct voluta_fusei *fusei);

/* fuseq */
int voluta_fuseq_init(struct voluta_fuseq *fq, struct voluta_sb_info *sbi);

void voluta_fuseq_fini(struct voluta_fuseq *fq);

int voluta_fuseq_mount(struct voluta_fuseq *fq, const char *mountpoint);

int voluta_fuseq_exec(struct voluta_fuseq *fq);


/* mpool */
void voluta_mpool_init(struct voluta_mpool *mpool, struct voluta_qalloc *qal);

void voluta_mpool_fini(struct voluta_mpool *mpool);

struct voluta_seg_info *voluta_malloc_sgi(struct voluta_mpool *mpool);

void voluta_free_sgi(struct voluta_mpool *mpool, struct voluta_seg_info *sgi);

struct voluta_vnode_info *voluta_malloc_vi(struct voluta_mpool *mpool);

void voluta_free_vi(struct voluta_mpool *mpool, struct voluta_vnode_info *vi);

struct voluta_inode_info *voluta_malloc_ii(struct voluta_mpool *mpool);

void voluta_free_ii(struct voluta_mpool *mpool, struct voluta_inode_info *ii);


/* thread */
int voluta_thread_sigblock_common(void);

int voluta_thread_create(struct voluta_thread *thread);

int voluta_thread_join(struct voluta_thread *thread);

int voluta_mutex_init(struct voluta_mutex *mutex);

void voluta_mutex_destroy(struct voluta_mutex *mutex);

void voluta_mutex_lock(struct voluta_mutex *mutex);

bool voluta_mutex_rylock(struct voluta_mutex *mutex);

void voluta_mutex_unlock(struct voluta_mutex *mutex);

/* utility */
void voluta_uuid_generate(struct voluta_uuid *uuid);

void voluta_uuid_clone(const struct voluta_uuid *u1, struct voluta_uuid *u2);

void voluta_ts_now(struct timespec *ts);

void voluta_ts_copy(struct timespec *dst, const struct timespec *src);

int voluta_ts_gettime(struct timespec *ts, bool realtime);


void voluta_buf_init(struct voluta_buf *buf, void *p, size_t n);

size_t voluta_buf_append(struct voluta_buf *buf, const void *ptr, size_t len);

void voluta_buf_seteos(struct voluta_buf *buf);

#endif /* VOLUTA_CORE_H_ */
