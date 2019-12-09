/* SPDX-License-Identifier: LGPL-3.0-or-later */
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
 */
#ifndef VOLUTA_LIB_H_
#define VOLUTA_LIB_H_

#ifndef VOLUTA_LIB_MODULE
#error "internal voluta library header -- no not include!"
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


/* Number of slab-allocator sub-elements sizes; powers-of-2 */
#define VOLUTA_ALLOC_NSLABS \
	(VOLUTA_PAGE_SHIFT_MIN - VOLUTA_CACHELINE_SHIFT_MIN)


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
	struct voluta_hash256 hash;
};

struct voluta_buf {
	void  *ptr;
	size_t len;
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
	ino_t   ia_ino;
	ino_t   ia_parent_ino;
	mode_t  ia_mode;
	nlink_t ia_nlink;
	uid_t   ia_uid;
	gid_t   ia_gid;
	dev_t   ia_rdev;
	loff_t  ia_size;
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

struct voluta_qmalloc {
	int memfd_data;         /* Data-memory file-descriptor */
	int memfd_meta;         /* Meta-memory file-descriptor */
	void *mem_data;         /* Beginning of memory-mapped data file */
	void *mem_meta;         /* Beginning of meory-mapped meta-data file */
	struct voluta_qmstat st;/* Stat accountings */
	size_t ncycles;         /* Running-index of operations-cycles */
	struct voluta_list_head free_list;   /* Free-pages list */
	struct voluta_slab slabs[VOLUTA_ALLOC_NSLABS]; /* Slab-lists */
	bool pedantic_mode;     /* Should trash memory upon free? */
} voluta_aligned;

/* Caching */
struct voluta_cache_elem {
	struct voluta_list_head ce_htbl_link;   /* Cache's hash-map link */
	struct voluta_list_head ce_lru_link;    /* Cache's LRU link */
	uint64_t ce_key;                        /* 64-bits caching key */
	size_t  ce_cycle;                       /* Id of last in-use cycle */
	size_t  ce_refcnt;                      /* References by other */
	long    ce_magic;
};

struct voluta_bk_info {
	struct voluta_cache_elem b_ce;          /* Cached element */
	union voluta_bk   *bk;                  /* Actual block's data */
	struct voluta_vaddr b_vaddr;            /* Logical-address */
	struct voluta_sb_info *b_sbi;           /* Top-level super-block */
	struct voluta_bk_info *b_uber_bki;      /* Superior uber-block */
	struct voluta_list_head  b_dirtyq_link; /* Dirty queue link */
	bool    b_dirty;                        /* Is dirty? */
	bool    b_staged;                       /* Is fully staged ? */
} voluta_aligned64;

union voluta_vnode_u {
	struct voluta_itable_tnode *itn;
	struct voluta_radix_tnode  *rtn;
	struct voluta_dir_tnode    *dtn;
	struct voluta_xattr_node   *xan;
	struct voluta_data_frg     *df;
	void *p;
};

struct voluta_vnode_info {
	struct voluta_vaddr vaddr;      /* Persistent logical address */
	struct voluta_cache_elem ce;    /* Caching base-element */
	struct voluta_bk_info *bki;     /* Containing block */
	union voluta_view *view;      /* "View" into "on-disk" element */
	union voluta_vnode_u u;

} voluta_aligned;

struct voluta_inode_info {
	struct voluta_vnode_info vi;    /* Base logical-node info */
	struct voluta_inode *inode;     /* Referenced inode */
	ino_t ino;                      /* Unique ino number */
} voluta_aligned;

/* Virtual-device interface */
struct voluta_vd_info {
	const struct voluta_crypto *crypto;
	struct voluta_qmalloc *qmal;
	union voluta_bk *enc_bk;        /* On-the-fly encryption block */
	loff_t size;                    /* Size in bytes (volume's capacity) */
	int fd;                         /* I/O file descriptor */
	int flags;                      /* Control flags */
};

typedef uint64_t (*voulta_hash64_fn)(uint64_t);

struct voluta_cachemap {
	struct voluta_qmalloc *qmal;      /* Memory allocator */
	struct voluta_list_head lru;    /* LRU queue of elements */
	struct voluta_list_head *htbl;  /* Elements table */
	size_t htbl_size;               /* Size of hash-table */
	size_t count;                   /* Number currently mapped elements */
	voulta_hash64_fn hash_fn;       /* Key re-hashing function */
};

struct voluta_dirtyq {
	struct voluta_list_head data;
	struct voluta_list_head meta;
	size_t data_size;
	size_t meta_size;
};

struct voluta_cache {
	struct voluta_qmalloc *qmal;    /* Memory allocator */
	struct voluta_cachemap bcmap;   /* Blocks cache-map */
	struct voluta_cachemap icmap;   /* Inodes cache-map */
	struct voluta_cachemap vcmap;   /* Vnodes cache-map */
	struct voluta_dirtyq dirtyq;    /* Dirty blocks queue */
	union voluta_bk *null_bk;       /* Block-of-zeros */
};

struct voluta_sp_stat {
	ssize_t sp_nbytes;
	ssize_t sp_nmeta;
	ssize_t sp_ndata;
	ssize_t sp_nfiles;
};

struct voluta_ag_info {
	struct voluta_list_head lh;
	int  nfree;
	bool uber;
	bool live;
	char pad[2];
};

struct voluta_ag_space {
	size_t ag_count;        /* Number of allocation groups */
	size_t ag_nlive;
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
	const char *s_name;             /* File system's name */
	struct voluta_uuid    s_fs_uuid;/* File system's UUID */
	struct voluta_qmalloc *s_qmal;
	struct voluta_cache   *s_cache;
	struct voluta_vd_info *s_vdi;
	struct voluta_super_block *sb;  /* Super-block */
	struct voluta_itable s_itable; /* Inodes table */
	struct voluta_iv_key s_iv_key;     /* Master encryption IV/key */
	struct voluta_sp_stat s_sp_stat;   /* Space accounting */
	struct voluta_ag_space s_ag_space; /* Allocation-groups space-map */
};

/* FUSE interface */
struct voluta_fusei {
	unsigned long magic;            /* Debug magic-number */
	struct voluta_env *env;         /* Parent environment */
	struct voluta_qmalloc *qmal;    /* Memory allocator */
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

/* Main environment context object */
struct voluta_env {
	struct voluta_qmalloc qmal;     /* Memory allocator */
	struct voluta_cache cache;      /* Volatile elements cache */
	struct voluta_crypto crypto;    /* Cryptographic context */
	struct voluta_vd_info vdi;      /* Virtual-device file interface */
	struct voluta_sb_info sbi;      /* In-memory (volatile) super-block */
	struct voluta_fusei fusei;      /* Bridge via FUSE */
	struct voluta_ucred ucred;      /* Current operation user-credentials */
	struct voluta_xtime xtime;      /* Current execution time-stamps */
	iconv_t iconv_handle;           /* Char-encoding converter */
	const char *mount_point;        /* Local mount-point path */
	const char *volume_path;        /* Data-store device path */
	loff_t volume_size;             /* Size of back-end storage volume */
	int signum;                     /* Last caught signum number */
	bool tmpfs_mode;                /* Run in tmpfs mode */
} voluta_aligned64;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

enum voluta_flags {
	VOLUTA_F_SYNC   = 0x0001,
	VOLUTA_F_META   = 0x0002,
	VOLUTA_F_DATA   = 0x0004,
	VOLUTA_F_NOW    = 0x0008,
	VOLUTA_F_BLKDEV = 0x0010,
};

/* super */
struct voluta_sb_info *voluta_sbi_of(const struct voluta_env *);
int voluta_sbi_init(struct voluta_sb_info *,
		    struct voluta_cache *, struct voluta_vd_info *);
void voluta_sbi_fini(struct voluta_sb_info *);
bool voluta_off_isnull(loff_t);
bool voluta_vaddr_isnull(const struct voluta_vaddr *);
void voluta_vaddr_reset(struct voluta_vaddr *);
void voluta_vaddr_clone(const struct voluta_vaddr *, struct voluta_vaddr *);
void voluta_vaddr_by_off(struct voluta_vaddr *, enum voluta_vtype, loff_t);
void voluta_iaddr_reset(struct voluta_iaddr *);
void voluta_iaddr_setup(struct voluta_iaddr *, ino_t, loff_t, size_t);
bool voluta_is_unwritten(const struct voluta_bk_info *);
void voluta_clear_unwritten(const struct voluta_bk_info *);
int voluta_prepare_space(struct voluta_sb_info *, const char *, loff_t);
int voluta_format_super_block(struct voluta_sb_info *);
int voluta_format_uber_blocks(struct voluta_sb_info *);
int voluta_load_super_block(struct voluta_sb_info *);
int voluta_load_ag_space(struct voluta_sb_info *);
int voluta_load_inode_table(struct voluta_sb_info *);
int voluta_verify_uber_block(const struct voluta_uber_block *);
int voluta_verify_super_block(const struct voluta_super_block *);
void voluta_space_to_statvfs(const struct voluta_sb_info *, struct statvfs *);
int voluta_commit_dirtyq(struct voluta_sb_info *, int);
int voluta_stage_inode(struct voluta_sb_info *, ino_t,
		       struct voluta_inode_info **);
int voluta_stage_vnode(struct voluta_sb_info *, const struct voluta_vaddr *,
		       struct voluta_vnode_info **);
int voluta_new_inode(struct voluta_env *, mode_t, ino_t,
		     struct voluta_inode_info **);
int voluta_del_inode(struct voluta_sb_info *, struct voluta_inode_info *);
int voluta_new_vnode(struct voluta_sb_info *, enum voluta_vtype,
		     struct voluta_vnode_info **);
int voluta_del_vnode(struct voluta_sb_info *, struct voluta_vnode_info *);
int voluta_del_vnode_at(struct voluta_sb_info *, const struct voluta_vaddr *);
int voluta_new_vspace(struct voluta_sb_info *, enum voluta_vtype,
		      size_t, struct voluta_vaddr *);

/* itable */
void voluta_itable_init(struct voluta_itable *);
void voluta_itable_fini(struct voluta_itable *);
int voluta_acquire_ino(struct voluta_sb_info *, struct voluta_iaddr *);
int voluta_discard_ino(struct voluta_sb_info *, ino_t);
int voluta_resolve_ino(struct voluta_sb_info *, ino_t, struct voluta_iaddr *);
int voluta_find_root_ino(struct voluta_sb_info *, struct voluta_iaddr *);
void voluta_bind_root_ino(struct voluta_sb_info *, ino_t);
int voluta_reload_itable(struct voluta_sb_info *, const struct voluta_vaddr *);
int voluta_format_itable(struct voluta_sb_info *);
int voluta_real_ino(const struct voluta_sb_info *, ino_t, ino_t *);
int voluta_verify_itnode(const struct voluta_itable_tnode *);

/* namei */
int voluta_authorize(const struct voluta_env *);
int voluta_name_to_str(struct voluta_env *, const struct voluta_inode_info *,
		       const char *, struct voluta_str *);
int voluta_name_to_qstr(struct voluta_env *, const struct voluta_inode_info *,
			const char *, struct voluta_qstr *);
int voluta_symval_to_str(const char *symval, struct voluta_str *str);

int voluta_do_statvfs(struct voluta_env *, struct statvfs *);
int voluta_do_access(struct voluta_env *,
		     const struct voluta_inode_info *, int);
int voluta_do_lookup(struct voluta_env *, const struct voluta_inode_info *,
		     const struct voluta_qstr *, struct voluta_inode_info **);
int voluta_open(struct voluta_env *, struct voluta_inode_info *, int);
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
int voluta_inc_fileref(const struct voluta_env *, struct voluta_inode_info *);

/* dir */
size_t voluta_dir_ndentries(const struct voluta_inode_info *);
int voluta_verify_dir_inode(const struct voluta_inode *);
int voluta_verify_dir_tnode(const struct voluta_dir_tnode *);
void voluta_setup_new_dir(struct voluta_inode_info *dir_ii);
int voluta_drop_dir(struct voluta_env *, struct voluta_inode_info *ii);
int voluta_resolve_by_name(struct voluta_env *,
			   const struct voluta_inode_info *,
			   const struct voluta_qstr *, struct voluta_ino_dt *);
int voluta_lookup_by_dname(struct voluta_env *,
			   const struct voluta_inode_info *,
			   const struct voluta_qstr *qstr, ino_t *out_ino);
int voluta_lookup_by_iname(struct voluta_env *,
			   const struct voluta_inode_info *,
			   const struct voluta_qstr *qstr, ino_t *out_ino);
int voluta_check_add_dentry(const struct voluta_inode_info *dir_ii,
			    const struct voluta_qstr *name);
int voluta_add_dentry(struct voluta_env *, struct voluta_inode_info *,
		      const struct voluta_qstr *, struct voluta_inode_info *);
int voluta_remove_dentry(struct voluta_env *, struct voluta_inode_info *,
			 const struct voluta_qstr *);
int voluta_do_fsyncdir(struct voluta_env *, struct voluta_inode_info *, bool);
int voluta_do_opendir(struct voluta_env *, struct voluta_inode_info *);
int voluta_do_releasedir(struct voluta_env *, struct voluta_inode_info *);
int voluta_do_readdir(struct voluta_env *, const struct voluta_inode_info *,
		      struct voluta_readdir_ctx *);

/* file */
void voluta_setup_new_reg(struct voluta_inode_info *);
int voluta_drop_reg(struct voluta_env *, struct voluta_inode_info *ii);
int voluta_do_write(struct voluta_env *, struct voluta_inode_info *,
		    const void *buf, size_t count, loff_t offset, size_t *);
int voluta_do_write_iter(struct voluta_env *,
			 struct voluta_inode_info *, struct voluta_rw_iter *);
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
int voluta_verify_radix_tnode(const struct voluta_radix_tnode *);

/* inode */
bool voluta_uid_eq(uid_t, uid_t);
bool voluta_gid_eq(gid_t, gid_t);
bool voluta_uid_isroot(uid_t uid);
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
dev_t voluta_rdev_of(const struct voluta_inode_info *ii);
size_t voluta_nxattr_of(const struct voluta_inode_info *ii);
bool voluta_isdir(const struct voluta_inode_info *ii);
bool voluta_isreg(const struct voluta_inode_info *ii);
bool voluta_isfifo(const struct voluta_inode_info *ii);
bool voluta_issock(const struct voluta_inode_info *ii);
bool voluta_islnk(const struct voluta_inode_info *ii);
bool voluta_isrootd(const struct voluta_inode_info *ii);
int voluta_setup_new_inode(struct voluta_env *, struct voluta_inode_info *,
			   mode_t mode, ino_t parent_ino);
int voluta_do_getattr(struct voluta_env *,
		      const struct voluta_inode_info *, struct stat *);
int voluta_do_statx(struct voluta_env *env,
		    const struct voluta_inode_info *ii, struct statx *);
int voluta_do_chmod(struct voluta_env *, struct voluta_inode_info *,
		    mode_t, const struct voluta_iattr *);
int voluta_do_chown(struct voluta_env *, struct voluta_inode_info *,
		    uid_t, gid_t, const struct voluta_iattr *);
int voluta_do_utimens(struct voluta_env *, struct voluta_inode_info *,
		      const struct voluta_iattr *);
int voluta_do_evict_inode(struct voluta_sb_info *, ino_t);
void voluta_update_iattr(struct voluta_env *, struct voluta_inode_info *,
			 const struct voluta_iattr *);
void voluta_update_itimes(struct voluta_env *,
			  struct voluta_inode_info *, enum voluta_attr_flags);
int voluta_verify_inode(const struct voluta_inode *);
void voluta_iattr_reset(struct voluta_iattr *);

const struct voluta_vaddr *voluta_vaddr_of_vi(const struct voluta_vnode_info *);
const struct voluta_vaddr *voluta_vaddr_of_ii(const struct voluta_inode_info *);

/* symlink */
void voluta_setup_new_symlnk(struct voluta_inode_info *);
int voluta_drop_symlink(struct voluta_env *, struct voluta_inode_info *);
int voluta_do_readlink(struct voluta_env *,
		       const struct voluta_inode_info *, struct voluta_buf *);
int voluta_setup_symlink(struct voluta_env *, struct voluta_inode_info *,
			 const struct voluta_str *);
int voluta_verify_symval(const struct voluta_lnk_symval *);

/* xattr */
int voluta_do_getxattr(struct voluta_env *, struct voluta_inode_info *ii,
		       const struct voluta_str *, void *, size_t, size_t *);
int voluta_do_setxattr(struct voluta_env *,
		       struct voluta_inode_info *ii,
		       const struct voluta_str *,
		       const char *, size_t size, int);
int voluta_do_removexattr(struct voluta_env *,
			  struct voluta_inode_info *ii,
			  const struct voluta_str *);
int voluta_do_listxattr(struct voluta_env *, struct voluta_inode_info *ii,
			struct voluta_listxattr_ctx *listxattr_ctx);
int voluta_drop_xattr(struct voluta_env *, struct voluta_inode_info *ii);
int voluta_verify_xattr_node(const struct voluta_xattr_node *);

/* volume */
int voluta_vdi_init(struct voluta_vd_info *, struct voluta_qmalloc *,
		    const struct voluta_crypto *);
void voluta_vdi_fini(struct voluta_vd_info *);
int voluta_vdi_create(struct voluta_vd_info *, const char *, loff_t);
int voluta_vdi_open(struct voluta_vd_info *, const char *);
void voluta_vdi_close(struct voluta_vd_info *);
int voluta_vdi_sync(struct voluta_vd_info *, bool datasync);
int voluta_vdi_read(const struct voluta_vd_info *, loff_t, size_t, void *);
int voluta_vdi_write(const struct voluta_vd_info *,
		     loff_t, size_t, const void *);
int voluta_vdi_load_dec(struct voluta_vd_info *, loff_t,
			const struct voluta_iv_key *, union voluta_bk *);
int voluta_vdi_enc_save(struct voluta_vd_info *, loff_t,
			const struct voluta_iv_key *, const union voluta_bk *);

/* crypto */
int voluta_init_gcrypt(void);
void voluta_fill_random_key(struct voluta_key *);
void voluta_fill_random_iv(struct voluta_iv *);
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

/* fusei */
int voluta_fusei_init(struct voluta_fusei *, struct voluta_env *);
void voluta_fusei_fini(struct voluta_fusei *);
int voluta_fusei_mount(struct voluta_fusei *, const char *mntpoint);
int voluta_fusei_umount(struct voluta_fusei *);
int voluta_fusei_session_loop(struct voluta_fusei *);
void voluta_fusei_session_break(struct voluta_fusei *);

/* cache */
int voluta_cache_init(struct voluta_cache *, struct voluta_qmalloc *);
void voluta_cache_fini(struct voluta_cache *);
void voluta_cache_step_cycles(struct voluta_cache *, size_t);
void voluta_cache_next_cycle(struct voluta_cache *);
void voluta_cache_relax(struct voluta_cache *, bool);
void voluta_cache_drop(struct voluta_cache *);
int voluta_cache_lookup_bk(struct voluta_cache *,
			   const struct voluta_vaddr *,
			   struct voluta_bk_info **);
int voluta_cache_spawn_bk(struct voluta_cache *, const struct voluta_vaddr *,
			  struct voluta_bk_info *, struct voluta_bk_info **);
void voluta_cache_forget_bk(struct voluta_cache *, struct voluta_bk_info *);
void voluta_cache_try_forget_bk(struct voluta_cache *, struct voluta_bk_info *);
int voluta_cache_spawn_ii(struct voluta_cache *, const struct voluta_iaddr *,
			  struct voluta_inode_info **);
int voluta_cache_lookup_ii(struct voluta_cache *, ino_t,
			   struct voluta_inode_info **);
void voulta_cache_forget_ii(struct voluta_cache *, struct voluta_inode_info *);

int voluta_cache_lookup_vi(struct voluta_cache *, const struct voluta_vaddr *,
			   struct voluta_vnode_info **);
void voulta_cache_forget_vi(struct voluta_cache *, struct voluta_vnode_info *);
int voluta_cache_spawn_vi(struct voluta_cache *, const struct voluta_vaddr *,
			  struct voluta_vnode_info **);

void voluta_dirtify_bk(struct voluta_bk_info *);
void voluta_undirtify_bk(struct voluta_bk_info *);
struct voluta_bk_info *voluta_first_dirty_bk(const struct voluta_cache *);
void voluta_dirtify_vi(const struct voluta_vnode_info *);
void voluta_undirtify_vi(const struct voluta_vnode_info *vi);
void voluta_dirtify_ii(const struct voluta_inode_info *);
bool voluta_isevictable_ii(const struct voluta_inode_info *);
bool voluta_isevictable_vi(const struct voluta_vnode_info *);

void voluta_attach_vi(struct voluta_vnode_info *, struct voluta_bk_info *);
const struct voluta_vaddr *voluta_vaddr_of_bk(const struct voluta_bk_info *);

/* persist */
void voluta_verify_persistent_format(void);
int voluta_psize_of(enum voluta_vtype, mode_t, size_t *);
int voluta_verify_view(const union voluta_view *, enum voluta_vtype);
int voluta_verify_ino(ino_t);
int voluta_verify_off(loff_t);
int voluta_verify_count(size_t, size_t);
void voluta_stamp_vbk(struct voluta_bk_info *);
void voluta_stamp_vnode(struct voluta_vnode_info *);
void voluta_stamp_meta(struct voluta_header *, enum voluta_vtype, size_t);

/* thread */
int voluta_thread_sigblock_common(void);
int voluta_thread_create(struct voluta_thread *thread);
int voluta_thread_join(struct voluta_thread *thread);
int voluta_mutex_init(struct voluta_mutex *mutex);
void voluta_mutex_destroy(struct voluta_mutex *mutex);
void voluta_mutex_lock(struct voluta_mutex *mutex);
bool voluta_mutex_rylock(struct voluta_mutex *mutex);
void voluta_mutex_unlock(struct voluta_mutex *mutex);

/* qmalloc */
int voluta_memref(const struct voluta_qmalloc *, const void *,
		  size_t len, struct voluta_qmref *);

/* utility */
void voluta_uuid_generate(struct voluta_uuid *);
void voluta_uuid_clone(const struct voluta_uuid *, struct voluta_uuid *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* aliases */
#define ARRAY_SIZE(x)                   VOLUTA_ARRAY_SIZE(x)
#define container_of(p, t, m)           voluta_container_of(p, t, m)
#define off_isnull(off)                 voluta_off_isnull(off)
#define vaddr_isnull(va)                voluta_vaddr_isnull(va)
#define vaddr_reset(va)                 voluta_vaddr_reset(va)
#define vaddr_clone(va1, va2)           voluta_vaddr_clone(va1, va2)
#define bk_dirtify(bki)                 voluta_dirtify_bk(bki)
#define bk_undirtify(bki)               voluta_undirtify_bk(bki)
#define v_vaddr_of(vi)                  voluta_vaddr_of_vi(vi)
#define v_dirtify(vi)                   voluta_dirtify_vi(vi)
#define v_undirtify(vi)                 voluta_undirtify_vi(vi)
#define i_vaddr_of(ii)                  voluta_vaddr_of_ii(ii);
#define i_refcnt_of(ii)                 voluta_inode_refcnt(ii)
#define i_ino_of(ii)                    voluta_ino_of(ii)
#define i_xino_of(ii)                   voluta_xino_of(ii)
#define i_parent_ino_of(ii)             voluta_parent_ino_of(ii)
#define i_uid_of(ii)                    voluta_uid_of(ii)
#define i_gid_of(ii)                    voluta_gid_of(ii)
#define i_mode_of(ii)                   voluta_mode_of(ii)
#define i_nlink_of(ii)                  voluta_nlink_of(ii)
#define i_size_of(ii)                   voluta_isize_of(ii)
#define i_blocks_of(ii)                 voluta_blocks_of(ii)
#define i_rdev_of(ii)                   voluta_rdev_of(ii)
#define i_nxattr_of(ii)                 voluta_nxattr_of(ii)
#define i_isrootd(ii)                   voluta_isrootd(ii)
#define i_isdir(ii)                     voluta_isdir(ii)
#define i_isreg(ii)                     voluta_isreg(ii)
#define i_islnk(ii)                     voluta_islnk(ii)
#define i_isfifo(ii)                    voluta_isfifo(ii)
#define i_issock(ii)                    voluta_issock(ii)
#define i_dirtify(ii)                   voluta_dirtify_ii(ii)
#define i_update_times(e, ii, f)        voluta_update_itimes(e, ii, f)
#define i_update_attr(e, ii, a)         voluta_update_iattr(e, ii, a)
#define i_revise_privs(e, ii)           voluta_revise_privs(e, ii)
#define attr_reset(a)                   voluta_iattr_reset(a)
#define ts_copy(dst, src)               voluta_copy_timespec(dst, src)
#define uid_isroot(u)                   voluta_uid_isroot(u)
#define uid_eq(u1, u2)                  voluta_uid_eq(u1, u2)
#define gid_eq(g1, g2)                  voluta_gid_eq(g1, g2)
#define sbi_of(env)                     voluta_sbi_of(env)

#define list_head_init(lh)              voluta_list_head_init(lh)
#define list_head_initn(lh, n)          voluta_list_head_initn(lh, n)
#define list_head_destroy(lh)           voluta_list_head_destroy(lh)
#define list_head_destroyn(lh, n)       voluta_list_head_destroyn(lh, n)
#define list_head_remove(lh)            voluta_list_head_remove(lh)
#define list_head_insert_after(p, q)    voluta_list_head_insert_after(p, q)
#define list_head_insert_before(p, q)   voluta_list_head_insert_before(p, q)

#define list_init(ls)                   voluta_list_init(ls)
#define list_destroy(ls)                voluta_list_destroy(ls)
#define list_push_back(ls, lh)          voluta_list_push_back(ls, lh)
#define list_push_front(ls, lh)         voluta_list_push_front(ls, lh)
#define list_pop_front(ls)              voluta_list_pop_front(ls)
#define list_isempty(ls)                voluta_list_isempty(ls)

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* error-codes borrowed from XFS */
#ifndef ENOATTR
#define ENOATTR         ENODATA /* Attribute not found */
#endif
#ifndef EFSCORRUPTED
#define EFSCORRUPTED    EUCLEAN /* Filesystem is corrupted */
#endif
#ifndef EFSBADCRC
#define EFSBADCRC       EBADMSG /* Bad CRC detected */
#endif

#endif /* VOLUTA_LIB_H_ */

