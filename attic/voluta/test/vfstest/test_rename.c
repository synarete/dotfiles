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
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "vfstest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects rename(3p) to successfully change file's new-name and return ENOENT
 * on old-name.
 */
static void test_rename_simple(struct vt_env *vt_env)
{
	int fd;
	ino_t ino;
	mode_t ifmt = S_IFMT;
	struct stat st1, st2;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_under(vt_env, path0);
	const char *path2 = vt_new_path_under(vt_env, path0);

	vt_mkdir(path0, 0755);
	vt_creat(path1, 0644, &fd);
	vt_stat(path1, &st1);
	vt_expect_true(S_ISREG(st1.st_mode));
	vt_expect_eq((st1.st_mode & ~ifmt), 0644);
	vt_expect_eq(st1.st_nlink, 1);

	ino = st1.st_ino;
	vt_rename(path1, path2);
	vt_stat_err(path1, -ENOENT);
	vt_fstat(fd, &st1);
	vt_expect_eq(st1.st_ino, ino);
	vt_stat(path2, &st2);
	vt_expect_true(S_ISREG(st2.st_mode));
	vt_expect_eq((st2.st_mode & ~ifmt), 0644);
	vt_expect_eq(st2.st_nlink, 1);

	vt_unlink(path2);
	vt_rmdir(path0);
	vt_close(fd);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects rename(3p) to update ctime only when successful.
 */
static void test_rename_ctime(struct vt_env *vt_env)
{
	int fd;
	struct stat st0, st1;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_unique(vt_env);
	const char *path2 = vt_new_path_unique(vt_env);
	const char *path3 = vt_new_path_under(vt_env, path2);

	vt_creat(path0, 0644, &fd);
	vt_close(fd);
	vt_stat(path0, &st0);
	vt_suspends(vt_env, 2);
	vt_rename(path0, path1);
	vt_stat(path1, &st1);
	vt_expect_lt(st0.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	vt_unlink(path1);

	vt_mkdir(path0, 0700);
	vt_stat(path0, &st0);
	vt_suspends(vt_env, 2);
	vt_rename(path0, path1);
	vt_stat(path1, &st1);
	vt_expect_lt(st0.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	vt_rmdir(path1);

	vt_mkfifo(path0, 0644);
	vt_stat(path0, &st0);
	vt_suspends(vt_env, 2);
	vt_rename(path0, path1);
	vt_stat(path1, &st1);
	vt_expect_lt(st0.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	vt_unlink(path1);

	vt_symlink(path2, path0);
	vt_lstat(path0, &st0);
	vt_suspends(vt_env, 2);
	vt_rename(path0, path1);
	vt_lstat(path1, &st1);
	vt_expect_lt(st0.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	vt_unlink(path1);

	vt_creat(path0, 0644, &fd);
	vt_close(fd);
	vt_stat(path0, &st0);
	vt_suspends(vt_env, 2);
	vt_rename_err(path0, path3, -ENOENT);
	vt_stat(path0, &st1);
	vt_expect_eq(st0.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	vt_unlink(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects rename(3p) to returns ENOTDIR when the 'from' argument is a
 * directory, but 'to' is not a directory.
 */
static void test_rename_notdirto(struct vt_env *vt_env)
{
	int fd;
	struct stat st0, st1;
	char *path0, *path1;

	path0 = vt_new_path_unique(vt_env);
	path1 = vt_new_path_unique(vt_env);

	vt_mkdir(path0, 0750);
	vt_creat(path1, 0644, &fd);
	vt_close(fd);
	vt_rename_err(path0, path1, -ENOTDIR);
	vt_lstat(path0, &st0);
	vt_expect_true(S_ISDIR(st0.st_mode));
	vt_unlink(path1);

	vt_symlink("test-rename-notdirto", path1);
	vt_rename_err(path0, path1, -ENOTDIR);
	vt_lstat(path0, &st0);
	vt_expect_true(S_ISDIR(st0.st_mode));
	vt_lstat(path1, &st1);
	vt_expect_true(S_ISLNK(st1.st_mode));
	vt_unlink(path1);

	vt_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects rename(3p) to returns EISDIR when the 'to' argument is a
 * directory, but 'from' is not a directory.
 */
static void test_rename_isdirto(struct vt_env *vt_env)
{
	int fd;
	struct stat st0, st1;
	char *path0, *path1;

	path0 = vt_new_path_unique(vt_env);
	path1 = vt_new_path_unique(vt_env);

	vt_mkdir(path0, 0750);
	vt_creat(path1, 0640, &fd);
	vt_close(fd);
	vt_rename_err(path1, path0, -EISDIR);
	vt_lstat(path0, &st0);
	vt_expect_true(S_ISDIR(st0.st_mode));
	vt_unlink(path1);

	vt_mkfifo(path1, 0644);
	vt_rename_err(path1, path0, -EISDIR);
	vt_lstat(path0, &st0);
	vt_expect_true(S_ISDIR(st0.st_mode));
	vt_lstat(path1, &st1);
	vt_expect_true(S_ISFIFO(st1.st_mode));
	vt_unlink(path1);

	vt_symlink("test-rename-isdirto", path1);
	vt_rename_err(path1, path0, -EISDIR);
	vt_lstat(path0, &st0);
	vt_expect_true(S_ISDIR(st0.st_mode));
	vt_lstat(path1, &st1);
	vt_expect_lnk(st1.st_mode);
	vt_unlink(path1);

	vt_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test rename(3p) with symlink(3p)
 */
static void test_rename_symlink(struct vt_env *vt_env)
{
	int fd;
	void *buf;
	size_t nwr, bsz = VT_BK_SIZE;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_under(vt_env, path0);
	const char *path2 = vt_new_path_under(vt_env, path0);
	const char *path3 = vt_new_path_under(vt_env, path0);

	vt_mkdir(path0, 0755);
	vt_creat(path1, 0600, &fd);
	buf = vt_new_buf_rands(vt_env, bsz);
	vt_write(fd, buf, bsz, &nwr);
	vt_symlink(path1, path2);
	vt_rename(path2, path3);
	vt_close(fd);
	vt_unlink(path1);
	vt_unlink(path3);
	vt_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test rename(3p) to preserve proper nlink semantics
 */
static void test_rename_nlink(struct vt_env *vt_env)
{
	int fd;
	struct stat st;
	const char *path_x = vt_new_path_unique(vt_env);
	const char *path_a = vt_new_path_under(vt_env, path_x);
	const char *path_b = vt_new_path_under(vt_env, path_x);
	const char *path_c = vt_new_path_under(vt_env, path_x);
	const char *path_d = vt_new_path_under(vt_env, path_x);
	const char *path_ab = vt_new_path_under(vt_env, path_a);
	const char *path_abc = vt_new_path_under(vt_env, path_ab);
	const char *path_abcd = vt_new_path_under(vt_env, path_abc);

	vt_mkdir(path_x, 0700);
	vt_stat(path_x, &st);
	vt_expect_eq(st.st_nlink, 2);
	vt_mkdir(path_a, 0700);
	vt_mkdir(path_b, 0700);
	vt_mkdir(path_c, 0700);
	vt_stat(path_x, &st);
	vt_expect_eq(st.st_nlink, 5);
	vt_creat(path_d, 0600, &fd);
	vt_close(fd);
	vt_stat(path_x, &st);
	vt_expect_eq(st.st_nlink, 5);
	vt_stat(path_d, &st);
	vt_expect_eq(st.st_nlink, 1);

	vt_rename(path_b, path_ab);
	vt_stat_noent(path_b);
	vt_stat(path_x, &st);
	vt_expect_eq(st.st_nlink, 4);
	vt_stat(path_a, &st);
	vt_expect_eq(st.st_nlink, 3);
	vt_stat(path_ab, &st);
	vt_expect_eq(st.st_nlink, 2);

	vt_rename(path_c, path_abc);
	vt_stat_noent(path_c);
	vt_stat(path_x, &st);
	vt_expect_eq(st.st_nlink, 3);
	vt_stat(path_a, &st);
	vt_expect_eq(st.st_nlink, 3);
	vt_stat(path_ab, &st);
	vt_expect_eq(st.st_nlink, 3);
	vt_stat(path_abc, &st);
	vt_expect_eq(st.st_nlink, 2);

	vt_rename(path_d, path_abcd);
	vt_stat_noent(path_d);
	vt_stat(path_x, &st);
	vt_expect_eq(st.st_nlink, 3);
	vt_stat(path_a, &st);
	vt_expect_eq(st.st_nlink, 3);
	vt_stat(path_ab, &st);
	vt_expect_eq(st.st_nlink, 3);
	vt_stat(path_abc, &st);
	vt_expect_eq(st.st_nlink, 2);
	vt_stat(path_abcd, &st);
	vt_expect_eq(st.st_nlink, 1);

	vt_unlink(path_abcd);
	vt_rmdir(path_abc);
	vt_rmdir(path_ab);
	vt_rmdir(path_a);
	vt_rmdir(path_x);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test rename(3p) within same directory without implicit unlink, where parent
 * directory is already populated with sibling links.
 */
static void test_rename_child_(struct vt_env *vt_env, int nsibs)
{
	int fd, i;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_under(vt_env, path0);
	const char *path2 = vt_new_path_under(vt_env, path0);
	const char *path3 = NULL;

	vt_mkdir(path0, 0700);
	for (i = 0; i < nsibs; ++i) {
		path3 = vt_new_pathf(vt_env, path0, "%08x", i);
		vt_creat(path3, 0600, &fd);
		vt_close(fd);
	}
	vt_creat(path1, 0600, &fd);
	vt_close(fd);
	vt_rename(path1, path2);
	vt_unlink(path2);
	for (i = 0; i < nsibs; ++i) {
		path3 = vt_new_pathf(vt_env, path0, "%08x", i);
		vt_unlink(path3);
	}
	vt_rmdir(path0);
}

static void test_rename_child(struct vt_env *vt_env)
{
	test_rename_child_(vt_env, 16);
	test_rename_child_(vt_env, 1024);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test rename(3p) within same directory with implicit unlink of target sibling
 * file.
 */
static void test_rename_replace_(struct vt_env *vt_env, int nsibs)
{
	int fd, i;
	char *path0, *path1, *path2;

	path0 = vt_new_path_unique(vt_env);
	path2 = vt_new_path_under(vt_env, path0);

	vt_mkdir(path0, 0700);
	for (i = 0; i < nsibs; ++i) {
		path1 = vt_new_pathf(vt_env, path0, "%08x", i);
		vt_creat(path1, 0600, &fd);
		vt_close(fd);
	}
	vt_creat(path2, 0600, &fd);
	vt_close(fd);
	for (i = 0; i < nsibs; ++i) {
		path1 = vt_new_pathf(vt_env, path0, "%08x", i);
		vt_rename(path2, path1);
		path2 = path1;
	}
	vt_unlink(path2);
	vt_rmdir(path0);
}

static void test_rename_replace(struct vt_env *vt_env)
{
	test_rename_replace_(vt_env, 1);
	test_rename_replace_(vt_env, 2);
	test_rename_replace_(vt_env, 3);
	test_rename_replace_(vt_env, 1024);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test rename(3p) between two directories without implicit unlink of target.
 */
static void test_rename_move_(struct vt_env *vt_env, size_t cnt)
{
	int fd;
	size_t i;
	char *src_path1, *tgt_path1;
	const char *src_path0 = vt_new_path_unique(vt_env);
	const char *tgt_path0 = vt_new_path_unique(vt_env);

	vt_mkdir(src_path0, 0700);
	vt_mkdir(tgt_path0, 0700);
	for (i = 0; i < cnt; ++i) {
		src_path1 = vt_new_pathf(vt_env, src_path0, "s%08x", i);
		vt_creat(src_path1, 0600, &fd);
		vt_close(fd);
	}
	for (i = 0; i < cnt; ++i) {
		src_path1 = vt_new_pathf(vt_env, src_path0, "s%08x", i);
		tgt_path1 = vt_new_pathf(vt_env, tgt_path0, "t%08x", i);
		vt_rename(src_path1, tgt_path1);
	}
	for (i = 0; i < cnt; ++i) {
		tgt_path1 = vt_new_pathf(vt_env, tgt_path0, "t%08x", i);
		vt_unlink(tgt_path1);
	}
	vt_rmdir(src_path0);
	vt_rmdir(tgt_path0);
}

static void test_rename_move(struct vt_env *vt_env)
{
	test_rename_move_(vt_env, 1);
	test_rename_move_(vt_env, 2);
	test_rename_move_(vt_env, 3);
	test_rename_move_(vt_env, 1024);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test rename(3p) between two directories with implicit truncate-unlink of
 * target.
 */
static void test_rename_override_(struct vt_env *vt_env,
				  size_t cnt, size_t bsz)
{
	int fd;
	void *buf;
	size_t i;
	char *src_path1, *tgt_path1;
	const char *src_path0 = vt_new_path_unique(vt_env);
	const char *tgt_path0 = vt_new_path_unique(vt_env);

	buf = vt_new_buf_rands(vt_env, bsz);
	vt_mkdir(src_path0, 0700);
	vt_mkdir(tgt_path0, 0700);
	for (i = 0; i < cnt; ++i) {
		src_path1 = vt_new_pathf(vt_env, src_path0, "s%08x", i);
		tgt_path1 = vt_new_pathf(vt_env, tgt_path0, "t%08x", i);
		vt_creat(src_path1, 0600, &fd);
		vt_close(fd);
		vt_creat(tgt_path1, 0600, &fd);
		vt_pwriten(fd, buf, bsz, (loff_t)i);
		vt_close(fd);
	}
	for (i = 0; i < cnt; ++i) {
		src_path1 = vt_new_pathf(vt_env, src_path0, "s%08x", i);
		tgt_path1 = vt_new_pathf(vt_env, tgt_path0, "t%08x", i);
		vt_rename(src_path1, tgt_path1);
	}
	for (i = 0; i < cnt; ++i) {
		tgt_path1 = vt_new_pathf(vt_env, tgt_path0, "t%08x", i);
		vt_unlink(tgt_path1);
	}
	vt_rmdir(src_path0);
	vt_rmdir(tgt_path0);
}

static void test_rename_override(struct vt_env *vt_env)
{
	test_rename_override_(vt_env, 1, VT_BK_SIZE);
	test_rename_override_(vt_env, 3, VT_BK_SIZE);
	test_rename_override_(vt_env, 7, VT_UMEGA);
	test_rename_override_(vt_env, 1024, VT_BK_SIZE);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test renameat(2) within same directory with different names length
 */
static const char *make_name(struct vt_env *vt_env, size_t len, char ch)
{
	size_t nn;
	char str[VOLUTA_NAME_MAX + 1] = "";

	nn = (len < sizeof(str)) ? len : (sizeof(str) - 1);
	memset(str, ch, nn);
	str[nn] = '\0';

	return vt_strdup(vt_env, str);
}

static const char *dup_name(struct vt_env *vt_env, const char *str)
{
	return vt_strdup(vt_env, str);
}

static void test_renameat_inplace(struct vt_env *vt_env)
{
	int dfd, fd;
	size_t i, count = VOLUTA_NAME_MAX;
	const char *name1;
	const char *name2;
	const char ch = 'A';
	const char *path = vt_new_path_unique(vt_env);

	vt_mkdir(path, 0700);
	vt_open(path, O_DIRECTORY | O_RDONLY, 0, &dfd);

	name1 = make_name(vt_env, 1, ch);
	vt_openat(dfd, name1, O_CREAT | O_RDWR, 0600, &fd);
	vt_close(fd);

	for (i = 2; i <= count; ++i) {
		name2 = make_name(vt_env, i, ch);
		vt_renameat(dfd, name1, dfd, name2);
		name1 = dup_name(vt_env, name2);
	}
	count = sizeof(name1) - 1;
	for (i = count; i > 0; --i) {
		name2 = make_name(vt_env, i, ch);
		vt_renameat(dfd, name1, dfd, name2);
		name1 = dup_name(vt_env, name2);
	}
	vt_unlinkat(dfd, name1, 0);
	vt_close(dfd);
	vt_rmdir(path);
}

static void test_renameat_inplace_rw(struct vt_env *vt_env)
{
	int dfd, fd;
	size_t i, nwr, nrd, count = VOLUTA_NAME_MAX;
	void *buf1, *buf2;
	const char *name1;
	const char *name2;
	const size_t bsz = 64 * VT_UKILO;
	const char ch = 'B';
	const char *path = vt_new_path_unique(vt_env);

	buf1 = vt_new_buf_rands(vt_env, bsz);
	buf2 = vt_new_buf_rands(vt_env, bsz);
	vt_mkdir(path, 0700);
	vt_open(path, O_DIRECTORY | O_RDONLY, 0, &dfd);

	name1 = make_name(vt_env, 1, ch);
	vt_openat(dfd, name1, O_CREAT | O_RDWR, 0600, &fd);
	vt_write(fd, buf1, bsz, &nwr);
	vt_close(fd);

	count = sizeof(name1) - 1;
	for (i = 2; i <= count; ++i) {
		name2 = make_name(vt_env, i, ch);
		vt_renameat(dfd, name1, dfd, name2);
		vt_openat(dfd, name2, O_RDONLY, 0600, &fd);
		vt_read(fd, buf2, bsz, &nrd);
		vt_close(fd);
		vt_expect_eqm(buf1, buf2, bsz);
		name1 = dup_name(vt_env, name2);
	}
	count = sizeof(name1) - 1;
	for (i = count; i > 0; --i) {
		name2 = make_name(vt_env, i, ch);
		vt_renameat(dfd, name1, dfd, name2);
		vt_openat(dfd, name2, O_RDONLY, 0600, &fd);
		vt_read(fd, buf2, bsz, &nrd);
		vt_close(fd);
		vt_expect_eqm(buf1, buf2, bsz);
		name1 = dup_name(vt_env, name2);
	}
	vt_unlinkat(dfd, name1, 0);
	vt_close(dfd);
	vt_rmdir(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test renameat(2) from one directory to other with different names length
 */
static void test_renameat_move(struct vt_env *vt_env)
{
	int dfd1, dfd2, fd;
	size_t i, count = VOLUTA_NAME_MAX;
	struct stat st;
	const char *name1;
	const char *name2;
	const char ch = 'C';
	const char *path1 = vt_new_path_unique(vt_env);
	const char *path2 = vt_new_path_unique(vt_env);

	vt_mkdir(path1, 0700);
	vt_mkdir(path2, 0700);

	vt_open(path1, O_DIRECTORY | O_RDONLY, 0, &dfd1);
	vt_open(path2, O_DIRECTORY | O_RDONLY, 0, &dfd2);

	name1 = make_name(vt_env, 1, ch);
	vt_openat(dfd1, name1, O_CREAT | O_RDWR, 0600, &fd);
	vt_close(fd);

	for (i = 2; i <= count; ++i) {
		vt_fstatat(dfd1, name1, &st, 0);
		name2 = make_name(vt_env, i, ch);
		vt_renameat(dfd1, name1, dfd2, name2);
		vt_fstatat_err(dfd1, name1, 0, -ENOENT);
		vt_fstatat(dfd2, name2, &st, 0);

		name1 = make_name(vt_env, i, ch);
		vt_renameat(dfd2, name2, dfd1, name1);
		vt_fstatat(dfd1, name1, &st, 0);
		vt_fstatat_err(dfd2, name2, 0, -ENOENT);
	}
	vt_unlinkat(dfd1, name1, 0);
	vt_close(dfd1);
	vt_close(dfd2);
	vt_rmdir(path1);
	vt_rmdir(path2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test renameat2(2) with RENAME_EXCHANGE flag
 */
static void test_renameat_exchange(struct vt_env *vt_env)
{
	int dfd1, dfd2, fd1, fd2;
	ino_t ino1, ino2;
	struct stat dst1, dst2, st1, st2;
	const char *name1 = vt_new_name_unique(vt_env);
	const char *name2 = vt_new_name_unique(vt_env);
	const char *path1 = vt_new_path_unique(vt_env);
	const char *path2 = vt_new_path_unique(vt_env);

	vt_mkdir(path1, 0700);
	vt_mkdir(path2, 0700);

	vt_open(path1, O_DIRECTORY | O_RDONLY, 0, &dfd1);
	vt_open(path2, O_DIRECTORY | O_RDONLY, 0, &dfd2);
	vt_openat(dfd1, name1, O_CREAT | O_RDWR, 0600, &fd1);
	vt_openat(dfd2, name2, O_CREAT | O_RDWR, 0600, &fd2);
	vt_fstat(dfd1, &dst1);
	vt_fstat(dfd2, &dst2);
	vt_fstat(fd1, &st1);
	vt_fstat(fd2, &st2);
	vt_expect_eq(st1.st_nlink, 1);
	vt_expect_eq(st2.st_nlink, 1);
	ino1 = st1.st_ino;
	ino2 = st2.st_ino;

	vt_renameat2(dfd1, name1, dfd2, name2, RENAME_EXCHANGE);
	vt_fstat(dfd1, &dst1);
	vt_fstat(dfd2, &dst2);
	vt_openat_err(dfd1, name2, O_RDONLY, 0600, -ENOENT);
	vt_openat_err(dfd2, name1, O_RDONLY, 0600, -ENOENT);
	vt_close(fd1);
	vt_close(fd2);
	vt_openat(dfd1, name1, O_RDONLY, 0600, &fd2);
	vt_openat(dfd2, name2, O_RDONLY, 0600, &fd1);
	vt_fstat(fd1, &st1);
	vt_fstat(fd2, &st2);
	vt_expect_eq(st1.st_nlink, 1);
	vt_expect_eq(st2.st_nlink, 1);
	vt_expect_eq(st1.st_ino, ino1);
	vt_expect_eq(st2.st_ino, ino2);
	vt_unlinkat(dfd1, name1, 0);
	vt_unlinkat(dfd2, name2, 0);

	vt_close(fd1);
	vt_close(fd2);
	vt_close(dfd1);
	vt_close(dfd2);
	vt_rmdir(path1);
	vt_rmdir(path2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test renameat2(2) back and forth within same dir
 */
static char *make_lname(struct vt_env *vt_env,
			const char *prefix, size_t idx)
{
	char name[VOLUTA_NAME_MAX + 1] = "";

	snprintf(name, sizeof(name) - 1, "%s-%08lu", prefix, idx);
	return vt_strdup(vt_env, name);
}

static void test_renameat_samedir(struct vt_env *vt_env)
{
	int dfd, fd;
	size_t i, cnt = VOLUTA_LINK_MAX / 3;
	ino_t ino;
	struct stat st;
	const char *path = vt_new_path_unique(vt_env);
	const char *fname = vt_new_name_unique(vt_env);
	char p1[] = "1", p2[] = "2";
	char *lname1, *lname2;

	vt_mkdir(path, 0700);
	vt_open(path, O_DIRECTORY | O_RDONLY, 0, &dfd);
	vt_openat(dfd, fname, O_CREAT | O_RDWR, 060, &fd);
	vt_fstat(fd, &st);
	ino = st.st_ino;

	for (i = 1; i <= cnt; ++i) {
		lname1 = make_lname(vt_env, p1, i);
		vt_linkat(dfd, fname, dfd, lname1, 0);
		vt_fstatat(dfd, lname1, &st, 0);
		vt_expect_eq(st.st_ino, ino);
	}
	for (i = 1; i <= cnt; ++i) {
		lname1 = make_lname(vt_env, p1, i);
		lname2 = make_lname(vt_env, p2, i);
		vt_renameat2(dfd, lname1, dfd, lname2, 0);
		vt_fstatat(dfd, lname2, &st, 0);
		vt_expect_eq(st.st_ino, ino);
	}
	for (i = 2; i <= cnt; i += 2) {
		lname1 = make_lname(vt_env, p1, i);
		lname2 = make_lname(vt_env, p2, i);
		vt_renameat2(dfd, lname2, dfd, lname1, 0);
		vt_fstatat(dfd, lname1, &st, 0);
		vt_expect_eq(st.st_ino, ino);
		vt_fstatat_err(dfd, lname2, 0, -ENOENT);
	}
	for (i = 1; i <= cnt; i += 2) {
		lname1 = make_lname(vt_env, p1, i);
		lname2 = make_lname(vt_env, p2, i);
		vt_renameat2(dfd, lname2, dfd, lname1, 0);
		vt_fstatat(dfd, lname1, &st, 0);
		vt_expect_eq(st.st_ino, ino);
		vt_fstatat_err(dfd, lname2, 0, -ENOENT);
	}
	for (i = 1; i <= cnt; ++i) {
		lname1 = make_lname(vt_env, p1, i);
		vt_unlinkat(dfd, lname1, 0);
		vt_fstatat_err(dfd, lname1, 0, -ENOENT);
	}

	vt_unlinkat(dfd, fname, 0);
	vt_close(fd);
	vt_close(dfd);
	vt_rmdir(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTEST(test_rename_simple),
	VT_DEFTEST(test_rename_ctime),
	VT_DEFTEST(test_rename_notdirto),
	VT_DEFTEST(test_rename_isdirto),
	VT_DEFTEST(test_rename_symlink),
	VT_DEFTEST(test_rename_nlink),
	VT_DEFTEST(test_rename_child),
	VT_DEFTEST(test_rename_replace),
	VT_DEFTEST(test_rename_move),
	VT_DEFTEST(test_rename_override),
	VT_DEFTEST(test_renameat_inplace),
	VT_DEFTEST(test_renameat_inplace_rw),
	VT_DEFTEST(test_renameat_move),
	VT_DEFTEST(test_renameat_exchange),
	VT_DEFTEST(test_renameat_samedir),
};

const struct vt_tests vt_test_rename = VT_DEFTESTS(vt_local_tests);
