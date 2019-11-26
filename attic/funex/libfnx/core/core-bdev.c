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
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

#include <fnxinfra.h>
#include "core-types.h"
#include "core-bdev.h"


/* Local functions declarations */
static int bdev_guaranty_cap(const fnx_bdev_t *, size_t);
static int bdev_isflocked(const fnx_bdev_t *);
static int bdev_ismmaped(const fnx_bdev_t *);
static int bdev_isinrange(const fnx_bdev_t *, fnx_lba_t, fnx_bkcnt_t);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_bdev_init(fnx_bdev_t *bdev)
{
	bdev->fd     = -1;
	bdev->flags  = 0;
	bdev->lbsz   = FNX_BLKSIZE;
	bdev->bcap   = 0;
	bdev->base   = 0;
	bdev->maddr  = NULL;
	bdev->path   = NULL;
}

void fnx_bdev_destroy(fnx_bdev_t *bdev)
{
	bdev->flags  = 0;
	bdev->lbsz   = 0;
	bdev->bcap   = 0;
	bdev->maddr  = NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_bool_t isblk(const char *path)
{
	int rc;
	struct stat st;

	rc = stat(path, &st);
	return ((rc == 0) && S_ISBLK(st.st_mode));
}

static int testf(unsigned flags, unsigned mask)
{
	return ((flags & mask) == mask);
}

static int bdev_testf(const fnx_bdev_t *bdev, unsigned flags)
{
	return testf(bdev->flags, flags);
}

static void bdev_setf(fnx_bdev_t *bdev, unsigned flags)
{
	bdev->flags |= flags;
}

static void bdev_unsetf(fnx_bdev_t *bdev, unsigned flags)
{
	bdev->flags &= ~(flags);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_bdev_isopen(const fnx_bdev_t *bdev)
{
	return (bdev->fd > 0);
}

static void bdev_savepath(fnx_bdev_t *bdev, const char *path)
{
	if (bdev->path != NULL) {
		free(bdev->path);
	}
	if (path != NULL) {
		bdev->path = strdup(path);
	} else {
		bdev->path = NULL;
	}
}

static void bdev_setpath(fnx_bdev_t *bdev, const char *path, int flags)
{
	if (flags & FNX_BDEV_SAVEPATH) {
		bdev_savepath(bdev, path);
	} else {
		bdev_savepath(bdev, NULL);
	}
}

static fnx_size_t bdev_bytesize(const fnx_bdev_t *bdev, fnx_bkcnt_t lbcnt)
{
	return (lbcnt * bdev->lbsz);
}

static fnx_bkcnt_t bdev_lbcount(const fnx_bdev_t *bdev, fnx_size_t bytes_sz)
{
	return (bytes_sz / bdev->lbsz);
}

static fnx_off_t bdev_offmax(const fnx_bdev_t *bdev)
{
	return (fnx_off_t)bdev_bytesize(bdev, bdev->bcap);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int bdev_stat(const fnx_bdev_t *bdev, struct stat *st)
{
	int rc;

	rc = fstat(bdev->fd, st);
	if (rc != 0) {
		rc = -errno;
		fnx_trace1("fstat-failure: fd=%d err=%d", bdev->fd, -rc);
	}
	return rc;
}

static int resolve_oflags(const char *path, unsigned vflags)
{
	int oflags = 0, create = 0;

	if (vflags & FNX_BDEV_RDWR) {
		oflags |= O_RDWR;
	} else if (vflags & FNX_BDEV_RDONLY) {
		oflags |= O_RDONLY;
	} else {
		oflags |= O_RDONLY; /* XXX */
	}
	if (vflags & FNX_BDEV_CREAT) {
		create = 1;
		oflags |= O_CREAT | O_TRUNC;
	}
	if (vflags & FNX_BDEV_DIRECT) {
		/* oflags |= O_SYNC; XXX Have O_SYNC ? */
		if (!create && isblk(path)) {
			oflags |= O_DIRECT;
		}
	}
	return oflags;
}

static int bdev_open_fd(fnx_bdev_t *bdev, const char *path, unsigned flags)
{
	int fd, oflags, rc = 0;
	mode_t mode;

	mode  = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
	if ((flags & FNX_BDEV_CREAT) && (flags & FNX_BDEV_LOCK)) {
		mode &= ~((mode_t)S_IXGRP);
		mode |= S_ISGID;
	}

	oflags  = resolve_oflags(path, flags);
	fd = open(path, oflags, mode);
	if (fd > 0) {
		bdev->fd = fd;
		bdev_setf(bdev, flags);
	} else {
		rc = -errno;
		fnx_warn("open-failure: path=%s oflags=%#x mode=%o err=%d",
		         path, oflags, mode, -rc);
	}

	return rc;
}

static int bdev_probe_type(fnx_bdev_t *bdev)
{
	int rc;
	struct stat st;

	rc = bdev_stat(bdev, &st);
	if (rc == 0) {
		if (S_ISREG(st.st_mode)) {
			bdev_setf(bdev, FNX_BDEV_REG);
		} else if (S_ISBLK(st.st_mode)) {
			bdev_setf(bdev, FNX_BDEV_BLK);
		} else {
			fnx_warn("unsupported-bdev-type: mode=%o", st.st_mode);
			rc = -EINVAL;
		}
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_bdev_create(fnx_bdev_t *bdev, const char *path,
                    fnx_bkcnt_t base, fnx_bkcnt_t bcap, int flags)
{
	int rc;

	flags |= FNX_BDEV_CREAT;
	if (fnx_bdev_isopen(bdev)) {
		return -EINVAL;
	}
	rc = bdev_open_fd(bdev, path, (unsigned)flags);
	if (rc != 0) {
		return rc;
	}
	rc = bdev_probe_type(bdev);
	if (rc != 0) {
		fnx_bdev_close(bdev);
		return rc;
	}
	rc = bdev_guaranty_cap(bdev, base + bcap);
	if (rc != 0) {
		fnx_bdev_close(bdev);
		return rc;
	}

	bdev->base = base;
	bdev->bcap = bcap;
	bdev_setpath(bdev, path, flags);
	bdev_setf(bdev, (unsigned)flags);

	return 0;
}

int fnx_bdev_open(fnx_bdev_t *bdev, const char *path,
                  fnx_bkcnt_t base, fnx_bkcnt_t bcap, int flags)
{
	int rc;
	fnx_bkcnt_t rem, nbk = 0;

	if (fnx_bdev_isopen(bdev)) {
		return -EINVAL;
	}
	rc = bdev_open_fd(bdev, path, (unsigned)flags);
	if (rc != 0) {
		return rc;
	}
	rc = bdev_probe_type(bdev);
	if (rc != 0) {
		fnx_bdev_close(bdev);
		return rc;
	}
	rc = bdev_guaranty_cap(bdev, base + bcap);
	if (rc != 0) {
		fnx_bdev_close(bdev);
		return rc;
	}
	rc = fnx_bdev_getcap(bdev, &nbk);
	if (rc != 0) {
		fnx_bdev_close(bdev);
		return rc;
	}
	if (base > nbk) {
		fnx_bdev_close(bdev);
		return -EINVAL;
	}
	rem = nbk - base;

	if (bcap == 0) { /* Special case: use probed capacity */
		bdev->base = base;
		bdev->bcap = rem;
	} else {
		bdev->base = base;
		bdev->bcap = (bcap < rem) ? bcap : rem;
	}
	bdev_setpath(bdev, path, flags);
	bdev_setf(bdev, (unsigned)flags);

	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int bdev_getcap_reg(const fnx_bdev_t *bdev, fnx_bkcnt_t *nbk)
{
	int rc;
	struct stat st;

	rc = bdev_stat(bdev, &st);
	if (rc == 0) {
		*nbk = bdev_lbcount(bdev, (size_t)(st.st_size));
	}
	return rc;
}

static int bdev_getcap_blk(const fnx_bdev_t *bdev, fnx_bkcnt_t *cnt)
{
	int rc;
	size_t sz;
	unsigned long cmd;

	cmd = BLKGETSIZE64;
	rc  = ioctl(bdev->fd, cmd, &sz);
	if (rc == 0) {
		*cnt = bdev_lbcount(bdev, sz);
	} else {
		rc = -errno;
		fnx_error("ioctl-blkgetsize64-failure: fd=%d cmd=%lx err=%d",
		          bdev->fd, cmd, -rc);
	}
	return rc;
}

int fnx_bdev_getcap(const fnx_bdev_t *bdev, fnx_bkcnt_t *nbk)
{
	int rc = -EBADF;

	if (fnx_bdev_isopen(bdev)) {
		if (bdev_testf(bdev, FNX_BDEV_REG)) {
			rc = bdev_getcap_reg(bdev, nbk);
		} else if (bdev_testf(bdev, FNX_BDEV_BLK)) {
			rc = bdev_getcap_blk(bdev, nbk);
		}
	}
	return rc;
}

int fnx_bdev_off2lba(const fnx_bdev_t *bdev, fnx_off_t off, fnx_lba_t *lba)
{
	*lba = (fnx_lba_t)(off / (fnx_off_t)bdev->lbsz);
	return bdev_isinrange(bdev, *lba, 1) ? 0 : -ERANGE;
}

static int bdev_guarantycap_reg(const fnx_bdev_t *bdev, fnx_bkcnt_t lbcnt)
{
	int rc;
	loff_t len, off = 0;
	fnx_bkcnt_t cur = 0;

	rc = fnx_bdev_getcap(bdev, &cur);
	if ((rc == 0) && (lbcnt > cur)) {
		len = (loff_t)bdev_bytesize(bdev, lbcnt);
		rc  = -posix_fallocate(bdev->fd, off, len);
		if (rc != 0) {
			fnx_trace1("posix_fallocate-failure: fd=%d off=%jd len=%jd err=%d",
			           bdev->fd, off, len, -rc);
		}
	}
	return rc;
}

static int bdev_lseek(const fnx_bdev_t *bdev, fnx_lba_t lba)
{
	int rc = 0;
	loff_t off, res;

	off = (loff_t)bdev_bytesize(bdev, lba);
	res = lseek64(bdev->fd, off, SEEK_SET);
	if (res == (off_t)(-1)) {
		rc = -errno;
		fnx_error("lseek-failure fd=%d off=%jd err=%d", bdev->fd, off, -rc);
	}
	return rc;
}

static int bdev_guarantycap_blk(const fnx_bdev_t *bdev, fnx_bkcnt_t lbcnt)
{
	int rc = 0;
	fnx_size_t sz;

	sz = bdev_bytesize(bdev, lbcnt);
	if (sz == 0) {
		goto out;
	}
	rc = bdev_lseek(bdev, lbcnt);
	if (rc != 0) {
		goto out;
	}
	rc = bdev_lseek(bdev, 0);
	if (rc != 0) {
		goto out;
	}
out:
	if (rc == -EINVAL) {
		rc = 0; /* Some block devices do-not support lseek */
	}
	return rc;
}

static int bdev_guaranty_cap(const fnx_bdev_t *bdev, fnx_bkcnt_t cnt)
{
	int rc = -1;

	if (bdev_testf(bdev, FNX_BDEV_REG)) {
		rc = bdev_guarantycap_reg(bdev, cnt);
	} else if (bdev_testf(bdev, FNX_BDEV_BLK)) {
		rc = bdev_guarantycap_blk(bdev, cnt);
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void bdev_close_fd(fnx_bdev_t *bdev)
{
	int rc;

	errno = 0;
	rc = close(bdev->fd);
	if ((rc != 0) && (errno != EINTR)) {
		/* http://lkml.org/lkml/2005/9/10/129 */
		fnx_panic("close-failure: fd=%d rc=%d", bdev->fd, rc);
	}
	bdev->fd = -1;
}

int fnx_bdev_close(fnx_bdev_t *bdev)
{
	if (bdev_ismmaped(bdev)) {
		fnx_bdev_munmap(bdev);
	}
	if (bdev_isflocked(bdev)) {
		fnx_bdev_funlock(bdev);
	}
	if (fnx_bdev_isopen(bdev)) {
		bdev_close_fd(bdev);
		bdev->flags = 0;
	}
	bdev_savepath(bdev, NULL);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int
bdev_isinrange(const fnx_bdev_t *bdev, fnx_lba_t lba, fnx_bkcnt_t cnt)
{
	int res = 1;
	fnx_lba_t end;

	end = bdev->base + bdev->bcap;
	if (lba < bdev->base) {
		res = 0;
	} else if (lba >= end) {
		res = 0;
	} else if (cnt > bdev->bcap) {
		res = 0;
	} else if ((lba + cnt) > end) {
		res = 0;
	}
	return res;
}

int fnx_bdev_read(const fnx_bdev_t *bdev,
                  void *buf, fnx_lba_t lba, fnx_bkcnt_t cnt)
{
	int rc;
	loff_t  off;
	size_t  len;
	ssize_t res;

	if (!fnx_bdev_isopen(bdev)) {
		return -EBADF;
	}
	if (!bdev_isinrange(bdev, lba, cnt)) {
		return -EINVAL;
	}
	len = bdev_bytesize(bdev, cnt);
	off = (loff_t)bdev_bytesize(bdev, bdev->base + lba);
	res = pread64(bdev->fd, buf, len, off);
	if (res != (ssize_t)len) {
		rc = -errno;
		fnx_error("pread-failure: fd=%d cnt=%zu off=%zu off-max=%zu err=%d",
		          bdev->fd, len, off, bdev_offmax(bdev), -rc);
		return rc;
	}
	return 0;
}

int fnx_bdev_write(const fnx_bdev_t *bdev,
                   const void *buf, fnx_lba_t lba, fnx_bkcnt_t cnt)
{
	int rc;
	loff_t  off;
	size_t  len;
	ssize_t res;

	if (!fnx_bdev_isopen(bdev)) {
		return -EBADF;
	}
	if (!bdev_isinrange(bdev, lba, cnt)) {
		return -EINVAL;
	}
	len = bdev_bytesize(bdev, cnt);
	off = (off_t)bdev_bytesize(bdev, bdev->base + lba);
	res = pwrite64(bdev->fd, buf, len, off);
	if (res != (ssize_t)len) {
		rc = -errno;
		fnx_error("pwrite-failure: fd=%d cnt=%zu off=%zu off-max=%zu err=%d",
		          bdev->fd, len, off, bdev_offmax(bdev), -rc);
		return rc;
	}
	return 0;
}

int fnx_bdev_sync(const fnx_bdev_t *bdev)
{
	int rc;

	if (!fnx_bdev_isopen(bdev)) {
		return -EBADF;
	}
	/*rc = fsync(bdev->fd);*/
	rc = fdatasync(bdev->fd);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int bdev_ismmaped(const fnx_bdev_t *bdev)
{
	return (bdev->maddr != NULL);
}

int fnx_bdev_mmap(fnx_bdev_t *bdev)
{
	int rc, prot, flags, rdwr;
	off_t off;
	size_t len;
	void *addr = NULL;

	if (!fnx_bdev_isopen(bdev)) {
		return -EBADF;
	}
	if (bdev_ismmaped(bdev)) {
		return -EALREADY;
	}
	if (bdev->bcap == 0) {
		return -ERANGE;
	}

	len     = bdev_bytesize(bdev, bdev->bcap);
	rdwr    = bdev_testf(bdev, FNX_BDEV_RDWR);
	prot    = (rdwr ? (PROT_READ | PROT_WRITE) : PROT_READ);
	flags   = MAP_SHARED;
	off     = 0;
	addr = mmap(NULL, len, prot, flags, bdev->fd, off);
	if (addr == MAP_FAILED) {
		rc = -errno;
		fnx_trace1("no-mmap: len=%zu prot=%d flags=%d fd=%d off=%jd err=%d",
		           len, prot, flags, bdev->fd, off, -rc);
		return rc;
	}

	rc = madvise(addr, len, MADV_RANDOM);
	if (rc != 0) {
		rc = -errno;
		fnx_error("no-madvise: addr=%p len=%zu fd=%d err=%d",
		          addr, len, bdev->fd, rc);
		munmap(addr, len);
		return rc;
	}

	bdev->maddr = addr;
	return 0;
}

int fnx_bdev_munmap(fnx_bdev_t *bdev)
{
	int rc;
	fnx_size_t len;

	if (!fnx_bdev_isopen(bdev)) {
		return -EBADF;
	}
	if (!bdev_ismmaped(bdev)) {
		return -ENOTSUP;
	}
	len = bdev_bytesize(bdev, bdev->bcap);
	rc  = munmap(bdev->maddr, len);
	if (rc != 0) {
		return -errno;
	}
	bdev->maddr = NULL;
	return 0;
}

int fnx_bdev_mgetp(const fnx_bdev_t *bdev, fnx_lba_t lba, void **p)
{
	char *addr = NULL;
	fnx_lba_t pos;

	if (!fnx_bdev_isopen(bdev)) {
		return -EBADF;
	}
	if (!bdev_ismmaped(bdev)) {
		return -ENOTSUP;
	}
	if (!bdev_isinrange(bdev, lba, 1)) {
		return -ERANGE;
	}

	addr = (char *)bdev->maddr;
	pos  = bdev_bytesize(bdev, lba);

	*p = (void *)(addr + pos);
	return 0;
}

int fnx_bdev_msync(const fnx_bdev_t *bdev,
                   fnx_lba_t lba, fnx_bkcnt_t cnt, int now)
{
	int rc, flags;
	size_t pos, len;
	char *addr;

	if (!fnx_bdev_isopen(bdev)) {
		return -EBADF;
	}
	if (!bdev_ismmaped(bdev)) {
		return -ENOTSUP;
	}
	if (!bdev_isinrange(bdev, lba, cnt)) {
		return -ERANGE;
	}

	flags   = now ? MS_SYNC : MS_ASYNC;
	pos     = bdev_bytesize(bdev, lba);
	len     = bdev_bytesize(bdev, cnt);
	addr    = ((char *)bdev->maddr) + pos;
	rc = msync(addr, len, flags);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_bdev_mflush(const fnx_bdev_t *bdev, int now)
{
	return fnx_bdev_msync(bdev, 0, bdev->bcap, now);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int bdev_isflocked(const fnx_bdev_t *bdev)
{
	return bdev_testf(bdev, FNX_BDEV_FLOCKED);
}

static int bdev_flock_op(const fnx_bdev_t *bdev, int op)
{
	int rc;

	rc = flock(bdev->fd, op);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_bdev_flock(fnx_bdev_t *bdev)
{
	int rc;

	if (!fnx_bdev_isopen(bdev)) {
		return -EBADF;
	}
	if (!bdev_testf(bdev, FNX_BDEV_LOCK)) {
		return -ENOTSUP;
	}
	if (bdev_isflocked(bdev)) {
		return -EALREADY;
	}
	rc = bdev_flock_op(bdev, LOCK_EX);
	if (rc != 0) {
		return rc;
	}
	bdev_setf(bdev, FNX_BDEV_FLOCKED);
	return 0;
}

int fnx_bdev_tryflock(fnx_bdev_t *bdev)
{
	int rc;

	if (!fnx_bdev_isopen(bdev)) {
		return -EBADF;
	}
	if (!bdev_testf(bdev, FNX_BDEV_LOCK)) {
		return -ENOTSUP;
	}
	if (bdev_isflocked(bdev)) {
		return -EALREADY;
	}
	rc = bdev_flock_op(bdev, LOCK_EX | LOCK_NB);
	if (rc != 0) {
		return rc;
	}
	bdev_setf(bdev, FNX_BDEV_FLOCKED);
	return 0;
}

int fnx_bdev_funlock(fnx_bdev_t *bdev)
{
	int rc;

	if (!fnx_bdev_isopen(bdev)) {
		return -EBADF;
	}
	if (!bdev_testf(bdev, FNX_BDEV_LOCK)) {
		return -ENOTSUP;
	}
	if (!bdev_isflocked(bdev)) {
		return -EINVAL;
	}
	rc = bdev_flock_op(bdev, LOCK_UN);
	if (rc != 0) {
		return rc;
	}
	bdev_unsetf(bdev, FNX_BDEV_FLOCKED);
	return 0;
}


