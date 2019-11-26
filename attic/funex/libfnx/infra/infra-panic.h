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
#ifndef FNX_INFRA_PANIC_H_
#define FNX_INFRA_PANIC_H_

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Handle fatal errors which prevents further execution of the application. By
 * default, prints error strings and abort. The user may customize with his own
 * panic-hook call-back.
 */
#define FNX_ASSERT(expr) \
	do { if (fnx_unlikely(!(expr))) fnx_assertion_failure( \
			        __FILE__, __LINE__, NULL, /* FNX_FUNCTION */ \
			        FNX_MAKESTR(expr)); } while(0)

#define FNX_PANIC(fmt, ...)                                 \
	fnx_panicf(__FILE__, __LINE__, FNX_FUNCTION, fmt, __VA_ARGS__)


#define fnx_assert(expr)        FNX_ASSERT(expr)
#define fnx_assert_ok(rc)       fnx_assert((rc) == 0)
#define fnx_panic(fmt, ...)     FNX_PANIC(fmt, __VA_ARGS__)


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * End process with abort.
 */
FNX_ATTR_NORETURN
void fnx_die(void);

/*
 * Panic -- halt process execution.
 */
FNX_ATTR_NORETURN
FNX_ATTR_FMTPRINTF(4, 5)
void fnx_panicf(const char *, unsigned, const char *, const char *, ...);

/*
 * Assertion failure, which calls panic.
 */
FNX_ATTR_NORETURN
void fnx_assertion_failure(const char *, unsigned, const char *, const char *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

typedef void (*fnx_error_cb)(int, const char *);

/*
 * Set user's hook for run-time fatal errors
 */
void fnx_set_panic_callback(fnx_error_cb fn);


/*
 * Two ways to stack-trace:
 * 1. Via libunwind + logging.
 * 2. Via command line utility 'addr2line' (let user have a string ready for
 *    copy-paste into command line).
 */
void fnx_backtrace_stackinfo(char *buf, size_t bsz);

void fnx_backtrace_addr2line(char *buf, size_t bsz);

#endif /* FNX_INFRA_PANIC_H_ */




