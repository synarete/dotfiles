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

#ifndef VOLUTA_FSTESTS_H_
#define VOLUTA_FSTESTS_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <errno.h>

#include <voluta/voluta.h>

/* Re-mapped macros */
#define voluta_t_expect_eqm(a, b, n) voluta_assert_eqm(a, b, n)
#define voluta_t_expect_true(p)     voluta_assert(p)
#define voluta_t_expect_false(p)    voluta_assert(!(p))
#define voluta_t_expect_err(err, x) voluta_assert_err(err, x)
#define voluta_t_expect_eq(a, b)    voluta_assert_eq(a, b)
#define voluta_t_expect_ne(a, b)    voluta_assert_ne(a, b)
#define voluta_t_expect_lt(a, b)    voluta_assert_lt(a, b)
#define voluta_t_expect_le(a, b)    voluta_assert_le(a, b)
#define voluta_t_expect_gt(a, b)    voluta_assert_gt(a, b)
#define voluta_t_expect_ge(a, b)    voluta_assert_ge(a, b)

#define voluta_t_expect_ts_eq(t1, t2) \
	voluta_assert_eq(voluta_t_timespec_diff(t1, t2), 0)
#define voluta_t_expect_ts_gt(t1, t2) \
	voluta_assert_gt(voluta_t_timespec_diff(t1, t2), 0)
#define voluta_t_expect_ts_ge(t1, t2) \
	voluta_assert_ge(voluta_t_timespec_diff(t1, t2), 0)

#define voluta_t_expect_mtime_eq(st1, st2) \
	voluta_t_expect_ts_eq(&((st1)->st_mtim), &((st2)->st_mtim))
#define voluta_t_expect_mtime_gt(st1, st2) \
	voluta_t_expect_ts_gt(&((st1)->st_mtim), &((st2)->st_mtim))
#define voluta_t_expect_ctime_eq(st1, st2) \
	voluta_t_expect_ts_eq(&((st1)->st_ctim), &((st2)->st_ctim))
#define voluta_t_expect_ctime_gt(st1, st2) \
	voluta_t_expect_ts_gt(&((st1)->st_ctim), &((st2)->st_ctim))
#define voluta_t_expect_ctime_ge(st1, st2) \
	voluta_t_expect_ts_ge(&((st1)->st_ctim), &((st2)->st_ctim))


/* Tests control flags */
enum voluta_t_flags {
	VOLUTA_T_NORMAL             = (1 << 1),
	VOLUTA_T_POSIX_EXTRA        = (1 << 2),
	VOLUTA_T_STAVFS             = (1 << 3),
	VOLUTA_T_IO_EXTRA           = (1 << 4),
	VOLUTA_T_IO_CLONE           = (1 << 5),
	VOLUTA_T_IO_TMPFILE         = (1 << 6),
	VOLUTA_T_VERIFY             = (1 << 7),
	VOLUTA_T_RANDOM             = (1 << 8)
};

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct voluta_t_ctx;

struct voluta_t_mchunk {
	struct voluta_t_mchunk *next;
	size_t  size;
	uint8_t data[40];
};


/* Test define */
struct voluta_t_tdef {
	void (*hook)(struct voluta_t_ctx *);
	const char  *name;
	int flags;
};


/* Tests-array define */
struct voluta_t_tests {
	const struct voluta_t_tdef *arr;
	size_t len;
};


/* Tests execution parameters */
struct voluta_t_params {
	const char *progname;
	const char *workdir;
	const char *testname;
	int testsmask;
	int repeatn;
	bool listtests;
};


/* Tests execution context */
struct voluta_t_ctx {
	struct voluta_t_params params;
	const struct voluta_t_tdef *currtest;
	struct statvfs stvfs;
	struct timespec ts_start;
	size_t  seqn;
	time_t  start;
	pid_t   pid;
	uid_t   uid;
	gid_t   gid;
	mode_t  umsk;
	size_t  nbytes_alloc;
	struct voluta_t_mchunk *malloc_list;
	struct voluta_t_tests   tests;
};


/* Sanity-testing utility */
void t_ctx_init(struct voluta_t_ctx *, const struct voluta_t_params *);
void t_ctx_exec(struct voluta_t_ctx *);
void t_ctx_fini(struct voluta_t_ctx *);

void voluta_t_suspend(const struct voluta_t_ctx *, int sec, int part);
void voluta_t_suspends(const struct voluta_t_ctx *, int sec);
char *voluta_t_strdup(struct voluta_t_ctx *, const char *str);
void voluta_t_freeall(struct voluta_t_ctx *);
char *voluta_t_strfmt(struct voluta_t_ctx *, const char *fmt, ...);
char *voluta_t_new_name_unique(struct voluta_t_ctx *);
char *voluta_t_new_path_unique(struct voluta_t_ctx *);
char *voluta_t_new_path_under(struct voluta_t_ctx *, const char *);
char *voluta_t_new_path_name(struct voluta_t_ctx *, const char *);
char *voluta_t_new_path_nested(struct voluta_t_ctx *, const char *,
			       const char *);
char *voluta_t_new_pathf(struct voluta_t_ctx *, const char *, const char *,
			 ...);
void *voluta_t_new_buf_zeros(struct voluta_t_ctx *, size_t bsz);
void *voluta_t_new_buf_rands(struct voluta_t_ctx *, size_t bsz);
void *voluta_t_new_buf_nums(struct voluta_t_ctx *, unsigned long, size_t);
long *voluta_t_new_randseq(struct voluta_t_ctx *t_ctx, size_t len, long);
long voluta_t_lrand(struct voluta_t_ctx *t_ctx);
long voluta_t_timespec_diff(const struct timespec *, const struct timespec *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Sub-tests grouped by topic */
extern const struct voluta_t_tests voluta_t_test_access;
extern const struct voluta_t_tests voluta_t_test_stat;
extern const struct voluta_t_tests voluta_t_test_statvfs;
extern const struct voluta_t_tests voluta_t_test_utimes;
extern const struct voluta_t_tests voluta_t_test_mkdir;
extern const struct voluta_t_tests voluta_t_test_readdir;
extern const struct voluta_t_tests voluta_t_test_create;
extern const struct voluta_t_tests voluta_t_test_open;
extern const struct voluta_t_tests voluta_t_test_link;
extern const struct voluta_t_tests voluta_t_test_unlink;
extern const struct voluta_t_tests voluta_t_test_chmod;
extern const struct voluta_t_tests voluta_t_test_symlink;
extern const struct voluta_t_tests voluta_t_test_mknod;
extern const struct voluta_t_tests voluta_t_test_fsync;
extern const struct voluta_t_tests voluta_t_test_rename;
extern const struct voluta_t_tests voluta_t_test_xattr;
extern const struct voluta_t_tests voluta_t_test_write;
extern const struct voluta_t_tests voluta_t_test_truncate;
extern const struct voluta_t_tests voluta_t_test_lseek;
extern const struct voluta_t_tests voluta_t_test_basic_io;
extern const struct voluta_t_tests voluta_t_test_boundaries;
extern const struct voluta_t_tests voluta_t_test_tmpfile;
extern const struct voluta_t_tests voluta_t_test_stat_io;
extern const struct voluta_t_tests voluta_t_test_sequencial_file;
extern const struct voluta_t_tests voluta_t_test_sparse_file;
extern const struct voluta_t_tests voluta_t_test_random;
extern const struct voluta_t_tests voluta_t_test_unlinked_file;
extern const struct voluta_t_tests voluta_t_test_truncate_io;
extern const struct voluta_t_tests voluta_t_test_fallocate;
extern const struct voluta_t_tests voluta_t_test_seek;
extern const struct voluta_t_tests voluta_t_test_clone;
extern const struct voluta_t_tests voluta_t_test_copy_file_range;
extern const struct voluta_t_tests voluta_t_test_mmap;
extern const struct voluta_t_tests voluta_t_test_namespace;


#define VOLUTA_T_DEFTESTF(fn_, fl_) \
	{ .hook = fn_, .name = VOLUTA_STR(fn_), .flags = fl_ }

#define VOLUTA_T_DEFTEST(fn_) \
	VOLUTA_T_DEFTESTF(fn_, VOLUTA_T_NORMAL)

#define VOLUTA_T_DEFTESTS(a_) \
	{ .arr = a_, .len = VOLUTA_ARRAY_SIZE(a_) }

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Wrapper over system calls */
void voluta_t_syncfs(int);
void voluta_t_fsync(int);
void voluta_t_statvfs(const char *, struct statvfs *);
void voluta_t_statvfs_err(const char *, int);
void voluta_t_fstatvfs(int, struct statvfs *);
void voluta_t_utime(const char *, const struct utimbuf *);
void voluta_t_utimes(const char *, const struct timeval[2]);
void voluta_t_utimensat(int, const char *, const struct timespec [2], int);
void voluta_t_futimens(int fd, const struct timespec times[2]);
void voluta_t_stat(const char *, struct stat *);
void voluta_t_fstat(int, struct stat *);
void voluta_t_fstatat(int, const char *, struct stat *, int);
void voluta_t_fstatat_err(int, const char *, int, int);
void voluta_t_lstat(const char *, struct stat *);
void voluta_t_lstat_err(const char *, int);
void voluta_t_stat_exists(const char *);
void voluta_t_stat_err(const char *, int);
void voluta_t_stat_noent(const char *);
void voluta_t_mkdir(const char *, mode_t);
void voluta_t_mkdir_err(const char *, mode_t, int);
void voluta_t_rmdir(const char *);
void voluta_t_rmdir_err(const char *, int);
void voluta_t_unlink(const char *);
void voluta_t_unlink_err(const char *, int);
void voluta_t_unlink_noent(const char *);
void voluta_t_unlinkat(int, const char *, int);
void voluta_t_open(const char *, int, mode_t, int *);
void voluta_t_open_err(const char *, int, mode_t, int);
void voluta_t_openat(int, const char *, int, mode_t, int *);
void voluta_t_openat_err(int, const char *, int, mode_t, int);
void voluta_t_creat(const char *, mode_t, int *);
void voluta_t_truncate(const char *, loff_t);
void voluta_t_ftruncate(int, loff_t);
void voluta_t_llseek(int, loff_t, int, loff_t *);
void voluta_t_llseek_err(int, loff_t, int, int);
void voluta_t_write(int, const void *, size_t, size_t *);
void voluta_t_pwrite(int, const void *, size_t, loff_t, size_t *);
void voluta_t_pwrite_err(int, const void *, size_t, loff_t, int);
void voluta_t_read(int, void *, size_t, size_t *);
void voluta_t_pread(int, void *, size_t, loff_t, size_t *);
void voluta_t_fallocate(int, int, loff_t, loff_t);
void voluta_t_fallocate_err(int, int, loff_t, loff_t, int);
void voluta_t_fdatasync(int);
void voluta_t_mkfifo(const char *, mode_t);
void voluta_t_mkfifoat(int, const char *, mode_t);
void voluta_t_mknod(const char *, mode_t, dev_t);
void voluta_t_mknodat(int, const char *, mode_t, dev_t);
void voluta_t_symlink(const char *, const char *);
void voluta_t_symlinkat(const char *, int, const char *);
void voluta_t_readlink(const char *, char *, size_t, size_t *);
void voluta_t_readlink_err(const char *, char *, size_t, int);
void voluta_t_readlinkat(int, const char *, char *, size_t, size_t *);
void voluta_t_rename(const char *, const char *);
void voluta_t_rename_err(const char *, const char *, int);
void voluta_t_renameat(int, const char *, int, const char *);
void voluta_t_renameat2(int, const char *, int, const char *, unsigned int);
void voluta_t_link(const char *, const char *);
void voluta_t_link_err(const char *, const char *, int);
void voluta_t_linkat(int, const char *, int, const char *, int);
void voluta_t_linkat_err(int, const char *, int, const char *, int, int);
void voluta_t_chmod(const char *, mode_t);
void voluta_t_fchmod(int, mode_t);
void voluta_t_chown(const char *, uid_t, gid_t);
void voluta_t_fchown(int, uid_t, gid_t);
void voluta_t_access(const char *, int);
void voluta_t_access_err(const char *, int, int);
void voluta_t_close(int);
void voluta_t_mmap(void *, size_t, int, int, int, off_t, void **);
void voluta_t_munmap(void *, size_t);
void voluta_t_msync(void *, size_t, int);
void voluta_t_madvise(void *, size_t, int);
void voluta_t_setxattr(const char *, const char *, const void *, size_t,
		       int);
void voluta_t_lsetxattr(const char *, const char *,
			const void *, size_t, int);
void voluta_t_fsetxattr(int, const char *, const void *, size_t, int);
void voluta_t_getxattr(const char *, const char *, void *, size_t, size_t *);
void voluta_t_getxattr_err(const char *, const char *, int);
void voluta_t_lgetxattr(const char *, const char *, void *, size_t,
			size_t *);
void voluta_t_fgetxattr(int, const char *, void *, size_t, size_t *);
void voluta_t_removexattr(const char *, const char *);
void voluta_t_lremovexattr(const char *, const char *);
void voluta_t_fremovexattr(int, const char *);
void voluta_t_listxattr(const char *, char *, size_t, size_t *);
void voluta_t_llistxattr(const char *, char *, size_t, size_t *);
void voluta_t_flistxattr(int, char *, size_t, size_t *);
void voluta_t_getdent(int, struct dirent64 *);
void voluta_t_getdents(int, void *, size_t,
		       struct dirent64 *, size_t, size_t *);
void voluta_t_copy_file_range(int, loff_t *, int, loff_t *, size_t,
			      unsigned int, loff_t *);
void voluta_t_ioctl_ficlone(int, int);

#endif /* VOLUTA_FSTESTS_H_ */

