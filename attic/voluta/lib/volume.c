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

int voluta_vdi_init(struct voluta_vd_info *vdi,
		    struct voluta_qmalloc *qmal,
		    const struct voluta_crypto *crypto)
{
	int err;
	size_t msz;
	void *mem = NULL;

	vdi->fd = -1;
	vdi->flags = 0;
	vdi->size = 0;
	vdi->crypto = crypto;
	vdi->qmal = qmal;
	vdi->enc_bk = NULL;

	msz = sizeof(*vdi->enc_bk);
	err = voluta_malloc(qmal, msz, &mem);
	if (err) {
		return err;
	}
	vdi->enc_bk = mem;
	return 0;
}

void voluta_vdi_fini(struct voluta_vd_info *vdi)
{
	struct voluta_qmalloc *qmal = vdi->qmal;

	voluta_free(qmal, vdi->enc_bk, sizeof(*vdi->enc_bk));
	vdi->fd = -1;
	vdi->flags = 0;
	vdi->size = 0;
	vdi->crypto = NULL;
	vdi->qmal = NULL;
	vdi->enc_bk = NULL;
}

static void vdi_bind_to_fd(struct voluta_vd_info *vdi, int fd, loff_t size)
{
	vdi->fd = fd;
	vdi->size = size;
}

static int ok_or_close_fd(int err, int fd)
{
	if (err && (fd > 0)) {
		voluta_sys_close(fd);
	}
	return err;
}

static int vdi_create_mem(struct voluta_vd_info *vdi, loff_t size)
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
	vdi_bind_to_fd(vdi, fd, size);
out:
	return ok_or_close_fd(err, fd);
}

static int vdi_create_reg(struct voluta_vd_info *vdi,
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
	vdi_bind_to_fd(vdi, fd, size);
out:
	return ok_or_close_fd(err, fd);
}

static int vdi_create_blk(struct voluta_vd_info *vdi,
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
	vdi_bind_to_fd(vdi, fd, size);
	vdi->flags |= VOLUTA_F_BLKDEV;
out:
	return ok_or_close_fd(err, fd);
}

static int vdi_create_vol(struct voluta_vd_info *vdi,
			  const char *path, loff_t size)
{
	int err;
	struct stat st;

	err = voluta_sys_stat(path, &st);
	if ((err == -ENOENT) || S_ISREG(st.st_mode)) {
		err = vdi_create_reg(vdi, path, size);
	} else if (!err && S_ISBLK(st.st_mode)) {
		err = vdi_create_blk(vdi, path, size);
	} else if (!err && S_ISDIR(st.st_mode)) {
		err = -EISDIR;
	} else if (!err) {
		err = -EINVAL;
	}
	return err;
}

int voluta_vdi_create(struct voluta_vd_info *vdi,
		      const char *path, loff_t size)
{
	int err;

	if (path) {
		err = vdi_create_vol(vdi, path, size);
	} else {
		err = vdi_create_mem(vdi, size);
	}
	return err;
}

int voluta_vdi_open(struct voluta_vd_info *vdi, const char *path)
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
	vdi_bind_to_fd(vdi, fd, size);
	if (S_ISBLK(mode)) {
		vdi->flags |= VOLUTA_F_BLKDEV;
	}
out:
	return ok_or_close_fd(err, fd);
}

void voluta_vdi_close(struct voluta_vd_info *vdi)
{
	if (vdi->fd > 0) {
		voluta_sys_fsync(vdi->fd);
		voluta_sys_close(vdi->fd);
		vdi_bind_to_fd(vdi, -1, 0);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int vdi_check_io(const struct voluta_vd_info *vdi,
			loff_t off, size_t len)
{
	const loff_t off_end = off + (loff_t)len;

	if (vdi->fd < 0) {
		return -EIO;
	}
	if (off >= vdi->size) {
		return -EIO;
	}
	if ((off_end < 0) || (off_end > vdi->size)) {
		return -EINVAL;
	}
	return 0;
}

int voluta_vdi_read(const struct voluta_vd_info *vdi,
		    loff_t off, size_t bsz, void *buf)
{
	int err;
	size_t nrd = 0;

	err = vdi_check_io(vdi, off, bsz);
	if (err) {
		return err;
	}
	err = voluta_sys_pread(vdi->fd, buf, bsz, off, &nrd);
	if (err) {
		return err;
	}
	if (nrd != bsz) {
		return -EIO;
	}
	return 0;
}

static int vdi_pread_bk(const struct voluta_vd_info *vdi,
			union voluta_bk *bk, loff_t off)
{
	return voluta_vdi_read(vdi, off, sizeof(*bk), bk);
}

static int vdi_decrypt_bk(const struct voluta_vd_info *vdi,
			  const struct voluta_iv_key *iv_key,
			  union voluta_bk *enc_bk, union voluta_bk *out_bk)
{
	return voluta_decrypt_bk(vdi->crypto, &iv_key->iv, &iv_key->key,
				 enc_bk, out_bk);
}

int voluta_vdi_load_dec(struct voluta_vd_info *vdi, loff_t off,
			const struct voluta_iv_key *iv_key,
			union voluta_bk *bk)
{
	int err;

	err = vdi_pread_bk(vdi, vdi->enc_bk, off);
	if (err) {
		return err;
	}
	err = vdi_decrypt_bk(vdi, iv_key, vdi->enc_bk, bk);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_vdi_write(const struct voluta_vd_info *vdi,
		     loff_t off, size_t bsz, const void *buf)
{
	int err;
	size_t nwr = 0;

	err = vdi_check_io(vdi, off, bsz);
	if (err) {
		return err;
	}
	err = voluta_sys_pwrite(vdi->fd, buf, bsz, off, &nwr);
	if (err) {
		return err;
	}
	if (nwr != bsz) {
		return -EIO;
	}
	return 0;
}

static int vdi_pwrite_nbk(const struct voluta_vd_info *vdi, loff_t off,
			  const union voluta_bk *bks, size_t nbks)
{
	return voluta_vdi_write(vdi, off, nbks * sizeof(*bks), bks);
}

static int vdi_pwrite_bk(const struct voluta_vd_info *vdi,
			 loff_t off, const union voluta_bk *bk)
{
	return vdi_pwrite_nbk(vdi, off, bk, 1);
}

static int vdi_encrypt_bk(const struct voluta_vd_info *vdi,
			  const struct voluta_iv_key *iv_key,
			  const union voluta_bk *bk, union voluta_bk *out_bk)
{
	return voluta_encrypt_bk(vdi->crypto,
				 &iv_key->iv, &iv_key->key, bk, out_bk);
}

int voluta_vdi_enc_save(struct voluta_vd_info *vdi, loff_t off,
			const struct voluta_iv_key *iv_key,
			const union voluta_bk *bk)
{
	int err;

	err = vdi_encrypt_bk(vdi, iv_key, bk, vdi->enc_bk);
	if (err) {
		return err;
	}
	err = vdi_pwrite_bk(vdi, off, vdi->enc_bk);
	if (err) {
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int vdi_fsync(struct voluta_vd_info *vdi)
{
	return voluta_sys_fsync(vdi->fd);
}

static int vdi_fdatasync(struct voluta_vd_info *vdi)
{
	return voluta_sys_fdatasync(vdi->fd);
}

int voluta_vdi_sync(struct voluta_vd_info *vdi, bool all)
{
	return all ? vdi_fsync(vdi) : vdi_fdatasync(vdi);
}


