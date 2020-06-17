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
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <error.h>
#include <getopt.h>
#include "voluta-prog.h"



static void inquiry_finalize(void)
{
	voluta_flush_stdout();
}


static void inquiry_check_params(void)
{
	int err;
	struct stat st;
	const char *path = voluta_globals.inq_path;

	err = voluta_sys_stat(path, &st);
	if (err) {
		voluta_die(err, "stat failure: %s", path);
	}
	if (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode)) {
		voluta_die(0, "not dir-or-reg: %s", path);
	}
}

static void inquiry_send_recv(void)
{
	int err;
	int fd = -1;
	struct voluta_inquiry inq = { .version = 0 };
	const char *path = voluta_globals.inq_path;

	err = voluta_sys_open(path, O_RDONLY, 0, &fd);
	if (err) {
		voluta_die(err, "failed to open: %s", path);
	}
	err = ioctl(fd, VOLUTA_FS_IOC_INQUIRY, &inq);
	voluta_sys_close(fd);
	if (err) {
		voluta_die(err, "ioctl error: %s", path);
	}
	fprintf(stdout, "version = %u\n", inq.version);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_execute_inquiry(void)
{
	/* Do all cleanups upon exits */
	atexit(inquiry_finalize);

	/* Verify user's arguments */
	inquiry_check_params();

	/* Do actual inquiry */
	inquiry_send_recv();

	/* Post execution cleanups */
	inquiry_finalize();
}

/*: : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : :*/

static const char *g_inquiry_usage =
	"[options] <path> \n\n" \
	"options: \n" \
	"  -v, --version                Show version and exit\n" \
	"";


void voluta_getopt_inquiry(void)
{
	int c = 1;
	int opt_index;
	int argc;
	char **argv;
	const struct option inquiry_options[] = {
		{ "version", no_argument, NULL, 'v' },
		{ "help", no_argument, NULL, 'h' },
		{ NULL, no_argument, NULL, 0 },
	};

	argc = voluta_globals.cmd_argc;
	argv = voluta_globals.cmd_argv;
	while (c > 0) {
		opt_index = 0;
		c = getopt_long(argc, argv, "vh", inquiry_options, &opt_index);
		if (c == -1) {
			break;
		}
		if (c == 'v') {
			voluta_show_version_and_exit(0);
		} else if (c == 'h') {
			voluta_show_help_and_exit(0, g_inquiry_usage);
		} else {
			voluta_die_unsupported_opt();
		}
	}
	if (optind >= argc) {
		voluta_die(0, "missing path");
	}
	voluta_globals.inq_path = argv[optind++];
	if (optind < argc) {
		voluta_die_redundant_arg(argv[optind]);
	}
}

