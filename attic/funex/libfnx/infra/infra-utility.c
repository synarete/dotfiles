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
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "infra-compiler.h"
#include "infra-macros.h"
#include "infra-panic.h"
#include "infra-utility.h"


const char *fnx_execname = NULL;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
FNX_ATTR_PURE
size_t fnx_min(size_t x, size_t y)
{
	return FNX_MIN(x, y);
}

FNX_ATTR_PURE
size_t fnx_max(size_t x, size_t y)
{
	return FNX_MAX(x, y);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Allocations wrappers:
 */
static void *stdlib_malloc(size_t n, void *hint)
{
	fnx_unused(hint);
	return malloc(n);
}

static void stdlib_free(void *p, size_t n, void *hint)
{
	free(p);
	fnx_unused(hint);
	fnx_unused(n);
}

/* Default allocator, mapped to std allocation functions. */
static fnx_alloc_if_t s_default_allocator = {
	.malloc = stdlib_malloc,
	.free   = stdlib_free,
	.userp  = NULL
};
fnx_alloc_if_t *fnx_mallocator = &s_default_allocator;


void *fnx_allocate(const fnx_alloc_if_t *alloc, size_t n)
{
	return alloc->malloc(n, alloc->userp);
}

void fnx_deallocate(const fnx_alloc_if_t *alloc, void *p, size_t n)
{
	alloc->free(p, n, alloc->userp);
}

/*
 * Wrappers over STD malloc/free
 */
void *fnx_malloc(size_t n)
{
	const fnx_alloc_if_t *alloc = &s_default_allocator;
	return fnx_allocate(alloc, n);
}

void fnx_free(void *p, size_t n)
{
	const fnx_alloc_if_t *alloc = &s_default_allocator;
	fnx_deallocate(alloc, p, n);
}

void *fnx_xmalloc(size_t n, int flags)
{
	void *p;

	p = fnx_malloc(n);
	if (p != NULL) {
		if (flags & FNX_BZERO) {
			fnx_bzero(p, n);
		}
	} else {
		if (flags & FNX_NOFAIL) {
			fnx_panic("malloc-failure n=%ld", (long)n);
		}
	}
	return p;
}


void fnx_xfree(void *p, size_t n, int flags)
{
	if (flags & FNX_BZERO) {
		fnx_bzero(p, n);
	}
	fnx_free(p, n);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

FNX_ATTR_NONNULL
void fnx_bzero(void *p, size_t n)
{
	memset(p, 0x00, n);
}

FNX_ATTR_NONNULL
void fnx_bff(void *p, size_t n)
{
	memset(p, 0xFF, n);
}

FNX_ATTR_NONNULL
void fnx_bcopy(void *t, const void *s, size_t n)
{
	memcpy(t, s, n);
}

FNX_ATTR_NONNULL
int fnx_bcmp(void *t, const void *s, size_t n)
{
	return memcmp(t, s, n);
}


void fnx_burnstack(int n)
{
	char buf[512];

	if (n > 0) {
		fnx_burnstack(n - 1);
	}
	fnx_bff(buf, sizeof(buf));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int resolve_execname(char *buf, size_t bufsz)
{
	ssize_t res;
	const char *prog;

	fnx_bzero(buf, bufsz);

	/* Linux specific */
	res = readlink("/proc/self/exe", buf, bufsz - 1);
	if (res > 0) {
		return 0;
	}

	/* BASH specific */
	prog = getenv("_");
	if ((prog != NULL) && (strlen(prog) < bufsz)) {
		strncpy(buf, prog, bufsz);
		return 0;
	}

	/* GNU */
	prog = program_invocation_name;
	if ((prog != NULL) && (strlen(prog) < bufsz)) {
		strncpy(buf, prog, bufsz);
		return 0;
	}
	prog = program_invocation_short_name;
	if ((prog != NULL) && (strlen(prog) < bufsz)) {
		strncpy(buf, prog, bufsz);
		return 0;
	}

	/* No good */
	return -1;
}


void fnx_setup_execname(void)
{
	int rc;
	static char s_execname[1024];

	rc = resolve_execname(s_execname, sizeof(s_execname));
	if (rc == 0) {
		fnx_execname = s_execname;
	} else {
		fnx_execname = program_invocation_name;
	}
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

#define E2S(s)    [s] = "" #s ""
static const char *s_errno_string[] = {
	E2S(EPERM),       E2S(ENOENT),      E2S(ESRCH),
	E2S(EINTR),       E2S(EIO),         E2S(ENXIO),
	E2S(E2BIG),       E2S(ENOEXEC),     E2S(EBADF),
	E2S(ECHILD),      E2S(EAGAIN),      E2S(ENOMEM),
	E2S(EACCES),      E2S(EFAULT),      E2S(ENOTBLK),
	E2S(EBUSY),       E2S(EEXIST),      E2S(EXDEV),
	E2S(ENODEV),      E2S(ENOTDIR),     E2S(EISDIR),
	E2S(EINVAL),      E2S(ENFILE),      E2S(EMFILE),
	E2S(ENOTTY),      E2S(ETXTBSY),     E2S(EFBIG),
	E2S(ENOSPC),      E2S(ESPIPE),      E2S(EROFS),
	E2S(EMLINK),      E2S(EPIPE),       E2S(EDOM),
	E2S(ERANGE),      E2S(EDEADLK),     E2S(ENAMETOOLONG),
	E2S(ENOLCK),      E2S(ENOSYS),      E2S(ENOTEMPTY),
	E2S(ELOOP),       E2S(ENOMSG),      E2S(EIDRM),
	E2S(ECHRNG),      E2S(EL2NSYNC),    E2S(EL3HLT),
	E2S(EL3RST),      E2S(ELNRNG),      E2S(EUNATCH),
	E2S(ENOCSI),      E2S(EL2HLT),      E2S(EBADE),
	E2S(EBADR),       E2S(EXFULL),      E2S(ENOANO),
	E2S(EBADRQC),     E2S(EBADSLT),     E2S(EBFONT),
	E2S(ENOSTR),      E2S(ENODATA),     E2S(ETIME),
	E2S(ENOSR),       E2S(ENONET),      E2S(ENOPKG),
	E2S(EREMOTE),     E2S(ENOLINK),     E2S(EADV),
	E2S(ESRMNT),      E2S(ECOMM),       E2S(EPROTO),
	E2S(EMULTIHOP),   E2S(EDOTDOT),     E2S(EBADMSG),
	E2S(EOVERFLOW),   E2S(ENOTUNIQ),    E2S(EBADFD),
	E2S(EREMCHG),     E2S(ELIBACC),     E2S(ELIBBAD),
	E2S(ELIBSCN),     E2S(ELIBMAX),     E2S(ELIBEXEC),
	E2S(EILSEQ),      E2S(ERESTART),    E2S(ESTRPIPE),
	E2S(EUSERS),      E2S(ENOTSOCK),    E2S(EDESTADDRREQ),
	E2S(EMSGSIZE),    E2S(EPROTOTYPE),  E2S(ENOPROTOOPT),
	E2S(EISCONN),     E2S(EOPNOTSUPP),  E2S(EPROTONOSUPPORT),
	E2S(ENOBUFS),     E2S(ENETRESET),   E2S(ESOCKTNOSUPPORT),
	E2S(EUCLEAN),     E2S(EHWPOISON),   E2S(EPFNOSUPPORT),
	E2S(ESHUTDOWN),   E2S(ENOTCONN),    E2S(EAFNOSUPPORT),
	E2S(EADDRINUSE),  E2S(ESTALE),      E2S(EADDRNOTAVAIL),
	E2S(ENETDOWN),    E2S(ENETUNREACH), E2S(ENOTRECOVERABLE),
	E2S(ENOTNAM),     E2S(ENOKEY),      E2S(ECONNABORTED),
	E2S(ECONNRESET),  E2S(ENAVAIL),     E2S(ETOOMANYREFS),
	E2S(ETIMEDOUT),   E2S(EALREADY),    E2S(ECONNREFUSED),
	E2S(EHOSTDOWN),   E2S(EISNAM),      E2S(EHOSTUNREACH),
	E2S(EINPROGRESS), E2S(EREMOTEIO),   E2S(EDQUOT),
	E2S(ENOMEDIUM),   E2S(EMEDIUMTYPE), E2S(ECANCELED),
	E2S(ERFKILL),     E2S(EKEYEXPIRED), E2S(EKEYREVOKED),
	E2S(EOWNERDEAD),  E2S(EKEYREJECTED),
};

/*
* Converts erron value to human-readable string.
* NB: strerror return errno description message.
*/
const char *fnx_errno_to_string(int errnum)
{
	const char *str = NULL;
	if ((errnum > 0) && (errnum < (int)FNX_NELEMS(s_errno_string))) {
		str = s_errno_string[errnum];
	}
	return str;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Prime numbers used by SGI STL C++ hashtable. See also:
 * http://planetmath.org/encyclopedia/GoodHashTablePrimes.html
 */
static const size_t s_good_hashtable_primes[] = {
	13UL,
	53UL,
	97UL,
	193UL,
	389UL,
	769UL,
	1543UL,
	3079UL,
	6151UL,
	12289UL,
	24593UL,
	49157UL,
	98317UL,
	196613UL,
	393241UL,
	786433UL,
	1572869UL,
	3145739UL,
	6291469UL,
	12582917UL,
	25165843UL,
	50331653UL,
	100663319UL,
	201326611UL,
	402653189UL,
	805306457UL,
	1610612741UL,
	3221225473UL,
	4294967291UL
};

size_t fnx_good_hprime(size_t n)
{
	size_t i, nelems, v;

	nelems = FNX_ARRAYSIZE(s_good_hashtable_primes);
	for (i = 0; i < nelems; ++i) {
		v = s_good_hashtable_primes[i];
		if (v >= n) {
			break;
		}
	}
	return v;
}
