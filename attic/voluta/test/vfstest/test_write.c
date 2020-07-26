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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "vfstest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful write(3p) of single block to regular file
 */
static void test_write_basic(struct vt_env *vt_env)
{
	int fd;
	void *buf1;
	size_t nwr = 0;
	size_t bsz = VT_BK_SIZE;
	const char *path = vt_new_path_unique(vt_env);

	buf1 = vt_new_buf_rands(vt_env, bsz);
	vt_open(path, O_CREAT | O_WRONLY, 0600, &fd);
	vt_write(fd, buf1, bsz, &nwr);
	vt_expect_eq(bsz, nwr);

	vt_close(fd);
	vt_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful write(3p) to unlinked file
 */
static void test_write_unlinked(struct vt_env *vt_env)
{
	int fd;
	void *buf1;
	size_t nwr = 0;
	size_t bsz = VT_BK_SIZE;
	const char *path = vt_new_path_unique(vt_env);

	buf1 = vt_new_buf_rands(vt_env, bsz);
	vt_open(path, O_CREAT | O_WRONLY, 0600, &fd);
	vt_unlink(path);

	vt_write(fd, buf1, bsz, &nwr);
	vt_expect_eq(bsz, nwr);
	vt_write(fd, buf1, bsz, &nwr);
	vt_expect_eq(bsz, nwr);
	vt_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects pwrite(3p) to return -ESPIPE if an attempt was made to write to a
 * file-descriptor which is associated with a pipe or FIFO.
 */
static void test_write_espipe(struct vt_env *vt_env)
{
	int fd;
	void *buf1;
	size_t bsz = VT_BK_SIZE;
	const loff_t off = (loff_t)VT_UGIGA;
	const char *path = vt_new_path_unique(vt_env);

	buf1 = vt_new_buf_rands(vt_env, bsz);
	vt_mkfifo(path, 0777);
	vt_open(path, O_RDWR, 0, &fd);
	vt_pwrite_err(fd, buf1, bsz, off, -ESPIPE);
	vt_close(fd);
	vt_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful write-read of single large-chunk to regular file
 */
static void test_write_read_chunk_(struct vt_env *vt_env,
				   size_t bsz)
{
	int fd;
	loff_t pos;
	void *buf1, *buf2;
	size_t nwr = 0, nrd = 0;
	const char *path = vt_new_path_unique(vt_env);

	buf1 = vt_new_buf_rands(vt_env, bsz);
	buf2 = vt_new_buf_rands(vt_env, bsz);
	vt_open(path, O_CREAT | O_RDWR, 0600, &fd);
	vt_write(fd, buf1, bsz, &nwr);
	vt_expect_eq(nwr, bsz);
	vt_llseek(fd, 0, SEEK_SET, &pos);
	vt_expect_eq(pos, 0);
	vt_read(fd, buf2, bsz, &nrd);
	vt_expect_eq(nrd, bsz);
	vt_expect_eqm(buf1, buf2, bsz);
	vt_close(fd);
	vt_unlink(path);
}

static void test_write_read_chunk(struct vt_env *vt_env)
{
	test_write_read_chunk_(vt_env, VT_UMEGA / 2);
	test_write_read_chunk_(vt_env, VT_UMEGA * 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful pwrite(3p) to clear SUID bit, pread(3p) to not change
 */
static void test_write_read_suid_(struct vt_env *vt_env,
				  loff_t off, size_t bsz)
{
	int fd;
	void *buf;
	struct stat st;
	mode_t mode = 0610;
	mode_t mask = S_IRWXU | S_IRWXG | S_IRWXO;
	const char *path = vt_new_path_unique(vt_env);

	buf = vt_new_buf_rands(vt_env, bsz);
	vt_open(path, O_CREAT | O_RDWR, mode, &fd);
	vt_pwriten(fd, buf, bsz, off);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & mask, mode);
	vt_fchmod(fd, st.st_mode | S_ISUID);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & S_ISUID, S_ISUID);
	vt_expect_eq(st.st_mode & mask, mode);
	vt_pwriten(fd, buf, bsz, off + (loff_t)bsz);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & S_ISUID, 0);
	vt_expect_eq(st.st_mode & mask, mode);
	vt_fchmod(fd, st.st_mode | S_ISUID);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & S_ISUID, S_ISUID);
	vt_preadn(fd, buf, bsz, off);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & S_ISUID, S_ISUID);
	vt_expect_eq(st.st_mode & mask, mode);
	vt_close(fd);
	vt_unlink(path);
}

static void test_write_read_suid(struct vt_env *vt_env)
{
	test_write_read_suid_(vt_env, 0, VT_BK_SIZE);
	test_write_read_suid_(vt_env, 0, VT_MEGA);
	test_write_read_suid_(vt_env, VT_GIGA - 1, VT_BK_SIZE);
	test_write_read_suid_(vt_env, VT_TERA, VT_MEGA);
	test_write_read_suid_(vt_env, VT_TERA - 1, VT_MEGA + 3);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Expects successful pwrite(3p) to clear SGID bit, pread(3p) to not change
 */
static void test_write_read_sgid_(struct vt_env *vt_env,
				  loff_t off, size_t bsz)
{
	int fd;
	mode_t mode = 0710;
	mode_t mask = S_IRWXU | S_IRWXG | S_IRWXO;
	size_t nwr = 0;
	size_t nrd = 0;
	struct stat st;
	void *buf = vt_new_buf_rands(vt_env, bsz);
	const char *path = vt_new_path_unique(vt_env);

	vt_open(path, O_CREAT | O_RDWR, mode, &fd);
	vt_fstat(fd, &st);
	vt_fchmod(fd, st.st_mode | S_ISGID);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & S_ISGID, S_ISGID);
	vt_pwrite(fd, buf, bsz, off, &nwr);
	vt_expect_eq(nwr, bsz);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & S_ISGID, 0);
	vt_expect_eq(st.st_mode & mask, mode);
	vt_fchmod(fd, st.st_mode | S_ISGID);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & S_ISGID, S_ISGID);
	vt_pread(fd, buf, bsz, off, &nrd);
	vt_expect_eq(nrd, bsz);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & S_ISGID, S_ISGID);
	vt_expect_eq(st.st_mode & mask, mode);
	vt_fchmod(fd, st.st_mode | S_ISUID | S_ISGID);
	vt_pwrite(fd, buf, bsz, 2 * off, &nwr);
	vt_expect_eq(nwr, bsz);
	vt_fstat(fd, &st);
	vt_expect_eq(st.st_mode & (S_ISUID | S_ISGID), 0);
	vt_expect_eq(st.st_mode & mask, mode);
	vt_close(fd);
	vt_unlink(path);
}

static void test_write_read_sgid(struct vt_env *vt_env)
{
	test_write_read_sgid_(vt_env, 0, VT_BK_SIZE);
	test_write_read_sgid_(vt_env, VT_MEGA - 1, VT_BK_SIZE + 3);
	test_write_read_sgid_(vt_env, VT_GIGA, VT_MEGA);
	test_write_read_sgid_(vt_env, VT_TERA - 3, VT_MEGA / 3);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTEST(test_write_basic),
	VT_DEFTEST(test_write_unlinked),
	VT_DEFTEST(test_write_espipe),
	VT_DEFTEST(test_write_read_chunk),
	VT_DEFTEST(test_write_read_suid),
	VT_DEFTEST(test_write_read_sgid),
};

const struct vt_tests vt_test_write = VT_DEFTESTS(vt_local_tests);


