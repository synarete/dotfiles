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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <error.h>
#include <getopt.h>
#include "voluta-prog.h"


static void fsck_finalize(void)
{
	voluta_delete_instance();
	voluta_flush_stdout();
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_execute_fsck(void)
{
	/* Do all cleanups upon exits */
	atexit(fsck_finalize);

	/* TODO: FSCK... */

	/* Post execution cleanups */
	fsck_finalize();
}


/*: : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : :*/

static const char *g_fsck_usage =
	"[options] <volume-path> \n\n" \
	"options: \n" \
	"  -v, --version                Show version and exit\n" \
	"";


void voluta_getopt_fsck(void)
{
	int c = 1;
	int opt_index;
	int argc;
	char **argv;
	const struct option fsck_options[] = {
		{ "version", no_argument, NULL, 'v' },
		{ "help", no_argument, NULL, 'h' },
		{ NULL, no_argument, NULL, 0 },
	};

	argc = voluta_globals.cmd_argc;
	argv = voluta_globals.cmd_argv;
	while (c > 0) {
		opt_index = 0;
		c = getopt_long(argc, argv, "vh",
				fsck_options, &opt_index);
		if (c == -1) {
			break;
		}
		if (c == 'v') {
			voluta_show_version_and_exit(0);
		} else if (c == 'h') {
			voluta_show_help_and_exit(0, g_fsck_usage);
		} else {
			voluta_die_unsupported_opt();
		}
	}
	if (optind >= argc) {
		voluta_die(0, "missing volume path");
	}
	voluta_globals.fsck_volume = argv[optind++];
	if (optind < argc) {
		voluta_die_redundant_arg(argv[optind]);
	}
}

