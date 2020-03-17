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
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "sanity.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects rename(3p) to successfully change file's new-name and return ENOENT
 * on old-name.
 */
static void test_rename_simple(struct voluta_t_ctx *t_ctx)
{
	int fd;
	ino_t ino;
	mode_t ifmt = S_IFMT;
	struct stat st1, st2;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_under(t_ctx, path0);
	const char *path2 = voluta_t_new_path_under(t_ctx, path0);

	voluta_t_mkdir(path0, 0755);
	voluta_t_creat(path1, 0644, &fd);
	voluta_t_stat(path1, &st1);
	voluta_t_expect_true(S_ISREG(st1.st_mode));
	voluta_t_expect_eq((st1.st_mode & ~ifmt), 0644);
	voluta_t_expect_eq(st1.st_nlink, 1);

	ino = st1.st_ino;
	voluta_t_rename(path1, path2);
	voluta_t_stat_err(path1, -ENOENT);
	voluta_t_fstat(fd, &st1);
	voluta_t_expect_eq(st1.st_ino, ino);
	voluta_t_stat(path2, &st2);
	voluta_t_expect_true(S_ISREG(st2.st_mode));
	voluta_t_expect_eq((st2.st_mode & ~ifmt), 0644);
	voluta_t_expect_eq(st2.st_nlink, 1);

	voluta_t_unlink(path2);
	voluta_t_rmdir(path0);
	voluta_t_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects rename(3p) to update ctime only when successful.
 */
static void test_rename_ctime(struct voluta_t_ctx *t_ctx)
{
	int fd;
	struct stat st0, st1;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_unique(t_ctx);
	const char *path2 = voluta_t_new_path_unique(t_ctx);
	const char *path3 = voluta_t_new_path_under(t_ctx, path2);

	voluta_t_creat(path0, 0644, &fd);
	voluta_t_close(fd);
	voluta_t_stat(path0, &st0);
	voluta_t_suspends(t_ctx, 2);
	voluta_t_rename(path0, path1);
	voluta_t_stat(path1, &st1);
	voluta_t_expect_lt(st0.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	voluta_t_unlink(path1);

	voluta_t_mkdir(path0, 0700);
	voluta_t_stat(path0, &st0);
	voluta_t_suspends(t_ctx, 2);
	voluta_t_rename(path0, path1);
	voluta_t_stat(path1, &st1);
	voluta_t_expect_lt(st0.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	voluta_t_rmdir(path1);

	voluta_t_mkfifo(path0, 0644);
	voluta_t_stat(path0, &st0);
	voluta_t_suspends(t_ctx, 2);
	voluta_t_rename(path0, path1);
	voluta_t_stat(path1, &st1);
	voluta_t_expect_lt(st0.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	voluta_t_unlink(path1);

	voluta_t_symlink(path2, path0);
	voluta_t_lstat(path0, &st0);
	voluta_t_suspends(t_ctx, 2);
	voluta_t_rename(path0, path1);
	voluta_t_lstat(path1, &st1);
	voluta_t_expect_lt(st0.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	voluta_t_unlink(path1);

	voluta_t_creat(path0, 0644, &fd);
	voluta_t_close(fd);
	voluta_t_stat(path0, &st0);
	voluta_t_suspends(t_ctx, 2);
	voluta_t_rename_err(path0, path3, -ENOENT);
	voluta_t_stat(path0, &st1);
	voluta_t_expect_eq(st0.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	voluta_t_unlink(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects rename(3p) to returns ENOTDIR when the 'from' argument is a
 * directory, but 'to' is not a directory.
 */
static void test_rename_notdirto(struct voluta_t_ctx *t_ctx)
{
	int fd;
	struct stat st0, st1;
	char *path0, *path1;

	path0 = voluta_t_new_path_unique(t_ctx);
	path1 = voluta_t_new_path_unique(t_ctx);

	voluta_t_mkdir(path0, 0750);
	voluta_t_creat(path1, 0644, &fd);
	voluta_t_close(fd);
	voluta_t_rename_err(path0, path1, -ENOTDIR);
	voluta_t_lstat(path0, &st0);
	voluta_t_expect_true(S_ISDIR(st0.st_mode));
	voluta_t_unlink(path1);

	voluta_t_symlink("test-rename-notdirto", path1);
	voluta_t_rename_err(path0, path1, -ENOTDIR);
	voluta_t_lstat(path0, &st0);
	voluta_t_expect_true(S_ISDIR(st0.st_mode));
	voluta_t_lstat(path1, &st1);
	voluta_t_expect_true(S_ISLNK(st1.st_mode));
	voluta_t_unlink(path1);

	voluta_t_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects rename(3p) to returns EISDIR when the 'to' argument is a
 * directory, but 'from' is not a directory.
 */
static void test_rename_isdirto(struct voluta_t_ctx *t_ctx)
{
	int fd;
	struct stat st0, st1;
	char *path0, *path1;

	path0 = voluta_t_new_path_unique(t_ctx);
	path1 = voluta_t_new_path_unique(t_ctx);

	voluta_t_mkdir(path0, 0750);
	voluta_t_creat(path1, 0640, &fd);
	voluta_t_close(fd);
	voluta_t_rename_err(path1, path0, -EISDIR);
	voluta_t_lstat(path0, &st0);
	voluta_t_expect_true(S_ISDIR(st0.st_mode));
	voluta_t_unlink(path1);

	voluta_t_mkfifo(path1, 0644);
	voluta_t_rename_err(path1, path0, -EISDIR);
	voluta_t_lstat(path0, &st0);
	voluta_t_expect_true(S_ISDIR(st0.st_mode));
	voluta_t_lstat(path1, &st1);
	voluta_t_expect_true(S_ISFIFO(st1.st_mode));
	voluta_t_unlink(path1);

	voluta_t_symlink("test-rename-isdirto", path1);
	voluta_t_rename_err(path1, path0, -EISDIR);
	voluta_t_lstat(path0, &st0);
	voluta_t_expect_true(S_ISDIR(st0.st_mode));
	voluta_t_lstat(path1, &st1);
	voluta_t_expect_lnk(st1.st_mode);
	voluta_t_unlink(path1);

	voluta_t_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test rename(3p) with symlink(3p)
 */
static void test_rename_symlink(struct voluta_t_ctx *t_ctx)
{
	int fd;
	void *buf;
	size_t nwr, bsz = BK_SIZE;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_under(t_ctx, path0);
	const char *path2 = voluta_t_new_path_under(t_ctx, path0);
	const char *path3 = voluta_t_new_path_under(t_ctx, path0);

	voluta_t_mkdir(path0, 0755);
	voluta_t_creat(path1, 0600, &fd);
	buf = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_write(fd, buf, bsz, &nwr);
	voluta_t_symlink(path1, path2);
	voluta_t_rename(path2, path3);
	voluta_t_close(fd);
	voluta_t_unlink(path1);
	voluta_t_unlink(path3);
	voluta_t_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test rename(3p) to preserve proper nlink semantics
 */
static void test_rename_nlink(struct voluta_t_ctx *t_ctx)
{
	int fd;
	struct stat st;
	const char *path_x = voluta_t_new_path_unique(t_ctx);
	const char *path_a = voluta_t_new_path_under(t_ctx, path_x);
	const char *path_b = voluta_t_new_path_under(t_ctx, path_x);
	const char *path_c = voluta_t_new_path_under(t_ctx, path_x);
	const char *path_d = voluta_t_new_path_under(t_ctx, path_x);
	const char *path_ab = voluta_t_new_path_under(t_ctx, path_a);
	const char *path_abc = voluta_t_new_path_under(t_ctx, path_ab);
	const char *path_abcd = voluta_t_new_path_under(t_ctx, path_abc);

	voluta_t_mkdir(path_x, 0700);
	voluta_t_stat(path_x, &st);
	voluta_t_expect_eq(st.st_nlink, 2);
	voluta_t_mkdir(path_a, 0700);
	voluta_t_mkdir(path_b, 0700);
	voluta_t_mkdir(path_c, 0700);
	voluta_t_stat(path_x, &st);
	voluta_t_expect_eq(st.st_nlink, 5);
	voluta_t_creat(path_d, 0600, &fd);
	voluta_t_close(fd);
	voluta_t_stat(path_x, &st);
	voluta_t_expect_eq(st.st_nlink, 5);
	voluta_t_stat(path_d, &st);
	voluta_t_expect_eq(st.st_nlink, 1);

	voluta_t_rename(path_b, path_ab);
	voluta_t_stat_noent(path_b);
	voluta_t_stat(path_x, &st);
	voluta_t_expect_eq(st.st_nlink, 4);
	voluta_t_stat(path_a, &st);
	voluta_t_expect_eq(st.st_nlink, 3);
	voluta_t_stat(path_ab, &st);
	voluta_t_expect_eq(st.st_nlink, 2);

	voluta_t_rename(path_c, path_abc);
	voluta_t_stat_noent(path_c);
	voluta_t_stat(path_x, &st);
	voluta_t_expect_eq(st.st_nlink, 3);
	voluta_t_stat(path_a, &st);
	voluta_t_expect_eq(st.st_nlink, 3);
	voluta_t_stat(path_ab, &st);
	voluta_t_expect_eq(st.st_nlink, 3);
	voluta_t_stat(path_abc, &st);
	voluta_t_expect_eq(st.st_nlink, 2);

	voluta_t_rename(path_d, path_abcd);
	voluta_t_stat_noent(path_d);
	voluta_t_stat(path_x, &st);
	voluta_t_expect_eq(st.st_nlink, 3);
	voluta_t_stat(path_a, &st);
	voluta_t_expect_eq(st.st_nlink, 3);
	voluta_t_stat(path_ab, &st);
	voluta_t_expect_eq(st.st_nlink, 3);
	voluta_t_stat(path_abc, &st);
	voluta_t_expect_eq(st.st_nlink, 2);
	voluta_t_stat(path_abcd, &st);
	voluta_t_expect_eq(st.st_nlink, 1);

	voluta_t_unlink(path_abcd);
	voluta_t_rmdir(path_abc);
	voluta_t_rmdir(path_ab);
	voluta_t_rmdir(path_a);
	voluta_t_rmdir(path_x);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test rename(3p) within same directory without implicit unlink, where parent
 * directory is already populated with sibling links.
 */
static void test_rename_child_(struct voluta_t_ctx *t_ctx, int nsibs)
{
	int fd, i;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = voluta_t_new_path_under(t_ctx, path0);
	const char *path2 = voluta_t_new_path_under(t_ctx, path0);
	const char *path3 = NULL;

	voluta_t_mkdir(path0, 0700);
	for (i = 0; i < nsibs; ++i) {
		path3 = voluta_t_new_pathf(t_ctx, path0, "%08x", i);
		voluta_t_creat(path3, 0600, &fd);
		voluta_t_close(fd);
	}
	voluta_t_creat(path1, 0600, &fd);
	voluta_t_close(fd);
	voluta_t_rename(path1, path2);
	voluta_t_unlink(path2);
	for (i = 0; i < nsibs; ++i) {
		path3 = voluta_t_new_pathf(t_ctx, path0, "%08x", i);
		voluta_t_unlink(path3);
	}
	voluta_t_rmdir(path0);
}

static void test_rename_child(struct voluta_t_ctx *t_ctx)
{
	test_rename_child_(t_ctx, 16);
	test_rename_child_(t_ctx, 1024);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test rename(3p) within same directory with implicit unlink of target sibling
 * file.
 */
static void test_rename_replace_(struct voluta_t_ctx *t_ctx, int nsibs)
{
	int fd, i;
	char *path0, *path1, *path2;

	path0 = voluta_t_new_path_unique(t_ctx);
	path2 = voluta_t_new_path_under(t_ctx, path0);

	voluta_t_mkdir(path0, 0700);
	for (i = 0; i < nsibs; ++i) {
		path1 = voluta_t_new_pathf(t_ctx, path0, "%08x", i);
		voluta_t_creat(path1, 0600, &fd);
		voluta_t_close(fd);
	}
	voluta_t_creat(path2, 0600, &fd);
	voluta_t_close(fd);
	for (i = 0; i < nsibs; ++i) {
		path1 = voluta_t_new_pathf(t_ctx, path0, "%08x", i);
		voluta_t_rename(path2, path1);
		path2 = path1;
	}
	voluta_t_unlink(path2);
	voluta_t_rmdir(path0);
}

static void test_rename_replace(struct voluta_t_ctx *t_ctx)
{
	test_rename_replace_(t_ctx, 1);
	test_rename_replace_(t_ctx, 2);
	test_rename_replace_(t_ctx, 3);
	test_rename_replace_(t_ctx, 1024);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test rename(3p) between two directories without implicit unlink of target.
 */
static void test_rename_move_(struct voluta_t_ctx *t_ctx, size_t cnt)
{
	int fd;
	size_t i;
	char *src_path1, *tgt_path1;
	const char *src_path0 = voluta_t_new_path_unique(t_ctx);
	const char *tgt_path0 = voluta_t_new_path_unique(t_ctx);

	voluta_t_mkdir(src_path0, 0700);
	voluta_t_mkdir(tgt_path0, 0700);
	for (i = 0; i < cnt; ++i) {
		src_path1 = voluta_t_new_pathf(t_ctx, src_path0, "s%08x", i);
		voluta_t_creat(src_path1, 0600, &fd);
		voluta_t_close(fd);
	}
	for (i = 0; i < cnt; ++i) {
		src_path1 = voluta_t_new_pathf(t_ctx, src_path0, "s%08x", i);
		tgt_path1 = voluta_t_new_pathf(t_ctx, tgt_path0, "t%08x", i);
		voluta_t_rename(src_path1, tgt_path1);
	}
	for (i = 0; i < cnt; ++i) {
		tgt_path1 = voluta_t_new_pathf(t_ctx, tgt_path0, "t%08x", i);
		voluta_t_unlink(tgt_path1);
	}
	voluta_t_rmdir(src_path0);
	voluta_t_rmdir(tgt_path0);
}

static void test_rename_move(struct voluta_t_ctx *t_ctx)
{
	test_rename_move_(t_ctx, 1);
	test_rename_move_(t_ctx, 2);
	test_rename_move_(t_ctx, 3);
	test_rename_move_(t_ctx, 1024);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test rename(3p) between two directories with implicit truncate-unlink of
 * target.
 */
static void test_rename_override_(struct voluta_t_ctx *t_ctx,
				  size_t cnt, size_t bsz)
{
	int fd;
	void *buf;
	size_t i;
	char *src_path1, *tgt_path1;
	const char *src_path0 = voluta_t_new_path_unique(t_ctx);
	const char *tgt_path0 = voluta_t_new_path_unique(t_ctx);

	buf = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_mkdir(src_path0, 0700);
	voluta_t_mkdir(tgt_path0, 0700);
	for (i = 0; i < cnt; ++i) {
		src_path1 = voluta_t_new_pathf(t_ctx, src_path0, "s%08x", i);
		tgt_path1 = voluta_t_new_pathf(t_ctx, tgt_path0, "t%08x", i);
		voluta_t_creat(src_path1, 0600, &fd);
		voluta_t_close(fd);
		voluta_t_creat(tgt_path1, 0600, &fd);
		voluta_t_pwriten(fd, buf, bsz, (loff_t)i);
		voluta_t_close(fd);
	}
	for (i = 0; i < cnt; ++i) {
		src_path1 = voluta_t_new_pathf(t_ctx, src_path0, "s%08x", i);
		tgt_path1 = voluta_t_new_pathf(t_ctx, tgt_path0, "t%08x", i);
		voluta_t_rename(src_path1, tgt_path1);
	}
	for (i = 0; i < cnt; ++i) {
		tgt_path1 = voluta_t_new_pathf(t_ctx, tgt_path0, "t%08x", i);
		voluta_t_unlink(tgt_path1);
	}
	voluta_t_rmdir(src_path0);
	voluta_t_rmdir(tgt_path0);
}

static void test_rename_override(struct voluta_t_ctx *t_ctx)
{
	test_rename_override_(t_ctx, 1, BK_SIZE);
	test_rename_override_(t_ctx, 3, BK_SIZE);
	test_rename_override_(t_ctx, 7, UMEGA);
	test_rename_override_(t_ctx, 1024, BK_SIZE);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test renameat(2) within same directory with different names length
 */
static const char *make_name(struct voluta_t_ctx *t_ctx, size_t len, char ch)
{
	size_t nn;
	char str[VOLUTA_NAME_MAX + 1] = "";

	nn = (len < sizeof(str)) ? len : (sizeof(str) - 1);
	memset(str, ch, nn);
	str[nn] = '\0';

	return voluta_t_strdup(t_ctx, str);
}

static const char *dup_name(struct voluta_t_ctx *t_ctx, const char *str)
{
	return voluta_t_strdup(t_ctx, str);
}

static void test_renameat_inplace(struct voluta_t_ctx *t_ctx)
{
	int dfd, fd;
	size_t i, count = VOLUTA_NAME_MAX;
	const char *name1;
	const char *name2;
	const char ch = 'A';
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_mkdir(path, 0700);
	voluta_t_open(path, O_DIRECTORY | O_RDONLY, 0, &dfd);

	name1 = make_name(t_ctx, 1, ch);
	voluta_t_openat(dfd, name1, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_close(fd);

	for (i = 2; i <= count; ++i) {
		name2 = make_name(t_ctx, i, ch);
		voluta_t_renameat(dfd, name1, dfd, name2);
		name1 = dup_name(t_ctx, name2);
	}
	count = sizeof(name1) - 1;
	for (i = count; i > 0; --i) {
		name2 = make_name(t_ctx, i, ch);
		voluta_t_renameat(dfd, name1, dfd, name2);
		name1 = dup_name(t_ctx, name2);
	}
	voluta_t_unlinkat(dfd, name1, 0);
	voluta_t_close(dfd);
	voluta_t_rmdir(path);
}

static void test_renameat_inplace_rw(struct voluta_t_ctx *t_ctx)
{
	int dfd, fd;
	size_t i, nwr, nrd, count = VOLUTA_NAME_MAX;
	void *buf1, *buf2;
	const char *name1;
	const char *name2;
	const size_t bsz = 64 * UKILO;
	const char ch = 'B';
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
	buf2 = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_mkdir(path, 0700);
	voluta_t_open(path, O_DIRECTORY | O_RDONLY, 0, &dfd);

	name1 = make_name(t_ctx, 1, ch);
	voluta_t_openat(dfd, name1, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_write(fd, buf1, bsz, &nwr);
	voluta_t_close(fd);

	count = sizeof(name1) - 1;
	for (i = 2; i <= count; ++i) {
		name2 = make_name(t_ctx, i, ch);
		voluta_t_renameat(dfd, name1, dfd, name2);
		voluta_t_openat(dfd, name2, O_RDONLY, 0600, &fd);
		voluta_t_read(fd, buf2, bsz, &nrd);
		voluta_t_close(fd);
		voluta_t_expect_eqm(buf1, buf2, bsz);
		name1 = dup_name(t_ctx, name2);
	}
	count = sizeof(name1) - 1;
	for (i = count; i > 0; --i) {
		name2 = make_name(t_ctx, i, ch);
		voluta_t_renameat(dfd, name1, dfd, name2);
		voluta_t_openat(dfd, name2, O_RDONLY, 0600, &fd);
		voluta_t_read(fd, buf2, bsz, &nrd);
		voluta_t_close(fd);
		voluta_t_expect_eqm(buf1, buf2, bsz);
		name1 = dup_name(t_ctx, name2);
	}
	voluta_t_unlinkat(dfd, name1, 0);
	voluta_t_close(dfd);
	voluta_t_rmdir(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test renameat(2) from one directory to other with different names length
 */
static void test_renameat_move(struct voluta_t_ctx *t_ctx)
{
	int dfd1, dfd2, fd;
	size_t i, count = VOLUTA_NAME_MAX;
	struct stat st;
	const char *name1;
	const char *name2;
	const char ch = 'C';
	const char *path1 = voluta_t_new_path_unique(t_ctx);
	const char *path2 = voluta_t_new_path_unique(t_ctx);

	voluta_t_mkdir(path1, 0700);
	voluta_t_mkdir(path2, 0700);

	voluta_t_open(path1, O_DIRECTORY | O_RDONLY, 0, &dfd1);
	voluta_t_open(path2, O_DIRECTORY | O_RDONLY, 0, &dfd2);

	name1 = make_name(t_ctx, 1, ch);
	voluta_t_openat(dfd1, name1, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_close(fd);

	for (i = 2; i <= count; ++i) {
		voluta_t_fstatat(dfd1, name1, &st, 0);
		name2 = make_name(t_ctx, i, ch);
		voluta_t_renameat(dfd1, name1, dfd2, name2);
		voluta_t_fstatat_err(dfd1, name1, 0, -ENOENT);
		voluta_t_fstatat(dfd2, name2, &st, 0);

		name1 = make_name(t_ctx, i, ch);
		voluta_t_renameat(dfd2, name2, dfd1, name1);
		voluta_t_fstatat(dfd1, name1, &st, 0);
		voluta_t_fstatat_err(dfd2, name2, 0, -ENOENT);
	}
	voluta_t_unlinkat(dfd1, name1, 0);
	voluta_t_close(dfd1);
	voluta_t_close(dfd2);
	voluta_t_rmdir(path1);
	voluta_t_rmdir(path2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test renameat2(2) with RENAME_EXCHANGE flag
 */
static void test_renameat_exchange(struct voluta_t_ctx *t_ctx)
{
	int dfd1, dfd2, fd1, fd2;
	ino_t ino1, ino2;
	struct stat dst1, dst2, st1, st2;
	const char *name1 = voluta_t_new_name_unique(t_ctx);
	const char *name2 = voluta_t_new_name_unique(t_ctx);
	const char *path1 = voluta_t_new_path_unique(t_ctx);
	const char *path2 = voluta_t_new_path_unique(t_ctx);

	voluta_t_mkdir(path1, 0700);
	voluta_t_mkdir(path2, 0700);

	voluta_t_open(path1, O_DIRECTORY | O_RDONLY, 0, &dfd1);
	voluta_t_open(path2, O_DIRECTORY | O_RDONLY, 0, &dfd2);
	voluta_t_openat(dfd1, name1, O_CREAT | O_RDWR, 0600, &fd1);
	voluta_t_openat(dfd2, name2, O_CREAT | O_RDWR, 0600, &fd2);
	voluta_t_fstat(dfd1, &dst1);
	voluta_t_fstat(dfd2, &dst2);
	voluta_t_fstat(fd1, &st1);
	voluta_t_fstat(fd2, &st2);
	voluta_t_expect_eq(st1.st_nlink, 1);
	voluta_t_expect_eq(st2.st_nlink, 1);
	ino1 = st1.st_ino;
	ino2 = st2.st_ino;

	voluta_t_renameat2(dfd1, name1, dfd2, name2, RENAME_EXCHANGE);
	voluta_t_fstat(dfd1, &dst1);
	voluta_t_fstat(dfd2, &dst2);
	voluta_t_openat_err(dfd1, name2, O_RDONLY, 0600, -ENOENT);
	voluta_t_openat_err(dfd2, name1, O_RDONLY, 0600, -ENOENT);
	voluta_t_close(fd1);
	voluta_t_close(fd2);
	voluta_t_openat(dfd1, name1, O_RDONLY, 0600, &fd2);
	voluta_t_openat(dfd2, name2, O_RDONLY, 0600, &fd1);
	voluta_t_fstat(fd1, &st1);
	voluta_t_fstat(fd2, &st2);
	voluta_t_expect_eq(st1.st_nlink, 1);
	voluta_t_expect_eq(st2.st_nlink, 1);
	voluta_t_expect_eq(st1.st_ino, ino1);
	voluta_t_expect_eq(st2.st_ino, ino2);
	voluta_t_unlinkat(dfd1, name1, 0);
	voluta_t_unlinkat(dfd2, name2, 0);

	voluta_t_close(fd1);
	voluta_t_close(fd2);
	voluta_t_close(dfd1);
	voluta_t_close(dfd2);
	voluta_t_rmdir(path1);
	voluta_t_rmdir(path2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test renameat2(2) back and forth within same dir
 */
static char *make_lname(struct voluta_t_ctx *t_ctx,
			const char *prefix, size_t idx)
{
	char name[VOLUTA_NAME_MAX + 1] = "";

	snprintf(name, sizeof(name) - 1, "%s-%08lu", prefix, idx);
	return voluta_t_strdup(t_ctx, name);
}

static void test_renameat_samedir(struct voluta_t_ctx *t_ctx)
{
	int dfd, fd;
	size_t i, cnt = VOLUTA_LINK_MAX / 3;
	ino_t ino;
	struct stat st;
	const char *path = voluta_t_new_path_unique(t_ctx);
	const char *fname = voluta_t_new_name_unique(t_ctx);
	char p1[] = "1", p2[] = "2";
	char *lname1, *lname2;

	voluta_t_mkdir(path, 0700);
	voluta_t_open(path, O_DIRECTORY | O_RDONLY, 0, &dfd);
	voluta_t_openat(dfd, fname, O_CREAT | O_RDWR, 060, &fd);
	voluta_t_fstat(fd, &st);
	ino = st.st_ino;

	for (i = 1; i <= cnt; ++i) {
		lname1 = make_lname(t_ctx, p1, i);
		voluta_t_linkat(dfd, fname, dfd, lname1, 0);
		voluta_t_fstatat(dfd, lname1, &st, 0);
		voluta_t_expect_eq(st.st_ino, ino);
	}
	for (i = 1; i <= cnt; ++i) {
		lname1 = make_lname(t_ctx, p1, i);
		lname2 = make_lname(t_ctx, p2, i);
		voluta_t_renameat2(dfd, lname1, dfd, lname2, 0);
		voluta_t_fstatat(dfd, lname2, &st, 0);
		voluta_t_expect_eq(st.st_ino, ino);
	}
	for (i = 2; i <= cnt; i += 2) {
		lname1 = make_lname(t_ctx, p1, i);
		lname2 = make_lname(t_ctx, p2, i);
		voluta_t_renameat2(dfd, lname2, dfd, lname1, 0);
		voluta_t_fstatat(dfd, lname1, &st, 0);
		voluta_t_expect_eq(st.st_ino, ino);
		voluta_t_fstatat_err(dfd, lname2, 0, -ENOENT);
	}
	for (i = 1; i <= cnt; i += 2) {
		lname1 = make_lname(t_ctx, p1, i);
		lname2 = make_lname(t_ctx, p2, i);
		voluta_t_renameat2(dfd, lname2, dfd, lname1, 0);
		voluta_t_fstatat(dfd, lname1, &st, 0);
		voluta_t_expect_eq(st.st_ino, ino);
		voluta_t_fstatat_err(dfd, lname2, 0, -ENOENT);
	}
	for (i = 1; i <= cnt; ++i) {
		lname1 = make_lname(t_ctx, p1, i);
		voluta_t_unlinkat(dfd, lname1, 0);
		voluta_t_fstatat_err(dfd, lname1, 0, -ENOENT);
	}

	voluta_t_unlinkat(dfd, fname, 0);
	voluta_t_close(fd);
	voluta_t_close(dfd);
	voluta_t_rmdir(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_rename_simple),
	VOLUTA_T_DEFTEST(test_rename_ctime),
	VOLUTA_T_DEFTEST(test_rename_notdirto),
	VOLUTA_T_DEFTEST(test_rename_isdirto),
	VOLUTA_T_DEFTEST(test_rename_symlink),
	VOLUTA_T_DEFTEST(test_rename_nlink),
	VOLUTA_T_DEFTEST(test_rename_child),
	VOLUTA_T_DEFTEST(test_rename_replace),
	VOLUTA_T_DEFTEST(test_rename_move),
	VOLUTA_T_DEFTEST(test_rename_override),
	VOLUTA_T_DEFTEST(test_renameat_inplace),
	VOLUTA_T_DEFTEST(test_renameat_inplace_rw),
	VOLUTA_T_DEFTEST(test_renameat_move),
	VOLUTA_T_DEFTEST(test_renameat_exchange),
	VOLUTA_T_DEFTEST(test_renameat_samedir),
};

const struct voluta_t_tests
voluta_t_test_rename = VOLUTA_T_DEFTESTS(t_local_tests);
