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
#include "unitest.h"

#define VOLUTA_UT_DEFTGRP(t_) \
	{ .tests = &t_, .name = VOLUTA_STR(t_) }

static const struct voluta_ut_tgroup g_ut_tgroups[] = {
	/* XXX */
	VOLUTA_UT_DEFTGRP(voluta_ut_file_fallocate_tests),

	VOLUTA_UT_DEFTGRP(voluta_ut_alloc_tests),
	VOLUTA_UT_DEFTGRP(voluta_ut_alloc_tests),
	VOLUTA_UT_DEFTGRP(voluta_ut_super_tests),
	VOLUTA_UT_DEFTGRP(voluta_ut_dir_tests),
	VOLUTA_UT_DEFTGRP(voluta_ut_dir_iter_tests),
	VOLUTA_UT_DEFTGRP(voluta_ut_dir_list_tests),
	VOLUTA_UT_DEFTGRP(voluta_ut_namei_tests),
	VOLUTA_UT_DEFTGRP(voluta_ut_rename_tests),
	VOLUTA_UT_DEFTGRP(voluta_ut_symlink_tests),
	VOLUTA_UT_DEFTGRP(voluta_ut_xattr_tests),
	VOLUTA_UT_DEFTGRP(voluta_ut_file_basic_tests),
	VOLUTA_UT_DEFTGRP(voluta_ut_file_ranges_tests),
	VOLUTA_UT_DEFTGRP(voluta_ut_file_records_tests),
	VOLUTA_UT_DEFTGRP(voluta_ut_file_random_tests),
	VOLUTA_UT_DEFTGRP(voluta_ut_file_truncate_tests),
	VOLUTA_UT_DEFTGRP(voluta_ut_file_fallocate_tests),
	VOLUTA_UT_DEFTGRP(voluta_ut_file_edges_tests),
};

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_track_test(struct voluta_ut_ctx *ut_ctx,
			  const struct voluta_ut_testdef *td, bool pre_execute)
{
	FILE *fp = stdout;
	struct timespec dur;

	if (ut_ctx->silent) {
		return;
	}
	if (pre_execute) {
		fprintf(fp, "  %-32s", td->name);
		voluta_mclock_now(&ut_ctx->ts_start);
	} else {
		voluta_mclock_dur(&ut_ctx->ts_start, &dur);
		fprintf(fp, "OK (%ld.%03lds)\n",
			dur.tv_sec, dur.tv_nsec / 1000000L);
	}
	fflush(fp);
}

static void ut_check_statvfs(const struct statvfs *stvfs1,
			     const struct statvfs *stvfs2)
{
	ut_assert_le(stvfs1->f_bfree, stvfs1->f_blocks);
	ut_assert_le(stvfs1->f_bavail, stvfs1->f_blocks);
	ut_assert_le(stvfs1->f_ffree, stvfs1->f_files);
	ut_assert_le(stvfs1->f_favail, stvfs1->f_files);

	ut_assert_le(stvfs2->f_bfree, stvfs2->f_blocks);
	ut_assert_le(stvfs2->f_bavail, stvfs2->f_blocks);
	ut_assert_le(stvfs2->f_ffree, stvfs2->f_files);
	ut_assert_le(stvfs2->f_favail, stvfs2->f_files);

	ut_assert_eq(stvfs1->f_bsize, stvfs2->f_bsize);
	ut_assert_eq(stvfs1->f_frsize, stvfs2->f_frsize);
	ut_assert_eq(stvfs1->f_blocks, stvfs2->f_blocks);
	ut_assert_eq(stvfs1->f_bfree, stvfs2->f_bfree);
	ut_assert_eq(stvfs1->f_bavail, stvfs2->f_bavail);
	ut_assert_eq(stvfs1->f_files, stvfs2->f_files);
	ut_assert_eq(stvfs1->f_ffree, stvfs2->f_ffree);
	ut_assert_eq(stvfs1->f_favail, stvfs2->f_favail);
}

static size_t ualloc_nbytes_now(const struct voluta_ut_ctx *ut_ctx)
{
	return voluta_env_allocated_mem(ut_ctx->env);
}

static void ut_probe_stats(struct voluta_ut_ctx *ut_ctx, bool pre_execute)
{
	size_t ualloc_now, bk_size = VOLUTA_BK_SIZE;
	struct statvfs stvfs_now;

	if (pre_execute) {
		voluta_ut_statfs_rootd(ut_ctx, &ut_ctx->stvfs_start);
		voluta_ut_drop_caches(ut_ctx);
		ut_ctx->ualloc_start = ualloc_nbytes_now(ut_ctx);
	} else {
		voluta_ut_statfs_rootd(ut_ctx, &stvfs_now);
		ut_check_statvfs(&ut_ctx->stvfs_start, &stvfs_now);
		voluta_ut_drop_caches(ut_ctx);
		ualloc_now = ualloc_nbytes_now(ut_ctx);
		/* XXX ut_assert_eq(ut_ctx->ualloc_start, ualloc_now); */
		ut_assert_ge(ut_ctx->ualloc_start + (2 * bk_size), ualloc_now);
	}
}

static bool ut_is_runnable(const struct voluta_ut_ctx *ut_ctx,
			   const struct voluta_ut_testdef *td)
{
	return (!ut_ctx->test_name || strstr(td->name, ut_ctx->test_name));
}

static void ut_run_tgroup(struct voluta_ut_ctx *ut_ctx,
			  const struct voluta_ut_tgroup *tgroup)
{
	const struct voluta_ut_testdef *td;

	for (size_t i = 0; i < tgroup->tests->len; ++i) {
		td = &tgroup->tests->arr[i];
		if (ut_is_runnable(ut_ctx, td)) {
			ut_track_test(ut_ctx, td, true);
			ut_probe_stats(ut_ctx, true);
			td->hook(ut_ctx);
			ut_probe_stats(ut_ctx, false);
			ut_track_test(ut_ctx, td, false);
			voluta_ut_freeall(ut_ctx);
		}
	}
}

static void ut_exec_tests(struct voluta_ut_ctx *ut_ctx)
{
	const size_t nelems = VOLUTA_ARRAY_SIZE(g_ut_tgroups);

	for (size_t i = 0; i < nelems; ++i) {
		ut_run_tgroup(ut_ctx, &g_ut_tgroups[i]);
	}
}

static struct voluta_ut_ctx *ut_create_ctx(const char *test_name)
{
	int err;
	struct voluta_ut_ctx *ut_ctx = NULL;

	ut_ctx = voluta_ut_new_ctx();
	voluta_ut_assert_not_null(ut_ctx);

	ut_ctx->volume_size = VOLUTA_VOLUME_SIZE_MIN;
	ut_ctx->test_name = test_name;
	err = voluta_ut_load(ut_ctx);
	voluta_ut_assert_ok(err);

	return ut_ctx;
}

static void voluta_ut_exec_tests(const char *test_name)
{
	struct voluta_ut_ctx *ut_ctx;

	ut_ctx = ut_create_ctx(test_name);
	ut_exec_tests(ut_ctx);
	voluta_ut_del_ctx(ut_ctx);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_ut_execute(const char *test_name)
{
	int err;

	err = voluta_lib_init();
	voluta_ut_assert_ok(err);

	voluta_ut_exec_tests(test_name);
}
