/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                             *
 *                            Funex File-System                                *
 *                                                                             *
 *  Copyright (C) 2014,2015 Synarete                                           *
 *                                                                             *
 *  Funex is a free software; you can redistribute it and/or modify it under   *
 *  the terms of the GNU General Public License as published by the Free       *
 *  Software Foundation, either version 3 of the License, or (at your option)  *
 *  any later version.                                                         *
 *                                                                             *
 *  Funex is distributed in the hope that it will be useful, but WITHOUT ANY   *
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS  *
 *  FOR A PARTICULAR PURPOSE. See the GNU General Public License for more      *
 *  details.                                                                   *
 *                                                                             *
 *  You should have received a copy of the GNU General Public License along    *
 *  with this program. If not, see <http://www.gnu.org/licenses/>              *
 *                                                                             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
#include <fnxconfig.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <error.h>
#include <signal.h>
#include <unistd.h>
#define UNW_LOCAL_ONLY
#include <libunwind.h>

#include "infra-compiler.h"
#include "infra-macros.h"
#include "infra-utility.h"
#include "infra-panic.h"

static void fnx_fatal_error(int errnum, const char *msg);


/*
 * User-provided hook for handling of fatal errors:
 */
static fnx_error_cb s_panic_error_fn = fnx_fatal_error;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* End process now! */
FNX_ATTR_NORETURN
void fnx_die(void)
{
	fflush(stdout);
	fflush(stderr);
	abort();
}

/* Default call-back: print error and abort. */
static void fnx_fatal_error(int errnum, const char *msg)
{
	char buf[1024];

	fnx_bzero(buf, sizeof(buf));
	fnx_backtrace_addr2line(buf, sizeof(buf));

	fprintf(stderr, "%s (errno=%d)\n", msg, errnum);
	fprintf(stderr, "%s\n", buf);
	fnx_die();
}

/* Helper function: format pretty panic message */
static void format_msg(char *msg, size_t sz,
                       const char *file, unsigned line,
                       const char *func, const char *prefix,
                       const char *fmt, va_list ap)
{
	int n = 0;

	fnx_bzero(msg, sz);
	if ((file != NULL) && (line > 0) && (strlen(file) < sz / 2)) {
		n += snprintf(msg, sz, "%s:%u: ", file, line);
	}
	if ((func != NULL) && (n < (int)sz)) {
		n += snprintf(msg + n, sz - (size_t)n, "%s: ", func);
	}
	if ((prefix != NULL) && (n < (int)sz)) {
		n += snprintf(msg + n, sz - (size_t)n, "%s: ", prefix);
	}

	if (n < (int)sz) {
		vsnprintf(msg + n, sz - (size_t)n, fmt, ap);
	} else {
		vsnprintf(msg, sz, fmt, ap);
	}
}

/* Panic -- halt process execution. */
FNX_ATTR_NORETURN
static void fnx_vpanic(int errnum, const char *file, unsigned line,
                       const char *func, const char *prefix,
                       const char *fmt, va_list ap)
{
	char msg[2048];
	const size_t sz = sizeof(msg);

	/* Make a nice human-readbale message */
	format_msg(msg, sz, file, line, func, prefix, fmt, ap);

	/* Let user's call-back do all the hassle; if none, use default & die */
	if (s_panic_error_fn != NULL) {
		s_panic_error_fn(errnum, msg);
	} else {
		fnx_fatal_error(errnum, msg);
	}
	fnx_die();
}

FNX_ATTR_NORETURN
FNX_ATTR_FMTPRINTF(4, 5)
void fnx_panicf(const char *file, unsigned line, const char *func,
                const char *fmt, ...)
{
	int errnum;
	va_list ap;

	errnum = errno;

	va_start(ap, fmt);
	fnx_vpanic(errnum, file, line, func, "*panic*", fmt, ap);
	va_end(ap);
}

/* Assertion failure call. */
FNX_ATTR_NORETURN
void fnx_assertion_failure(const char *file, unsigned line,
                           const char *func, const char *expr)
{
	fnx_panicf(file, line, func, "Assertion-failure `%s'", expr);
}


void fnx_set_panic_callback(fnx_error_cb fn)
{
	s_panic_error_fn = fn;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Poor-man's stack-tracing:
 *
 * When no good results with backtrace_symbols or other libunwind hooks (even
 * when compiled with -rdynamic to ld), then try having back-trace via
 * 'addr2line' command line utility.
 */
void fnx_backtrace_stackinfo(char *buf, size_t bsz)
{
	int rc, n, lim = 128;
	size_t k;
	void *ptr = NULL;
	char func[256], sufx[64];
	unw_word_t ip, sp, off;
	unw_context_t   context;
	unw_cursor_t    cursor;

	rc = unw_getcontext(&context);
	if (rc != UNW_ESUCCESS) {
		return;
	}
	rc = unw_init_local(&cursor, &context);
	if (rc != UNW_ESUCCESS) {
		return;
	}
	while (lim-- > 0) {
		ip = sp = off = 0;
		fnx_bzero(func, sizeof(func));
		fnx_bzero(sufx, sizeof(sufx));

		/* The unwind routines return negative values for error (unw_error_t),
		 * and a non-negative value on success */
		rc = unw_step(&cursor);
		if (rc < 0) {
			break;
		}
		rc = unw_get_reg(&cursor, UNW_REG_IP, &ip);
		if (rc < 0) {
			break;
		}
		rc = unw_get_reg(&cursor, UNW_REG_SP, &sp);
		if (rc < 0) {
			break;
		}
		rc = unw_get_proc_name(&cursor, func, sizeof(func) - 1, &off);
		if (rc < 0) {
			break;
		}
		if ((ptr = (void *)ip) == NULL) {
			break;
		}
		if (off > 0) {
			snprintf(sufx, sizeof(sufx), "+0x%lx", (long)off);
		}
		n = snprintf(buf, bsz, "[<%p>] %s%s\n", ptr, func, sufx);
		k = ((n < (int)bsz) && (n > 0)) ? (size_t)n : bsz;
		bsz -= k;
		buf += k;
	}
}

void fnx_backtrace_addr2line(char *buf, size_t bsz)
{
	int i, n, rc;
	size_t sz, len;
	char *ptr;
	void *bt_arr[64];
	char addr_list[512];
	const char *progname = program_invocation_name;

	sz = sizeof(bt_arr);
	fnx_bzero(bt_arr, sz);

	sz  = sizeof(addr_list);
	ptr = addr_list;
	fnx_bzero(ptr, sz);

	n = unw_backtrace(bt_arr, (int)(FNX_ARRAYSIZE(bt_arr)));
	for (i = 1; i < n - 2; ++i) {
		sz  = sizeof(addr_list);
		len = strlen(addr_list);
		ptr = addr_list + len;
		snprintf(ptr, sz - len - 1, "%p ", bt_arr[i]);
	}

	rc = snprintf(buf, bsz, "addr2line -a -C -e %s -f -p -s %s",
	              progname, addr_list);
	fnx_unused(rc);
}
