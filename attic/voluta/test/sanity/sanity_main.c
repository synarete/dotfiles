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
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>
#include <error.h>
#include <errno.h>
#include <locale.h>

#include "sanity.h"

struct voluta_t_globals {
	int     argc;
	char  **argv;
	char   *workdir_root;
	char   *workdir_real;
	char   *test_name;
	int     repeat_count;
	int     tests_bitmask;
	bool    no_check_statvfs;
	bool    do_extra_tests;
	bool    random_order;
	bool    list_tests;
};

/* Global settings */
static struct voluta_t_globals voluta_t_g_globals;

/* Real-path resolved buffer */
static char voluta_t_dirpath_buf[PATH_MAX];

/* Execution environment context */
static struct voluta_t_ctx *voluta_t_g_ctx;

/* Local functions */
static void voluta_t_setup_globals(int argc, char *argv[]);
static void voluta_t_parse_args(void);
static void voluta_t_verify_args(void);
static void voluta_t_pre_execute(void);
static void voluta_t_post_execute(void);
static void voluta_t_execute_all(void);
static void voluta_t_register_sigactions(void);
static void voluta_t_show_version(void);


/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                           *
 *                                                                           *
 *                Voluta's file-system sanity-testing utility                *
 *                                                                           *
 *                                                                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int main(int argc, char *argv[])
{
	/* Setup process defaults */
	voluta_t_setup_globals(argc, argv);

	/* Parse command-line arguments */
	voluta_t_parse_args();

	/* Check program's parameters */
	voluta_t_verify_args();

	/* Signal handling (ignored) */
	voluta_t_register_sigactions();

	/* Prepare execution */
	voluta_t_pre_execute();

	/* Actual tests execution */
	voluta_t_execute_all();

	/* Final cleanups */
	voluta_t_post_execute();

	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void voluta_t_error_print_progname(void)
{
	FILE *fp = stderr;

	fprintf(fp, "%s: ", program_invocation_short_name);
	fflush(fp);
}

static void voluta_t_atexit_cleanup(void)
{
	voluta_t_g_ctx = NULL;
	memset(&voluta_t_g_globals, 0, sizeof(voluta_t_g_globals));
}

static void voluta_t_setup_globals(int argc, char *argv[])
{
	umask(0002);
	setlocale(LC_ALL, "");
	atexit(voluta_t_atexit_cleanup);
	error_print_progname = voluta_t_error_print_progname;

	voluta_t_g_globals.argc = argc;
	voluta_t_g_globals.argv = argv;
	voluta_t_g_globals.repeat_count = 1;
	voluta_g_log_mask =
		(VOLUTA_LOG_INFO | VOLUTA_LOG_WARN |
		 VOLUTA_LOG_ERROR | VOLUTA_LOG_CRIT | VOLUTA_LOG_STDOUT);
}

static int voluta_t_tests_mask(void)
{
	int mask = VOLUTA_T_NORMAL | VOLUTA_T_VERIFY;

	if (voluta_t_g_globals.do_extra_tests) {
		mask |= VOLUTA_T_IO_EXTRA;
		mask |= VOLUTA_T_IO_CLONE;
	}
	if (voluta_t_g_globals.no_check_statvfs) {
		mask &= ~VOLUTA_T_VERIFY;
	}
	if (voluta_t_g_globals.random_order) {
		mask |= VOLUTA_T_RANDOM;
	}
	return mask;
}

static void voluta_t_pre_execute(void)
{
	size_t size;

	size = sizeof(*voluta_t_g_ctx);
	voluta_t_g_ctx = (struct voluta_t_ctx *)malloc(size);
	if (voluta_t_g_ctx == NULL) {
		error(EXIT_FAILURE, errno, "malloc %lu-nbytes failed", size);
	}
	voluta_t_g_globals.tests_bitmask = voluta_t_tests_mask();
}

static void voluta_t_post_execute(void)
{
	free(voluta_t_g_ctx);
	voluta_t_g_ctx = NULL;
}

static void voluta_t_execute_all(void)
{
	struct voluta_t_params params = {
		.progname = program_invocation_short_name,
		.workdir = voluta_t_g_globals.workdir_real,
		.testname = voluta_t_g_globals.test_name,
		.testsmask = voluta_t_g_globals.tests_bitmask,
		.repeatn = voluta_t_g_globals.repeat_count,
		.listtests = voluta_t_g_globals.list_tests
	};

	t_ctx_init(voluta_t_g_ctx, &params);
	t_ctx_exec(voluta_t_g_ctx);
	t_ctx_fini(voluta_t_g_ctx);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void sigaction_noop_handler(int signum)
{
	voluta_unused(signum);
}

static struct sigaction s_sigaction_noop = {
	.sa_handler = sigaction_noop_handler
};

static void register_sigaction(int signum, struct sigaction *sa)
{
	int err;

	err = voluta_sys_sigaction(signum, sa, NULL);
	if (err) {
		error(EXIT_FAILURE, err, "sigaction error: %d", signum);
	}
}

static void sigaction_noop(int signum)
{
	register_sigaction(signum, &s_sigaction_noop);
}

static void voluta_t_register_sigactions(void)
{
	sigaction_noop(SIGHUP);
	sigaction_noop(SIGTRAP);
	sigaction_noop(SIGUSR1);
	sigaction_noop(SIGUSR2);
	sigaction_noop(SIGPIPE);
	sigaction_noop(SIGALRM);
	sigaction_noop(SIGCHLD);
	sigaction_noop(SIGCONT);
	sigaction_noop(SIGURG);
	sigaction_noop(SIGPROF);
	sigaction_noop(SIGWINCH);
	sigaction_noop(SIGIO);
	/* GC specifics */
	sigaction_noop(SIGPWR);
	sigaction_noop(SIGXCPU);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const char *voluta_t_usage =
	"[options] <dirpath>\n\n" \
	"options: \n" \
	" -t, --test=<name>         Run tests which contains name\n" \
	" -n, --repeat=<count>      Execute tests count times\n" \
	" -e, --extra               Use extra tests\n" \
	" -r, --random              Run tests in random order\n" \
	" -C, --nostatvfs           Do not check statvfs between tests\n" \
	" -l, --list                List tests names\n"
	" -v, --version             Show version info\n";

static void show_help_and_exit(void)
{
	printf("%s %s\n", program_invocation_short_name, voluta_t_usage);
	exit(EXIT_SUCCESS);
}

static void show_version_and_exit(void)
{
	voluta_t_show_version();
	exit(EXIT_SUCCESS);
}

static void voluta_t_parse_args(void)
{
	int c, opt_index, getnext = 1;
	struct option long_opts[] = {
		{ "test", required_argument, NULL, 't' },
		{ "repeat", required_argument, NULL, 'n' },
		{ "extra", no_argument, NULL, 'e' },
		{ "random", no_argument, NULL, 'r' },
		{ "nostatvfs", no_argument, NULL, 'C' },
		{ "list", no_argument, NULL, 'l' },
		{ "version", no_argument, NULL, 'v' },
		{ "help", no_argument, NULL, 'h' },
		{ NULL, no_argument, NULL, 0 },
	};

	while (getnext) {
		opt_index = 0;
		c = getopt_long(voluta_t_g_globals.argc,
				voluta_t_g_globals.argv,
				"t:n:erClvh", long_opts, &opt_index);
		if (c == -1) {
			getnext = 0;
		} else if (c == 't') {
			voluta_t_g_globals.test_name = optarg;
		} else if (c == 'n') {
			voluta_t_g_globals.repeat_count = atoi(optarg);
		} else if (c == 'e') {
			voluta_t_g_globals.do_extra_tests = true;
		} else if (c == 'r') {
			voluta_t_g_globals.random_order = true;
		} else if (c == 'C') {
			voluta_t_g_globals.no_check_statvfs = true;
		} else if (c == 'l') {
			voluta_t_g_globals.list_tests = true;
		} else if (c == 'v') {
			show_version_and_exit();
		} else if (c == 'h') {
			show_help_and_exit();
		} else {
			error(EXIT_FAILURE, 0, "bad option 0%o", c);
		}
	}

	if (voluta_t_g_globals.list_tests) {
		return;
	}
	if (optind >= voluta_t_g_globals.argc) {
		error(EXIT_FAILURE, 0, "missing root pathname");
	}
	voluta_t_g_globals.workdir_root = voluta_t_g_globals.argv[optind++];
	if (optind < voluta_t_g_globals.argc) {
		error(EXIT_FAILURE, 0, "redundant argument: %s",
		      voluta_t_g_globals.argv[optind]);
	}
	if (!realpath(voluta_t_g_globals.workdir_root, voluta_t_dirpath_buf)) {
		error(EXIT_FAILURE, errno, "no realpath: %s",
		      voluta_t_g_globals.workdir_root);
	}
	voluta_t_g_globals.workdir_real = voluta_t_dirpath_buf;
}

static void voluta_t_verify_args(void)
{
	int err;
	struct stat st;
	const char *base = voluta_t_g_globals.workdir_root;

	if (voluta_t_g_globals.list_tests) {
		return;
	}
	err = voluta_sys_stat(base, &st);
	if (err) {
		error(EXIT_FAILURE, err, "no stat: %s", base);
	}
	if (!S_ISDIR(st.st_mode)) {
		error(EXIT_FAILURE, 0, "not a directory: %s", base);
	}
}

static void voluta_t_show_version(void)
{
	printf("%s\n", voluta_version_string);
}

