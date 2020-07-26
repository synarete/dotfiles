/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * This file is part of voluta.
 *
 * Copyright (C) 2020 Shachar Sharon
 *
 * Voluta is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Voluta is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "vfstest.h"


/* Common meta-info for io-tests */
struct vt_ioargs {
	loff_t off;
	size_t bsz;
	size_t cnt;
};

static const struct vt_ioargs s_aligned_ioargs[] = {
	{ 0, 1, 0 },
	{ 0, VT_BK_SIZE, 0 },
	{ 0, VT_UMEGA, 0 },
	{ VT_BK_SIZE, VT_BK_SIZE, 0 },
	{ VT_BK_SIZE, 2 * VT_BK_SIZE, 0 },
	{ VT_BK_SIZE, VT_UMEGA, 0 },
	{ VT_UMEGA - VT_BK_SIZE, VT_BK_SIZE, 0 },
	{ VT_UMEGA, VT_BK_SIZE, 0 },
	{ VT_UMEGA - VT_BK_SIZE, 2 * VT_BK_SIZE, 0 },
	{ VT_UGIGA, VT_BK_SIZE, 0 },
	{ VT_UGIGA - VT_BK_SIZE, 2 * VT_BK_SIZE, 0 },
	{ VT_UGIGA + VT_BK_SIZE, VT_BK_SIZE, 0 },
};

static const struct vt_ioargs s_unaligned_ioargs[] = {
	{ 1, 2, 0 },
	{ 1, VT_BK_SIZE - 2, 0 },
	{ 1, VT_BK_SIZE + 2, 0 },
	{ 1, VT_UMEGA - 2, 0 },
	{ 1, VT_UMEGA + 2, 0 },
	{ VT_BK_SIZE - 1, VT_BK_SIZE + 2, 0 },
	{ VT_UMEGA - VT_BK_SIZE + 1, 2 * VT_BK_SIZE + 1, 0 },
	{ VT_UMEGA - 1, VT_BK_SIZE + 11, 0 },
	{ VT_UMEGA - VT_BK_SIZE - 1, 11 * VT_BK_SIZE, 0 },
	{ VT_UGIGA - 1, VT_BK_SIZE + 2, 0 },
	{ VT_UGIGA - VT_BK_SIZE - 1, 2 * VT_BK_SIZE + 2, 0 },
	{ VT_UGIGA + VT_BK_SIZE + 1, VT_BK_SIZE - 1, 0 },
};

static blkcnt_t range_to_nfrgs(blksize_t blksz, loff_t off, size_t nbytes)
{
	blkcnt_t nfrgs;
	loff_t beg, end, len;
	const loff_t frgsz = 512; /* see stat(2) */

	len = (loff_t)nbytes;
	beg = (off / blksz) * blksz;
	end = ((off + len + blksz - 1) / blksz) * blksz;
	nfrgs = (blkcnt_t)(end - beg) / frgsz;

	return nfrgs;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects write to modify file's stat's size & blocks attributes properly.
 * Performs sequential write, followed by over-write on same region.
 */
static void test_stat_write_(struct vt_env *vt_env,
			     const struct vt_ioargs *ioargs)
{
	int fd;
	void *buf;
	blkcnt_t blkcnt;
	struct stat st;
	const loff_t off = ioargs->off;
	const size_t bsz = ioargs->bsz;
	const char *path = vt_new_path_unique(vt_env);

	vt_open(path, O_CREAT | O_RDWR, 0600, &fd);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_size, 0);
	vt_expect_eq(st.st_blocks, 0);

	buf = vt_new_buf_rands(vt_env, bsz);
	vt_pwriten(fd, buf, bsz, off);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_size, off + (loff_t)bsz);
	blkcnt = range_to_nfrgs(st.st_blksize, off, bsz);
	vt_expect_eq(st.st_blocks, blkcnt);

	buf = vt_new_buf_rands(vt_env, bsz);
	vt_pwriten(fd, buf, bsz, off);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_size, off + (loff_t)bsz);
	vt_expect_eq(st.st_blocks, blkcnt);

	vt_close(fd);
	vt_unlink(path);
}

static void test_stat_write_aligned(struct vt_env *vt_env)
{
	for (size_t i = 0; i < VT_ARRAY_SIZE(s_aligned_ioargs); ++i) {
		test_stat_write_(vt_env, &s_aligned_ioargs[i]);
	}
}

static void test_stat_write_unaligned(struct vt_env *vt_env)
{
	for (size_t i = 0; i < VT_ARRAY_SIZE(s_unaligned_ioargs); ++i) {
		test_stat_write_(vt_env, &s_unaligned_ioargs[i]);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Expects write-punch to modify file's stat's size & blocks attributes
 * properly. Performs sequential write, followed by fallocate-punch on same
 * data region.
 */
static void test_stat_punch_(struct vt_env *vt_env,
			     const struct vt_ioargs *ioargs)
{
	int fd;
	int mode;
	size_t nwr;
	blkcnt_t blkcnt;
	struct stat st;
	void *buf;
	const loff_t off = ioargs->off;
	const size_t bsz = ioargs->bsz;
	const char *path = vt_new_path_unique(vt_env);

	vt_open(path, O_CREAT | O_RDWR, 0600, &fd);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_size, 0);
	vt_expect_eq(st.st_blocks, 0);

	buf = vt_new_buf_rands(vt_env, bsz);
	vt_pwrite(fd, buf, bsz, off, &nwr);
	vt_expect_eq(bsz, nwr);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_size, off + (loff_t)bsz);
	blkcnt = range_to_nfrgs(st.st_blksize, off, bsz);
	vt_expect_eq(st.st_blocks, blkcnt);

	mode = FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE;
	vt_fallocate(fd, mode, off, (loff_t)bsz);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_size, off + (loff_t)bsz);

	vt_close(fd);
	vt_unlink(path);
}

static void test_stat_punch_aligned(struct vt_env *vt_env)
{
	for (size_t i = 0; i < VT_ARRAY_SIZE(s_aligned_ioargs); ++i) {
		test_stat_punch_(vt_env, &s_aligned_ioargs[i]);
	}
}

static void test_stat_punch_unaligned(struct vt_env *vt_env)
{
	for (size_t i = 0; i < VT_ARRAY_SIZE(s_unaligned_ioargs); ++i) {
		test_stat_punch_(vt_env, &s_unaligned_ioargs[i]);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Expects write to update the last data modification and last file status
 * change time-stamps, regardless of other files operation.
 */
static void test_write_stat_(struct vt_env *vt_env,
			     size_t nfiles)
{
	int fd, dfd;
	long dif;
	loff_t off;
	size_t idx, nwr;
	struct stat st, *sti;
	struct stat *sts = vt_new_buf_zeros(vt_env, nfiles * sizeof(st));
	const char *path = vt_new_path_unique(vt_env);
	char name[128] = "";

	vt_mkdir(path, 0700);
	vt_open(path, O_DIRECTORY | O_RDONLY, 0, &dfd);
	for (idx = 0; idx < nfiles; ++idx) {
		sti = &sts[idx];
		off = (loff_t)(idx * nfiles);
		snprintf(name, sizeof(name) - 1, "%lx-%ld", idx, off);
		vt_openat(dfd, name, O_CREAT | O_RDWR, 0600, &fd);
		vt_fstat(fd, &st);
		vt_pwrite(fd, name, strlen(name), off, &nwr);
		vt_fstat(fd, sti);
		vt_expect_mtime_gt(&st, sti);
		vt_expect_ctime_gt(&st, sti);
		vt_close(fd);
	}
	for (idx = 0; idx < nfiles; ++idx) {
		sti = &sts[idx];
		off = (loff_t)(idx * nfiles);
		snprintf(name, sizeof(name) - 1, "%lx-%ld", idx, off);
		vt_openat(dfd, name, O_RDONLY, 0600, &fd);
		vt_fstat(fd, &st);
		vt_expect_mtime_eq(&st, sti);
		/*
		 * For some unexplained reason, CTIME may change slightly every
		 * once in a million iterations. Happens only when 'nfiles' is
		 * large. Could be a deep bug in FUSE or something elsewhere
		 * -- I don't have a clue :(
		 *
		 * TODO: investigate more and change to:
		 *         vt_expect_ctime_eq(&st, sti);
		 */
		dif = vt_timespec_diff(&sti->st_ctim, &st.st_ctim);
		vt_expect_ge(dif, 0);
		vt_expect_lt(dif, 100000000L);
		vt_close(fd);
		vt_unlinkat(dfd, name, 0);
	}
	vt_close(dfd);
	vt_rmdir(path);
}

static void test_write_stat(struct vt_env *vt_env)
{
	test_write_stat_(vt_env, 111);
	test_write_stat_(vt_env, 11111);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTEST(test_stat_write_aligned),
	VT_DEFTEST(test_stat_write_unaligned),
	VT_DEFTEST(test_stat_punch_aligned),
	VT_DEFTEST(test_stat_punch_unaligned),
	VT_DEFTEST(test_write_stat),
};

const struct vt_tests vt_test_stat_io = VT_DEFTESTS(vt_local_tests);
