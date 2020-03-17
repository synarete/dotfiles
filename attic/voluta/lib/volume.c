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
#include "voluta-lib.h"

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

int voluta_resolve_volume_size(const char *path,
			       loff_t size_want, loff_t *out_size)
{
	int err;
	loff_t size_cur = 0;

	err = resolve_by_path(path, &size_cur);
	if (err) {
		return err;
	}
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

int voluta_require_volume_exclusive(const char *path)
{
	int err;
	struct voluta_pstore pstore;

	err = voluta_pstore_init(&pstore);
	if (err) {
		return err;
	}
	err = voluta_pstore_open(&pstore, path);
	if (err) {
		goto out;
	}
	err = voluta_pstore_flock(&pstore);
	if (err) {
		goto out;
	}
	err = voluta_pstore_funlock(&pstore);
	if (err) {
		goto out;
	}
out:
	voluta_pstore_fini(&pstore);
	return err;
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

static int pstore_pwrite_nbk(const struct voluta_pstore *pstore, loff_t off,
			     const union voluta_bk *bks, size_t nbks)
{
	return voluta_pstore_write(pstore, off, nbks * sizeof(*bks), bks);
}

static int pstore_pwrite_bk(const struct voluta_pstore *pstore,
			    loff_t off, const union voluta_bk *bk)
{
	return pstore_pwrite_nbk(pstore, off, bk, 1);
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

static int new_bk(struct voluta_qalloc *qal, union voluta_bk **out_bk)
{
	int err;
	void *mem = NULL;

	err = voluta_malloc(qal, sizeof(**out_bk), &mem);
	*out_bk = mem;
	return err;
}

static void del_bk(struct voluta_qalloc *qal, union voluta_bk *bk)
{
	voluta_free(qal, bk, sizeof(*bk));
}

int voluta_cstore_init(struct voluta_cstore *cstore,
		       struct voluta_qalloc *qal,
		       const struct voluta_crypto *crypto)
{
	int err;

	err = voluta_pstore_init(&cstore->pstore);
	if (err) {
		return err;
	}
	cstore->crypto = crypto;
	cstore->qal = qal;
	err = new_bk(qal, &cstore->enc_bk);
	if (err) {
		voluta_pstore_fini(&cstore->pstore);
		return err;
	}
	return 0;
}

void voluta_cstore_fini(struct voluta_cstore *cstore)
{
	del_bk(cstore->qal, cstore->enc_bk);
	cstore->crypto = NULL;
	cstore->qal = NULL;
	cstore->enc_bk = NULL;
	voluta_pstore_fini(&cstore->pstore);
}

static int cstore_decrypt_bk(const struct voluta_cstore *cstore,
			     const struct voluta_iv_key *iv_key,
			     union voluta_bk *enc_bk,
			     union voluta_bk *out_bk)
{
	return voluta_decrypt_bk(cstore->crypto,
				 &iv_key->iv, &iv_key->key, enc_bk, out_bk);
}

int voluta_cstore_load_dec(struct voluta_cstore *cstore, loff_t off,
			   const struct voluta_iv_key *iv_key,
			   union voluta_bk *bk)
{
	int err;


	err = pstore_pread_bk(&cstore->pstore, cstore->enc_bk, off);
	if (err) {
		return err;
	}
	err = cstore_decrypt_bk(cstore, iv_key, cstore->enc_bk, bk);
	if (err) {
		return err;
	}
	return 0;
}

static int cstore_encrypt_bk(const struct voluta_cstore *cstore,
			     const struct voluta_iv_key *iv_key,
			     const union voluta_bk *bk,
			     union voluta_bk *out_bk)
{
	return voluta_encrypt_bk(cstore->crypto,
				 &iv_key->iv, &iv_key->key, bk, out_bk);
}

int voluta_cstore_enc_save(struct voluta_cstore *cstore, loff_t off,
			   const struct voluta_iv_key *iv_key,
			   const union voluta_bk *bk)
{
	int err;

	err = cstore_encrypt_bk(cstore, iv_key, bk, cstore->enc_bk);
	if (err) {
		return err;
	}
	err = pstore_pwrite_bk(&cstore->pstore, off, cstore->enc_bk);
	if (err) {
		return err;
	}
	return 0;
}

