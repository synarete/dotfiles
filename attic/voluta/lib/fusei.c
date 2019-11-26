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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <linux/fs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define FUSE_USE_VERSION 34
#include <fuse3/fuse_lowlevel.h>
#include <fuse3/fuse_opt.h>
#include <fuse3/fuse.h>

#include "voluta-lib.h"

/* Local defs */
#define FUSEI_ATTR_TIMEOUT    (1000000.0f)
#define FUSEI_ENTRY_TIMEOUT   (1000000.0f)

#define FUSEI_DATA_BUFSIZE      VOLUTA_IO_SIZE_MAX
#define FUSEI_READDIR_BUFSIZE   (64 * VOLUTA_KILO)
#define FUSEI_XATTR_BUFSIZE     (64 * VOLUTA_KILO)

/* Local types */
struct voluta_fusei_bufvec {
	struct fuse_bufvec bv;
	struct fuse_buf buf[VOLUTA_IO_NBK_MAX];
};


struct voluta_fusei_rw_iter {
	struct voluta_fusei_bufvec bv;
	struct voluta_rw_iter rwi;
	struct fuse_bufvec *bvec_in;
	uint8_t *dat;
	size_t dat_len;
	size_t dat_max;
	bool with_splice;
};


struct voluta_fusei_databuf {
	uint8_t data[FUSEI_DATA_BUFSIZE];
};


struct voluta_fusei_rw {
	struct voluta_fusei_databuf db;
	struct voluta_fusei_rw_iter rwi;
	struct voluta_fusei_bufvec  bv;
};


struct voluta_fusei_diter {
	struct voluta_readdir_ctx readdir_ctx;
	struct voluta_fusei *fusei;
	const char *beg;
	const char *end;
	char *cur;
	size_t ndents;
	loff_t dent_off;
	size_t dent_namelen;
	struct stat dent_stat;
	char dent_name[VOLUTA_NAME_MAX + 1];
	char buf[FUSEI_READDIR_BUFSIZE];
};


struct voluta_fusei_xiter {
	struct voluta_listxattr_ctx listxattr_ctx;
	struct voluta_fusei *fusei;
	size_t count;
	const char *beg;
	const char *end;
	char *cur;
	char buf[FUSEI_XATTR_BUFSIZE];
};


struct voluta_fusei_pathbuf {
	char path[VOLUTA_PATH_MAX];
};


union voluta_fusei_u {
	struct voluta_fusei_rw      rw;
	struct voluta_fusei_pathbuf pb;
	struct voluta_fusei_diter   di;
	struct voluta_fusei_xiter   xi;
};


/* Local convenience macros */
#define req_to_fusei(req_, pfusei_)                      \
	do { if (!req_to_fusei_or_inval(req, pfusei_)) return; } while(0)

#define require_arg(fusei, fn, arg, st) \
	do { if (reply_if_not(fusei, fn(arg), st)) return; } while (0)

#define require_ino(fusei, ino) \
	require_arg(fusei, isvalid_ino, ino, EINVAL)

#define require_fi_(fusei, fi) \
	require_arg(fusei, isvalid_file_info, fi, EINVAL)

#define require_fi(fusei, fi) \
	do { require_fi_(fusei, fi); update_fusei(fusei, fi); } while(0)

#define require_ok(fusei, err) \
	do { if (reply_if_not(fusei, (err == 0), err)) return; } while (0)

/* Local functions forward declarations */
static void reply_inval(fuse_req_t req);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t min_size(size_t a, size_t b)
{
	return (a < b) ? a : b;
}

static bool isvalid_ino(ino_t ino)
{
	return (ino > 0);
}

static bool isvalid_file_info(const struct fuse_file_info *fi)
{
	return ((fi == NULL) || (fi->fh == 0));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_fusei *resolve_fusei(struct fuse_req *req)
{
	struct voluta_fusei *fusei;
	const struct fuse_ctx *curr_req_ctx;

	fusei = fuse_req_userdata(req);
	voluta_assert_eq(fusei->magic, VOLUTA_MAGIC);
	if (fusei->magic != VOLUTA_MAGIC) {
		return NULL;
	}
	curr_req_ctx = fuse_req_ctx(req);
	if (!curr_req_ctx) {
		return NULL;
	}
	fusei->ucred->uid = curr_req_ctx->uid;
	fusei->ucred->gid = curr_req_ctx->gid;
	fusei->ucred->pid = curr_req_ctx->pid;
	fusei->ucred->umask = curr_req_ctx->umask;
	fusei->req = req;
	fusei->err = 0;
	return fusei;
}

static void update_fusei(struct voluta_fusei *fusei,
			 const struct fuse_file_info *fi)
{
	/*
	 * FUSE docs says that 'writepage' flag indicates that a delayed write
	 * by page-cache, which implies non-valid credentials.
	 */
	if (fi && fi->writepage) {
		fusei->ucred->uid = 0;
		fusei->ucred->gid = 0;
		fusei->ucred->pid = 1;
		fusei->ucred->umask = 0002;
	}
}

static struct voluta_fusei *resolve_fusei_or_inval(struct fuse_req *req)
{
	struct voluta_fusei *fusei;

	fusei = resolve_fusei(req);
	if (fusei == NULL)  {
		reply_inval(req);
	}
	return fusei;
}

static bool req_to_fusei_or_inval(struct fuse_req *req,
				  struct voluta_fusei **fusei)
{
	*fusei = resolve_fusei_or_inval(req);

	return (*fusei != NULL);
}

static struct voluta_env *env_of(const struct voluta_fusei *fusei)
{
	return fusei->env;
}

static bool is_conn_cap(const struct voluta_fusei *fusei, unsigned int mask)
{
	return (fusei->conn_caps & mask) == mask;
}

static bool cap_splice(const struct voluta_fusei *fusei)
{
	unsigned int mask = FUSE_CAP_SPLICE_READ | FUSE_CAP_SPLICE_WRITE;

	return is_conn_cap(fusei, mask);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void fill_entry_param(struct fuse_entry_param *ep, const struct stat *st)
{
	memset(ep, 0, sizeof(*ep));
	ep->ino = st->st_ino;
	ep->generation = 0;
	ep->attr_timeout = FUSEI_ATTR_TIMEOUT;
	ep->entry_timeout = FUSEI_ENTRY_TIMEOUT;
	memcpy(&ep->attr, st, sizeof(ep->attr));
}

static void fill_buf(struct fuse_buf *fb, const struct voluta_qmref *qmr)
{
	fb->fd = qmr->fd;
	fb->pos = qmr->off;
	fb->size = qmr->len;
	fb->mem = qmr->mem;
	fb->flags = FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK;
}

static void fill_bufvec(struct fuse_bufvec *bv,
			const struct voluta_qmref *qmr, size_t cnt)
{
	bv->count = cnt;
	bv->idx = 0;
	bv->off = 0;
	for (size_t i = 0; i < cnt; ++i) {
		fill_buf(&bv->buf[i], &qmr[i]);
	}
}

static void add_to_bufvec(struct fuse_bufvec *bv,
			  const struct voluta_qmref *qmr)
{
	fill_buf(&bv->buf[bv->count++], qmr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_fusei_diter *diter_of(struct voluta_readdir_ctx *ptr)
{
	return voluta_container_of(ptr, struct voluta_fusei_diter, readdir_ctx);
}

static size_t diter_length(const struct voluta_fusei_diter *diter)
{
	return (size_t)(diter->cur - diter->beg);
}

static void update_dirent(struct voluta_fusei_diter *diter,
			  const struct voluta_readdir_entry_info *rdei)
{
	struct stat *dent_stat = &diter->dent_stat;
	const size_t namelen_max = sizeof(diter->dent_name) - 1;

	memset(dent_stat, 0, sizeof(*dent_stat));
	diter->dent_namelen = min_size(rdei->name_len, namelen_max);
	strncpy(diter->dent_name, rdei->name, diter->dent_namelen);
	diter->dent_name[diter->dent_namelen] = '\0';
	diter->dent_off = rdei->off;
	dent_stat->st_ino = rdei->ino;
	dent_stat->st_mode = DTTOIF(rdei->dt);
}

static int emit_dirent(struct voluta_fusei_diter *diter, loff_t doff_next)
{
	char *buf;
	size_t bsz, cnt;
	fuse_req_t req;

	buf = diter->cur;
	if (buf >= diter->end) {
		return -EINVAL;
	}

	req = diter->fusei->req;
	bsz = (size_t)(diter->end - buf);
	cnt = fuse_add_direntry(req, buf, bsz, diter->dent_name,
				&diter->dent_stat, doff_next);
	if (!cnt || (cnt > bsz)) {
		return -EINVAL;
	}
	diter->ndents++;
	diter->cur += cnt;
	return 0;
}

static bool has_dirent(const struct voluta_fusei_diter *diter)
{
	return (diter->dent_stat.st_ino > 0) &&
	       (diter->dent_namelen > 0);
}

static int filldir(struct voluta_readdir_ctx *readdir_ctx,
		   const struct voluta_readdir_entry_info *rdei)
{
	int err = 0;
	struct voluta_fusei_diter *diter_ctx;

	diter_ctx = diter_of(readdir_ctx);
	if (has_dirent(diter_ctx)) {
		err = emit_dirent(diter_ctx, rdei->off);
	}
	if (!err) {
		update_dirent(diter_ctx, rdei);
	}
	return err;
}

static struct voluta_fusei_diter *
diter_prep(struct voluta_fusei *fusei, size_t bsz_in, loff_t pos)
{
	size_t bsz;
	struct voluta_fusei_diter *diter = &fusei->u->di;

	bsz = min_size(bsz_in, sizeof(diter->buf));
	diter->ndents = 0;
	diter->dent_off = 0;
	diter->dent_namelen = 0;
	diter->beg = diter->buf;
	diter->end = diter->buf + bsz;
	diter->cur = diter->buf;
	diter->readdir_ctx.actor = filldir;
	diter->readdir_ctx.pos = pos;
	diter->fusei = fusei;
	memset(diter->dent_name, 0, sizeof(diter->dent_name));
	memset(&diter->dent_stat, 0, sizeof(diter->dent_stat));
	return diter;
}

static void diter_done(struct voluta_fusei_diter *diter)
{
	diter->ndents = 0;
	diter->dent_off = 0;
	diter->dent_namelen = 0;
	diter->beg = NULL;
	diter->end = NULL;
	diter->cur = NULL;
	diter->readdir_ctx.actor = NULL;
	diter->readdir_ctx.pos = 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_fusei_xiter *xiter_of(struct voluta_listxattr_ctx *p)
{
	return voluta_container_of(p, struct voluta_fusei_xiter, listxattr_ctx);
}

static size_t xiter_avail(const struct voluta_fusei_xiter *xiter)
{
	return (size_t)(xiter->end - xiter->cur);
}

static int fillxent(struct voluta_listxattr_ctx *listxent_ctx,
		    const char *name, size_t nlen)
{
	size_t avail, size = nlen + 1;
	struct voluta_fusei_xiter *xiter = xiter_of(listxent_ctx);

	avail = xiter_avail(xiter);
	if (xiter->cur) {
		if (avail < size) {
			return -ERANGE;
		}
		memcpy(xiter->cur, name, nlen);
		xiter->cur[nlen] = '\0';
		xiter->cur += size;
	}
	xiter->count += size;
	return 0;
}

static struct voluta_fusei_xiter *xiter_prep(struct voluta_fusei *fusei,
		size_t size)
{
	struct voluta_fusei_xiter *xiter = &fusei->u->xi;

	xiter->listxattr_ctx.actor = fillxent;
	xiter->fusei = fusei;
	xiter->count = 0;
	if (size) {
		xiter->beg = xiter->buf;
		xiter->end = xiter->beg + sizeof(xiter->buf);
		xiter->cur = xiter->buf;
	} else {
		xiter->beg = NULL;
		xiter->end = NULL;
		xiter->cur = NULL;
	}
	return xiter;
}

static void xiter_done(struct voluta_fusei *fusei)
{
	struct voluta_fusei_xiter *xiter = &fusei->u->xi;

	xiter->listxattr_ctx.actor = NULL;
	xiter->fusei = NULL;
	xiter->count = 0;
	xiter->beg = NULL;
	xiter->end = NULL;
	xiter->cur = NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void reply_inval(fuse_req_t req)
{
	/* No fusei associated with req here! */
	fuse_reply_err(req, EINVAL);
}

static void reply_none(struct voluta_fusei *fusei)
{
	fuse_reply_none(fusei->req);
	fusei->err = 0;
	fusei->req = NULL;
}

static void reply_nonsys(struct voluta_fusei *fusei)
{
	fusei->err = fuse_reply_err(fusei->req, ENOSYS);
	fusei->req = NULL;
}

static void reply_status(struct voluta_fusei *fusei, int status)
{
	int err;

	err = (status < 0) ? -status : status;
	fusei->err = fuse_reply_err(fusei->req, err);
	fusei->req = NULL;
}

static bool reply_if_not(struct voluta_fusei *fusei, bool cond, int status)
{
	if (!cond) {
		reply_status(fusei, status);
	}
	return !cond;
}

static void reply_err(struct voluta_fusei *fusei, int err)
{
	reply_status(fusei, err);
}

static void reply_statfs(struct voluta_fusei *fusei,
			 const struct statvfs *stvfs)
{
	fusei->err = fuse_reply_statfs(fusei->req, stvfs);
	fusei->req = NULL;
}

static void reply_statfs_or_err(struct voluta_fusei *fusei,
				const struct statvfs *stvfs, int err)
{
	if (!err) {
		reply_statfs(fusei, stvfs);
	} else {
		reply_err(fusei, err);
	}
}

static void reply_entry(struct voluta_fusei *fusei, const struct stat *st)
{
	struct fuse_entry_param entry_param;

	fill_entry_param(&entry_param, st);
	fusei->err = fuse_reply_entry(fusei->req, &entry_param);
	fusei->req = NULL;
}

static void reply_entry_or_err(struct voluta_fusei *fusei,
			       const struct stat *st, int err)
{
	if (!err) {
		reply_entry(fusei, st);
	} else {
		reply_err(fusei, err);
	}
}

static void reply_attr(struct voluta_fusei *fusei, const struct stat *st)
{
	const double attr_timeout = FUSEI_ATTR_TIMEOUT;

	fusei->err = fuse_reply_attr(fusei->req, st, attr_timeout);
	fusei->req = NULL;
}

static void reply_attr_or_err(struct voluta_fusei *fusei,
			      const struct stat *st, int err)
{
	if (!err) {
		reply_attr(fusei, st);
	} else {
		reply_err(fusei, err);
	}
}

static void reply_readlink(struct voluta_fusei *fusei, const char *lnk)
{
	fusei->err = fuse_reply_readlink(fusei->req, lnk);
	fusei->req = NULL;
}

static void reply_readlink_or_err(struct voluta_fusei *fusei,
				  const char *lnk, int err)
{
	if (!err) {
		reply_readlink(fusei, lnk);
	} else {
		reply_err(fusei, err);
	}
}

static void reply_open(struct voluta_fusei *fusei)
{
	struct fuse_file_info fi;

	memset(&fi, 0, sizeof(fi));
	fi.keep_cache = 1;

	fusei->err = fuse_reply_open(fusei->req, &fi);
	fusei->req = NULL;
}

static void reply_open_or_err(struct voluta_fusei *fusei, int err)
{
	if (!err) {
		reply_open(fusei);
	} else {
		reply_err(fusei, err);
	}
}

static void reply_opendir(struct voluta_fusei *fusei)
{
	struct fuse_file_info fi;

	memset(&fi, 0, sizeof(fi));
	fi.cache_readdir = 1;

	fusei->err = fuse_reply_open(fusei->req, &fi);
	fusei->req = NULL;
}

static void reply_opendir_or_err(struct voluta_fusei *fusei, int err)
{
	if (!err) {
		reply_opendir(fusei);
	} else {
		reply_err(fusei, err);
	}
}

static void reply_buf(struct voluta_fusei *fusei,
		      const void *buf, size_t bsz)
{
	fusei->err = fuse_reply_buf(fusei->req, buf, bsz);
	fusei->req = NULL;
}

static inline void reply_buf_or_err(struct voluta_fusei *fusei, const void *buf,
				    size_t bsz, int err)
{
	if (!err) {
		reply_buf(fusei, buf, bsz);
	} else {
		reply_err(fusei, err);
	}
}

static inline void reply_iov(struct voluta_fusei *fusei,
			     const struct iovec *iov, size_t cnt)
{
	fusei->err = fuse_reply_iov(fusei->req, iov, (int)cnt);
	fusei->req = NULL;
}

static void reply_bufvec(struct voluta_fusei *fusei, struct fuse_bufvec *bufv)
{
	fusei->err = fuse_reply_data(fusei->req, bufv, 0);
	fusei->req = NULL;
}

static void reply_read(struct voluta_fusei *fusei)
{
	struct voluta_fusei_rw_iter *rwi = &fusei->u->rw.rwi;

	if (!cap_splice(fusei)) {
		reply_buf(fusei, rwi->dat, rwi->dat_len);
	} else {
		reply_bufvec(fusei, &rwi->bv.bv);
	}
}

static void reply_read_or_err(struct voluta_fusei *fusei, int err)
{
	if (!err) {
		reply_read(fusei);
	} else {
		reply_err(fusei, err);
	}
}

static void reply_write(struct voluta_fusei *fusei, size_t count)
{
	fusei->err = fuse_reply_write(fusei->req, count);
	fusei->req = NULL;
}

static void reply_write_or_err(struct voluta_fusei *fusei, size_t count,
			       int err)
{
	if (!err) {
		reply_write(fusei, count);
	} else {
		reply_err(fusei, err);
	}
}

static void reply_create(struct voluta_fusei *fusei, const struct stat *st)
{
	struct fuse_file_info fi;
	struct fuse_entry_param entry_param;

	memset(&fi, 0, sizeof(fi));
	fi.direct_io = 0;
	fi.keep_cache = 1;

	memset(&entry_param, 0, sizeof(entry_param));
	entry_param.ino = st->st_ino;
	entry_param.generation = 1;
	entry_param.attr_timeout = FUSEI_ATTR_TIMEOUT;
	entry_param.entry_timeout = FUSEI_ENTRY_TIMEOUT;
	memcpy(&entry_param.attr, st, sizeof(entry_param.attr));

	fusei->err = fuse_reply_create(fusei->req, &entry_param, &fi);
	fusei->req = NULL;
}

static void reply_create_or_err(struct voluta_fusei *fusei,
				const struct stat *st, int err)
{
	if (!err) {
		reply_create(fusei, st);
	} else {
		reply_err(fusei, err);
	}
}

static void reply_readdir_or_err(struct voluta_fusei *fusei,
				 struct voluta_fusei_diter *diter, int err)
{
	size_t bsz;
	const void *buf;

	if (!err) {
		bsz = diter_length(diter);
		buf = (bsz > 0) ? diter->beg : NULL;
		reply_buf(fusei, buf, bsz);
	} else {
		reply_err(fusei, err);
	}
}

static void reply_xattr(struct voluta_fusei *fusei, size_t insize,
			const void *buf, size_t size)
{
	if (insize > 0) {
		fusei->err = fuse_reply_buf(fusei->req, buf, size);
	} else {
		fusei->err = fuse_reply_xattr(fusei->req, size);
	}
	fusei->req = NULL;
}

static void reply_xattr_or_err(struct voluta_fusei *fusei, size_t insize,
			       const void *buf, size_t size, int err)
{
	if (!err) {
		reply_xattr(fusei, insize, buf, size);
	} else {
		reply_err(fusei, err);
	}
}

static void reply_ioctl(struct voluta_fusei *fusei, int result,
			const void *buf, size_t size)
{
	fusei->err = fuse_reply_ioctl(fusei->req, result, buf, size);
	fusei->req = NULL;
}

static void reply_ioctl_or_err(struct voluta_fusei *fusei, int result,
			       const void *buf, size_t size, int err)
{
	if (!err) {
		reply_ioctl(fusei, result, buf, size);
	} else {
		reply_err(fusei, err);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void set_want(struct fuse_conn_info *conn, unsigned int mask)
{
	if ((conn->capable & mask) == mask) {
		conn->want |= mask;
	}
}

static unsigned int data_buf_size(const struct voluta_fusei *fusei)
{
	return (unsigned int)sizeof(fusei->u->rw.db.data);
}

static unsigned int rdwr_max_size(const struct voluta_fusei *fusei)
{
	/*
	 * TODO: Fix this crap XXX
	 * fuse uses fctnl F_SETPIPE_SZ on pipe, which is limited by system
	 * value of /proc/sys/fs/pipe-max-size
	 *
	 * When using splice, libfuse need this size + FUSE_BUFFER_HEADER_SIZE.
	 * Crap, crap, crap! need more investigating. Or better, re-write this
	 * pile of crap.
	 */
	size_t pipe_max_size, data_max_size, max_size;

	pipe_max_size = VOLUTA_MEGA; /* should be /proc/sys/fs/pipe-max-size */
	data_max_size = data_buf_size(fusei);
	max_size = voluta_min(pipe_max_size, data_max_size);

	/* from libfuse lib/fuse_i.h */
#define FUSE_BUFFER_HEADER_SIZE 0x1000

	return (unsigned int)max_size -  FUSE_BUFFER_HEADER_SIZE;
}

static void do_init(void *userdata, struct fuse_conn_info *coni)
{
	struct voluta_fusei *fusei = userdata;

	voluta_assert_eq(fusei->magic, VOLUTA_MAGIC);

	coni->want = 0;
	set_want(coni, FUSE_CAP_ATOMIC_O_TRUNC);
	set_want(coni, FUSE_CAP_EXPORT_SUPPORT);
	set_want(coni, FUSE_CAP_SPLICE_WRITE);
	set_want(coni, FUSE_CAP_SPLICE_READ);
	set_want(coni, FUSE_CAP_HANDLE_KILLPRIV);
	/* set_want(conn, FUSE_CAP_DONT_MASK); */
	/*
	 * XXX: When enabling FUSE_CAP_WRITEBACK_CACHE 'git tests' pass ok,
	 * but fstests fail with ctime issues. Needs further investigation.
	 *
	 * XXX: Also, bugs in 'test_truncate_zero'
	 */
	/* set_want(coni, FUSE_CAP_WRITEBACK_CACHE); */


	/* TODO: Align? that is, have I/O buffer aligned at source */
	coni->max_write = rdwr_max_size(fusei);
	coni->max_readahead = coni->max_read = coni->max_write;

	coni->time_gran = 1;

	/* Store connection's params internally */
	fusei->conn_caps = coni->capable;

	voluta_tr_info("fuse-init: userdata=%p capable=%#x",
		       userdata, fusei->conn_caps);
}

static void do_destroy(void *userdata)
{
	const struct voluta_fusei *fusei = userdata;

	/* XXX: TODO me */
	voluta_assert_eq(fusei->magic, VOLUTA_MAGIC);

	voluta_tr_info("fuse-destroy: userdata=%p capable=%#x",
		       userdata, fusei->conn_caps);
}

static void do_forget(fuse_req_t req, ino_t ino, uint64_t nlookup)
{
	int err;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	err = voluta_fs_forget(env_of(fusei), ino);
	reply_none(fusei);
	voluta_unused(err);
	voluta_unused(nlookup);
}

static void do_statfs(fuse_req_t req, ino_t ino)
{
	int err;
	struct statvfs stvfs;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	err = voluta_fs_statfs(env_of(fusei), ino, &stvfs);
	reply_statfs_or_err(fusei, &stvfs, err);
}

static void do_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	int err;
	struct stat st;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, parent);

	err = voluta_fs_lookup(env_of(fusei), parent, name, &st);
	reply_entry_or_err(fusei, &st, err);
}

static void do_mkdir(fuse_req_t req, fuse_ino_t parent,
		     const char *name, mode_t mode)
{
	int err;
	struct stat st;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, parent);

	err = voluta_fs_mkdir(env_of(fusei), parent, name, mode | S_IFDIR, &st);
	reply_entry_or_err(fusei, &st, err);
}

static void do_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	int err;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, parent);

	err = voluta_fs_rmdir(env_of(fusei), parent, name);
	reply_status(fusei, err);
}

static void do_access(fuse_req_t req, fuse_ino_t ino, int mask)
{
	int err;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);

	err = voluta_fs_access(env_of(fusei), ino, mask);
	reply_status(fusei, err);
}

static void do_getattr(fuse_req_t req, fuse_ino_t ino,
		       struct fuse_file_info *fi)
{
	int err;
	struct stat st;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);
	require_fi(fusei, fi);

	err = voluta_fs_getattr(env_of(fusei), ino, &st);
	reply_attr_or_err(fusei, &st, err);
}

static int
uid_gid_of(const struct stat *attr, int to_set, uid_t *uid, gid_t *gid)
{
	*uid = (to_set & FUSE_SET_ATTR_UID) ? attr->st_uid : (uid_t)(-1);
	*gid = (to_set & FUSE_SET_ATTR_GID) ? attr->st_gid : (gid_t)(-1);
	return 0; /* TODO: Check valid ranges */
}

#define FUSE_SET_ATTR_AMTIME_NOW \
	(FUSE_SET_ATTR_ATIME_NOW | FUSE_SET_ATTR_MTIME_NOW)

#define FUSE_SET_ATTR_AMCTIME \
	(FUSE_SET_ATTR_ATIME | FUSE_SET_ATTR_MTIME | FUSE_SET_ATTR_CTIME)

#define FUSE_SET_ATTR_NONTIME \
	(FUSE_SET_ATTR_MODE | FUSE_SET_ATTR_UID | \
	 FUSE_SET_ATTR_GID | FUSE_SET_ATTR_SIZE)

static void utimens_of(const struct stat *st, int to_set, struct stat *times)
{
	bool ctime_now = !(to_set & FUSE_SET_ATTR_AMTIME_NOW);

	memset(times, 0, sizeof(*times));
	times->st_atim.tv_nsec = UTIME_OMIT;
	times->st_mtim.tv_nsec = UTIME_OMIT;
	times->st_ctim.tv_nsec = ctime_now ? UTIME_NOW : UTIME_OMIT;

	if (to_set & FUSE_SET_ATTR_ATIME) {
		ts_copy(&times->st_atim, &st->st_atim);
	}
	if (to_set & FUSE_SET_ATTR_MTIME) {
		ts_copy(&times->st_mtim, &st->st_mtim);
	}
	if (to_set & FUSE_SET_ATTR_CTIME) {
		ts_copy(&times->st_ctim, &st->st_ctim);
	}
}

static void do_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr,
		       int to_set, struct fuse_file_info *fi)
{
	int err;
	uid_t uid;
	gid_t gid;
	struct stat st, times;
	struct voluta_fusei *fusei;
	struct voluta_env *env;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);
	require_fi(fusei, fi);

	utimens_of(attr, to_set, &times);

	env = env_of(fusei);
	err = uid_gid_of(attr, to_set, &uid, &gid);
	require_ok(fusei, err);

	if (to_set & FUSE_SET_ATTR_AMTIME_NOW) {
		err = voluta_fs_utimens(env, ino, &times, &st);
		require_ok(fusei, err);
	}
	if (to_set & FUSE_SET_ATTR_MODE) {
		err = voluta_fs_chmod(env, ino, attr->st_mode, &times, &st);
		require_ok(fusei, err);
	}
	if (to_set & (FUSE_SET_ATTR_UID | FUSE_SET_ATTR_GID)) {
		err = voluta_fs_chown(env, ino, uid, gid, &times, &st);
		require_ok(fusei, err);
	}
	if (to_set & FUSE_SET_ATTR_SIZE) {
		err = voluta_fs_truncate(env, ino, attr->st_size, &st);
		require_ok(fusei, err);
	}
	if ((to_set & FUSE_SET_ATTR_AMCTIME) &&
	    !(to_set & FUSE_SET_ATTR_NONTIME)) {
		err = voluta_fs_utimens(env, ino, &times, &st);
		require_ok(fusei, err);
	}
	reply_attr_or_err(fusei, &st, err);
}

static void do_symlink(fuse_req_t req, const char *target,
		       fuse_ino_t parent, const char *name)
{
	int err;
	struct stat st;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, parent);

	err = voluta_fs_symlink(env_of(fusei), parent, name, target, &st);
	reply_entry_or_err(fusei, &st, err);
}

static void do_readlink(fuse_req_t req, fuse_ino_t ino)
{
	int err;
	size_t len;
	char *buf;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);

	buf = fusei->u->pb.path;
	len = sizeof(fusei->u->pb.path);
	err = voluta_fs_readlink(env_of(fusei), ino, buf, len);
	reply_readlink_or_err(fusei, buf, err);
}

static void do_mknod(fuse_req_t req, fuse_ino_t parent,
		     const char *name, mode_t mode, dev_t rdev)
{
	int err;
	struct stat st;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, parent);

	err = voluta_fs_mknod(env_of(fusei), parent, name, mode, rdev, &st);
	reply_entry_or_err(fusei, &st, err);
}


static void do_unlink(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	int err;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, parent);

	err = voluta_fs_unlink(env_of(fusei), parent, name);
	reply_status(fusei, err);
}

static void do_link(fuse_req_t req, fuse_ino_t ino,
		    fuse_ino_t newparent, const char *newname)
{
	int err;
	struct stat st;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);
	require_ino(fusei, newparent);

	err = voluta_fs_link(env_of(fusei), ino, newparent, newname, &st);
	reply_entry_or_err(fusei, &st, err);
}

static void do_rename(fuse_req_t req, fuse_ino_t parent, const char *name,
		      fuse_ino_t newparent, const char *newname,
		      unsigned int flags)
{
	int err;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, parent);
	require_ino(fusei, newparent);

	err = voluta_fs_rename(env_of(fusei), parent,
			       name, newparent, newname, (int)flags);
	reply_status(fusei, err);
}

static void do_opendir(fuse_req_t req, fuse_ino_t dir_ino,
		       struct fuse_file_info *fi)
{
	int err;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, dir_ino);
	require_fi(fusei, fi);

	err = voluta_fs_opendir(env_of(fusei), dir_ino);
	reply_opendir_or_err(fusei, err);
}

static void do_releasedir(fuse_req_t req, fuse_ino_t ino,
			  struct fuse_file_info *fi)
{
	int err;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);
	require_fi(fusei, fi);

	err = voluta_fs_releasedir(env_of(fusei), ino);
	reply_status(fusei, err);
}

static void do_fsyncdir(fuse_req_t req, fuse_ino_t ino, int datasync,
			struct fuse_file_info *fi)
{
	int err;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);
	require_fi(fusei, fi);

	err = voluta_fs_fsyncdir(env_of(fusei), ino, datasync != 0);
	reply_status(fusei, err);
}

static void do_readdir(fuse_req_t req, fuse_ino_t dir_ino, size_t size,
		       off_t off, struct fuse_file_info *fi)
{
	int err;
	struct voluta_fusei *fusei;
	struct voluta_fusei_diter *diter;

	req_to_fusei(req, &fusei);
	require_ino(fusei, dir_ino);
	require_fi(fusei, fi);

	diter = diter_prep(fusei, size, off);
	err = voluta_fs_readdir(env_of(fusei), dir_ino, &diter->readdir_ctx);
	reply_readdir_or_err(fusei, diter, err);
	diter_done(diter);
}

static void do_create(fuse_req_t req, fuse_ino_t parent, const char *name,
		      mode_t mode, struct fuse_file_info *fi)
{
	int err;
	struct stat st;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, parent);
	require_fi(fusei, fi);

	err = voluta_fs_create(env_of(fusei), parent, name, mode, &st);
	reply_create_or_err(fusei, &st, err);
}

static void do_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	int err;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);

	err = voluta_fs_open(env_of(fusei), ino, fi->flags);
	reply_open_or_err(fusei, err);
}

static void do_release(fuse_req_t req, fuse_ino_t ino,
		       struct fuse_file_info *fi)
{
	int err;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);
	require_fi(fusei, fi);

	err = voluta_fs_release(env_of(fusei), ino);
	reply_status(fusei, err);
}

static void do_flush(fuse_req_t req, fuse_ino_t ino,
		     struct fuse_file_info *fi)
{
	int err;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);
	require_fi(fusei, fi);

	err = voluta_fs_flush(env_of(fusei), ino);
	reply_status(fusei, err);
}

static void do_fsync(fuse_req_t req, fuse_ino_t ino, int datasync,
		     struct fuse_file_info *fi)
{
	int err;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);
	require_fi(fusei, fi);

	err = voluta_fs_fsync(env_of(fusei), ino, datasync != 0);
	reply_status(fusei, err);
}

static void do_fallocate(fuse_req_t req, fuse_ino_t ino, int mode, off_t off,
			 off_t len, struct fuse_file_info *fi)
{
	int err;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);
	require_fi(fusei, fi);

	err = voluta_fs_fallocate(env_of(fusei), ino, mode, off, len);
	reply_status(fusei, err);
}

static struct voluta_fusei_rw_iter *
fusei_rw_iter_of(const struct voluta_rw_iter *rwi)
{
	return container_of(rwi, struct voluta_fusei_rw_iter, rwi);
}

static int fusei_read_iter_actor(struct voluta_rw_iter *rd_iter,
				 const struct voluta_qmref *qmr)
{
	struct voluta_fusei_rw_iter *rwi = fusei_rw_iter_of(rd_iter);

	if ((qmr->fd > 0) && (qmr->off < 0)) {
		return -EINVAL;
	}
	if (rwi->with_splice) {
		if (rwi->bv.bv.count >= ARRAY_SIZE(rwi->bv.buf)) {
			return -EINVAL;
		}
		add_to_bufvec(&rwi->bv.bv, qmr);
	}
	if (rwi->dat != NULL) {
		if ((rwi->dat_len + qmr->len) > rwi->dat_max) {
			return -EINVAL;
		}
		memcpy(rwi->dat + rwi->dat_len, qmr->mem, qmr->len);
	}
	rwi->dat_len += qmr->len;
	return 0;
}

static struct voluta_fusei_rw_iter *
setup_read_iter(struct voluta_fusei *fusei, size_t len, loff_t off)
{
	struct voluta_fusei_rw_iter *rwi = &fusei->u->rw.rwi;

	rwi->rwi.actor = fusei_read_iter_actor;
	rwi->bvec_in = NULL;
	rwi->rwi.len = len;
	rwi->rwi.off = off;
	rwi->dat_len = 0;
	rwi->dat = NULL;
	rwi->dat_max = 0;
	rwi->bv.bv.count = 0;
	rwi->bv.bv.idx = 0;
	rwi->bv.bv.off = 0;
	rwi->with_splice = cap_splice(fusei);
	if (!rwi->with_splice) {
		rwi->dat = fusei->u->rw.db.data;
		rwi->dat_max = min_size(len, sizeof(fusei->u->rw.db.data));
	}
	return rwi;
}

static void do_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
		    struct fuse_file_info *fi)
{
	int err;
	struct voluta_fusei *fusei;
	struct voluta_fusei_rw_iter *frwi;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);
	require_fi(fusei, fi);

	frwi = setup_read_iter(fusei, size, off);
	err = voluta_fs_read_iter(env_of(fusei), ino, &frwi->rwi);
	reply_read_or_err(fusei, err);
}

static int fusei_write_iter_actor(struct voluta_rw_iter *wr_iter,
				  const struct voluta_qmref *qmr)
{
	ssize_t ret;
	struct fuse_bufvec bv;
	struct voluta_fusei_rw_iter *rwi = fusei_rw_iter_of(wr_iter);

	if ((qmr->fd > 0) && (qmr->off < 0)) {
		return -EINVAL;
	}
	if ((rwi->dat_len + qmr->len) > rwi->dat_max) {
		return -EINVAL;
	}
	if (rwi->dat != NULL) {
		memcpy(qmr->mem, rwi->dat + rwi->dat_len, qmr->len);
	} else if (rwi->with_splice && rwi->bvec_in) {
		fill_bufvec(&bv, qmr, 1);
		ret = fuse_buf_copy(&bv, rwi->bvec_in, 0);

		/* XXX crap when running dd with big buffers */
		voluta_assert_ge(ret, 0);
	}
	rwi->dat_len += qmr->len;

	return 0;
}

static struct voluta_fusei_rw_iter *
setup_write_iter(struct voluta_fusei *fusei, struct fuse_bufvec *bvec_in,
		 const void *buf, size_t len, loff_t off)
{
	struct voluta_fusei_rw_iter *rwi = &fusei->u->rw.rwi;

	rwi->rwi.actor = fusei_write_iter_actor;
	rwi->bvec_in = bvec_in;
	rwi->rwi.len = len;
	rwi->rwi.off = off;
	rwi->dat_len = 0;
	rwi->dat_max = len;
	rwi->dat = (uint8_t *)buf;
	rwi->with_splice = cap_splice(fusei);
	return rwi;
}

static void do_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
		     size_t size, off_t off, struct fuse_file_info *fi)
{
	int err;
	struct voluta_fusei *fusei;
	struct voluta_fusei_rw_iter *rwi;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);
	require_fi(fusei, fi);

	rwi = setup_write_iter(fusei, NULL, buf, size, off);
	err = voluta_fs_write_iter(env_of(fusei), ino, &rwi->rwi);
	reply_write_or_err(fusei, rwi->dat_len, err);
}

static void do_write_buf(fuse_req_t req, fuse_ino_t ino,
			 struct fuse_bufvec *bufv, off_t off,
			 struct fuse_file_info *fi)
{
	int err;
	size_t size;
	struct voluta_fusei *fusei;
	struct voluta_fusei_rw_iter *rwi;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);
	require_fi(fusei, fi);

	size = fuse_buf_size(bufv);
	rwi = setup_write_iter(fusei, bufv, NULL, size, off);
	err = voluta_fs_write_iter(env_of(fusei), ino, &rwi->rwi);
	reply_write_or_err(fusei, rwi->dat_len, err);
}

static void do_setxattr(fuse_req_t req, fuse_ino_t ino, const char *name,
			const char *val, size_t vsz, int flags)
{
	int err;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);

	err = voluta_fs_setxattr(env_of(fusei), ino, name, val, vsz, flags);
	reply_status(fusei, err);
}

static void do_getxattr(fuse_req_t req, fuse_ino_t ino,
			const char *name, size_t size)
{
	int err;
	void *buf;
	size_t len, cnt = 0;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);

	buf = fusei->u->xi.buf;
	len = min_size(size, sizeof(fusei->u->xi.buf));
	err = voluta_fs_getxattr(env_of(fusei), ino, name, buf, len, &cnt);
	reply_xattr_or_err(fusei, size, buf, cnt, err);
}

static void do_listxattr(fuse_req_t req, fuse_ino_t ino, size_t size)
{
	int err;
	struct voluta_fusei *fusei;
	struct voluta_fusei_xiter *xiter;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);

	xiter = xiter_prep(fusei, size);
	err = voluta_fs_listxattr(env_of(fusei), ino, &xiter->listxattr_ctx);
	reply_xattr_or_err(fusei, size, xiter->beg, xiter->count, err);
	xiter_done(fusei);
}

static void do_removexattr(fuse_req_t req, fuse_ino_t ino, const char *name)
{
	int err;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);

	err = voluta_fs_removexattr(env_of(fusei), ino, name);
	reply_status(fusei, err);
}

static void do_bmap(fuse_req_t req, fuse_ino_t ino, size_t blksz, uint64_t idx)
{
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);

	reply_nonsys(fusei);
	voluta_unused(blksz);
	voluta_unused(idx);
}

/* XXX crap */
#if FUSE_VERSION >= 32
typedef unsigned int voluta_ioctl_cmd_t;
#else
typedef int voluta_ioctl_cmd_t;
#endif

static void do_ioc_getflags(fuse_req_t req, fuse_ino_t ino,
			    struct fuse_file_info *fi, size_t out_bufsz)
{
	int err = -EINVAL;
	long ret = 0;
	struct statx stx;
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);
	require_fi(fusei, fi);

	if (out_bufsz == sizeof(ret)) {
		err = voluta_fs_statx(env_of(fusei), ino, &stx);
		ret = (long)stx.stx_attributes;
	}
	reply_ioctl_or_err(fusei, 0, &ret, sizeof(ret), err);
}

static void do_ioc_notimpl(fuse_req_t req, ino_t ino, struct fuse_file_info *fi)
{
	struct voluta_fusei *fusei;

	req_to_fusei(req, &fusei);
	require_ino(fusei, ino);
	require_fi(fusei, fi);

	reply_nonsys(fusei);
}

static void do_ioctl(fuse_req_t req, fuse_ino_t ino, voluta_ioctl_cmd_t cmd,
		     void *arg, struct fuse_file_info *fi, unsigned int flags,
		     const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
	switch ((long)cmd) {
	case FS_IOC_GETFLAGS:
		do_ioc_getflags(req, ino, fi, out_bufsz);
		break;
	default:
		do_ioc_notimpl(req, ino, fi);
		break;
	}
	voluta_unused(arg);
	voluta_unused(flags);
	voluta_unused(in_buf);
	voluta_unused(in_bufsz);
}


static const struct fuse_lowlevel_ops voluta_fusei_ll_ops = {
	.init = do_init,
	.destroy = do_destroy,
	.statfs = do_statfs,
	.lookup = do_lookup,
	.forget = do_forget,
	.access = do_access,
	.getattr = do_getattr,
	.setattr = do_setattr,
	.mkdir = do_mkdir,
	.rmdir = do_rmdir,
	.readlink = do_readlink,
	.symlink = do_symlink,
	.mknod = do_mknod,
	.unlink = do_unlink,
	.link = do_link,
	.rename = do_rename,
	.opendir = do_opendir,
	.readdir = do_readdir,
	.releasedir = do_releasedir,
	.fsyncdir = do_fsyncdir,
	.create = do_create,
	.open = do_open,
	.release = do_release,
	.flush = do_flush,
	.fsync = do_fsync,
	.fallocate = do_fallocate,
	.read = do_read,
	.write = do_write,
	.write_buf = do_write_buf,
	.setxattr = do_setxattr,
	.getxattr = do_getxattr,
	.listxattr = do_listxattr,
	.removexattr = do_removexattr,
	.bmap = do_bmap,
	.ioctl = do_ioctl
};

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int fusei_alloc_u(struct voluta_fusei *fusei)
{
	int err;
	void *ptr = NULL;

	err = voluta_zalloc(fusei->qmal, sizeof(*fusei->u), &ptr);
	if (!err) {
		fusei->u = ptr;
	} else {
		fusei->u = NULL;
	}
	return err;
}

static void fusei_free_u(struct voluta_fusei *fusei)
{
	voluta_zfree(fusei->qmal, fusei->u, sizeof(*fusei->u));
	fusei->u = NULL;
}

int voluta_fusei_init(struct voluta_fusei *fusei, struct voluta_env *env)
{
	fusei->env = env;
	fusei->qmal = &env->qmal;
	fusei->ucred = &env->ucred;
	fusei->page_size = voluta_sc_page_size();
	fusei->conn_caps = 0;
	fusei->err = 0;
	fusei->session_loop_active = false;
	/* TODO: is it? re-check yourself */
	fusei->blkdev_mode = (env->bdev.flags & VOLUTA_F_BLKDEV);

	return fusei_alloc_u(fusei);
}

void voluta_fusei_fini(struct voluta_fusei *fusei)
{
	fusei_free_u(fusei);
	fusei->qmal = NULL;
	fusei->env = NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void free_fuse_args(struct fuse_args *args)
{
	if (args != NULL) {
		fuse_opt_free_args(args);
		free(args);
	}
}

static int resolve_fuse_args(const char *mount_options,
			     struct fuse_args **out_args)
{
	int err;
	struct fuse_args *args;

	args = (struct fuse_args *)calloc(1, sizeof(*args));
	if (!args) {
		return -1;
	}
	err = fuse_opt_add_arg(args, "");
	if (err) {
		goto out;
	}
	err = fuse_opt_add_arg(args, mount_options);
	if (err) {
		goto out;
	}
out:
	if (err) {
		free_fuse_args(args);
		args = NULL;
	}
	*out_args = args;
	return err;
}

static int setup_mount_params(struct voluta_fusei *fusei, const char *mntpoint)
{
	size_t len = 512;
	char *mntpt, *mntopts;
	const char *fsname = "voluta";
	const char *blkdev = fusei->blkdev_mode ? "blkdev," : "";

	mntpt = strdup(mntpoint);
	if (mntpt == NULL) {
		return -ENOMEM;
	}
	mntopts = (char *)calloc(len, 1);
	if (mntopts == NULL) {
		free(mntpt);
		return -ENOMEM;
	}
	snprintf(mntopts, len - 1, "-osubtype=%s,fsname=%s,"\
		 "%sdefault_permissions,allow_root,max_read=%u",
		 fsname, fsname, blkdev, rdwr_max_size(fusei));
	/* + "noatime " */
	/* + "nodev,nosuid,rw " */
	/* + "debug,atomic_o_trunc,writeback_cache" */

	fusei->mount_point = mntpt;
	fusei->mount_options = mntopts;
	return 0;
}

static void reset_mount_params(struct voluta_fusei *fusei)
{
	free(fusei->mount_point);
	free(fusei->mount_options);
	fusei->mount_point = NULL;
	fusei->mount_options = NULL;
}

int voluta_fusei_mount(struct voluta_fusei *fusei, const char *mountpoint)
{
	int err;
	struct fuse_session *se = NULL;
	struct fuse_args *args = NULL;
	const struct fuse_lowlevel_ops *ops = &voluta_fusei_ll_ops;

	err = setup_mount_params(fusei, mountpoint);
	if (err) {
		return err;
	}
	err = resolve_fuse_args(fusei->mount_options, &args);
	if (err) {
		return err;
	}
	se = fuse_session_new(args, ops, sizeof(*ops), fusei);
	if (se == NULL) {
		err = -EINVAL;
		goto out;
	}
	err = fuse_session_mount(se, mountpoint);
	if (err) {
		goto out;
	}
	fusei->session = se;
	fusei->magic = VOLUTA_MAGIC;

out:
	if (err && se) {
		fuse_session_destroy(se);
		reset_mount_params(fusei);
	}
	free_fuse_args(args);
	return err;
}

int voluta_fusei_umount(struct voluta_fusei *fusei)
{
	struct fuse_session *se = fusei->session;

	if (!se) {
		return -EINVAL;
	}
	fuse_session_unmount(se);
	fuse_session_destroy(se);
	reset_mount_params(fusei);
	fusei->session = NULL;
	return 0;
}

int voluta_fusei_session_loop(struct voluta_fusei *fusei)
{
	int err;
	struct fuse_session *se = fusei->session;

	fusei->session_loop_active = true;
	err = fuse_session_loop(se);
	fusei->session_loop_active = false;
	voluta_burnstack();
	return err;
}

void voluta_fusei_session_break(struct voluta_fusei *fusei)
{
	struct fuse_session *se = fusei->session;

	if (fusei->session_loop_active) {
		fuse_session_exit(se);
	}
}

