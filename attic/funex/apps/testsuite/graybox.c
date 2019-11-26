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
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <error.h>
#include <errno.h>
#include <err.h>
#include <time.h>
#include <limits.h>
#include <uuid/uuid.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/capability.h>
#include <unistd.h>

#include "graybox.h"
#include "syscalls.h"
#include "test-hooks.h"

#define gbx_assert(x) fnx_assert(x)


static char *joinpath(const char *path1, const char *path2)
{
	char *path;

	errno = 0;
	if ((path = fnx_sys_joinpath(path1, path2)) == NULL) {
		err(errno, "no-joinpath: path1=%s path2=%s", path1, path2);
	}
	return path;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void gbx_lock(gbx_env_t *gbx)
{
	fnx_mutex_lock(&gbx->mutex);
}

static void gbx_unlock(gbx_env_t *gbx)
{
	fnx_mutex_unlock(&gbx->mutex);
}

static void *gbx_malloc(size_t sz)
{
	void *ptr;

	errno = 0;
	ptr = calloc(1, sz);
	if (ptr == NULL) {
		err(EXIT_FAILURE, "no-malloc: sz=%zu", sz);
	}
	return ptr;
}

static void gbx_free(void *ptr)
{
	free(ptr);
}

static void gbx_store_memref(gbx_env_t *gbx, void *ptr)
{
	gbx_memref_t *mref;

	mref = (gbx_memref_t *)gbx_malloc(sizeof(*mref));
	gbx_lock(gbx);
	mref->pmem = ptr;
	mref->next = gbx->mems;
	gbx->mems = mref;
	gbx_unlock(gbx);
}

void gbx_gc(gbx_env_t *gbx)
{
	gbx_memref_t *mref, *next;

	gbx_lock(gbx);
	mref = gbx->mems;
	while (mref != NULL) {
		next = mref->next;
		gbx_free(mref->pmem);
		gbx_free(mref);
		mref = next;
	}
	gbx->mems = NULL;
	gbx_unlock(gbx);
}

static void *gbx_malloc_and_store(gbx_env_t *gbx, size_t sz)
{
	void *ptr;

	ptr = gbx_malloc(sz);
	gbx_store_memref(gbx, ptr);
	return ptr;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void gbx_sub_init(gbx_sub_t *sub, gbx_env_t *gbx, unsigned slot)
{
	fnx_thread_init(&sub->th);
	sub->env    = gbx;
	sub->hook   = NULL;
	sub->slot   = slot;
	sub->path   = NULL;
	sub->fd     = -1;
}

static void gbx_init_subs(gbx_env_t *gbx)
{
	gbx_sub_t *sub;

	for (size_t i = 0; i < fnx_nelems(gbx->subs); ++i) {
		sub = &gbx->subs[i];
		gbx_sub_init(sub, gbx, (unsigned)i);
	}
}

static void run_sub(void *p)
{
	gbx_sub_t *sub = (gbx_sub_t *)p;
	sub->hook(sub);
}

static void gbx_do_start_sub(gbx_env_t *gbx, gbx_execs_fn hook,
                             size_t slot, const char *path, int fd)
{
	gbx_sub_t *sub = &gbx->subs[slot];

	gbx_assert(slot < fnx_nelems(gbx->subs));
	gbx_assert(sub->hook == NULL);

	sub->hook = hook;
	sub->path = path;
	sub->fd   = fd;
	fnx_thread_init(&sub->th);
	fnx_thread_start(&sub->th, run_sub, sub);
}


void gbx_start_sub(gbx_env_t *gbx, gbx_execs_fn hook,
                   size_t slot, const char *path)
{
	gbx_do_start_sub(gbx, hook, slot, path, -1);
}

void gbx_start_fsub(gbx_env_t *gbx, gbx_execs_fn hook, size_t slot, int fd)
{
	gbx_do_start_sub(gbx, hook, slot, NULL, fd);
}

void gbx_join_sub(gbx_env_t *gbx, size_t slot)
{
	gbx_sub_t *sub;

	gbx_assert(slot < fnx_nelems(gbx->subs));
	sub = &gbx->subs[slot];
	if (sub->hook != NULL) {
		fnx_thread_join(&sub->th, FNX_NOFAIL);
		fnx_thread_destroy(&sub->th);
		sub->hook = NULL;
		sub->path = NULL;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static char iscap(cap_t cap_p, cap_value_t value)
{
	int rc;
	cap_flag_value_t flag = 0;

	rc  = cap_get_flag(cap_p, value, CAP_EFFECTIVE, &flag);
	return ((rc == 0) && (flag == CAP_SET));
}

static void gbx_init_user(gbx_env_t *gbx, pid_t pid)
{
	cap_t cap;

	gbx->user.uid   = geteuid();
	gbx->user.gid   = getegid();
	gbx->user.umsk  = umask(0);
	umask(gbx->user.umsk);

	if (pid == 0) {
		gbx->user.isroot = 1;
	}
	cap = cap_get_pid(pid);
	if (cap != NULL) {
		gbx->user.cap_chown      = iscap(cap, CAP_CHOWN);
		gbx->user.cap_fowner     = iscap(cap, CAP_FOWNER);
		gbx->user.cap_fsetid     = iscap(cap, CAP_FSETID);
		gbx->user.cap_sysadmin   = iscap(cap, CAP_SYS_ADMIN);
		gbx->user.cap_mknod      = iscap(cap, CAP_MKNOD);
		cap_free(cap);
	}
}

void gbx_init(gbx_env_t *gbx, const char *base, pid_t pid)
{
	memset(gbx, 0, sizeof(*gbx));

	gbx_init_user(gbx, pid);
	gbx_init_subs(gbx);
	fnx_mutex_init(&gbx->mutex);
	gbx->base   = strdup(base);
	gbx->start  = time(NULL);
	gbx->mask   = 0;
	gbx->nerr   = 0;
	gbx->seqn   = 0;
	gbx->mems   = NULL;
	gbx->mntp   = NULL;
	gbx->iocp   = NULL;
	gbx->test   = NULL;
	gbx->ioargs = NULL;
}

void gbx_destroy(gbx_env_t *gbx)
{
	gbx_gc(gbx);
	free(gbx->base);
	free(gbx->mntp);
	free(gbx->iocp);
	free(gbx->test);
	fnx_mutex_destroy(&gbx->mutex);
	memset(gbx, 0, sizeof(*gbx));
}

void gbx_setup(gbx_env_t *gbx)
{
	int rc;
	const char *psrootname = FNX_PSROOTNAME;
	const char *psioctlname = FNX_PSIOCTLNAME;

	rc = gbx_sys_fsboundary(gbx->base, &gbx->mntp);
	if (rc != 0) {
		err(rc, "no-fsboundary: %s", gbx->base);
	}
	gbx->iocp = fnx_sys_makepath(gbx->mntp, psrootname, psioctlname, NULL);
	if (gbx->iocp == NULL) {
		err(rc, "no-makepath: %s %s %s", gbx->mntp, psrootname, psioctlname);
	}
}

/* Fill with pseudo-numbers */
void gbx_fillrbuf(gbx_env_t *gbx, void *buf, size_t bsz)
{
	fnx_unused(gbx);
	blgn_prandom_buf(buf, bsz, (unsigned long)time(NULL));
}

void gbx_fillsbuf(gbx_env_t *gbx, unsigned long seed, void *buf, size_t bsz)
{
	const uint64_t xseed = (uint64_t)gbx->start ^ seed;
	blgn_prandom_buf(buf, bsz, xseed);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_suspend(gbx_env_t *gbx, int sec, int part)
{
	int rc;
	struct timespec rem = { 0, 0 };
	struct timespec req = { sec, (long)part * 1000000LL };

	rc = nanosleep(&req, &rem);
	while ((rc != 0) && (errno == EINTR)) {
		memcpy(&req, &rem, sizeof(req));
		rc = nanosleep(&req, &rem);
	}
	(void)gbx;
}

char *gbx_newpath(gbx_env_t *gbx)
{
	return gbx_newpath1(gbx, gbx_genname(gbx));
}

char *gbx_newpath1(gbx_env_t *gbx, const char *path1)
{
	char *path;

	path = joinpath(gbx->base, path1);
	gbx_store_memref(gbx, path);
	return path;
}

char *gbx_newpath2(gbx_env_t *gbx, const char *path1, const char *path2)
{
	char *path;

	path = joinpath(path1, path2);
	gbx_store_memref(gbx, path);
	return path;
}

char *gbx_newpathf(gbx_env_t *gbx, const char *path0, const char *fmt, ...)
{
	va_list ap;
	char buf[1024];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	return gbx_newpath2(gbx, path0, buf);
}

char *gbx_psrootpath(gbx_env_t *gbx)
{
	return gbx_newpath2(gbx, gbx->mntp, FNX_PSROOTNAME);
}

void *gbx_newzbuf(gbx_env_t *gbx, size_t bsz)
{
	return gbx_malloc_and_store(gbx, bsz);
}

void *gbx_newrbuf(gbx_env_t *gbx, size_t bsz)
{
	void *buf;
	buf = gbx_newzbuf(gbx, bsz);
	gbx_fillrbuf(gbx, buf, bsz);
	return buf;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const char *base(const char *path)
{
	const char *s;

	s = strrchr(path, '/');
	if (s != NULL) {
		s = s + 1;
	} else {
		s = path;
	}
	return s;
}

static void gbx_die_failure(const gbx_env_t *gbx, const char *backtrace_info,
                            const char *file, unsigned line, long res)
{
	const char *name = gbx->test;
	gbx_critical("Failed: '%s' res=%ld (%s:%u)", name, res, base(file), line);
	gbx_critical("Backtrace: %s", backtrace_info);
	error(EXIT_FAILURE, 0, "Test-failure: %s", name);
}

static void gbx_report_failure(const gbx_env_t *gbx,
                               const char *file, unsigned line, long res)
{
	const char *name = gbx->test;
	gbx_critical("Failed: '%s' res=%ld (%s:%u)", name, res, base(file), line);
}

void gbx_do_expect(const gbx_env_t *gbx,
                   const char *file, unsigned line, long res, long exp)
{
	char bti[1024] = "";

	if (res != exp) {
		if (gbx->mask & GBX_KEEPGO) {
			gbx_report_failure(gbx, file, line, res);
		} else {
			fnx_backtrace_addr2line(bti, sizeof(bti));
			gbx_die_failure(gbx, bti, file, line, res);
		}
	}
}

char *gbx_genname(gbx_env_t *gbx)
{
	uint32_t seq, val;
	char *name;
	const char *test;
	const size_t nsz = FNX_NAME_MAX + 1;

	gbx_lock(gbx);
	seq = (uint32_t)(++gbx->seqn);
	val = (uint32_t)(gbx->start ^ (gbx->start >> 32)) + seq;
	gbx_unlock(gbx);

	test = gbx->test ? gbx->test : "x";
	name = (char *)gbx_malloc_and_store(gbx, nsz);
	snprintf(name, nsz, "funex_%s.%08x", test, val);
	return name;
}

static void clocknow(struct timespec *ts)
{
	clock_gettime(CLOCK_MONOTONIC, ts);
}

static void clockdif(const struct timespec *beg,
                     const struct timespec *end,
                     struct timespec *dif)
{
	dif->tv_sec = end->tv_sec - beg->tv_sec;
	if (end->tv_nsec >= beg->tv_nsec) {
		dif->tv_nsec = end->tv_nsec - beg->tv_nsec;
	} else {
		dif->tv_sec -= 1;
		dif->tv_nsec = beg->tv_nsec - end->tv_nsec;
	}
}

static void gbx_have_fsinfo(gbx_env_t *gbx, int idx)
{
	fnx_fsinfo_t *fsinfo = &gbx->fsinfo[idx];

	if (gbx->mask & GBX_VERIFY) {
		gbx_expect_ok(gbx, gbx_sys_fsinfo(gbx->iocp, fsinfo));
	}
}

static void gbx_start_test(gbx_env_t *gbx, const char *test)
{
	gbx->nerr = 0;
	gbx->test = strdup(test);
	gbx_info("Run: '%s'", gbx->test);
	clocknow(&gbx->times[0]);
	gbx_have_fsinfo(gbx, 0);
}

#define gbx_pred(a, b, op) (a op b)
#define gbx_probe_fsstat(gbx, beg, end, field, op) \
	do { \
		if (!gbx_pred(beg->field, end->field, op)) { \
			gbx_error("beg.%s=%ld end.%s=%ld", \
			          FNX_MAKESTR(field), (long)beg->field, \
			          FNX_MAKESTR(field), (long)end->field); \
			gbx->nerr++; \
		} \
	} while (0)

static void gbx_verify_fs(gbx_env_t *gbx)
{
	const fnx_fsstat_t *beg = &gbx->fsinfo[0].fs_stat;
	const fnx_fsstat_t *end = &gbx->fsinfo[1].fs_stat;

	if (gbx->mask & GBX_VERIFY) {
		gbx_probe_fsstat(gbx, beg, end, f_apex_ino, <=);
		gbx_probe_fsstat(gbx, beg, end, f_apex_vlba, <=);
		gbx_probe_fsstat(gbx, beg, end, f_inodes, ==);
		gbx_probe_fsstat(gbx, beg, end, f_iused, ==);
		gbx_probe_fsstat(gbx, beg, end, f_bfrgs, ==);
		gbx_probe_fsstat(gbx, beg, end, f_bused, ==);
		gbx_probe_fsstat(gbx, beg, end, f_super, ==);
		gbx_probe_fsstat(gbx, beg, end, f_dir, ==);
		gbx_probe_fsstat(gbx, beg, end, f_dirseg, ==);
		gbx_probe_fsstat(gbx, beg, end, f_symlnk, ==);
		gbx_probe_fsstat(gbx, beg, end, f_reflnk, ==);
		gbx_probe_fsstat(gbx, beg, end, f_special, ==);
		gbx_probe_fsstat(gbx, beg, end, f_reg, ==);
		gbx_probe_fsstat(gbx, beg, end, f_regsec, ==);
		gbx_probe_fsstat(gbx, beg, end, f_regseg, ==);
		gbx_probe_fsstat(gbx, beg, end, f_vbk, ==);

		gbx_expect_eq(gbx, gbx->nerr, 0);
	}
}

static void gbx_finish_test(gbx_env_t *gbx)
{
	struct timespec dif;

	clocknow(&gbx->times[1]);
	clockdif(&gbx->times[0], &gbx->times[1], &dif);
	gbx_have_fsinfo(gbx, 1);
}

static void gbx_cleanup_test(gbx_env_t *gbx)
{
	if (gbx->test != NULL) {
		free(gbx->test);
		gbx->test = NULL;
	}
	gbx->ioargs = NULL;
}

static int gbx_cap_chown(const gbx_env_t *gbx)
{
	return (gbx->user.isroot || gbx->user.cap_chown);
}

void gbx_nexecute(gbx_env_t *gbx, const gbx_execdef_t tdefs[], size_t n)
{
	int flags, idx[256];
	const gbx_execdef_t *tdef;
	const size_t nt = fnx_min(n, fnx_nelems(idx));

	if (gbx->mask & GBX_RANDOM) {
		blgn_prandom_tseq(idx, nt, 0);
	} else {
		blgn_create_seq(idx, nt, 0);
	}

	for (size_t i = 0; i < nt; ++i) {
		tdef  = &tdefs[idx[i]];
		flags = tdef->flags;
		if (!tdef->hook) {
			continue;
		}
		if ((flags & GBX_CHOWN) && !gbx_cap_chown(gbx)) {
			continue;
		}
		if (flags & GBX_META) {
			tdef->hook(gbx);
		} else if (flags & gbx->mask) {
			gbx_start_test(gbx, tdef->name);
			tdef->hook(gbx);
			gbx_finish_test(gbx);
			gbx_verify_fs(gbx);
			gbx_cleanup_test(gbx);
		}
		gbx_gc(gbx);
	}
}


void gbx_run(const char *base, int mask)
{
	gbx_env_t *gbx;

	gbx = (gbx_env_t *)gbx_malloc(sizeof(*gbx));
	gbx_init(gbx, base, getpid());
	gbx_setup(gbx);
	gbx->mask = mask;
	gbx_nexecute(gbx, gbx_tests_list, gbx_tests_list_len);
	gbx_destroy(gbx);
	gbx_free(gbx);
}

