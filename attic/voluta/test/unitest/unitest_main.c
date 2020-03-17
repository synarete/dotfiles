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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <error.h>
#include <errno.h>
#include <locale.h>
#include <getopt.h>

#include "unitest.h"

/* Local variables */
static int g_ut_argc;
static char **g_ut_argv;
static const char *g_ut_test_name;
static struct timespec g_start_ts;


/* Local functions */
static void voluta_ut_setup_globals(int argc, char *argv[]);
static void voluta_ut_show_version(void);
static void voluta_ut_show_done(void);
static void voluta_ut_parse_args(void);
static void voluta_ut_setup_tracing(void);


/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                           *
 *                                                                           *
 *                        Voluta unit-testing program                        *
 *                                                                           *
 *                                                                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int main(int argc, char *argv[])
{
	/* Setup process defaults */
	voluta_ut_setup_globals(argc, argv);

	/* Disable tracing */
	voluta_ut_setup_tracing();

	/* Parse common line arguments */
	voluta_ut_parse_args();

	/* Show generic info */
	voluta_ut_show_version();

	/* Actual tests execution... */
	voluta_ut_execute(g_ut_test_name);

	/* ...and we are done! */
	voluta_ut_show_done();

	return 0;
}

static void voluta_ut_setup_globals(int argc, char *argv[])
{
	g_ut_argc = argc;
	g_ut_argv = argv;

	umask(0002);
	setlocale(LC_ALL, "");
	voluta_mclock_now(&g_start_ts);
}

static void voluta_ut_setup_tracing(void)
{
	voluta_g_trace_flags =
		(VOLUTA_TRACE_ERROR | VOLUTA_TRACE_CRIT | VOLUTA_TRACE_STDOUT);
}

static void voluta_ut_show_version(void)
{
	printf("%s\n", voluta_version_string);
}

static void voluta_ut_show_done(void)
{
	struct timespec dur;

	voluta_mclock_dur(&g_start_ts, &dur);
	printf("%s: done (%ld.%03lds)\n", program_invocation_short_name,
	       dur.tv_sec, dur.tv_nsec / 1000000L);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const char *ut_usage =
	"\n\n" \
	"options: \n" \
	" -t, --test=<name>     Run tests which contains name\n" \
	" -v, --version         Show version info\n";

static void show_help_and_exit(void)
{
	printf("%s %s\n", program_invocation_short_name, ut_usage);
	exit(EXIT_SUCCESS);
}

static void show_version_and_exit(void)
{
	voluta_ut_show_version();
	exit(EXIT_SUCCESS);
}

static void voluta_ut_parse_args(void)
{
	int c, opt_index, getnext = 1;
	struct option long_opts[] = {
		{ "test", required_argument, NULL, 't' },
		{ "version", no_argument, NULL, 'v' },
		{ "help", no_argument, NULL, 'h' },
		{ NULL, no_argument, NULL, 0 },
	};

	while (getnext) {
		opt_index = 0;
		c = getopt_long(g_ut_argc, g_ut_argv, "vht:",
				long_opts, &opt_index);
		if (c == -1) {
			getnext = 0;
		} else if (c == 't') {
			g_ut_test_name = optarg;
		} else if (c == 'v') {
			show_version_and_exit();
		} else if (c == 'h') {
			show_help_and_exit();
		} else {
			error(EXIT_FAILURE, 0, "bad option 0%o", c);
		}
	}
	if (optind < g_ut_argc) {
		error(EXIT_FAILURE, 0,
		      "redundant argument: %s", g_ut_argv[optind]);
	}
}


