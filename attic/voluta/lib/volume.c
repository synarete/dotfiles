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
#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "libvoluta.h"

#define PSTORE_QUEUE_DEPTH      VOLUTA_NKB_IN_SEG

static int resolve_by_fd(int fd, loff_t *out_size, mode_t *out_mode)
{
	int err;
	size_t sz = 0;
	struct stat st;

	err = voluta_sys_fstat(fd, &st);
	if (err) {
		return err;
	}
	*out_mode = st.st_mode;
	if (S_ISREG(*out_mode)) {
		*out_size = st.st_size;
	} else if (S_ISBLK(*out_mode)) {
		err = voluta_sys_ioctl_blkgetsize64(fd, &sz);
		*out_size = (loff_t)sz;
	} else {
		err = -EINVAL;
	}
	return err;
}

static int resolve_by_path(const char *path, loff_t *out_size)
{
	int err;
	int fd;
	mode_t mode;
	struct stat st;

	if (path == NULL) {
		*out_size = 0;
		return 0;
	}
	err = voluta_sys_stat(path, &st);
	if (err == -ENOENT) {
		*out_size = 0;
		return 0;
	}
	if (err) {
		return err;
	}
	if (S_ISREG(st.st_mode)) {
		*out_size = st.st_size;
		return 0;
	}
	err = voluta_sys_open(path, O_RDONLY, S_IFBLK | S_IRUSR, &fd);
	if (err) {
		return err;
	}
	err = resolve_by_fd(fd, out_size, &mode);
	voluta_sys_close(fd);
	return err;
}

static loff_t vol_size_fixup(loff_t size)
{
	const loff_t ag_size = VOLUTA_AG_SIZE;
	const long nags = (size / ag_size);

	return nags * ag_size;
}

static int vol_size_check(loff_t size)
{
	const loff_t size_min = VOLUTA_VOLUME_SIZE_MIN;
	const loff_t size_max = VOLUTA_VOLUME_SIZE_MAX;

	return ((size < size_min) || (size > size_max)) ? -EINVAL : 0;
}

static int vol_calc_size(loff_t size_cur, loff_t size_want, loff_t *out_size)
{
	int err;

	if (size_want == 0) {
		size_want = size_cur;
	}
	err = vol_size_check(size_want);
	if (err) {
		return err;
	}
	*out_size = vol_size_fixup(size_want);
	return 0;
}

int voluta_resolve_volume_size(const char *path,
			       loff_t size_want, loff_t *out_size)
{
	int err;
	loff_t size_cur = 0;

	err = resolve_by_path(path, &size_cur);
	if (!err) {
		err = vol_calc_size(size_cur, size_want, out_size);
	}
	return err;
}

int voluta_require_volume_path(const char *path)
{
	int err;
	loff_t size;
	struct stat st;

	err = voluta_sys_access(path, R_OK | W_OK);
	if (err) {
		return err;
	}
	err = voluta_sys_stat(path, &st);
	if (err) {
		return err;
	}
	if (S_ISDIR(st.st_mode)) {
		return -EISDIR;
	}
	if (!S_ISREG(st.st_mode) && !S_ISBLK(st.st_mode)) {
		return -EINVAL;
	}
	if ((st.st_mode & S_IRUSR) != S_IRUSR) {
		return -EPERM;
	}
	err = voluta_resolve_volume_size(path, st.st_size, &size);
	if (err) {
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int pstore_init_uring(struct voluta_pstore *pstore)
{
	const unsigned int queue_depth = PSTORE_QUEUE_DEPTH;

	pstore->ps_usend = 0;
	pstore->ps_urecv = 0;
	return io_uring_queue_init(queue_depth, &pstore->ps_uring, 0);
}

static void pstore_fini_uring(struct voluta_pstore *pstore)
{
	io_uring_queue_exit(&pstore->ps_uring);
}

int voluta_pstore_init(struct voluta_pstore *pstore)
{
	pstore->ps_fd = -1;
	pstore->ps_flags = 0;
	pstore->ps_size = 0;
	return pstore_init_uring(pstore);
}

void voluta_pstore_fini(struct voluta_pstore *pstore)
{
	voluta_pstore_close(pstore);
	pstore_fini_uring(pstore);
	pstore->ps_fd = -1;
	pstore->ps_flags = 0;
	pstore->ps_size = 0;
}

static void pstore_bind_to_fd(struct voluta_pstore *pstore,
			      int fd, loff_t size)
{
	pstore->ps_fd = fd;
	pstore->ps_size = size;
}

static int ok_or_close_fd(int err, int fd)
{
	if (err && (fd > 0)) {
		voluta_sys_close(fd);
	}
	return err;
}

static int pstore_create_mem(struct voluta_pstore *pstore, loff_t size)
{
	int err;
	int fd = -1;

	err = vol_size_check(size);
	if (err) {
		return err;
	}
	err = voluta_sys_memfd_create("voluta-volume", 0, &fd);
	if (err) {
		goto out;
	}
	err = voluta_sys_ftruncate(fd, size);
	if (err) {
		goto out;
	}
	pstore_bind_to_fd(pstore, fd, size);
out:
	return ok_or_close_fd(err, fd);
}

static int pstore_create_reg(struct voluta_pstore *pstore,
			     const char *path, loff_t size)
{
	int err;
	int fd = -1;
	mode_t mode = S_IFREG | S_IRUSR | S_IWUSR;

	err = voluta_sys_open(path, O_CREAT | O_RDWR, mode, &fd);
	if (err) {
		goto out;
	}
	err = voluta_sys_ftruncate(fd, size);
	if (err) {
		goto out;
	}
	pstore_bind_to_fd(pstore, fd, size);
out:
	return ok_or_close_fd(err, fd);
}

static int pstore_create_blk(struct voluta_pstore *pstore,
			     const char *path, loff_t size)
{
	int err;
	int fd = -1;
	loff_t sz;
	mode_t mode = S_IFBLK | S_IRUSR | S_IWUSR;

	err = voluta_sys_open(path, O_CREAT | O_RDWR, mode, &fd);
	if (err) {
		goto out;
	}
	err = resolve_by_fd(fd, &sz, &mode);
	if (err) {
		goto out;
	}
	err = vol_size_check(sz);
	if (err) {
		return err;
	}
	if (size == 0) {
		size = sz;
	}
	pstore_bind_to_fd(pstore, fd, size);
	pstore->ps_flags |= VOLUTA_F_BLKDEV;
out:
	return ok_or_close_fd(err, fd);
}

static int pstore_create_vol(struct voluta_pstore *pstore,
			     const char *path, loff_t size)
{
	int err;
	struct stat st;

	err = voluta_sys_stat(path, &st);
	if ((err == -ENOENT) || S_ISREG(st.st_mode)) {
		err = pstore_create_reg(pstore, path, size);
	} else if (!err && S_ISBLK(st.st_mode)) {
		err = pstore_create_blk(pstore, path, size);
	} else if (!err && S_ISDIR(st.st_mode)) {
		err = -EISDIR;
	} else if (!err) {
		err = -EINVAL;
	}
	return err;
}

int voluta_pstore_create(struct voluta_pstore *pstore,
			 const char *path, loff_t size)
{
	int err;

	if (path) {
		err = pstore_create_vol(pstore, path, size);
	} else {
		err = pstore_create_mem(pstore, size);
	}
	return err;
}

int voluta_pstore_open(struct voluta_pstore *pstore, const char *path)
{
	int err;
	int fd = -1;
	mode_t mode;
	loff_t size;

	err = voluta_sys_open(path, O_RDWR, 0, &fd);
	if (err) {
		goto out;
	}
	err = resolve_by_fd(fd, &size, &mode);
	if (err) {
		goto out;
	}
	err = vol_size_check(size);
	if (err) {
		goto out;
	}
	pstore_bind_to_fd(pstore, fd, size);
	if (S_ISBLK(mode)) {
		pstore->ps_flags |= VOLUTA_F_BLKDEV;
	}
out:
	return ok_or_close_fd(err, fd);
}

static bool pstore_has_open_fd(const struct voluta_pstore *pstore)
{
	return (pstore->ps_fd > 0);
}

static void pstore_fsync_close(struct voluta_pstore *pstore)
{
	const int fd = pstore->ps_fd;

	voluta_sys_fsync(fd);
	voluta_sys_close(fd);
}

void voluta_pstore_close(struct voluta_pstore *pstore)
{
	if (pstore_has_open_fd(pstore)) {
		pstore_fsync_close(pstore);
		pstore_bind_to_fd(pstore, -1, 0);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int pstore_check_io(const struct voluta_pstore *pstore,
			   loff_t off, size_t len)
{
	const loff_t off_end = off + (loff_t)len;

	if (pstore->ps_fd < 0) {
		return -EIO;
	}
	if (off >= pstore->ps_size) {
		return -EIO;
	}
	if ((off_end < 0) || (off_end > pstore->ps_size)) {
		return -EINVAL;
	}
	return 0;
}

int voluta_pstore_read(const struct voluta_pstore *pstore,
		       loff_t off, size_t bsz, void *buf)
{
	int err;
	size_t nrd = 0;

	err = pstore_check_io(pstore, off, bsz);
	if (err) {
		return err;
	}
	err = voluta_sys_pread(pstore->ps_fd, buf, bsz, off, &nrd);
	if (err) {
		return err;
	}
	if (nrd != bsz) {
		return -EIO;
	}
	return 0;
}

int voluta_pstore_write(const struct voluta_pstore *pstore,
			loff_t off, size_t bsz, const void *buf)
{
	int err;
	size_t nwr = 0;

	err = pstore_check_io(pstore, off, bsz);
	if (err) {
		return err;
	}
	err = voluta_sys_pwrite(pstore->ps_fd, buf, bsz, off, &nwr);
	if (err) {
		return err;
	}
	if (nwr != bsz) {
		return -EIO;
	}
	return 0;
}

static int pstore_writev(const struct voluta_pstore *pstore, loff_t off,
			 size_t len, const struct iovec *iov, size_t cnt)
{
	int err;
	size_t nwr = 0;

	err = voluta_sys_pwritev(pstore->ps_fd, iov, (int)cnt, off, &nwr);
	if (err) {
		return err;
	}
	if (nwr != len) {
		return -EIO;
	}
	return 0;
}

int voluta_pstore_writev(const struct voluta_pstore *pstore, loff_t off,
			 size_t len, const struct iovec *iov, size_t cnt)
{
	int err;

	err = pstore_check_io(pstore, off, len);
	if (err) {
		return err;
	}
	err = pstore_writev(pstore, off, len, iov, cnt);
	if (err) {
		return err;
	}
	return 0;
}

static int pstore_sync(struct voluta_pstore *pstore, bool all)
{
	int err;

	if (all) {
		err = voluta_sys_fsync(pstore->ps_fd);
	} else {
		err = voluta_sys_fdatasync(pstore->ps_fd);
	}
	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int flock_volume(int fd)
{
	struct flock fl;

	voluta_memzero(&fl, sizeof(fl));
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;

	return voluta_sys_fcntl_flock(fd, F_SETLK, &fl);
}

static int funlock_volume(int fd)
{
	struct flock fl;

	voluta_memzero(&fl, sizeof(fl));
	fl.l_type = F_UNLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;

	return voluta_sys_fcntl_flock(fd, F_SETLK, &fl);
}

int voluta_pstore_flock(const struct voluta_pstore *pstore)
{
	int err = -EPERM;

	if (pstore_has_open_fd(pstore)) {
		err = flock_volume(pstore->ps_fd);
	}
	return err;
}

int voluta_pstore_funlock(const struct voluta_pstore *pstore)
{
	int err = 0;

	if (pstore_has_open_fd(pstore)) {
		err = funlock_volume(pstore->ps_fd);
	}
	return err;
}


/*: : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : :*/

struct voluta_seg_iovec {
	loff_t off;
	struct iovec iov;
};

static int pstore_write_iovec(const struct voluta_pstore *pstore,
			      const struct voluta_seg_iovec *iov)
{
	return pstore_writev(pstore, iov->off, iov->iov.iov_len, &iov->iov, 1);
}

static bool pstore_uring_mode(const struct voluta_pstore *pstore)
{
	const int mask = VOLUTA_F_URING;

	return (pstore->ps_flags & mask) == mask;
}

static size_t pstore_num_inflight(const struct voluta_pstore *pstore)
{
	return pstore->ps_usend - pstore->ps_urecv;
}

static bool pstore_has_inflight(const struct voluta_pstore *pstore)
{
	return pstore_num_inflight(pstore) > 0;
}

static bool pstore_has_room(const struct voluta_pstore *pstore)
{
	const size_t inflight = pstore_num_inflight(pstore);

	return (inflight < PSTORE_QUEUE_DEPTH);
}

bool voluta_pstore_exhuased(const struct voluta_pstore *pstore)
{
	return pstore_uring_mode(pstore) && !pstore_has_room(pstore);
}

static int pstore_prep_iovec(struct voluta_pstore *pstore,
			     struct voluta_seg_iovec *iov)
{
	int fd = pstore->ps_fd;
	struct io_uring_sqe *sqe;

	sqe = io_uring_get_sqe(&pstore->ps_uring);
	if (sqe == NULL) {
		return -ENOSR;
	}
	io_uring_prep_writev(sqe, fd, &iov->iov, 1, iov->off);
	io_uring_sqe_set_data(sqe, iov);
	return 0;
}

static int pstore_submit_uring(struct voluta_pstore *pstore)
{
	int nsub;

	nsub = io_uring_submit(&pstore->ps_uring);
	if (nsub != 1) {
		return (nsub < 0) ? nsub : -EIO;
	}
	pstore->ps_usend++;
	return 0;
}

static int pstore_usend_iovec(struct voluta_pstore *pstore,
			      struct voluta_seg_iovec *iov)
{
	int err;

	err = pstore_prep_iovec(pstore, iov);
	if (err) {
		return err;
	}
	err = pstore_submit_uring(pstore);
	if (err) {
		return err;
	}
	return 0;
}

static int pstore_wait_completed(struct voluta_pstore *pstore,
				 struct voluta_seg_iovec **out_iov)
{
	int err;
	struct io_uring_cqe *cqe = NULL;

	err = io_uring_wait_cqe(&pstore->ps_uring, &cqe);
	if (err) {
		return err;
	}
	*out_iov = io_uring_cqe_get_data(cqe);

	/* TODO: handle cases of parital write via retry */
	voluta_assert_eq(cqe->res, (*out_iov)->iov.iov_len);

	io_uring_cqe_seen(&pstore->ps_uring, cqe);
	pstore->ps_urecv++;

	return 0;
}

static int pstore_urecv_iobuf(struct voluta_pstore *pstore,
			      struct voluta_seg_iovec **out_iov)
{
	int err;
	struct voluta_seg_iovec *iov = NULL;

	if (!pstore_has_inflight(pstore)) {
		return -ENOENT;
	}
	err = pstore_wait_completed(pstore, &iov);
	if (err) {
		/* TODO: Cleanup to sg_iob if not NULL */
		return err;
	}
	*out_iov = iov;
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct voluta_seg_vector {
	struct voluta_vnode_info *vi[VOLUTA_NKB_IN_SEG];
	loff_t off;
	size_t len;
	size_t cnt;
};

static loff_t off_to_seg(loff_t off)
{
	return off_align(off, VOLUTA_SEG_SIZE);
}

static loff_t off_to_next_seg(loff_t off)
{
	return off_to_seg(off + VOLUTA_SEG_SIZE);
}

static void *new_buf(struct voluta_qalloc *qal, size_t len)
{
	int err;
	void *buf = NULL;

	err = voluta_malloc(qal, len, &buf);
	return err ? NULL : buf;
}

static void del_buf(struct voluta_qalloc *qal, void *buf, size_t len)
{
	voluta_free(qal, buf, len);
}

static int alloc_iovec_buf(struct voluta_qalloc *qal,
			   struct voluta_seg_iovec *iov, size_t len)
{
	iov->iov.iov_base = new_buf(qal, len);
	if (iov->iov.iov_base == NULL) {
		return -ENOMEM;
	}
	iov->iov.iov_len = len;
	return 0;
}

static void free_iovec_buf(struct voluta_qalloc *qal,
			   struct voluta_seg_iovec *iov)
{
	del_buf(qal, iov->iov.iov_base, iov->iov.iov_len);
	iov->iov.iov_base = NULL;
	iov->iov.iov_len = 0;
}

static struct voluta_seg_iovec *
new_iovec(struct voluta_qalloc *qal, loff_t off, size_t len)
{
	int err;
	void *mem;
	struct voluta_seg_iovec *iov = NULL;

	err = voluta_malloc(qal, sizeof(*iov), &mem);
	if (err) {
		return NULL;
	}
	iov = mem;
	iov->off = off;
	err = alloc_iovec_buf(qal, iov, len);
	if (err) {
		voluta_free(qal, iov, sizeof(*iov));
		return NULL;
	}
	return iov;
}

static void del_iovec(struct voluta_qalloc *qal, struct voluta_seg_iovec *iov)
{
	free_iovec_buf(qal, iov);
	voluta_free(qal, iov, sizeof(*iov));
}

static bool sgv_isappendable(const struct voluta_seg_vector *sgv,
			     const struct voluta_vnode_info *vi)
{
	loff_t off;
	const struct voluta_vaddr *vaddr = v_vaddr_of(vi);

	if (sgv->cnt == 0) {
		return true;
	}
	if (sgv->cnt == ARRAY_SIZE(sgv->vi)) {
		return false;
	}
	off = off_end(sgv->off, sgv->len);
	if (vaddr->off != off) {
		return false;
	}
	off = off_to_next_seg(sgv->off);
	if (vaddr->off >= off) {
		return false;
	}
	return true;
}

static void sgv_append(struct voluta_seg_vector *sgv,
		       struct voluta_vnode_info *vi)
{
	const struct voluta_vaddr *vaddr = v_vaddr_of(vi);

	if (sgv->cnt == 0) {
		sgv->off = vaddr->off;
		sgv->len = 0;
	}
	sgv->len += vaddr->len;
	sgv->vi[sgv->cnt++] = vi;
}

static void sgv_reset(struct voluta_seg_vector *sgv)
{
	memset(sgv, 0, sizeof(*sgv));
}


static int sgv_encrypt_to(const struct voluta_seg_vector *sgv,
			  const struct voluta_seg_iovec *iov)
{
	int err;
	size_t len;
	char *buf;
	const struct voluta_vnode_info *vi;

	buf = iov->iov.iov_base;
	for (size_t i = 0; i < sgv->cnt; ++i) {
		vi = sgv->vi[i];
		len = v_length_of(vi);
		err = voluta_encrypt_intobuf(vi, buf, len);
		if (err) {
			return err;
		}
		buf += len;
	}
	return 0;
}

static int sgv_destage_uring(const struct voluta_sb_info *sbi,
			     const struct voluta_seg_vector *sgv)
{
	int err;
	struct voluta_seg_iovec *iov;
	struct voluta_pstore *pstore = sbi->s_pstore;

	iov = new_iovec(sbi->s_qalloc, sgv->off, sgv->len);
	if (iov == NULL) {
		return -ENOMEM;
	}
	err = sgv_encrypt_to(sgv, iov);
	if (err) {
		del_iovec(sbi->s_qalloc, iov);
		return err;
	}
	err = pstore_usend_iovec(pstore, iov);
	if (err) {
		del_iovec(sbi->s_qalloc, iov);
		return err;
	}
	return 0;
}

static int sgv_destage_writev(const struct voluta_sb_info *sbi,
			      const struct voluta_seg_vector *sgv)
{
	int err;
	struct voluta_seg_iovec iov;
	struct voluta_segment  *aux_seg = sbi->s_cache->c_aux_seg;

	voluta_assert_not_null(aux_seg);
	voluta_assert_le(sgv->len, sizeof(aux_seg->u.seg));

	iov.off = sgv->off;
	iov.iov.iov_len = sgv->len;
	iov.iov.iov_base = aux_seg->u.seg;

	err = sgv_encrypt_to(sgv, &iov);
	if (err) {
		return err;
	}
	err = pstore_write_iovec(sbi->s_pstore, &iov);
	if (err) {
		return err;
	}
	return 0;
}

static int sgv_destage(const struct voluta_sb_info *sbi,
		       const struct voluta_seg_vector *sgv)
{
	int err = 0;

	if (sgv->cnt > 0) {
		if (pstore_uring_mode(sbi->s_pstore)) {
			err = sgv_destage_uring(sbi, sgv);
		} else {
			err = sgv_destage_writev(sbi, sgv);
		}
	}
	return err;
}

static int collect_if_exhuased(const struct voluta_sb_info *sbi)
{
	int err = 0;

	if (!pstore_has_room(sbi->s_pstore)) {
		err = voluta_collect_flushed(sbi, 1);
	}
	return err;
}

int voluta_flush_dirty(const struct voluta_sb_info *sbi,
		       const struct voluta_dirtyset *dset)
{
	int err = 0;
	struct voluta_vnode_info *vi;
	struct voluta_seg_vector sgv;

	sgv_reset(&sgv);
	vi = voluta_dirtyset_first(dset);
	while (vi != NULL) {
		err = collect_if_exhuased(sbi);
		if (err) {
			return err;
		}
		if (!sgv_isappendable(&sgv, vi)) {
			sgv_destage(sbi, &sgv);
			sgv_reset(&sgv);
		}
		sgv_append(&sgv, vi);
		vi = voluta_dirtyset_next(dset, vi);
	}
	return sgv_destage(sbi, &sgv);
}

int voluta_collect_flushed(const struct voluta_sb_info *sbi, size_t count)
{
	int err;
	size_t inflight;
	struct voluta_seg_iovec *iov = NULL;
	struct voluta_pstore *pstore = sbi->s_pstore;

	if (!pstore_uring_mode(pstore)) {
		return 0;
	}
	inflight = pstore_num_inflight(pstore);
	if (!count || (count > inflight)) {
		count = inflight;
	}
	for (size_t i = 0; i < count; ++i) {
		err = pstore_urecv_iobuf(pstore, &iov);
		if (err) {
			return err;
		}
		del_iovec(sbi->s_qalloc, iov);
	}
	return 0;
}

int voluta_sync_volume(const struct voluta_sb_info *sbi)
{
	return pstore_sync(sbi->s_pstore, true);
}
