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

#include "vfstest.h"

#define VT_METATEST(t_) (&t_)

static const struct vt_tests *vt_testsbl[]  = {
	VT_METATEST(vt_test_access),
	VT_METATEST(vt_test_stat),
	VT_METATEST(vt_test_statvfs),
	VT_METATEST(vt_test_utimes),
	VT_METATEST(vt_test_mkdir),
	VT_METATEST(vt_test_readdir),
	VT_METATEST(vt_test_create),
	VT_METATEST(vt_test_open),
	VT_METATEST(vt_test_link),
	VT_METATEST(vt_test_unlink),
	VT_METATEST(vt_test_chmod),
	VT_METATEST(vt_test_symlink),
	VT_METATEST(vt_test_mknod),
	VT_METATEST(vt_test_fsync),
	VT_METATEST(vt_test_rename),
	VT_METATEST(vt_test_xattr),
	VT_METATEST(vt_test_write),
	VT_METATEST(vt_test_lseek),
	VT_METATEST(vt_test_fiemap),
	VT_METATEST(vt_test_truncate),
	VT_METATEST(vt_test_namespace),
	VT_METATEST(vt_test_basic_io),
	VT_METATEST(vt_test_boundaries),
	VT_METATEST(vt_test_stat_io),
	VT_METATEST(vt_test_sequencial_file),
	VT_METATEST(vt_test_sparse_file),
	VT_METATEST(vt_test_random),
	VT_METATEST(vt_test_unlinked_file),
	VT_METATEST(vt_test_truncate_io),
	VT_METATEST(vt_test_fallocate),
	VT_METATEST(vt_test_clone),
	VT_METATEST(vt_test_copy_file_range),
	VT_METATEST(vt_test_tmpfile),
	VT_METATEST(vt_test_mmap),
};

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int mask_of(const struct vt_env *vt_env)
{
	return vt_env->params.testsmask;
}

static void statvfs_of(const struct vt_env *vt_env,
		       struct statvfs *stvfs)
{
	vt_statvfs(vt_env->params.workdir, stvfs);
}

static void list_test(struct vt_env *vt_env,
		      const struct vt_tdef *tdef)
{
	FILE *fp = stdout;

	vt_env->currtest = tdef;
	fprintf(fp, "  %-40s\n", vt_env->currtest->name);
	fflush(fp);
}

static void start_test(struct vt_env *vt_env,
		       const struct vt_tdef *tdef)
{
	FILE *fp = stdout;

	vt_env->currtest = tdef;
	vt_env->nbytes_alloc = 0;
	fprintf(fp, "  %-40s", vt_env->currtest->name);
	fflush(fp);
	voluta_mclock_now(&vt_env->ts_start);
	statvfs_of(vt_env, &vt_env->stvfs);
}

static void finish_test(struct vt_env *vt_env)
{
	FILE *fp = stdout;
	struct timespec dur;

	voluta_mclock_dur(&vt_env->ts_start, &dur);
	fprintf(fp, "OK (%ld.%03lds)\n", dur.tv_sec, dur.tv_nsec / 1000000L);
	fflush(fp);

	umask(vt_env->umsk);
	vt_env->currtest = NULL;
	vt_freeall(vt_env);
}


static void verify_fsstat(const struct vt_env *vt_env)
{
	struct statvfs stvfs_end;
	const struct statvfs *beg = &vt_env->stvfs;
	const struct statvfs *end = &stvfs_end;

	if (mask_of(vt_env) & VT_VERIFY) {
		statvfs_of(vt_env, &stvfs_end);
		vt_expect_eq(beg->f_namemax, end->f_namemax);
		vt_expect_eq(beg->f_flag, end->f_flag);
		vt_expect_eq(beg->f_bsize, end->f_bsize);
		vt_expect_eq(beg->f_frsize, end->f_frsize);
		vt_expect_eq(beg->f_blocks, end->f_blocks);
		vt_expect_eq(beg->f_bfree, end->f_bfree);
		vt_expect_eq(beg->f_bavail, end->f_bavail);
		vt_expect_eq(beg->f_files, end->f_files);
		vt_expect_eq(beg->f_ffree, end->f_ffree);
		vt_expect_eq(beg->f_favail, end->f_favail);

		vt_expect_lt(end->f_bfree, end->f_blocks);
		vt_expect_lt(end->f_bavail, end->f_blocks);
		vt_expect_lt(end->f_ffree, end->f_files);
		vt_expect_lt(end->f_favail, end->f_files);
	}
}

static void exec_test(struct vt_env *vt_env,
		      const struct vt_tdef *tdef)
{
	start_test(vt_env, tdef);
	tdef->hook(vt_env);
	verify_fsstat(vt_env);
	finish_test(vt_env);
}

static void vt_runtests(struct vt_env *vt_env)
{
	const struct vt_tdef *tdef;
	const struct vt_tests *tests = &vt_env->tests;
	const struct vt_params *params = &vt_env->params;

	for (size_t i = 0; i < tests->len; ++i) {
		tdef = &tests->arr[i];
		if (tdef->flags & VT_IGNORE) {
			continue;
		}
		if (params->listtests) {
			list_test(vt_env, tdef);
		} else if (params->testname) {
			if (strstr(tdef->name, params->testname)) {
				exec_test(vt_env, tdef);
			}
		} else if (tdef->flags) {
			if (mask_of(vt_env) & tdef->flags) {
				exec_test(vt_env, tdef);
			}
		}
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void copy_testdef(struct vt_tdef *dst,
			 const struct vt_tdef *src)
{
	memcpy(dst, src, sizeof(*dst));
}

static void swap_testdef(struct vt_tdef *td1, struct vt_tdef *td2)
{
	struct vt_tdef tmp;

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

static struct vt_tdef *alloc_tests_arr(void)
{
	size_t asz, cnt = 0;
	struct vt_tdef *arr;
	const size_t nelems = VT_ARRAY_SIZE(vt_testsbl);

	for (size_t i = 0; i < nelems; ++i) {
		cnt += vt_testsbl[i]->len;
	}
	asz = cnt * sizeof(*arr);
	arr = (struct vt_tdef *)safe_malloc(asz);
	memset(arr, 0, asz);

	return arr;
}

static void random_shuffle_tests(struct vt_env *vt_env)
{
	size_t pos1, pos2, rand;
	struct vt_tests *tests = &vt_env->tests;
	struct vt_tdef *tests_arr = voluta_unconst(tests->arr);

	for (size_t i = 0; i < tests->len; ++i) {
		rand = (size_t)vt_lrand(vt_env);
		pos1 = (rand ^ i) % tests->len;
		pos2 = ~(rand >> 32) % tests->len;
		swap_testdef(&tests_arr[pos1], &tests_arr[pos2]);
	}
}

static void vt_clone_tests(struct vt_env *vt_env)
{
	size_t len = 0;
	struct vt_tdef *arr = alloc_tests_arr();
	const struct vt_tdef *tdef;
	const size_t nelems = VT_ARRAY_SIZE(vt_testsbl);

	for (size_t i = 0; i < nelems; ++i) {
		for (size_t j = 0; j < vt_testsbl[i]->len; ++j) {
			tdef = &vt_testsbl[i]->arr[j];
			copy_testdef(&arr[len++], tdef);
		}
	}
	vt_env->tests.arr = arr;
	vt_env->tests.len = len;
	if (mask_of(vt_env) & VT_RANDOM) {
		random_shuffle_tests(vt_env);
	}
}

static void vt_free_tests(struct vt_env *vt_env)
{
	free(voluta_unconst(vt_env->tests.arr));
	vt_env->tests.arr = NULL;
	vt_env->tests.len = 0;
}

void vt_env_exec(struct vt_env *vt_env)
{
	for (int i = 0; i < vt_env->params.repeatn; ++i) {
		vt_clone_tests(vt_env);
		vt_runtests(vt_env);
		vt_free_tests(vt_env);
	}
}


