/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * This file is part of libvoluta
 *
 * Copyright (C) 2019 Shachar Sharon
 *
 * Libvoluta is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Libvoluta is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 */
#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <uuid/uuid.h>
#include "voluta-lib.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Common */

size_t voluta_min(size_t x, size_t y)
{
	return (x < y) ? x : y;
}

size_t voluta_min3(size_t x, size_t y, size_t z)
{
	return voluta_min(voluta_min(x, y), z);
}

size_t voluta_max(size_t x, size_t y)
{
	return (x > y) ? x : y;
}

size_t voluta_clamp(size_t v, size_t lo, size_t hi)
{
	return voluta_min(voluta_max(v, lo), hi);
}

static void voluta_burnstack_n(size_t nbytes)
{
	char buf[1024];
	const size_t cnt = voluta_min(sizeof(buf), nbytes);

	if (cnt > 0) {
		memset(buf, 0xfe, cnt);
		voluta_burnstack_n(nbytes - cnt);
	}
}

void voluta_burnstack(void)
{
	const size_t page_size = voluta_sc_page_size();

	voluta_burnstack_n(page_size);
}

/* 3-way compare functions when using long-integers as keys */
int voluta_compare3(long x, long y)
{
	int res = 0;

	if (x < y) {
		res = -1;
	} else if (x > y) {
		res = 1;
	}
	return res;
}

size_t voluta_clz(unsigned int n)
{
	return n ? (size_t)__builtin_clz(n) : 32;
}


size_t voluta_sc_l1_dcache_linesize(void)
{
	return (size_t)sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
}

size_t voluta_sc_page_size(void)
{
	return (size_t)sysconf(_SC_PAGE_SIZE);
}

size_t voluta_sc_phys_pages(void)
{
	return (size_t)sysconf(_SC_PHYS_PAGES);
}

size_t voluta_sc_avphys_pages(void)
{
	return (size_t)sysconf(_SC_AVPHYS_PAGES);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Linked-list */

static void list_head_set(struct voluta_list_head *lnk,
			  struct voluta_list_head *prv,
			  struct voluta_list_head *nxt)
{
	lnk->next = nxt;
	lnk->prev = prv;
}

static void list_head_insert(struct voluta_list_head *lnk,
			     struct voluta_list_head *prv,
			     struct voluta_list_head *nxt)
{
	list_head_set(lnk, prv, nxt);

	nxt->prev = lnk;
	prv->next = lnk;
}

void voluta_list_head_insert_after(struct voluta_list_head *prev_lnk,
				   struct voluta_list_head *lnk)
{
	list_head_insert(lnk, prev_lnk, prev_lnk->next);
}

void voluta_list_head_insert_before(struct voluta_list_head *lnk,
				    struct voluta_list_head *next_lnk)
{
	list_head_insert(lnk, next_lnk->prev, next_lnk);
}

void voluta_list_head_remove(struct voluta_list_head *lnk)
{
	struct voluta_list_head *next = lnk->next, *prev = lnk->prev;

	next->prev = prev;
	prev->next = next;
	list_head_set(lnk, lnk, lnk);
}

void voluta_list_head_init(struct voluta_list_head *lnk)
{
	list_head_set(lnk, lnk, lnk);
}

void voluta_list_head_initn(struct voluta_list_head *lnk_arr, size_t cnt)
{
	for (size_t i = 0; i < cnt; ++i) {
		voluta_list_head_init(&lnk_arr[i]);
	}
}

void voluta_list_head_destroy(struct voluta_list_head *lnk)
{
	list_head_set(lnk, NULL, NULL);
}

void voluta_list_head_destroyn(struct voluta_list_head *lnk_arr, size_t cnt)
{
	for (size_t i = 0; i < cnt; ++i) {
		voluta_list_head_destroy(&lnk_arr[i]);
	}
}

void voluta_list_init(struct voluta_list_head *lst)
{
	voluta_list_head_init(lst);
}

void voluta_list_destroy(struct voluta_list_head *lst)
{
	voluta_list_head_destroy(lst);
}

void voluta_list_push_front(struct voluta_list_head *lst,
			    struct voluta_list_head *lnk)
{
	voluta_list_head_insert_after(lst, lnk);
}

void voluta_list_push_back(struct voluta_list_head *lst,
			   struct voluta_list_head *lnk)
{
	voluta_list_head_insert_before(lnk, lst);
}

struct voluta_list_head *voluta_list_pop_front(struct voluta_list_head *lst)
{
	struct voluta_list_head *lnk = lst->next;

	if (lnk != lst) {
		voluta_list_head_remove(lnk);
	} else {
		lnk = NULL;
	}
	return lnk;
}

bool voluta_list_isempty(const struct voluta_list_head *lst)
{
	return (lst->next == lst);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* UUID */

void voluta_uuid_generate(struct voluta_uuid *uuid)
{
	uuid_generate_random(uuid->uu);
}

void voluta_uuid_clone(const struct voluta_uuid *uuid,
		       struct voluta_uuid *other)
{
	uuid_copy(other->uu, uuid->uu);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Monotonic clock */
void voluta_mclock_now(struct timespec *ts)
{
	int err;

	err = voluta_sys_clock_gettime(CLOCK_MONOTONIC, ts);
	if (err) {
		voluta_panic("clock_gettime err=%d", err);
	}
}

static void mclock_dif(const struct timespec *beg,
		       const struct timespec *end, struct timespec *dif)
{
	dif->tv_sec = end->tv_sec - beg->tv_sec;
	if (end->tv_nsec >= beg->tv_nsec) {
		dif->tv_nsec = end->tv_nsec - beg->tv_nsec;
	} else {
		dif->tv_sec -= 1;
		dif->tv_nsec = beg->tv_nsec - end->tv_nsec;
	}
}

void voluta_mclock_dur(const struct timespec *start, struct timespec *dur)
{
	struct timespec now;

	voluta_mclock_now(&now);
	mclock_dif(start, &now, dur);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Random generator */

void voluta_getentropy(void *buf, size_t len)
{
	int err;
	size_t cnt;
	uint8_t *ptr = buf;
	const uint8_t *end = ptr + len;

	while (ptr < end) {
		cnt = voluta_min((size_t)(end - ptr), 256);
		err = getentropy(ptr, cnt);
		if (err) {
			voluta_panic("getentropy failed err=%d", errno);
		}
		ptr += cnt;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Memory */

static size_t size_to_page_up(size_t sz)
{
	const size_t page_size = voluta_sc_page_size();

	return ((sz + page_size - 1) / page_size) * page_size;
}

int voluta_mmap_memory(size_t msz, void **mem)
{
	const size_t size = size_to_page_up(msz);

	return voluta_sys_mmap_anon(size, 0, mem);
}

int voluta_mmap_secure_memory(size_t msz, void **mem)
{
	int err;
	const size_t size = size_to_page_up(msz);

	err = voluta_sys_mmap_anon(size, 0, mem);
	if (err) {
		return err;
	}
	err = voluta_sys_madvise(*mem, size, MADV_DONTDUMP);
	if (err) {
		voluta_munmap_memory(*mem, size);
		return err;
	}
	/* TODO: check error of mlock2 when possible by getrlimit */
	voluta_sys_mlock2(*mem, size, MLOCK_ONFAULT);
	if (err) {
		voluta_munmap_memory(*mem, size);
		return err;
	}
	return 0;
}

void voluta_munmap_memory(void *mem, size_t msz)
{
	if (mem) {
		voluta_sys_munmap(mem, size_to_page_up(msz));
	}
}

void voluta_munmap_secure_memory(void *mem, size_t msz)
{
	if (mem) {
		/* TODO: enable if done mlock
		voluta_sys_munlock(mem, msz);
		*/
		voluta_sys_munmap(mem, msz);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Traces */

int voluta_g_trace_flags =
	(VOLUTA_TRACE_ERROR | VOLUTA_TRACE_CRIT | VOLUTA_TRACE_STDOUT);

static void trace_stdout(const char *msg)
{
	FILE *fp = stdout;
	const char *prog = program_invocation_short_name;

	flockfile(fp);
	fputs(prog, fp);
	fputs(": ", fp);
	fputs(msg, fp);
	fputs("\n", fp);
	funlockfile(fp);
}

static int syslog_level(int tr_mask)
{
	int sl_level;

	if (tr_mask & VOLUTA_TRACE_CRIT) {
		sl_level = LOG_CRIT;
	} else if (tr_mask & VOLUTA_TRACE_ERROR) {
		sl_level = LOG_ERR;
	} else if (tr_mask & VOLUTA_TRACE_WARN) {
		sl_level = LOG_WARNING;
	} else if (tr_mask & VOLUTA_TRACE_INFO) {
		sl_level = LOG_INFO;
	} else if (tr_mask & VOLUTA_TRACE_DEBUG) {
		sl_level = LOG_DEBUG;
	} else {
		sl_level = 0;
	}
	return sl_level;
}

static void trace_syslog(int tr_mask, const char *msg)
{
	syslog(syslog_level(tr_mask), "%s", msg);
}

static void vtracef(int tr_mask, const char *fmt, va_list ap)
{
	int len, tr_flags = (tr_mask | voluta_g_trace_flags);
	char msg[1024];

	len = vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	if (len >= (int)sizeof(msg)) {
		msg[sizeof(msg) - 1] = '\0';
	}
	if (tr_flags & VOLUTA_TRACE_STDOUT) {
		trace_stdout(msg);
	}
	if (tr_flags & VOLUTA_TRACE_SYSLOG) {
		trace_syslog(tr_mask, msg);
	}
}

void voluta_tracef(int tr_mask, const char *fmt, ...)
{
	va_list ap;
	int saved_errno;

	if (tr_mask & voluta_g_trace_flags) {
		saved_errno = errno;
		va_start(ap, fmt);
		vtracef(tr_mask, fmt, ap);
		va_end(ap);
		errno = saved_errno;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Miscellaneous */

void voluta_copy_timespec(struct timespec *dst, const struct timespec *src)
{
	dst->tv_sec = src->tv_sec;
	dst->tv_nsec = src->tv_nsec;
}

