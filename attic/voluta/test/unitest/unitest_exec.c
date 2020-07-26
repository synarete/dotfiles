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
#include "unitest.h"

#define UT_DEFTGRP(t_) \
	{ .tests = &(t_), .name = VOLUTA_STR(t_) }

static const struct ut_tgroup g_ut_tgroups[] = {
	UT_DEFTGRP(ut_test_avl),
	UT_DEFTGRP(ut_test_qalloc),
	UT_DEFTGRP(ut_test_super),
	UT_DEFTGRP(ut_test_dir),
	UT_DEFTGRP(ut_test_dir_iter),
	UT_DEFTGRP(ut_test_dir_list),
	UT_DEFTGRP(ut_test_namei),
	UT_DEFTGRP(ut_test_rename),
	UT_DEFTGRP(ut_test_symlink),
	UT_DEFTGRP(ut_test_xattr),
	UT_DEFTGRP(ut_test_ioctl),
	UT_DEFTGRP(ut_test_file_basic),
	UT_DEFTGRP(ut_test_file_ranges),
	UT_DEFTGRP(ut_test_file_records),
	UT_DEFTGRP(ut_test_file_random),
	UT_DEFTGRP(ut_test_file_edges),
	UT_DEFTGRP(ut_test_file_truncate),
	UT_DEFTGRP(ut_test_file_fallocate),
	UT_DEFTGRP(ut_test_file_lseek),
	UT_DEFTGRP(ut_test_file_fiemap),
	UT_DEFTGRP(ut_test_fs_reload),
	UT_DEFTGRP(ut_test_fs_fill),
};

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_track_test(struct ut_env *ut_env,
			  const struct ut_testdef *td, bool pre_execute)
{
	FILE *fp = stdout;
	struct timespec dur;

	if (ut_env->silent) {
		return;
	}
	if (pre_execute) {
		fprintf(fp, "  %-32s", td->name);
		voluta_mclock_now(&ut_env->ts_start);
	} else {
		voluta_mclock_dur(&ut_env->ts_start, &dur);
		fprintf(fp, "OK (%ld.%03lds)\n",
			dur.tv_sec, dur.tv_nsec / 1000000L);
	}
	fflush(fp);
}

static void ut_check_statvfs(const struct statvfs *stv1,
			     const struct statvfs *stv2)
{
	ut_assert_valid_statvfs(stv1);
	ut_assert_valid_statvfs(stv2);
	ut_assert_eq_statvfs(stv1, stv2);
}

static size_t ualloc_nbytes_now(const struct ut_env *ut_env)
{
	struct voluta_stats st;

	voluta_fs_env_stats(ut_env->fs_env, &st);
	return st.nalloc_bytes;
}

static void ut_probe_stats(struct ut_env *ut_env, bool pre_execute)
{
	size_t ualloc_now, seg_sz = VOLUTA_SEG_SIZE;
	struct statvfs stvfs_now;

	if (pre_execute) {
		ut_statfs_rootd(ut_env, &ut_env->stvfs_start);
		ut_drop_caches_fully(ut_env);
		ut_env->ualloc_start = ualloc_nbytes_now(ut_env);
	} else {
		ut_statfs_rootd(ut_env, &stvfs_now);
		ut_check_statvfs(&ut_env->stvfs_start, &stvfs_now);
		ut_drop_caches_fully(ut_env);
		ualloc_now = ualloc_nbytes_now(ut_env);
		/* XXX ut_assert_eq(ut_env->ualloc_start, ualloc_now); */
		ut_assert_ge(ut_env->ualloc_start + (2 * seg_sz), ualloc_now);
	}
}

static bool ut_is_runnable(const struct ut_env *ut_env,
			   const struct ut_testdef *td)
{
	return (!ut_env->test_name || strstr(td->name, ut_env->test_name));
}

static void ut_run_tgroup(struct ut_env *ut_env,
			  const struct ut_tgroup *tgroup)
{
	const struct ut_testdef *td;

	for (size_t i = 0; i < tgroup->tests->len; ++i) {
		td = &tgroup->tests->arr[i];
		if (ut_is_runnable(ut_env, td)) {
			ut_track_test(ut_env, td, true);
			ut_probe_stats(ut_env, true);
			td->hook(ut_env);
			ut_probe_stats(ut_env, false);
			ut_track_test(ut_env, td, false);
			ut_freeall(ut_env);
		}
	}
}

static void ut_exec_tgroups(struct ut_env *ut_env)
{
	for (size_t i = 0; i < UT_ARRAY_SIZE(g_ut_tgroups); ++i) {
		ut_run_tgroup(ut_env, &g_ut_tgroups[i]);
	}
}

static struct ut_env *ut_create_ctx(const char *test_name)
{
	int err;
	struct ut_env *ut_env;

	ut_env = ut_new_ctx();
	ut_env->test_name = test_name;

	err = voluta_fs_env_load(ut_env->fs_env);
	voluta_assert_ok(err);

	return ut_env;
}

static void ut_exec_tests(const char *test_name)
{
	struct ut_env *ut_env;

	ut_env = ut_create_ctx(test_name);
	ut_exec_tgroups(ut_env);
	ut_del_ctx(ut_env);
}

void ut_execute(const char *test_name)
{
	int err;

	err = voluta_lib_init();
	voluta_assert_ok(err);

	ut_exec_tests(test_name);
}

/*: : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : :*/


static size_t aligned_size(size_t sz, size_t a)
{
	return ((sz + a - 1) / a) * a;
}

static size_t malloc_total_size(size_t nbytes)
{
	size_t total_size;
	struct ut_malloc_chunk *mchunk = NULL;
	const size_t mchunk_size = sizeof(*mchunk);
	const size_t data_size = sizeof(mchunk->data);

	total_size = mchunk_size;
	if (nbytes > data_size) {
		total_size += aligned_size(nbytes - data_size, mchunk_size);
	}
	return total_size;
}

static struct ut_malloc_chunk *
ut_malloc_chunk(struct ut_env *ut_env, size_t nbytes)
{
	size_t total_size;
	struct ut_malloc_chunk *mchunk;

	total_size = malloc_total_size(nbytes);
	mchunk = (struct ut_malloc_chunk *)malloc(total_size);
	voluta_assert_not_null(mchunk);

	mchunk->size = total_size;
	mchunk->next = ut_env->malloc_list;
	ut_env->malloc_list = mchunk;
	ut_env->nbytes_alloc += total_size;

	return mchunk;
}

static void ut_free(struct ut_env *ut_env,
		    struct ut_malloc_chunk *mchunk)
{
	voluta_assert_ge(ut_env->nbytes_alloc, mchunk->size);

	ut_env->nbytes_alloc -= mchunk->size;
	memset(mchunk, 0xFC, mchunk->size);
	free(mchunk);
}

void *ut_malloc(struct ut_env *ut_env, size_t nbytes)
{
	struct ut_malloc_chunk *mchunk;

	mchunk = ut_malloc_chunk(ut_env, nbytes);
	return mchunk->data;
}

void *ut_zalloc(struct ut_env *ut_env, size_t nbytes)
{
	void *ptr;

	ptr = ut_malloc(ut_env, nbytes);
	memset(ptr, 0, nbytes);

	return ptr;
}

char *ut_strdup(struct ut_env *ut_env, const char *str)
{
	return ut_strndup(ut_env, str, strlen(str));
}

char *ut_strndup(struct ut_env *ut_env, const char *str,
		 size_t len)
{
	char *str2;

	str2 = ut_zalloc(ut_env, len + 1);
	memcpy(str2, str, len);

	return str2;
}

void ut_freeall(struct ut_env *ut_env)
{
	struct ut_malloc_chunk *mnext;
	struct ut_malloc_chunk *mchunk = ut_env->malloc_list;

	while (mchunk != NULL) {
		mnext = mchunk->next;
		ut_free(ut_env, mchunk);
		mchunk = mnext;
	}
	voluta_assert_eq(ut_env->nbytes_alloc, 0);

	ut_env->nbytes_alloc = 0;
	ut_env->malloc_list = NULL;
}

const char *ut_make_name(struct ut_env *ut_env, const char *pre, size_t idx)
{
	const char *name;

	if (pre && strlen(pre)) {
		name = ut_strfmt(ut_env, "%s-%lx", pre, idx + 1);
	} else {
		name = ut_strfmt(ut_env, "%lx", idx + 1);
	}
	return name;
}
/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_env_init(struct ut_env *ut_env)
{
	int err;
	size_t len;
	size_t psz;
	time_t now = time(NULL);
	struct voluta_fs_env *env = NULL;
	struct voluta_params params = {
		.ucred = {
			.uid = getuid(),
			.gid = getgid(),
			.pid = getpid(),
			.umask = 0002
		},
		.mount_point = "/",
		.volume_path = NULL,
		.volume_name = "voluta-tmpfs",
		.volume_size = VOLUTA_VOLUME_SIZE_MIN,
		.passphrase = ut_env->pass,
		.salt = ut_env->salt,
		.tmpfs = true,
		.pedantic = true
	};

	memset(ut_env, 0, sizeof(*ut_env));
	ut_env->silent = false;
	ut_env->malloc_list = NULL;
	ut_env->nbytes_alloc = 0;
	ut_env->unique_count = 1;

	psz = sizeof(ut_env->pass);
	len = voluta_clamp((size_t)now % psz, 1, psz - 1);
	err = voluta_make_ascii_passphrase(ut_env->pass, len);
	voluta_assert_ok(err);

	psz = sizeof(ut_env->salt);
	len = voluta_clamp(++len % psz, 1, psz - 1);
	err = voluta_make_ascii_passphrase(ut_env->salt, len);
	voluta_assert_ok(err);

	err = voluta_fs_env_new(&env);
	voluta_assert_ok(err);

	err = voluta_fs_env_setparams(env, &params);
	voluta_assert_ok(err);

	ut_env->fs_env = env;
	ut_env->volume_size = params.volume_size;
}

static void ut_env_fini(struct ut_env *ut_env)
{
	struct voluta_fs_env *fs_env = ut_env->fs_env;

	voluta_fs_env_term(fs_env);
	voluta_fs_env_del(fs_env);
	memset(ut_env, 0, sizeof(*ut_env));
}

struct ut_env *ut_new_ctx(void)
{
	struct ut_env *ut_env;

	ut_env = (struct ut_env *)malloc(sizeof(*ut_env));
	voluta_assert_not_null(ut_env);

	ut_env_init(ut_env);
	return ut_env;
}

void ut_del_ctx(struct ut_env *ut_env)
{
	ut_freeall(ut_env);
	ut_env_fini(ut_env);
	memset(ut_env, 0, sizeof(*ut_env));
	free(ut_env);
}

void *ut_zerobuf(struct ut_env *ut_env, size_t bsz)
{
	return ut_zalloc(ut_env, bsz);
}

void ut_randfill(struct ut_env *ut_env, void *buf, size_t bsz)
{
	voluta_getentropy(buf, bsz);
	voluta_unused(ut_env);
}

void *ut_randbuf(struct ut_env *ut_env, size_t bsz)
{
	uint8_t *buf = NULL;

	if (bsz > 0) {
		buf = ut_malloc(ut_env, bsz);
		ut_randfill(ut_env, buf, bsz);
	}
	return buf;
}

static void swap(long *arr, size_t p1, size_t p2)
{
	long tmp = arr[p1];

	arr[p1] = arr[p2];
	arr[p2] = tmp;
}

long *ut_randseq(struct ut_env *ut_env, size_t len, long base)
{
	long *arr;
	size_t *pos;

	arr = ut_zerobuf(ut_env, len * sizeof(*arr));
	for (size_t i = 0; i < len; ++i) {
		arr[i] = base++;
	}

	pos = ut_randbuf(ut_env, len * sizeof(*pos));
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

char *ut_randstr(struct ut_env *ut_env, size_t len)
{
	char *str;

	str = ut_randbuf(ut_env, len + 1);
	force_alnum(str, len);
	str[len] = '\0';
	return str;
}

char *ut_strfmt(struct ut_env *ut_env, const char *fmt, ...)
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
	buf = ut_zerobuf(ut_env, bsz + 1);
	nb = vsnprintf(buf, bsz, fmt, ap);
	va_end(ap);

	voluta_unused(nb);
	return buf;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct ut_dvec *ut_new_dvec(struct ut_env *ut_env,
			    loff_t off, size_t len)
{
	size_t size;
	struct ut_dvec *dvec;

	size = (sizeof(*dvec) + len - sizeof(dvec->dat)) | 0x7;
	dvec = ut_zerobuf(ut_env, size);
	dvec->off = off;
	dvec->len = len;
	ut_randfill(ut_env, dvec->dat, len);
	return dvec;
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
		pr->nr = UT_ARRAY_SIZE(pr->dat);
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
void ut_prandom_seq(long *arr, size_t len, long base)
{
	create_seq(arr, len, base);
	prandom_shuffle(arr, len);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static bool ut_equal_strings(const char *s1, const char *s2)
{
	return (strcmp(s1, s2) == 0);
}

bool ut_dot_or_dotdot(const char *s)
{
	bool ret = false;

	if (ut_equal_strings(s, ".")) {
		ret = true;
	} else if (ut_equal_strings(s, "..")) {
		ret = true;
	}
	return ret;
}

bool ut_not_dot_or_dotdot(const char *s)
{
	bool ret = true;

	if (ut_equal_strings(s, ".")) {
		ret = false;
	} else if (ut_equal_strings(s, "..")) {
		ret = false;
	}
	return ret;
}


