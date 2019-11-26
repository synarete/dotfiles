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

#define _GNU_SOURCE 1
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <error.h>
#include <errno.h>
#include <time.h>
#include "fstests.h"


#define expect_ok(err)          do_expect_ok(err, __func__)
#define expect_err(err, x)      do_expect_err(err, x, __func__)


static const char *syscall_name(const char *fn)
{
	const char *prefix = "voluta_t_";
	const size_t preflen = strlen(prefix);

	if (!strncmp(prefix, fn, preflen)) {
		fn += preflen;
	}
	return fn;
}

static void do_expect_ok(int err, const char *fn)
{
	if (err != 0) {
		voluta_panic("%s ==> %d", syscall_name(fn), err);
	}
}

static void do_expect_err(int err, int exp, const char *fn)
{
	if (err != exp) {
		voluta_panic("%s ==> %d (!%d)", syscall_name(fn), err, exp);
	}
}

void voluta_t_syncfs(int fd)
{
	expect_ok(voluta_sys_syncfs(fd));
}

void voluta_t_fsync(int fd)
{
	expect_ok(voluta_sys_fsync(fd));
}

void voluta_t_stat(const char *path, struct stat *st)
{
	expect_ok(voluta_sys_stat(path, st));
}

void voluta_t_stat_exists(const char *path)
{
	struct stat st;

	voluta_t_stat(path, &st);
}

void voluta_t_stat_err(const char *path, int err)
{
	struct stat st;

	expect_err(voluta_sys_stat(path, &st), err);
}

void voluta_t_stat_noent(const char *path)
{
	struct stat st;

	expect_err(voluta_sys_stat(path, &st), -ENOENT);
}

void voluta_t_fstat(int fd, struct stat *st)
{
	expect_ok(voluta_sys_fstat(fd, st));
}

void voluta_t_lstat(const char *path, struct stat *st)
{
	expect_ok(voluta_sys_lstat(path, st));
}

void voluta_t_fstatat(int dirfd, const char *path, struct stat *st, int flags)
{
	expect_ok(voluta_sys_fstatat(dirfd, path, st, flags));
}

void voluta_t_fstatat_err(int dirfd, const char *path, int flags, int err)
{
	struct stat st;

	expect_err(voluta_sys_fstatat(dirfd, path, &st, flags), err);
}

void voluta_t_lstat_err(const char *path, int err)
{
	struct stat st;

	expect_err(voluta_sys_lstat(path, &st), err);
}

void voluta_t_statvfs(const char *path, struct statvfs *stvfs)
{
	expect_ok(voluta_sys_statvfs(path, stvfs));
}

void voluta_t_statvfs_err(const char *path, int err)
{
	struct statvfs stvfs;

	expect_err(voluta_sys_statvfs(path, &stvfs), err);
}

void voluta_t_fstatvfs(int fd, struct statvfs *stvfs)
{
	expect_ok(voluta_sys_fstatvfs(fd, stvfs));
}

void voluta_t_utime(const char *filename, const struct utimbuf *times)
{
	expect_ok(voluta_sys_utime(filename, times));
}

void voluta_t_utimes(const char *filename, const struct timeval times[2])
{
	expect_ok(voluta_sys_utimes(filename, times));
}

void voluta_t_utimensat(int dirfd, const char *pathname,
			const struct timespec times[2], int flags)
{
	expect_ok(voluta_sys_utimensat(dirfd, pathname, times, flags));
}

void voluta_t_futimens(int fd, const struct timespec times[2])
{
	expect_ok(voluta_sys_futimens(fd, times));
}

void voluta_t_mkdir(const char *path, mode_t mode)
{
	expect_ok(voluta_sys_mkdir(path, mode));
}

void voluta_t_mkdir_err(const char *path, mode_t mode, int err)
{
	expect_err(voluta_sys_mkdir(path, mode), err);
}

void voluta_t_rmdir(const char *path)
{
	expect_ok(voluta_sys_rmdir(path));
}

void voluta_t_rmdir_err(const char *path, int err)
{
	expect_err(voluta_sys_rmdir(path), err);
}

void voluta_t_unlink(const char *path)
{
	expect_ok(voluta_sys_unlink(path));
}

void voluta_t_unlink_err(const char *path, int err)
{
	expect_err(voluta_sys_unlink(path), err);
}

void voluta_t_unlink_noent(const char *path)
{
	voluta_t_unlink_err(path, -ENOENT);
}

void voluta_t_unlinkat(int dirfd, const char *pathname, int flags)
{
	expect_ok(voluta_sys_unlinkat(dirfd, pathname, flags));
}

void voluta_t_open(const char *path, int flags, mode_t mode, int *fd)
{
	expect_ok(voluta_sys_open(path, flags, mode, fd));
}

void voluta_t_open_err(const char *path, int flags, mode_t mode, int err)
{
	int fd;

	expect_err(voluta_sys_open(path, flags, mode, &fd), err);
}

void voluta_t_openat(int dirfd, const char *path,
		     int flags, mode_t mode, int *fd)
{
	expect_ok(voluta_sys_openat(dirfd, path, flags, mode, fd));
}

void voluta_t_openat_err(int dirfd, const char *path,
			 int flags, mode_t mode, int err)
{
	int fd;

	expect_err(voluta_sys_openat(dirfd, path, flags, mode, &fd), err);
}


void voluta_t_creat(const char *path, mode_t mode, int *fd)
{
	expect_ok(voluta_sys_creat(path, mode, fd));
}

void voluta_t_close(int fd)
{
	expect_ok(voluta_sys_close(fd));
}

void voluta_t_truncate(const char *path, loff_t len)
{
	expect_ok(voluta_sys_truncate(path, len));
}

void voluta_t_ftruncate(int fd, loff_t len)
{
	expect_ok(voluta_sys_ftruncate(fd, len));
}

void voluta_t_llseek(int fd, loff_t off, int whence, loff_t *pos)
{
	expect_ok(voluta_sys_llseek(fd, off, whence, pos));
}

void voluta_t_llseek_err(int fd, loff_t off, int whence, int err)
{
	loff_t pos;

	expect_err(voluta_sys_llseek(fd, off, whence, &pos), err);
}

void voluta_t_write(int fd, const void *buf, size_t cnt, size_t *nwr)
{
	expect_ok(voluta_sys_write(fd, buf, cnt, nwr));
}

void voluta_t_pwrite(int fd, const void *buf, size_t cnt,
		     loff_t off, size_t *nwr)
{
	expect_ok(voluta_sys_pwrite(fd, buf, cnt, off, nwr));
}

void voluta_t_pwrite_err(int fd, const void *buf,
			 size_t cnt, loff_t off, int err)
{
	size_t nwr;

	expect_err(voluta_sys_pwrite(fd, buf, cnt, off, &nwr), err);
}

void voluta_t_read(int fd, void *buf, size_t cnt, size_t *nrd)
{
	expect_ok(voluta_sys_read(fd, buf, cnt, nrd));
}

void voluta_t_pread(int fd, void *buf, size_t cnt, loff_t off, size_t *nrd)
{
	expect_ok(voluta_sys_pread(fd, buf, cnt, off, nrd));
}

void voluta_t_fallocate(int fd, int mode, loff_t off, loff_t len)
{
	expect_ok(voluta_sys_fallocate(fd, mode, off, len));
}

void voluta_t_fallocate_err(int fd, int mode, loff_t off, loff_t len, int err)
{
	expect_err(voluta_sys_fallocate(fd, mode, off, len), err);
}

void voluta_t_fdatasync(int fd)
{
	expect_ok(voluta_sys_fdatasync(fd));
}

void voluta_t_mkfifo(const char *path, mode_t mode)
{
	expect_ok(voluta_sys_mkfifo(path, mode));
}

void voluta_t_mkfifoat(int dirfd, const char *pathname, mode_t mode)
{
	expect_ok(voluta_sys_mkfifoat(dirfd, pathname, mode));
}

void voluta_t_mknod(const char *pathname, mode_t mode, dev_t dev)
{
	expect_ok(voluta_sys_mknod(pathname, mode, dev));
}

void voluta_t_mknodat(int dirfd, const char *pathname, mode_t mode, dev_t dev)
{
	expect_ok(voluta_sys_mknodat(dirfd, pathname, mode, dev));
}

void voluta_t_symlink(const char *oldpath, const char *newpath)
{
	expect_ok(voluta_sys_symlink(oldpath, newpath));
}

void voluta_t_symlinkat(const char *target, int dirfd, const char *linkpath)
{
	expect_ok(voluta_sys_symlinkat(target, dirfd, linkpath));
}

void voluta_t_readlink(const char *path, char *buf, size_t bsz, size_t *cnt)
{
	expect_ok(voluta_sys_readlink(path, buf, bsz, cnt));
}

void voluta_t_readlink_err(const char *path, char *buf, size_t bsz, int err)
{
	size_t cnt;

	expect_err(voluta_sys_readlink(path, buf, bsz, &cnt), err);
}

void voluta_t_readlinkat(int dirfd, const char *pathname,
			 char *buf, size_t bsz, size_t *cnt)
{
	expect_ok(voluta_sys_readlinkat(dirfd, pathname, buf, bsz, cnt));
}

void voluta_t_rename(const char *oldpath, const char *newpath)
{
	expect_ok(voluta_sys_rename(oldpath, newpath));
}

void voluta_t_rename_err(const char *oldpath, const char *newpath, int err)
{
	expect_err(voluta_sys_rename(oldpath, newpath), err);
}

void voluta_t_renameat(int olddirfd, const char *oldpath,
		       int newdirfd, const char *newpath)
{
	expect_ok(voluta_sys_renameat(olddirfd, oldpath, newdirfd, newpath));
}

void voluta_t_renameat2(int olddirfd, const char *oldpath,
			int newdirfd, const char *newpath,
			unsigned int flags)
{
	expect_ok(voluta_sys_renameat2(olddirfd, oldpath,
				       newdirfd, newpath, flags));
}

void voluta_t_link(const char *path1, const char *path2)
{
	expect_ok(voluta_sys_link(path1, path2));
}

void voluta_t_link_err(const char *path1, const char *path2, int err)
{
	expect_err(voluta_sys_link(path1, path2), err);
}

void voluta_t_linkat(int olddirfd, const char *oldpath,
		     int newdirfd, const char *newpath, int flags)
{
	expect_ok(voluta_sys_linkat(olddirfd, oldpath,
				    newdirfd, newpath, flags));
}

void voluta_t_linkat_err(int olddirfd, const char *oldpath,
			 int newdirfd, const char *newpath,
			 int flags, int err)
{
	expect_err(voluta_sys_linkat(olddirfd, oldpath,
				     newdirfd, newpath, flags), err);
}

void voluta_t_chmod(const char *path, mode_t mode)
{
	expect_ok(voluta_sys_chmod(path, mode));
}

void voluta_t_fchmod(int fd, mode_t mode)
{
	expect_ok(voluta_sys_fchmod(fd, mode));
}

void voluta_t_chown(const char *path, uid_t uid, gid_t gid)
{
	expect_ok(voluta_sys_chown(path, uid, gid));
}

void voluta_t_fchown(int fd, uid_t uid, gid_t gid)
{
	expect_ok(voluta_sys_fchown(fd, uid, gid));
}

void voluta_t_access(const char *path, int mode)
{
	expect_ok(voluta_sys_access(path, mode));
}

void voluta_t_access_err(const char *path, int mode, int err)
{
	expect_err(voluta_sys_access(path, mode), err);
}

void voluta_t_mmap(void *addr, size_t length, int prot, int flags,
		   int fd, off_t offset, void **out)
{
	expect_ok(voluta_sys_mmap(addr, length, prot, flags, fd, offset, out));
}

void voluta_t_munmap(void *addr, size_t length)
{
	expect_ok(voluta_sys_munmap(addr, length));
}

void voluta_t_msync(void *addr, size_t len, int flags)
{
	expect_ok(voluta_sys_msync(addr, len, flags));
}

void voluta_t_madvise(void *addr, size_t len, int advice)
{
	expect_ok(voluta_sys_madvise(addr, len, advice));
}

void voluta_t_setxattr(const char *path, const char *name,
		       const void *value, size_t size, int flags)
{
	expect_ok(voluta_sys_setxattr(path, name, value, size, flags));
}

void voluta_t_lsetxattr(const char *path, const char *name,
			const void *value, size_t size, int flags)
{
	expect_ok(voluta_sys_lsetxattr(path, name, value, size, flags));
}

void voluta_t_fsetxattr(int fd, const char *name,
			const void *value, size_t size, int flags)
{
	expect_ok(voluta_sys_fsetxattr(fd, name, value, size, flags));
}

void voluta_t_getxattr(const char *path, const char *name,
		       void *value, size_t size, size_t *cnt)
{
	expect_ok(voluta_sys_getxattr(path, name, value, size, cnt));
}

void voluta_t_getxattr_err(const char *path, const char *name, int err)
{
	size_t cnt;

	expect_err(voluta_sys_getxattr(path, name, NULL, 0, &cnt), err);
}

void voluta_t_lgetxattr(const char *path, const char *name,
			void *value, size_t size, size_t *cnt)
{
	expect_ok(voluta_sys_lgetxattr(path, name, value, size, cnt));
}

void voluta_t_fgetxattr(int fd, const char *name,
			void *value, size_t size, size_t *cnt)
{
	expect_ok(voluta_sys_fgetxattr(fd, name, value, size, cnt));
}

void voluta_t_removexattr(const char *path, const char *name)
{
	expect_ok(voluta_sys_removexattr(path, name));
}

void voluta_t_lremovexattr(const char *path, const char *name)
{
	expect_ok(voluta_sys_lremovexattr(path, name));
}

void voluta_t_fremovexattr(int fd, const char *name)
{
	expect_ok(voluta_sys_fremovexattr(fd, name));
}

void voluta_t_listxattr(const char *path, char *list,
			size_t size, size_t *out)
{
	expect_ok(voluta_sys_listxattr(path, list, size, out));
}

void voluta_t_llistxattr(const char *path, char *list,
			 size_t size, size_t *out)
{
	expect_ok(voluta_sys_llistxattr(path, list, size, out));
}

void voluta_t_flistxattr(int fd, char *list, size_t size, size_t *out)
{
	expect_ok(voluta_sys_flistxattr(fd, list, size, out));
}

void voluta_t_getdents(int fd, void *buf, size_t bsz, struct dirent64 *dents,
		       size_t ndents, size_t *out_ndents)
{
	expect_ok(voluta_sys_getdents(fd, buf, bsz,
				      dents, ndents, out_ndents));
}

void voluta_t_getdent(int fd, struct dirent64 *dent)
{
	size_t nde;
	char buf[1024];

	voluta_t_getdents(fd, buf, sizeof(buf), dent, 1, &nde);
}

void voluta_t_copy_file_range(int fd_in, loff_t *off_in, int fd_out,
			      loff_t *off_out, size_t len,
			      unsigned int flags, loff_t *result)
{
	expect_ok(voluta_sys_copy_file_range(fd_in, off_in, fd_out,
					     off_out, len, flags, result));
}

void voluta_t_ioctl_ficlone(int dest_fd, int src_fd)
{
	expect_ok(voluta_sys_ioctl_ficlone(dest_fd, src_fd));
}
