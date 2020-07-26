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

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "config-site.h"
#endif

#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <error.h>
#include <locale.h>
#include <getopt.h>

#include "voluta-prog.h"


static void mountd_getopt(void);
static void mountd_enable_signals(void);
static void mountd_boostrap_process(void);
static void mountd_create_setup_env(void);
static void mountd_trace_start(void);
static void mound_execute_ms(void);
static void mountd_finalize(void);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                           *
 *                                                                           *
 *                         Voluta's Mounting-Daemon                          *
 *                                                                           *
 *                                                                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int main(int argc, char *argv[])
{
	/* Setup process defaults */
	voluta_setup_globals(argc, argv);

	/* Parse command-line options */
	mountd_getopt();

	/* Common process initializations */
	voluta_init_process();

	/* Process specific bootstrap sequence */
	mountd_boostrap_process();

	/* Setup enviroment instance */
	mountd_create_setup_env();

	/* Say something */
	mountd_trace_start();

	/* Allow halt by signal */
	mountd_enable_signals();

	/* Execute as long as needed... */
	mound_execute_ms();

	/* Post execution cleanups */
	mountd_finalize();

	/* Goodbye ;) */
	return 0;
}



static void mountd_halt_by_signal(int signum)
{
	/* TODO: halt me */
	voluta_unused(signum);
}

static void mountd_enable_signals(void)
{
	voluta_signal_callback_hook = mountd_halt_by_signal;
	voluta_register_sigactions();
}


static void mountd_boostrap_process(void)
{
	if (!voluta_globals.allow_coredump) {
		voluta_setrlimit_nocore();
	}
	if (!voluta_globals.disable_ptrace) {
		voluta_prctl_non_dumpable();
	}
	atexit(mountd_finalize);
}

static void mountd_create_setup_env(void)
{
	struct voluta_ms_env *ms_env = NULL;

	voluta_init_ms_env();
	ms_env = voluta_ms_env_inst();
	if (ms_env == NULL) {
		voluta_die(0, "ilternal error");
	}
}

static void mountd_trace_start(void)
{
	voluta_log_process_info();
}

static void mountd_finalize(void)
{
	voluta_fini_ms_env();
	voluta_flush_stdout();
}

static void mound_execute_ms(void)
{
	int err;
	struct voluta_ms_env *ms_env = voluta_ms_env_inst();

	err = voluta_ms_env_serve(ms_env);
	if (err) {
		voluta_die(err, "mount-service error");
	}
}


/*: : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : :*/

static const char *voluta_mountd_usage =
	"[options] \n\n" \
	"options: \n" \
	"  -v, --version                Show version and exit\n" \
	"";

static void mountd_getopt(void)
{
	int c = 1;
	int opt_index;
	int argc;
	char **argv;
	const struct option opts[] = {
		{ "version", no_argument, NULL, 'v' },
		{ "help", no_argument, NULL, 'h' },
		{ NULL, no_argument, NULL, 0 },
	};

	argc = voluta_globals.argc;
	argv = voluta_globals.argv;
	while (c > 0) {
		opt_index = 0;
		c = getopt_long(argc, argv, "vh", opts, &opt_index);
		if (c == -1) {
			break;
		}
		if (c == 'v') {
			voluta_show_version_and_exit(0);
		} else if (c == 'h') {
			voluta_show_help_and_exit(0, voluta_mountd_usage);
		} else {
			voluta_die_unsupported_opt();
		}
	}
	voluta_check_no_redundant_arg();
}

