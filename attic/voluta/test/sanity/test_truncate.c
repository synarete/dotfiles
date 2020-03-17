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
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "sanity.h"


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
		0, 1, BK_SIZE, MEGA, MEGA + 1, GIGA, GIGA - 1,
		11 * GIGA, 111 * GIGA - 111, TERA, TERA - 11, FILESIZE_MAX
	};
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path, 0600, &fd);
	for (size_t i = 0; i < ARRAY_SIZE(offs); ++i) {
		off = offs[i];
		voluta_t_truncate(path, off);
		voluta_t_stat(path, &st);
		voluta_t_expect_eq(st.st_size, off);
		voluta_t_expect_eq(st.st_blocks, 0);
	}
	for (size_t j = 0; j < ARRAY_SIZE(offs); ++j) {
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
	struct stat st;
	const char *dat = "ABCDEFGHIJKLMNOPQ";
	const size_t len = strlen(dat);
	const loff_t offs[] = {
		17, 7177, 17 * MEGA - 7, 17 * GIGA - 7,
		3 * TERA - 7, FILESIZE_MAX / 7,
	};
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path, 0600, &fd);
	for (size_t i = 0; i < ARRAY_SIZE(offs); ++i) {
		off = offs[i];
		voluta_t_pwriten(fd, dat, len, off - 1);
		voluta_t_truncate(path, off);
		voluta_t_stat(path, &st);
		voluta_t_expect_eq(st.st_size, off);
		voluta_t_expect_gt(st.st_blocks, 0);
	}
	for (size_t j = 0; j < ARRAY_SIZE(offs); ++j) {
		off = offs[j];
		voluta_t_pwriten(fd, dat, len, off - 3);
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
	size_t bsz = BK_SIZE;
	struct stat st;
	const loff_t offs[] = {
		UMEGA, UGIGA, UTERA,
		UMEGA - 1, UGIGA - 1, UTERA - 1
	};
	const void *buf = voluta_t_new_buf_rands(t_ctx, bsz);
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_creat(path, 0600, &fd);
	for (size_t i = 0; i < ARRAY_SIZE(offs); ++i) {
		off = offs[i];
		voluta_t_pwriten(fd, buf, bsz, off);
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
	const loff_t off = FILESIZE_MAX;
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
	loff_t off = UMEGA;
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
