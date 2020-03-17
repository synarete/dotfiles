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
#ifndef VOLUTA_LIB_H_
#define VOLUTA_LIB_H_

#ifndef VOLUTA_LIB_MODULE
#error "internal voluta library header -- do not include!"
#endif

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
#include <time.h>
#include <gcrypt.h>
#include <pthread.h>
#include <iconv.h>
#include <uuid/uuid.h>
#include <voluta/voluta.h>


/* Threading */
struct voluta_thread;

typedef void (*voluta_execute_fn)(struct voluta_thread *);

struct voluta_thread {
	pthread_t thread;
	time_t start_time;
	time_t finish_time;
	voluta_execute_fn exec;
	void *arg;
	int status;
};

struct voluta_mutex {
	pthread_mutex_t mutex;
};

/* Hash */
struct voluta_iv_key {
	struct voluta_iv  iv;
	struct voluta_key key;
} voluta_aligned;

struct voluta_hash256 {
	uint8_t hash[VOLUTA_HASH256_LEN];
} voluta_aligned;

struct voluta_hash512 {
	uint8_t hash[VOLUTA_HASH512_LEN];
} voluta_aligned;

/*
 * Cryptographic operations context
 */
struct voluta_crypto {
	gcry_md_hd_t     md_hd;
	gcry_cipher_hd_t chiper_hd;
} voluta_aligned64;

struct voluta_str {
	const char *str;
	size_t len;
};

struct voluta_qstr {
	struct voluta_str str;
	uint64_t hash;
};

struct voluta_buf {
	void  *buf;
	size_t len;
	size_t bsz;
};

struct voluta_xtime {
	time_t start;
	struct timespec now;
};

struct voluta_ino_dt {
	ino_t  ino;
	mode_t dt;
};

struct voluta_passphrase {
	char pass[VOLUTA_PASSPHRASE_MAX + 1];
	char salt[VOLUTA_PASSPHRASE_MAX + 1];
};

struct voluta_iattr {
	enum voluta_attr_flags ia_flags;
	ino_t           ia_ino;
	ino_t           ia_parent_ino;
	mode_t          ia_mode;
	nlink_t         ia_nlink;
	uid_t           ia_uid;
	gid_t           ia_gid;
	dev_t           ia_rdev;
	loff_t          ia_size;
	blksize_t       ia_blksize;
	blkcnt_t        ia_blocks;
	struct timespec ia_btime;
	struct timespec ia_atime;
	struct timespec ia_mtime;
	struct timespec ia_ctime;
};

/* Logical-address within storage space */
struct voluta_vaddr {
	size_t ag_index;        /* Uber-group index */
	loff_t off;             /* Bytes-offset within logical space */
	loff_t lba;             /* Corresponding logical block */
	size_t len;             /* Size in bytes ("on-disk" size, optional) */
	enum voluta_vtype vtype;/* Referenced object type */
};

/* Inode-address */
struct voluta_iaddr {
	struct voluta_vaddr vaddr;
	ino_t ino;
};

/* Quick memory allocator */
struct voluta_slab {
	struct voluta_list_head free_list;   /* Element's free-list */
	size_t sindex;  /* Index in parent allocator */
	size_t elemsz;  /* Size of single slab-element */
	size_t nfree;   /* Current number of elements in free-list */
	size_t nused;   /* Current number of allocated elements used by user */
	size_t pad_[2];
} voluta_aligned;

struct voluta_qalloc {
	int memfd_data;         /* Data-memory file-descriptor */
	int memfd_meta;         /* Meta-memory file-descriptor */
	void *mem_data;         /* Beginning of memory-mapped data file */
	void *mem_meta;         /* Beginning of meory-mapped meta-data file */
	struct voluta_qmstat st;/* Stat accountings */
	size_t ncycles;         /* Running-index of operations-cycles */
	struct voluta_list_head free_list;   /* Free-pages list */
	struct voluta_slab slabs[8]; /* Slab-lists */
	bool pedantic_mode;     /* Should trash memory upon free? */
} voluta_aligned;

/* Caching */
struct voluta_dirtyq {
	struct voluta_list_head dq;
	size_t sz;
};

struct voluta_dirty_set {
	struct voluta_vnode_info *vi[VOLUTA_NKB_IN_BK];
	size_t cnt;
};

struct voluta_cache_elem {
	struct voluta_list_head ce_htb_lh;      /* Hash-map link */
	struct voluta_list_head ce_lru_lh;      /* LRU link */
	struct voluta_list_head ce_drq_lh;      /* Dirty-queue link */
	uint64_t ce_key;                        /* 64-bits caching key */
	size_t  ce_cycle;                       /* Id of last in-use cycle */
	size_t  ce_refcnt;                      /* References by other */
	int     ce_dirty;                       /* Is in dirty-queue? */
	int     ce_magic;
};

struct voluta_bk_info {
	struct voluta_cache_elem b_ce;          /* Cached element */
	struct voluta_dirtyq b_dq;              /* This block dirty vis */
	union voluta_bk *bk;                    /* Actual block's data */
	struct voluta_sb_info *b_sbi;           /* Top-level super-block */
	struct voluta_vnode_info *b_agm;        /* Allocation group */
	loff_t  b_lba;                          /* Logical block address */
	int     b_staged;                       /* Is fully staged ? */
} voluta_aligned64;

union voluta_vnode_u {
	struct voluta_agroup_map        *agm;
	struct voluta_itable_tnode      *itn;
	struct voluta_radix_tnode       *rtn;
	struct voluta_dir_htree_node    *htn;
	struct voluta_xattr_node        *xan;
	struct voluta_lnk_value         *lnv;
	struct voluta_data_seg          *ds;
	void *p;
};

struct voluta_vnode_info {
	struct voluta_vaddr vaddr;      /* Persistent logical address */
	struct voluta_cache_elem ce;    /* Caching base-element */
	struct voluta_bk_info *bki;     /* Containing block */
	union voluta_view *view;        /* "View" into "on-disk" element */
	union voluta_vnode_u u;

} voluta_aligned;

struct voluta_inode_info {
	struct voluta_vnode_info vi;    /* Base logical-node info */
	struct voluta_inode *inode;     /* Referenced inode */
	ino_t ino;                      /* Unique ino number */
} voluta_aligned;

/* Persistent-storage I/O-control */
struct voluta_pstore {
	loff_t size;                    /* Size in bytes (volume's capacity) */
	int fd;                         /* I/O file descriptor */
	int flags;                      /* Control flags */
};

/* Cryptographic-storage I/O-control */
struct voluta_cstore {
	struct voluta_pstore pstore;
	const struct voluta_crypto *crypto;
	struct voluta_qalloc *qal;
	union voluta_bk *enc_bk;        /* On-the-fly encryption block */
};

typedef uint64_t (*voulta_hash64_fn)(uint64_t);

struct voluta_cacheq {
	struct voluta_qalloc *qal;      /* Memory allocator */
	struct voluta_list_head lru;    /* LRU queue of elements */
	struct voluta_list_head *htbl;  /* Elements table */
	voulta_hash64_fn hash_fn;       /* Key re-hashing function */
	size_t hsize;                   /* Hash-table size */
	size_t count;                   /* Number currently mapped elements */
};

struct voluta_cache {
	struct voluta_qalloc *qal;
	struct voluta_cacheq bcq;
	struct voluta_cacheq icq;
	struct voluta_cacheq vcq;
	struct voluta_dirtyq dirtyq;
	union voluta_bk *null_bk;
};

struct voluta_sp_stat {
	ssize_t sp_nbytes;
	ssize_t sp_nmeta;
	ssize_t sp_ndata;
	ssize_t sp_nfiles;
};

struct voluta_ag_info {
	struct voluta_list_head lh;
	int  index;
	int  nfree;
	bool formatted;
	bool live;
	bool cap_alloc_bo;
};

struct voluta_ag_space {
	size_t ags_count;        /* Number of allocation groups */
	size_t ags_nlive;
	struct voluta_list_head spq;
	struct voluta_ag_info agi[VOLUTA_VOLUME_NAG_MAX]; /* TODO: dynamic */
};

struct voluta_itable {
	struct voluta_vaddr it_tree_vaddr; /* Tree-root address */
	ino_t  it_ino_rootd;            /* Root-directory ino number */
	ino_t  it_ino_apex;             /* Apex ino number */
	size_t it_ninodes;              /* Number of active inodes */
	size_t it_ninodes_max;          /* Limit on active inodes */
};

struct voluta_sb_info {
	uid_t  s_owner;                 /* Owner of current mount */
	time_t s_birth_time;            /* Birth time */
	iconv_t s_namei_iconv;          /* Names encoding converter */
	const char *s_name;             /* File system's name */
	struct voluta_uuid    s_fs_uuid;/* File system's UUID */
	struct voluta_qalloc  *s_qalloc;
	struct voluta_cache   *s_cache;
	struct voluta_cstore  *s_cstore;
	struct voluta_pstore  *s_pstore;
	struct voluta_super_block *sb;  /* Super-block */
	struct voluta_itable s_itable;  /* Inodes table */
	struct voluta_iv_key s_iv_key;     /* Master encryption IV/key */
	struct voluta_sp_stat s_sp_stat;   /* Space accounting */
	struct voluta_ag_space s_ag_space; /* Allocation-groups space-map */
};

/* FUSE interface */
struct voluta_fusei {
	unsigned long magic;            /* Debug magic-number */
	struct voluta_env *env;         /* Parent environment */
	struct voluta_qalloc *qal;    /* Memory allocator */
	char *mount_point;              /* Mount point path */
	char *mount_options;            /* Mount options string */
	struct fuse_req *req;           /* Current FUSE request handle */
	unsigned int conn_caps;         /* Connection capabilities */
	size_t page_size;               /* System's page-size */
	struct voluta_ucred *ucred;     /* Current caller credentials */
	struct fuse_session *session;   /* FUSE session context */
	union voluta_fusei_u *u;        /* Buffers union */
	int err;                        /* Last error code with FUSE layer */
	bool session_loop_active;       /* Is executing session loop? */
	bool blkdev_mode;               /* Is backed by raw block-device */
};

/* Current operation context */
struct voluta_oper_info {
	struct voluta_ucred ucred;      /* User-credentials */
	struct voluta_xtime xtime;      /* Execution time-stamps */
};

/* Main environment context object */
struct voluta_env {
	struct voluta_qalloc    qal;    /* Memory allocator */
	struct voluta_cache     cache;  /* Volatile elements cache */
	struct voluta_crypto    crypto; /* Cryptographic context */
	struct voluta_cstore    cstore; /* Cryptographic persistent storage */
	struct voluta_sb_info   sbi;    /* Ephemeral super-block info */
	struct voluta_oper_info opi;    /* Operation context */
	struct voluta_fusei     fusei;  /* Bridge via FUSE */
	const char *mount_point;        /* Local mount-point path */
	const char *volume_path;        /* Data-store device path */
	loff_t volume_size;             /* Size of back-end storage volume */
	int signum;                     /* Last caught signum number */
	bool tmpfs_mode;                /* Run in tmpfs mode */

} voluta_aligned64;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

enum voluta_flags {
	VOLUTA_F_SYNC   = 0x0001,
	VOLUTA_F_NOW    = 0x0002,
	VOLUTA_F_BLKDEV = 0x0004,
};

/* super */
int voluta_sbi_init(struct voluta_sb_info *sbi,
		    struct voluta_cache *cache,
		    struct voluta_cstore *cstore);

void voluta_sbi_fini(struct voluta_sb_info *sbi);

bool voluta_vaddr_isnull(const struct voluta_vaddr *vaddr);

void voluta_vaddr_reset(struct voluta_vaddr *vaddr);

void voluta_vaddr_clone(const struct voluta_vaddr *vaddr,
			struct voluta_vaddr *other);

void voluta_vaddr_by_off(struct voluta_vaddr *vaddr,
			 enum voluta_vtype vtype, loff_t off);

void voluta_clear_unwritten(const struct voluta_vnode_info *vi);

int voluta_prepare_space(struct voluta_sb_info *, const char *, loff_t);

int voluta_format_sb(struct voluta_sb_info *sbi);

int voluta_format_agmaps(struct voluta_sb_info *sbi);

int voluta_load_sb(struct voluta_sb_info *sbi);

int voluta_load_agmaps(struct voluta_sb_info *sbi);

int voluta_load_itable(struct voluta_sb_info *sbi);

void voluta_space_to_statvfs(const struct voluta_sb_info *, struct statvfs *);

int voluta_commit_dirtyq(struct voluta_sb_info *sbi, int flags);

int voluta_stage_inode(struct voluta_sb_info *sbi, ino_t xino,
		       struct voluta_inode_info **out_ii);

int voluta_stage_vnode(struct voluta_sb_info *sbi,
		       const struct voluta_vaddr *vaddr,
		       struct voluta_vnode_info **out_vi);

int voluta_new_inode(struct voluta_sb_info *sbi,
		     const struct voluta_oper_info *opi,
		     mode_t mode, ino_t parent_ino,
		     struct voluta_inode_info **out_ii);

int voluta_del_inode(struct voluta_sb_info *sbi, struct voluta_inode_info *ii);

int voluta_new_vnode(struct voluta_sb_info *sbi, enum voluta_vtype vtype,
		     struct voluta_vnode_info **out_vi);

int voluta_del_vnode(struct voluta_sb_info *sbi, struct voluta_vnode_info *vi);

int voluta_del_vnode_at(struct voluta_sb_info *sbi,
			const struct voluta_vaddr *vaddr);

int voluta_new_vspace(struct voluta_sb_info *sbi,
		      enum voluta_vtype vtype, struct voluta_vaddr *out_vaddr);

int voluta_verify_agroup_map(const struct voluta_agroup_map *agm);

int voluta_verify_super_block(const struct voluta_super_block *sb);


/* itable */
int voluta_init_itable(struct voluta_sb_info *sbi);

void voluta_fini_itable(struct voluta_sb_info *sbi);

int voluta_acquire_ino(struct voluta_sb_info *sbi, struct voluta_iaddr *iaddr);

int voluta_discard_ino(struct voluta_sb_info *sbi, ino_t ino);

int voluta_resolve_ino(struct voluta_sb_info *sbi,
		       ino_t ino, struct voluta_iaddr *iaddr);

int voluta_find_root_ino(struct voluta_sb_info *sbi,
			 struct voluta_iaddr *iaddr);

void voluta_bind_root_ino(struct voluta_sb_info *sbi, ino_t ino);

int voluta_reload_itable(struct voluta_sb_info *sbi,
			 const struct voluta_vaddr *vaddr);

int voluta_format_itable(struct voluta_sb_info *sbi);

int voluta_real_ino(const struct voluta_sb_info *sbi,
		    ino_t ino, ino_t *out_ino);

int voluta_verify_itnode(const struct voluta_itable_tnode *itnode);

/* namei */
int voluta_authorize(const struct voluta_env *);

int voluta_name_to_str(const struct voluta_sb_info *,
		       const struct voluta_inode_info *,
		       const char *, struct voluta_str *);

int voluta_name_to_qstr(const struct voluta_sb_info *,
			const struct voluta_inode_info *,
			const char *, struct voluta_qstr *);

int voluta_symval_to_str(const char *symval, struct voluta_str *str);

int voluta_do_statvfs(const struct voluta_sb_info *sbi, struct statvfs *stvfs);

int voluta_do_access(const struct voluta_oper_info *,
		     const struct voluta_inode_info *, int);

int voluta_do_open(struct voluta_env *, struct voluta_inode_info *, int);
int voluta_do_release(struct voluta_env *, struct voluta_inode_info *);
int voluta_do_mkdir(struct voluta_env *, struct voluta_inode_info *,
		    const struct voluta_qstr *, mode_t,
		    struct voluta_inode_info **);
int voluta_do_rmdir(struct voluta_env *, struct voluta_inode_info *,
		    const struct voluta_qstr *);
int voluta_do_rename(struct voluta_env *, struct voluta_inode_info *,
		     const struct voluta_qstr *, struct voluta_inode_info *,
		     const struct voluta_qstr *, int);
int voluta_do_symlink(struct voluta_env *, struct voluta_inode_info *,
		      const struct voluta_qstr *, const struct voluta_str *,
		      struct voluta_inode_info **);
int voluta_do_link(struct voluta_env *, struct voluta_inode_info *,
		   const struct voluta_qstr *, struct voluta_inode_info *);
int voluta_do_unlink(struct voluta_env *, struct voluta_inode_info *,
		     const struct voluta_qstr *);
int voluta_do_create(struct voluta_env *, struct voluta_inode_info *,
		     const struct voluta_qstr *, mode_t,
		     struct voluta_inode_info **);
int voluta_do_mknod(struct voluta_env *, struct voluta_inode_info *,
		    const struct voluta_qstr *, mode_t, dev_t,
		    struct voluta_inode_info **);

int voluta_inc_fileref(const struct voluta_oper_info *opi,
		       struct voluta_inode_info *ii);

int voluta_do_lookup(struct voluta_sb_info *sbi,
		     const struct voluta_oper_info *opi,
		     const struct voluta_inode_info *dir_ii,
		     const struct voluta_qstr *qstr,
		     struct voluta_inode_info **out_ii);

/* dir */
size_t voluta_dir_ndentries(const struct voluta_inode_info *dir_ii);

bool voluta_dir_hasflag(const struct voluta_inode_info *dir_ii,
			enum voluta_dir_flags mask);

int voluta_verify_dir_inode(const struct voluta_inode *inode);

int voluta_verify_dir_htree_node(const struct voluta_dir_htree_node *dtn);

void voluta_setup_new_dir(struct voluta_inode_info *dir_ii);

int voluta_check_add_dentry(const struct voluta_inode_info *dir_ii,
			    const struct voluta_qstr *name);

int voluta_resolve_by_name(struct voluta_sb_info *sbi,
			   const struct voluta_oper_info *opi,
			   const struct voluta_inode_info *dir_ii,
			   const struct voluta_qstr *name,
			   struct voluta_ino_dt *out_idt);

int voluta_do_opendir(struct voluta_sb_info *sbi,
		      const struct voluta_oper_info *opi,
		      struct voluta_inode_info *dir_ii);

int voluta_do_readdir(struct voluta_sb_info *sbi,
		      const struct voluta_oper_info *opi,
		      const struct voluta_inode_info *dir_ii,
		      struct voluta_readdir_ctx *readdir_ctx);

int voluta_add_dentry(struct voluta_sb_info *sbi,
		      const struct voluta_oper_info *opi,
		      struct voluta_inode_info *dir_ii,
		      const struct voluta_qstr *name,
		      struct voluta_inode_info *ii);

int voluta_remove_dentry(struct voluta_sb_info *sbi,
			 const struct voluta_oper_info *opi,
			 struct voluta_inode_info *dir_ii,
			 const struct voluta_qstr *name);

int voluta_drop_dir(struct voluta_sb_info *sbi,
		    struct voluta_inode_info *dir_ii);

int voluta_do_releasedir(struct voluta_sb_info *sbi,
			 const struct voluta_oper_info *opi,
			 struct voluta_inode_info *dir_ii);

int voluta_do_fsyncdir(struct voluta_sb_info *sbi,
		       const struct voluta_oper_info *opi,
		       struct voluta_inode_info *dir_ii, bool dsync);

/* file */
void voluta_setup_new_reg(struct voluta_inode_info *ii);

int voluta_drop_reg(struct voluta_sb_info *sbi, struct voluta_inode_info *ii);

int voluta_do_write(struct voluta_sb_info *sbi,
		    const struct voluta_oper_info *opi,
		    struct voluta_inode_info *ii,
		    const void *buf, size_t len,
		    loff_t off, size_t *out_len);

int voluta_do_write_iter(struct voluta_sb_info *sbi,
			 const struct voluta_oper_info *opi,
			 struct voluta_inode_info *ii,
			 struct voluta_rw_iter *rwi);

int voluta_do_read(struct voluta_env *, struct voluta_inode_info *,
		   void *buf, size_t len, loff_t offset, size_t *);
int voluta_do_read_iter(struct voluta_env *, struct voluta_inode_info *,
			struct voluta_rw_iter *);
int voluta_lseek(struct voluta_inode_info *,
		 loff_t offset, int whence, loff_t *out_off);
int voluta_do_fallocate(struct voluta_env *, struct voluta_inode_info *,
			int mode, loff_t offset, loff_t length);
int voluta_do_truncate(struct voluta_env *,
		       struct voluta_inode_info *, loff_t);
int voluta_do_fsync(struct voluta_env *, struct voluta_inode_info *, bool);
int voluta_do_flush(struct voluta_env *, struct voluta_inode_info *);

int voluta_verify_radix_tnode(const struct voluta_radix_tnode *rtn);

/* inode */
size_t voluta_inode_refcnt(const struct voluta_inode_info *ii);

ino_t voluta_ino_of(const struct voluta_inode_info *ii);

ino_t voluta_parent_ino_of(const struct voluta_inode_info *ii);

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

int voluta_do_getattr(struct voluta_env *,
		      const struct voluta_inode_info *, struct stat *);
int voluta_do_statx(struct voluta_env *env,
		    const struct voluta_inode_info *ii, struct statx *);

int voluta_do_chmod(struct voluta_sb_info *sbi,
		    const struct voluta_oper_info *opi,
		    struct voluta_inode_info *ii, mode_t mode,
		    const struct voluta_iattr *attr);

int voluta_do_chown(struct voluta_env *, struct voluta_inode_info *,
		    uid_t, gid_t, const struct voluta_iattr *);

int voluta_do_utimens(const struct voluta_oper_info *opi,
		      struct voluta_inode_info *ii,
		      const struct voluta_iattr *attr);

int voluta_do_evict_inode(struct voluta_sb_info *, ino_t);


int voluta_setup_new_inode(struct voluta_inode_info *ii,
			   const struct voluta_oper_info *opi,
			   mode_t mode, ino_t parent_ino);

int voluta_setup_ispecial(struct voluta_inode_info *ii, dev_t rdev);

int voluta_verify_inode(const struct voluta_inode *inode);

void voluta_update_itimes(struct voluta_inode_info *ii,
			  const struct voluta_oper_info *opi,
			  enum voluta_attr_flags attr_flags);

void voluta_update_iattr(struct voluta_inode_info *ii,
			 const struct voluta_oper_info *opi,
			 const struct voluta_iattr *attr);

void voluta_iattr_init(struct voluta_iattr *iattr,
		       const struct voluta_inode_info *ii);

/* symlink */
void voluta_setup_new_symlnk(struct voluta_inode_info *lnk_ii);

int voluta_drop_symlink(struct voluta_sb_info *sbi,
			struct voluta_inode_info *lnk_ii);

int voluta_do_readlink(struct voluta_sb_info *sbi,
		       const struct voluta_inode_info *lnk_ii,
		       struct voluta_buf *buf);

int voluta_setup_symlink(struct voluta_sb_info *sbi,
			 const struct voluta_oper_info *opi,
			 struct voluta_inode_info *lnk_ii,
			 const struct voluta_str *symval);

int voluta_verify_lnk_value(const struct voluta_lnk_value *lnv);

/* xattr */
void voluta_setup_inode_xattr(struct voluta_inode_info *ii);

int voluta_do_getxattr(struct voluta_sb_info *sbi,
		       const struct voluta_oper_info *opi,
		       struct voluta_inode_info *ii,
		       const struct voluta_str *name,
		       void *buf, size_t size, size_t *out_size);

int voluta_do_setxattr(struct voluta_sb_info *sbi,
		       const struct voluta_oper_info *opi,
		       struct voluta_inode_info *ii,
		       const struct voluta_str *name,
		       const char *value, size_t size, int flags);

int voluta_do_removexattr(struct voluta_sb_info *sbi,
			  const struct voluta_oper_info *opi,
			  struct voluta_inode_info *ii,
			  const struct voluta_str *name);

int voluta_do_listxattr(struct voluta_sb_info *sbi,
			const struct voluta_oper_info *opi,
			struct voluta_inode_info *ii,
			struct voluta_listxattr_ctx *lsxa);

int voluta_drop_xattr(struct voluta_sb_info *sbi,
		      const struct voluta_oper_info *opi,
		      struct voluta_inode_info *ii);

int voluta_verify_inode_xattr(const struct voluta_inode *inode);

int voluta_verify_xattr_node(const struct voluta_xattr_node *xan);


/* volume */
int voluta_pstore_init(struct voluta_pstore *pstore);

void voluta_pstore_fini(struct voluta_pstore *pstore);

int voluta_pstore_create(struct voluta_pstore *pstore,
			 const char *path, loff_t size);

int voluta_pstore_open(struct voluta_pstore *, const char *);
void voluta_pstore_close(struct voluta_pstore *);
int voluta_pstore_read(const struct voluta_pstore *, loff_t, size_t, void *);
int voluta_pstore_write(const struct voluta_pstore *,
			loff_t, size_t, const void *);
int voluta_pstore_load_dec(struct voluta_pstore *, loff_t,
			   const struct voluta_iv_key *, union voluta_bk *);
int voluta_pstore_enc_save(struct voluta_pstore *, loff_t,
			   const struct voluta_iv_key *,
			   const union voluta_bk *);
int voluta_pstore_sync(struct voluta_pstore *, bool datasync);
int voluta_pstore_flock(const struct voluta_pstore *);
int voluta_pstore_funlock(const struct voluta_pstore *);

int voluta_cstore_init(struct voluta_cstore *, struct voluta_qalloc *,
		       const struct voluta_crypto *);
void voluta_cstore_fini(struct voluta_cstore *);
int voluta_cstore_load_dec(struct voluta_cstore *, loff_t,
			   const struct voluta_iv_key *, union voluta_bk *);
int voluta_cstore_enc_save(struct voluta_cstore *, loff_t,
			   const struct voluta_iv_key *,
			   const union voluta_bk *);

/* crypto */
int voluta_init_gcrypt(void);

void voluta_fill_random_key(struct voluta_key *key);

void voluta_fill_random_iv(struct voluta_iv *iv);

void voluta_fill_random(void *, size_t, bool);
void voluta_fill_random_ascii(char *, size_t);
int voluta_derive_iv_key(const struct voluta_crypto *, const void *, size_t,
			 const void *, size_t, struct voluta_iv_key *);
int voluta_crypto_init(struct voluta_crypto *);
void voluta_crypto_fini(struct voluta_crypto *);
int voluta_sha256_of(const struct voluta_crypto *,
		     const void *, size_t, struct voluta_hash256 *);
int voluta_sha512_of(const struct voluta_crypto *,
		     const void *, size_t, struct voluta_hash512 *);
int voluta_crc32_of(const struct voluta_crypto *,
		    const void *, size_t, uint32_t *);
int voluta_encrypt_bk(const struct voluta_crypto *,
		      const struct voluta_iv *, const struct voluta_key *,
		      const union voluta_bk *, union voluta_bk *);
int voluta_decrypt_bk(const struct voluta_crypto *,
		      const struct voluta_iv *, const struct voluta_key *,
		      const union voluta_bk *, union voluta_bk *);

/* cache */
int voluta_cache_init(struct voluta_cache *cache, struct voluta_qalloc *qal);

void voluta_cache_fini(struct voluta_cache *cache);

void voluta_cache_step_cycles(struct voluta_cache *cache, size_t n);

void voluta_cache_next_cycle(struct voluta_cache *cache);

void voluta_cache_relax(struct voluta_cache *cache, bool force);

void voluta_cache_drop(struct voluta_cache *cache);

int voluta_cache_lookup_bk(struct voluta_cache *cache, loff_t lba,
			   struct voluta_bk_info **out_bki);

int voluta_cache_spawn_bk(struct voluta_cache *cache, loff_t lba,
			  struct voluta_vnode_info *agm_vi,
			  struct voluta_bk_info **out_bki);

void voluta_cache_forget_bk(struct voluta_cache *cache,
			    struct voluta_bk_info *bki);

int voluta_cache_spawn_ii(struct voluta_cache *cache,
			  const struct voluta_iaddr *iaddr,
			  struct voluta_inode_info **out_ii);

void voulta_cache_forget_ii(struct voluta_cache *cache,
			    struct voluta_inode_info *ii);

int voluta_cache_lookup_ii(struct voluta_cache *cache, ino_t ino,
			   struct voluta_inode_info **out_ii);

int voluta_cache_lookup_vi(struct voluta_cache *cache,
			   const struct voluta_vaddr *vaddr,
			   struct voluta_vnode_info **out_vi);

int voluta_cache_spawn_vi(struct voluta_cache *cache,
			  const struct voluta_vaddr *vaddr,
			  struct voluta_vnode_info **out_vi);

void voulta_cache_forget_vi(struct voluta_cache *cache,
			    struct voluta_vnode_info *vi);

int voluta_cache_get_dirty(const struct voluta_cache *cache,
			   struct voluta_bk_info **out_bki);

void voluta_dirtify_vi(struct voluta_vnode_info *vi);

void voluta_undirtify_vi(struct voluta_vnode_info *vi);

void voluta_dirtify_ii(struct voluta_inode_info *ii);

bool voluta_isevictable_ii(const struct voluta_inode_info *ii);

void voluta_attach_vi(struct voluta_vnode_info *vi,
		      struct voluta_bk_info *bki);

void voluta_mark_as_clean(struct voluta_bk_info *bki);

void voluta_dirty_set_of(const struct voluta_bk_info *bki,
			 struct voluta_dirty_set *dset);

/* persist */
void voluta_verify_persistent_format(void);

size_t voluta_persistent_size(enum voluta_vtype);

int voluta_verify_ino(ino_t ino);

int voluta_verify_off(loff_t off);

int voluta_verify_view(const union voluta_view *view,
		       const struct voluta_crypto *crypto,
		       enum voluta_vtype vtype);

void voluta_stamp_vnode(struct voluta_vnode_info *vi);

void voluta_stamp_meta(struct voluta_header *hdr,
		       enum voluta_vtype vtype, size_t size);

void voluta_seal_vnode(struct voluta_vnode_info *vi,
		       const struct voluta_crypto *crypto);

/* fusei */
int voluta_fusei_init(struct voluta_fusei *, struct voluta_env *);
void voluta_fusei_fini(struct voluta_fusei *);
int voluta_fusei_mount(struct voluta_fusei *, const char *mntpoint);
int voluta_fusei_umount(struct voluta_fusei *);
int voluta_fusei_session_loop(struct voluta_fusei *);
void voluta_fusei_session_break(struct voluta_fusei *);

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

void voluta_copy_timespec(struct timespec *, const struct timespec *);


void voluta_buf_init(struct voluta_buf *buf, void *p, size_t n);

size_t voluta_buf_append(struct voluta_buf *buf, const void *ptr, size_t len);

void voluta_buf_seteos(struct voluta_buf *buf);


void voluta_set_bit(uint32_t *arr, size_t nr);

void voluta_clear_bit(uint32_t *arr, size_t nr);

int voluta_test_bit(const uint32_t *arr, size_t nr);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* error-codes borrowed from XFS */
#ifndef ENOATTR
#define ENOATTR         ENODATA /* Attribute not found */
#endif
#ifndef EFSCORRUPTED
#define EFSCORRUPTED    EUCLEAN /* File-system is corrupted */
#endif
#ifndef EFSBADCRC
#define EFSBADCRC       EBADMSG /* Bad CRC detected */
#endif

#include "voluta-inline.h"

#endif /* VOLUTA_LIB_H_ */

