/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * This file is part of voluta.
 *
 * Copyright (C) 2019 Shachar Sharon
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
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "fstests.h"


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects truncate(3p) on empty regular file to update size properly.
 */
static void test_truncate_simple(struct voluta_t_ctx *t_ctx)
{
	int fd;
	loff_t off;
	struct stat st;
	const loff_t offs[] = {
		0, 1, VOLUTA_BK_SIZE, VOLUTA_MEGA,
		VOLUTA_GIGA, VOLUTA_TERA
	};
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path, 0600, &fd);
	for (size_t i = 0; i < VOLUTA_ARRAY_SIZE(offs); ++i) {
		off = offs[i];
		voluta_t_truncate(path, off);
		voluta_t_stat(path, &st);
		voluta_t_expect_eq(st.st_size, off);
		voluta_t_expect_eq(st.st_blocks, 0);
	}
	for (size_t j = 0; j < VOLUTA_ARRAY_SIZE(offs); ++j) {
		off = offs[j];
		voluta_t_ftruncate(fd, off);
		voluta_t_fstat(fd, &st);
		voluta_t_expect_eq(st.st_size, off);
		voluta_t_expect_eq(st.st_blocks, 0);
	}
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects truncate(3p) after write on unaligned offsets to update
 * size properly.
 */
static void test_truncate_unaligned(struct voluta_t_ctx *t_ctx)
{
	int fd;
	loff_t off;
	size_t nwr;
	struct stat st;
	const char *dat = "ABCDEFGHIJKLMNOPQ";
	const size_t len = strlen(dat);
	const loff_t offs[] = {
		17, 7177, 17 * VOLUTA_MEGA - 7,
		17 * VOLUTA_GIGA - 7, 17 * VOLUTA_TERA - 7
	};
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path, 0600, &fd);
	for (size_t i = 0; i < VOLUTA_ARRAY_SIZE(offs); ++i) {
		off = offs[i];
		voluta_t_pwrite(fd, dat, len, off - 1, &nwr);
		voluta_t_expect_eq(len, nwr);
		voluta_t_truncate(path, off);
		voluta_t_stat(path, &st);
		voluta_t_expect_eq(st.st_size, off);
		voluta_t_expect_gt(st.st_blocks, 0);
	}
	for (size_t j = 0; j < VOLUTA_ARRAY_SIZE(offs); ++j) {
		off = offs[j];
		voluta_t_pwrite(fd, dat, len, off - 3, &nwr);
		voluta_t_expect_eq(len, nwr);
		voluta_t_ftruncate(fd, off);
		voluta_t_fstat(fd, &st);
		voluta_t_expect_eq(st.st_size, off);
	}
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects ftruncate(3p) to zero after write to clear file's block count.
 */
static void test_truncate_zero(struct voluta_t_ctx *t_ctx)
{
	int fd;
	loff_t off;
	size_t nwr, bsz = VOLUTA_BK_SIZE;
	void *buf;
	struct stat st;
	const loff_t offs[] = {
		VOLUTA_MEGA, VOLUTA_GIGA, VOLUTA_TERA,
		VOLUTA_MEGA - 1, VOLUTA_GIGA - 1, VOLUTA_TERA - 1
	};
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path, 0600, &fd);
	buf = voluta_t_new_buf_rands(t_ctx, bsz);
	for (size_t i = 0; i < VOLUTA_ARRAY_SIZE(offs); ++i) {
		off = offs[i];
		voluta_t_pwrite(fd, buf, bsz, off, &nwr);
		voluta_t_expect_eq(bsz, nwr);
		voluta_t_fstat(fd, &st);
		voluta_t_expect_gt(st.st_blocks, 0);
		voluta_t_ftruncate(fd, 0);
		voluta_t_fstat(fd, &st);
		voluta_t_expect_eq(st.st_blocks, 0);
		voluta_t_ftruncate(fd, off);
		voluta_t_fstat(fd, &st);
		voluta_t_expect_eq(st.st_size, off);
	}
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects ftruncate(3p) to change size successfully on file-size-limit
 */
static void test_truncate_filesize_max(struct voluta_t_ctx *t_ctx)
{
	int fd;
	struct stat st;
	const loff_t off = VOLUTA_FILESIZE_MAX;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path, 0600, &fd);
	voluta_t_ftruncate(fd, off);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, off);

	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful ftruncate(3p) to clear SUID/SGID bits
 */
static void test_truncate_suid_sgid(struct voluta_t_ctx *t_ctx)
{
	int fd;
	loff_t off = VOLUTA_MEGA;
	struct stat st;
	const mode_t mode = 0770;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, mode, &fd);
	voluta_t_fstat(fd, &st);
	voluta_t_fchmod(fd, st.st_mode | S_ISUID | S_ISGID);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISGID, S_ISGID);
	voluta_t_expect_eq(st.st_mode & S_ISUID, S_ISUID);
	voluta_t_ftruncate(fd, off);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, off);
	voluta_t_expect_eq(st.st_mode & S_ISUID, 0);
	voluta_t_expect_eq(st.st_mode & S_ISGID, 0);

	voluta_t_fchmod(fd, st.st_mode | S_ISUID | S_ISGID);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISGID, S_ISGID);
	voluta_t_expect_eq(st.st_mode & S_ISUID, S_ISUID);
	voluta_t_ftruncate(fd, 0);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_size, 0);
	voluta_t_expect_eq(st.st_mode & S_ISUID, 0);
	voluta_t_expect_eq(st.st_mode & S_ISGID, 0);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_truncate_simple),
	VOLUTA_T_DEFTEST(test_truncate_unaligned),
	VOLUTA_T_DEFTEST(test_truncate_zero),
	VOLUTA_T_DEFTEST(test_truncate_filesize_max),
	VOLUTA_T_DEFTEST(test_truncate_suid_sgid),
};

const struct voluta_t_tests
voluta_t_test_truncate = VOLUTA_T_DEFTESTS(t_local_tests);
