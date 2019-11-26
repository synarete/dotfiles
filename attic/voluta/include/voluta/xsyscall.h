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
#ifndef VOLUTA_XSYSCALL_H_
#define VOLUTA_XSYSCALL_H_

#include <unistd.h>
#include <stdlib.h>
#include <time.h>

struct stat;
struct statvfs;
struct statfs;
struct dirent64;
struct iovec;
struct utimbuf;
struct timeval;
struct timespec;
struct sigaction;
struct rlimit;

int voluta_sys_access(const char *path, int mode);
int voluta_sys_faccessat(int dirfd, const char *pathname, int mode, int flags);
int voluta_sys_link(const char *path1, const char *path2);
int voluta_sys_linkat(int, const char *, int, const char *newpath, int flags);
int voluta_sys_unlink(const char *path);
int voluta_sys_unlinkat(int dirfd, const char *pathname, int flags);
int voluta_sys_rename(const char *oldpath, const char *newpath);
int voluta_sys_renameat(int, const char *, int, const char *);
int voluta_sys_renameat2(int, const char *, int, const char *, unsigned int);
int voluta_sys_fstatvfs(int fd, struct statvfs *);
int voluta_sys_statfs(const char *path, struct statfs *);
int voluta_sys_fstatfs(int fd, struct statfs *);
int voluta_sys_flock(int fd, int operation);
int voluta_sys_statvfs(const char *path, struct statvfs *stv);
int voluta_sys_fstat(int fd, struct stat *st);
int voluta_sys_fstatat(int dirfd, const char *pathname, struct stat *st, int);
int voluta_sys_stat(const char *path, struct stat *st);
int voluta_sys_lstat(const char *path, struct stat *st);
int voluta_sys_chmod(const char *path, mode_t mode);
int voluta_sys_fchmod(int fd, mode_t mode);
int voluta_sys_fchmodat(int dirfd, const char *pathname, mode_t mode,
			int flags);
int voluta_sys_chown(const char *path, uid_t uid, gid_t gid);
int voluta_sys_fchown(int fd, uid_t uid, gid_t gid);
int voluta_sys_fchownat(int dirfd, const char *path, uid_t uid, gid_t gid, int);
int voluta_sys_utime(const char *filename, const struct utimbuf *times);
int voluta_sys_utimes(const char *filename, const struct timeval times[2]);
int voluta_sys_utimensat(int dirfd, const char *pathname,
			 const struct timespec times[2], int flags);
int voluta_sys_futimens(int fd, const struct timespec times[2]);
int voluta_sys_mkdir(const char *path, mode_t mode);
int voluta_sys_mkdirat(int dirfd, const char *pathname, mode_t mode);
int voluta_sys_rmdir(const char *path);
int voluta_sys_getdent(int fd, loff_t off, struct dirent64 *dent);
int voluta_sys_getdents(int fd, void *buf, size_t bsz, struct dirent64 *dents,
			size_t ndents, size_t *out_ndents);
int voluta_sys_creat(const char *path, mode_t mode, int *fd);
int voluta_sys_memfd_create(const char *name, unsigned int flags, int *fd);
int voluta_sys_open(const char *path, int flags, mode_t mode, int *fd);
int voluta_sys_openat(int dirfd, const char *, int flags, mode_t mode, int *fd);
int voluta_sys_close(int fd);
int voluta_sys_llseek(int fd, loff_t off, int whence, loff_t *pos);
int voluta_sys_syncfs(int fd);
int voluta_sys_fsync(int fd);
int voluta_sys_fdatasync(int fd);
int voluta_sys_fallocate(int fd, int mode, loff_t off, loff_t len);
int voluta_sys_truncate(const char *path, loff_t len);
int voluta_sys_ftruncate(int fd, loff_t len);
int voluta_sys_readlink(const char *path, char *buf, size_t bsz, size_t *nwr);
int voluta_sys_readlinkat(int dirfd, const char *pathname,
			  char *buf, size_t bsz, size_t *cnt);
int voluta_sys_symlink(const char *oldpath, const char *newpath);
int voluta_sys_symlinkat(const char *target, int dirfd, const char *linkpath);
int voluta_sys_mkfifo(const char *path, mode_t mode);
int voluta_sys_mkfifoat(int dirfd, const char *pathname, mode_t mode);
int voluta_sys_mknod(const char *pathname, mode_t mode, dev_t dev);
int voluta_sys_mknodat(int dirfd, const char *pathname, mode_t mode, dev_t dev);
int voluta_sys_mmap(void *addr, size_t length, int prot, int flags,
		    int fd, off_t offset, void **out_addr);
int voluta_sys_mmap_anon(size_t length, int flags, void **out_addr);
int voluta_sys_munmap(void *addr, size_t length);
int voluta_sys_msync(void *addr, size_t len, int flags);
int voluta_sys_madvise(void *addr, size_t len, int advice);
int voluta_sys_mlock(const void *addr, size_t len);
int voluta_sys_mlock2(const void *addr, size_t len, unsigned int flags);
int voluta_sys_munlock(const void *addr, size_t len);
int voluta_sys_mlockall(int flags);
int voluta_sys_munlockall(void);
int voluta_sys_brk(void *addr);
int voluta_sys_sbrk(intptr_t increment, void **out_addr);
int voluta_sys_ioctl_blkgetsize64(int fd, size_t *sz);
int voluta_sys_ioctl_ficlone(int dest_fd, int src_fd);
int voluta_sys_copy_file_range(int fd_in, loff_t *off_in, int fd_out,
			       loff_t *, size_t, unsigned int, loff_t *);
int voluta_sys_read(int fd, void *buf, size_t cnt, size_t *);
int voluta_sys_pread(int fd, void *buf, size_t cnt, loff_t off, size_t *);
int voluta_sys_write(int fd, const void *buf, size_t cnt, size_t *);
int voluta_sys_pwrite(int fd, const void *buf, size_t cnt, loff_t, size_t *);
int voluta_sys_readv(int fd, const struct iovec *iov, int iovcnt, size_t *);
int voluta_sys_writev(int fd, const struct iovec *iov, int iovcnt, size_t *);
int voluta_sys_preadv(int fd, const struct iovec *iov, int iovcnt,
		      off_t off, size_t *);
int voluta_sys_pwritev(int fd, const struct iovec *iov, int iovcnt,
		       off_t off, size_t *);
int voluta_sys_preadv2(int fd, const struct iovec *iov, int iovcnt,
		       off_t off, int flags, size_t *);
int voluta_sys_pwritev2(int fd, const struct iovec *iov, int iovcnt,
			off_t off, int flags, size_t *);

int voluta_sys_setxattr(const char *path, const char *name,
			const void *value, size_t size, int flags);
int voluta_sys_lsetxattr(const char *path, const char *name,
			 const void *value, size_t size, int flags);
int voluta_sys_fsetxattr(int fd, const char *name,
			 const void *value, size_t size, int flags);
int voluta_sys_getxattr(const char *path, const char *name,
			void *value, size_t size, size_t *cnt);
int voluta_sys_lgetxattr(const char *path, const char *name,
			 void *value, size_t size, size_t *cnt);
int voluta_sys_fgetxattr(int fd, const char *name,
			 void *value, size_t size, size_t *cnt);
int voluta_sys_removexattr(const char *path, const char *name);
int voluta_sys_lremovexattr(const char *path, const char *name);
int voluta_sys_fremovexattr(int fd, const char *name);
int voluta_sys_listxattr(const char *path, char *list, size_t size, size_t *);
int voluta_sys_llistxattr(const char *path, char *list, size_t size, size_t *);
int voluta_sys_flistxattr(int fd, char *list, size_t size, size_t *out);

int voluta_sys_sigaction(int, const struct sigaction *, struct sigaction *);

int voluta_sys_getrlimit(int resource, struct rlimit *rlim);
int voluta_sys_setrlimit(int resource, const struct rlimit *rlim);
int voluta_sys_prctl(int option, unsigned long arg2, unsigned long arg3,
		     unsigned long arg4, unsigned long arg5);

int voluta_sys_clock_gettime(clockid_t, struct timespec *);

#endif /* VOLUTA_XSYSCALL_H_ */

