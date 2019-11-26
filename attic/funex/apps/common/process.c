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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <error.h>
#include <err.h>
#include <limits.h>
#include <locale.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/capability.h>

#include <fnxserv.h>
#include "version.h"
#include "logging.h"
#include "globals.h"
#include "process.h"

#define globals (funex_globals)

/* Preemptive verifications */
static void funex_verify_lib(void)
{
	if (fnx_verify_core() != 0) {
		funex_dief("non-valid-system");
	}
}

/* Singleton objectS creation/deletion */
void *funex_new_sobject(size_t size)
{
	size_t nbks;
	void *sobj = NULL;

	nbks = fnx_bytes_to_nbk(size);
	sobj = fnx_mmap_bks(nbks);
	if (sobj == NULL) {
		funex_dief("no-mmap size=%lu", size);
	}
	fnx_burnstack(8);

	return sobj;
}

void funex_del_sobject(void *sobj, size_t size)
{
	size_t nbks;

	nbks = fnx_bytes_to_nbk(size);
	fnx_munmap_bks(sobj, nbks);
	fnx_burnstack(8);
}

/* Setup/cleanup defaults */
static void funex_init_args(int argc, char *argv[])
{
	umask(077);
	setlocale(LC_ALL, "");
	fnx_setup_execname();

	funex_globals_setup();
	atexit(funex_globals_cleanup);

	globals.prog.name    = program_invocation_short_name;
	globals.prog.argc    = argc;
	globals.prog.argv    = argv;
	globals.proc.utility = 1;
}

void funex_setup_process(void)
{
	funex_setrlimit_core(globals.proc.core);
}

void funex_finalize(void)
{
	funex_globals_cleanup();
}

void funex_finalize_exit(void)
{
	funex_finalize();
	funex_goodbye();
}

void funex_uptime_str(char *str, size_t sz)
{
	time_t uptime, secs, mins, hours;

	uptime = time(NULL) - globals.start;

	hours = uptime / 3600;
	mins  = (uptime / 60) % 60;
	secs  = uptime % 60;

	snprintf(str, sz, "%ldh%ldm%lds", (long)hours, (long)mins, (long)secs);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

FNX_ATTR_NORETURN void funex_exit(int rc)
{
	fflush(stdout);
	fflush(stderr);
	exit(rc);
}

FNX_ATTR_NORETURN void funex_exiterr(void)
{
	funex_exit(EXIT_FAILURE);
}

FNX_ATTR_NORETURN void funex_goodbye(void)
{
	funex_exit(EXIT_SUCCESS);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

FNX_ATTR_NORETURN void funex_die(void)
{
	fnx_die();
}

static void funex_die_errx(const char *fmt, ...)
{
	va_list ap;

	program_invocation_name = program_invocation_short_name;

	va_start(ap, fmt);
	verrx(EXIT_FAILURE, fmt, ap);
	va_end(ap);
}

FNX_ATTR_NORETURN void funex_dief(const char *fmt, ...)
{
	va_list ap;
	char msg[1024] = "";

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg) - 1, fmt, ap);
	va_end(ap);

	funex_die_errx("%s", msg);
	funex_die(); /* Should never get here... */
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Fatal-error callback-hook */
FNX_ATTR_NORETURN void funex_fatal_error(int errnum, const char *msg)
{
	char *str, *saveptr = NULL;
	const char *sp = " ";
	char bt_msg[2048] = "";
	const char *prog  = fnx_execname;
	const char *node  = globals.sys.nodename;

	/* Log error-message */
	fnx_critical("%s", sp);
	fnx_critical("%s (errno=%d)", msg, errnum);

	/* Program, version & system info */
	fnx_critical("%s", sp);
	fnx_critical("%s (%s@%s)", prog, funex_version(), node);

	/* Stack-trace */
	fnx_critical("%s", sp);
	fnx_backtrace_stackinfo(bt_msg, sizeof(bt_msg) - 1);
	str = strtok_r(bt_msg, "\n", &saveptr);
	while (str != NULL) {
		fnx_critical("%s", str);
		str = strtok_r(NULL, "\n", &saveptr);
	}

	/* And via 'addr2line' utility */
	fnx_critical("%s", sp);
	fnx_backtrace_addr2line(bt_msg, sizeof(bt_msg) - 1);
	fnx_critical("%s", bt_msg);

	/* Minimal cleanups and die */
	funex_close_syslog();
	funex_die();
}

static void funex_set_panic_hook(void)
{
	fnx_set_panic_callback(funex_fatal_error);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

char *funex_username(void)
{
	char *str;

	str = getenv("USER");
	if (str != NULL) {
		str = strdup(str);
	}
	return str;
}

char *funex_selfexe(void)
{
	char *exe;
	const char *name;

	name = fnx_execname;
	if (name == NULL) {
		name = program_invocation_name;
	}
	exe = strdup(name);
	return exe;
}

char *funex_nodename(void)
{
	int rc;
	char *str = NULL;
	char name[NAME_MAX + 1] = "";

	rc = gethostname(name, sizeof(name) - 1);
	if (rc == 0) {
		str = strdup(name);
	}
	return str;
}

#ifndef SYSCONFDIR
#define SYSCONFDIR "../etc/"
#endif

char *funex_sysconfdir(void)
{
	size_t bufsz;
	char *ptr, *self, *buf, *rpath;
	const char *suffix = SYSCONFDIR;

	buf = rpath = NULL;
	self = funex_selfexe();
	if (self == NULL) {
		goto out;
	}
	ptr = strrchr(self, '/');
	if (ptr == NULL) {
		goto out;
	}
	*ptr = '\0';

	bufsz = PATH_MAX;
	buf = (char *)calloc(1, bufsz);
	if (buf == NULL) {
		goto out;
	}

	snprintf(buf, bufsz, "%s/%s", self, suffix);
	rpath = realpath(buf, NULL);

out:
	if (self != NULL) {
		free(self);
	}
	if (buf != NULL) {
		free(buf);
	}
	return rpath;
}

char *funex_mntusock(void)
{
	const char *path;
#ifdef RUNDIR
	path = RUNDIR "/" FNX_MNTSOCK;
#else
	path = "/tmp/" FNX_MNTSOCK;
#endif
	return strdup(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Daemon operations:
 */
int funex_forknwait(void (*child_fn)(void), pid_t *p_pid)
{
	int rc, status = 0;
	pid_t pid;
	struct rlimit rlm;

	funex_getrlimit_nproc(&rlm);

	rc  = 0;
	pid = fork();
	if (pid > 0) {  /* Parent process */
		waitpid(pid, &status, 0);
		rc = WEXITSTATUS(status);
		if (!WIFEXITED(status)) {
			rc = -1;
		} else {
			*p_pid = pid;
		}
	} else if (pid == 0) { /* Child process  */
		child_fn();
	} else {
		fnx_panic("no-fork RLIMIT_NPROC.rlim_cur=%ju", rlm.rlim_cur);
	}
	return rc;
}

static void funex_pinfo(const char *label)
{
	pid_t pid;
	char *wdir;

	pid  = getpid();
	wdir = get_current_dir_name();

	fnx_trace1("%s pid=%d work-dir=%s", label, (int)pid, wdir);
	free(wdir);
}

static void funex_daemonize2(int dofork, fnx_error_cb err_cb)
{
	int rc, nochdir, noclose;

	/* Do actual daemonization via syscall */
	if (dofork) {
		nochdir = noclose = 0;
		rc = daemon(nochdir, noclose);
		if (rc != 0) {
			fnx_panic("daemon-failed rc=%d", rc);
		}
		funex_pinfo("daemonized");
	} else {
		funex_pinfo("non-daemon-mode");
	}

	/* ReBind panic call-back for daemon */
	fnx_set_panic_callback(err_cb);
}

void funex_daemonize(void)
{
	const int dofork = globals.proc.nodaemon ? 0 : 1;

	/* Daemonize only if explicit global set; in such case, have syslog */
	if (dofork) {
		globals.log.syslog = FNX_TRUE;
	}
	funex_daemonize2(dofork, funex_fatal_error);
}


/*
 * Become a daemon process: fork child process, which in turn will do the
 * actual daemonization and do the rest of initialization & execution.
 */
void funex_forkd(void (*start)(void))
{
	int rc;
	pid_t pid = (pid_t)(-1);

	errno = 0;
	rc  = funex_forknwait(start, &pid);
	if (rc != 0) {
		funex_exiterr();
	}
	fnx_info("forked-child pid=%d", (int)pid);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Resource operations:
 */
typedef __rlimit_resource_t fnx_rlimit_resource_t;

static const char *rlimit_name(fnx_rlimit_resource_t rsrc)
{
	const char *s = NULL;

	switch (rsrc) {
		case RLIMIT_CPU:
			s = "RLIMIT_CPU";
			break;
		case RLIMIT_FSIZE:
			s = "RLIMIT_FSIZE";
			break;
		case RLIMIT_DATA:
			s = "RLIMIT_DATA";
			break;
		case RLIMIT_STACK:
			s = "RLIMIT_STACK";
			break;
		case RLIMIT_CORE:
			s = "RLIMIT_CORE";
			break;
		case RLIMIT_RSS:
			s = "RLIMIT_RSS";
			break;
		case RLIMIT_NOFILE:
			s = "RLIMIT_NOFILE";
			break;
		case RLIMIT_AS:
			s = "RLIMIT_AS";
			break;
		case RLIMIT_NPROC:
			s = "RLIMIT_NPROC";
			break;
		case RLIMIT_MEMLOCK:
			s = "RLIMIT_MEMLOCK";
			break;
		case RLIMIT_LOCKS:
			s = "RLIMIT_LOCKS";
			break;
		case RLIMIT_SIGPENDING:
			s = "RLIMIT_SIGPENDING";
			break;
		case RLIMIT_MSGQUEUE:
			s = "RLIMIT_MSGQUEUE";
			break;
		case RLIMIT_NICE:
			s = "RLIMIT_NICE";
			break;
		case RLIMIT_RTPRIO:
			s = "RLIMIT_RTPRIO";
			break;
		case RLIMIT_RTTIME:
			s = "RLIMIT_RTTIME";
			break;
		case RLIM_NLIMITS:
		default:
			s = "RLIMIT_XXX";
			break;
	}
	return s;
}


static void
funex_getrlimit(fnx_rlimit_resource_t rsrc, struct rlimit *rlm)
{
	int rc;

	rc = getrlimit(rsrc, rlm);
	if (rc != 0) {
		fnx_panic("getrlimit-failure: %s err=%d", rlimit_name(rsrc), errno);
	}
}

static void
funex_setrlimit(fnx_rlimit_resource_t rsrc, const struct rlimit *rlm)
{
	int rc;

	rc = setrlimit(rsrc, rlm);
	if (rc != 0) {
		fnx_panic("setrlimit-failure: %s err=%d", rlimit_name(rsrc), errno);
	}
}

static void
funex_report_rlimit(fnx_rlimit_resource_t rsrc, const struct rlimit *rlm)
{
	if (rlm->rlim_max == RLIM_INFINITY) {
		fnx_info("%s rlim_cur=%ju rlim_max=INFINITY",
		         rlimit_name(rsrc), rlm->rlim_cur);
	} else {
		fnx_info("%s rlim_cur=%ju rlim_max=%ju",
		         rlimit_name(rsrc), rlm->rlim_cur, rlm->rlim_max);
	}
}

void funex_getrlimit_as(struct rlimit *rlm)
{
	funex_getrlimit(RLIMIT_AS, rlm);
}

void funex_getrlimit_nproc(struct rlimit *rlm)
{
	funex_getrlimit(RLIMIT_NPROC, rlm);
}


void funex_setrlimit_core(long sz)
{
	fnx_rlimit_resource_t rsrc = RLIMIT_CORE;
	struct rlimit rlim;
	const rlim_t max_sz = (rlim_t)sz;

	funex_getrlimit(rsrc, &rlim);
	rlim.rlim_cur = fnx_min(max_sz, rlim.rlim_max); /* 0 = No core */
	funex_setrlimit(rsrc, &rlim);
	funex_report_rlimit(rsrc, &rlim);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Capabilities (effective):
 */
int funex_cap_is_sys_admin(void)
{
	int rc;
	pid_t pid;
	cap_t cap;
	cap_flag_value_t flag;

	pid = getpid();
	cap = cap_get_pid(pid);
	if (cap == NULL) {
		return FNX_FALSE;
	}
	rc = cap_get_flag(cap, CAP_SYS_ADMIN, CAP_EFFECTIVE, &flag);
	if (rc != 0) {
		cap_free(cap);
		return FNX_FALSE;
	}
	rc  = cap_free(cap);
	if (rc != 0) {
		fnx_panic("cap_free-failure: pid=%d cap=%p rc=%d",
		          pid, (void *)cap, rc);
	}
	return (flag == CAP_SET);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* First call in each funex process; setup common globals/configurations */
void funex_init_process(int argc, char *argv[])
{
	/* Verifications */
	funex_verify_lib();

	/* Begin with defaults */
	funex_init_args(argc, argv);

	/* Bind panic callback hook */
	funex_set_panic_hook();
}


