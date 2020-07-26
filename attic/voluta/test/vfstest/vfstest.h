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

#ifndef VOLUTA_VFSTEST_H_
#define VOLUTA_VFSTEST_H_

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
#include <voluta/syscall.h>
#include <voluta/error.h>

/* Re-mapped macros */
#define VT_KILO                 VOLUTA_KILO
#define VT_MEGA                 VOLUTA_MEGA
#define VT_GIGA                 VOLUTA_GIGA
#define VT_TERA                 VOLUTA_TERA
#define VT_PETA                 VOLUTA_PETA

#define VT_UKILO                VOLUTA_UKILO
#define VT_UMEGA                VOLUTA_UMEGA
#define VT_UGIGA                VOLUTA_UGIGA
#define VT_UTERA                VOLUTA_UTERA
#define VT_UPETA                VOLUTA_UPETA

#define VT_FRGSIZE              (512) /* Fragment size (see stat(2)) */
#define VT_BK_SIZE              VOLUTA_BK_SIZE
#define VT_FILEMAP_NCHILD       VOLUTA_FILE_MAP_NCHILD
#define VT_FILESIZE_MAX         VOLUTA_FILE_SIZE_MAX
#define VT_IOSIZE_MAX           VOLUTA_IO_SIZE_MAX
#define VT_STR(x_)              VOLUTA_STR(x_)
#define VT_ARRAY_SIZE(x_)       VOLUTA_ARRAY_SIZE(x_)


#define vt_expect_eqm(a, b, n)  voluta_assert_eqm(a, b, n)
#define vt_expect_true(p)       voluta_assert(p)
#define vt_expect_false(p)      voluta_assert(!(p))
#define vt_expect_err(err, x)   voluta_assert_err(err, x)
#define vt_expect_eq(a, b)      voluta_assert_eq(a, b)
#define vt_expect_ne(a, b)      voluta_assert_ne(a, b)
#define vt_expect_lt(a, b)      voluta_assert_lt(a, b)
#define vt_expect_le(a, b)      voluta_assert_le(a, b)
#define vt_expect_gt(a, b)      voluta_assert_gt(a, b)
#define vt_expect_ge(a, b)      voluta_assert_ge(a, b)

#define vt_expect_ts_eq(t1, t2) \
	voluta_assert_eq(vt_timespec_diff(t1, t2), 0)
#define vt_expect_ts_gt(t1, t2) \
	voluta_assert_gt(vt_timespec_diff(t1, t2), 0)
#define vt_expect_ts_ge(t1, t2) \
	voluta_assert_ge(vt_timespec_diff(t1, t2), 0)

#define vt_expect_mtime_eq(st1, st2) \
	vt_expect_ts_eq(&((st1)->st_mtim), &((st2)->st_mtim))
#define vt_expect_mtime_gt(st1, st2) \
	vt_expect_ts_gt(&((st1)->st_mtim), &((st2)->st_mtim))
#define vt_expect_ctime_eq(st1, st2) \
	vt_expect_ts_eq(&((st1)->st_ctim), &((st2)->st_ctim))
#define vt_expect_ctime_gt(st1, st2) \
	vt_expect_ts_gt(&((st1)->st_ctim), &((st2)->st_ctim))
#define vt_expect_ctime_ge(st1, st2) \
	vt_expect_ts_ge(&((st1)->st_ctim), &((st2)->st_ctim))

#define vt_expect_dir(m)        vt_expect_true(S_ISDIR(m))
#define vt_expect_reg(m)        vt_expect_true(S_ISREG(m))
#define vt_expect_lnk(m)        vt_expect_true(S_ISLNK(m))


/* Tests control flags */
enum vt_flags {
	VT_IGNORE             = (1 << 1),
	VT_NORMAL             = (1 << 2),
	VT_POSIX_EXTRA        = (1 << 3),
	VT_STAVFS             = (1 << 4),
	VT_IO_EXTRA           = (1 << 5),
	VT_IO_TMPFILE         = (1 << 6),
	VT_VERIFY             = (1 << 7),
	VT_RANDOM             = (1 << 8),
};

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct vt_env;

struct vt_mchunk {
	struct vt_mchunk *next;
	size_t  size;
	uint8_t data[40];
};


/* Test define */
struct vt_tdef {
	void (*hook)(struct vt_env *);
	const char  *name;
	int flags;
	int pad;
};


/* Tests-array define */
struct vt_tests {
	const struct vt_tdef *arr;
	size_t len;
};


/* Tests execution parameters */
struct vt_params {
	const char *progname;
	const char *workdir;
	const char *testname;
	int testsmask;
	int repeatn;
	int listtests;
	int pad;
};


/* Tests execution environment context */
struct vt_env {
	struct vt_params params;
	const struct vt_tdef *currtest;
	struct statvfs stvfs;
	struct timespec ts_start;
	size_t  seqn;
	time_t  start;
	pid_t   pid;
	uid_t   uid;
	gid_t   gid;
	mode_t  umsk;
	size_t  nbytes_alloc;
	struct vt_mchunk *malloc_list;
	struct vt_tests   tests;
};


/* Sanity-testing utility */
void vt_env_init(struct vt_env *vt_env, const struct vt_params *params);
void vt_env_exec(struct vt_env *);
void vt_env_fini(struct vt_env *);

void vt_suspend(const struct vt_env *, int sec, int part);
void vt_suspends(const struct vt_env *, int sec);
char *vt_strdup(struct vt_env *, const char *str);
void vt_freeall(struct vt_env *);
char *vt_strfmt(struct vt_env *, const char *fmt, ...);
char *vt_new_name_unique(struct vt_env *);
char *vt_new_path_unique(struct vt_env *);
char *vt_new_path_under(struct vt_env *, const char *);
char *vt_new_path_name(struct vt_env *, const char *);
char *vt_new_path_nested(struct vt_env *, const char *,
			 const char *);
char *vt_new_pathf(struct vt_env *, const char *, const char *,
		   ...);
void *vt_new_buf_zeros(struct vt_env *, size_t bsz);
void *vt_new_buf_rands(struct vt_env *, size_t bsz);
void *vt_new_buf_nums(struct vt_env *, unsigned long, size_t);
long *vt_new_randseq(struct vt_env *vt_env, size_t len, long);
long vt_lrand(struct vt_env *vt_env);
long vt_timespec_diff(const struct timespec *, const struct timespec *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Wrapper over system calls */
void vt_syncfs(int fd);
void vt_fsync(int fd);
void vt_statvfs(const char *path, struct statvfs *stv);
void vt_statvfs_err(const char *path, int err);
void vt_fstatvfs(int, struct statvfs *);
void vt_utime(const char *, const struct utimbuf *);
void vt_utimes(const char *, const struct timeval[2]);
void vt_utimensat(int, const char *, const struct timespec [2], int);
void vt_futimens(int fd, const struct timespec times[2]);
void vt_stat(const char *, struct stat *);
void vt_fstat(int, struct stat *);
void vt_fstatat(int, const char *, struct stat *, int);
void vt_fstatat_err(int, const char *, int, int);
void vt_lstat(const char *, struct stat *);
void vt_lstat_err(const char *, int);
void vt_stat_exists(const char *);
void vt_stat_err(const char *, int);
void vt_stat_noent(const char *);
void vt_mkdir(const char *path, mode_t mode);
void vt_mkdir_err(const char *path, mode_t mode, int err);
void vt_mkdirat(int dirfd, const char *pathname, mode_t mode);
void vt_rmdir(const char *);
void vt_rmdir_err(const char *, int);
void vt_unlink(const char *);
void vt_unlink_err(const char *, int);
void vt_unlink_noent(const char *);
void vt_unlinkat(int, const char *, int);
void vt_open(const char *, int, mode_t, int *);
void vt_open_err(const char *, int, mode_t, int);
void vt_openat(int, const char *, int, mode_t, int *);
void vt_openat_err(int, const char *, int, mode_t, int);
void vt_creat(const char *, mode_t, int *);
void vt_truncate(const char *, loff_t);
void vt_ftruncate(int, loff_t);
void vt_llseek(int, loff_t, int, loff_t *);
void vt_llseek_err(int, loff_t, int, int);
void vt_write(int, const void *, size_t, size_t *);
void vt_pwrite(int, const void *, size_t, loff_t, size_t *);
void vt_pwrite_err(int, const void *, size_t, loff_t, int);
void vt_read(int, void *, size_t, size_t *);
void vt_pread(int, void *, size_t, loff_t, size_t *);
void vt_fallocate(int, int, loff_t, loff_t);
void vt_fallocate_err(int, int, loff_t, loff_t, int);
void vt_fdatasync(int);
void vt_mkfifo(const char *, mode_t);
void vt_mkfifoat(int, const char *, mode_t);
void vt_mknod(const char *, mode_t, dev_t);
void vt_mknodat(int, const char *, mode_t, dev_t);
void vt_symlink(const char *, const char *);
void vt_symlinkat(const char *, int, const char *);
void vt_readlink(const char *, char *, size_t, size_t *);
void vt_readlink_err(const char *, char *, size_t, int);
void vt_readlinkat(int, const char *, char *, size_t, size_t *);
void vt_rename(const char *, const char *);
void vt_rename_err(const char *, const char *, int);
void vt_renameat(int, const char *, int, const char *);
void vt_renameat2(int, const char *, int, const char *, unsigned int);
void vt_link(const char *, const char *);
void vt_link_err(const char *, const char *, int);
void vt_linkat(int, const char *, int, const char *, int);
void vt_linkat_err(int, const char *, int, const char *, int, int);
void vt_chmod(const char *, mode_t);
void vt_fchmod(int, mode_t);
void vt_chown(const char *, uid_t, gid_t);
void vt_fchown(int, uid_t, gid_t);
void vt_access(const char *, int);
void vt_access_err(const char *, int, int);
void vt_close(int);
void vt_mmap(void *, size_t, int, int, int, off_t, void **);
void vt_munmap(void *, size_t);
void vt_msync(void *, size_t, int);
void vt_madvise(void *, size_t, int);
void vt_setxattr(const char *, const char *, const void *, size_t, int);
void vt_lsetxattr(const char *, const char *,
		  const void *, size_t, int);
void vt_fsetxattr(int, const char *, const void *, size_t, int);
void vt_getxattr(const char *, const char *, void *, size_t, size_t *);
void vt_getxattr_err(const char *, const char *, int);
void vt_lgetxattr(const char *, const char *, void *, size_t,
		  size_t *);
void vt_fgetxattr(int, const char *, void *, size_t, size_t *);
void vt_removexattr(const char *, const char *);
void vt_lremovexattr(const char *, const char *);
void vt_fremovexattr(int, const char *);
void vt_listxattr(const char *, char *, size_t, size_t *);
void vt_llistxattr(const char *, char *, size_t, size_t *);
void vt_flistxattr(int, char *, size_t, size_t *);
void vt_getdent(int, struct dirent64 *);
void vt_getdents(int, void *, size_t,
		 struct dirent64 *, size_t, size_t *);
void vt_copy_file_range(int, loff_t *, int, loff_t *, size_t,
			unsigned int, loff_t *);
void vt_ioctl_ficlone(int, int);
void vt_fiemap(int fd, struct fiemap *fm);

/* Complex wrappers */
void vt_readn(int fd, void *buf, size_t cnt);
void vt_preadn(int fd, void *buf, size_t cnt, loff_t offset);
void vt_writen(int fd, const void *buf, size_t cnt);
void vt_pwriten(int fd, const void *buf, size_t cnt, loff_t offset);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Sub-tests grouped by topic */
extern const struct vt_tests vt_test_access;
extern const struct vt_tests vt_test_stat;
extern const struct vt_tests vt_test_statvfs;
extern const struct vt_tests vt_test_utimes;
extern const struct vt_tests vt_test_mkdir;
extern const struct vt_tests vt_test_readdir;
extern const struct vt_tests vt_test_create;
extern const struct vt_tests vt_test_open;
extern const struct vt_tests vt_test_link;
extern const struct vt_tests vt_test_unlink;
extern const struct vt_tests vt_test_chmod;
extern const struct vt_tests vt_test_symlink;
extern const struct vt_tests vt_test_mknod;
extern const struct vt_tests vt_test_fsync;
extern const struct vt_tests vt_test_rename;
extern const struct vt_tests vt_test_xattr;
extern const struct vt_tests vt_test_write;
extern const struct vt_tests vt_test_truncate;
extern const struct vt_tests vt_test_lseek;
extern const struct vt_tests vt_test_fiemap;
extern const struct vt_tests vt_test_basic_io;
extern const struct vt_tests vt_test_boundaries;
extern const struct vt_tests vt_test_tmpfile;
extern const struct vt_tests vt_test_stat_io;
extern const struct vt_tests vt_test_sequencial_file;
extern const struct vt_tests vt_test_sparse_file;
extern const struct vt_tests vt_test_random;
extern const struct vt_tests vt_test_unlinked_file;
extern const struct vt_tests vt_test_truncate_io;
extern const struct vt_tests vt_test_fallocate;
extern const struct vt_tests vt_test_clone;
extern const struct vt_tests vt_test_copy_file_range;
extern const struct vt_tests vt_test_mmap;
extern const struct vt_tests vt_test_namespace;

/* Test-define helper macros */
#define VT_DEFTESTF(fn_, fl_) \
	{ .hook = (fn_), .name = VT_STR(fn_), .flags = (fl_) }

#define VT_DEFTEST(fn_) \
	VT_DEFTESTF(fn_, VT_NORMAL)

#define VT_DEFTESTS(a_) \
	{ .arr = (a_), .len = VT_ARRAY_SIZE(a_) }

#endif /* VOLUTA_VFSTEST_H_ */

