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
#ifndef FUNEX_TESTSUITE_GRAYBOX_H_
#define FUNEX_TESTSUITE_GRAYBOX_H_

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <fnxhash.h>
#include <fnxinfra.h>
#include <fnxcore.h>


/* Logging macros */
#define gbx_trace(fmt, ...)     fnx_trace3(fmt, __VA_ARGS__)
#define gbx_info(fmt, ...)      fnx_info(fmt, __VA_ARGS__)
#define gbx_warn(fmt, ...)      fnx_warn(fmt, __VA_ARGS__)
#define gbx_error(fmt, ...)     fnx_error(fmt, __VA_ARGS__)
#define gbx_critical(fmt, ...)  fnx_critical(fmt, __VA_ARGS__)

#define gbx_ttrace(gbx, fmt, ...) \
	fnx_trace1("%s: " fmt, gbx->test, __VA_ARGS__)


/* Helper macros */
#define gbx_expect(env, res, exp) \
	gbx_do_expect(env, __FILE__, __LINE__, (long)(res), (long)(exp))

#define gbx_expect_true(env, pred) \
	gbx_expect(env, pred, 1)

#define gbx_expect_false(env, pred) \
	gbx_expect(env, pred, 0)

#define gbx_expect_ok(env, oper) \
	gbx_expect(env, oper, 0)

#define gbx_expect_err(env, oper, err) \
	gbx_expect(env, oper, err)

#define gbx_expect_eq(env, a, b) \
	gbx_expect_true(env, (long)(a) == (long)(b))

#define gbx_expect_eqm(env, a, b, sz) \
	gbx_expect_eq(env, fnx_bcmp(a, b, sz), 0)

#define gbx_expect_lt(env, a, b) \
	gbx_expect_true(env, ((long)(a) < (long)(b)))

#define gbx_expect_le(env, a, b) \
	gbx_expect_true(env, ((long)(a) <= (long)(b)))

#define gbx_expect_gt(env, a, b) \
	gbx_expect_true(env, ((long)(a) > (long)(b)))

#define gbx_expect_ge(env, a, b) \
	gbx_expect_true(env, ((long)(a) >= (long)(b)))

#define gbx_execute(env, arr) \
	gbx_nexecute(env, arr, gbx_nelems(arr))

#define gbx_nelems(x) \
	(sizeof(x) / sizeof(x[0]))



/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Tests control flags */
#define GBX_META        (1 << 0)
#define GBX_POSIX       (1 << 1)
#define GBX_ADMIN       (1 << 2)
#define GBX_CHOWN       (1 << 3)
#define GBX_SIO         (1 << 4)
#define GBX_MIO         (1 << 5)
#define GBX_CUSTOM      (1 << 6)
#define GBX_STRESS      (1 << 7)
#define GBX_PERF        (1 << 8)
#define GBX_VERIFY      (1 << 9)
#define GBX_RANDOM      (1 << 10)
#define GBX_KEEPGO      (1 << 11)


#define GBX_ALL \
	(GBX_POSIX | GBX_SIO | GBX_MIO |GBX_CUSTOM | GBX_STRESS)

#define GBX_DEFTEST(fn, flags) \
	{ fn, FNX_MAKESTR(fn), flags }

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Typedefs */
struct gbx_env;
struct gbx_sub;
typedef void (*gbx_exec_fn)(struct gbx_env *);
typedef void (*gbx_execs_fn)(struct gbx_sub *);
typedef fnx_mutex_t gbx_mutex_t;


/* Execution (test) definer */
struct gbx_execdef {
	gbx_exec_fn  hook;
	const char  *name;
	signed int   flags;
};
typedef struct gbx_execdef gbx_execdef_t;


/* Memory reference (GC) */
struct gbx_memref {
	void *pmem;
	struct gbx_memref *next;
};
typedef struct gbx_memref gbx_memref_t;


/* User's context & capabilities */
struct gbx_user {
	uid_t   uid;
	gid_t   gid;
	mode_t  umsk;
	char    isroot;
	char    cap_chown;
	char    cap_mknod;
	char    cap_fowner;
	char    cap_fsetid;
	char    cap_sysadmin;
};
typedef struct gbx_user gbx_user_t;


/* Common meta-info for io-tests */
struct gbx_ioargs {
	loff_t off;
	size_t bsz;
	size_t cnt;
};
typedef struct gbx_ioargs gbx_ioargs_t;


/* Sub-thread context */
struct gbx_sub {
	fnx_thread_t    th;
	gbx_execs_fn    hook;
	unsigned int    slot;
	const char     *path;
	int             fd;
	struct gbx_env *env;
};
typedef struct gbx_sub gbx_sub_t;


/* Environment context */
struct gbx_env {
	char           *base;
	char           *mntp;
	char           *iocp;
	char           *test;
	long            seqn;
	int             mask;
	int             nerr;
	time_t          start;
	gbx_user_t      user;
	fnx_timespec_t  times[2];
	fnx_fsinfo_t    fsinfo[2];
	gbx_memref_t   *mems;
	gbx_mutex_t     mutex;
	gbx_sub_t       subs[64];
	const gbx_ioargs_t *ioargs;
};
typedef struct gbx_env gbx_env_t;



/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Utility functions */
void gbx_init(gbx_env_t *, const char *, pid_t);

void gbx_destroy(gbx_env_t *);

void gbx_setup(gbx_env_t *);

void gbx_gc(gbx_env_t *);

void gbx_nexecute(gbx_env_t *, const gbx_execdef_t [], size_t);

void gbx_fillrbuf(gbx_env_t *, void *, size_t);

void gbx_fillsbuf(gbx_env_t *, unsigned long, void *, size_t);

void gbx_suspend(gbx_env_t *, int, int);

char *gbx_genname(gbx_env_t *);

void *gbx_newzbuf(gbx_env_t *, size_t);

void *gbx_newrbuf(gbx_env_t *, size_t);

char *gbx_newpath(gbx_env_t *);

char *gbx_newpath1(gbx_env_t *, const char *);

char *gbx_newpath2(gbx_env_t *, const char *, const char *);

char *gbx_newpathf(gbx_env_t *, const char *, const char *, ...);

char *gbx_psrootpath(gbx_env_t *);

void gbx_do_expect(const gbx_env_t *, const char *, unsigned, long, long);


/* Multi-threads tests helpers */
void gbx_start_sub(gbx_env_t *, gbx_execs_fn, size_t, const char *);

void gbx_start_fsub(gbx_env_t *, gbx_execs_fn, size_t, int);

void gbx_join_sub(gbx_env_t *, size_t);

/* Tests executions main entry point */
void gbx_run(const char *, int);

#endif /* FUNEX_TESTSUITE_GRAYBOX_H_ */


