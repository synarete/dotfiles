/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                             *
 *                            Funex File-System                                *
 *                                                                             *
 *  Copyright (C) 2014,2015 Synarete                                           *
 *                                                                             *
 *  Funex is a free software; you can redistribute it and/or modify it under   *
 *  the terms of the GNU General Public License as published by the Free       *
 *  Software Foundation, either version 3 of the License, or (at your option)  *
 *  any later version.                                                         *
 *                                                                             *
 *  Funex is distributed in the hope that it will be useful, but WITHOUT ANY   *
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS  *
 *  FOR A PARTICULAR PURPOSE. See the GNU General Public License for more      *
 *  details.                                                                   *
 *                                                                             *
 *  You should have received a copy of the GNU General Public License along    *
 *  with this program. If not, see <http://www.gnu.org/licenses/>              *
 *                                                                             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
#include <fnxconfig.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "graybox.h"
#include "syscalls.h"


int gbx_sys_access(const char *path, int mode)
{
	int rc = fnx_sys_access(path, mode);
	gbx_trace("access(%s, %o) => %d", path, mode, rc);
	return rc;
}

int gbx_sys_link(const char *path1, const char *path2)
{
	int rc = fnx_sys_link(path1, path2);
	gbx_trace("link(%s, %s) => %d", path1, path2, rc);
	return rc;
}

int gbx_sys_unlink(const char *path)
{
	int rc = fnx_sys_unlink(path);
	gbx_trace("unlink(%s) => %d", path, rc);
	return rc;
}

int gbx_sys_rename(const char *oldpath, const char *newpath)
{
	int rc = fnx_sys_rename(oldpath, newpath);
	gbx_trace("rename(%s, %s) => %d", oldpath, newpath, rc);
	return rc;
}

int gbx_sys_realpath(const char *path, char **real)
{
	int rc = fnx_sys_realpath(path, real);
	gbx_trace("realpath(%s) => %s %d", path, *real, rc);
	return rc;
}

int gbx_sys_fstatvfs(int fd, struct statvfs *stv)
{
	int rc = fnx_sys_fstatvfs(fd, stv);
	gbx_trace("fstatvfs(%d) => %d", fd, rc);
	return rc;
}

int gbx_sys_statvfs(const char *path, struct statvfs *stv)
{
	int rc = fnx_sys_statvfs(path, stv);
	gbx_trace("statvfs(%s) => %d", path, rc);
	return rc;
}

int gbx_sys_fstat(int fd, struct stat *st)
{
	int rc = fnx_sys_fstat(fd, st);
	gbx_trace("fstat(%d) => %d", fd, rc);
	return rc;
}

int gbx_sys_stat(const char *path, struct stat *st)
{
	int rc = fnx_sys_stat(path, st);
	gbx_trace("stat(%s) => %d", path, rc);
	return rc;
}

int gbx_sys_lstat(const char *path, struct stat *st)
{
	int rc = fnx_sys_lstat(path, st);
	gbx_trace("lstat(%s) => %d", path, rc);
	return rc;
}

int gbx_sys_chmod(const char *path, mode_t mode)
{
	int rc = fnx_sys_chmod(path, mode);
	gbx_trace("chmod(%s, %o) => %d", path, mode, rc);
	return rc;
}

int gbx_sys_fchmod(int fd, mode_t mode)
{
	int rc = fnx_sys_fchmod(fd, mode);
	gbx_trace("fchmod(%d, %o) => %d", fd, mode, rc);
	return rc;
}

int gbx_sys_chown(const char *path, uid_t uid, gid_t gid)
{
	int rc = fnx_sys_chown(path, uid, gid);
	gbx_trace("chown(%s, %d, %d) => %d", path, uid, gid, rc);
	return rc;
}

int gbx_sys_mkdir(const char *path, mode_t mode)
{
	int rc = fnx_sys_mkdir(path, mode);
	gbx_trace("mkdir(%s, %o) => %d", path, mode, rc);
	return rc;
}

int gbx_sys_rmdir(const char *path)
{
	int rc = fnx_sys_rmdir(path);
	gbx_trace("rmdir(%s) => %d", path, rc);
	return rc;
}

int gbx_sys_getdent(int fd, loff_t off, gbx_dirent_t *dent)
{
	int rc = fnx_sys_getdent(fd, off, dent);
	gbx_trace("getdent(%d, %ld) => %d", fd, off, rc);
	return rc;
}

int gbx_sys_create(const char *path, mode_t mode, int *fd)
{
	int rc = fnx_sys_create(path, mode, fd);
	gbx_trace("create(%s, %o) => %d %d", path, mode, *fd, rc);
	return rc;
}

int gbx_sys_create2(const char *path, mode_t mode)
{
	int rc, fd;

	rc = gbx_sys_create(path, mode, &fd);
	if (rc == 0) {
		rc = gbx_sys_close(fd);
	}
	return rc;
}

int gbx_sys_open(const char *path, int flags, mode_t mode, int *fd)
{
	int rc = fnx_sys_open(path, flags, mode, fd);
	gbx_trace("open(%s, %#x, %o) => %d %d", path, flags, mode, *fd, rc);
	return rc;
}

int gbx_sys_close(int fd)
{
	int rc = fnx_sys_close(fd);
	gbx_trace("close(%d) => %d", fd, rc);
	return rc;
}

int gbx_sys_read(int fd, void *buf, size_t cnt, size_t *nrd)
{
	int rc = fnx_sys_read(fd, buf, cnt, nrd);
	gbx_trace("read(%d, %p, %lu) => %lu %d", fd, buf, cnt, *nrd, rc);
	return rc;
}

int gbx_sys_pread(int fd, void *buf, size_t cnt, loff_t off, size_t *nrd)
{
	int rc = fnx_sys_pread(fd, buf, cnt, off, nrd);
	gbx_trace("pread(%d, %p, %lu, %ld) => %lu %d", fd, buf, cnt, off, *nrd, rc);
	return rc;
}

int gbx_sys_write(int fd, const void *buf, size_t cnt, size_t *nwr)
{
	int rc = fnx_sys_write(fd, buf, cnt, nwr);
	gbx_trace("write(%d, %p, %lu) => %lu %d", fd, buf, cnt, *nwr, rc);
	return rc;
}

int gbx_sys_pwrite(int fd, const void *buf, size_t cnt, loff_t off, size_t *nwr)
{
	int rc = fnx_sys_pwrite(fd, buf, cnt, off, nwr);
	gbx_trace("pwrite(%d, %p, %lu, %ld) => %lu %d",
	          fd, buf, cnt, off, *nwr, rc);
	return rc;
}

int gbx_sys_lseek(int fd, loff_t off, int whence, loff_t *pos)
{
	int rc = fnx_sys_lseek(fd, off, whence, pos);
	gbx_trace("lseek(%d, %ld, %d) => %ld %d", fd, off, whence, *pos, rc);
	return rc;
}

int gbx_sys_syncfs(int fd)
{
	int rc;

	rc = fnx_sys_syncfs(fd);
	gbx_trace("syncfs(%d) => %d", fd, rc);
	return rc;
}

int gbx_sys_fsync(int fd)
{
	int rc = fnx_sys_fsync(fd);
	gbx_trace("fsync(%d) => %d", fd, rc);
	return rc;
}

int gbx_sys_fdatasync(int fd)
{
	int rc = fnx_sys_fdatasync(fd);
	gbx_trace("fdatasync(%d) => %d", fd, rc);
	return rc;
}

int gbx_sys_fallocate(int fd, int mode, loff_t off, loff_t len)
{
	int rc = fnx_sys_fallocate(fd, mode, off, len);
	gbx_trace("fallocate(%d, %o, %ld, %ld) => %d", fd, mode, off, len, rc);
	return rc;
}

int gbx_sys_truncate(const char *path, loff_t len)
{
	int rc = fnx_sys_truncate(path, len);
	gbx_trace("truncate(%s, %ld) => %d", path, len, rc);
	return rc;
}

int gbx_sys_ftruncate(int fd, loff_t len)
{
	int rc = fnx_sys_ftruncate(fd, len);
	gbx_trace("ftruncate(%d, %ld) => %d", fd, len, rc);
	return rc;
}

int gbx_sys_readlink(const char *path, char *buf, size_t bsz, size_t *nwr)
{
	int rc = fnx_sys_readlink(path, buf, bsz, nwr);
	gbx_trace("readlink(%s, %p, %lu) => %lu %d", path, buf, bsz, *nwr, rc);
	return rc;
}

int gbx_sys_symlink(const char *oldpath, const char *newpath)
{
	int rc = fnx_sys_symlink(oldpath, newpath);
	gbx_trace("symlink(%s, %s) => %d", oldpath, newpath, rc);
	return rc;
}

int gbx_sys_mkfifo(const char *path, mode_t mode)
{
	int rc = fnx_sys_mkfifo(path, mode);
	gbx_trace("mkfifo(%s, %o) => %d", path, mode, rc);
	return rc;
}

int gbx_sys_mmap(int fd, loff_t off, size_t len,
                 int prot, int flags, void **addr)
{
	int rc = fnx_sys_mmap(fd, off, len, prot, flags, addr);
	gbx_trace("mmap(%d, %ld, %lu, %d, %x) => %p %d",
	          fd, off, len, prot, flags, *addr, rc);
	return rc;
}

int gbx_sys_munmap(void *addr, size_t len)
{
	int rc = fnx_sys_munmap(addr, len);
	gbx_trace("munmap(%p, %lu) => %d", addr, len, rc);
	return rc;
}

int gbx_sys_msync(void *addr, size_t len, int flags)
{
	int rc = fnx_sys_msync(addr, len, flags);
	gbx_trace("msync(%p, %lu, %x) => %d", addr, len, flags, rc);
	return rc;
}

