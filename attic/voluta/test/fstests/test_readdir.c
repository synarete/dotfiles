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
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include "fstests.h"


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

struct voluta_t_getdents_ctx {
	char buf[VOLUTA_MEGA / 4];
	struct dirent64 dents[1024];
	size_t ndents;
};

static struct voluta_t_getdents_ctx *
new_getdents_ctx(struct voluta_t_ctx *t_ctx)
{
	struct voluta_t_getdents_ctx *getdents_ctx;

	getdents_ctx = voluta_t_new_buf_zeros(t_ctx, sizeof(*getdents_ctx));
	return getdents_ctx;
}

static void verify_getdents_ctx(struct voluta_t_ctx *t_ctx,
				struct voluta_t_getdents_ctx *getdents_ctx)
{
	loff_t off_curr, off_prev = -1;
	const struct dirent64 *dent;

	for (size_t i = 0; i < getdents_ctx->ndents; ++i) {
		dent = &getdents_ctx->dents[i];
		off_curr = dent->d_off;
		if (off_curr == -1) {
			voluta_t_expect_eq(i + 1, getdents_ctx->ndents);
		} else {
			voluta_t_expect_gt(off_curr, off_prev);
		}
		off_prev = dent->d_off;
	}
	voluta_unused(t_ctx);
}

static void voluta_t_getdents2(int fd,
			       struct voluta_t_getdents_ctx *getdents_ctx)
{
	size_t ndents = 0;
	const size_t ndents_max = VOLUTA_ARRAY_SIZE(getdents_ctx->dents);

	voluta_t_getdents(fd, getdents_ctx->buf, sizeof(getdents_ctx->buf),
			  getdents_ctx->dents, ndents_max, &ndents);
	voluta_t_expect_le(ndents, ndents_max);
	getdents_ctx->ndents = ndents;
}

static void voluta_t_getdents_from(struct voluta_t_ctx *t_ctx,
				   int fd, loff_t off,
				   struct voluta_t_getdents_ctx *getdents_ctx)
{
	loff_t pos = -1;

	voluta_t_llseek(fd, off, SEEK_SET, &pos);
	voluta_t_expect_eq(off, pos);
	voluta_t_getdents2(fd, getdents_ctx);
	verify_getdents_ctx(t_ctx, getdents_ctx);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects Linux getdents(2) to read all dir-entries.
 */
static void test_readdir_basic_(struct voluta_t_ctx *t_ctx, size_t lim)
{
	int dfd, fd;
	loff_t pos, off = 0;
	size_t cnt = 0, itr = 0;
	struct stat st;
	struct dirent64 dent = { .d_ino = 0 };
	const char *path1 = NULL;
	const char *path0 = voluta_t_new_path_unique(t_ctx);

	voluta_t_mkdir(path0, 0755);
	voluta_t_open(path0, O_DIRECTORY | O_RDONLY, 0, &dfd);
	for (size_t i = 0; i < lim; ++i) {
		path1 = voluta_t_new_pathf(t_ctx, path0, "%08x", i);
		voluta_t_creat(path1, 0600, &fd);
		voluta_t_close(fd);
		voluta_t_fstat(dfd, &st);
		voluta_t_expect_ge(st.st_size, i + 1);
	}
	while (cnt < lim) {
		itr += 1;
		voluta_t_expect_lt(itr, 10 * lim);

		voluta_t_llseek(dfd, off, SEEK_SET, &pos);
		voluta_t_expect_eq(off, pos);
		voluta_t_getdent(dfd, &dent);
		off = dent.d_off;
		if (is_dot_or_dotdot(&dent)) {
			continue;
		}
		voluta_t_expect_true(dirent_isreg(&dent));
		cnt++;
	}
	for (size_t j = 0; j < lim; ++j) {
		voluta_t_fstat(dfd, &st);
		voluta_t_expect_ge(st.st_size, lim - j);
		path1 = voluta_t_new_pathf(t_ctx, path0, "%08x", j);
		voluta_t_stat(path1, &st);
		voluta_t_unlink(path1);
		voluta_t_stat_noent(path1);
	}
	voluta_t_close(dfd);
	voluta_t_rmdir(path0);
}

static void test_readdir_basic(struct voluta_t_ctx *t_ctx)
{
	test_readdir_basic_(t_ctx, 1);
	test_readdir_basic_(t_ctx, 2);
	test_readdir_basic_(t_ctx, 4);
	test_readdir_basic_(t_ctx, 32);
	test_readdir_basic_(t_ctx, 64);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects getdents(2) to read all dir-entries while unlinking.
 */
static void test_readdir_unlink_(struct voluta_t_ctx *t_ctx, size_t lim)
{
	int dfd, fd;
	loff_t pos, off = 0;
	size_t cnt = 0;
	struct stat st;
	struct dirent64 dent;
	char *path1, *name;
	const char *path0 = voluta_t_new_path_unique(t_ctx);

	voluta_t_mkdir(path0, 0700);
	voluta_t_open(path0, O_DIRECTORY | O_RDONLY, 0, &dfd);
	for (size_t i = 0; i < lim; ++i) {
		path1 = voluta_t_new_path_under(t_ctx, path0);
		voluta_t_creat(path1, 0600, &fd);
		voluta_t_close(fd);
		voluta_t_fstat(dfd, &st);
		voluta_t_expect_ge(st.st_size, i + 1);
	}
	while (cnt < lim) {
		voluta_t_expect_lt(cnt, (2 * lim));

		voluta_t_llseek(dfd, off, SEEK_SET, &pos);
		voluta_t_expect_eq(off, pos);
		voluta_t_getdent(dfd, &dent);
		if (!strlen(dent.d_name)) {
			break;
		}
		if (is_dot_or_dotdot(&dent)) {
			off = dent.d_off;
			continue;
		}
		voluta_t_expect_true(dirent_isreg(&dent));
		voluta_t_expect_false(dirent_isdir(&dent));

		name = dent.d_name;
		path1 = voluta_t_new_path_nested(t_ctx, path0, name);
		voluta_t_stat(path1, &st);
		voluta_t_unlink(path1);
		voluta_t_stat_noent(path1);
		off = 2;
		cnt++;
	}
	voluta_t_close(dfd);
	voluta_t_rmdir(path0);
}

static void test_readdir_unlink(struct voluta_t_ctx *t_ctx)
{
	test_readdir_unlink_(t_ctx, 4);
}

static void test_readdir_unlink_big(struct voluta_t_ctx *t_ctx)
{
	test_readdir_unlink_(t_ctx, 128);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects Linux getdents(2) to read all dir-entries of large dir. Read single
 * dentry at a time.
 */
static const char *make_iname(struct voluta_t_ctx *t_ctx,
			      const char *path,
			      const char *name_prefix, size_t idx)
{
	return voluta_t_new_pathf(t_ctx, path, "%s-%08lx", name_prefix, idx);
}

static void test_readdir_getdents(struct voluta_t_ctx *t_ctx, size_t lim)
{
	int dfd, fd, cmp;
	loff_t pos, off = 0;
	size_t nde, cnt = 0;
	struct stat st;
	struct dirent64 dents[8];
	const struct dirent64 *dent;
	char buf[1024];
	const size_t bsz = sizeof(buf);
	const size_t ndents = VOLUTA_ARRAY_SIZE(dents);
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = NULL;
	const char *prefix = voluta_t_new_name_unique(t_ctx);

	voluta_t_mkdir(path0, 0755);
	voluta_t_open(path0, O_DIRECTORY | O_RDONLY, 0, &dfd);
	for (size_t i = 0; i < lim; ++i) {
		path1 = make_iname(t_ctx, path0, prefix, i);
		voluta_t_creat(path1, 0600, &fd);
		voluta_t_close(fd);
		voluta_t_fstat(dfd, &st);
		voluta_t_expect_ge(st.st_size, i + 1);
	}
	while (cnt < lim) {
		voluta_t_llseek(dfd, off, SEEK_SET, &pos);
		voluta_t_expect_eq(off, pos);

		voluta_t_getdents(dfd, buf, bsz, dents, ndents, &nde);
		for (size_t j = 0; j < nde; ++j) {
			dent = &dents[j];
			off = dent->d_off;
			if (is_dot_or_dotdot(dent)) {
				continue;
			}
			voluta_t_expect_true(dirent_isreg(dent));
			cmp = strncmp(dent->d_name, prefix, strlen(prefix));
			voluta_t_expect_eq(cmp, 0);
			cnt++;
		}
	}
	for (size_t k = 0; k < lim; ++k) {
		path1 = make_iname(t_ctx, path0, prefix, k);
		voluta_t_stat(path1, &st);
		voluta_t_unlink(path1);
		voluta_t_stat_noent(path1);
		voluta_t_fstat(dfd, &st);
		voluta_t_expect_ge(st.st_size, lim - (k + 1));
	}
	voluta_t_close(dfd);
	voluta_t_rmdir(path0);
}

static void test_readdir_small(struct voluta_t_ctx *t_ctx)
{
	test_readdir_getdents(t_ctx, 16);
}

static void test_readdir_normal(struct voluta_t_ctx *t_ctx)
{
	test_readdir_getdents(t_ctx, 128);
}

static void test_readdir_big(struct voluta_t_ctx *t_ctx)
{
	test_readdir_getdents(t_ctx, 8192);
}

static void test_readdir_large(struct voluta_t_ctx *t_ctx)
{
	test_readdir_getdents(t_ctx, 32768);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests getdents(2) system call with multiple dir-entries at a time.
 */
static void test_readdir_counted_(struct voluta_t_ctx *t_ctx, size_t lim)
{
	int dfd;
	size_t cnt = 0;
	loff_t off = 0;
	const struct dirent64 *dent;
	const char *path0 = voluta_t_new_path_unique(t_ctx);
	const char *path1 = NULL;
	const char *name = NULL;
	struct voluta_t_getdents_ctx *getdents_ctx = new_getdents_ctx(t_ctx);

	voluta_t_mkdir(path0, 0700);
	for (size_t diri = 0; diri < lim; ++diri) {
		path1 = voluta_t_new_pathf(t_ctx, path0, "%04lx", diri);
		voluta_t_mkdir(path1, 0700);
	}
	voluta_t_open(path0, O_DIRECTORY | O_RDONLY, 0, &dfd);
	while (cnt < lim) {
		voluta_t_getdents_from(t_ctx, dfd, off, getdents_ctx);
		voluta_t_expect_gt(getdents_ctx->ndents, 0);
		for (size_t i = 0; i < getdents_ctx->ndents; ++i) {
			dent = &getdents_ctx->dents[i];
			off = dent->d_off;
			voluta_t_expect_true(dirent_isdir(dent));
			if (is_dot_or_dotdot(dent)) {
				continue;
			}
			cnt++;
		}
	}
	cnt = 0;
	while (cnt < lim) {
		voluta_t_getdents_from(t_ctx, dfd, 0, getdents_ctx);
		voluta_t_expect_gt(getdents_ctx->ndents, 0);
		for (size_t j = 0; j < getdents_ctx->ndents; ++j) {
			dent = &getdents_ctx->dents[j];
			voluta_t_expect_true(dirent_isdir(dent));
			if (is_dot_or_dotdot(dent)) {
				continue;
			}
			name = dent->d_name;
			path1 = voluta_t_new_path_nested(t_ctx, path0, name);
			voluta_t_rmdir(path1);
			cnt++;
		}
	}
	voluta_t_close(dfd);
	voluta_t_rmdir(path0);
}

static void test_readdir_counted(struct voluta_t_ctx *t_ctx)
{
	test_readdir_counted_(t_ctx, 64);
	test_readdir_counted_(t_ctx, 1024);
}

static void test_readdir_counted_big(struct voluta_t_ctx *t_ctx)
{
	test_readdir_counted_(t_ctx, 16 * 1024);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void fill_name(char *name, size_t lim, size_t idx)
{
	snprintf(name, lim, "%061lx", idx);
}

static void test_readdir_unlinkat_(struct voluta_t_ctx *t_ctx, size_t lim)
{
	int dfd1, dfd2, fd;
	loff_t doff;
	size_t i, cnt = 0, itr = 0;
	struct stat st;
	char name[VOLUTA_NAME_MAX + 1] = "";
	const struct dirent64 *dent;
	const char *path1 = voluta_t_new_path_unique(t_ctx);
	const char *path2 = voluta_t_new_path_unique(t_ctx);
	const char *fname = voluta_t_new_name_unique(t_ctx);
	struct voluta_t_getdents_ctx *getdents_ctx = new_getdents_ctx(t_ctx);

	voluta_t_mkdir(path1, 0700);
	voluta_t_open(path1, O_DIRECTORY | O_RDONLY, 0, &dfd1);
	voluta_t_mkdir(path2, 0700);
	voluta_t_open(path2, O_DIRECTORY | O_RDONLY, 0, &dfd2);
	voluta_t_openat(dfd2, fname, O_CREAT | O_RDWR, 0600, &fd);
	for (i = 0; i < lim; ++i) {
		fill_name(name, sizeof(name) - 1, i + 1);
		voluta_t_linkat(dfd2, fname, dfd1, name, 0);
		voluta_t_fstat(dfd1, &st);
		voluta_t_expect_ge(st.st_size, i + 1);
	}
	for (i = 0; i < lim; ++i) {
		fill_name(name, sizeof(name) - 1, i + 1);
		voluta_t_linkat_err(dfd2, fname, dfd1, name, 0, -EEXIST);
	}
	while (cnt < lim) {
		voluta_t_fstat(dfd1, &st);
		voluta_t_expect_gt(st.st_size, 0);
		doff = st.st_size / 2;
		voluta_t_getdents_from(t_ctx, dfd1, doff, getdents_ctx);
		if (getdents_ctx->ndents == 0) {
			voluta_t_getdents_from(t_ctx, dfd1, 2, getdents_ctx);
			voluta_t_expect_gt(getdents_ctx->ndents, 0);
		}
		for (size_t j = 0; j < getdents_ctx->ndents; ++j) {
			dent = &getdents_ctx->dents[j];
			if (is_dot_or_dotdot(dent)) {
				continue;
			}
			voluta_t_expect_true(dirent_isreg(dent));
			voluta_t_unlinkat(dfd1, dent->d_name, 0);
			cnt++;
		}
		voluta_t_expect_lt(itr, lim);
		itr++;
	}
	voluta_t_close(fd);
	voluta_t_close(dfd1);
	voluta_t_rmdir(path1);
	voluta_t_unlinkat(dfd2, fname, 0);
	voluta_t_close(dfd2);
	voluta_t_rmdir(path2);
}

static void test_readdir_unlinkat(struct voluta_t_ctx *t_ctx)
{
	test_readdir_unlinkat_(t_ctx, 8);
	test_readdir_unlinkat_(t_ctx, 64);
	test_readdir_unlinkat_(t_ctx, 512);
}

static void test_readdir_unlinkat_big(struct voluta_t_ctx *t_ctx)
{
	test_readdir_unlinkat_(t_ctx, 8192);
}

static void test_readdir_unlinkat_large(struct voluta_t_ctx *t_ctx)
{
	test_readdir_unlinkat_(t_ctx, VOLUTA_LINK_MAX - 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_readdir_basic),
	VOLUTA_T_DEFTEST(test_readdir_unlink),
	VOLUTA_T_DEFTEST(test_readdir_unlink_big),
	VOLUTA_T_DEFTEST(test_readdir_small),
	VOLUTA_T_DEFTEST(test_readdir_normal),
	VOLUTA_T_DEFTEST(test_readdir_big),
	VOLUTA_T_DEFTEST(test_readdir_large),
	VOLUTA_T_DEFTEST(test_readdir_counted),
	VOLUTA_T_DEFTEST(test_readdir_counted_big),
	VOLUTA_T_DEFTEST(test_readdir_unlinkat),
	VOLUTA_T_DEFTEST(test_readdir_unlinkat_big),
	VOLUTA_T_DEFTEST(test_readdir_unlinkat_large),
};

const struct voluta_t_tests
voluta_t_test_readdir = VOLUTA_T_DEFTESTS(t_local_tests);
