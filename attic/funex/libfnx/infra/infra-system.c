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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <dirent.h>

#include "infra-macros.h"
#include "infra-compiler.h"
#include "infra-system.h"

int fnx_sys_chmod(const char *path, mode_t mode)
{
	int rc;

	rc = chmod(path, mode);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_sys_fchmod(int fd, mode_t mode)
{
	int rc;

	rc = fchmod(fd, mode);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_sys_chown(const char *path, uid_t uid, gid_t gid)
{
	int rc;

	rc = chown(path, uid, gid);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct linux_dirent {
	unsigned long  d_ino;
	unsigned long  d_off;
	unsigned short d_reclen;
	char           d_name[1];
};

int fnx_sys_getdent(int fd, loff_t off, fnx_sys_dirent_t *dent)
{
	long rc, dt, nread;
	size_t len;
	char buf[1024];
	void *ptr = buf;
	const struct linux_dirent *d = NULL;

	memset(dent, 0, sizeof(*dent));

	if (off >= 0) {
		rc = lseek(fd, off, SEEK_SET);
		if (rc == -1) {
			return -errno;
		}
	}

	/* Read dir-entry using low-level syscall */
	nread = syscall(SYS_getdents, fd, ptr, sizeof(buf));
	if (nread == -1) {
		return -errno;
	}
	if (nread == 0) {
		return 0; /* End-of-stream */
	}
	d = (const struct linux_dirent *)ptr;
	if (d->d_reclen >= sizeof(buf)) {
		return -EINVAL;
	}
	len = strlen(d->d_name);
	if (len >= sizeof(dent->d_name)) {
		return -ENAMETOOLONG;
	}
	dt = (long)buf[d->d_reclen - 1];

	/* Convert to fnx_sys_dirent format */
	dent->d_ino     = d->d_ino;
	dent->d_off     = (loff_t)d->d_off;
	dent->d_type    = (mode_t)DTTOIF(dt);
	memcpy(dent->d_name, d->d_name, len);

	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_sys_mkdir(const char *path, mode_t mode)
{
	int rc;

	errno = 0;
	rc = mkdir(path, mode);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_sys_rmdir(const char *path)
{
	int rc;

	errno = 0;
	rc = rmdir(path);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_sys_create(const char *path, mode_t mode, int *fd)
{
	int rc;

	rc = creat(path, mode);
	if (rc < 0) {
		*fd = -1;
		rc = -errno;
	} else {
		*fd = rc;
		rc = 0;
	}
	return rc;
}

int fnx_sys_open(const char *path, int flags, mode_t mode, int *fd)
{
	int rc;

	rc = open(path, flags, mode);
	if (rc < 0) {
		*fd = -1;
		rc = -errno;
	} else {
		*fd = rc;
		rc = 0;
	}
	return rc;
}

int fnx_sys_close(int fd)
{
	int rc;

	errno = 0;
	rc = close(fd);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_sys_access(const char *path, int mode)
{
	int rc;

	rc = access(path, mode);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_sys_link(const char *path1, const char *path2)
{
	int rc;

	rc = link(path1, path2);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_sys_unlink(const char *path)
{
	int rc;

	rc = unlink(path);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_sys_rename(const char *oldpath, const char *newpath)
{
	int rc;

	rc = rename(oldpath, newpath);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_sys_realpath(const char *path, char **resp)
{
	int rc = 0;
	char *real;

	real = realpath(path, NULL);
	if (real != NULL) {
		*resp = real;
	} else {
		rc = -errno;
	}
	return rc;
}

static void join(char *buf, const char *comps[], size_t ncomp)
{
	int sep, last;
	char *tgt;
	const char *src;

	sep = 0;
	tgt = buf;
	for (size_t i = 0; i < ncomp; ++i) {
		if ((src = comps[i]) != NULL) {
			while (*src != '\0') {
				if (*src == '/') {
					if (!sep) {
						*tgt++ = *src++;
						sep = 1;
					} else {
						src++;
					}
				} else {
					*tgt++ = *src++;
					sep = 0;
				}
			}

			last = ((i + 1) == ncomp);
			if (!sep && !last) {
				*tgt++ = '/';
				sep = 1;
			}
		}
	}
	*tgt = '\0';
}

char *fnx_sys_makepath(const char *base, ...)
{
	va_list ap;
	char *path;
	const char *comp;
	const char *comps[256];
	size_t ncomp, length, total;
	const size_t nelem = FNX_NELEMS(comps);

	ncomp = 0;
	comps[ncomp++] = base;
	total = strlen(base);

	va_start(ap, base);
	while (1) {
		comp = va_arg(ap, char *);
		if (comp == NULL) {
			break; /* OK, end-of-list */
		}
		if (ncomp == nelem) {
			errno = EINVAL;
			return NULL; /* Too many comps */
		}
		length = strlen(comp);
		if (length == 0) {
			break;
		}
		comps[ncomp++] = comp;
		total += (length + 1);
	}
	va_end(ap);

	if (total >= PATH_MAX) {
		errno = EOVERFLOW;
		return NULL;
	}

	path = (char *)malloc(total + 1);
	if (path != NULL) {
		join(path, comps, ncomp);
	}
	return path;
}

char *fnx_sys_joinpath(const char *base, const char *comp)
{
	return fnx_sys_makepath(base, comp, NULL);
}

void fnx_sys_freepath(char *path)
{
	free(path);
}

void fnx_sys_freeppath(char **ppath)
{
	fnx_sys_freepath(*ppath);
	*ppath = NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_sys_read(int fd, void *buf, size_t cnt, size_t *nrd)
{
	int rc;
	ssize_t res;

	res = read(fd, buf, cnt);
	if (res < 0) {
		rc = -errno;
		*nrd = 0;
	} else {
		rc = 0;
		*nrd = (size_t)res;
	}
	return rc;
}

int fnx_sys_pread(int fd, void *buf, size_t cnt, loff_t off, size_t *nrd)
{
	int rc;
	ssize_t res;

	res = pread(fd, buf, cnt, off);
	if (res < 0) {
		rc = -errno;
		*nrd = 0;
	} else {
		rc = 0;
		*nrd = (size_t)res;
	}
	return rc;
}


int fnx_sys_write(int fd, const void *buf, size_t cnt, size_t *nwr)
{
	int rc;
	ssize_t res;

	res = write(fd, buf, cnt);
	if (res < 0) {
		rc = -errno;
		*nwr = 0;
	} else {
		rc = 0;
		*nwr = (size_t)res;
	}
	return rc;
}

int fnx_sys_pwrite(int fd, const void *buf, size_t cnt, loff_t off, size_t *nwr)
{
	int rc;
	ssize_t res;

	res = pwrite(fd, buf, cnt, off);
	if (res < 0) {
		rc = -errno;
		*nwr = 0;
	} else {
		rc = 0;
		*nwr = (size_t)res;
	}
	return rc;
}

int fnx_sys_lseek(int fd, loff_t off, int whence, loff_t *pos)
{
	int rc;
	loff_t res;

	res = lseek64(fd, off, whence);
	if (res == (off_t)(-1)) {
		rc = -errno;
	} else {
		rc = 0;
		*pos = res;
	}
	return rc;
}

int fnx_sys_syncfs(int fd)
{
	int rc;

	rc = syncfs(fd);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_sys_fsync(int fd)
{
	int rc;

	rc = fsync(fd);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_sys_fdatasync(int fd)
{
	int rc;

	rc = fdatasync(fd);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_sys_fallocate(int fd, int mode, loff_t off, loff_t len)
{
	int rc;

	rc = fallocate(fd, mode, off, len);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_sys_truncate(const char *path, loff_t len)
{
	int rc;

	rc = truncate(path, len);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_sys_ftruncate(int fd, loff_t len)
{
	int rc;

	rc = ftruncate(fd, len);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_sys_symlink(const char *oldpath, const char *newpath)
{
	int rc;

	rc = symlink(oldpath, newpath);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_sys_mkfifo(const char *path, mode_t mode)
{
	int rc;

	rc = mkfifo(path, mode);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_sys_readlink(const char *path, char *buf, size_t bsz, size_t *nwr)
{
	int rc = 0;
	ssize_t res;

	res = readlink(path, buf, bsz);
	if (res == -1) {
		rc = -errno;
		*nwr = 0;
	} else {
		*nwr = (size_t)res;
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int stat_isdir(const struct stat *st)
{
	return S_ISDIR(st->st_mode);
}

static int stat_isreg(const struct stat *st)
{
	return S_ISREG(st->st_mode);
}

static int stat_isblk(const struct stat *st)
{
	return S_ISBLK(st->st_mode);
}

static int stat_isvol(const struct stat *st)
{
	return S_ISREG(st->st_mode) || S_ISBLK(st->st_mode);
}

static int stat_access(const struct stat *st, mode_t mode)
{
	mode_t mask = 0;

	if (mode & R_OK) {
		mask |= S_IRUSR | S_IRGRP | S_IROTH;
	}
	if (mode & W_OK) {
		mask |= S_IWUSR | S_IWGRP | S_IWOTH;
	}
	return ((st->st_mode & mask) != 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_sys_fstat(int fd, struct stat *st)
{
	int rc;
	struct stat buf;

	if (st == NULL) {
		st = &buf;
	}
	rc = fstat(fd, st);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_sys_stat(const char *path, struct stat *st)
{
	int rc;
	struct stat buf;

	if (st == NULL) {
		st = &buf;
	}
	rc = stat(path, st);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_sys_lstat(const char *path, struct stat *st)
{
	int rc;
	struct stat buf;

	if (st == NULL) {
		st = &buf;
	}
	rc = lstat(path, st);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_sys_statdev(const char *path, struct stat *st, dev_t *dev)
{
	int rc;
	struct stat buf;

	if (st == NULL) {
		st = &buf;
	}
	rc = fnx_sys_stat(path, st);
	if (rc == 0) {
		*dev = st->st_dev;
	} else {
		*dev = 0;
	}
	return rc;
}

int fnx_sys_statdir(const char *path, struct stat *st)
{
	int rc;
	struct stat buf;

	if (st == NULL) {
		st = &buf;
	}
	rc = fnx_sys_stat(path, st);
	if ((rc == 0) && !stat_isdir(st)) {
		rc = -ENOTDIR;
	}
	return rc;
}

int fnx_sys_statedir(const char *path, struct stat *st)
{
	int rc;
	struct stat buf;

	if (st == NULL) {
		st = &buf;
	}
	rc = fnx_sys_stat(path, st);
	if (rc == 0) {
		if (!stat_isdir(st)) {
			rc = -ENOTDIR;
		} else if (st->st_nlink > 2) {
			rc = -ENOTEMPTY;
		}
	}
	return rc;
}

int fnx_sys_statreg(const char *path, struct stat *st)
{
	int rc;
	struct stat buf;

	if (st == NULL) {
		st = &buf;
	}
	rc = fnx_sys_stat(path, st);
	if ((rc == 0) && !stat_isreg(st)) {
		rc = -EINVAL;
	}
	return rc;
}

int fnx_sys_statblk(const char *path, struct stat *st)
{
	int rc;
	struct stat buf;

	if (st == NULL) {
		st = &buf;
	}
	rc = fnx_sys_stat(path, st);
	if ((rc == 0) && !stat_isblk(st)) {
		rc = -ENOTBLK;
	}
	return rc;
}

int fnx_sys_statvol(const char *path, struct stat *st, mode_t mode)
{
	int rc;
	struct stat buf;

	if (st == NULL) {
		st = &buf;
	}
	rc = fnx_sys_stat(path, st);
	if (rc != 0) {
		return rc;
	}
	if (!stat_isvol(st)) {
		return -ENOTBLK;
	}
	if (mode && !stat_access(st, mode)) {
		return -EACCES;
	}
	return 0;
}

static int blkgetsize(const char *path, loff_t *sz)
{
	int fd, rc = -1;
	size_t blksz = 0;

	errno = 0;
	fd = open(path, O_RDONLY, S_IRUSR);
	if (fd > 0) {
		errno = 0;
		rc = ioctl(fd, BLKGETSIZE64, &blksz);
		if (rc == 0) {
			*sz = (loff_t)blksz;
		} else {
			rc = -errno;
		}
		close(fd);
	} else {
		rc = -errno;
	}
	return rc;
}

int fnx_sys_statsz(const char *path, struct stat *st, loff_t *sz)
{
	int rc;
	struct stat buf;

	if (st == NULL) {
		st = &buf;
	}
	rc = fnx_sys_stat(path, st);
	if (rc != 0) {
		return rc;
	}

	*sz = st->st_size;
	if (stat_isblk(st)) {
		/* Special handling for block-device via ioctl */
		rc = blkgetsize(path, sz);
	}
	return rc;
}

int fnx_sys_fstatvfs(int fd, struct statvfs *stv)
{
	int rc;
	struct statvfs buf;

	if (stv == NULL) {
		stv = &buf;
	}
	errno = 0;
	rc = fstatvfs(fd, stv);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_sys_statvfs(const char *path, struct statvfs *stv)
{
	int rc;
	struct statvfs buf;

	if (stv == NULL) {
		stv = &buf;
	}
	errno = 0;
	rc = statvfs(path, stv);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_sys_mmap(int fd, loff_t off, size_t len,
                 int prot, int flags, void **addr)
{
	int rc;
	void *ptr = NULL;

	ptr = mmap(NULL, len, prot, flags, fd, off);
	if (ptr == MAP_FAILED) {
		rc = -errno;
		*addr = NULL;
	} else {
		rc = 0;
		*addr = ptr;
	}
	return rc;
}

int fnx_sys_munmap(void *addr, size_t len)
{
	int rc;

	rc = munmap(addr, len);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_sys_msync(void *addr, size_t len, int flags)
{
	int rc;

	rc = msync(addr, len, flags);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}
