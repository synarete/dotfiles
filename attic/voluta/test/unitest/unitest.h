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
#ifndef VOLUTA_UNITEST_H_
#define VOLUTA_UNITEST_H_

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <linux/fs.h>
#include <linux/fiemap.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>

#include <voluta/voluta.h>
#include <voluta/syscall.h>
#include <voluta/types.h>
#include <voluta/list.h>
#include <voluta/avl.h>
#include <voluta/socket.h>
#include <voluta/infra.h>
#include <voluta/error.h>
#include <voluta/fuse.h>
#include <voluta/core.h>

#ifndef VOLUTA_UNITEST
#error "this header must not be included out-side of unitest"
#endif


struct ut_range {
	loff_t off;
	size_t len;
};

struct ut_ranges {
	const struct ut_range *arr;
	size_t cnt;
};

struct ut_keyval {
	const char *name;
	const void *value;
	size_t size;
};

struct ut_kvl {
	struct ut_env *ut_env;
	struct ut_keyval **list;
	size_t limit;
	size_t count;
};

struct ut_dirent_info {
	struct dirent64 de;
	struct stat attr;
};

struct ut_readdir_ctx {
	struct voluta_readdir_ctx rd_ctx;
	struct ut_dirent_info dei[64];
	unsigned int nde;
	int plus;
};

struct ut_listxattr_ctx {
	struct ut_env *ut_env;
	struct voluta_listxattr_ctx lxa_ctx;
	size_t count;
	char *names[64];
};

struct ut_malloc_chunk {
	struct ut_malloc_chunk *next;
	size_t  size;
	uint8_t data[32];
};

struct ut_env {
	char pass[VOLUTA_PASSPHRASE_MAX + 1];
	char salt[VOLUTA_PASSPHRASE_MAX + 1];
	struct voluta_fs_env *fs_env;
	struct voluta_oper oper;
	struct timespec ts_start;
	struct statvfs stvfs_start;
	size_t ualloc_start;
	loff_t volume_size;
	size_t nbytes_alloc;
	size_t unique_count;
	struct ut_malloc_chunk *malloc_list;
	const char *test_name;
	int silent;
	int pad;
};

struct ut_dvec {
	loff_t  off;
	size_t  len;
	uint8_t dat[8];
};

typedef void (*ut_hook_fn)(struct ut_env *);

struct ut_testdef {
	ut_hook_fn hook;
	const char *name;
};

struct ut_tests {
	const struct ut_testdef *arr;
	size_t len;
};

struct ut_tgroup {
	const struct ut_tests *tests;
	const char *name;
};


/* modules unit-tests entry-points */
extern const struct ut_tests ut_test_avl;
extern const struct ut_tests ut_test_qalloc;
extern const struct ut_tests ut_test_super;
extern const struct ut_tests ut_test_dir;
extern const struct ut_tests ut_test_dir_iter;
extern const struct ut_tests ut_test_dir_list;
extern const struct ut_tests ut_test_namei;
extern const struct ut_tests ut_test_rename;
extern const struct ut_tests ut_test_symlink;
extern const struct ut_tests ut_test_xattr;
extern const struct ut_tests ut_test_ioctl;
extern const struct ut_tests ut_test_file_basic;
extern const struct ut_tests ut_test_file_ranges;
extern const struct ut_tests ut_test_file_truncate;
extern const struct ut_tests ut_test_file_records;
extern const struct ut_tests ut_test_file_random;
extern const struct ut_tests ut_test_file_edges;
extern const struct ut_tests ut_test_file_fallocate;
extern const struct ut_tests ut_test_file_fiemap;
extern const struct ut_tests ut_test_file_lseek;
extern const struct ut_tests ut_test_fs_reload;
extern const struct ut_tests ut_test_fs_fill;

/* exec */
void ut_execute(const char *test_name);

void ut_freeall(struct ut_env *ut_env);

void *ut_malloc(struct ut_env *ut_env, size_t nbytes);

void *ut_zalloc(struct ut_env *ut_env, size_t nbytes);

char *ut_strdup(struct ut_env *ut_env, const char *str);

char *ut_strndup(struct ut_env *ut_env, const char *str, size_t);

const char *ut_make_name(struct ut_env *ut_env, const char *pre, size_t idx);

struct ut_env *ut_new_ctx(void);

void ut_del_ctx(struct ut_env *ut_env);

void *ut_zerobuf(struct ut_env *ut_env, size_t bsz);

void ut_randfill(struct ut_env *ut_env, void *buf, size_t bsz);

void *ut_randbuf(struct ut_env *ut_env, size_t bsz);

long *ut_randseq(struct ut_env *, size_t len, long base);

char *ut_randstr(struct ut_env *, size_t len);

char *ut_strfmt(struct ut_env *ut_env, const char *fmt, ...);

struct ut_readdir_ctx *ut_new_readdir_ctx(struct ut_env *ut_env);

struct ut_dvec *ut_new_dvec(struct ut_env *, loff_t, size_t);



/* no-fail operations wrappers */
void ut_access_ok(struct ut_env *ut_env, ino_t ino, int mode);

void ut_statfs_ok(struct ut_env *ut_env, ino_t ino, struct statvfs *st);

void ut_statfs_rootd(struct ut_env *ut_env, struct statvfs *st);

void ut_statx_exists(struct ut_env *ut_env, ino_t ino, struct statx *stx);

void ut_getattr_ok(struct ut_env *ut_env, ino_t ino, struct stat *st);

void ut_getattr_noent(struct ut_env *ut_env, ino_t ino);

void ut_getattr_file(struct ut_env *ut_env, ino_t ino, struct stat *st);

void ut_getattr_lnk(struct ut_env *ut_env, ino_t ino, struct stat *st);

void ut_getattr_dir(struct ut_env *ut_env, ino_t ino, struct stat *st);

void ut_getattr_dirsize(struct ut_env *ut_env, ino_t ino, loff_t size);

void ut_utimens_atime(struct ut_env *ut_env,
		      ino_t ino, const struct timespec *atime);

void ut_utimens_mtime(struct ut_env *ut_env,
		      ino_t ino, const struct timespec *mtime);

void ut_lookup_ok(struct ut_env *ut_env, ino_t parent,
		  const char *name, struct stat *out_st);

void ut_lookup_ino(struct ut_env *ut_env, ino_t parent,
		   const char *name, ino_t *out_ino);

void ut_lookup_exists(struct ut_env *ut_env, ino_t parent,
		      const char *name, ino_t ino, mode_t mode);

void ut_lookup_dir(struct ut_env *ut_env, ino_t parent,
		   const char *name, ino_t dino);

void ut_lookup_file(struct ut_env *ut_env, ino_t parent,
		    const char *name, ino_t ino);

void ut_lookup_lnk(struct ut_env *ut_env, ino_t parent,
		   const char *name, ino_t ino);

void ut_lookup_noent(struct ut_env *ut_env, ino_t ino, const char *name);

void ut_mkdir_ok(struct ut_env *ut_env, ino_t parent,
		 const char *name, struct stat *out_st);

void ut_mkdir_oki(struct ut_env *ut_env, ino_t parent,
		  const char *name, ino_t *out_ino);

void ut_mkdir_at_root(struct ut_env *ut_env, const char *name, ino_t *out_ino);

void ut_mkdir_err(struct ut_env *ut_env,
		  ino_t parent, const char *name, int err);

void ut_rmdir_ok(struct ut_env *ut_env, ino_t parent, const char *name);

void ut_rmdir_err(struct ut_env *ut_env, ino_t parent,
		  const char *name, int err);

void ut_rmdir_at_root(struct ut_env *ut_env, const char *name);

void ut_opendir_ok(struct ut_env *ut_env, ino_t ino);

void ut_opendir_err(struct ut_env *ut_env, ino_t ino, int err);

void ut_releasedir_ok(struct ut_env *ut_env, ino_t ino);

void ut_releasedir_err(struct ut_env *ut_env, ino_t ino, int err);

void ut_fsyncdir_ok(struct ut_env *ut_env, ino_t ino);

void ut_readdir_ok(struct ut_env *ut_env, ino_t ino, loff_t doff,
		   struct ut_readdir_ctx *ut_rd_ctx);

void ut_readdirplus_ok(struct ut_env *ut_env, ino_t ino, loff_t doff,
		       struct ut_readdir_ctx *ut_rd_ctx);

void ut_link_ok(struct ut_env *ut_env, ino_t ino,
		ino_t parent, const char *name, struct stat *out_st);

void ut_link_err(struct ut_env *ut_env, ino_t ino,
		 ino_t parent, const char *name, int err);

void ut_unlink_ok(struct ut_env *ut_env, ino_t parent, const char *name);

void ut_unlink_err(struct ut_env *ut_env, ino_t parent,
		   const char *name, int err);

void ut_unlink_file(struct ut_env *ut_env, ino_t parent, const char *name);

void ut_rename_move(struct ut_env *ut_env, ino_t parent, const char *name,
		    ino_t newparent, const char *newname);

void ut_rename_replace(struct ut_env *ut_env, ino_t parent, const char *name,
		       ino_t newparent, const char *newname);

void ut_rename_exchange(struct ut_env *ut_env, ino_t parent, const char *name,
			ino_t newparent, const char *newname);

void ut_symlink_ok(struct ut_env *ut_env, ino_t parent,
		   const char *name, const char *value, ino_t *out_ino);

void ut_readlink_expect(struct ut_env *ut_env,
			ino_t ino, const char *symval);

void ut_create_ok(struct ut_env *ut_env, ino_t parent,
		  const char *name, mode_t mode, struct stat *out_st);

void ut_create_file(struct ut_env *ut_env, ino_t parent,
		    const char *name, ino_t *out_ino);

void ut_create_noent(struct ut_env *ut_env,
		     ino_t parent, const char *name);

void ut_create_special(struct ut_env *ut_env, ino_t parent,
		       const char *name, mode_t mode, ino_t *out_ino);


void ut_release_ok(struct ut_env *ut_env, ino_t ino);

void ut_release_file(struct ut_env *ut_env, ino_t ino);

void ut_fsync_ok(struct ut_env *ut_env, ino_t ino, bool datasync);

void ut_remove_file(struct ut_env *ut_env, ino_t parent,
		    const char *, ino_t ino);

void ut_create_only(struct ut_env *ut_env, ino_t parent,
		    const char *name, ino_t *out_ino);

void ut_open_rdonly(struct ut_env *ut_env, ino_t ino);

void ut_open_rdwr(struct ut_env *ut_env, ino_t ino);

void ut_remove_link(struct ut_env *ut_env,
		    ino_t parent, const char *name);

void ut_write_ok(struct ut_env *ut_env, ino_t ino,
		 const void *buf, size_t bsz, loff_t off);

void ut_write_nospc(struct ut_env *ut_env, ino_t ino,
		    const void *buf, size_t bsz,
		    loff_t off, size_t *out_nwr);

void ut_write_read(struct ut_env *ut_env, ino_t ino,
		   const void *buf, size_t bsz, loff_t off);

void ut_write_read1(struct ut_env *ut_env, ino_t ino, loff_t off);

void ut_write_read_str(struct ut_env *ut_env, ino_t ino,
		       const char *str, loff_t off);

void ut_read_verify(struct ut_env *ut_env, ino_t ino,
		    const void *buf, size_t bsz, loff_t off);

void ut_read_verify_str(struct ut_env *ut_env,
			ino_t ino, const char *str, loff_t off);

void ut_read_zero(struct ut_env *ut_env, ino_t ino, loff_t off);

void ut_read_zeros(struct ut_env *ut_env,
		   ino_t ino, loff_t off, size_t len);

void ut_read_ok(struct ut_env *ut_env, ino_t ino,
		void *buf, size_t bsz, loff_t off);

void ut_trunacate_file(struct ut_env *ut_env, ino_t ino, loff_t len);

void ut_fallocate_reserve(struct ut_env *ut_env, ino_t ino,
			  loff_t offset, loff_t len);

void ut_fallocate_punch_hole(struct ut_env *ut_env, ino_t ino,
			     loff_t offset, loff_t len);

void ut_setxattr_create(struct ut_env *ut_env, ino_t ino,
			const struct ut_keyval *kv);

void ut_setxattr_replace(struct ut_env *ut_env, ino_t ino,
			 const struct ut_keyval *kv);

void ut_setxattr_rereplace(struct ut_env *ut_env, ino_t ino,
			   const struct ut_keyval *kv);

void ut_getxattr_value(struct ut_env *ut_env, ino_t ino,
		       const struct ut_keyval *kv);

void ut_getxattr_nodata(struct ut_env *ut_env, ino_t ino,
			const struct ut_keyval *);

void ut_removexattr_ok(struct ut_env *ut_env, ino_t ino,
		       const struct ut_keyval *);

void ut_listxattr_ok(struct ut_env *ut_env, ino_t ino,
		     const struct ut_kvl *kvl);

void ut_setxattr_all(struct ut_env *ut_env, ino_t ino,
		     const struct ut_kvl *kvl);

void ut_removexattr_all(struct ut_env *ut_env, ino_t ino,
			const struct ut_kvl *kvl);

void ut_query_ok(struct ut_env *ut_env, ino_t ino,
		 struct voluta_query *out_inq);

void ut_fiemap_ok(struct ut_env *ut_env, ino_t ino, struct fiemap *fm);

void ut_lseek_data(struct ut_env *ut_env,
		   ino_t ino, loff_t off, loff_t *out_off);

void ut_lseek_hole(struct ut_env *ut_env,
		   ino_t ino, loff_t off, loff_t *out_off);

void ut_write_dvec(struct ut_env *ut_env, ino_t ino,
		   const struct ut_dvec *dvec);

void ut_read_dvec(struct ut_env *ut_env, ino_t ino,
		  const struct ut_dvec *dvec);

void ut_sync_drop(struct ut_env *ut_env);

void ut_drop_caches_fully(struct ut_env *ut_env);

void ut_reload_ok(struct ut_env *ut_env, ino_t ino);

/* utilities */
void ut_prandom_seq(long *arr, size_t len, long base);
bool ut_dot_or_dotdot(const char *s);
bool ut_not_dot_or_dotdot(const char *s);

/* assertions */
#define ut_assert(cond)         voluta_assert(cond)
#define ut_assert_lt(a, b)      voluta_assert_lt(a, b)
#define ut_assert_le(a, b)      voluta_assert_le(a, b)
#define ut_assert_gt(a, b)      voluta_assert_gt(a, b)
#define ut_assert_ge(a, b)      voluta_assert_ge(a, b)
#define ut_assert_eq(a, b)      voluta_assert_eq(a, b)
#define ut_assert_ne(a, b)      voluta_assert_ne(a, b)
#define ut_assert_eqs(a, b)     voluta_assert_eqs(a, b)
#define ut_assert_eqm(a, b, n)  voluta_assert_eqm(a, b, n)
#define ut_assert_ok(err)       voluta_assert_ok(err)
#define ut_assert_err(err, val) voluta_assert_err(err, val)
#define ut_assert_null(ptr)     voluta_assert_null(ptr)
#define ut_assert_not_null(ptr)  voluta_assert_not_null(ptr)

#define ut_zerobuf(ut_env, n)   ut_zerobuf(ut_env, n)
#define ut_randbuf(ut_env, n)   ut_randbuf(ut_env, n)
#define ut_randstr(ut_env, n)   ut_randstr(ut_env, n)
#define ut_strfmt(ut_env, fmt, ...) \
	ut_strfmt(ut_env, fmt, __VA_ARGS__)

void ut_assert_eq_ts(const struct timespec *ts1, const struct timespec *ts2);

void ut_assert_eq_stat(const struct stat *st1, const struct stat *st2);

void ut_assert_eq_statvfs(const struct statvfs *s1, const struct statvfs *s2);

void ut_assert_valid_statvfs(const struct statvfs *stv);

/* aliases */
#define UT_KILO                 VOLUTA_KILO
#define UT_MEGA                 VOLUTA_MEGA
#define UT_GIGA                 VOLUTA_GIGA
#define UT_TERA                 VOLUTA_TERA
#define UT_UMEGA                VOLUTA_UMEGA
#define UT_UGIGA                VOLUTA_UGIGA
#define UT_UTERA                VOLUTA_UTERA
#define UT_ARRAY_SIZE(x)        VOLUTA_ARRAY_SIZE(x)
#define UT_NAME_MAX             VOLUTA_NAME_MAX
#define UT_BK_SIZE              VOLUTA_BK_SIZE
#define UT_FILESIZE_MAX         VOLUTA_FILE_SIZE_MAX
#define UT_IOSIZE_MAX           VOLUTA_IO_SIZE_MAX
#define UT_FILEMAP_NCHILD       VOLUTA_FILE_MAP_NCHILD
#define UT_ROOT_INO             VOLUTA_INO_ROOT
#define UT_NAME                 __func__

#define ut_container_of(ptr_, type_, member_) \
	voluta_container_of(ptr_, type_, member_)

#define ut_container_of2(ptr_, type_, member_) \
	voluta_container_of2(ptr_, type_, member_)

#define UT_DEFTEST(fn_) \
	{ .hook = fn_, .name = VOLUTA_STR(fn_) }
#define UT_MKTESTS(arr_) \
	{ arr_, VOLUTA_ARRAY_SIZE(arr_) }

/* inlines */
static inline loff_t ut_off_aligned(loff_t off, loff_t align)
{
	return (off / align) * align;
}

static inline loff_t ut_off_baligned(loff_t off)
{
	return ut_off_aligned(off, VOLUTA_BK_SIZE);
}

static inline size_t ut_off_len(loff_t beg, loff_t end)
{
	return (size_t)(end - beg);
}

#endif /* VOLUTA_UNITEST_H_ */
