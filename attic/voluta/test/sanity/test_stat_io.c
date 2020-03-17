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
#define VOLUTA_TEST 1
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sanity.h"


/* Common meta-info for io-tests */
struct voluta_t_ioargs {
	loff_t off;
	size_t bsz;
	size_t cnt;
};

static const struct voluta_t_ioargs s_aligned_ioargs[] = {
	{ 0, 1, 0 },
	{ 0, BK_SIZE, 0 },
	{ 0, UMEGA, 0 },
	{ BK_SIZE, BK_SIZE, 0 },
	{ BK_SIZE, 2 * BK_SIZE, 0 },
	{ BK_SIZE, UMEGA, 0 },
	{ UMEGA - BK_SIZE, BK_SIZE, 0 },
	{ UMEGA, BK_SIZE, 0 },
	{ UMEGA - BK_SIZE, 2 * BK_SIZE, 0 },
	{ UGIGA, BK_SIZE, 0 },
	{ UGIGA - BK_SIZE, 2 * BK_SIZE, 0 },
	{ UGIGA + BK_SIZE, BK_SIZE, 0 },
};

static const struct voluta_t_ioargs s_unaligned_ioargs[] = {
	{ 1, 2, 0 },
	{ 1, BK_SIZE - 2, 0 },
	{ 1, BK_SIZE + 2, 0 },
	{ 1, UMEGA - 2, 0 },
	{ 1, UMEGA + 2, 0 },
	{ BK_SIZE - 1, BK_SIZE + 2, 0 },
	{ UMEGA - BK_SIZE + 1, 2 * BK_SIZE + 1, 0 },
	{ UMEGA - 1, BK_SIZE + 11, 0 },
	{ UMEGA - BK_SIZE - 1, 11 * BK_SIZE, 0 },
	{ UGIGA - 1, BK_SIZE + 2, 0 },
	{ UGIGA - BK_SIZE - 1, 2 * BK_SIZE + 2, 0 },
	{ UGIGA + BK_SIZE + 1, BK_SIZE - 1, 0 },
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
static void test_stat_write_(struct voluta_t_ctx *t_ctx,
			     const struct voluta_t_ioargs *ioargs)
{
	int fd;
	void *buf;
	blkcnt_t blkcnt;
	struct stat st;
	const loff_t off = ioargs->off;
	const size_t bsz = ioargs->bsz;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, 0);
	voluta_t_expect_eq(st.st_blocks, 0);

	buf = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_pwriten(fd, buf, bsz, off);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, off + (loff_t)bsz);
	blkcnt = range_to_nfrgs(st.st_blksize, off, bsz);
	voluta_t_expect_eq(st.st_blocks, blkcnt);

	buf = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_pwriten(fd, buf, bsz, off);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, off + (loff_t)bsz);
	voluta_t_expect_eq(st.st_blocks, blkcnt);

	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_stat_write_aligned(struct voluta_t_ctx *t_ctx)
{
	for (size_t i = 0; i < ARRAY_SIZE(s_aligned_ioargs); ++i) {
		test_stat_write_(t_ctx, &s_aligned_ioargs[i]);
	}
}

static void test_stat_write_unaligned(struct voluta_t_ctx *t_ctx)
{
	for (size_t i = 0; i < ARRAY_SIZE(s_unaligned_ioargs); ++i) {
		test_stat_write_(t_ctx, &s_unaligned_ioargs[i]);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Expects write-punch to modify file's stat's size & blocks attributes
 * properly. Performs sequential write, followed by fallocate-punch on same
 * data region.
 */
static void test_stat_punch_(struct voluta_t_ctx *t_ctx,
			     const struct voluta_t_ioargs *ioargs)
{
	int fd;
	int mode;
	size_t nwr;
	blkcnt_t blkcnt;
	struct stat st;
	void *buf;
	const loff_t off = ioargs->off;
	const size_t bsz = ioargs->bsz;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, 0);
	voluta_t_expect_eq(st.st_blocks, 0);

	buf = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_pwrite(fd, buf, bsz, off, &nwr);
	voluta_t_expect_eq(bsz, nwr);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, off + (loff_t)bsz);
	blkcnt = range_to_nfrgs(st.st_blksize, off, bsz);
	voluta_t_expect_eq(st.st_blocks, blkcnt);

	mode = FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE;
	voluta_t_fallocate(fd, mode, off, (loff_t)bsz);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, off + (loff_t)bsz);

	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_stat_punch_aligned(struct voluta_t_ctx *t_ctx)
{
	for (size_t i = 0; i < ARRAY_SIZE(s_aligned_ioargs); ++i) {
		test_stat_punch_(t_ctx, &s_aligned_ioargs[i]);
	}
}

static void test_stat_punch_unaligned(struct voluta_t_ctx *t_ctx)
{
	for (size_t i = 0; i < ARRAY_SIZE(s_unaligned_ioargs); ++i) {
		test_stat_punch_(t_ctx, &s_unaligned_ioargs[i]);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Expects write to update the last data modification and last file status
 * change time-stamps, regardless of other files operation.
 */
static void test_write_stat_(struct voluta_t_ctx *t_ctx,
			     size_t nfiles)
{
	int fd, dfd;
	long dif;
	loff_t off;
	size_t idx, nwr;
	struct stat st, *sti;
	struct stat *sts = voluta_t_new_buf_zeros(t_ctx, nfiles * sizeof(st));
	const char *path = voluta_t_new_path_unique(t_ctx);
	char name[128] = "";

	voluta_t_mkdir(path, 0700);
	voluta_t_open(path, O_DIRECTORY | O_RDONLY, 0, &dfd);
	for (idx = 0; idx < nfiles; ++idx) {
		sti = &sts[idx];
		off = (loff_t)(idx * nfiles);
		snprintf(name, sizeof(name) - 1, "%lx-%ld", idx, off);
		voluta_t_openat(dfd, name, O_CREAT | O_RDWR, 0600, &fd);
		voluta_t_fstat(fd, &st);
		voluta_t_pwrite(fd, name, strlen(name), off, &nwr);
		voluta_t_fstat(fd, sti);
		voluta_t_expect_mtime_gt(&st, sti);
		voluta_t_expect_ctime_gt(&st, sti);
		voluta_t_close(fd);
	}
	for (idx = 0; idx < nfiles; ++idx) {
		sti = &sts[idx];
		off = (loff_t)(idx * nfiles);
		snprintf(name, sizeof(name) - 1, "%lx-%ld", idx, off);
		voluta_t_openat(dfd, name, O_RDONLY, 0600, &fd);
		voluta_t_fstat(fd, &st);
		voluta_t_expect_mtime_eq(&st, sti);
		/*
		 * For some unexplained reason, CTIME may change slightly every
		 * once in a million iterations. Happens only when 'nfiles' is
		 * large. Could be a deep bug in FUSE or something elsewhere
		 * -- I don't have a clue :(
		 *
		 * TODO: investigate more and change to:
		 *         voluta_t_expect_ctime_eq(&st, sti);
		 */
		dif = voluta_t_timespec_diff(&sti->st_ctim, &st.st_ctim);
		voluta_t_expect_ge(dif, 0);
		voluta_t_expect_lt(dif, 100000000L);
		voluta_t_close(fd);
		voluta_t_unlinkat(dfd, name, 0);
	}
	voluta_t_close(dfd);
	voluta_t_rmdir(path);
}

static void test_write_stat(struct voluta_t_ctx *t_ctx)
{
	test_write_stat_(t_ctx, 111);
	test_write_stat_(t_ctx, 11111);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_stat_write_aligned),
	VOLUTA_T_DEFTEST(test_stat_write_unaligned),
	VOLUTA_T_DEFTEST(test_stat_punch_aligned),
	VOLUTA_T_DEFTEST(test_stat_punch_unaligned),
	VOLUTA_T_DEFTEST(test_write_stat),
};

const struct voluta_t_tests
voluta_t_test_stat_io = VOLUTA_T_DEFTESTS(t_local_tests);
