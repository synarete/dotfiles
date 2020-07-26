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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "vfstest.h"

/* TODO: Also readdir */
struct vt_ns_ctx {
	struct vt_env *vt_env;
	const char *root_path;
	size_t depth_max;
	size_t dirs_per_level;
	size_t files_per_level;
};


static char *make_path(const struct vt_ns_ctx *ns_ctx,
		       const char *parent_dir, const char *prefix,
		       size_t depth, size_t idx)
{
	const char *name =
		vt_strfmt(ns_ctx->vt_env, "%s_%lu_%lu",
			  prefix, depth + 1, idx + 1);

	return vt_new_path_nested(ns_ctx->vt_env, parent_dir, name);
}

static char *make_dirpath(const struct vt_ns_ctx *ns_ctx,
			  const char *parent_dir, size_t depth, size_t idx)
{
	return make_path(ns_ctx, parent_dir, "dir", depth, idx);
}

static char *make_filepath(const struct vt_ns_ctx *ns_ctx,
			   const char *parent_dir, size_t depth, size_t idx)
{
	return make_path(ns_ctx, parent_dir, "file", depth, idx);
}

static void test_mktree_recursive(const struct vt_ns_ctx *ns_ctx,
				  const char *parent_dir, size_t depth)
{
	int fd;
	char *path;

	if (depth >= ns_ctx->depth_max) {
		return;
	}
	for (size_t i = 0; i < ns_ctx->dirs_per_level; ++i) {
		path = make_dirpath(ns_ctx, parent_dir, depth, i);
		vt_mkdir(path, 0700);
		test_mktree_recursive(ns_ctx, path, depth + 1);
	}
	for (size_t j = 0; j < ns_ctx->files_per_level; ++j) {
		path = make_filepath(ns_ctx, parent_dir, depth, j);
		vt_open(path, O_CREAT | O_WRONLY, 0600, &fd);
		vt_close(fd);
	}
}

static void test_rmtree_recursive(const struct vt_ns_ctx *ns_ctx,
				  const char *parent_dir, size_t depth)
{
	char *path;

	if (depth >= ns_ctx->depth_max) {
		return;
	}
	for (size_t j = 0; j < ns_ctx->files_per_level; ++j) {
		path = make_filepath(ns_ctx, parent_dir, depth, j);
		vt_unlink(path);
	}
	for (size_t i = 0; i < ns_ctx->dirs_per_level; ++i) {
		path = make_dirpath(ns_ctx, parent_dir, depth, i);
		test_rmtree_recursive(ns_ctx, path, depth + 1);
		vt_rmdir(path);
	}
}

static void test_namespace_(struct vt_ns_ctx *ns_ctx)
{
	struct vt_env *vt_env = ns_ctx->vt_env;
	const char *path = vt_new_path_unique(vt_env);

	ns_ctx->root_path = path;
	vt_mkdir(path, 0700);
	test_mktree_recursive(ns_ctx, path, 0);
	test_rmtree_recursive(ns_ctx, path, 0);
	vt_rmdir(path);
}


static void test_namespace_simple(struct vt_env *vt_env)
{
	struct vt_ns_ctx ns_ctx = {
		.vt_env = vt_env,
		.depth_max = 4,
		.dirs_per_level = 4,
		.files_per_level = 4
	};

	test_namespace_(&ns_ctx);
}

static void test_namespace_deep(struct vt_env *vt_env)
{
	struct vt_ns_ctx ns_ctx = {
		.vt_env = vt_env,
		.depth_max = 16,
		.dirs_per_level = 2,
		.files_per_level = 1
	};

	test_namespace_(&ns_ctx);
}

static void test_namespace_wide(struct vt_env *vt_env)
{
	struct vt_ns_ctx ns_ctx = {
		.vt_env = vt_env,
		.depth_max = 2,
		.dirs_per_level = 64,
		.files_per_level = 64
	};

	test_namespace_(&ns_ctx);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTEST(test_namespace_simple),
	VT_DEFTEST(test_namespace_deep),
	VT_DEFTEST(test_namespace_wide),
};

const struct vt_tests vt_test_namespace = VT_DEFTESTS(vt_local_tests);

