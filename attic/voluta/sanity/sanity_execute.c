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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <error.h>
#include <time.h>

#include "sanity.h"

#define VOLUTA_T_METATEST(t_) (&t_)

static const struct voluta_t_tests *voluta_t_testsbl[]  = {
	VOLUTA_T_METATEST(voluta_t_test_access),
	VOLUTA_T_METATEST(voluta_t_test_stat),
	VOLUTA_T_METATEST(voluta_t_test_statvfs),
	VOLUTA_T_METATEST(voluta_t_test_utimes),
	VOLUTA_T_METATEST(voluta_t_test_mkdir),
	VOLUTA_T_METATEST(voluta_t_test_readdir),
	VOLUTA_T_METATEST(voluta_t_test_create),
	VOLUTA_T_METATEST(voluta_t_test_open),
	VOLUTA_T_METATEST(voluta_t_test_link),
	VOLUTA_T_METATEST(voluta_t_test_unlink),
	VOLUTA_T_METATEST(voluta_t_test_chmod),
	VOLUTA_T_METATEST(voluta_t_test_symlink),
	VOLUTA_T_METATEST(voluta_t_test_mknod),
	VOLUTA_T_METATEST(voluta_t_test_fsync),
	VOLUTA_T_METATEST(voluta_t_test_rename),
	VOLUTA_T_METATEST(voluta_t_test_xattr),
	VOLUTA_T_METATEST(voluta_t_test_write),
	VOLUTA_T_METATEST(voluta_t_test_lseek),
	VOLUTA_T_METATEST(voluta_t_test_truncate),
	VOLUTA_T_METATEST(voluta_t_test_namespace),
	VOLUTA_T_METATEST(voluta_t_test_basic_io),
	VOLUTA_T_METATEST(voluta_t_test_boundaries),
	VOLUTA_T_METATEST(voluta_t_test_stat_io),
	VOLUTA_T_METATEST(voluta_t_test_sequencial_file),
	VOLUTA_T_METATEST(voluta_t_test_sparse_file),
	VOLUTA_T_METATEST(voluta_t_test_random),
	VOLUTA_T_METATEST(voluta_t_test_unlinked_file),
	VOLUTA_T_METATEST(voluta_t_test_truncate_io),
	VOLUTA_T_METATEST(voluta_t_test_fallocate),
	VOLUTA_T_METATEST(voluta_t_test_seek),
	VOLUTA_T_METATEST(voluta_t_test_clone),
	VOLUTA_T_METATEST(voluta_t_test_copy_file_range),
	VOLUTA_T_METATEST(voluta_t_test_tmpfile),
	VOLUTA_T_METATEST(voluta_t_test_mmap),
};

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int mask_of(const struct voluta_t_ctx *t_ctx)
{
	return t_ctx->params.testsmask;
}

static void statvfs_of(const struct voluta_t_ctx *t_ctx,
		       struct statvfs *stvfs)
{
	voluta_t_statvfs(t_ctx->params.workdir, stvfs);
}

static void list_test(struct voluta_t_ctx *t_ctx,
		      const struct voluta_t_tdef *tdef)
{
	FILE *fp = stdout;

	t_ctx->currtest = tdef;
	fprintf(fp, "  %-40s\n", t_ctx->currtest->name);
	fflush(fp);
}

static void start_test(struct voluta_t_ctx *t_ctx,
		       const struct voluta_t_tdef *tdef)
{
	FILE *fp = stdout;

	t_ctx->currtest = tdef;
	t_ctx->nbytes_alloc = 0;
	fprintf(fp, "  %-40s", t_ctx->currtest->name);
	fflush(fp);
	voluta_mclock_now(&t_ctx->ts_start);
	statvfs_of(t_ctx, &t_ctx->stvfs);
}

static void finish_test(struct voluta_t_ctx *t_ctx)
{
	FILE *fp = stdout;
	struct timespec dur;

	voluta_mclock_dur(&t_ctx->ts_start, &dur);
	fprintf(fp, "OK (%ld.%03lds)\n", dur.tv_sec, dur.tv_nsec / 1000000L);
	fflush(fp);

	umask(t_ctx->umsk);
	t_ctx->currtest = NULL;
	voluta_t_freeall(t_ctx);
}


static void verify_fsstat(const struct voluta_t_ctx *t_ctx)
{
	struct statvfs stvfs_end;
	const struct statvfs *beg = &t_ctx->stvfs;
	const struct statvfs *end = &stvfs_end;

	if (mask_of(t_ctx) & VOLUTA_T_VERIFY) {
		statvfs_of(t_ctx, &stvfs_end);
		voluta_t_expect_eq(beg->f_namemax, end->f_namemax);
		voluta_t_expect_eq(beg->f_flag, end->f_flag);
		voluta_t_expect_eq(beg->f_bsize, end->f_bsize);
		voluta_t_expect_eq(beg->f_frsize, end->f_frsize);
		voluta_t_expect_eq(beg->f_blocks, end->f_blocks);
		voluta_t_expect_eq(beg->f_bfree, end->f_bfree);
		voluta_t_expect_eq(beg->f_bavail, end->f_bavail);
		voluta_t_expect_eq(beg->f_files, end->f_files);
		voluta_t_expect_eq(beg->f_ffree, end->f_ffree);
		voluta_t_expect_eq(beg->f_favail, end->f_favail);

		voluta_t_expect_lt(end->f_bfree, end->f_blocks);
		voluta_t_expect_lt(end->f_bavail, end->f_blocks);
		voluta_t_expect_lt(end->f_ffree, end->f_files);
		voluta_t_expect_lt(end->f_favail, end->f_files);
	}
}

static void exec_test(struct voluta_t_ctx *t_ctx,
		      const struct voluta_t_tdef *tdef)
{
	start_test(t_ctx, tdef);
	tdef->hook(t_ctx);
	verify_fsstat(t_ctx);
	finish_test(t_ctx);
}

static void voluta_t_runtests(struct voluta_t_ctx *t_ctx)
{
	const struct voluta_t_tdef *tdef;
	const struct voluta_t_tests *tests = &t_ctx->tests;
	const struct voluta_t_params *params = &t_ctx->params;

	for (size_t i = 0; i < tests->len; ++i) {
		tdef = &tests->arr[i];
		if (params->listtests) {
			list_test(t_ctx, tdef);
		} else if (params->testname) {
			if (strstr(tdef->name, params->testname)) {
				exec_test(t_ctx, tdef);
			}
		} else if (tdef->flags) {
			if (mask_of(t_ctx) & tdef->flags) {
				exec_test(t_ctx, tdef);
			}
		}
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void copy_testdef(struct voluta_t_tdef *dst,
			 const struct voluta_t_tdef *src)
{
	memcpy(dst, src, sizeof(*dst));
}

static void swap_testdef(struct voluta_t_tdef *td1, struct voluta_t_tdef *td2)
{
	struct voluta_t_tdef tmp;

	copy_testdef(&tmp, td1);
	copy_testdef(td1, td2);
	copy_testdef(td2, &tmp);
}

static void *safe_malloc(size_t size)
{
	void *ptr;

	ptr = malloc(size);
	if (ptr == NULL) {
		error(EXIT_FAILURE, errno, "malloc failed: size=%lu", size);
	}
	return ptr;
}

static struct voluta_t_tdef *alloc_tests_arr(void)
{
	size_t asz, cnt = 0;
	struct voluta_t_tdef *arr;
	const size_t nelems = VOLUTA_ARRAY_SIZE(voluta_t_testsbl);

	for (size_t i = 0; i < nelems; ++i) {
		cnt += voluta_t_testsbl[i]->len;
	}
	asz = cnt * sizeof(*arr);
	arr = (struct voluta_t_tdef *)safe_malloc(asz);
	memset(arr, 0, asz);

	return arr;
}

static void random_shuffle_tests(struct voluta_t_ctx *t_ctx)
{
	size_t pos1, pos2, rand;
	struct voluta_t_tests *tests = &t_ctx->tests;
	struct voluta_t_tdef *tests_arr = (struct voluta_t_tdef *)tests->arr;

	for (size_t i = 0; i < tests->len; ++i) {
		rand = (size_t)voluta_t_lrand(t_ctx);
		pos1 = (rand ^ i) % tests->len;
		pos2 = ~(rand >> 32) % tests->len;
		swap_testdef(&tests_arr[pos1], &tests_arr[pos2]);
	}
}

static void voluta_t_clone_tests(struct voluta_t_ctx *t_ctx)
{
	size_t len = 0;
	struct voluta_t_tdef *arr = alloc_tests_arr();
	const struct voluta_t_tdef *tdef;
	const size_t nelems = VOLUTA_ARRAY_SIZE(voluta_t_testsbl);

	for (size_t i = 0; i < nelems; ++i) {
		for (size_t j = 0; j < voluta_t_testsbl[i]->len; ++j) {
			tdef = &voluta_t_testsbl[i]->arr[j];
			copy_testdef(&arr[len++], tdef);
		}
	}
	t_ctx->tests.arr = arr;
	t_ctx->tests.len = len;
	if (mask_of(t_ctx) & VOLUTA_T_RANDOM) {
		random_shuffle_tests(t_ctx);
	}
}

static void voluta_t_free_tests(struct voluta_t_ctx *t_ctx)
{
	free((void *)t_ctx->tests.arr);
	t_ctx->tests.arr = NULL;
	t_ctx->tests.len = 0;
}

void t_ctx_exec(struct voluta_t_ctx *t_ctx)
{
	for (int i = 0; i < t_ctx->params.repeatn; ++i) {
		voluta_t_clone_tests(t_ctx);
		voluta_t_runtests(t_ctx);
		voluta_t_free_tests(t_ctx);
	}
}


