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
#include <sys/resource.h>
#include <sys/capability.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <error.h>
#include <locale.h>
#include <time.h>
#include <getopt.h>

#include "voluta-prog.h"


/* Local functions forward declarations */
static void voluta_parse_args(void);
static void voluta_exec_subcmd(void);


/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                           *
 *                                                                           *
 *                      Voluta: Encrypted Data Vault                         *
 *                                                                           *
 *                                                                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int main(int argc, char *argv[])
{
	/* Setup process defaults */
	voluta_setup_globals(argc, argv);

	/* Parse sub-commands arguments */
	voluta_parse_args();

	/* Common process initializations */
	voluta_init_process();

	/* Execute sub-command by hook */
	voluta_exec_subcmd();

	/* Goodbye ;) */
	return 0;
}

static void voluta_exec_subcmd(void)
{
	voluta_globals.exec_hook();
}

/*: : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : :*/

static const char *g_main_usage =
	"<command> [options]\n\n" \
	"main commands: \n" \
	"  mkfs\n" \
	"  fsck\n" \
	"  mount\n" \
	"  inquiry\n" \
	"";


static void show_help_and_exit(int exit_code, const char *help_string)
{
	printf("%s %s\n", voluta_globals.name, help_string);
	exit(exit_code);
}

static void show_main_help_and_exit(int exit_code)
{
	show_help_and_exit(exit_code, g_main_usage);
}

static bool issubcmd(const char *cmd, const char *s1)
{
	return (cmd && s1 && !strcmp(cmd, s1));
}

static bool voluta_issubcmd(const char *s)
{
	return issubcmd(voluta_globals.subcmd, s);
}

static bool voluta_issubcmd2(const char *s1, const char *s2)
{
	return voluta_issubcmd(s1) || voluta_issubcmd(s2);
}


static void voluta_parse_args(void)
{
	if (voluta_globals.argc <= 1) {
		show_main_help_and_exit(1);
	}
	voluta_globals.subcmd = voluta_globals.argv[1];
	voluta_globals.cmd_argc = voluta_globals.argc - 1;
	voluta_globals.cmd_argv = voluta_globals.argv + 1;

	if (voluta_issubcmd2("-v", "--version")) {
		voluta_show_version_and_exit(0);
	} else if (voluta_issubcmd2("-h", "--help")) {
		show_main_help_and_exit(0);
	} else if (voluta_issubcmd("mount")) {
		voluta_getopt_mount();
		voluta_globals.exec_hook = voluta_execute_mount;
	} else if (voluta_issubcmd("mkfs")) {
		voluta_getopt_mkfs();
		voluta_globals.exec_hook = voluta_execute_mkfs;
	} else if (voluta_issubcmd("fsck")) {
		voluta_getopt_fsck();
		voluta_globals.exec_hook = voluta_execute_fsck;
	} else if (voluta_issubcmd("inquiry")) {
		voluta_getopt_inquiry();
		voluta_globals.exec_hook = voluta_execute_inquiry;
	} else {
		show_main_help_and_exit(1);
	}
}
