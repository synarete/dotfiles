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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <error.h>
#include <time.h>
#include <limits.h>
#include "vfstest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void fill_random(const struct vt_env *vt_env, void *buf,
			size_t len)
{
	voluta_getentropy(buf, len);
	voluta_unused(vt_env);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t aligned_size(size_t sz, size_t a)
{
	return ((sz + a - 1) / a) * a;
}

static size_t malloc_total_size(size_t nbytes)
{
	size_t total_size, mchunk_size, data_size;
	struct vt_mchunk *mchunk = NULL;

	total_size = mchunk_size = sizeof(*mchunk);
	data_size = sizeof(mchunk->data);
	if (nbytes > data_size) {
		total_size += aligned_size(nbytes - data_size, mchunk_size);
	}
	return total_size;
}

static struct vt_mchunk *malloc_chunk(struct vt_env *vt_env,
				      size_t nbytes)
{
	size_t total_size;
	struct vt_mchunk *mchunk;

	total_size = malloc_total_size(nbytes);
	mchunk = (struct vt_mchunk *)malloc(total_size);
	if (mchunk == NULL) {
		error(1, errno, "malloc failure size=%lu", total_size);
		abort(); /* Make clang happy */
	}

	mchunk->size = total_size;
	mchunk->next = vt_env->malloc_list;
	vt_env->malloc_list = mchunk;
	vt_env->nbytes_alloc += total_size;

	return mchunk;
}

static void free_chunk(struct vt_env *vt_env,
		       struct vt_mchunk *mchunk)
{
	voluta_assert(vt_env->nbytes_alloc >= mchunk->size);

	vt_env->nbytes_alloc -= mchunk->size;
	memset(mchunk, 0xFD, mchunk->size);
	free(mchunk);
}

static void *do_malloc(struct vt_env *vt_env, size_t sz)
{
	struct vt_mchunk *mchunk;

	mchunk = malloc_chunk(vt_env, sz);
	return mchunk->data;
}

static void *do_zalloc(struct vt_env *vt_env, size_t sz)
{
	void *ptr;

	ptr = do_malloc(vt_env, sz);
	memset(ptr, 0, sz);

	return ptr;
}

char *vt_strdup(struct vt_env *vt_env, const char *str)
{
	char *str2;
	const size_t len = strlen(str);

	str2 = do_malloc(vt_env, len + 1);
	memcpy(str2, str, len);
	str2[len] = '\0';

	return str2;
}

void vt_freeall(struct vt_env *vt_env)
{
	struct vt_mchunk *mchunk, *mnext;

	mchunk = vt_env->malloc_list;
	while (mchunk != NULL) {
		mnext = mchunk->next;
		free_chunk(vt_env, mchunk);
		mchunk = mnext;
	}
	voluta_assert_eq(vt_env->nbytes_alloc, 0);

	vt_env->nbytes_alloc = 0;
	vt_env->malloc_list = NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void vt_env_init(struct vt_env *vt_env, const struct vt_params *params)
{
	memset(vt_env, 0, sizeof(*vt_env));
	memcpy(&vt_env->params, params, sizeof(vt_env->params));
	vt_env->currtest = NULL;
	vt_env->start = time(NULL);
	vt_env->seqn = 0;
	vt_env->nbytes_alloc = 0;
	vt_env->malloc_list = NULL;
	vt_env->pid = getpid();
	vt_env->uid = geteuid();
	vt_env->gid = getegid();
	vt_env->umsk = umask(0);
	umask(vt_env->umsk);
}

void vt_env_fini(struct vt_env *vt_env)
{
	vt_freeall(vt_env);
	memset(vt_env, 0, sizeof(*vt_env));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void vt_suspend(const struct vt_env *vt_env, int sec,
		int part)
{
	int err;
	struct timespec rem = { 0, 0 };
	struct timespec req = { sec, (long)part * 1000000LL };

	err = nanosleep(&req, &rem);
	while (err && (errno == EINTR)) {
		memcpy(&req, &rem, sizeof(req));
		err = nanosleep(&req, &rem);
	}
	(void)vt_env;
}

void vt_suspends(const struct vt_env *vt_env, int sec)
{
	vt_suspend(vt_env, sec, 0);
}

static char *joinpath(struct vt_env *vt_env, const char *s1,
		      const char *s2)
{
	char *path;
	const size_t len1 = strlen(s1);
	const size_t len2 = strlen(s2);
	const size_t msz = len1 + len2 + 2;

	path = (char *)do_malloc(vt_env, msz);
	strncpy(path, s1, len1 + 1);
	path[len1] = '/';
	strncpy(path + len1 + 1, s2, len2 + 1);
	path[len1 + 1 + len2] = '\0';
	return path;
}

char *vt_new_path_nested(struct vt_env *vt_env,
			 const char *base,
			 const char *name)
{
	return joinpath(vt_env, base, name);
}

char *vt_new_path_name(struct vt_env *vt_env,
		       const char *name)
{
	const char *workdir = vt_env->params.workdir;

	return vt_new_path_nested(vt_env, workdir, name);
}

char *vt_new_path_unique(struct vt_env *vt_env)
{
	const char *name = vt_new_name_unique(vt_env);

	return vt_new_path_name(vt_env, name);
}

char *vt_new_path_under(struct vt_env *vt_env,
			const char *base)
{
	const char *name = vt_new_name_unique(vt_env);

	return vt_new_path_nested(vt_env, base, name);
}

char *vt_new_pathf(struct vt_env *vt_env,
		   const char *path, const char *fmt, ...)
{
	va_list ap;
	char buf[PATH_MAX / 2] = "";

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
	va_end(ap);
	return vt_new_path_nested(vt_env, path, buf);
}

void *vt_new_buf_zeros(struct vt_env *vt_env, size_t bsz)
{
	return do_zalloc(vt_env, bsz);
}

void *vt_new_buf_rands(struct vt_env *vt_env, size_t bsz)
{
	void *buf = NULL;

	if (bsz > 0) {
		buf = do_malloc(vt_env, bsz);
		fill_random(vt_env, buf, bsz);
	}
	return buf;
}

long vt_lrand(struct vt_env *vt_env)
{
	long r;

	fill_random(vt_env, &r, sizeof(r));
	return r;
}

/* Generates ordered sequence of integers [base..base+n) */
static void vt_create_seq(long *arr, size_t n, long base)
{
	for (size_t i = 0; i < n; ++i) {
		arr[i] = base++;
	}
}

static long *vt_new_seq(struct vt_env *vt_env, size_t cnt,
			long base)
{
	long *arr;

	arr = vt_new_buf_zeros(vt_env, cnt * sizeof(*arr));
	vt_create_seq(arr, cnt, base);

	return arr;
}

/* Generates sequence of integers [base..base+n) and then random shuffle */
static void swap(long *arr, size_t i, size_t j)
{
	long tmp = arr[i];

	arr[i] = arr[j];
	arr[j] = tmp;
}

long *vt_new_randseq(struct vt_env *vt_env, size_t cnt, long base)
{
	long *arr;
	size_t *pos;

	arr = vt_new_seq(vt_env, cnt, base);
	pos = vt_new_buf_rands(vt_env, cnt * sizeof(*pos));
	for (size_t j = 0; j < cnt; ++j) {
		swap(arr, j, pos[j] % cnt);
	}
	return arr;
}

static void fill_buf_nums(unsigned long base, void *buf, size_t bsz)
{
	size_t i, cnt;
	uint64_t *ubuf = (uint64_t *)buf;
	char *rem, *end;

	cnt = bsz / sizeof(*ubuf);
	for (i = 0; i < cnt; ++i) {
		ubuf[i] = base + i;
	}
	rem = (char *)(ubuf + cnt);
	end = (char *)buf + bsz;
	cnt = (size_t)(end - rem);
	memset(rem, 0, cnt);
}

void *vt_new_buf_nums(struct vt_env *vt_env,
		      unsigned long base, size_t bsz)
{
	void *buf;

	buf = vt_new_buf_zeros(vt_env, bsz);
	fill_buf_nums(base, buf, bsz);
	return buf;
}

char *vt_strfmt(struct vt_env *vt_env, const char *fmt, ...)
{
	int nb;
	size_t bsz = 255;
	char *buf;
	va_list ap;

	va_start(ap, fmt);
	nb = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);

	if ((size_t)nb > bsz) {
		bsz = (size_t)nb;
	}

	va_start(ap, fmt);
	buf = do_zalloc(vt_env, bsz + 1);
	nb = vsnprintf(buf, bsz, fmt, ap);
	va_end(ap);

	(void)nb;
	return buf;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

char *vt_new_name_unique(struct vt_env *vt_env)
{
	uint32_t seq, rnd, val;
	const char *test = vt_env->currtest->name;

	seq = (uint32_t)(++vt_env->seqn);
	rnd = (uint32_t)vt_lrand(vt_env);
	val = seq ^ rnd ^ (uint32_t)vt_env->pid;
	return vt_strfmt(vt_env, "%s_%08x", test, val);
}

long vt_timespec_diff(const struct timespec *ts1,
		      const struct timespec *ts2)
{
	long d_sec, d_nsec, n = 1000000000L;

	d_sec = ts2->tv_sec - ts1->tv_sec;
	d_nsec = ts2->tv_nsec - ts1->tv_nsec;

	return (d_sec * n) + d_nsec;
}
