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

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "config-site.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/capability.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <error.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <locale.h>
#include <getopt.h>

#include "voluta-prog.h"

/* Global process' variables */
struct voluta_globals voluta_globals;


__attribute__((__noreturn__))
static void error_with(int errnum, const char *msg)
{
	error(EXIT_FAILURE, abs(errnum), "%s", msg);
	/* never gets here, but makes compiler happy */
	abort();
}

__attribute__((__noreturn__))
void voluta_die(int errnum, const char *fmt, ...)
{
	va_list ap;
	char msg[2048] = "";

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg) - 1, fmt, ap);
	va_end(ap);

	error_with(errnum, msg);
}

__attribute__((__noreturn__))
void voluta_die_redundant_arg(const char *s)
{
	voluta_die(0, "redundant argument: %s", s);
}

__attribute__((__noreturn__))
void voluta_die_no_volume_path(void)
{
	voluta_die(0, "missing volume path");
}

__attribute__((__noreturn__))
void voluta_die_no_mount_point(void)
{
	voluta_die(0, "missing mount-point");
}

__attribute__((__noreturn__))
void voluta_die_unsupported_opt(void)
{
	exit(EXIT_FAILURE);
}

void voluta_check_no_redundant_arg(void)
{
	int argc = voluta_globals.cmd_argc;
	char **argv = voluta_globals.cmd_argv;

	if (optind < argc) {
		voluta_die_redundant_arg(argv[optind]);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

voluta_signal_hook_fn voluta_signal_callback_hook = NULL;


static void sigaction_info_handler(int signum)
{
	voluta_log_debug("signal: %d", signum);
}

static void sigaction_halt_handler(int signum)
{
	voluta_log_info("halt-signal: %d", signum);
	voluta_globals.sig_halt = signum;
	if (voluta_signal_callback_hook != NULL) {
		/* Call sub-program specific logic */
		voluta_signal_callback_hook(signum);
	} else {
		/* Force re-wake-up */
		raise(SIGHUP);
	}
}

static void sigaction_term_handler(int signum)
{
	voluta_dump_backtrace();
	voluta_log_crit("term-signal: %d", signum);
	voluta_globals.sig_halt = signum;
	voluta_globals.sig_fatal = signum;
	exit(EXIT_FAILURE);
}

static void sigaction_abort_handler(int signum)
{
	if (voluta_globals.sig_fatal) {
		_exit(EXIT_FAILURE);
	}

	voluta_dump_backtrace();
	voluta_log_crit("abort-signal: %d", signum);
	voluta_globals.sig_halt = signum;
	voluta_globals.sig_fatal = signum;
	abort(); /* Re-raise to _exit */
}

static struct sigaction s_sigaction_info = {
	.sa_handler = sigaction_info_handler
};

static struct sigaction s_sigaction_halt = {
	.sa_handler = sigaction_halt_handler
};

static struct sigaction s_sigaction_term = {
	.sa_handler = sigaction_term_handler
};

static struct sigaction s_sigaction_abort = {
	.sa_handler = sigaction_abort_handler
};

static void register_sigaction(int signum, struct sigaction *sa)
{
	int err;

	err = voluta_sys_sigaction(signum, sa, NULL);
	if (err) {
		voluta_die(err, "sigaction error: signum=%d", signum);
	}
}

static void sigaction_info(int signum)
{
	register_sigaction(signum, &s_sigaction_info);
}

static void sigaction_halt(int signum)
{
	register_sigaction(signum, &s_sigaction_halt);
}

static void sigaction_term(int signum)
{
	register_sigaction(signum, &s_sigaction_term);
}

static void sigaction_abort(int signum)
{
	register_sigaction(signum, &s_sigaction_abort);
}

void voluta_register_sigactions(void)
{
	sigaction_info(SIGHUP);
	sigaction_halt(SIGINT);
	sigaction_halt(SIGQUIT);
	sigaction_term(SIGILL);
	sigaction_info(SIGTRAP);
	sigaction_abort(SIGABRT);
	sigaction_term(SIGBUS);
	sigaction_term(SIGFPE);
	sigaction_info(SIGUSR1);
	sigaction_term(SIGSEGV);
	sigaction_info(SIGUSR2);
	sigaction_info(SIGPIPE);
	sigaction_info(SIGALRM);
	sigaction_halt(SIGTERM);
	sigaction_term(SIGSTKFLT);
	sigaction_info(SIGCHLD);
	sigaction_info(SIGCONT);
	sigaction_halt(SIGTSTP);
	sigaction_halt(SIGTTIN);
	sigaction_halt(SIGTTOU);
	sigaction_info(SIGURG);
	sigaction_halt(SIGXCPU);
	sigaction_halt(SIGXFSZ);
	sigaction_halt(SIGVTALRM);
	sigaction_info(SIGPROF);
	sigaction_info(SIGWINCH);
	sigaction_info(SIGIO);
	sigaction_halt(SIGPWR);
	sigaction_halt(SIGSYS);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Known file-systems */
#define FUSE_SUPER_MAGIC        0x65735546
#define TMPFS_MAGIC             0x01021994
#define XFS_SB_MAGIC            0x58465342
#define EXT234_SUPER_MAGIC      0x0000EF53
#define ZFS_SUPER_MAGIC         0x2FC12FC1
#define BTRFS_SUPER_MAGIC       0x9123683E
#define CEPH_SUPER_MAGIC        0x00C36400
#define CIFS_MAGIC_NUMBER       0xFF534D42
#define ECRYPTFS_SUPER_MAGIC    0x0000F15F
#define F2FS_SUPER_MAGIC        0xF2F52010
#define NFS_SUPER_MAGIC         0x00006969
#define NTFS_SB_MAGIC           0x5346544E
#define OVERLAYFS_SUPER_MAGIC   0x794C7630

#define MKFSINFO(t_, n_, f_) \
	{ .vfstype = (t_), .name = (n_), .permitted_mount = (f_) }


static const struct voluta_fsinfo fsinfo_whitelist[] = {
	MKFSINFO(FUSE_SUPER_MAGIC, "FUSE", 0),
	MKFSINFO(TMPFS_MAGIC, "TMPFS", 0),
	MKFSINFO(XFS_SB_MAGIC, "XFS", 1),
	MKFSINFO(EXT234_SUPER_MAGIC, "EXT234", 1),
	MKFSINFO(ZFS_SUPER_MAGIC, "ZFS", 1),
	MKFSINFO(BTRFS_SUPER_MAGIC, "BTRFS", 1),
	MKFSINFO(CEPH_SUPER_MAGIC, "CEPH", 1),
	MKFSINFO(CIFS_MAGIC_NUMBER, "CIFS", 1),
	MKFSINFO(ECRYPTFS_SUPER_MAGIC, "ECRYPTFS", 0),
	MKFSINFO(F2FS_SUPER_MAGIC, "F2FS", 1),
	MKFSINFO(NFS_SUPER_MAGIC, "NFS", 1),
	MKFSINFO(NTFS_SB_MAGIC, "NTFS", 1),
	MKFSINFO(OVERLAYFS_SUPER_MAGIC, "OVERLAYFS", 0)
};

const struct voluta_fsinfo *voluta_fsinfo_by_vfstype(long vfstype)
{
	const struct voluta_fsinfo *fsinfo = NULL;

	for (size_t i = 0; i < VOLUTA_ARRAY_SIZE(fsinfo_whitelist); ++i) {
		fsinfo = &fsinfo_whitelist[i];
		if (fsinfo->vfstype == vfstype) {
			break;
		}
		fsinfo = NULL;
	}
	return fsinfo;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

long voluta_parse_size(const char *str)
{
	char *endptr = NULL;
	long double val, mul, iz;

	errno = 0;
	val = strtold(str, &endptr);
	if ((endptr == str) || (errno == ERANGE) || isnan(val)) {
		goto illegal_value;
	}
	if (strlen(endptr) > 1) {
		goto illegal_value;
	}
	switch (toupper(*endptr)) {
	case 'K':
		mul = (double)VOLUTA_UKILO;
		break;
	case 'M':
		mul = (double)VOLUTA_UMEGA;
		break;
	case 'G':
		mul = (double)VOLUTA_UGIGA;
		break;
	case 'T':
		mul = (double)VOLUTA_UTERA;
		break;
	case 'P':
		mul = (double)VOLUTA_UPETA;
		break;
	case '\0':
		mul = 1.0F;
		break;
	default:
		goto illegal_value;
	}
	modfl(val, &iz);
	if ((iz < 0.0F) || isnan(iz)) {
		goto illegal_value;
	}
	return (long)(val * mul);

illegal_value:
	voluta_die(0, "illegal value: %s", str);
	return -EINVAL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * TODO-0014: Dance with systemd upon logout
 *
 * May need to call 'loginctl enable-linger username' if we want daemon to
 * stay alive after login. Need more investigating.
 */
void voluta_daemonize(void)
{
	int err;

	err = daemon(0, 0);
	if (err) {
		voluta_die(0, "failed to daemonize");
	}
	voluta_log_mask_add(VOLUTA_LOG_SYSLOG | VOLUTA_LOG_STDOUT);
}

void voluta_fork_daemon(void)
{
	pid_t pid;

	pid = fork();
	if (pid == -1) {
		voluta_die(errno, "fork error");
	}
	if (pid == 0) {
		voluta_daemonize();
	}
}

void voluta_open_syslog(void)
{
	voluta_log_mask_add(VOLUTA_LOG_SYSLOG);
	openlog(voluta_globals.name, LOG_CONS | LOG_NDELAY, 0);
}

void voluta_close_syslog(void)
{
	if (voluta_log_mask_test(VOLUTA_LOG_SYSLOG)) {
		closelog();
		voluta_log_mask_clear(VOLUTA_LOG_SYSLOG);
	}
}

void voluta_flush_stdout(void)
{
	fflush(stdout);
	fflush(stderr);
}

void voluta_setrlimit_nocore(void)
{
	int err;
	struct rlimit rlim = { .rlim_cur = 0, .rlim_max = 0 };

	err = voluta_sys_setrlimit(RLIMIT_CORE, &rlim);
	if (err) {
		voluta_die(err, "failed to disable core-dupms");
	}
}

void voluta_prctl_non_dumpable(void)
{
	int err;

	err = voluta_sys_prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
	if (err) {
		voluta_die(err, "failed to prctl non-dumpable");
	}
}

char *voluta_strdup_safe(const char *s)
{
	char *d = strdup(s);

	if (d == NULL) {
		voluta_die(errno, "strdup failed");
	}
	return d;
}

void voluta_pfree_string(char **pp)
{
	if (*pp != NULL) {
		free(*pp);
		*pp = NULL;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Singleton instances */
static struct voluta_fs_env *g_fs_env_inst;
static struct voluta_ms_env *g_ms_env_inst;

void voluta_init_fs_env(void)
{
	int err;

	if (g_fs_env_inst) {
		voluta_die(0, "internal error: instance exists");
	}
	err = voluta_fs_env_new(&g_fs_env_inst);
	if (err) {
		voluta_die(err, "failed to create instance");
	}
}

void voluta_fini_fs_env(void)
{
	if (g_fs_env_inst) {
		voluta_fs_env_del(g_fs_env_inst);
		g_fs_env_inst = NULL;
	}
}

struct voluta_fs_env *voluta_fs_env_inst(void)
{
	return g_fs_env_inst;
}


void voluta_init_ms_env(void)
{
	int err;

	if (g_ms_env_inst) {
		voluta_die(0, "internal error: instance exists");
	}
	err = voluta_ms_env_new(&g_ms_env_inst);
	if (err) {
		voluta_die(err, "failed to create instance");
	}
}

void voluta_fini_ms_env(void)
{
	if (g_ms_env_inst) {
		voluta_ms_env_del(g_ms_env_inst);
		g_ms_env_inst = NULL;
	}
}

struct voluta_ms_env *voluta_ms_env_inst(void)
{
	return g_ms_env_inst;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_show_help_and_exit(int exit_code, const char *help_string)
{
	FILE *fp = exit_code ? stderr : stdout;
	const char *name = voluta_globals.name;
	const char *subcmd = voluta_globals.cmd_name;

	if (subcmd) {
		fprintf(fp, "%s %s %s\n", name, subcmd, help_string);
	} else {
		fprintf(fp, "%s %s\n", name, help_string);
	}
	exit(exit_code);
}

void voluta_show_version_and_exit(int exit_code)
{
	FILE *fp = exit_code ? stderr : stdout;

	fprintf(fp, "%s\n", voluta_globals.version);
	exit(exit_code);
}

static void voluta_atexit_flush(void)
{
	fflush(stdout);
	fflush(stderr);
}


static void voluta_error_print_progname(void)
{
	FILE *fp = stderr;
	const char *name = voluta_globals.name;
	const char *subcmd = voluta_globals.cmd_name;

	if (subcmd && (subcmd[0] != '-')) {
		fprintf(fp, "%s %s: ", name, subcmd);
	} else {
		fprintf(fp, "%s: ", name);
	}
	fflush(fp);
}

void voluta_setup_globals(int argc, char *argv[])
{
	voluta_globals.version = voluta_g_config.version;
	voluta_globals.name = program_invocation_short_name;
	voluta_globals.prog = program_invocation_name;
	voluta_globals.argc = argc;
	voluta_globals.argv = argv;
	voluta_globals.cmd_argc = argc;
	voluta_globals.cmd_argv = argv;
	voluta_globals.cmd_name = NULL;
	voluta_globals.pid = getpid();
	voluta_globals.uid = getuid();
	voluta_globals.gid = getgid();
	voluta_globals.umsk = umask(0002);
	voluta_globals.umsk = umask(0002);
	voluta_globals.start_time = time(NULL);
	voluta_globals.dont_daemonize = false;
	voluta_globals.allow_coredump = false;
	voluta_globals.disable_ptrace = true; /* XXX */

	umask(0002);
	setlocale(LC_ALL, "");
	atexit(voluta_atexit_flush);
	error_print_progname = voluta_error_print_progname;
}

static void voluta_resolve_caps(void)
{
	int err = 1;
	pid_t pid;
	cap_t cap;
	cap_flag_value_t flag = CAP_CLEAR;

	pid = getpid();
	cap = cap_get_pid(pid);
	if (cap != NULL) {
		err = cap_get_flag(cap, CAP_SYS_ADMIN, CAP_EFFECTIVE, &flag);
		cap_free(cap);
	}
	voluta_globals.cap_sys_admin = (!err && (flag == CAP_SET));
}

void voluta_init_process(void)
{
	int err;

	err = voluta_lib_init();
	if (err) {
		voluta_die(err, "unable to init lib");
	}
	voluta_log_mask_set(VOLUTA_LOG_INFO | VOLUTA_LOG_WARN |
			    VOLUTA_LOG_ERROR | VOLUTA_LOG_CRIT |
			    VOLUTA_LOG_STDOUT | VOLUTA_LOG_SYSLOG);
	voluta_resolve_caps();
}

void voluta_log_process_info(void)
{
	voluta_log_info("%s", voluta_globals.version);
	voluta_log_info("%s", voluta_globals.prog);
}
