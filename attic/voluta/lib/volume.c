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
	loff_t nbks;
	const loff_t bk_size = VOLUTA_BK_SIZE;
	const loff_t nbks_in = size / bk_size;
	const loff_t nbk_in_ag = VOLUTA_NBK_IN_AG;

	nbks = ((nbks_in / nbk_in_ag) * nbk_in_ag);
	return nbks * bk_size;
}

static int vol_size_check(loff_t size)
{
	const loff_t size_min = VOLUTA_VOLUME_SIZE_MIN;
	const loff_t size_max = VOLUTA_VOLUME_SIZE_MAX;

	return ((size < size_min) || (size > size_max)) ? -EINVAL : 0;
}

static int vol_size_resolve(loff_t size_cur, loff_t size_want, loff_t *out_vsz)
{
	int err;

	if (size_want == 0) {
		size_want = size_cur;
	}
	err = vol_size_check(size_want);
	if (err) {
		return err;
	}
	*out_vsz = vol_size_fixup(size_want);
	return 0;
}

int voluta_resolve_volume_size(const char *path,
			       loff_t size_want, loff_t *out_size)
{
	int err;
	loff_t size_cur = 0;

	err = resolve_by_path(path, &size_cur);
	if (!err) {
		err = vol_size_resolve(size_cur, size_want, out_size);
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

int voluta_pstore_init(struct voluta_pstore *pstore)
{
	pstore->fd = -1;
	pstore->flags = 0;
	pstore->size = 0;
	return 0;
}

void voluta_pstore_fini(struct voluta_pstore *pstore)
{
	voluta_pstore_close(pstore);
	pstore->fd = -1;
	pstore->flags = 0;
	pstore->size = 0;
}

static void pstore_bind_to_fd(struct voluta_pstore *pstore,
			      int fd, loff_t size)
{
	pstore->fd = fd;
	pstore->size = size;
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
	pstore->flags |= VOLUTA_F_BLKDEV;
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
		pstore->flags |= VOLUTA_F_BLKDEV;
	}
out:
	return ok_or_close_fd(err, fd);
}

static bool pstore_has_open_fd(const struct voluta_pstore *pstore)
{
	return (pstore->fd > 0);
}

void voluta_pstore_close(struct voluta_pstore *pstore)
{
	if (pstore_has_open_fd(pstore)) {
		voluta_sys_fsync(pstore->fd);
		voluta_sys_close(pstore->fd);
		pstore_bind_to_fd(pstore, -1, 0);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int pstore_check_io(const struct voluta_pstore *pstore,
			   loff_t off, size_t len)
{
	const loff_t off_end = off + (loff_t)len;

	if (pstore->fd < 0) {
		return -EIO;
	}
	if (off >= pstore->size) {
		return -EIO;
	}
	if ((off_end < 0) || (off_end > pstore->size)) {
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
	err = voluta_sys_pread(pstore->fd, buf, bsz, off, &nrd);
	if (err) {
		return err;
	}
	if (nrd != bsz) {
		return -EIO;
	}
	return 0;
}

static int pstore_pread_bk(const struct voluta_pstore *pstore,
			   union voluta_bk *bk, loff_t off)
{
	return voluta_pstore_read(pstore, off, sizeof(*bk), bk);
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
	err = voluta_sys_pwrite(pstore->fd, buf, bsz, off, &nwr);
	if (err) {
		return err;
	}
	if (nwr != bsz) {
		return -EIO;
	}
	return 0;
}

static int pstore_pwrite_nkbs(const struct voluta_pstore *pstore, loff_t off,
			      const union voluta_kb *kbs, size_t nkbs)
{
	return voluta_pstore_write(pstore, off, nkbs * sizeof(*kbs), kbs);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_pstore_sync(struct voluta_pstore *pstore, bool all)
{
	int err;

	if (all) {
		err = voluta_sys_fsync(pstore->fd);
	} else {
		err = voluta_sys_fdatasync(pstore->fd);
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
		err = flock_volume(pstore->fd);
	}
	return err;
}

int voluta_pstore_funlock(const struct voluta_pstore *pstore)
{
	int err = 0;

	if (pstore_has_open_fd(pstore)) {
		err = funlock_volume(pstore->fd);
	}
	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/


static int cstore_init_aux_bk(struct voluta_cstore *cstore)
{
	int err;
	void *mem = NULL;

	err = voluta_malloc(cstore->qal, sizeof(*cstore->aux_bk), &mem);
	if (err) {
		return err;
	}
	cstore->aux_bk = mem;
	return 0;
}

static void cstore_fini_aux_bk(struct voluta_cstore *cstore)
{
	union voluta_bk *aux_bk = cstore->aux_bk;

	if (aux_bk != NULL) {
		voluta_free(cstore->qal, aux_bk, sizeof(*aux_bk));
		cstore->aux_bk = NULL;
	}
}

static int cstore_init_crypto(struct voluta_cstore *cstore)
{
	return voluta_crypto_init(&cstore->crypto);
}

static void cstore_fini_crypto(struct voluta_cstore *cstore)
{
	voluta_crypto_fini(&cstore->crypto);
}

static int cstore_init_pstore(struct voluta_cstore *cstore)
{
	return voluta_pstore_init(&cstore->pstore);
}

static void cstore_fini_pstore(struct voluta_cstore *cstore)
{
	voluta_pstore_fini(&cstore->pstore);
}

int voluta_cstore_init(struct voluta_cstore *cstore,
		       struct voluta_qalloc *qalloc)
{
	int err;

	cstore->qal = qalloc;
	cstore->aux_bk = NULL;
	cstore->pstore.fd = -1;

	err = cstore_init_aux_bk(cstore);
	if (err) {
		return err;
	}
	err = cstore_init_crypto(cstore);
	if (err) {
		cstore_fini_aux_bk(cstore);
		return err;
	}
	err = cstore_init_pstore(cstore);
	if (err) {
		cstore_fini_crypto(cstore);
		cstore_fini_aux_bk(cstore);
		return err;
	}
	return 0;
}

void voluta_cstore_fini(struct voluta_cstore *cstore)
{
	cstore_fini_pstore(cstore);
	cstore_fini_crypto(cstore);
	cstore_fini_aux_bk(cstore);
	cstore->qal = NULL;
}

int voluta_cstore_stage_bk(struct voluta_cstore *cstore,
			   loff_t off, union voluta_bk *bk)
{
	return pstore_pread_bk(&cstore->pstore, bk, off);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t view_nkbs_of(const struct voluta_vnode_info *vi)
{
	return voluta_nkbs_of(v_vaddr_of(vi));
}

static long view_kb_index(const struct voluta_vnode_info *vi)
{
	const union voluta_kb *kb_view = &vi->view->kb;
	const union voluta_kb *kb_start = vi->v_bki->bk->kb;

	voluta_assert_ge(kb_view, kb_start);
	voluta_assert_le(kb_view - kb_start, VOLUTA_NKB_IN_BK);

	return (kb_view - kb_start);
}

static union voluta_kb *view_kbs_of(const struct voluta_vnode_info *vi)
{
	return &vi->v_bki->bk->kb[view_kb_index(vi)];
}

static union voluta_kb *aux_kbs_of(const struct voluta_vnode_info *vi)
{
	const struct voluta_cstore *cstore = vi->v_sbi->s_cstore;

	return &cstore->aux_bk->kb[view_kb_index(vi)];
}

static int cstore_encrypt_vnode(const struct voluta_cstore *cstore,
				const struct voluta_vnode_info *vi,
				const struct voluta_iv_key *iv_key)
{
	const size_t nkbs = view_nkbs_of(vi);
	const union voluta_kb *kbs = view_kbs_of(vi);
	union voluta_kb *enc = aux_kbs_of(vi);

	return voluta_encrypt_nkbs(&cstore->crypto, iv_key, kbs, enc, nkbs);
}

static int cstore_write_enc_kbs_of(const struct voluta_cstore *cstore,
				   const struct voluta_vnode_info *vi)
{
	const size_t nkbs = view_nkbs_of(vi);
	const struct voluta_vaddr *vaddr = v_vaddr_of(vi);
	union voluta_kb *enc_kbs = aux_kbs_of(vi);

	return pstore_pwrite_nkbs(&cstore->pstore, vaddr->off, enc_kbs, nkbs);
}

int voluta_encrypt_destage(const struct voluta_vnode_info *vi)
{
	int err;
	struct voluta_iv_key iv_key;
	const struct voluta_sb_info *sbi = v_sbi_of(vi);

	voluta_assert(voluta_is_visible(vi));

	voluta_iv_key_of(vi, &iv_key);
	err = cstore_encrypt_vnode(sbi->s_cstore, vi, &iv_key);
	if (err) {
		return err;
	}
	err = cstore_write_enc_kbs_of(sbi->s_cstore, vi);
	if (err) {
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int cstore_decrypt_vnode(const struct voluta_cstore *cstore,
				const struct voluta_vnode_info *vi,
				const struct voluta_iv_key *iv_key)
{
	int err;
	const size_t nkbs = view_nkbs_of(vi);
	union voluta_kb *kbs = view_kbs_of(vi);
	const struct voluta_vaddr *vaddr = v_vaddr_of(vi);

	/* Decrypt in-place */
	err = voluta_decrypt_kbs(&cstore->crypto, iv_key, kbs, kbs, nkbs);
	if (err) {
		log_error("decypt error: vtype=%d off=0x%lx err=%d",
			  vaddr->vtype, vaddr->off, err);

		/* Narrow to file-system error type */
		err = vaddr_isdata(vaddr) ? -EIO : -EFSCORRUPTED;
	}
	return err;
}

static bool need_restage_view(const struct voluta_vnode_info *vi)
{
	return !voluta_is_visible(vi);
}

int voluta_decrypt_reload(const struct voluta_vnode_info *vi)
{
	int err;
	struct voluta_iv_key iv_key;
	const struct voluta_sb_info *sbi = vi->v_sbi;

	if (!need_restage_view(vi)) {
		return 0;
	}
	voluta_iv_key_of(vi, &iv_key);
	err = cstore_decrypt_vnode(sbi->s_cstore, vi, &iv_key);
	if (err) {
		return err;
	}
	voluta_mark_visible(vi);
	return 0;
}
