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
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/ioctl.h>

#define FUSE_USE_VERSION    28
#include <fuse/fuse_lowlevel.h>
#include <fuse/fuse_common.h>
#include <fuse/fuse_opt.h>

#include <fnxinfra.h>
#include <fnxcore.h>
#include "fusei-elems.h"
#include "fusei-reply.h"


#define IOBUFS_NBK(z) (FNX_NELEMS(z->iob[0].bks) * FNX_NELEMS(z->iob))


static fnx_fuse_req_t getreq(const fnx_task_t *task)
{
	return task->tsk_fuse_req;
}

static fuse_ino_t fnx_ino_to_fuse_ino(fnx_ino_t ino)
{
	const fuse_ino_t fuse_ino = (fuse_ino_t)ino;
	fnx_assert(fnx_ino_isvalid(ino));
	return fuse_ino;
}

static fnx_bool_t isfatalerr(int err)
{
	if (err < 0) {
		err = -err;
	}
	return ((err != 0) && (err != ENOENT) && (err != EPIPE));
}

static void fnx_fuse_check_rc(int rc)
{
	if (isfatalerr(rc)) {
		fnx_panic("fuse-reply-error rc=%d", rc);
	}
}

static void
fill_entry_param(struct fuse_entry_param *entry, const fnx_iattr_t *iattr,
                 fnx_size_t attr_timeout, fnx_size_t entry_timeout)
{
	fuse_ino_t  ino;

	fnx_bzero(entry, sizeof(*entry));
	ino = fnx_ino_to_fuse_ino(iattr->i_ino);
	entry->ino           = ino;
	entry->generation    = iattr->i_gen;
	if ((attr_timeout > 0) && (entry_timeout > 0)) {
		entry->attr_timeout  = (float)attr_timeout + 0.05f;
		entry->entry_timeout = (float)entry_timeout + 0.05f;
	}

	fnx_iattr_to_stat(iattr, &entry->attr);
	entry->attr.st_ino = ino;
}

static void
fill_file_info(struct fuse_file_info *file_info, const fnx_fileref_t *fref)
{
	size_t sz;

	fnx_assert(fref != NULL);
	fnx_bzero(file_info, sizeof(*file_info));

	sz = fnx_min(sizeof(file_info->fh), sizeof(void *));
	memcpy(&file_info->fh, &fref, sz);

	/* NB: direct_io=1 causes mmap to fail */
	file_info->direct_io    = fref->f_direct_io;
	file_info->keep_cache   = fref->f_keep_cache;
	file_info->nonseekable  = fref->f_nonseekable;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_fuse_reply_none(fnx_fuse_req_t req)
{
	fuse_reply_none(req);
}

static void fnx_fuse_reply_req(fnx_fuse_req_t req, int st)
{
	int rc;

	fnx_assert(st != FNX_EPEND);
	fnx_assert(st != FNX_EEOS);
	if (st < 0) {
		st = -st;
	}
	rc = fuse_reply_err(req, st);
	fnx_fuse_check_rc(rc);
}

void fnx_fuse_reply_invalid(fnx_fuse_req_t req)
{
	fnx_fuse_reply_req(req, EINVAL);
}

void fnx_fuse_reply_badname(fnx_fuse_req_t req)
{
	fnx_fuse_reply_req(req, ENAMETOOLONG);
}

void fnx_fuse_reply_nosys(fnx_fuse_req_t req)
{
	fnx_fuse_reply_req(req, ENOSYS);
}

void fnx_fuse_reply_notsupp(fnx_fuse_req_t req)
{
	fnx_fuse_reply_req(req, EOPNOTSUPP);
}

void fnx_fuse_reply_nomem(fnx_fuse_req_t req)
{
	fnx_fuse_reply_req(req, ENOMEM);
}

void fnx_fuse_reply_status(const fnx_task_t *task)
{
	fnx_fuse_reply_req(getreq(task), fnx_task_status(task));
}

void fnx_fuse_reply_nwrite(const fnx_task_t *task, size_t count)
{
	int rc;

	rc = fuse_reply_write(getreq(task), count);
	fnx_fuse_check_rc(rc);
}

void fnx_fuse_reply_iattr(const fnx_task_t *task, const fnx_iattr_t *iattr)
{
	int rc;
	double attr_timeout;
	struct stat st;

	/* Validity timeout (in seconds) for the attributes; */
	/* TODO: Fix? */
	attr_timeout = 1.0f;

	fnx_iattr_to_stat(iattr, &st);
	st.st_ino = fnx_ino_to_fuse_ino(iattr->i_ino);

	rc = fuse_reply_attr(getreq(task), &st, attr_timeout);
	fnx_fuse_check_rc(rc);
}

void fnx_fuse_reply_entry(const fnx_task_t *task, const fnx_iattr_t *iattr)
{
	int rc;
	struct fuse_entry_param entry_param;
	const fnx_fuseinfo_t *info = &task->tsk_fusei->fi_info;

	fill_entry_param(&entry_param, iattr,
	                 info->attr_timeout, info->entry_timeout);
	rc = fuse_reply_entry(getreq(task), &entry_param);
	fnx_fuse_check_rc(rc);
}

void fnx_fuse_reply_create(const fnx_task_t *task, const fnx_fileref_t *fref,
                           const fnx_iattr_t *iattr)
{
	int rc;
	struct fuse_file_info file_info;
	struct fuse_entry_param entry_param;
	const fnx_fuseinfo_t *info = &task->tsk_fusei->fi_info;

	fill_entry_param(&entry_param, iattr,
	                 info->attr_timeout, info->entry_timeout);
	fill_file_info(&file_info, fref);

	rc = fuse_reply_create(getreq(task), &entry_param, &file_info);
	fnx_fuse_check_rc(rc);
}

void fnx_fuse_reply_readlink(const fnx_task_t *task, const char *lnkval)
{
	int rc;

	rc = fuse_reply_readlink(getreq(task), lnkval);
	fnx_fuse_check_rc(rc);
}

void fnx_fuse_reply_open(const fnx_task_t *task, const fnx_fileref_t *fref)
{
	int rc;
	struct fuse_file_info file_info;

	fill_file_info(&file_info, fref);

	rc = fuse_reply_open(getreq(task), &file_info);
	fnx_fuse_check_rc(rc);
}

void fnx_fuse_reply_statvfs(const fnx_task_t *task, const fnx_fsinfo_t *fsinfo)
{
	int rc;
	struct statvfs stvfs;

	fnx_fsinfo_to_statvfs(fsinfo, &stvfs);
	rc = fuse_reply_statfs(getreq(task), &stvfs);
	fnx_fuse_check_rc(rc);
}

void fnx_fuse_reply_buf(const fnx_task_t *task, const void *buf, size_t bsz)
{
	int rc;
	const char *pb;

	pb = (const char *)buf;
	rc = fuse_reply_buf(getreq(task), pb, bsz);
	fnx_fuse_check_rc(rc);
}

void fnx_fuse_reply_zbuf(const fnx_task_t *task)
{
	fnx_fuse_reply_buf(task, NULL, 0);
}

void fnx_fuse_reply_iobufs(const fnx_task_t *task, const fnx_iobufs_t *iobufs)
{
	int rc;
	size_t cnt = 0;
	fnx_iovec_t iov[2 * FNX_RSEGNBK];

	fnx_staticassert(FNX_NELEMS(iov) >= IOBUFS_NBK(iobufs));

	rc = fnx_iobufs_mkiovec(iobufs, iov, fnx_nelems(iov), &cnt);
	fnx_assert(rc == 0);

	if (cnt == 1) {
		/* Optimizatoin for short reads */
		fnx_fuse_reply_buf(task, iov[0].iov_base, iov[0].iov_len);
	} else {
		rc = fuse_reply_iov(getreq(task), iov, (int)cnt);
		fnx_fuse_check_rc(rc);
	}
}

void fnx_fuse_reply_ioctl(const fnx_task_t *task, const void *buf, size_t bufsz)
{
	int rc, status;

	status = 0; /* FIXME */
	rc = fuse_reply_ioctl(getreq(task), status, buf, bufsz);
	fnx_fuse_check_rc(rc);
}

static size_t
add_direntry(fnx_fuse_req_t req, fnx_ino_t child_ino, const char *name,
             fnx_mode_t mode, fnx_off_t off_next, void *buf, size_t bsz)
{
	size_t sz;
	struct stat st;

	fnx_bzero(&st, sizeof(st));
	st.st_ino  = fnx_ino_to_fuse_ino(child_ino);
	st.st_mode = mode;

	sz = fuse_add_direntry(req, (char *)buf, bsz, name, &st, off_next);
	if (sz > bsz) {
		fnx_panic("fuse_add_direntry name=%s sz=%lu", name, sz);
	}
	return sz;
}

void fnx_fuse_reply_readdir(const fnx_task_t *task, size_t isz,
                            fnx_ino_t child_ino, const char *name,
                            fnx_mode_t mode, fnx_off_t off_next)
{
	size_t bsz, osz;
	char buf[2 * (FNX_NAME_MAX + 1)];
	fnx_fuse_req_t req = getreq(task);

	bsz  = fnx_min(sizeof(buf), isz);
	osz  = add_direntry(req, child_ino, name, mode, off_next, buf, bsz);
	fnx_fuse_reply_buf(task, buf, osz);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_fuse_notify_inval_iattr(const fnx_task_t *task,
                                 const fnx_iattr_t *iattr)
{
	int rc;
	fnx_ino_t ino;
	struct fuse_chan *ch;

	ch  = task->tsk_fuse_chan;
	ino = iattr->i_ino;
	rc  = fuse_lowlevel_notify_inval_inode(ch, ino, -1, 0);
	if (rc != 0) {
		fnx_error("fuse_lowlevel_notify_inval_inode: ino=%#jx rc=%d", ino, rc);
	}
}

