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
#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <sys/xattr.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <features.h>
#include <errno.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <utime.h>
#include <voluta/xsyscall.h>


#if _POSIX_C_SOURCE < 200809L
#error "_POSIX_C_SOURCE < 200809L"
#endif

#if _ATFILE_SOURCE != 1
#error "_ATFILE_SOURCE != 1"
#endif


static int ok_or_errno(int err)
{
	return err ? -errno : 0;
}

static int val_or_errno(int val)
{
	return (val < 0) ? -errno : val;
}


static int fd_or_errno(int err, int *fd)
{
	if (err >= 0) {
		*fd = err;
		err = 0;
	} else {
		*fd = -1;
		err = -errno;
	}
	return err;
}

static int size_or_errno(ssize_t res, size_t *cnt)
{
	int err;

	if (res >= 0) {
		err = 0;
		*cnt = (size_t)res;
	} else {
		err = -errno;
		*cnt = 0;
	}
	return err;
}

static int off_or_errno(loff_t off, loff_t *out)
{
	int err;

	if (off >= 0) {
		err = 0;
		*out = off;
	} else {
		err = -errno;
		*out = off;
	}
	return err;
}

static int differ_or_errno(void *ptr, void *errptr, void **out)
{
	int err;

	if (ptr == errptr) {
		err = -errno;
		*out = NULL;
	} else {
		err = 0;
		*out = ptr;
	}
	return err;
}

static int errno_or_generic_error(void)
{
	const int errnum = abs(errno);

	return (errnum > 0) ? -errnum : -EPERM;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* POSIX */
int voluta_sys_chmod(const char *path, mode_t mode)
{
	return ok_or_errno(chmod(path, mode));
}

int voluta_sys_fchmod(int fd, mode_t mode)
{
	return ok_or_errno(fchmod(fd, mode));
}

int voluta_sys_fchmodat(int dirfd, const char *pathname, mode_t mode, int flags)
{
	return ok_or_errno(fchmodat(dirfd, pathname, mode, flags));
}

int voluta_sys_chown(const char *path, uid_t uid, gid_t gid)
{
	return ok_or_errno(chown(path, uid, gid));
}

int voluta_sys_fchown(int fd, uid_t uid, gid_t gid)
{
	return ok_or_errno(fchown(fd, uid, gid));
}

int voluta_sys_fchownat(int dirfd, const char *pathname,
			uid_t uid, gid_t gid, int flags)
{
	return ok_or_errno(fchownat(dirfd, pathname, uid, gid, flags));
}

int voluta_sys_utime(const char *filename, const struct utimbuf *times)
{
	return ok_or_errno(utime(filename, times));
}

int voluta_sys_utimes(const char *filename, const struct timeval times[2])
{
	return ok_or_errno(utimes(filename, times));
}

int voluta_sys_utimensat(int dirfd, const char *pathname,
			 const struct timespec times[2], int flags)
{
	return ok_or_errno(utimensat(dirfd, pathname, times, flags));
}

int voluta_sys_futimens(int fd, const struct timespec times[2])
{
	return ok_or_errno(futimens(fd, times));
}

int voluta_sys_mkdir(const char *path, mode_t mode)
{
	return ok_or_errno(mkdir(path, mode));
}

int voluta_sys_mkdirat(int dirfd, const char *pathname, mode_t mode)
{
	return ok_or_errno(mkdirat(dirfd, pathname, mode));
}

int voluta_sys_rmdir(const char *path)
{
	return ok_or_errno(rmdir(path));
}

int voluta_sys_creat(const char *path, mode_t mode, int *fd)
{
	return fd_or_errno(creat(path, mode), fd);
}

int voluta_sys_open(const char *path, int flags, mode_t mode, int *fd)
{
	return fd_or_errno(open(path, flags, mode), fd);
}

int voluta_sys_openat(int dirfd, const char *path,
		      int flags, mode_t mode, int *fd)
{
	return fd_or_errno(openat(dirfd, path, flags, mode), fd);
}

int voluta_sys_close(int fd)
{
	return ok_or_errno(close(fd));
}

int voluta_sys_access(const char *path, int mode)
{
	return ok_or_errno(access(path, mode));
}

int voluta_sys_faccessat(int dirfd, const char *pathname, int mode, int flags)
{
	return ok_or_errno(faccessat(dirfd, pathname, mode, flags));
}

int voluta_sys_link(const char *path1, const char *path2)
{
	return ok_or_errno(link(path1, path2));
}

int voluta_sys_linkat(int olddirfd, const char *oldpath,
		      int newdirfd, const char *newpath, int flags)
{
	return ok_or_errno(linkat(olddirfd, oldpath,
				  newdirfd, newpath, flags));
}

int voluta_sys_unlink(const char *path)
{
	return ok_or_errno(unlink(path));
}

int voluta_sys_unlinkat(int dirfd, const char *pathname, int flags)
{
	return ok_or_errno(unlinkat(dirfd, pathname, flags));
}

int voluta_sys_rename(const char *oldpath, const char *newpath)
{
	return ok_or_errno(rename(oldpath, newpath));
}

int voluta_sys_renameat(int olddirfd, const char *oldpath,
			int newdirfd, const char *newpath)
{
	return ok_or_errno(renameat(olddirfd, oldpath, newdirfd, newpath));
}

int voluta_sys_renameat2(int olddirfd, const char *oldpath,
			 int newdirfd, const char *newpath, unsigned int flags)
{
	return ok_or_errno(renameat2(olddirfd, oldpath, newdirfd,
				     newpath, flags));
}

int voluta_sys_llseek(int fd, loff_t off, int whence, loff_t *pos)
{
	return off_or_errno(lseek64(fd, off, whence), pos);
}

int voluta_sys_syncfs(int fd)
{
	return ok_or_errno(syncfs(fd));
}

int voluta_sys_fsync(int fd)
{
	return ok_or_errno(fsync(fd));
}

int voluta_sys_fdatasync(int fd)
{
	return ok_or_errno(fdatasync(fd));
}

int voluta_sys_fallocate(int fd, int mode, loff_t off, loff_t len)
{
	return ok_or_errno(fallocate(fd, mode, off, len));
}

int voluta_sys_truncate(const char *path, loff_t len)
{
	return ok_or_errno(truncate(path, len));
}

int voluta_sys_ftruncate(int fd, loff_t len)
{
	return ok_or_errno(ftruncate(fd, len));
}

int voluta_sys_mkfifo(const char *path, mode_t mode)
{
	return ok_or_errno(mkfifo(path, mode));
}

int voluta_sys_mkfifoat(int dirfd, const char *pathname, mode_t mode)
{
	return ok_or_errno(mkfifoat(dirfd, pathname, mode));
}

int voluta_sys_mknod(const char *pathname, mode_t mode, dev_t dev)
{
	return ok_or_errno(mknod(pathname, mode, dev));
}

int voluta_sys_mknodat(int dirfd, const char *pathname, mode_t mode, dev_t dev)
{
	return ok_or_errno(mknodat(dirfd, pathname, mode, dev));
}

int voluta_sys_symlink(const char *oldpath, const char *newpath)
{
	return ok_or_errno(symlink(oldpath, newpath));
}

int voluta_sys_symlinkat(const char *target, int dirfd, const char *linkpath)
{
	return ok_or_errno(symlinkat(target, dirfd, linkpath));
}

int voluta_sys_readlink(const char *path, char *buf, size_t bsz, size_t *cnt)
{
	return size_or_errno(readlink(path, buf, bsz), cnt);
}

int voluta_sys_readlinkat(int dirfd, const char *pathname,
			  char *buf, size_t bsz, size_t *cnt)
{
	return size_or_errno(readlinkat(dirfd, pathname, buf, bsz), cnt);
}

int voluta_sys_fstat(int fd, struct stat *st)
{
	return ok_or_errno(fstat(fd, st));
}

int voluta_sys_fstatat(int dirfd, const char *path, struct stat *st, int flags)
{
	return ok_or_errno(fstatat(dirfd, path, st, flags));
}

int voluta_sys_stat(const char *path, struct stat *st)
{
	return ok_or_errno(stat(path, st));
}

int voluta_sys_lstat(const char *path, struct stat *st)
{
	return ok_or_errno(lstat(path, st));
}

int voluta_sys_statvfs(const char *path, struct statvfs *stvfs)
{
	return ok_or_errno(statvfs(path, stvfs));
}

int voluta_sys_fstatvfs(int fd, struct statvfs *stvfs)
{
	return ok_or_errno(fstatvfs(fd, stvfs));
}

int voluta_sys_statfs(const char *path, struct statfs *stfs)
{
	return ok_or_errno(statfs(path, stfs));
}

int voluta_sys_fstatfs(int fd, struct statfs *stfs)
{
	return ok_or_errno(fstatfs(fd, stfs));
}

int voluta_sys_flock(int fd, int operation)
{
	return ok_or_errno(flock(fd, operation));
}

/* RDWR */
int voluta_sys_read(int fd, void *buf, size_t cnt, size_t *nrd)
{
	return size_or_errno(read(fd, buf, cnt), nrd);
}

int voluta_sys_pread(int fd, void *buf, size_t cnt, loff_t off, size_t *nrd)
{
	return size_or_errno(pread(fd, buf, cnt, off), nrd);
}

int voluta_sys_write(int fd, const void *buf, size_t cnt, size_t *nwr)
{
	return size_or_errno(write(fd, buf, cnt), nwr);
}

int voluta_sys_pwrite(int fd, const void *buf, size_t cnt,
		      loff_t off, size_t *nwr)
{
	return size_or_errno(pwrite(fd, buf, cnt, off), nwr);
}

int voluta_sys_readv(int fd, const struct iovec *iov, int iovcnt, size_t *nrd)
{
	return size_or_errno(readv(fd, iov, iovcnt), nrd);
}

int voluta_sys_writev(int fd, const struct iovec *iov, int iovcnt, size_t *nwr)
{
	return size_or_errno(writev(fd, iov, iovcnt), nwr);
}

int voluta_sys_preadv(int fd, const struct iovec *iov, int iovcnt,
		      off_t off, size_t *nrd)
{
	return size_or_errno(preadv(fd, iov, iovcnt, off), nrd);
}

int voluta_sys_pwritev(int fd, const struct iovec *iov, int iovcnt,
		       off_t off, size_t *nwr)
{
	return size_or_errno(pwritev(fd, iov, iovcnt, off), nwr);
}

int voluta_sys_preadv2(int fd, const struct iovec *iov, int iovcnt,
		       off_t off, int flags, size_t *nrd)
{
	return size_or_errno(preadv2(fd, iov, iovcnt, off, flags), nrd);
}

int voluta_sys_pwritev2(int fd, const struct iovec *iov, int iovcnt,
			off_t off, int flags, size_t *nwr)
{
	return size_or_errno(pwritev2(fd, iov, iovcnt, off, flags), nwr);
}

/* XATTR */
int voluta_sys_setxattr(const char *path, const char *name,
			const void *value, size_t size, int flags)
{
	return ok_or_errno(setxattr(path, name, value, size, flags));
}

int voluta_sys_lsetxattr(const char *path, const char *name,
			 const void *value, size_t size, int flags)
{
	return ok_or_errno(lsetxattr(path, name, value, size, flags));
}

int voluta_sys_fsetxattr(int fd, const char *name,
			 const void *value, size_t size, int flags)
{
	return ok_or_errno(fsetxattr(fd, name, value, size, flags));
}

int voluta_sys_getxattr(const char *path, const char *name,
			void *value, size_t size, size_t *cnt)
{
	return size_or_errno(getxattr(path, name, value, size), cnt);
}

int voluta_sys_lgetxattr(const char *path, const char *name,
			 void *value, size_t size, size_t *cnt)
{
	return size_or_errno(lgetxattr(path, name, value, size), cnt);
}

int voluta_sys_fgetxattr(int fd, const char *name,
			 void *value, size_t size, size_t *cnt)
{
	return size_or_errno(fgetxattr(fd, name, value, size), cnt);
}


int voluta_sys_removexattr(const char *path, const char *name)
{
	return ok_or_errno(removexattr(path, name));
}

int voluta_sys_lremovexattr(const char *path, const char *name)
{
	return ok_or_errno(lremovexattr(path, name));
}

int voluta_sys_fremovexattr(int fd, const char *name)
{
	return ok_or_errno(fremovexattr(fd, name));
}


int voluta_sys_listxattr(const char *path, char *list, size_t size, size_t *out)
{
	return size_or_errno(listxattr(path, list, size), out);
}

int voluta_sys_llistxattr(const char *path, char *list, size_t size,
			  size_t *out)
{
	return size_or_errno(llistxattr(path, list, size), out);
}

int voluta_sys_flistxattr(int fd, char *list, size_t size, size_t *out)
{
	return size_or_errno(flistxattr(fd, list, size), out);
}

/* MMAP */
int voluta_sys_mmap(void *addr, size_t length, int prot, int flags,
		    int fd, off_t offset, void **out_addr)
{
	return differ_or_errno(mmap(addr, length, prot, flags, fd, offset),
			       MAP_FAILED, out_addr);
}

int voluta_sys_mmap_anon(size_t length, int xflags, void **out_addr)
{
	const int prot = PROT_WRITE | PROT_READ;
	const int flags = MAP_PRIVATE | MAP_ANONYMOUS;

	return voluta_sys_mmap(NULL, length, prot,
			       flags | xflags, -1, 0, out_addr);
}

int voluta_sys_munmap(void *addr, size_t length)
{
	return ok_or_errno(munmap(addr, length));
}

int voluta_sys_msync(void *addr, size_t len, int flags)
{
	return ok_or_errno(msync(addr, len, flags));
}

int voluta_sys_madvise(void *addr, size_t len, int advice)
{
	return ok_or_errno(madvise(addr, len, advice));
}

int voluta_sys_mlock(const void *addr, size_t len)
{
	return ok_or_errno(mlock(addr, len));
}

#ifdef MLOCK_ONFAULT /* TODO: RM; CentOS7 only */
int voluta_sys_mlock2(const void *addr, size_t len, unsigned int flags)
{
	return ok_or_errno(mlock2(addr, len, flags));
}
#endif

int voluta_sys_munlock(const void *addr, size_t len)
{
	return ok_or_errno(munlock(addr, len));
}

int voluta_sys_mlockall(int flags)
{
	return ok_or_errno(mlockall(flags));
}

int voluta_sys_munlockall(void)
{
	return ok_or_errno(munlockall());
}

int voluta_sys_brk(void *addr)
{
	return ok_or_errno(brk(addr));
}

int voluta_sys_sbrk(intptr_t increment, void **out_addr)
{
	return differ_or_errno(sbrk(increment), (void *)(-1), out_addr);
}

/* RLIMITS */
int voluta_sys_getrlimit(int resource, struct rlimit *rlim)
{
	return ok_or_errno(getrlimit(resource, rlim));
}

int voluta_sys_setrlimit(int resource, const struct rlimit *rlim)
{
	return ok_or_errno(setrlimit(resource, rlim));
}

/* PRCTL */
int voluta_sys_prctl(int option, unsigned long arg2, unsigned long arg3,
		     unsigned long arg4, unsigned long arg5)
{
	return val_or_errno(prctl(option, arg2, arg3, arg4, arg5));
}

/* SYSCALL */
int voluta_sys_copy_file_range(int fd_in, loff_t *off_in, int fd_out,
			       loff_t *off_out, size_t len, unsigned int flags,
			       loff_t *result)
{
	return off_or_errno(copy_file_range(fd_in, off_in, fd_out,
					    off_out, len, flags), result);
}

int voluta_sys_memfd_create(const char *name, unsigned int flags, int *fd)
{
	return fd_or_errno(memfd_create(name, flags), fd);
}

/* IOCTLS */
int voluta_sys_ioctl_blkgetsize64(int fd, size_t *sz)
{
	return ok_or_errno(ioctl(fd, BLKGETSIZE64, sz));
}

int voluta_sys_ioctl_ficlone(int dest_fd, int src_fd)
{
	return ok_or_errno(ioctl(dest_fd, FICLONE, src_fd));
}

/* GETDENTS */
struct linux_dirent64 {
	ino64_t        d_ino;
	off64_t        d_off;
	unsigned short d_reclen;
	unsigned char  d_type;
	char           d_name[1];
};

int voluta_sys_getdent(int fd, loff_t off, struct dirent64 *dent)
{
	long err, nread;
	size_t len;
	const struct linux_dirent64 *d = NULL;
	char buf[1024];
	void *ptr = buf;

	memset(buf, 0, sizeof(buf));
	memset(dent, 0, sizeof(*dent));

	err = lseek64(fd, off, SEEK_SET);
	if (err < 0) {
		return errno_or_generic_error();
	}
	nread = syscall(SYS_getdents64, fd, ptr, sizeof(buf));
	if (nread == -1) {
		return errno_or_generic_error();
	}
	if (nread == 0) {
		dent->d_off = -1;
		return 0; /* End-of-stream */
	}
	d = (const struct linux_dirent64 *)ptr;
	if (d->d_reclen >= sizeof(buf)) {
		return -EINVAL;
	}
	dent->d_ino = d->d_ino;
	dent->d_off = (loff_t)d->d_off;
	dent->d_type = d->d_type;

	len = strlen(d->d_name);
	if (!len) {
		return d->d_type ? -EINVAL : 0;
	}
	if (len >= sizeof(dent->d_name)) {
		return -ENAMETOOLONG;
	}
	memcpy(dent->d_name, d->d_name, len);
	return 0;
}

int voluta_sys_getdents(int fd, void *buf, size_t bsz, struct dirent64 *dents,
			size_t ndents, size_t *out_ndents)
{
	long nread, pos = 0;
	size_t len, ndents_decoded = 0;
	const struct linux_dirent64 *d = NULL;
	void *ptr = buf;
	struct dirent64 *dent = dents, *end = dents + ndents;

	errno = 0;
	if (!ndents || (bsz < sizeof(*dents))) {
		return -EINVAL;
	}
	nread = syscall(SYS_getdents64, fd, ptr, bsz);
	if (nread == -1) {
		return errno_or_generic_error();
	}
	if (nread == 0) {
		memset(dent, 0, sizeof(*dent));
		dent->d_off = -1;
		goto out; /* End-of-stream */
	}
	d = (const struct linux_dirent64 *)ptr;
	if (d->d_reclen >= bsz) {
		return -EINVAL;
	}
	*out_ndents = 0;
	while ((pos < nread) && (dent < end)) {
		len = strlen(d->d_name);
		if (len >= sizeof(dent->d_name)) {
			return -ENAMETOOLONG;
		}
		memset(dent, 0, sizeof(*dent));
		dent->d_ino = d->d_ino;
		dent->d_off = (loff_t)d->d_off;
		dent->d_type = d->d_type;
		memcpy(dent->d_name, d->d_name, len);

		pos += d->d_reclen;
		ptr = (char *)buf + pos;
		d = (const struct linux_dirent64 *)ptr;

		++ndents_decoded;
		++dent;
	}
out:
	*out_ndents = ndents_decoded;
	return 0;
}

/* SIGNALS */
int voluta_sys_sigaction(int signum, const struct sigaction *act,
			 struct sigaction *oldact)
{
	return ok_or_errno(sigaction(signum, act, oldact));
}

/* CLOCK */
int voluta_sys_clock_gettime(clockid_t clock_id, struct timespec *tp)
{
	return ok_or_errno(clock_gettime(clock_id, tp));
}

