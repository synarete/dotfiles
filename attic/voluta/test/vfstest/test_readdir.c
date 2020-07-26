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
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include "vfstest.h"


static int isdot(const struct dirent64 *dent)
{
	return (strcmp(".", dent->d_name) == 0);
}

static int isdotdot(const struct dirent64 *dent)
{
	return (strcmp("..", dent->d_name) == 0);
}

static int is_dot_or_dotdot(const struct dirent64 *dent)
{
	return isdot(dent) || isdotdot(dent);
}

static mode_t dirent_gettype(const struct dirent64 *dent)
{
	const mode_t d_type = (mode_t)dent->d_type;

	return DTTOIF(d_type);
}

static int dirent_isdir(const struct dirent64 *dent)
{
	const mode_t mode = dirent_gettype(dent);

	return S_ISDIR(mode);
}

static int dirent_isreg(const struct dirent64 *dent)
{
	const mode_t mode = dirent_gettype(dent);

	return S_ISREG(mode);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct vt_getdents_ctx {
	char buf[VT_UMEGA / 4];
	struct dirent64 dents[1024];
	size_t ndents;
};

static struct vt_getdents_ctx *
new_getdents_ctx(struct vt_env *vt_env)
{
	struct vt_getdents_ctx *getdents_ctx;

	getdents_ctx = vt_new_buf_zeros(vt_env, sizeof(*getdents_ctx));
	return getdents_ctx;
}

static void verify_getdents_ctx(struct vt_env *vt_env,
				struct vt_getdents_ctx *getdents_ctx)
{
	loff_t off_curr;
	loff_t off_prev = -1;
	const struct dirent64 *dent;

	for (size_t i = 0; i < getdents_ctx->ndents; ++i) {
		dent = &getdents_ctx->dents[i];
		off_curr = dent->d_off;
		if (off_curr == -1) {
			vt_expect_eq(i + 1, getdents_ctx->ndents);
		} else {
			vt_expect_gt(off_curr, off_prev);
		}
		off_prev = dent->d_off;
	}
	voluta_unused(vt_env);
}

static void vt_getdents2(int fd,
			 struct vt_getdents_ctx *getdents_ctx)
{
	size_t ndents = 0;
	const size_t ndents_max = VT_ARRAY_SIZE(getdents_ctx->dents);

	vt_getdents(fd, getdents_ctx->buf, sizeof(getdents_ctx->buf),
		    getdents_ctx->dents, ndents_max, &ndents);
	vt_expect_le(ndents, ndents_max);
	getdents_ctx->ndents = ndents;
}

static void vt_getdents_from(struct vt_env *vt_env,
			     int fd, loff_t off,
			     struct vt_getdents_ctx *getdents_ctx)
{
	loff_t pos = -1;

	vt_llseek(fd, off, SEEK_SET, &pos);
	vt_expect_eq(off, pos);
	vt_getdents2(fd, getdents_ctx);
	verify_getdents_ctx(vt_env, getdents_ctx);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects Linux getdents(2) to read all dir-entries.
 */
static void test_readdir_basic_(struct vt_env *vt_env, size_t lim)
{
	int fd;
	int dfd;
	loff_t pos;
	loff_t off = 0;
	size_t cnt = 0;
	size_t itr = 0;
	struct stat st;
	struct dirent64 dent = { .d_ino = 0 };
	const char *path1 = NULL;
	const char *path0 = vt_new_path_unique(vt_env);

	vt_mkdir(path0, 0755);
	vt_open(path0, O_DIRECTORY | O_RDONLY, 0, &dfd);
	for (size_t i = 0; i < lim; ++i) {
		path1 = vt_new_pathf(vt_env, path0, "%08x", i);
		vt_creat(path1, 0600, &fd);
		vt_close(fd);
		vt_fstat(dfd, &st);
		vt_expect_ge(st.st_size, i + 1);
	}
	while (cnt < lim) {
		itr += 1;
		vt_expect_lt(itr, 10 * lim);

		vt_llseek(dfd, off, SEEK_SET, &pos);
		vt_expect_eq(off, pos);
		vt_getdent(dfd, &dent);
		off = dent.d_off;
		if (is_dot_or_dotdot(&dent)) {
			continue;
		}
		vt_expect_true(dirent_isreg(&dent));
		cnt++;
	}
	for (size_t j = 0; j < lim; ++j) {
		vt_fstat(dfd, &st);
		vt_expect_ge(st.st_size, lim - j);
		path1 = vt_new_pathf(vt_env, path0, "%08x", j);
		vt_stat(path1, &st);
		vt_unlink(path1);
		vt_stat_noent(path1);
	}
	vt_close(dfd);
	vt_rmdir(path0);
}

static void test_readdir_basic(struct vt_env *vt_env)
{
	test_readdir_basic_(vt_env, 1);
	test_readdir_basic_(vt_env, 2);
	test_readdir_basic_(vt_env, 4);
	test_readdir_basic_(vt_env, 32);
	test_readdir_basic_(vt_env, 64);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects getdents(2) to read all dir-entries while unlinking.
 */
static void test_readdir_unlink_(struct vt_env *vt_env, size_t lim)
{
	int fd;
	int dfd;
	loff_t pos;
	loff_t off = 0;
	size_t cnt = 0;
	struct stat st;
	struct dirent64 dent;
	const char *path1;
	const char *path0 = vt_new_path_unique(vt_env);

	vt_mkdir(path0, 0700);
	vt_open(path0, O_DIRECTORY | O_RDONLY, 0, &dfd);
	for (size_t i = 0; i < lim; ++i) {
		path1 = vt_new_path_under(vt_env, path0);
		vt_creat(path1, 0600, &fd);
		vt_close(fd);
		vt_fstat(dfd, &st);
		vt_expect_ge(st.st_size, i + 1);
	}
	while (cnt < lim) {
		vt_expect_lt(cnt, (2 * lim));

		vt_llseek(dfd, off, SEEK_SET, &pos);
		vt_expect_eq(off, pos);
		vt_getdent(dfd, &dent);
		if (!strlen(dent.d_name)) {
			break;
		}
		if (is_dot_or_dotdot(&dent)) {
			off = dent.d_off;
			continue;
		}
		vt_expect_true(dirent_isreg(&dent));
		vt_expect_false(dirent_isdir(&dent));

		path1 = vt_new_path_nested(vt_env, path0, dent.d_name);
		vt_stat(path1, &st);
		vt_unlink(path1);
		vt_stat_noent(path1);
		off = 2;
		cnt++;
	}
	vt_close(dfd);
	vt_rmdir(path0);
}

static void test_readdir_unlink(struct vt_env *vt_env)
{
	test_readdir_unlink_(vt_env, 4);
}

static void test_readdir_unlink_big(struct vt_env *vt_env)
{
	test_readdir_unlink_(vt_env, 128);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects Linux getdents(2) to read all dir-entries of large dir. Read single
 * dentry at a time.
 */
static const char *make_iname(struct vt_env *vt_env,
			      const char *path,
			      const char *name_prefix, size_t idx)
{
	return vt_new_pathf(vt_env, path, "%s-%08lx", name_prefix, idx);
}

static void test_readdir_getdents(struct vt_env *vt_env, size_t lim)
{
	int fd;
	int dfd;
	int cmp;
	loff_t pos;
	loff_t off = 0;
	size_t nde;
	size_t cnt = 0;
	struct stat st;
	struct dirent64 dents[8];
	const struct dirent64 *dent;
	char buf[1024];
	const size_t bsz = sizeof(buf);
	const size_t ndents = VT_ARRAY_SIZE(dents);
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = NULL;
	const char *prefix = vt_new_name_unique(vt_env);

	vt_mkdir(path0, 0755);
	vt_open(path0, O_DIRECTORY | O_RDONLY, 0, &dfd);
	for (size_t i = 0; i < lim; ++i) {
		path1 = make_iname(vt_env, path0, prefix, i);
		vt_creat(path1, 0600, &fd);
		vt_close(fd);
		vt_fstat(dfd, &st);
		vt_expect_ge(st.st_size, i + 1);
	}
	while (cnt < lim) {
		vt_llseek(dfd, off, SEEK_SET, &pos);
		vt_expect_eq(off, pos);

		vt_getdents(dfd, buf, bsz, dents, ndents, &nde);
		for (size_t j = 0; j < nde; ++j) {
			dent = &dents[j];
			off = dent->d_off;
			if (is_dot_or_dotdot(dent)) {
				continue;
			}
			vt_expect_true(dirent_isreg(dent));
			cmp = strncmp(dent->d_name, prefix, strlen(prefix));
			vt_expect_eq(cmp, 0);
			cnt++;
		}
	}
	for (size_t k = 0; k < lim; ++k) {
		path1 = make_iname(vt_env, path0, prefix, k);
		vt_stat(path1, &st);
		vt_unlink(path1);
		vt_stat_noent(path1);
		vt_fstat(dfd, &st);
		vt_expect_ge(st.st_size, lim - (k + 1));
	}
	vt_close(dfd);
	vt_rmdir(path0);
}

static void test_readdir_small(struct vt_env *vt_env)
{
	test_readdir_getdents(vt_env, 16);
}

static void test_readdir_normal(struct vt_env *vt_env)
{
	test_readdir_getdents(vt_env, 128);
}

static void test_readdir_big(struct vt_env *vt_env)
{
	test_readdir_getdents(vt_env, 8192);
}

static void test_readdir_large(struct vt_env *vt_env)
{
	test_readdir_getdents(vt_env, 32768);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests getdents(2) system call with multiple dir-entries at a time.
 */
static void test_readdir_counted_(struct vt_env *vt_env, size_t lim)
{
	int dfd;
	size_t cnt = 0;
	loff_t off = 0;
	const struct dirent64 *dent;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = NULL;
	const char *name = NULL;
	struct vt_getdents_ctx *getdents_ctx = new_getdents_ctx(vt_env);

	vt_mkdir(path0, 0700);
	for (size_t diri = 0; diri < lim; ++diri) {
		path1 = vt_new_pathf(vt_env, path0, "%04lx", diri);
		vt_mkdir(path1, 0700);
	}
	vt_open(path0, O_DIRECTORY | O_RDONLY, 0, &dfd);
	while (cnt < lim) {
		vt_getdents_from(vt_env, dfd, off, getdents_ctx);
		vt_expect_gt(getdents_ctx->ndents, 0);
		for (size_t i = 0; i < getdents_ctx->ndents; ++i) {
			dent = &getdents_ctx->dents[i];
			off = dent->d_off;
			vt_expect_true(dirent_isdir(dent));
			if (is_dot_or_dotdot(dent)) {
				continue;
			}
			cnt++;
		}
	}
	cnt = 0;
	while (cnt < lim) {
		vt_getdents_from(vt_env, dfd, 0, getdents_ctx);
		vt_expect_gt(getdents_ctx->ndents, 0);
		for (size_t j = 0; j < getdents_ctx->ndents; ++j) {
			dent = &getdents_ctx->dents[j];
			vt_expect_true(dirent_isdir(dent));
			if (is_dot_or_dotdot(dent)) {
				continue;
			}
			name = dent->d_name;
			path1 = vt_new_path_nested(vt_env, path0, name);
			vt_rmdir(path1);
			cnt++;
		}
	}
	vt_close(dfd);
	vt_rmdir(path0);
}

static void test_readdir_counted(struct vt_env *vt_env)
{
	test_readdir_counted_(vt_env, 64);
	test_readdir_counted_(vt_env, 1024);
}

static void test_readdir_counted_big(struct vt_env *vt_env)
{
	test_readdir_counted_(vt_env, 16 * 1024);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void fill_name(char *name, size_t lim, size_t idx)
{
	snprintf(name, lim, "%061lx", idx);
}

static void test_readdir_unlinkat_(struct vt_env *vt_env, size_t lim)
{
	int fd;
	int dfd1;
	int dfd2;
	loff_t doff;
	size_t cnt = 0;
	size_t itr = 0;
	struct stat st;
	char name[VOLUTA_NAME_MAX + 1] = "";
	const struct dirent64 *dent;
	const char *path1 = vt_new_path_unique(vt_env);
	const char *path2 = vt_new_path_unique(vt_env);
	const char *fname = vt_new_name_unique(vt_env);
	struct vt_getdents_ctx *getdents_ctx = new_getdents_ctx(vt_env);

	vt_mkdir(path1, 0700);
	vt_open(path1, O_DIRECTORY | O_RDONLY, 0, &dfd1);
	vt_mkdir(path2, 0700);
	vt_open(path2, O_DIRECTORY | O_RDONLY, 0, &dfd2);
	vt_openat(dfd2, fname, O_CREAT | O_RDWR, 0600, &fd);
	for (size_t i = 0; i < lim; ++i) {
		fill_name(name, sizeof(name) - 1, i + 1);
		vt_linkat(dfd2, fname, dfd1, name, 0);
		vt_fstat(dfd1, &st);
		vt_expect_ge(st.st_size, i + 1);
	}
	for (size_t i = 0; i < lim; ++i) {
		fill_name(name, sizeof(name) - 1, i + 1);
		vt_linkat_err(dfd2, fname, dfd1, name, 0, -EEXIST);
	}
	while (cnt < lim) {
		vt_fstat(dfd1, &st);
		vt_expect_gt(st.st_size, 0);
		doff = st.st_size / 2;
		vt_getdents_from(vt_env, dfd1, doff, getdents_ctx);
		if (getdents_ctx->ndents == 0) {
			vt_getdents_from(vt_env, dfd1, 2, getdents_ctx);
			vt_expect_gt(getdents_ctx->ndents, 0);
		}
		for (size_t j = 0; j < getdents_ctx->ndents; ++j) {
			dent = &getdents_ctx->dents[j];
			if (is_dot_or_dotdot(dent)) {
				continue;
			}
			vt_expect_true(dirent_isreg(dent));
			vt_unlinkat(dfd1, dent->d_name, 0);
			cnt++;
		}
		vt_expect_lt(itr, lim);
		itr++;
	}
	vt_close(fd);
	vt_close(dfd1);
	vt_rmdir(path1);
	vt_unlinkat(dfd2, fname, 0);
	vt_close(dfd2);
	vt_rmdir(path2);
}

static void test_readdir_unlinkat(struct vt_env *vt_env)
{
	test_readdir_unlinkat_(vt_env, 8);
	test_readdir_unlinkat_(vt_env, 64);
	test_readdir_unlinkat_(vt_env, 512);
}

static void test_readdir_unlinkat_big(struct vt_env *vt_env)
{
	test_readdir_unlinkat_(vt_env, 8192);
}

static void test_readdir_unlinkat_large(struct vt_env *vt_env)
{
	test_readdir_unlinkat_(vt_env, VOLUTA_LINK_MAX - 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTEST(test_readdir_basic),
	VT_DEFTEST(test_readdir_unlink),
	VT_DEFTEST(test_readdir_unlink_big),
	VT_DEFTEST(test_readdir_small),
	VT_DEFTEST(test_readdir_normal),
	VT_DEFTEST(test_readdir_big),
	VT_DEFTEST(test_readdir_large),
	VT_DEFTEST(test_readdir_counted),
	VT_DEFTEST(test_readdir_counted_big),
	VT_DEFTEST(test_readdir_unlinkat),
	VT_DEFTEST(test_readdir_unlinkat_big),
	VT_DEFTEST(test_readdir_unlinkat_large),
};

const struct vt_tests vt_test_readdir = VT_DEFTESTS(vt_local_tests);
