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
#include "fstests.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void fill_random(const struct voluta_t_ctx *t_ctx, void *buf,
			size_t len)
{
	voluta_getentropy(buf, len);
	voluta_unused(t_ctx);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t aligned_size(size_t sz, size_t a)
{
	return ((sz + a - 1) / a) * a;
}

static size_t malloc_total_size(size_t nbytes)
{
	size_t total_size, mchunk_size, data_size;
	struct voluta_t_mchunk *mchunk = NULL;

	total_size = mchunk_size = sizeof(*mchunk);
	data_size = sizeof(mchunk->data);
	if (nbytes > data_size) {
		total_size += aligned_size(nbytes - data_size, mchunk_size);
	}
	return total_size;
}

static struct voluta_t_mchunk *malloc_chunk(struct voluta_t_ctx *t_ctx,
		size_t nbytes)
{
	size_t total_size;
	struct voluta_t_mchunk *mchunk;

	total_size = malloc_total_size(nbytes);
	mchunk = (struct voluta_t_mchunk *)malloc(total_size);
	if (mchunk == NULL) {
		error(1, errno, "malloc failure size=%lu", total_size);
		abort(); /* Make clang happy */
	}

	mchunk->size = total_size;
	mchunk->next = t_ctx->malloc_list;
	t_ctx->malloc_list = mchunk;
	t_ctx->nbytes_alloc += total_size;

	return mchunk;
}

static void free_chunk(struct voluta_t_ctx *t_ctx,
		       struct voluta_t_mchunk *mchunk)
{
	voluta_assert(t_ctx->nbytes_alloc >= mchunk->size);

	t_ctx->nbytes_alloc -= mchunk->size;
	memset(mchunk, 0xFD, mchunk->size);
	free(mchunk);
}

static void *do_malloc(struct voluta_t_ctx *t_ctx, size_t sz)
{
	struct voluta_t_mchunk *mchunk;

	mchunk = malloc_chunk(t_ctx, sz);
	return mchunk->data;
}

static void *do_zalloc(struct voluta_t_ctx *t_ctx, size_t sz)
{
	void *ptr;

	ptr = do_malloc(t_ctx, sz);
	memset(ptr, 0, sz);

	return ptr;
}

char *voluta_t_strdup(struct voluta_t_ctx *t_ctx, const char *str)
{
	char *str2;
	const size_t len = strlen(str);

	str2 = do_malloc(t_ctx, len + 1);
	memcpy(str2, str, len);
	str2[len] = '\0';

	return str2;
}

void voluta_t_freeall(struct voluta_t_ctx *t_ctx)
{
	struct voluta_t_mchunk *mchunk, *mnext;

	mchunk = t_ctx->malloc_list;
	while (mchunk != NULL) {
		mnext = mchunk->next;
		free_chunk(t_ctx, mchunk);
		mchunk = mnext;
	}
	voluta_assert_eq(t_ctx->nbytes_alloc, 0);

	t_ctx->nbytes_alloc = 0;
	t_ctx->malloc_list = NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void t_ctx_init(struct voluta_t_ctx *t_ctx,
		const struct voluta_t_params *params)
{
	memset(t_ctx, 0, sizeof(*t_ctx));
	memcpy(&t_ctx->params, params, sizeof(t_ctx->params));
	t_ctx->currtest = NULL;
	t_ctx->start = time(NULL);
	t_ctx->seqn = 0;
	t_ctx->nbytes_alloc = 0;
	t_ctx->malloc_list = NULL;
	t_ctx->pid = getpid();
	t_ctx->uid = geteuid();
	t_ctx->gid = getegid();
	t_ctx->umsk = umask(0);
	umask(t_ctx->umsk);
}

void t_ctx_fini(struct voluta_t_ctx *t_ctx)
{
	voluta_t_freeall(t_ctx);
	memset(t_ctx, 0, sizeof(*t_ctx));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_t_suspend(const struct voluta_t_ctx *t_ctx, int sec,
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
	(void)t_ctx;
}

void voluta_t_suspends(const struct voluta_t_ctx *t_ctx, int sec)
{
	voluta_t_suspend(t_ctx, sec, 0);
}

static char *joinpath(struct voluta_t_ctx *t_ctx, const char *s1,
		      const char *s2)
{
	char *path;
	const size_t len1 = strlen(s1);
	const size_t len2 = strlen(s2);
	const size_t msz = len1 + len2 + 2;

	path = (char *)do_malloc(t_ctx, msz);
	strncpy(path, s1, len1 + 1);
	path[len1] = '/';
	strncpy(path + len1 + 1, s2, len2 + 1);
	path[len1 + 1 + len2] = '\0';
	return path;
}

char *voluta_t_new_path_nested(struct voluta_t_ctx *t_ctx,
			       const char *base,
			       const char *name)
{
	return joinpath(t_ctx, base, name);
}

char *voluta_t_new_path_name(struct voluta_t_ctx *t_ctx,
			     const char *name)
{
	const char *workdir = t_ctx->params.workdir;

	return voluta_t_new_path_nested(t_ctx, workdir, name);
}

char *voluta_t_new_path_unique(struct voluta_t_ctx *t_ctx)
{
	const char *name = voluta_t_new_name_unique(t_ctx);

	return voluta_t_new_path_name(t_ctx, name);
}

char *voluta_t_new_path_under(struct voluta_t_ctx *t_ctx,
			      const char *base)
{
	const char *name = voluta_t_new_name_unique(t_ctx);

	return voluta_t_new_path_nested(t_ctx, base, name);
}

char *voluta_t_new_pathf(struct voluta_t_ctx *t_ctx,
			 const char *path, const char *fmt, ...)
{
	va_list ap;
	char buf[PATH_MAX / 2] = "";

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
	va_end(ap);
	return voluta_t_new_path_nested(t_ctx, path, buf);
}

void *voluta_t_new_buf_zeros(struct voluta_t_ctx *t_ctx, size_t bsz)
{
	return do_zalloc(t_ctx, bsz);
}

void *voluta_t_new_buf_rands(struct voluta_t_ctx *t_ctx, size_t bsz)
{
	void *buf = NULL;

	if (bsz > 0) {
		buf = do_malloc(t_ctx, bsz);
		fill_random(t_ctx, buf, bsz);
	}
	return buf;
}

long voluta_t_lrand(struct voluta_t_ctx *t_ctx)
{
	long r;

	fill_random(t_ctx, &r, sizeof(r));
	return r;
}

/* Generates ordered sequence of integers [base..base+n) */
static void voluta_t_create_seq(long *arr, size_t n, long base)
{
	for (size_t i = 0; i < n; ++i) {
		arr[i] = base++;
	}
}

static long *voluta_t_new_seq(struct voluta_t_ctx *t_ctx, size_t cnt,
			      long base)
{
	long *arr;

	arr = voluta_t_new_buf_zeros(t_ctx, cnt * sizeof(*arr));
	voluta_t_create_seq(arr, cnt, base);

	return arr;
}

/* Generates sequence of integers [base..base+n) and then random shuffle */
static void swap(long *arr, size_t i, size_t j)
{
	long tmp = arr[i];

	arr[i] = arr[j];
	arr[j] = tmp;
}

long *voluta_t_new_randseq(struct voluta_t_ctx *t_ctx, size_t cnt,
			   long base)
{
	long *arr;
	size_t *pos;

	arr = voluta_t_new_seq(t_ctx, cnt, base);
	pos = voluta_t_new_buf_rands(t_ctx, cnt * sizeof(*pos));
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

void *voluta_t_new_buf_nums(struct voluta_t_ctx *t_ctx,
			    unsigned long base,
			    size_t bsz)
{
	void *buf;

	buf = voluta_t_new_buf_zeros(t_ctx, bsz);
	fill_buf_nums(base, buf, bsz);
	return buf;
}

char *voluta_t_strfmt(struct voluta_t_ctx *t_ctx, const char *fmt, ...)
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
	buf = do_zalloc(t_ctx, bsz + 1);
	nb = vsnprintf(buf, bsz, fmt, ap);
	va_end(ap);

	(void)nb;
	return buf;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

char *voluta_t_new_name_unique(struct voluta_t_ctx *t_ctx)
{
	uint32_t seq, rnd, val;
	const char *test = t_ctx->currtest->name;

	seq = (uint32_t)(++t_ctx->seqn);
	rnd = (uint32_t)voluta_t_lrand(t_ctx);
	val = seq ^ rnd ^ (uint32_t)t_ctx->pid;
	return voluta_t_strfmt(t_ctx, "%s_%08x", test, val);
}

long voluta_t_timespec_diff(const struct timespec *ts1,
			    const struct timespec *ts2)
{
	long d_sec, d_nsec, n = 1000000000L;

	d_sec = ts2->tv_sec - ts1->tv_sec;
	d_nsec = ts2->tv_nsec - ts1->tv_nsec;

	return (d_sec * n) + d_nsec;
}
