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
#ifndef VOLUTA_UNITEST_H_
#define VOLUTA_UNITEST_H_

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
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

#ifndef VOLUTA_UNITEST
#error "this header must not be included out-side of unitest"
#endif

#define VOLUTA_UT_NAME __func__


struct voluta_ut_range {
	loff_t off;
	size_t len;
};

struct voluta_ut_ranges {
	const struct voluta_ut_range *arr;
	size_t cnt;
};

struct voluta_ut_keyval {
	const char *name;
	const void *value;
	size_t size;
};

struct voluta_ut_kvl {
	struct voluta_ut_ctx *ut_ctx;
	struct voluta_ut_keyval **list;
	size_t limit;
	size_t count;
};

struct voluta_ut_readdir_ctx {
	struct voluta_readdir_ctx readdir_ctx;
	struct dirent64 dents[64];
	size_t ndents;
};

struct voluta_ut_listxattr_ctx {
	struct voluta_ut_ctx *ut_ctx;
	struct voluta_listxattr_ctx listxattr_ctx;
	size_t count;
	char *names[64];
};

struct voluta_ut_malloc_chunk {
	struct voluta_ut_malloc_chunk *next;
	size_t  size;
	uint8_t data[32];
};

struct voluta_ut_ctx {
	struct voluta_env *env;
	struct timespec ts_start;
	struct statvfs stvfs_start;
	size_t ualloc_start;
	loff_t volume_size;
	size_t nbytes_alloc;
	struct voluta_ut_malloc_chunk *malloc_list;
	const char *test_name;
	bool silent;
};

struct voluta_ut_iobuf {
	loff_t  off;
	size_t  len;
	uint8_t buf[8];
};

typedef void (*voluta_ut_hook_fn)(struct voluta_ut_ctx *);

struct voluta_ut_testdef {
	voluta_ut_hook_fn hook;
	const char *name;
};

struct voluta_ut_tests {
	const struct voluta_ut_testdef *arr;
	size_t len;
};

struct voluta_ut_tgroup {
	const struct voluta_ut_tests *tests;
	const char *name;
};


#define VOLUTA_UT_MKTESTS(arr_) \
	{ arr_, VOLUTA_ARRAY_SIZE(arr_) }

#define VOLUTA_UT_DEFTEST(fn_) \
	{ .hook = fn_, .name = VOLUTA_STR(fn_) }


/* modules unit-tests entry-points */
extern const struct voluta_ut_tests voluta_ut_alloc_tests;
extern const struct voluta_ut_tests voluta_ut_super_tests;
extern const struct voluta_ut_tests voluta_ut_dir_tests;
extern const struct voluta_ut_tests voluta_ut_dir_iter_tests;
extern const struct voluta_ut_tests voluta_ut_dir_list_tests;
extern const struct voluta_ut_tests voluta_ut_namei_tests;
extern const struct voluta_ut_tests voluta_ut_rename_tests;
extern const struct voluta_ut_tests voluta_ut_symlink_tests;
extern const struct voluta_ut_tests voluta_ut_xattr_tests;
extern const struct voluta_ut_tests voluta_ut_file_basic_tests;
extern const struct voluta_ut_tests voluta_ut_file_ranges_tests;
extern const struct voluta_ut_tests voluta_ut_file_truncate_tests;
extern const struct voluta_ut_tests voluta_ut_file_records_tests;
extern const struct voluta_ut_tests voluta_ut_file_random_tests;
extern const struct voluta_ut_tests voluta_ut_file_fallocate_tests;
extern const struct voluta_ut_tests voluta_ut_file_edges_tests;


/* common */
void voluta_ut_test_alloc(void);

void voluta_ut_execute(const char *);
void voluta_ut_freeall(struct voluta_ut_ctx *ut_ctx);
void *voluta_ut_malloc(struct voluta_ut_ctx *ut_ctx, size_t nbytes);
void *voluta_ut_zalloc(struct voluta_ut_ctx *ut_ctx, size_t nbytes);
char *voluta_ut_strdup(struct voluta_ut_ctx *ut_ctx, const char *str);
char *voluta_ut_strndup(struct voluta_ut_ctx *ut_ctx, const char *str, size_t);

struct voluta_ut_ctx *voluta_ut_new_ctx(void);
void voluta_ut_del_ctx(struct voluta_ut_ctx *ut_ctx);

void *voluta_ut_zerobuf(struct voluta_ut_ctx *, size_t bsz);
void voluta_ut_randfill(struct voluta_ut_ctx *, void *, size_t);
void *voluta_ut_randbuf(struct voluta_ut_ctx *, size_t bsz);
long *voluta_ut_randseq(struct voluta_ut_ctx *, size_t len, long base);
char *voluta_ut_randstr(struct voluta_ut_ctx *, size_t len);
char *voluta_ut_strfmt(struct voluta_ut_ctx *ut_ctx, const char *fmt, ...);
struct voluta_ut_readdir_ctx *
voluta_ut_new_readdir_ctx(struct voluta_ut_ctx *);
struct voluta_ut_iobuf *
voluta_ut_new_iobuf(struct voluta_ut_ctx *, loff_t, size_t);

/* operations wrappers */
int voluta_ut_load(struct voluta_ut_ctx *ut_ctx);
int voluta_ut_statfs(struct voluta_ut_ctx *ut_ctx, ino_t ino,
		     struct statvfs *st);
int voluta_ut_getattr(struct voluta_ut_ctx *ut_ctx, ino_t ino, struct stat *);
int voluta_ut_utimens(struct voluta_ut_ctx *ut_ctx, ino_t ino,
		      const struct stat *utimes, struct stat *st);
int voluta_ut_access(struct voluta_ut_ctx *ut_ctx, ino_t ino, int mode);
int voluta_ut_lookup(struct voluta_ut_ctx *ut_ctx, ino_t parent,
		     const char *name, struct stat *out_stat);
int voluta_ut_mkdir(struct voluta_ut_ctx *ut_ctx, ino_t parent,
		    const char *name, mode_t mode, struct stat *out);
int voluta_ut_rmdir(struct voluta_ut_ctx *ut_ctx, ino_t parent,
		    const char *name);
int voluta_ut_opendir(struct voluta_ut_ctx *ut_ctx, ino_t ino);
int voluta_ut_releasedir(struct voluta_ut_ctx *ut_ctx, ino_t ino);
int voluta_ut_fsyncdir(struct voluta_ut_ctx *ut_ctx, ino_t ino, bool datasync);
int voluta_ut_readdir(struct voluta_ut_ctx *ut_ctx, ino_t ino,
		      loff_t doff, struct voluta_ut_readdir_ctx *);
int voluta_ut_symlink(struct voluta_ut_ctx *ut_ctx, ino_t parent,
		      const char *name, const char *linkpath, struct stat *);
int voluta_ut_readlink(struct voluta_ut_ctx *ut_ctx, ino_t ino,
		       char *buf, size_t bufsz);
int voluta_ut_link(struct voluta_ut_ctx *ut_ctx, ino_t ino, ino_t parent,
		   const char *name, struct stat *out);
int voluta_ut_unlink(struct voluta_ut_ctx *ut_ctx, ino_t parent,
		     const char *name);
int voluta_ut_rename(struct voluta_ut_ctx *ut_ctx, ino_t parent,
		     const char *name,
		     ino_t newparent, const char *newname, int);
int voluta_ut_create(struct voluta_ut_ctx *ut_ctx, ino_t parent,
		     const char *name,
		     mode_t mode, struct stat *out_stat);
int voluta_ut_mknod(struct voluta_ut_ctx *ut_ctx, ino_t parent,
		    const char *name,
		    mode_t mode, dev_t rdev, struct stat *out_stat);
int voluta_ut_open(struct voluta_ut_ctx *ut_ctx, ino_t ino, int flags);
int voluta_ut_release(struct voluta_ut_ctx *ut_ctx, ino_t ino);
int voluta_ut_fsync(struct voluta_ut_ctx *ut_ctx, ino_t ino, bool datasync);
int voluta_ut_read(struct voluta_ut_ctx *ut_ctx, ino_t ino, void *buf,
		   size_t len, loff_t off, size_t *out_len);
int voluta_ut_write(struct voluta_ut_ctx *ut_ctx, ino_t ino, const void *buf,
		    size_t len, off_t off, size_t *out_len);
int voluta_ut_truncate(struct voluta_ut_ctx *ut_ctx, ino_t ino, loff_t length);
int voluta_ut_fallocate(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			int mode, loff_t offset, loff_t len);
int voluta_ut_setxattr(struct voluta_ut_ctx *ut_ctx, ino_t ino,
		       const char *name, const void *value,
		       size_t size, int flags);
int voluta_ut_getxattr(struct voluta_ut_ctx *ut_ctx, ino_t ino,
		       const char *name, void *buf, size_t size, size_t *out);
int voluta_ut_removexattr(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			  const char *);
int voluta_ut_listxattr(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			struct voluta_ut_listxattr_ctx *ut_listxattr_ctx);

/* no-fail operations wrappers */
void voluta_ut_statfs_ok(struct voluta_ut_ctx *, ino_t, struct statvfs *);
void voluta_ut_statfs_rootd(struct voluta_ut_ctx *, struct statvfs *);
void voluta_ut_getattr_exists(struct voluta_ut_ctx *, ino_t, struct stat *);
void voluta_ut_getattr_file(struct voluta_ut_ctx *, ino_t, struct stat *);
void voluta_ut_getattr_dirsize(struct voluta_ut_ctx *, ino_t ino, loff_t size);
void voluta_ut_utimens_atime(struct voluta_ut_ctx *, ino_t ino,
			     const struct timespec *atime);
void voluta_ut_utimens_mtime(struct voluta_ut_ctx *, ino_t ino,
			     const struct timespec *mtime);
void voluta_ut_lookup_noent(struct voluta_ut_ctx *, ino_t ino,
			    const char *name);
void voluta_ut_make_dir(struct voluta_ut_ctx *, ino_t parent_ino,
			const char *name, ino_t *out_ino);
void voluta_ut_remove_dir(struct voluta_ut_ctx *ut_ctx,
			  ino_t parent_ino, const char *name);
void voluta_ut_noremove_dir(struct voluta_ut_ctx *ut_ctx,
			    ino_t parent_ino, const char *name);
void voluta_ut_lookup_resolve(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			      const char *name, ino_t *out_ino);
void voluta_ut_lookup_exists(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			     const char *name, ino_t ino, mode_t mode);
void voluta_ut_lookup_dir(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			  const char *name, ino_t dino);
void voluta_ut_lookup_reg(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			  const char *name, ino_t ino);
void voluta_ut_lookup_lnk(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			  const char *name, ino_t ino);
void voluta_ut_link_new(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			ino_t parent_ino, const char *name);
void voluta_ut_unlink_exists(struct voluta_ut_ctx *ut_ctx,
			     ino_t parent_ino, const char *name);
void voluta_ut_rename_move(struct voluta_ut_ctx *ut_ctx,
			   ino_t parent_ino, const char *name,
			   ino_t newparent_ino, const char *newname);
void voluta_ut_rename_replace(struct voluta_ut_ctx *ut_ctx,
			      ino_t parent_ino, const char *name,
			      ino_t newparent_ino, const char *newname);
void voluta_ut_rename_exchange(struct voluta_ut_ctx *ut_ctx,
			       ino_t parent_ino, const char *name,
			       ino_t newparent_ino, const char *newname);
void voluta_ut_create_symlink(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			      const char *name, const char *value, ino_t *out);
void voluta_ut_readlink_expect(struct voluta_ut_ctx *ut_ctx,
			       ino_t ino, const char *symval);
void voluta_ut_create_special(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			      const char *name, mode_t mode, ino_t *out_ino);
void voluta_ut_create_file(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			   const char *name, ino_t *out_ino);
void voluta_ut_release_file(struct voluta_ut_ctx *ut_ctx, ino_t ino);
void voluta_ut_fsync_file(struct voluta_ut_ctx *ut_ctx, ino_t ino, bool);
void voluta_ut_remove_file(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			   const char *, ino_t ino);
void voluta_ut_create_only(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			   const char *name, ino_t *out_ino);
void voluta_ut_open_rdonly(struct voluta_ut_ctx *ut_ctx, ino_t ino);
void voluta_ut_remove_link(struct voluta_ut_ctx *ut_ctx, ino_t parent_ino,
			   const char *name);
void voluta_ut_write_read(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			  const void *buf, size_t bsz, loff_t off);
void voluta_ut_write_read_str(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			      const char *str, loff_t off);
void voluta_ut_read_verify(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			   const void *buf, size_t bsz, loff_t off);
void voluta_ut_read_verify_str(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			       const char *str, loff_t off);
void voluta_ut_read_zero(struct voluta_ut_ctx *ut_ctx, ino_t ino, loff_t off);
void voluta_ut_read_zeros(struct voluta_ut_ctx *ut_ctx,
			  ino_t ino, loff_t off, size_t len);
void voluta_ut_read_only(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			 void *buf, size_t bsz, loff_t off);
void voluta_ut_trunacate_file(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			      loff_t len);
void voluta_ut_fallocate_reserve(struct voluta_ut_ctx *ut_ctx, ino_t ino,
				 loff_t offset, loff_t len);
void voluta_ut_fallocate_punch_hole(struct voluta_ut_ctx *ut_ctx, ino_t ino,
				    loff_t offset, loff_t len);
void voluta_ut_setxattr_create(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			       const struct voluta_ut_keyval *kv);
void voluta_ut_getxattr_value(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			      const struct voluta_ut_keyval *kv);
void voluta_ut_getxattr_nodata(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			       const struct voluta_ut_keyval *);
void voluta_ut_removexattr_exists(struct voluta_ut_ctx *ut_ctx, ino_t ino,
				  const struct voluta_ut_keyval *);
void voluta_ut_listxattr_exists(struct voluta_ut_ctx *ut_ctx, ino_t ino,
				const struct voluta_ut_kvl *kvl);
void voluta_ut_setxattr_all(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			    const struct voluta_ut_kvl *kvl);
void voluta_ut_removexattr_all(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			       const struct voluta_ut_kvl *kvl);
void voluta_ut_write_iobuf(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			   const struct voluta_ut_iobuf *iobuf);
void voluta_ut_read_iobuf(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			  const struct voluta_ut_iobuf *iobuf);

void voluta_ut_drop_caches(struct voluta_ut_ctx *ut_ctx);

/* utilities */
void *voluta_ut_mmap_memory(size_t msz);
void voluta_ut_munmap_memory(void *mem, size_t msz);
void voluta_ut_prandom_seq(long *arr, size_t len, long base);

/* assertions */
void voluta_ut_assert_if(int cond, const char *desc, const char *file, int ln);

#define voluta_ut_assert(cond) \
	do { \
		voluta_ut_assert_if(cond, #cond, __FILE__, __LINE__); \
		if (!(cond)) \
			abort(); /* Make clang happy */ \
	} while(0)

#define voluta_ut_assert_ok(ok)         voluta_assert_ok(ok)
#define voluta_ut_assert_err(err, exp)  voluta_assert_err(err, exp)
#define voluta_ut_assert_lt(a, b)       voluta_assert_lt(a, b)
#define voluta_ut_assert_le(a, b)       voluta_assert_le(a, b)
#define voluta_ut_assert_gt(a, b)       voluta_assert_gt(a, b)
#define voluta_ut_assert_ge(a, b)       voluta_assert_ge(a, b)
#define voluta_ut_assert_eq(a, b)       voluta_assert_eq(a, b)
#define voluta_ut_assert_ne(a, b)       voluta_assert_ne(a, b)
#define voluta_ut_assert_not_null(p)    voluta_assert_not_null(p)
#define voluta_ut_assert_eqs(a, b)      voluta_assert_eqs(a, b)
#define voluta_ut_assert_eqm(a, b, n)   voluta_assert_eqm(a, b, n)

/* aliases */
#define UT_DEFTEST(hook)        VOLUTA_UT_DEFTEST(hook)
#define UT_MKTESTS(tag)         VOLUTA_UT_MKTESTS(tag)
#define UT_NAME                 VOLUTA_UT_NAME

#define ut_assert(cond)         voluta_ut_assert(cond)
#define ut_assert_lt(a, b)      voluta_ut_assert_lt(a, b)
#define ut_assert_le(a, b)      voluta_ut_assert_le(a, b)
#define ut_assert_gt(a, b)      voluta_ut_assert_gt(a, b)
#define ut_assert_ge(a, b)      voluta_ut_assert_ge(a, b)
#define ut_assert_eq(a, b)      voluta_ut_assert_eq(a, b)
#define ut_assert_ne(a, b)      voluta_ut_assert_ne(a, b)
#define ut_assert_eqs(a, b)     voluta_ut_assert_eqs(a, b)
#define ut_assert_eqm(a, b, n)  voluta_ut_assert_eqm(a, b, n)
#define ut_assert_ok(err)       voluta_ut_assert_ok(err)
#define ut_assert_err(err, val) voluta_ut_assert_err(err, val)
#define ut_assert_notnull(ptr)  voluta_ut_assert_not_null(ptr)
#define ut_assert_zero(z)       voluta_ut_assert_eq(z, 0)

#define ut_zerobuf(ut_ctx, n)   voluta_ut_zerobuf(ut_ctx, n)
#define ut_biovec(ut_ctx, n)    voluta_ut_new_biovec(ut_ctx, n)
#define ut_randbuf(ut_ctx, n)   voluta_ut_randbuf(ut_ctx, n)
#define ut_randstr(ut_ctx, n)   voluta_ut_randstr(ut_ctx, n)
#define ut_strfmt(ut_ctx, fmt, ...) \
	voluta_ut_strfmt(ut_ctx, fmt, __VA_ARGS__)

#endif /* VOLUTA_UNITEST_H_ */
