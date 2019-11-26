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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#define UNW_LOCAL_ONLY 1
#include <libunwind.h>

#include "voluta-lib.h"

/* TODO: be visible in header */
#define voluta_likely(x_)       __builtin_expect(!!(x_), 1)
#define voluta_unlikely(x_)     __builtin_expect(!!(x_), 0)

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_dump_backtrace(void)
{
	int err, lim = 64;
	char sym[256];
	unw_word_t ip, sp, off;
	unw_context_t context;
	unw_cursor_t cursor;

	err = unw_getcontext(&context);
	if (err != UNW_ESUCCESS) {
		return err;
	}
	err = unw_init_local(&cursor, &context);
	if (err != UNW_ESUCCESS) {
		return err;
	}
	while (lim-- > 0) {
		memset(sym, 0, sizeof(sym));
		ip = sp = off = 0;
		err = unw_step(&cursor);
		if (err <= 0) {
			return err;
		}
		err = unw_get_reg(&cursor, UNW_REG_IP, &ip);
		if (err) {
			return err;
		}
		err = unw_get_reg(&cursor, UNW_REG_SP, &sp);
		if (err) {
			return err;
		}
		err = unw_get_proc_name(&cursor, sym, sizeof(sym) - 1, &off);
		if (err) {
			return err;
		}
		voluta_tr_error("[<%p>] 0x%lx %s+0x%lx",
				(void *)ip, (long)sp, sym, (long)off);
	}
	return 0;
}

static void bt_addrs_to_str(char *buf, size_t bsz, void **bt_arr, int bt_len)
{
	size_t len;

	for (int i = 1; i < bt_len - 2; ++i) {
		len = strlen(buf);
		if ((len + 8) >= bsz) {
			break;
		}
		snprintf(buf + len, bsz - len, "%p ", bt_arr[i]);
	}
}

static void voluta_dump_addr2line(void)
{
	int bt_len, bt_cnt;
	void *bt_arr[128];
	char bt_addrs[1024];

	memset(bt_arr, 0, sizeof(bt_arr));
	memset(bt_addrs, 0, sizeof(bt_addrs));

	bt_cnt = (int)(VOLUTA_ARRAY_SIZE(bt_arr));
	bt_len = unw_backtrace(bt_arr, bt_cnt);
	bt_addrs_to_str(bt_addrs, sizeof(bt_addrs) - 1, bt_arr, bt_len);
	voluta_tr_error("addr2line -a -C -e %s -f -p -s %s",
			program_invocation_name, bt_addrs);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

voluta_decl_nonreturn_fn
static void voluta_abort(void)
{
	fflush(stdout);
	fflush(stderr);
	abort();
}

voluta_decl_nonreturn_fn
static void assertion_failure_at(const char *msg, const char *file, int line)
{
	voluta_panicf(file, line, "failure: `%s'", msg);
	voluta_abort();
}

void voluta_assert_if_(bool cond, const char *str, const char *file, int line)
{
	if (voluta_unlikely(!cond)) {
		assertion_failure_at(str, file, line);
	}
}

voluta_decl_nonreturn_fn
static void assertion_failure_op(long a, const char *op, long b,
				 const char *file, int line)
{
	char str[128] = "";

	snprintf(str, sizeof(str) - 1, "%ld %s %ld", a, op, b);
	assertion_failure_at(str, file, line);
}

void voluta_assert_eq_(long a, long b, const char *file, int line)
{
	if (voluta_unlikely(a != b)) {
		assertion_failure_op(a, "!=", b, file, line);
	}
}

void voluta_assert_ne_(long a, long b, const char *file, int line)
{
	if (voluta_unlikely(a == b)) {
		assertion_failure_op(a, "==", b, file, line);
	}
}

void voluta_assert_lt_(long a, long b, const char *file, int line)
{
	if (voluta_unlikely(a >= b)) {
		assertion_failure_op(a, ">=", b, file, line);
	}
}

void voluta_assert_le_(long a, long b, const char *file, int line)
{
	if (voluta_unlikely(a > b)) {
		assertion_failure_op(a, ">", b, file, line);
	}
}

void voluta_assert_gt_(long a, long b, const char *file, int line)
{
	if (voluta_unlikely(a <= b)) {
		assertion_failure_op(a, "<=", b, file, line);
	}
}

void voluta_assert_ge_(long a, long b, const char *file, int line)
{
	if (voluta_unlikely(a < b)) {
		assertion_failure_op(a, "<", b, file, line);
	}
}

static void assertion_failure(const char *file, int line, const char *fmt, ...)
{
	va_list ap;
	char str[512] = "";

	va_start(ap, fmt);
	vsnprintf(str, sizeof(str) - 1, fmt, ap);
	va_end(ap);

	assertion_failure_at(str, file, line);
}

void voluta_assert_ok_(int ok, const char *file, int line)
{
	if (voluta_unlikely(ok != 0)) {
		assertion_failure(file, line, "not ok: %d", ok);
	}
}

void voluta_assert_err_(int err, int exp, const char *file, int line)
{
	if (voluta_unlikely(err != exp)) {
		assertion_failure(file, line, "status %d != %d", err, exp);
	}
}

void voluta_assert_not_null_(const void *ptr, const char *file, int line)
{
	if (voluta_unlikely(ptr == NULL)) {
		assertion_failure_at("NULL pointer", file, line);
	}
}

void voluta_assert_null_(const void *ptr, const char *file, int line)
{
	if (voluta_unlikely(ptr != NULL)) {
		assertion_failure(file, line, "not NULL %p", ptr);
	}
}

void voluta_assert_eqs_(const char *s1, const char *s2,
			const char *file, int line)
{
	int cmp;

	cmp = strcmp(s1, s2);
	if (voluta_unlikely(cmp != 0)) {
		assertion_failure(file, line,
				  "strings-not-equal: %s != %s", s1, s2);
	}
}

static char nibble_to_ascii(unsigned int n)
{
	const char xdigits[] = "0123456789ABCDEF";

	return xdigits[n & 0xF];
}

static const char *
mem_to_str(const void *mem, size_t nn, char *str, size_t len)
{
	size_t pos = 0, i = 0;
	const uint8_t *ptr = mem;

	memset(str, 0, len);
	while ((i < nn) && ((pos + 4) < len)) {
		str[pos++] = nibble_to_ascii(ptr[i] >> 4);
		str[pos++] = nibble_to_ascii(ptr[i]);
		i += 1;
	}
	if (i < nn) {
		while ((pos + 2) < len) {
			str[pos++] = '.';
		}
	}
	return str;
}

void voluta_assert_eqm_(const void *m1, const void *m2, size_t nn,
			const char *file, int line)
{
	int cmp;
	char s1[36], s2[36];

	cmp = memcmp(m1, m2, nn);
	if (voluta_unlikely(cmp != 0)) {
		assertion_failure(file, line, "memory-not-equal: %s != %s ",
				  mem_to_str(m1, nn, s1, sizeof(s1)),
				  mem_to_str(m2, nn, s2, sizeof(s2)));
	}
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const char *basename_of(const char *file)
{
	const char *base = strrchr(file, '/');

	return voluta_likely(base) ? (base + 1) : file;
}

static void voluta_dump_panic_msg(const char *file, int line,
				  const char *msg, int errnum)
{
	const char *es = " ";
	const char *name = basename_of(file);

	voluta_tr_error("%s", es);
	if (errnum) {
		voluta_tr_error("%s:%d: %s %d", name, line, msg, errnum);
	} else {
		voluta_tr_error("%s:%d: %s", name, line, msg);
	}
	voluta_tr_error("%s", es);
}

voluta_decl_nonreturn_fn
void voluta_panicf(const char *file, int line, const char *fmt, ...)
{
	va_list ap;
	char msg[512] = "";
	const int errnum = errno;

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg) - 1, fmt, ap);
	va_end(ap);

	voluta_dump_panic_msg(file, line, msg, errnum);
	voluta_dump_backtrace();
	voluta_dump_addr2line();
	voluta_abort();
}
