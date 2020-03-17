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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "sanity.h"

/* TODO: Also readdir */
struct voluta_t_ns_ctx {
	struct voluta_t_ctx *t_ctx;
	const char *root_path;
	size_t depth_max;
	size_t dirs_per_level;
	size_t files_per_level;
};


static char *make_path(const struct voluta_t_ns_ctx *ns_ctx,
		       const char *parent_dir, const char *prefix,
		       size_t depth, size_t idx)
{
	const char *name =
		voluta_t_strfmt(ns_ctx->t_ctx, "%s_%lu_%lu",
				prefix, depth + 1, idx + 1);

	return voluta_t_new_path_nested(ns_ctx->t_ctx, parent_dir, name);
}

static char *make_dirpath(const struct voluta_t_ns_ctx *ns_ctx,
			  const char *parent_dir, size_t depth, size_t idx)
{
	return make_path(ns_ctx, parent_dir, "dir", depth, idx);
}

static char *make_filepath(const struct voluta_t_ns_ctx *ns_ctx,
			   const char *parent_dir, size_t depth, size_t idx)
{
	return make_path(ns_ctx, parent_dir, "file", depth, idx);
}

static void test_mktree_recursive(const struct voluta_t_ns_ctx *ns_ctx,
				  const char *parent_dir, size_t depth)
{
	int fd;
	char *path;

	if (depth >= ns_ctx->depth_max) {
		return;
	}
	for (size_t i = 0; i < ns_ctx->dirs_per_level; ++i) {
		path = make_dirpath(ns_ctx, parent_dir, depth, i);
		voluta_t_mkdir(path, 0700);
		test_mktree_recursive(ns_ctx, path, depth + 1);
	}
	for (size_t j = 0; j < ns_ctx->files_per_level; ++j) {
		path = make_filepath(ns_ctx, parent_dir, depth, j);
		voluta_t_open(path, O_CREAT | O_WRONLY, 0600, &fd);
		voluta_t_close(fd);
	}
}

static void test_rmtree_recursive(const struct voluta_t_ns_ctx *ns_ctx,
				  const char *parent_dir, size_t depth)
{
	char *path;

	if (depth >= ns_ctx->depth_max) {
		return;
	}
	for (size_t j = 0; j < ns_ctx->files_per_level; ++j) {
		path = make_filepath(ns_ctx, parent_dir, depth, j);
		voluta_t_unlink(path);
	}
	for (size_t i = 0; i < ns_ctx->dirs_per_level; ++i) {
		path = make_dirpath(ns_ctx, parent_dir, depth, i);
		test_rmtree_recursive(ns_ctx, path, depth + 1);
		voluta_t_rmdir(path);
	}
}

static void test_namespace_(struct voluta_t_ns_ctx *ns_ctx)
{
	struct voluta_t_ctx *t_ctx = ns_ctx->t_ctx;
	const char *path = voluta_t_new_path_unique(t_ctx);

	ns_ctx->root_path = path;
	voluta_t_mkdir(path, 0700);
	test_mktree_recursive(ns_ctx, path, 0);
	test_rmtree_recursive(ns_ctx, path, 0);
	voluta_t_rmdir(path);
}


static void test_namespace_simple(struct voluta_t_ctx *t_ctx)
{
	struct voluta_t_ns_ctx ns_ctx = {
		.t_ctx = t_ctx,
		.depth_max = 4,
		.dirs_per_level = 4,
		.files_per_level = 4
	};

	test_namespace_(&ns_ctx);
}

static void test_namespace_deep(struct voluta_t_ctx *t_ctx)
{
	struct voluta_t_ns_ctx ns_ctx = {
		.t_ctx = t_ctx,
		.depth_max = 16,
		.dirs_per_level = 2,
		.files_per_level = 1
	};

	test_namespace_(&ns_ctx);
}

static void test_namespace_wide(struct voluta_t_ctx *t_ctx)
{
	struct voluta_t_ns_ctx ns_ctx = {
		.t_ctx = t_ctx,
		.depth_max = 2,
		.dirs_per_level = 64,
		.files_per_level = 64
	};

	test_namespace_(&ns_ctx);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_namespace_simple),
	VOLUTA_T_DEFTEST(test_namespace_deep),
	VOLUTA_T_DEFTEST(test_namespace_wide),
};

const struct voluta_t_tests
voluta_t_test_namespace = VOLUTA_T_DEFTESTS(t_local_tests);

