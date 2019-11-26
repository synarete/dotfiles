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
	int err, fd;
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

int voluta_vol_resolve_size(const char *path,
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

int voluta_vol_check_path(const char *path)
{
	int err;
	loff_t size;
	struct stat st;

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
	err = voluta_vol_resolve_size(path, st.st_size, &size);
	if (err) {
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static loff_t pos_of(const struct voluta_bk_info *bki)
{
	return bk_vaddr_of(bki)->off;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_bdev_init(struct voluta_bdev *bdev, struct voluta_qmalloc *qmal,
		     const struct voluta_crypto *crypto)
{
	int err;
	size_t msz;
	void *mem = NULL;

	bdev->fd = -1;
	bdev->flags = 0;
	bdev->size = 0;
	bdev->crypto = crypto;
	bdev->qmal = qmal;
	bdev->enc_bk = NULL;

	msz = sizeof(*bdev->enc_bk);
	err = voluta_malloc(qmal, msz, &mem);
	if (err) {
		return err;
	}
	bdev->enc_bk = mem;
	return 0;
}

void voluta_bdev_fini(struct voluta_bdev *bdev)
{
	struct voluta_qmalloc *qmal = bdev->qmal;

	voluta_free(qmal, bdev->enc_bk, sizeof(*bdev->enc_bk));
	bdev->fd = -1;
	bdev->flags = 0;
	bdev->size = 0;
	bdev->crypto = NULL;
	bdev->qmal = NULL;
	bdev->enc_bk = NULL;
}

static void bdev_bind_to_fd(struct voluta_bdev *bdev, int fd, loff_t size)
{
	bdev->fd = fd;
	bdev->size = size;
}

static int ok_or_close_fd(int err, int fd)
{
	if (err && (fd > 0)) {
		voluta_sys_close(fd);
	}
	return err;
}

static int bdev_create_mem(struct voluta_bdev *bdev, loff_t size)
{
	int err, fd = -1;

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
	bdev_bind_to_fd(bdev, fd, size);
out:
	return ok_or_close_fd(err, fd);
}

static int bdev_create_reg(struct voluta_bdev *bdev,
			   const char *path, loff_t size)
{
	int err, fd = -1;
	mode_t mode = S_IFREG | S_IRUSR | S_IWUSR;

	err = voluta_sys_open(path, O_CREAT | O_RDWR, mode, &fd);
	if (err) {
		goto out;
	}
	err = voluta_sys_ftruncate(fd, size);
	if (err) {
		goto out;
	}
	bdev_bind_to_fd(bdev, fd, size);
out:
	return ok_or_close_fd(err, fd);
}

static int bdev_create_blk(struct voluta_bdev *bdev,
			   const char *path, loff_t size)
{
	int err, fd = -1;
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
	bdev_bind_to_fd(bdev, fd, size);
	bdev->flags |= VOLUTA_F_BLKDEV;
out:
	return ok_or_close_fd(err, fd);
}

static int bdev_create_vol(struct voluta_bdev *bdev,
			   const char *path, loff_t size)
{
	int err;
	struct stat st;

	err = voluta_sys_stat(path, &st);
	if ((err == -ENOENT) || S_ISREG(st.st_mode)) {
		err = bdev_create_reg(bdev, path, size);
	} else if (!err && S_ISBLK(st.st_mode)) {
		err = bdev_create_blk(bdev, path, size);
	} else if (!err && S_ISDIR(st.st_mode)) {
		err = -EISDIR;
	} else if (!err) {
		err = -EINVAL;
	}
	return err;
}

int voluta_bdev_create(struct voluta_bdev *bdev, const char *path,
		       loff_t size)
{
	int err;

	if (path) {
		err = bdev_create_vol(bdev, path, size);
	} else {
		err = bdev_create_mem(bdev, size);
	}
	return err;
}

int voluta_bdev_open(struct voluta_bdev *bdev, const char *path)
{
	int err, fd = -1;
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
	bdev_bind_to_fd(bdev, fd, size);
	if (S_ISBLK(mode)) {
		bdev->flags |= VOLUTA_F_BLKDEV;
	}
out:
	return ok_or_close_fd(err, fd);
}

void voluta_bdev_close(struct voluta_bdev *bdev)
{
	if (bdev->fd > 0) {
		voluta_sys_fsync(bdev->fd);
		voluta_sys_close(bdev->fd);
		bdev_bind_to_fd(bdev, -1, 0);
	}
}

int voluta_bdev_fsync(struct voluta_bdev *bdev)
{
	return voluta_sys_fsync(bdev->fd);
}

int voluta_bdev_fdatasync(struct voluta_bdev *bdev)
{
	return voluta_sys_fdatasync(bdev->fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool off_isinrange(loff_t off, loff_t off_min, loff_t off_max)
{
	return (off_min <= off) && (off <= off_max);
}

static int bdev_check_io(const struct voluta_bdev *bdev, loff_t off)
{
	const loff_t off_max = VOLUTA_OFF_MAX;

	voluta_assert_le(off, off_max);
	voluta_assert_ge(off, 0);

	if (off_isnull(off) || !off_isinrange(off, 0, off_max)) {
		return -EINVAL;
	}
	if (bdev->fd < 0) {
		return -EIO;
	}
	if (off >= bdev->size) {
		return -EIO;
	}
	return 0;
}

static int bdev_pread(const struct voluta_bdev *bdev,
		      loff_t off, size_t bsz, void *buf)
{
	int err;
	size_t nrd = 0;

	err = voluta_sys_pread(bdev->fd, buf, bsz, off, &nrd);
	if (err) {
		return err;
	}
	if (nrd != bsz) {
		return -EIO;
	}
	return 0;
}

static int bdev_pread_bk(const struct voluta_bdev *bdev,
			 union voluta_bk *bk, loff_t off)
{
	return bdev_pread(bdev, off, sizeof(*bk), bk);
}

static int bdev_decrypt_bk(const struct voluta_bdev *bdev,
			   const struct voluta_iv_key *iv_key,
			   union voluta_bk *enc_bk, union voluta_bk *out_bk)
{
	return voluta_decrypt_bk(bdev->crypto, &iv_key->iv, &iv_key->key,
				 enc_bk, out_bk);
}

int voluta_bdev_load_bk(struct voluta_bdev *bdev, loff_t off,
			const struct voluta_iv_key *iv_key,
			union voluta_bk *bk)
{
	int err;

	err = bdev_check_io(bdev, off);
	if (err) {
		return err;
	}
	err = bdev_pread_bk(bdev, bdev->enc_bk, off);
	if (err) {
		return err;
	}
	err = bdev_decrypt_bk(bdev, iv_key, bdev->enc_bk, bk);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_bdev_load(struct voluta_bdev *bdev, struct voluta_bk_info *bki)
{
	struct voluta_iv_key iv_key;

	voluta_iv_key_of(bki, &iv_key);
	return voluta_bdev_load_bk(bdev, pos_of(bki), &iv_key, bki->bk);
}

static int bdev_pwrite(const struct voluta_bdev *bdev,
		       loff_t off, size_t bsz, const void *buf)
{
	int err;
	size_t nwr = 0;

	err = voluta_sys_pwrite(bdev->fd, buf, bsz, off, &nwr);
	if (err) {
		return err;
	}
	if (nwr != bsz) {
		return -EIO;
	}
	return 0;
}

static int bdev_pwrite_nbk(const struct voluta_bdev *bdev, loff_t off,
			   const union voluta_bk *bks, size_t nbks)
{
	return bdev_pwrite(bdev, off, nbks * sizeof(*bks), bks);
}

static int bdev_pwrite_bk(const struct voluta_bdev *bdev,
			  loff_t off, const union voluta_bk *bk)
{
	return bdev_pwrite_nbk(bdev, off, bk, 1);
}

static int bdev_encrypt_bk(const struct voluta_bdev *bdev,
			   const struct voluta_iv_key *iv_key,
			   const union voluta_bk *bk, union voluta_bk *out_bk)
{
	return voluta_encrypt_bk(bdev->crypto,
				 &iv_key->iv, &iv_key->key, bk, out_bk);
}

int voluta_bdev_save_bk(struct voluta_bdev *bdev, loff_t off,
			const struct voluta_iv_key *iv_key,
			const union voluta_bk *bk)
{
	int err;

	err = bdev_check_io(bdev, off);
	if (err) {
		return err;
	}
	err = bdev_encrypt_bk(bdev, iv_key, bk, bdev->enc_bk);
	if (err) {
		return err;
	}
	err = bdev_pwrite_bk(bdev, off, bdev->enc_bk);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_bdev_save(struct voluta_bdev *bdev,
		     const struct voluta_bk_info *bki)
{
	struct voluta_iv_key iv_key;

	voluta_iv_key_of(bki, &iv_key);
	return voluta_bdev_save_bk(bdev, pos_of(bki), &iv_key, bki->bk);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_sync_volume(struct voluta_bdev *bdev, bool all)
{
	return all ? voluta_bdev_fsync(bdev) : voluta_bdev_fdatasync(bdev);
}

