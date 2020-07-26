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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include "vfstest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects mkdir(3p) to create nested directories structure and allow rmdir(3p)
 * only to apply on last. Expects all other non-empty directories to return
 * -ENOTEMPTY upon rmdir(3p).
 */
static void test_rmdir_notempty(struct vt_env *vt_env,
				char const **pathi, size_t count)
{
	for (size_t i = 0; i < count; ++i) {
		vt_rmdir_err(pathi[i], -ENOTEMPTY);
	}
	for (size_t j = count; j > 0; --j) {
		vt_rmdir_err(pathi[j - 1], -ENOTEMPTY);
	}
	voluta_unused(vt_env);
}

static void test_mkdir_rmdir(struct vt_env *vt_env)
{
	const char *pathi[32];
	const size_t nelems = VT_ARRAY_SIZE(pathi);
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = path0;

	vt_mkdir(path0, 0700);
	for (size_t i = 0; i < nelems; ++i) {
		path1 = vt_new_pathf(vt_env, path1, "D%d", (int)i);
		vt_mkdir(path1, 0700);
		pathi[i] = path1;
	}
	for (size_t j = nelems; j > 0; --j) {
		test_rmdir_notempty(vt_env, pathi, j - 1);
		path1 = pathi[j - 1];
		vt_rmdir(path1);
	}
	vt_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects mkdir(3p) to create a directory with a mode modified by the process'
 * umask.
 */
static void test_mkdir_umask(struct vt_env *vt_env)
{
	mode_t umsk;
	const mode_t ifmt = S_IFMT;
	struct stat st0;
	struct stat st1;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_under(vt_env, path0);

	umsk  = umask(0020);
	vt_mkdir(path0, 0755);
	vt_stat(path0, &st0);
	vt_expect_dir(st0.st_mode);

	vt_mkdir(path1, 0755);
	vt_stat(path1, &st1);
	vt_expect_dir(st1.st_mode);
	vt_expect_eq((st1.st_mode & ~ifmt), 0755);
	vt_rmdir(path1);

	vt_mkdir(path1, 0153);
	vt_stat(path1, &st1);
	vt_expect_dir(st1.st_mode);
	vt_expect_eq((st1.st_mode & ~ifmt), 0153);
	vt_rmdir(path1);

	umask(077);
	vt_mkdir(path1, 0151);
	vt_stat(path1, &st1);
	vt_expect_dir(st1.st_mode);
	vt_expect_eq((st1.st_mode & ~ifmt), 0100);
	vt_rmdir(path1);

	umask(070);
	vt_mkdir(path1, 0345);
	vt_stat(path1, &st1);
	vt_expect_dir(st1.st_mode);
	vt_expect_eq((st1.st_mode & ~ifmt), 0305);
	vt_rmdir(path1);

	umask(0501);
	vt_mkdir(path1, 0345);
	vt_stat(path1, &st1);
	vt_expect_dir(st1.st_mode);
	vt_expect_eq((st1.st_mode & ~ifmt), 0244);

	vt_rmdir(path1);
	vt_rmdir(path0);
	vt_stat_noent(path0);
	vt_stat_noent(path1);
	umask(umsk);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects mkdir(3p) to create a nested directory when parent directory is
 * writable or not.
 */
static void test_mkdir_chmod(struct vt_env *vt_env)
{
	struct stat st;
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_under(vt_env, path0);
	const char *path2 = vt_new_path_under(vt_env, path1);

	vt_mkdir(path0, 0700);
	vt_stat(path0, &st);
	vt_expect_true(st.st_mode & S_IWUSR);
	vt_expect_true(st.st_mode & S_IXUSR);
	vt_mkdir(path1, st.st_mode);
	vt_stat(path1, &st);
	vt_expect_true(st.st_mode & S_IWUSR);
	vt_expect_true(st.st_mode & S_IXUSR);
	vt_chmod(path1, st.st_mode & ~((mode_t)S_IRUSR));
	vt_stat(path1, &st);
	vt_expect_false(st.st_mode & S_IRUSR);
	vt_mkdir(path2, st.st_mode);
	vt_stat(path2, &st);
	vt_expect_true(st.st_mode & S_IWUSR);
	vt_expect_true(st.st_mode & S_IXUSR);
	vt_expect_false(st.st_mode & S_IRUSR);
	vt_chmod(path2, st.st_mode & ~((mode_t)S_IXUSR));
	vt_stat(path2, &st);
	vt_expect_true(st.st_mode & S_IWUSR);
	vt_expect_false(st.st_mode & S_IXUSR);
	vt_rmdir(path2);
	vt_stat(path1, &st);
	vt_expect_true(st.st_mode & S_IWUSR);
	vt_chmod(path1, st.st_mode & ~((mode_t)S_IWUSR));
	vt_mkdir_err(path2, 0700, -EACCES);
	vt_rmdir(path1);
	vt_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects mkdir(3p) to return ELOOP if too many symbolic links were
 * encountered in translating of the pathname.
 */
static void test_mkdir_loop(struct vt_env *vt_env)
{
	const char *path0 = vt_new_path_unique(vt_env);
	const char *path1 = vt_new_path_unique(vt_env);
	const char *path2 = vt_new_path_under(vt_env, path0);
	const char *path3 = vt_new_path_under(vt_env, path1);

	vt_symlink(path0, path1);
	vt_symlink(path1, path0);
	vt_mkdir_err(path2, 0755, -ELOOP);
	vt_mkdir_err(path3, 0750, -ELOOP);
	vt_unlink(path0);
	vt_unlink(path1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Verify creation & removal of many-many dir-entries.
 */
static void test_mkdir_many(struct vt_env *vt_env, size_t lim)
{
	int fd;
	int dfd;
	const char *name;
	struct stat st;
	const char *path = vt_new_path_unique(vt_env);

	vt_mkdir(path, 0755);
	vt_open(path, O_DIRECTORY | O_RDONLY, 0, &dfd);

	for (size_t i = 0; i < lim; ++i) {
		name = vt_strfmt(vt_env, "%08jx", i);
		vt_openat(dfd, name, O_CREAT | O_RDWR, 0644, &fd);
		vt_close(fd);
	}
	for (size_t j = 0; j < lim; ++j) {
		name = vt_strfmt(vt_env, "%08jx", j);
		vt_fstatat(dfd, name, &st, 0);
		vt_unlinkat(dfd, name, 0);
	}
	vt_stat(path, &st);
	vt_expect_eq(st.st_nlink, 2);
	vt_close(dfd);
	vt_rmdir(path);
}

static void test_mkdir_big(struct vt_env *vt_env)
{
	test_mkdir_many(vt_env, 1000);
}

static void test_mkdir_bigger(struct vt_env *vt_env)
{
	test_mkdir_many(vt_env, 30000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Create and remove deeply-nested directories structure
 */
static void test_mkdir_nested(struct vt_env *vt_env)
{
	size_t i;
	char *path0, *path1;
	char *pathi[64];

	path1 = path0 = vt_new_path_unique(vt_env);
	vt_mkdir(path0, 0700);
	for (i = 0; i < VT_ARRAY_SIZE(pathi); ++i) {
		path1 = vt_new_pathf(vt_env, path1, "D%d", (int)i);
		vt_mkdir(path1, 0700);
		pathi[i] = path1;
	}
	for (i = VT_ARRAY_SIZE(pathi); i > 0; --i) {
		path1 = pathi[i - 1];
		vt_rmdir(path1);
	}
	vt_rmdir(path0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Create recursively directory tree structure
 */
static const char *makename(struct vt_env *vt_env,
			    const char *prefix, size_t depth, size_t id)
{
	return vt_strfmt(vt_env, "%s%03x-%03x",
			 prefix, (int)depth, (int)id);
}

static void test_walktree_recursive(struct vt_env *vt_env,
				    const char *base)
{
	int fd;
	loff_t pos, off = 0;
	struct dirent64 dent = { .d_ino = 0 };
	const char *path;

	vt_open(base, O_DIRECTORY | O_RDONLY, 0, &fd);
	while (1) {
		vt_llseek(fd, off, SEEK_SET, &pos);
		vt_getdent(fd, &dent);
		off = dent.d_off;
		if (off <= 0) {
			break;
		}
		if (!strcmp(dent.d_name, ".") || !strcmp(dent.d_name, "..")) {
			continue;
		}
		if (dent.d_type == DT_DIR) {
			continue;
		}
		path = vt_new_path_nested(vt_env, base, dent.d_name);
		test_walktree_recursive(vt_env, path);
	}
	vt_close(fd);
}


static void test_mktree_recursive(struct vt_env *vt_env,
				  const char *parent,
				  size_t id, size_t nchilds,
				  size_t depth, size_t depth_max)
{
	int fd;
	const char *path;
	const char *name;

	if (depth < depth_max) {
		name = makename(vt_env, "d", depth, id);
		path = vt_new_path_nested(vt_env, parent, name);
		vt_mkdir(path, 0700);
		for (size_t i = 0; i < nchilds; ++i) {
			test_mktree_recursive(vt_env, path, i + 1, nchilds,
					      depth + 1, depth_max);
		}
	} else {
		name = makename(vt_env, "f", depth, id);
		path = vt_new_path_nested(vt_env, parent, name);
		vt_open(path, O_CREAT | O_RDWR, 0600, &fd);
		vt_close(fd);
	}
}

static void test_rmtree_recursive(struct vt_env *vt_env,
				  const char *parent,
				  size_t id, size_t nchilds,
				  size_t depth, size_t depth_max)
{
	const char *path;
	const char *name;

	if (depth < depth_max) {
		name = makename(vt_env, "d", depth, id);
		path = vt_new_path_nested(vt_env, parent, name);
		for (size_t i = 0; i < nchilds; ++i) {
			test_rmtree_recursive(vt_env, path, i + 1, nchilds,
					      depth + 1, depth_max);
		}
		vt_rmdir(path);
	} else {
		name = makename(vt_env, "f", depth, id);
		path = vt_new_path_nested(vt_env, parent, name);
		vt_unlink(path);
	}
}

static void test_mkdir_tree_(struct vt_env *vt_env,
			     size_t nchilds, size_t depth_max)
{
	const char *path = vt_new_path_unique(vt_env);

	vt_mkdir(path, 0700);
	for (size_t i = 0; i < nchilds; ++i) {
		test_mktree_recursive(vt_env, path, i + 1,
				      nchilds, 1, depth_max);
	}
	test_walktree_recursive(vt_env, path);
	for (size_t j = 0; j < nchilds; ++j) {
		test_rmtree_recursive(vt_env, path, j + 1,
				      nchilds, 1, depth_max);
	}
	vt_rmdir(path);
}

static void test_mkdir_tree_wide(struct vt_env *vt_env)
{
	test_mkdir_tree_(vt_env, 32, 2);
}

static void test_mkdir_tree_deep(struct vt_env *vt_env)
{
	test_mkdir_tree_(vt_env, 2, 8);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Expects successful rmdir(3p) on empty directory while still referenced by
 * open file-descriptor.
 */
static void test_rmdir_openat(struct vt_env *vt_env)
{
	int dfd1;
	int dfd2;
	struct stat st;
	const char *name = vt_new_name_unique(vt_env);
	const char *path1 = vt_new_path_unique(vt_env);
	const char *path2 = vt_new_path_nested(vt_env, path1, name);

	vt_mkdir(path1, 0700);
	vt_open(path1, O_DIRECTORY | O_RDONLY, 0, &dfd1);
	vt_mkdirat(dfd1, name, 0700);
	vt_openat(dfd1, name, O_DIRECTORY | O_RDONLY, 0, &dfd2);
	vt_fstat(dfd1, &st);
	vt_expect_true(S_ISDIR(st.st_mode));
	vt_fstat(dfd2, &st);
	vt_expect_true(S_ISDIR(st.st_mode));
	vt_expect_eq(st.st_nlink, 2);
	vt_rmdir(path2);
	vt_fstat(dfd2, &st);
	vt_expect_true(S_ISDIR(st.st_mode));
	vt_expect_le(st.st_nlink, 1); /* TODO: why not eq 1 ? */
	vt_rmdir(path1);
	vt_close(dfd1);
	vt_close(dfd2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTEST(test_mkdir_rmdir),
	VT_DEFTEST(test_mkdir_umask),
	VT_DEFTEST(test_mkdir_chmod),
	VT_DEFTEST(test_mkdir_loop),
	VT_DEFTEST(test_mkdir_nested),
	VT_DEFTEST(test_mkdir_big),
	VT_DEFTEST(test_mkdir_bigger),
	VT_DEFTEST(test_mkdir_tree_wide),
	VT_DEFTEST(test_mkdir_tree_deep),
	VT_DEFTEST(test_rmdir_openat),
};

const struct vt_tests vt_test_mkdir = VT_DEFTESTS(vt_local_tests);
