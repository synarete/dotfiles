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
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>
#include "unitest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t aligned_size(size_t sz, size_t a)
{
	return ((sz + a - 1) / a) * a;
}

static size_t malloc_total_size(size_t nbytes)
{
	size_t total_size;
	struct voluta_ut_malloc_chunk *mchunk = NULL;
	const size_t mchunk_size = sizeof(*mchunk);
	const size_t data_size = sizeof(mchunk->data);

	total_size = mchunk_size;
	if (nbytes > data_size) {
		total_size += aligned_size(nbytes - data_size, mchunk_size);
	}
	return total_size;
}

static struct voluta_ut_malloc_chunk *
ut_malloc(struct voluta_ut_ctx *ut_ctx, size_t nbytes)
{
	size_t total_size;
	struct voluta_ut_malloc_chunk *mchunk;

	total_size = malloc_total_size(nbytes);
	mchunk = (struct voluta_ut_malloc_chunk *)malloc(total_size);
	voluta_assert_not_null(mchunk);

	mchunk->size = total_size;
	mchunk->next = ut_ctx->malloc_list;
	ut_ctx->malloc_list = mchunk;
	ut_ctx->nbytes_alloc += total_size;

	return mchunk;
}

static void ut_free(struct voluta_ut_ctx *ut_ctx,
		    struct voluta_ut_malloc_chunk *mchunk)
{
	voluta_assert_ge(ut_ctx->nbytes_alloc, mchunk->size);

	ut_ctx->nbytes_alloc -= mchunk->size;
	memset(mchunk, 0xFC, mchunk->size);
	free(mchunk);
}

void *voluta_ut_malloc(struct voluta_ut_ctx *ut_ctx, size_t nbytes)
{
	struct voluta_ut_malloc_chunk *mchunk;

	mchunk = ut_malloc(ut_ctx, nbytes);
	return mchunk->data;
}

void *voluta_ut_zalloc(struct voluta_ut_ctx *ut_ctx, size_t nbytes)
{
	void *ptr;

	ptr = voluta_ut_malloc(ut_ctx, nbytes);
	memset(ptr, 0, nbytes);

	return ptr;
}

char *voluta_ut_strdup(struct voluta_ut_ctx *ut_ctx, const char *str)
{
	return voluta_ut_strndup(ut_ctx, str, strlen(str));
}

char *voluta_ut_strndup(struct voluta_ut_ctx *ut_ctx, const char *str,
			size_t len)
{
	char *str2;

	str2 = voluta_ut_zalloc(ut_ctx, len + 1);
	memcpy(str2, str, len);

	return str2;
}

void voluta_ut_freeall(struct voluta_ut_ctx *ut_ctx)
{
	struct voluta_ut_malloc_chunk *mnext;
	struct voluta_ut_malloc_chunk *mchunk = ut_ctx->malloc_list;

	while (mchunk != NULL) {
		mnext = mchunk->next;
		ut_free(ut_ctx, mchunk);
		mchunk = mnext;
	}
	voluta_assert_eq(ut_ctx->nbytes_alloc, 0);

	ut_ctx->nbytes_alloc = 0;
	ut_ctx->malloc_list = NULL;
}

char *voluta_ut_make_name(struct voluta_ut_ctx *ut_ctx,
			  const char *prefix, size_t idx)
{
	char name[UT_NAME_MAX + 1] = "";

	snprintf(name, sizeof(name) - 1, "%s-%lx", prefix, idx + 1);
	return voluta_ut_strdup(ut_ctx, name);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void *voluta_ut_mmap_memory(size_t msz)
{
	int err;
	void *addr = NULL;

	err = voluta_sys_mmap_anon(msz, 0, &addr);
	voluta_assert_ok(err);

	return addr;
}

void voluta_ut_munmap_memory(void *mem, size_t msz)
{
	if (mem) {
		voluta_sys_munmap(mem, msz);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_ctx_init(struct voluta_ut_ctx *ut_ctx)
{
	int err;
	size_t len;
	size_t psz;
	time_t now = time(NULL);
	struct voluta_env *env = NULL;
	struct voluta_params params = {
		.ucred = {
			.uid = getuid(),
			.gid = getgid(),
			.pid = getpid(),
			.umask = 0002
		},
		.mount_point = "/",
		.volume_path = NULL,
		.volume_size = VOLUTA_VOLUME_SIZE_MIN,
		.passphrase = ut_ctx->pass,
		.salt = ut_ctx->salt,
		.tmpfs = true,
		.pedantic = true
	};

	memset(ut_ctx, 0, sizeof(*ut_ctx));
	ut_ctx->silent = false;
	ut_ctx->malloc_list = NULL;
	ut_ctx->nbytes_alloc = 0;

	psz = sizeof(ut_ctx->pass);
	len = voluta_clamp((size_t)now % psz, 1, psz - 1);
	err = voluta_make_ascii_passphrase(ut_ctx->pass, len);
	voluta_assert_ok(err);

	psz = sizeof(ut_ctx->salt);
	len = voluta_clamp(++len % psz, 1, psz - 1);
	err = voluta_make_ascii_passphrase(ut_ctx->salt, len);
	voluta_assert_ok(err);

	err = voluta_env_new(&env);
	voluta_assert_ok(err);

	err = voluta_env_setparams(env, &params);
	voluta_assert_ok(err);

	ut_ctx->env = env;
	ut_ctx->volume_size = params.volume_size;
}

static void ut_ctx_fini(struct voluta_ut_ctx *ut_ctx)
{
	struct voluta_env *env = ut_ctx->env;

	voluta_env_del(env);
	memset(ut_ctx, 0, sizeof(*ut_ctx));
}

struct voluta_ut_ctx *voluta_ut_new_ctx(void)
{
	struct voluta_ut_ctx *ut_ctx;

	ut_ctx = (struct voluta_ut_ctx *)malloc(sizeof(*ut_ctx));
	voluta_assert_not_null(ut_ctx);

	ut_ctx_init(ut_ctx);
	return ut_ctx;
}

void voluta_ut_del_ctx(struct voluta_ut_ctx *ut_ctx)
{
	voluta_ut_freeall(ut_ctx);
	ut_ctx_fini(ut_ctx);
	memset(ut_ctx, 0, sizeof(*ut_ctx));
	free(ut_ctx);
}

void *voluta_ut_zerobuf(struct voluta_ut_ctx *ut_ctx, size_t bsz)
{
	return voluta_ut_zalloc(ut_ctx, bsz);
}

void voluta_ut_randfill(struct voluta_ut_ctx *ut_ctx, void *buf, size_t bsz)
{
	voluta_getentropy(buf, bsz);
	voluta_unused(ut_ctx);
}

void *voluta_ut_randbuf(struct voluta_ut_ctx *ut_ctx, size_t bsz)
{
	uint8_t *buf = NULL;

	if (bsz > 0) {
		buf = voluta_ut_malloc(ut_ctx, bsz);
		voluta_ut_randfill(ut_ctx, buf, bsz);
	}
	return buf;
}

static void swap(long *arr, size_t p1, size_t p2)
{
	long tmp = arr[p1];

	arr[p1] = arr[p2];
	arr[p2] = tmp;
}

long *voluta_ut_randseq(struct voluta_ut_ctx *ut_ctx, size_t len, long base)
{
	long *arr;
	size_t *pos;

	arr = voluta_ut_zerobuf(ut_ctx, len * sizeof(*arr));
	for (size_t i = 0; i < len; ++i) {
		arr[i] = base++;
	}

	pos = voluta_ut_randbuf(ut_ctx, len * sizeof(*pos));
	for (size_t i = 0; i < len; ++i) {
		swap(arr, i, pos[i] % len);
	}
	return arr;
}

static void force_alnum(char *str, size_t len)
{
	int ch;
	size_t idx;
	const char *alt = "_0123456789abcdefghijklmnopqrstuvwxyz";

	for (size_t i = 0; i < len; ++i) {
		ch = str[i];
		if (!isalnum(ch)) {
			idx = (size_t)abs(ch);
			str[i] = alt[idx % strlen(alt)];
		}
	}
}

char *voluta_ut_randstr(struct voluta_ut_ctx *ut_ctx, size_t len)
{
	char *str;

	str = voluta_ut_randbuf(ut_ctx, len + 1);
	force_alnum(str, len);
	str[len] = '\0';
	return str;
}

char *voluta_ut_strfmt(struct voluta_ut_ctx *ut_ctx, const char *fmt, ...)
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
	buf = voluta_ut_zerobuf(ut_ctx, bsz + 1);
	nb = vsnprintf(buf, bsz, fmt, ap);
	va_end(ap);

	voluta_unused(nb);
	return buf;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct voluta_ut_iobuf *voluta_ut_new_iobuf(struct voluta_ut_ctx *ut_ctx,
		loff_t off, size_t len)
{
	size_t size;
	struct voluta_ut_iobuf *iobuf;

	size = (sizeof(*iobuf) + len - sizeof(iobuf->buf)) | 0x7;
	iobuf = voluta_ut_zerobuf(ut_ctx, size);
	iobuf->off = off;
	iobuf->len = len;
	voluta_ut_randfill(ut_ctx, iobuf->buf, len);
	return iobuf;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void swap_long(long *a, long *b)
{
	const long c = *a;

	*a = *b;
	*b = c;
}

/*
 * Pseudo-random shuffle, using hash function.
 * See: http://benpfaff.org/writings/clc/shuffle.html
 */
struct voluta_prandoms {
	uint64_t dat[16];
	size_t nr;
};

static uint64_t next_prandom(struct voluta_prandoms *pr)
{
	if (pr->nr == 0) {
		voluta_getentropy(pr->dat, sizeof(pr->dat));
		pr->nr = VOLUTA_ARRAY_SIZE(pr->dat);
	}
	pr->nr -= 1;
	return pr->dat[pr->nr];
}

static void prandom_shuffle(long *arr, size_t len)
{
	size_t j;
	uint64_t rnd;
	struct voluta_prandoms pr = { .nr = 0 };

	if (len < 2) {
		return;
	}
	for (size_t i = 0; i < len - 1; i++) {
		rnd = next_prandom(&pr);
		j = i + (rnd / (ULONG_MAX / (len - i) + 1));
		swap_long(arr + i, arr + j);
	}
}

static void create_seq(long *arr, size_t len, long base)
{
	for (size_t i = 0; i < len; ++i) {
		arr[i] = base++;
	}
}

/* Generates sequence of integers [base..base+n) and then random shuffle */
void voluta_ut_prandom_seq(long *arr, size_t len, long base)
{
	create_seq(arr, len, base);
	prandom_shuffle(arr, len);
}


