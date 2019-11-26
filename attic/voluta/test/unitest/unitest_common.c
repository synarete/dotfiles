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
	size_t total_size, mchunk_size, data_size;
	struct voluta_ut_malloc_chunk *mchunk = NULL;

	total_size = mchunk_size = sizeof(*mchunk);
	data_size = sizeof(mchunk->data);
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
	voluta_ut_assert_not_null(mchunk);

	mchunk->size = total_size;
	mchunk->next = ut_ctx->malloc_list;
	ut_ctx->malloc_list = mchunk;
	ut_ctx->nbytes_alloc += total_size;

	return mchunk;
}

static void ut_free(struct voluta_ut_ctx *ut_ctx,
		    struct voluta_ut_malloc_chunk *mchunk)
{
	voluta_ut_assert_ge(ut_ctx->nbytes_alloc, mchunk->size);

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
	struct voluta_ut_malloc_chunk *mchunk, *mnext;

	mchunk = ut_ctx->malloc_list;
	while (mchunk != NULL) {
		mnext = mchunk->next;
		ut_free(ut_ctx, mchunk);
		mchunk = mnext;
	}
	voluta_assert_eq(ut_ctx->nbytes_alloc, 0);

	ut_ctx->nbytes_alloc = 0;
	ut_ctx->malloc_list = NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_ut_assert_if(int cond, const char *desc, const char *file, int line)
{
	voluta_assert_if_(cond, desc, file, line);
}

void *voluta_ut_mmap_memory(size_t msz)
{
	int err;
	void *addr = NULL;

	err = voluta_sys_mmap_anon(msz, 0, &addr);
	voluta_ut_assert_ok(err);

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
	struct voluta_env *env = NULL;
	const struct voluta_ucred ucred = {
		.uid = getuid(),
		.gid = getgid(),
		.pid = getpid(),
		.umask = 0002
	};

	memset(ut_ctx, 0, sizeof(*ut_ctx));
	ut_ctx->silent = false;
	ut_ctx->malloc_list = NULL;
	ut_ctx->nbytes_alloc = 0;

	err = voluta_env_new(&env);
	voluta_ut_assert_ok(err);

	voluta_env_setcreds(env, &ucred);
	voluta_env_setmode(env, true);
	ut_ctx->env = env;
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
	voluta_ut_assert_not_null(ut_ctx);

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
	uint8_t *buf;

	buf = voluta_ut_malloc(ut_ctx, bsz);
	voluta_ut_randfill(ut_ctx, buf, bsz);
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
	size_t i, *pos;

	arr = voluta_ut_zerobuf(ut_ctx, len * sizeof(*arr));
	for (i = 0; i < len; ++i) {
		arr[i] = base++;
	}

	pos = voluta_ut_randbuf(ut_ctx, len * sizeof(*pos));
	for (i = 0; i < len; ++i) {
		swap(arr, i, pos[i] % len);
	}
	return arr;
}

static void force_alnum(char *str, size_t len)
{
	int ch;
	size_t idx;
	const char *alt = "_0123456789_abcdefghijklmnopqrstuvwxyz_";

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
/*
 * Generate pseudo-random priority-value.
 * Don't care about thread-safety here -- its random...
 *
 * See also: www0.cs.ucl.ac.uk/staff/d.jones/GoodPracticeRNG.pdf
 * Does is it make real random? no! but for unit-tests its good enough...
 */
static unsigned long marsaglia_prng(void)
{
	/* Seed variables */
	static uint64_t x = 123456789ULL;
	static uint64_t y = 362436000ULL;
	static uint64_t z = 521288629ULL;
	static uint64_t c = 7654321ULL;

	uint64_t t, a = 698769069ULL;

	x = 69069 * x + 12345;
	y ^= (y << 13);
	y ^= (y >> 17);
	y ^= (y << 5); /* y must never be set to zero! */
	t = a * z + c;
	c = (t >> 32); /* Also avoid setting z=c=0! */
	return (unsigned long)(x + y + (z = t));
}

static uint64_t xor_next_prng(uint64_t a)
{
	return a ^ marsaglia_prng();
}

static uint64_t rotate(uint64_t v, unsigned int n)
{
	return ((v << n) | (v >> (64 - n)));
}

static void next_prandoms(uint64_t dat[8], long seed)
{
	const uint64_t useed = ((uint64_t)seed);

	dat[0] = xor_next_prng(0x6A09E667UL + rotate(useed, 1));
	dat[1] = xor_next_prng(0xBB67AE85UL - rotate(useed, 2));
	dat[2] = xor_next_prng(0x3C6EF372UL ^ rotate(useed, 3));
	dat[3] = xor_next_prng(0xA54FF53AUL * rotate(useed, 5));
	dat[4] = xor_next_prng(0x510E527FUL + ~rotate(useed, 7));
	dat[5] = xor_next_prng(0x9B05688CUL - ~rotate(useed, 11));
	dat[6] = xor_next_prng(0x1F83D9ABUL ^ ~rotate(useed, 13));
	dat[7] = xor_next_prng(0x5BE0CD19UL * ~rotate(useed, 19));
}

static void next_prandoms_time(uint64_t dat[8])
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	next_prandoms(dat, (long)tv.tv_sec ^ (long)tv.tv_usec);
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
static uint64_t next_prandom(uint64_t dat[8], size_t *nr)
{
	if (*nr == 0) {
		next_prandoms_time(dat);
		*nr = 8;
	}
	*nr -= 1;
	return dat[*nr];
}

static void prandom_shuffle(long *arr, size_t len)
{
	size_t i, j, nr = 0;
	uint64_t rnd, dat[8];

	if (len < 2) {
		return;
	}
	for (i = 0; i < len - 1; i++) {
		rnd = next_prandom(dat, &nr);
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


