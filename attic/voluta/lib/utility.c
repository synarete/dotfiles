/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * This file is part of libvoluta
 *
 * Copyright (C) 2020 Shachar Sharon
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
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <uuid/uuid.h>
#include "libvoluta.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t voluta_min(size_t x, size_t y)
{
	return min(x, y);
}

size_t voluta_max(size_t x, size_t y)
{
	return max(x, y);
}

size_t voluta_clamp(size_t v, size_t lo, size_t hi)
{
	return clamp(v, lo, hi);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void do_burnstack_n(size_t nbytes)
{
	char buf[1024];
	const size_t cnt = min(sizeof(buf), nbytes);

	if (cnt > 0) {
		memset(buf, 0xfb, cnt);
		do_burnstack_n(nbytes - cnt);
	}
}

void voluta_burnstack(void)
{
	const size_t page_size = voluta_sc_page_size();

	do_burnstack_n(page_size);
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

size_t voluta_popcount(unsigned int n)
{
	return n ? (size_t)__builtin_popcount(n) : 0;
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
/* UUID */
void voluta_uuid_generate(struct voluta_uuid *uuid)
{
	uuid_generate_random(uuid->uu);
}

void voluta_uuid_clone(const struct voluta_uuid *u1, struct voluta_uuid *u2)
{
	uuid_copy(u2->uu, u1->uu);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* monotonic clock */
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
/* random generator */
void voluta_getentropy(void *buf, size_t len)
{
	int err;
	size_t cnt;
	uint8_t *ptr = buf;
	const uint8_t *end = ptr + len;

	while (ptr < end) {
		cnt = min((size_t)(end - ptr), 256);
		err = getentropy(ptr, cnt);
		if (err) {
			voluta_panic("getentropy failed err=%d", errno);
		}
		ptr += cnt;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* mapped-memory */
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

void voluta_memzero(void *s, size_t n)
{
	memset(s, 0, n);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* logging */
int voluta_g_log_mask =
	(VOLUTA_LOG_ERROR | VOLUTA_LOG_CRIT | VOLUTA_LOG_STDOUT);

static void log_to_stdout(const char *msg)
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

static int syslog_level(int log_mask)
{
	int sl_level;

	if (log_mask & VOLUTA_LOG_CRIT) {
		sl_level = LOG_CRIT;
	} else if (log_mask & VOLUTA_LOG_ERROR) {
		sl_level = LOG_ERR;
	} else if (log_mask & VOLUTA_LOG_WARN) {
		sl_level = LOG_WARNING;
	} else if (log_mask & VOLUTA_LOG_INFO) {
		sl_level = LOG_INFO;
	} else if (log_mask & VOLUTA_LOG_DEBUG) {
		sl_level = LOG_DEBUG;
	} else {
		sl_level = 0;
	}
	return sl_level;
}

static void log_to_syslog(int log_mask, const char *msg)
{
	syslog(syslog_level(log_mask), "%s", msg);
}

static void log_msg(int log_mask, const char *msg)
{
	const int flags = (log_mask | voluta_g_log_mask);

	if (flags & VOLUTA_LOG_STDOUT) {
		log_to_stdout(msg);
	}
	if (flags & VOLUTA_LOG_SYSLOG) {
		log_to_syslog(log_mask, msg);
	}
}

void voluta_logf(int log_mask, const char *fmt, ...)
{
	va_list ap;
	size_t len;
	int saved_errno;
	char msg[512];

	if (log_mask & voluta_g_log_mask) {
		saved_errno = errno;
		va_start(ap, fmt);
		len = (size_t)vsnprintf(msg, sizeof(msg), fmt, ap);
		va_end(ap);
		len = min(len, sizeof(msg) - 1);
		msg[len] = '\0';
		log_msg(log_mask, msg);
		errno = saved_errno;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* buffer */
void voluta_buf_init(struct voluta_buf *buf, void *p, size_t n)
{
	buf->buf = p;
	buf->bsz = n;
	buf->len = 0;
}

size_t voluta_buf_append(struct voluta_buf *buf, const void *ptr, size_t len)
{
	size_t cnt;

	cnt = min(len, buf->bsz - buf->len);
	memcpy((char *)buf->buf + buf->len, ptr, cnt);
	buf->len += cnt;

	return cnt;
}

void voluta_buf_seteos(struct voluta_buf *buf)
{
	char *s = buf->buf;

	if (buf->len < buf->bsz) {
		s[buf->len] = '\0';
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* miscellaneous */
void voluta_ts_now(struct timespec *ts)
{
	ts->tv_sec = 0;
	ts->tv_nsec = UTIME_NOW;
}

void voluta_ts_copy(struct timespec *dst, const struct timespec *src)
{
	dst->tv_sec = src->tv_sec;
	dst->tv_nsec = src->tv_nsec;
}
