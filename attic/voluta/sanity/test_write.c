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
#include <errno.h>
#include <string.h>
#include "sanity.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful write(3p) of single block to regular file
 */
static void test_write_basic(struct voluta_t_ctx *t_ctx)
{
	int fd;
	void *buf1;
	size_t nwr = 0;
	size_t bsz = VOLUTA_BK_SIZE;
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_open(path, O_CREAT | O_WRONLY, 0600, &fd);
	voluta_t_write(fd, buf1, bsz, &nwr);
	voluta_t_expect_eq(bsz, nwr);

	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful write(3p) to unlinked file
 */
static void test_write_unlinked(struct voluta_t_ctx *t_ctx)
{
	int fd;
	void *buf1;
	size_t nwr = 0;
	size_t bsz = VOLUTA_BK_SIZE;
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_open(path, O_CREAT | O_WRONLY, 0600, &fd);
	voluta_t_unlink(path);

	voluta_t_write(fd, buf1, bsz, &nwr);
	voluta_t_expect_eq(bsz, nwr);
	voluta_t_write(fd, buf1, bsz, &nwr);
	voluta_t_expect_eq(bsz, nwr);
	voluta_t_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects pwrite(3p) to return -ESPIPE if an attempt was made to write to a
 * file-descriptor which is associated with a pipe or FIFO.
 */
static void test_write_espipe(struct voluta_t_ctx *t_ctx)
{
	int fd;
	void *buf1;
	size_t bsz = VOLUTA_BK_SIZE;
	const loff_t off = (loff_t)VOLUTA_GIGA;
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_mkfifo(path, 0777);
	voluta_t_open(path, O_RDWR, 0, &fd);
	voluta_t_pwrite_err(fd, buf1, bsz, off, -ESPIPE);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful write-read of single large-chunk to regular file
 */
static void test_write_read_chunk_(struct voluta_t_ctx *t_ctx,
				   size_t bsz)
{
	int fd;
	loff_t pos;
	void *buf1, *buf2;
	size_t nwr = 0, nrd = 0;
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
	buf2 = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_write(fd, buf1, bsz, &nwr);
	voluta_t_expect_eq(nwr, bsz);
	voluta_t_llseek(fd, 0, SEEK_SET, &pos);
	voluta_t_expect_eq(pos, 0);
	voluta_t_read(fd, buf2, bsz, &nrd);
	voluta_t_expect_eq(nrd, bsz);
	voluta_t_expect_eqm(buf1, buf2, bsz);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_write_read_chunk(struct voluta_t_ctx *t_ctx)
{
	test_write_read_chunk_(t_ctx, VOLUTA_MEGA / 2);
	test_write_read_chunk_(t_ctx, VOLUTA_MEGA * 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful pwrite(3p) to clear SUID bit, pread(3p) to not change
 */
static void test_write_read_suid(struct voluta_t_ctx *t_ctx)
{
	int fd;
	void *buf;
	loff_t off = 0;
	struct stat st;
	mode_t mode = 0610, mask = S_IRWXU | S_IRWXG | S_IRWXO;
	size_t nwr = 0, nrd = 0, bsz = VOLUTA_BK_SIZE;
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_open(path, O_CREAT | O_RDWR, mode, &fd);
	voluta_t_pwrite(fd, buf, bsz, off, &nwr);
	voluta_t_expect_eq(nwr, bsz);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & mask, mode);
	voluta_t_fchmod(fd, st.st_mode | S_ISUID);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISUID, S_ISUID);
	voluta_t_expect_eq(st.st_mode & mask, mode);
	voluta_t_pwrite(fd, buf, bsz, 4 * off, &nwr);
	voluta_t_expect_eq(nwr, bsz);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISUID, 0);
	voluta_t_expect_eq(st.st_mode & mask, mode);
	voluta_t_fchmod(fd, st.st_mode | S_ISUID);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISUID, S_ISUID);
	voluta_t_pread(fd, buf, bsz, off, &nrd);
	voluta_t_expect_eq(nrd, bsz);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISUID, S_ISUID);
	voluta_t_expect_eq(st.st_mode & mask, mode);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*
 * Expects successful pwrite(3p) to clear SGID bit, pread(3p) to not change
 */
static void test_write_read_sgid(struct voluta_t_ctx *t_ctx)
{
	int fd;
	void *buf;
	loff_t off = VOLUTA_MEGA;
	struct stat st;
	mode_t mode = 0710, mask = S_IRWXU | S_IRWXG | S_IRWXO;
	size_t nwr = 0, nrd = 0, bsz = VOLUTA_BK_SIZE;
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_open(path, O_CREAT | O_RDWR, mode, &fd);
	voluta_t_fstat(fd, &st);
	voluta_t_fchmod(fd, st.st_mode | S_ISGID);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISGID, S_ISGID);
	voluta_t_pwrite(fd, buf, bsz, off, &nwr);
	voluta_t_expect_eq(nwr, bsz);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISGID, 0);
	voluta_t_expect_eq(st.st_mode & mask, mode);
	voluta_t_fchmod(fd, st.st_mode | S_ISGID);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISGID, S_ISGID);
	voluta_t_pread(fd, buf, bsz, off, &nrd);
	voluta_t_expect_eq(nrd, bsz);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & S_ISGID, S_ISGID);
	voluta_t_expect_eq(st.st_mode & mask, mode);
	voluta_t_fchmod(fd, st.st_mode | S_ISUID | S_ISGID);
	voluta_t_pwrite(fd, buf, bsz, 2 * off, &nwr);
	voluta_t_expect_eq(nwr, bsz);
	voluta_t_fstat(fd, &st);
	voluta_t_expect_eq(st.st_mode & (S_ISUID | S_ISGID), 0);
	voluta_t_expect_eq(st.st_mode & mask, mode);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_write_basic),
	VOLUTA_T_DEFTEST(test_write_unlinked),
	VOLUTA_T_DEFTEST(test_write_espipe),
	VOLUTA_T_DEFTEST(test_write_read_chunk),
	VOLUTA_T_DEFTEST(test_write_read_suid),
	VOLUTA_T_DEFTEST(test_write_read_sgid),
};

const struct voluta_t_tests
voluta_t_test_write = VOLUTA_T_DEFTESTS(t_local_tests);


