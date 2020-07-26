/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * This file is part of libvoluta
 *
 * Copyright (C) 2020 Shachar Sharon
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

#include <linux/fuse.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include "libvoluta.h"

#if FUSE_KERNEL_VERSION != 7
#error "unsupported FUSE_KERNEL_VERSION"
#endif
#if FUSE_KERNEL_MINOR_VERSION < 31
#error "unsupported FUSE_KERNEL_MINOR_VERSION"
#endif


/* Local types */
struct voluta_fuseq_hdr_in {
	struct fuse_in_header   hdr;
};

struct voluta_fuseq_init_in {
	struct fuse_in_header   hdr;
	struct fuse_init_in     arg;
};

struct voluta_fuseq_setattr_in {
	struct fuse_in_header   hdr;
	struct fuse_setattr_in  arg;
};

struct voluta_fuseq_lookup_in {
	struct fuse_in_header   hdr;
	char name[VOLUTA_NAME_MAX + 1];
};

struct voluta_fuseq_forget_in {
	struct fuse_in_header   hdr;
	struct fuse_forget_in   arg;
};

struct voluta_fuseq_batch_forget_in {
	struct fuse_in_header   hdr;
	struct fuse_batch_forget_in arg;
	struct fuse_forget_one  one[256];
};

struct voluta_fuseq_getattr_in {
	struct fuse_in_header   hdr;
	struct fuse_getattr_in  arg;
};

struct voluta_fuseq_symlink_in {
	struct fuse_in_header   hdr;
	char name_target[VOLUTA_NAME_MAX + 1 + VOLUTA_SYMLNK_MAX];
};

struct voluta_fuseq_mknod_in {
	struct fuse_in_header   hdr;
	struct fuse_mknod_in    arg;
	char name[VOLUTA_NAME_MAX + 1];
};

struct voluta_fuseq_mkdir_in {
	struct fuse_in_header   hdr;
	struct fuse_mkdir_in    arg;
	char name[VOLUTA_NAME_MAX + 1];
};

struct voluta_fuseq_unlink_in {
	struct fuse_in_header   hdr;
	char name[VOLUTA_NAME_MAX + 1];
};

struct voluta_fuseq_rmdir_in {
	struct fuse_in_header   hdr;
	char name[VOLUTA_NAME_MAX + 1];
};

struct voluta_fuseq_rename_in {
	struct fuse_in_header   hdr;
	struct fuse_rename_in   arg;
	char name_newname[2 * (VOLUTA_NAME_MAX + 1)];
};

struct voluta_fuseq_link_in {
	struct fuse_in_header   hdr;
	struct fuse_link_in     arg;
	char name[VOLUTA_NAME_MAX + 1];
};

struct voluta_fuseq_open_in {
	struct fuse_in_header   hdr;
	struct fuse_open_in     arg;
};

struct voluta_fuseq_release_in {
	struct fuse_in_header   hdr;
	struct fuse_release_in  arg;
};

struct voluta_fuseq_fsync_in {
	struct fuse_in_header   hdr;
	struct fuse_fsync_in    arg;
};

struct voluta_fuseq_setxattr_in {
	struct fuse_in_header   hdr;
	struct fuse_setxattr_in arg;
	char name_value[VOLUTA_NAME_MAX + 1 + VOLUTA_SYMLNK_MAX];
};

struct voluta_fuseq_getxattr_in {
	struct fuse_in_header   hdr;
	struct fuse_getxattr_in arg;
	char name[VOLUTA_NAME_MAX + 1];
};

struct voluta_fuseq_listxattr_in {
	struct fuse_in_header   hdr;
	struct fuse_getxattr_in arg;
};

struct voluta_fuseq_removexattr_in {
	struct fuse_in_header   hdr;
	char name[VOLUTA_NAME_MAX + 1];
};

struct voluta_fuseq_flush_in {
	struct fuse_in_header   hdr;
	struct fuse_flush_in    arg;
};

struct voluta_fuseq_opendir_in {
	struct fuse_in_header   hdr;
	struct fuse_open_in     arg;
};

struct voluta_fuseq_readdir_in {
	struct fuse_in_header   hdr;
	struct fuse_read_in     arg;
};

struct voluta_fuseq_releasedir_in {
	struct fuse_in_header   hdr;
	struct fuse_release_in  arg;
};

struct voluta_fuseq_fsyncdir_in {
	struct fuse_in_header   hdr;
	struct fuse_fsync_in    arg;
};

struct voluta_fuseq_access_in {
	struct fuse_in_header   hdr;
	struct fuse_access_in   arg;
};

struct voluta_fuseq_create_in {
	struct fuse_in_header   hdr;
	struct fuse_create_in   arg;
	char name[VOLUTA_NAME_MAX + 1];
};

struct voluta_fuseq_fallocate_in {
	struct fuse_in_header   hdr;
	struct fuse_fallocate_in arg;
};

struct voluta_fuseq_rename2_in {
	struct fuse_in_header   hdr;
	struct fuse_rename2_in   arg;
	char name_newname[2 * (VOLUTA_NAME_MAX + 1)];
};

struct voluta_fuseq_lseek_in {
	struct fuse_in_header   hdr;
	struct fuse_lseek_in    arg;
};

struct voluta_fuseq_read_in {
	struct fuse_in_header   hdr;
	struct fuse_read_in     arg;
};

struct voluta_fuseq_write_in {
	struct fuse_in_header   hdr;
	struct fuse_write_in    arg;
	char dat[8];
};

union voluta_fuseq_in {
	struct voluta_fuseq_hdr_in              hdr;
	struct voluta_fuseq_init_in             init;
	struct voluta_fuseq_setattr_in          setattr;
	struct voluta_fuseq_lookup_in           lookup;
	struct voluta_fuseq_forget_in           forget;
	struct voluta_fuseq_batch_forget_in     batch_forget;
	struct voluta_fuseq_getattr_in          getattr;
	struct voluta_fuseq_symlink_in          symlink;
	struct voluta_fuseq_mknod_in            mknod;
	struct voluta_fuseq_mkdir_in            mkdir;
	struct voluta_fuseq_unlink_in           unlink;
	struct voluta_fuseq_rmdir_in            rmdir;
	struct voluta_fuseq_rename_in           rename;
	struct voluta_fuseq_link_in             link;
	struct voluta_fuseq_open_in             open;
	struct voluta_fuseq_release_in          release;
	struct voluta_fuseq_fsync_in            fsync;
	struct voluta_fuseq_setxattr_in         setxattr;
	struct voluta_fuseq_getxattr_in         getxattr;
	struct voluta_fuseq_listxattr_in        listxattr;
	struct voluta_fuseq_removexattr_in      removexattr;
	struct voluta_fuseq_flush_in            flush;
	struct voluta_fuseq_opendir_in          opendir;
	struct voluta_fuseq_readdir_in          readdir;
	struct voluta_fuseq_releasedir_in       releasedir;
	struct voluta_fuseq_fsyncdir_in         fsyncdir;
	struct voluta_fuseq_access_in           access;
	struct voluta_fuseq_create_in           create;
	struct voluta_fuseq_fallocate_in        fallocate;
	struct voluta_fuseq_rename2_in          rename2;
	struct voluta_fuseq_lseek_in            lseek;
	struct voluta_fuseq_read_in             read;
	struct voluta_fuseq_write_in            write;
};



union voluta_fuseq_inb_u {
	union voluta_fuseq_in in;
	uint8_t b[VOLUTA_PAGE_SIZE + VOLUTA_IO_SIZE_MAX];
};

struct voluta_fuseq_inb {
	union voluta_fuseq_inb_u u;
};

union voluta_fuseq_outb_u {
	uint8_t b[VOLUTA_IO_SIZE_MAX];
	struct voluta_fuseq_databuf  dab;
	struct voluta_fuseq_pathbuf  pab;
	struct voluta_fuseq_xattrbuf xab;
	struct voluta_fuseq_xiter    xit;
	struct voluta_fuseq_diter    dit;
};

struct voluta_fuseq_outb {
	union voluta_fuseq_outb_u u;
};


/* Local functions */
static void fill_fuse_entry(struct fuse_entry_out *ent, const struct stat *st);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const void *after_name(const char *name)
{
	return name + strlen(name) + 1;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_fuseq_xiter *xiter_of(struct voluta_listxattr_ctx *p)
{
	return container_of(p, struct voluta_fuseq_xiter, lxa);
}

static size_t xiter_avail(const struct voluta_fuseq_xiter *xi)
{
	return (size_t)(xi->end - xi->cur);
}

static bool xiter_hasroom(const struct voluta_fuseq_xiter *xi, size_t size)
{
	const size_t avail = xiter_avail(xi);

	return (avail >= size);
}

static int fillxent(struct voluta_listxattr_ctx *lsx,
		    const char *name, size_t nlen)
{
	const size_t size = nlen + 1;
	struct voluta_fuseq_xiter *xi = xiter_of(lsx);

	if (xi->cur) {
		if (!xiter_hasroom(xi, size)) {
			return -ERANGE;
		}
		memcpy(xi->cur, name, nlen);
		xi->cur[nlen] = '\0';
		xi->cur += size;
	}
	xi->cnt += size;
	return 0;
}

static void xiter_prep(struct voluta_fuseq_xiter *xi, size_t size)
{
	xi->lxa.actor = fillxent;
	xi->cnt = 0;
	xi->beg = xi->buf;
	xi->end = xi->beg + min(size, sizeof(xi->buf));
	xi->cur = size ? xi->buf : NULL;
}

static void xiter_done(struct voluta_fuseq_xiter *xi)
{
	xi->lxa.actor = NULL;
	xi->cnt = 0;
	xi->beg = NULL;
	xi->end = NULL;
	xi->cur = NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int
emit_direntonly(void *buf, size_t bsz, const char *name, size_t nlen,
		ino_t ino, mode_t dt, loff_t off, size_t *out_sz)
{
	size_t entlen;
	size_t entlen_padded;
	struct fuse_dirent *fde = buf;

	entlen = FUSE_NAME_OFFSET + nlen;
	entlen_padded = FUSE_DIRENT_ALIGN(entlen);
	if (entlen_padded > bsz) {
		return -EINVAL;
	}

	fde->ino = ino;
	fde->off = (uint64_t)off;
	fde->namelen = (uint32_t)nlen;
	fde->type = dt;
	memcpy(fde->name, name, nlen);
	memset(fde->name + nlen, 0, entlen_padded - entlen);

	*out_sz = entlen_padded;
	return 0;
}

static int
emit_direntplus(void *buf, size_t bsz, const char *name, size_t nlen,
		const struct stat *attr, loff_t off, size_t *out_sz)
{
	size_t entlen;
	size_t entlen_padded;
	struct fuse_direntplus *fdp = buf;
	struct fuse_dirent *fde = &fdp->dirent;

	entlen = FUSE_NAME_OFFSET_DIRENTPLUS + nlen;
	entlen_padded = FUSE_DIRENT_ALIGN(entlen);
	if (entlen_padded > bsz) {
		return -EINVAL;
	}

	memset(&fdp->entry_out, 0, sizeof(fdp->entry_out));
	fill_fuse_entry(&fdp->entry_out, attr);

	fde->ino = attr->st_ino;
	fde->off = (uint64_t)off;
	fde->namelen = (uint32_t)nlen;
	fde->type =  IFTODT(attr->st_mode);
	memcpy(fde->name, name, nlen);
	memset(fde->name + nlen, 0, entlen_padded - entlen);

	*out_sz = entlen_padded;
	return 0;
}

static int emit_dirent(struct voluta_fuseq_diter *di, loff_t off)
{
	int err;
	size_t cnt = 0;
	char *buf = di->buf + di->len;
	const size_t rem = di->bsz - di->len;
	const ino_t ino = di->de_ino;
	const size_t nlen = di->de_nlen;
	const char *name = di->de_name;

	voluta_assert_le(di->len, di->bsz);

	if (rem <= di->de_nlen) {
		return -EINVAL;
	}
	err = likely(di->plus) ?
	      emit_direntplus(buf, rem, name, nlen, &di->de_attr, off, &cnt) :
	      emit_direntonly(buf, rem, name, nlen, ino, di->de_dt, off, &cnt);
	if (err) {
		return err;
	}
	voluta_assert_gt(cnt, 0);
	di->ndes++;
	di->len += cnt;
	return 0;
}

static void update_dirent(struct voluta_fuseq_diter *di,
			  const struct voluta_readdir_info *rdi)
{
	const size_t namebuf_sz = sizeof(di->de_name);

	di->de_off = rdi->off;
	di->de_ino = rdi->ino;
	di->de_dt = rdi->dt;
	di->de_nlen = min(rdi->namelen, namebuf_sz - 1);
	memcpy(di->de_name, rdi->name, di->de_nlen);
	memset(di->de_name + di->de_nlen, 0, namebuf_sz - di->de_nlen);
	if (di->plus) {
		memcpy(&di->de_attr, &rdi->attr, sizeof(di->de_attr));
	}
}

static bool has_dirent(const struct voluta_fuseq_diter *di)
{
	return (di->de_ino > 0) && (di->de_nlen > 0);
}

static struct voluta_fuseq_diter *diter_of(struct voluta_readdir_ctx *rd_ctx)
{
	return container_of(rd_ctx, struct voluta_fuseq_diter, rd_ctx);
}

static int filldir(struct voluta_readdir_ctx *rd_ctx,
		   const struct voluta_readdir_info *rdi)
{
	int err = 0;
	struct voluta_fuseq_diter *di;

	di = diter_of(rd_ctx);
	if (has_dirent(di)) {
		err = emit_dirent(di, rdi->off);
	}
	if (!err) {
		update_dirent(di, rdi);
	}
	return err;
}

static void diter_prep(struct voluta_fuseq_diter *di,
		       size_t bsz, loff_t pos, int plus)
{
	di->ndes = 0;
	di->de_off = 0;
	di->de_nlen = 0;
	di->de_ino = 0;
	di->de_dt = 0;
	di->de_name[0] = '\0';
	di->bsz = min(bsz, sizeof(di->buf));
	di->len = 0;
	di->rd_ctx.actor = filldir;
	di->rd_ctx.pos = pos;
	di->plus = plus;
	memset(&di->de_attr, 0, sizeof(di->de_attr));
}

static void diter_done(struct voluta_fuseq_diter *di)
{
	di->ndes = 0;
	di->de_off = 0;
	di->de_nlen = 0;
	di->de_ino = 0;
	di->de_dt = 0;
	di->len = 0;
	di->rd_ctx.actor = NULL;
	di->rd_ctx.pos = 0;
	di->plus = 0;
}
/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void assign_oper_ctx(struct voluta_fuseq_ctx *fqc,
			    const struct fuse_in_header *hdr)
{
	struct voluta_oper *op = &fqc->op;

	op->ucred.uid = hdr->uid;
	op->ucred.gid = hdr->gid;
	op->ucred.pid = (pid_t)hdr->pid;
	op->unique = hdr->unique;
	op->opcode = (int)hdr->opcode;
}

static void timespec_to_fuse_attr(const struct timespec *ts,
				  uint64_t *sec, uint32_t *nsec)
{
	*sec = (uint64_t)ts->tv_sec;
	*nsec = (uint32_t)ts->tv_nsec;
}

static void fuse_attr_to_timespec(uint64_t sec, uint32_t nsec,
				  struct timespec *ts)
{
	ts->tv_sec = (time_t)sec;
	ts->tv_nsec = (long)nsec;
}

static void stat_to_fuse_attr(const struct stat *st, struct fuse_attr *attr)
{
	memset(attr, 0, sizeof(*attr));
	attr->ino = st->st_ino;
	attr->mode = st->st_mode;
	attr->nlink = (uint32_t)st->st_nlink;
	attr->uid = st->st_uid;
	attr->gid = st->st_gid;
	attr->rdev = (uint32_t)st->st_rdev;
	attr->size = (uint64_t)st->st_size;
	attr->blksize = (uint32_t)st->st_blksize;
	attr->blocks = (uint64_t)st->st_blocks;
	timespec_to_fuse_attr(&st->st_atim, &attr->atime, &attr->atimensec);
	timespec_to_fuse_attr(&st->st_mtim, &attr->mtime, &attr->mtimensec);
	timespec_to_fuse_attr(&st->st_ctim, &attr->ctime, &attr->ctimensec);
}

static void
fuse_setattr_to_stat(const struct fuse_setattr_in *attr, struct stat *st)
{
	memset(st, 0, sizeof(*st));
	st->st_mode = attr->mode;
	st->st_uid = attr->uid;
	st->st_gid = attr->gid;
	st->st_size = (loff_t)attr->size;
	fuse_attr_to_timespec(attr->atime, attr->atimensec, &st->st_atim);
	fuse_attr_to_timespec(attr->mtime, attr->mtimensec, &st->st_mtim);
	fuse_attr_to_timespec(attr->ctime, attr->ctimensec, &st->st_ctim);
}

static void
statfs_to_fuse_kstatfs(const struct statvfs *stv, struct fuse_kstatfs *kstfs)
{
	kstfs->bsize = (uint32_t)stv->f_bsize;
	kstfs->frsize = (uint32_t)stv->f_frsize;
	kstfs->blocks = stv->f_blocks;
	kstfs->bfree = stv->f_bfree;
	kstfs->bavail = stv->f_bavail;
	kstfs->files = stv->f_files;
	kstfs->ffree = stv->f_ffree;
	kstfs->namelen = (uint32_t)stv->f_namemax;
}

static void fill_fuse_entry(struct fuse_entry_out *ent, const struct stat *st)
{
	memset(ent, 0, sizeof(*ent));
	ent->nodeid = st->st_ino;
	ent->generation = 0;
	ent->entry_valid = UINT_MAX;
	ent->attr_valid = UINT_MAX;
	stat_to_fuse_attr(st, &ent->attr);
}

static void fill_fuse_attr(struct fuse_attr_out *attr, const struct stat *st)
{
	memset(attr, 0, sizeof(*attr));
	attr->attr_valid = UINT_MAX;
	stat_to_fuse_attr(st, &attr->attr);
}

static void fill_fuse_open(struct fuse_open_out *open)
{
	memset(open, 0, sizeof(*open));
	open->open_flags = FOPEN_KEEP_CACHE | FOPEN_CACHE_DIR;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
check_fh(const struct voluta_fuseq_ctx *fqc, ino_t ino, uint64_t fh)
{
	if (fh != 0) {
		log_warn("op=%s ino=%lu fh=0x%lx", fqc->opdsc->name, ino, fh);
	}
}

static void fuseq_fill_out_header(struct voluta_fuseq_ctx *fqc,
				  struct fuse_out_header *out_hdr,
				  size_t len, int err)
{
	out_hdr->len = (uint32_t)len;
	out_hdr->error = -abs(err);
	out_hdr->unique = fqc->op.unique;
}

static void fuseq_send_msg(struct voluta_fuseq_ctx *fqc,
			   const struct iovec *iov, size_t iovcnt)
{
	int err;
	size_t nwr = 0;
	struct voluta_fuseq *fq = fqc->fq;

	err = voluta_sys_writev(fq->fq_fuse_fd, iov, (int)iovcnt, &nwr);

	/* XXX */
	voluta_assert_ok(err);
	voluta_assert_gt(nwr, 0);

	fq->fq_status = err;
}

static void fuseq_reply_arg(struct voluta_fuseq_ctx *fqc,
			    const void *arg, size_t argsize)
{
	struct iovec iov[2];
	struct fuse_out_header hdr;
	const size_t hdrsize = sizeof(hdr);

	iov[0].iov_base = &hdr;
	iov[0].iov_len = hdrsize;
	iov[1].iov_base = unconst(arg);
	iov[1].iov_len = argsize;

	fuseq_fill_out_header(fqc, &hdr, hdrsize + argsize, 0);
	fuseq_send_msg(fqc, iov, 2);
}

static void fuseq_reply_buf(struct voluta_fuseq_ctx *fqc,
			    const void *buf, size_t bsz)
{
	fuseq_reply_arg(fqc, buf, bsz);
}

static void fuseq_reply_err(struct voluta_fuseq_ctx *fqc, int err)
{
	struct iovec iov[1];
	struct fuse_out_header hdr;
	const size_t hdrsize = sizeof(hdr);

	iov[0].iov_base = &hdr;
	iov[0].iov_len = hdrsize;

	fuseq_fill_out_header(fqc, &hdr, hdrsize, err);
	fuseq_send_msg(fqc, iov, 1);
}

static void fuseq_reply_status(struct voluta_fuseq_ctx *fqc, int status)
{
	fuseq_reply_err(fqc, status);
}

static void fuseq_reply_none(struct voluta_fuseq_ctx *fqc)
{
	fqc->op.unique = 0;
}

static void fuseq_reply_entry_ok(struct voluta_fuseq_ctx *fqc,
				 const struct stat *st)
{
	struct fuse_entry_out arg;

	fill_fuse_entry(&arg, st);
	fuseq_reply_arg(fqc, &arg, sizeof(arg));
}

static void fuseq_reply_create_ok(struct voluta_fuseq_ctx *fqc,
				  const struct stat *st)
{
	struct fuseq_create_out {
		struct fuse_entry_out ent;
		struct fuse_open_out  open;
	} voluta_packed_aligned16 arg;

	fill_fuse_entry(&arg.ent, st);
	fill_fuse_open(&arg.open);
	fuseq_reply_arg(fqc, &arg, sizeof(arg));
}

static void fuseq_reply_attr_ok(struct voluta_fuseq_ctx *fqc,
				const struct stat *st)
{
	struct fuse_attr_out arg;

	fill_fuse_attr(&arg, st);
	fuseq_reply_arg(fqc, &arg, sizeof(arg));
}

static void fuseq_reply_statfs_ok(struct voluta_fuseq_ctx *fqc,
				  const struct statvfs *stv)
{
	struct fuse_statfs_out arg;

	statfs_to_fuse_kstatfs(stv, &arg.st);
	fuseq_reply_arg(fqc, &arg, sizeof(arg));
}

static void fuseq_reply_buf_ok(struct voluta_fuseq_ctx *fqc,
			       const char *buf, size_t bsz)
{
	fuseq_reply_arg(fqc, buf, bsz);
}

static void fuseq_reply_readlink_ok(struct voluta_fuseq_ctx *fqc,
				    const char *lnk)
{
	fuseq_reply_buf_ok(fqc, lnk, strlen(lnk));
}

static void fuseq_reply_open_ok(struct voluta_fuseq_ctx *fqc)
{
	struct fuse_open_out arg;

	fill_fuse_open(&arg);
	fuseq_reply_arg(fqc, &arg, sizeof(arg));
}

static void fuseq_reply_opendir_ok(struct voluta_fuseq_ctx *fqc)
{
	fuseq_reply_open_ok(fqc);
}

static void fuseq_reply_write_ok(struct voluta_fuseq_ctx *fqc, size_t cnt)
{
	struct fuse_write_out arg = {
		.size = (uint32_t)cnt
	};

	fuseq_reply_arg(fqc, &arg, sizeof(arg));
}

static void fuseq_reply_lseek_ok(struct voluta_fuseq_ctx *fqc, loff_t off)
{
	struct fuse_lseek_out arg = {
		.offset = (uint64_t)off
	};

	fuseq_reply_arg(fqc, &arg, sizeof(arg));
}

static void fuseq_reply_xattr_len(struct voluta_fuseq_ctx *fqc, size_t len)
{
	struct fuse_getxattr_out arg = {
		.size = (uint32_t)len
	};

	fuseq_reply_arg(fqc, &arg, sizeof(arg));
}

static void fuseq_reply_xattr_buf(struct voluta_fuseq_ctx *fqc,
				  const void *buf, size_t len)
{
	fuseq_reply_buf(fqc, buf, len);
}

static void fuseq_reply_init_ok(struct voluta_fuseq_ctx *fqc,
				const struct voluta_fuseq_conn *conn)
{
	struct fuse_init_out arg = {
		.major = FUSE_KERNEL_VERSION,
		.minor = FUSE_KERNEL_MINOR_VERSION,
		.flags = 0
	};

	if (conn->cap_kern & FUSE_MAX_PAGES) {
		arg.flags |= FUSE_MAX_PAGES;
		arg.max_pages =
			(uint16_t)((conn->max_write - 1) / conn->pagesize + 1);
	}
	arg.flags |= FUSE_BIG_WRITES;
	arg.flags |= (uint32_t)conn->cap_want;
	arg.max_readahead = (uint32_t)conn->max_readahead;
	arg.max_write = (uint32_t)conn->max_write;
	arg.max_background = (uint16_t)conn->max_background;
	arg.congestion_threshold = (uint16_t)conn->congestion_threshold;
	arg.time_gran = (uint32_t)conn->time_gran;

	fuseq_reply_arg(fqc, &arg, sizeof(arg));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void fuseq_reply_attr(struct voluta_fuseq_ctx *fqc,
			     const struct stat *st, int err)
{
	if (unlikely(err)) {
		fuseq_reply_err(fqc, err);
	} else {
		fuseq_reply_attr_ok(fqc, st);
	}
}

static void fuseq_reply_entry(struct voluta_fuseq_ctx *fqc,
			      const struct stat *st, int err)
{
	if (unlikely(err)) {
		fuseq_reply_err(fqc, err);
	} else {
		fuseq_reply_entry_ok(fqc, st);
	}
}

static void fuseq_reply_create(struct voluta_fuseq_ctx *fqc,
			       const struct stat *st, int err)
{
	if (unlikely(err)) {
		fuseq_reply_err(fqc, err);
	} else {
		fuseq_reply_create_ok(fqc, st);
	}
}

static void fuseq_reply_readlink(struct voluta_fuseq_ctx *fqc,
				 const char *lnk, int err)
{
	if (unlikely(err)) {
		fuseq_reply_err(fqc, err);
	} else {
		fuseq_reply_readlink_ok(fqc, lnk);
	}
}

static void fuseq_reply_statfs(struct voluta_fuseq_ctx *fqc,
			       const struct statvfs *stv, int err)
{
	if (unlikely(err)) {
		fuseq_reply_err(fqc, err);
	} else {
		fuseq_reply_statfs_ok(fqc, stv);
	}
}

static void fuseq_reply_open(struct voluta_fuseq_ctx *fqc, int err)
{
	if (unlikely(err)) {
		fuseq_reply_err(fqc, err);
	} else {
		fuseq_reply_open_ok(fqc);
	}
}

static void fuseq_reply_xattr(struct voluta_fuseq_ctx *fqc,
			      const void *buf, size_t len, int err)
{
	if (unlikely(err)) {
		fuseq_reply_err(fqc, err);
	} else if (buf == NULL) {
		fuseq_reply_xattr_len(fqc, len);
	} else {
		fuseq_reply_xattr_buf(fqc, buf, len);
	}
}

static void fuseq_reply_opendir(struct voluta_fuseq_ctx *fqc, int err)
{
	if (unlikely(err)) {
		fuseq_reply_err(fqc, err);
	} else {
		fuseq_reply_opendir_ok(fqc);
	}
}

static void fuseq_reply_readdir(struct voluta_fuseq_ctx *fqc,
				const struct voluta_fuseq_diter *di, int err)
{
	if (unlikely(err)) {
		fuseq_reply_err(fqc, err);
	} else {
		fuseq_reply_buf(fqc, di->buf, di->len);
	}
}

static void fuseq_reply_read(struct voluta_fuseq_ctx *fqc,
			     const void *dat, size_t len, int err)
{
	if (unlikely(err)) {
		fuseq_reply_err(fqc, err);
	} else {
		fuseq_reply_buf_ok(fqc, dat, len);
	}
}

static void fuseq_reply_write(struct voluta_fuseq_ctx *fqc,
			      size_t cnt, int err)
{
	if (unlikely(err)) {
		fuseq_reply_err(fqc, err);
	} else {
		fuseq_reply_write_ok(fqc, cnt);
	}
}

static void fuseq_reply_lseek(struct voluta_fuseq_ctx *fqc,
			      loff_t off, int err)
{
	if (unlikely(err)) {
		fuseq_reply_err(fqc, err);
	} else {
		fuseq_reply_lseek_ok(fqc, off);
	}
}

static void fuseq_reply_init(struct voluta_fuseq_ctx *fqc, int err)
{
	if (unlikely(err)) {
		fuseq_reply_err(fqc, err);
	} else {
		fuseq_reply_init_ok(fqc, &fqc->fq->fq_conn);
	}
}

/*: : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : :*/

static void setup_cap_want(struct voluta_fuseq_conn *conn, int cap)
{
	if (conn->cap_kern & cap) {
		conn->cap_want |= cap;
	}
}

static int check_init(const struct voluta_fuseq_ctx *fqc,
		      const struct fuse_init_in *arg)
{
	int err = 0;
	const unsigned int u_major = FUSE_KERNEL_VERSION;
	const unsigned int u_minor = FUSE_KERNEL_MINOR_VERSION;

	unused(fqc);
	if ((arg->major != u_major) || (arg->minor != u_minor)) {
		log_warn("version mismatch: kernel=%u.%u userspace=%u.%u",
			 arg->major, arg->minor, u_major, u_minor);
	}
	if ((arg->major != 7) || (arg->minor < 31)) {
		log_err("unsupported fuse-protocol version: %u.%u",
			arg->major, arg->minor);
		err = -EPROTO;
	}
	return err;
}

static void do_init(struct voluta_fuseq_ctx *fqc, ino_t ino,
		    const union voluta_fuseq_in *in)
{
	int err = 0;
	struct voluta_fuseq_conn *conn = &fqc->fq->fq_conn;

	unused(ino);

	err = check_init(fqc, &in->init.arg);
	if (err) {
		fuseq_reply_init(fqc, err);
		return;
	}

	fqc->fq->fq_got_init = 1;
	conn->proto_major = (int)(in->init.arg.major);
	conn->proto_minor = (int)(in->init.arg.minor);
	conn->cap_kern = (int)(in->init.arg.flags);
	conn->cap_want = 0;

	/*
	 * TODO-0018: Enable more capabilities
	 *
	 * XXX: When enabling FUSE_WRITEBACK_CACHE fstests fails with
	 * metadata (st_ctime,st_blocks) issues. Needs further investigation.
	 *
	 * XXX: Also, bugs in 'test_truncate_zero'
	 */
	setup_cap_want(conn, FUSE_ATOMIC_O_TRUNC);
	setup_cap_want(conn, FUSE_EXPORT_SUPPORT);
	/* set_cap_want(conn, FUSE_SPLICE_WRITE); */
	/* set_cap_want(conn, FUSE_SPLICE_READ); */
	/* set_cap_want(conn, FUSE_WRITEBACK_CACHE); */
	setup_cap_want(conn, FUSE_HANDLE_KILLPRIV);
	setup_cap_want(conn, FUSE_CACHE_SYMLINKS);
	setup_cap_want(conn, FUSE_DO_READDIRPLUS);

	fuseq_reply_init(fqc, 0);
}

static void do_destroy(struct voluta_fuseq_ctx *fqc, ino_t ino,
		       const union voluta_fuseq_in *in)
{
	unused(ino);
	unused(in);

	fqc->fq->fq_got_destroy = 1;
	fuseq_reply_status(fqc, 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

#define FATTR_MASK \
	(FATTR_MODE | FATTR_UID | FATTR_GID | FATTR_SIZE | \
	 FATTR_ATIME | FATTR_MTIME | FATTR_FH | FATTR_ATIME_NOW | \
	 FATTR_MTIME_NOW | FATTR_LOCKOWNER | FATTR_CTIME)

#define FATTR_AMTIME_NOW \
	(FATTR_ATIME_NOW | FATTR_MTIME_NOW)

#define FATTR_AMCTIME \
	(FATTR_ATIME | FATTR_MTIME | FATTR_CTIME)

#define FATTR_NONTIME \
	(FATTR_MODE | FATTR_UID | FATTR_GID | FATTR_SIZE)


static int
uid_gid_of(const struct stat *attr, int to_set, uid_t *uid, gid_t *gid)
{
	*uid = (to_set & FATTR_UID) ? attr->st_uid : (uid_t)(-1);
	*gid = (to_set & FATTR_GID) ? attr->st_gid : (gid_t)(-1);
	return 0; /* TODO: Check valid ranges */
}

static void utimens_of(const struct stat *st, int to_set, struct stat *times)
{
	bool ctime_now = !(to_set & FATTR_AMTIME_NOW);

	voluta_memzero(times, sizeof(*times));
	times->st_atim.tv_nsec = UTIME_OMIT;
	times->st_mtim.tv_nsec = UTIME_OMIT;
	times->st_ctim.tv_nsec = ctime_now ? UTIME_NOW : UTIME_OMIT;

	if (to_set & FATTR_ATIME) {
		ts_copy(&times->st_atim, &st->st_atim);
	}
	if (to_set & FATTR_MTIME) {
		ts_copy(&times->st_mtim, &st->st_mtim);
	}
	if (to_set & FATTR_CTIME) {
		ts_copy(&times->st_ctim, &st->st_ctim);
	}
}

static void do_setattr(struct voluta_fuseq_ctx *fqc, ino_t ino,
		       const union voluta_fuseq_in *in)
{
	int err;
	int to_set;
	uid_t uid;
	gid_t gid;
	mode_t mode;
	loff_t size;
	struct stat st;
	struct stat times;
	struct stat attr;

	fuse_setattr_to_stat(&in->setattr.arg, &attr);

	to_set = (int)(in->setattr.arg.valid & FATTR_MASK);
	utimens_of(&attr, to_set, &times);

	err = voluta_fs_getattr(fqc->sbi, &fqc->op, ino, &st);
	if (!err && (to_set & (FATTR_UID | FATTR_GID))) {
		err = uid_gid_of(&attr, to_set, &uid, &gid);
	}
	if (!err && (to_set & FATTR_AMTIME_NOW)) {
		err = voluta_fs_utimens(fqc->sbi, &fqc->op, ino, &times, &st);
	}
	if (!err && (to_set & FATTR_MODE)) {
		mode = attr.st_mode;
		err = voluta_fs_chmod(fqc->sbi, &fqc->op,
				      ino, mode, &times, &st);
	}
	if (!err && (to_set & (FATTR_UID | FATTR_GID))) {
		err = voluta_fs_chown(fqc->sbi, &fqc->op,
				      ino, uid, gid, &times, &st);
	}
	if (!err && (to_set & FATTR_SIZE)) {
		size = attr.st_size;
		err = voluta_fs_truncate(fqc->sbi, &fqc->op, ino, size, &st);
	}
	if (!err && (to_set & FATTR_AMCTIME) && !(to_set & FATTR_NONTIME)) {
		err = voluta_fs_utimens(fqc->sbi, &fqc->op, ino, &times, &st);
	}
	fuseq_reply_attr(fqc, &st, err);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void do_lookup(struct voluta_fuseq_ctx *fqc, ino_t ino,
		      const union voluta_fuseq_in *in)
{
	int err;
	const char *name;
	struct stat st = { .st_ino = 0 };

	name = in->lookup.name;
	err = voluta_fs_lookup(fqc->sbi, &fqc->op, ino, name, &st);
	fuseq_reply_entry(fqc, &st, err);
}

static void do_forget(struct voluta_fuseq_ctx *fqc, ino_t ino,
		      const union voluta_fuseq_in *in)
{
	int err;
	unsigned long nlookup;

	nlookup = in->forget.arg.nlookup;
	err = voluta_fs_forget(fqc->sbi, &fqc->op, ino, nlookup);
	fuseq_reply_none(fqc);
	unused(err);
}

static void do_batch_forget(struct voluta_fuseq_ctx *fqc, ino_t unused_ino,
			    const union voluta_fuseq_in *in)
{
	int err;
	ino_t ino;
	size_t nlookup;
	size_t count;

	count = in->batch_forget.arg.count;
	for (size_t i = 0; i < count; ++i) {
		ino = (ino_t)(in->batch_forget.one[i].nodeid);
		nlookup = (ino_t)(in->batch_forget.one[i].nlookup);

		err = voluta_fs_forget(fqc->sbi, &fqc->op, ino, nlookup);
		unused(err);
	}
	fuseq_reply_none(fqc);
	unused(unused_ino);
}

static void do_getattr(struct voluta_fuseq_ctx *fqc, ino_t ino,
		       const union voluta_fuseq_in *in)
{
	int err;
	struct stat st = { .st_ino = 0 };

	check_fh(fqc, ino, in->getattr.arg.fh);
	err = voluta_fs_getattr(fqc->sbi, &fqc->op, ino, &st);
	fuseq_reply_attr(fqc, &st, err);
}

static void do_readlink(struct voluta_fuseq_ctx *fqc, ino_t ino,
			const union voluta_fuseq_in *in)
{
	int err;
	size_t lim;
	struct voluta_fuseq_pathbuf *pab;

	pab = &fqc->outb->u.pab;
	lim = sizeof(pab->path);

	err = voluta_fs_readlink(fqc->sbi, &fqc->op, ino, pab->path, lim);
	fuseq_reply_readlink(fqc, pab->path, err);

	unused(in);
}

static void do_symlink(struct voluta_fuseq_ctx *fqc, ino_t ino,
		       const union voluta_fuseq_in *in)
{
	int err;
	const char *name;
	const char *target;
	struct stat st = { .st_ino = 0 };

	name = in->symlink.name_target;
	target = after_name(name);

	err = voluta_fs_symlink(fqc->sbi, &fqc->op, ino, name, target, &st);
	fuseq_reply_entry(fqc, &st, err);
}

static void do_mknod(struct voluta_fuseq_ctx *fqc, ino_t ino,
		     const union voluta_fuseq_in *in)
{
	int err;
	dev_t rdev;
	mode_t mode;
	const char *name;
	struct stat st = { .st_ino = 0 };

	fqc->op.ucred.umask = in->mknod.arg.umask;
	rdev = in->mknod.arg.rdev;
	mode = in->mknod.arg.mode;
	name = in->mknod.name;

	err = voluta_fs_mknod(fqc->sbi, &fqc->op, ino, name, mode, rdev, &st);
	fuseq_reply_entry(fqc, &st, err);
}

static void do_mkdir(struct voluta_fuseq_ctx *fqc, ino_t ino,
		     const union voluta_fuseq_in *in)
{
	int err;
	mode_t mode;
	const char *name;
	struct stat st = { .st_ino = 0 };

	fqc->op.ucred.umask = in->mkdir.arg.umask;
	mode = (mode_t)(in->mkdir.arg.mode | S_IFDIR);
	name = in->mkdir.name;

	err = voluta_fs_mkdir(fqc->sbi, &fqc->op, ino, name, mode, &st);
	fuseq_reply_entry(fqc, &st, err);
}

static void do_unlink(struct voluta_fuseq_ctx *fqc, ino_t ino,
		      const union voluta_fuseq_in *in)
{
	int err;
	const char *name;

	name = in->unlink.name;
	err = voluta_fs_unlink(fqc->sbi, &fqc->op, ino, name);
	fuseq_reply_status(fqc, err);
}

static void do_rmdir(struct voluta_fuseq_ctx *fqc, ino_t ino,
		     const union voluta_fuseq_in *in)
{
	int err;
	const char *name;

	name = in->rmdir.name;
	err = voluta_fs_rmdir(fqc->sbi, &fqc->op, ino, name);
	fuseq_reply_status(fqc, err);
}

static void do_rename(struct voluta_fuseq_ctx *fqc, ino_t ino,
		      const union voluta_fuseq_in *in)
{
	int err;
	ino_t newparent;
	const char *name;
	const char *newname;

	newparent = (ino_t)(in->rename.arg.newdir);
	name = in->rename.name_newname;
	newname = after_name(name);
	err = voluta_fs_rename(fqc->sbi, &fqc->op, ino, name,
			       newparent, newname, 0);
	fuseq_reply_status(fqc, err);
}

static void do_link(struct voluta_fuseq_ctx *fqc, ino_t ino,
		    const union voluta_fuseq_in *in)
{
	int err;
	ino_t oldino;
	const char *newname;
	struct stat st = { .st_ino = 0 };

	oldino = (ino_t)(in->link.arg.oldnodeid);
	newname = in->link.name;
	err = voluta_fs_link(fqc->sbi, &fqc->op, oldino, ino, newname, &st);
	fuseq_reply_entry(fqc, &st, err);
}

static void do_open(struct voluta_fuseq_ctx *fqc, ino_t ino,
		    const union voluta_fuseq_in *in)
{
	int err;
	int o_flags;

	o_flags = (int)(in->open.arg.flags);
	err = voluta_fs_open(fqc->sbi, &fqc->op, ino, o_flags);
	fuseq_reply_open(fqc, err);
}

static void do_statfs(struct voluta_fuseq_ctx *fqc, ino_t ino,
		      const union voluta_fuseq_in *in)
{
	int err;
	struct statvfs stv = { .f_bsize = 0 };

	err = voluta_fs_statfs(fqc->sbi, &fqc->op, ino, &stv);
	fuseq_reply_statfs(fqc, &stv, err);
	unused(in);
}

static void do_release(struct voluta_fuseq_ctx *fqc, ino_t ino,
		       const union voluta_fuseq_in *in)
{
	int err;
	int o_flags;
	bool flush;

	o_flags = (int)in->release.arg.flags;
	flush = (in->release.arg.flags & FUSE_RELEASE_FLUSH) != 0;
	check_fh(fqc, ino, in->release.arg.fh);

	err = voluta_fs_release(fqc->sbi, &fqc->op, ino, o_flags, flush);
	fuseq_reply_status(fqc, err);
}

static void do_fsync(struct voluta_fuseq_ctx *fqc, ino_t ino,
		     const union voluta_fuseq_in *in)
{
	int err;
	bool datasync;

	datasync = (in->fsync.arg.fsync_flags & 1) != 0;
	check_fh(fqc, ino, in->fsync.arg.fh);

	err = voluta_fs_fsync(fqc->sbi, &fqc->op, ino, datasync);
	fuseq_reply_status(fqc, err);
}

static void do_setxattr(struct voluta_fuseq_ctx *fqc, ino_t ino,
			const union voluta_fuseq_in *in)
{
	int err;
	int xflags;
	size_t value_size;
	const char *name;
	const char *value;

	value_size = in->setxattr.arg.size;
	xflags = (int)(in->setxattr.arg.flags);
	name = in->setxattr.name_value;
	value = after_name(name);

	err = voluta_fs_setxattr(fqc->sbi, &fqc->op, ino,
				 name, value, value_size, xflags);
	fuseq_reply_status(fqc, err);
}

static void do_getxattr(struct voluta_fuseq_ctx *fqc, ino_t ino,
			const union voluta_fuseq_in *in)
{
	int err;
	size_t cnt = 0;
	size_t len;
	void *buf;
	const char *name;
	struct voluta_fuseq_xattrbuf *xab;

	xab = &fqc->outb->u.xab;
	buf = xab->value;
	len = min(in->getxattr.arg.size, sizeof(xab->value));
	name = in->getxattr.name;

	err = voluta_fs_getxattr(fqc->sbi, &fqc->op, ino,
				 name, buf, len, &cnt);
	fuseq_reply_xattr(fqc, buf, cnt, err);
}

static void do_listxattr(struct voluta_fuseq_ctx *fqc, ino_t ino,
			 const union voluta_fuseq_in *in)
{
	int err;
	struct voluta_fuseq_xiter *xit;

	xit = &fqc->outb->u.xit;
	xiter_prep(xit, in->listxattr.arg.size);
	err = voluta_fs_listxattr(fqc->sbi, &fqc->op, ino, &xit->lxa);
	fuseq_reply_xattr(fqc, xit->beg, xit->cnt, err);
	xiter_done(xit);
}

static void do_removexattr(struct voluta_fuseq_ctx *fqc, ino_t ino,
			   const union voluta_fuseq_in *in)
{
	int err;
	const char *name;

	name = in->removexattr.name;
	err = voluta_fs_removexattr(fqc->sbi, &fqc->op, ino, name);
	fuseq_reply_status(fqc, err);
}

static void do_flush(struct voluta_fuseq_ctx *fqc, ino_t ino,
		     const union voluta_fuseq_in *in)
{
	int err;

	check_fh(fqc, ino, in->flush.arg.fh);
	err = voluta_fs_flush(fqc->sbi, &fqc->op, ino);
	fuseq_reply_status(fqc, err);
}

static void do_opendir(struct voluta_fuseq_ctx *fqc, ino_t ino,
		       const union voluta_fuseq_in *in)
{
	int err;
	int o_flags;

	o_flags = (int)(in->opendir.arg.flags);
	unused(o_flags); /* XXX use me */

	err = voluta_fs_opendir(fqc->sbi, &fqc->op, ino);
	fuseq_reply_opendir(fqc, err);
}

static void do_readdir(struct voluta_fuseq_ctx *fqc, ino_t ino,
		       const union voluta_fuseq_in *in)
{
	int err;
	size_t size;
	loff_t off;
	struct voluta_fuseq_diter *dit;

	size = in->readdir.arg.size;
	off = (loff_t)(in->readdir.arg.offset);
	check_fh(fqc, ino, in->readdir.arg.fh);

	dit = &fqc->outb->u.dit;
	diter_prep(dit, size, off, 0);
	err = voluta_fs_readdir(fqc->sbi, &fqc->op, ino, &dit->rd_ctx);
	fuseq_reply_readdir(fqc, dit, err);
	diter_done(dit);
}

static void do_readdirplus(struct voluta_fuseq_ctx *fqc, ino_t ino,
			   const union voluta_fuseq_in *in)
{
	int err;
	size_t size;
	loff_t off;
	struct voluta_fuseq_diter *dit;

	size = in->readdir.arg.size;
	off = (loff_t)(in->readdir.arg.offset);
	check_fh(fqc, ino, in->readdir.arg.fh);

	dit = &fqc->outb->u.dit;
	diter_prep(dit, size, off, 1);
	err = voluta_fs_readdirplus(fqc->sbi, &fqc->op, ino, &dit->rd_ctx);
	fuseq_reply_readdir(fqc, dit, err);
	diter_done(dit);
}

static void do_releasedir(struct voluta_fuseq_ctx *fqc, ino_t ino,
			  const union voluta_fuseq_in *in)
{
	int err;
	int o_flags;

	o_flags = (int)(in->releasedir.arg.flags);
	check_fh(fqc, ino, in->releasedir.arg.fh);

	err = voluta_fs_releasedir(fqc->sbi, &fqc->op, ino, o_flags);
	fuseq_reply_status(fqc, err);
}

static void do_fsyncdir(struct voluta_fuseq_ctx *fqc, ino_t ino,
			const union voluta_fuseq_in *in)
{
	int err;
	bool datasync;

	datasync = (in->fsyncdir.arg.fsync_flags & 1) != 0;
	check_fh(fqc, ino, in->fsyncdir.arg.fh);

	err = voluta_fs_fsyncdir(fqc->sbi, &fqc->op, ino, datasync);
	fuseq_reply_status(fqc, err);
}

static void do_access(struct voluta_fuseq_ctx *fqc, ino_t ino,
		      const union voluta_fuseq_in *in)
{
	int err;
	int mask;

	mask = (int)(in->access.arg.mask);
	err = voluta_fs_access(fqc->sbi, &fqc->op, ino, mask);
	fuseq_reply_status(fqc, err);
}

static void do_create(struct voluta_fuseq_ctx *fqc, ino_t ino,
		      const union voluta_fuseq_in *in)
{
	int err;
	int o_flags;
	mode_t mode;
	mode_t umask;
	const char *name;
	struct stat st = { .st_ino = 0 };

	o_flags = (int)(in->create.arg.flags);
	mode = (mode_t)(in->create.arg.mode);
	umask = (mode_t)(in->create.arg.umask);
	name = in->create.name;

	fqc->op.ucred.umask = umask;
	err = voluta_fs_create(fqc->sbi, &fqc->op, ino,
			       name, o_flags, mode, &st);
	fuseq_reply_create(fqc, &st, err);
}

static void do_fallocate(struct voluta_fuseq_ctx *fqc, ino_t ino,
			 const union voluta_fuseq_in *in)
{
	int err;
	int mode;
	loff_t off;
	loff_t len;

	mode = (int)(in->fallocate.arg.mode);
	off = (loff_t)(in->fallocate.arg.offset);
	len = (loff_t)(in->fallocate.arg.length);
	check_fh(fqc, ino, in->fallocate.arg.fh);

	err = voluta_fs_fallocate(fqc->sbi, &fqc->op, ino, mode, off, len);
	fuseq_reply_status(fqc, err);
}

static void do_rename2(struct voluta_fuseq_ctx *fqc, ino_t ino,
		       const union voluta_fuseq_in *in)
{
	int err;
	int flags;
	ino_t newparent;
	const char *name;
	const char *newname;

	newparent = (ino_t)(in->rename2.arg.newdir);
	name = in->rename2.name_newname;
	newname = after_name(name);
	flags = (int)(in->rename2.arg.flags);

	err = voluta_fs_rename(fqc->sbi, &fqc->op, ino,
			       name, newparent, newname, flags);
	fuseq_reply_status(fqc, err);
}

static void do_lseek(struct voluta_fuseq_ctx *fqc, ino_t ino,
		     const union voluta_fuseq_in *in)
{
	int err;
	int whence;
	loff_t off;
	loff_t soff = -1;

	off = (loff_t)(in->lseek.arg.offset);
	whence = (int)(in->lseek.arg.whence);
	check_fh(fqc, ino, in->lseek.arg.fh);

	err = voluta_fs_lseek(fqc->sbi, &fqc->op, ino, off, whence, &soff);
	fuseq_reply_lseek(fqc, soff, err);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void do_read(struct voluta_fuseq_ctx *fqc, ino_t ino,
		    const union voluta_fuseq_in *in)
{
	int err;
	loff_t off;
	size_t len;
	size_t nrd = 0;
	struct voluta_fuseq_databuf *dab;

	off = (loff_t)(in->read.arg.offset);
	len = min(in->read.arg.size, fqc->fq->fq_conn.max_read);
	dab = &fqc->outb->u.dab;
	check_fh(fqc, ino, in->read.arg.fh);

	err = voluta_fs_read(fqc->sbi, &fqc->op, ino,
			     dab->buf, len, off, &nrd);
	fuseq_reply_read(fqc, dab->buf, nrd, err);
}

static void do_write(struct voluta_fuseq_ctx *fqc, ino_t ino,
		     const union voluta_fuseq_in *in)
{
	int err;
	loff_t off;
	size_t len;
	size_t nwr = 0;
	const void *dat;

	off = (loff_t)(in->write.arg.offset);
	len = min(in->write.arg.size, fqc->fq->fq_conn.max_write);
	dat = in->write.dat;
	check_fh(fqc, ino, in->write.arg.fh);

	err = voluta_fs_write(fqc->sbi, &fqc->op, ino, dat, len, off, &nwr);
	fuseq_reply_write(fqc, nwr, err);
}

/*: : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : :*/

#define FUSEQ_OPDESC(opcode_, hook_, rtime_) \
	[opcode_] = { hook_, VOLUTA_STR(opcode_), opcode_, rtime_ }

static const struct voluta_opdesc fuseq_ops_tbl[] = {
	FUSEQ_OPDESC(FUSE_LOOKUP, do_lookup, 0),
	FUSEQ_OPDESC(FUSE_FORGET, do_forget, 0),
	FUSEQ_OPDESC(FUSE_GETATTR, do_getattr, 0),
	FUSEQ_OPDESC(FUSE_SETATTR, do_setattr, 1),
	FUSEQ_OPDESC(FUSE_READLINK, do_readlink, 0),
	FUSEQ_OPDESC(FUSE_SYMLINK, do_symlink, 1),
	FUSEQ_OPDESC(FUSE_MKNOD, do_mknod, 1),
	FUSEQ_OPDESC(FUSE_MKDIR, do_mkdir, 1),
	FUSEQ_OPDESC(FUSE_UNLINK, do_unlink, 1),
	FUSEQ_OPDESC(FUSE_RMDIR, do_rmdir, 1),
	FUSEQ_OPDESC(FUSE_RENAME, do_rename, 1),
	FUSEQ_OPDESC(FUSE_LINK, do_link, 1),
	FUSEQ_OPDESC(FUSE_OPEN, do_open, 1),
	FUSEQ_OPDESC(FUSE_READ, do_read, 0),
	FUSEQ_OPDESC(FUSE_WRITE, do_write, 1),
	FUSEQ_OPDESC(FUSE_STATFS, do_statfs, 0),
	FUSEQ_OPDESC(FUSE_RELEASE, do_release, 0),
	FUSEQ_OPDESC(FUSE_FSYNC, do_fsync, 0),
	FUSEQ_OPDESC(FUSE_SETXATTR, do_setxattr, 1),
	FUSEQ_OPDESC(FUSE_GETXATTR, do_getxattr, 0),
	FUSEQ_OPDESC(FUSE_LISTXATTR, do_listxattr, 0),
	FUSEQ_OPDESC(FUSE_REMOVEXATTR, do_removexattr, 0),
	FUSEQ_OPDESC(FUSE_FLUSH, do_flush, 0),
	FUSEQ_OPDESC(FUSE_INIT, do_init, 1),
	FUSEQ_OPDESC(FUSE_OPENDIR, do_opendir, 1),
	FUSEQ_OPDESC(FUSE_READDIR, do_readdir, 1),
	FUSEQ_OPDESC(FUSE_RELEASEDIR, do_releasedir, 1),
	FUSEQ_OPDESC(FUSE_FSYNCDIR, do_fsyncdir, 1),
	FUSEQ_OPDESC(FUSE_GETLK, NULL, 0),
	FUSEQ_OPDESC(FUSE_SETLKW, NULL, 0),
	FUSEQ_OPDESC(FUSE_ACCESS, do_access, 0),
	FUSEQ_OPDESC(FUSE_CREATE, do_create, 1),
	FUSEQ_OPDESC(FUSE_INTERRUPT, NULL, 0),
	FUSEQ_OPDESC(FUSE_BMAP, NULL, 0),
	FUSEQ_OPDESC(FUSE_DESTROY, do_destroy, 1),
	FUSEQ_OPDESC(FUSE_IOCTL, NULL, 0),
	FUSEQ_OPDESC(FUSE_POLL, NULL, 0),
	FUSEQ_OPDESC(FUSE_NOTIFY_REPLY, NULL, 0),
	FUSEQ_OPDESC(FUSE_BATCH_FORGET, do_batch_forget, 0),
	FUSEQ_OPDESC(FUSE_FALLOCATE, do_fallocate, 1),
	FUSEQ_OPDESC(FUSE_READDIRPLUS, do_readdirplus, 0),
	FUSEQ_OPDESC(FUSE_RENAME2, do_rename2, 1),
	FUSEQ_OPDESC(FUSE_LSEEK, do_lseek, 0),
	FUSEQ_OPDESC(FUSE_COPY_FILE_RANGE, NULL, 0),
	FUSEQ_OPDESC(FUSE_SETUPMAPPING, NULL, 0),
	FUSEQ_OPDESC(FUSE_REMOVEMAPPING, NULL, 0),
};

static const struct voluta_opdesc *
operation_desc(unsigned int opc)
{
	return (opc <= ARRAY_SIZE(fuseq_ops_tbl)) ? &fuseq_ops_tbl[opc] : NULL;
}

static int fuseq_resolve_oper(struct voluta_fuseq_ctx *fqc, unsigned int opc)
{
	const struct voluta_opdesc *opdsc = operation_desc(opc);

	if ((opdsc == NULL) || (opdsc->hook == NULL)) {
		return -ENOSYS;
	}
	if (!fqc->fq->fq_got_init && (opdsc->code != FUSE_INIT)) {
		return -EIO;
	}
	if (fqc->fq->fq_got_init && (opdsc->code == FUSE_INIT)) {
		return -EIO;
	}
	fqc->opdsc = opdsc;
	return 0;
}

static int fuseq_check_perm(const struct voluta_fuseq_ctx *fqc)
{
	const uid_t owner = fqc->sbi->s_owner.uid;
	const uid_t opuid = fqc->op.ucred.uid;


	if (!fqc->fq->fq_deny_others) {
		return 0;
	}
	if ((opuid == 0) || (owner == opuid)) {
		return 0;
	}
	switch (fqc->op.opcode) {
	case FUSE_INIT:
	case FUSE_READ:
	case FUSE_WRITE:
	case FUSE_FSYNC:
	case FUSE_RELEASE:
	case FUSE_READDIR:
	case FUSE_FSYNCDIR:
	case FUSE_RELEASEDIR:
	case FUSE_NOTIFY_REPLY:
	case FUSE_READDIRPLUS:
		return 0;
	default:
		break;
	}
	return -EACCES;
}

static int fuseq_update_xtime(struct voluta_fuseq_ctx *fqc)
{
	const bool is_realtime = (fqc->opdsc->realtime > 0);

	return voluta_ts_gettime(&fqc->op.xtime, is_realtime);
}

static int fuseq_process_hdr(struct voluta_fuseq_ctx *fqc,
			     const union voluta_fuseq_in *in)
{
	int err;
	const struct fuse_in_header *hdr = &in->hdr.hdr;

	assign_oper_ctx(fqc, hdr);
	err = fuseq_resolve_oper(fqc, hdr->opcode);
	if (err) {
		return err;
	}
	err = fuseq_check_perm(fqc);
	if (err) {
		return err;
	}
	err = fuseq_update_xtime(fqc);
	if (err) {
		return err;
	}
	return 0;
}

static int fuseq_process_op(struct voluta_fuseq_ctx *fqc,
			    const union voluta_fuseq_in *in)
{
	const unsigned long nodeid = in->hdr.hdr.nodeid;

	fqc->opdsc->hook(fqc, (ino_t)nodeid, in);
	return 0;
}

static void fuseq_process_req(struct voluta_fuseq_ctx *fqc,
			      const union voluta_fuseq_in *in)
{
	int err;

	err = fuseq_process_hdr(fqc, in);
	if (!err) {
		err = fuseq_process_op(fqc, in);
	}
	if (err) {
		fuseq_reply_err(fqc, err);
	}
}

static int fuseq_exec_next_req(struct voluta_fuseq *fq)
{
	int err;
	size_t bsz;
	size_t nrd = 0;
	struct voluta_fuseq_ctx *fqc = &fq->fqc;
	union voluta_fuseq_in *in = &fqc->inb->u.in;

	bsz = fq->fq_conn.buffsize;
	err = voluta_sys_read(fq->fq_fuse_fd, in, bsz, &nrd);
	if (err) {
		return err;
	}
	if (nrd < sizeof(in->hdr)) {
		log_err("short fuse read: nrd=%lu", nrd);
		return -EIO;
	}
	fuseq_process_req(fqc, in);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int new_inb(struct voluta_qalloc *qal,
		   struct voluta_fuseq_inb **out_inb)
{
	int err;
	void *mem = NULL;

	err = voluta_zalloc(qal, sizeof(**out_inb), &mem);
	*out_inb = mem;
	return err;
}

static void del_inb(struct voluta_qalloc *qal, struct voluta_fuseq_inb *inb)
{
	voluta_free(qal, inb, sizeof(*inb));
}

static int new_outb(struct voluta_qalloc *qal,
		    struct voluta_fuseq_outb **out_outb)
{
	int err;
	void *mem = NULL;

	err = voluta_zalloc(qal, sizeof(**out_outb), &mem);
	*out_outb = mem;
	return err;
}

static void del_outb(struct voluta_qalloc *qal, struct voluta_fuseq_outb *outb)
{
	voluta_free(qal, outb, sizeof(*outb));
}


static int fuseq_init_conn(struct voluta_fuseq *fq, size_t bufsize_max)
{
	int err;
	long pipe_maxsz;
	size_t page_size;
	size_t pipe_size;
	size_t buff_size;
	size_t rdwr_size;
	const size_t mega = VOLUTA_UMEGA;
	const size_t fuse_min_bsz = FUSE_MIN_READ_BUFFER;
	struct voluta_fuseq_conn *conn = &fq->fq_conn;

	page_size = voluta_sc_page_size();
	err = voluta_proc_pipe_max_size(&pipe_maxsz);
	if (err) {
		return err;
	}
	pipe_size = (pipe_maxsz > (long)mega) ? mega : (size_t)pipe_maxsz;
	if (pipe_size < (2 * page_size)) {
		log_err("bad system config: page_size=%lu pipe_size=%lu",
			page_size, pipe_size);
		return -EINVAL;
	}
	buff_size = min(pipe_size, bufsize_max);
	if (buff_size < (2 * page_size)) {
		log_err("illegal conn params: page_size=%lu buff_size=%lu",
			page_size, buff_size);
		return -EPROTO;
	}
	if (buff_size < fuse_min_bsz) {
		log_err("buffer too small: buff_size=%lu fuse_min_bsz=%lu",
			buff_size, fuse_min_bsz);
		return -EPROTO;
	}
	rdwr_size = buff_size - page_size;

	conn->pagesize = page_size;
	conn->buffsize = buff_size;
	conn->max_write = rdwr_size;
	conn->max_read = rdwr_size;
	conn->max_readahead = rdwr_size;
	conn->max_background = 16; /* XXX crap */
	conn->congestion_threshold = 12;
	conn->time_gran = 1; /* XXX Is it ? */
	return 0;
}

static int fuseq_init_ctx(struct voluta_fuseq *fq)
{
	int err;
	struct voluta_fuseq_ctx *fqc = &fq->fqc;

	fqc->opdsc = NULL;
	fqc->fq  = fq;
	fqc->sbi = fq->fq_sbi;
	fqc->inb = NULL;
	fqc->outb = NULL;

	err = new_inb(fq->fq_qal, &fqc->inb);
	if (err) {
		return err;
	}
	err = new_outb(fq->fq_qal, &fqc->outb);
	if (err) {
		del_inb(fq->fq_qal, fqc->inb);
		return err;
	}
	return 0;
}

static void fuseq_fini_ctx(struct voluta_fuseq *fq)
{
	struct voluta_fuseq_ctx *fqc = &fq->fqc;

	del_outb(fq->fq_qal, fqc->outb);
	del_inb(fq->fq_qal, fqc->inb);
	fqc->opdsc = NULL;
	fqc->fq  = NULL;
	fqc->sbi = NULL;
	fqc->inb = NULL;
	fqc->outb = NULL;
}

int voluta_fuseq_init(struct voluta_fuseq *fq, struct voluta_sb_info *sbi)
{
	int err;

	voluta_memzero(fq, sizeof(*fq));
	fq->fq_sbi = sbi;
	fq->fq_qal = sbi->s_qalloc;
	fq->fq_fuse_fd = -1;
	fq->fq_status = 0;
	fq->fq_got_init = 0;
	fq->fq_got_destroy = 0;
	fq->fq_deny_others = 0;
	fq->fq_active = 0;

	err = fuseq_init_ctx(fq);
	if (err) {
		return err;
	}
	err = fuseq_init_conn(fq, sizeof(*fq->fqc.inb));
	if (err) {
		fuseq_fini_ctx(fq);
		return err;
	}
	voluta_mntclnt_init(&fq->fq_sock);
	return 0;
}

void voluta_fuseq_fini(struct voluta_fuseq *fq)
{
	voluta_mntclnt_fini(&fq->fq_sock);
	fuseq_fini_ctx(fq);
	fq->fq_qal = NULL;
	fq->fq_sbi = NULL;
}

int voluta_fuseq_mount(struct voluta_fuseq *fq, const char *mountpoint)
{
	int err;
	int status = -1;
	int fuse_fd = -1;
	struct voluta_socket *sock;
	struct voluta_mntparams mntp = {
		.path = mountpoint,
		.flags = MS_NOSUID | MS_NODEV,
		.rootmode = S_IFDIR | S_IRWXU,
		.user_id = fq->fq_sbi->s_owner.uid,
		.group_id = fq->fq_sbi->s_owner.gid,
		.max_read = fq->fq_conn.buffsize,
	};

	sock = &fq->fq_sock;
	err = voluta_mntclnt_connect(sock);
	if (err) {
		return err;
	}
	err = voluta_mntclnt_mount(sock, &mntp, &status, &fuse_fd);
	if (err) {
		return err;
	}
	fq->fq_fuse_fd = fuse_fd;
	return 0;
}

int voluta_fuseq_exec(struct voluta_fuseq *fq)
{
	int err = 0;
	int timed_out = 0;

	fq->fq_active = 1;
	while (fq->fq_active) {
		timed_out = 0;
		err = fuseq_exec_next_req(fq);
		if (err == -ENOENT) {
			timed_out = 1;
			err = 0;
		} else if ((err == -EINTR) || (err == -EAGAIN)) {
			err = 0;
		} else if (err == -ENODEV) {
			/* Filesystem unmounted, or connection aborted */
			fq->fq_active = 0;
			err = 0;
		}
		voluta_assert_ok(err);
	}
	fq->fq_active = 0;
	unused(timed_out);

	return err;
}

