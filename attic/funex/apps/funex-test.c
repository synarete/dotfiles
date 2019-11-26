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
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "funex-app.h"
#include "testsuite/graybox.h"


#define globals (funex_globals)

/* Locals */
static void funex_test_parse_args(int argc, char *argv[]);
static void funex_test_set_panic_hook(void);
static void funex_test_setproc(void);
static void funex_test_execute(void);
static void funex_test_prepare(void);
static void funex_test_setup_logger(void);

static int funex_test_mask = 0;
static int funex_test_nitr = 1;


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                           *
 *                                                                           *
 *                         The Funex File-System                             *
 *                                                                           *
 *                           Test-Suite Utility                              *
 *                                                                           *
 *                                                                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
int main(int argc, char *argv[])
{
	/* Begin with defaults */
	funex_init_process(argc, argv);

	/* Bind panic callback hook */
	funex_test_set_panic_hook();

	/* Parse command-line args */
	funex_test_parse_args(argc, argv);

	/* Set process defaults & limits */
	funex_test_setproc();

	/* Enable printing-logging */
	funex_setup_plogger();

	/* Check settings validity */
	funex_test_prepare();

	/* Configure tests-logger */
	funex_test_setup_logger();

	/* Run as long as active */
	funex_test_execute();

	/* Cleanups and Goodbye :) */
	funex_finalize_exit();

	/* Done. */
	return EXIT_SUCCESS;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_test_set_panic_hook(void)
{
	fnx_set_panic_callback(funex_fatal_error);
}

static void funex_test_setproc(void)
{
	umask(S_IWGRP);

	/* TODO - core etc. */
}


static void funex_test_prepare(void)
{
	int rc;
	char *path, *real;

	path = globals.path;
	if ((rc = fnx_sys_statdir(path, NULL)) != 0) {
		funex_dief("illegal-dir: %s err=%d", path, -rc);
	}
	if ((rc = fnx_sys_realpath(path, &real)) != 0) {
		funex_dief("no-realpath: %s err=%d", path, -rc);
	}
	if (strlen(real) > (PATH_MAX / 2)) {
		funex_dief("base-too-long: %s", real);
	}
	funex_globals_set_path(real);
	funex_globals_set_head(real);
	fnx_sys_freepath(real);

	if (funex_test_mask == 0) {
		funex_test_mask = GBX_POSIX; /* Default setting: minimal tests */
	}
}

static void funex_test_execute(void)
{
	const char *base = globals.path;

	gbx_info("Version: %s ", funex_version());
	for (int i = 0; i < funex_test_nitr; ++i) {
		gbx_info("Start[%d]: %s ", i, base);
		gbx_run(base, funex_test_mask);
		gbx_info("Done[%d]: %s ", i, base);
	}
}

static void funex_test_setup_logger(void)
{
	globals.proc.utility    = 1;
	globals.proc.service    = 0;
	globals.log.showtimes   = 1;
	funex_setup_logger();
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int funex_test_getnitr_or_die(funex_getopts_t *opt)
{
	int rc, nitr = 0;
	const char *arg;
	const char *key = "loop";

	arg = funex_getoptarg(opt, key);
	rc = funex_parse_int(arg, &nitr);
	if (rc != 0) {
		funex_die_illegal_arg(key, arg);
	}
	return nitr;
}

static int funex_test_getxio_or_die(funex_getopts_t *opt)
{
	int mask = 0;
	const char *arg;
	const char *key = "xio";

	arg = funex_getoptarg(opt, key);
	if (!strcmp(arg, "single")) {
		mask |= GBX_SIO;
	} else if (!strcmp(arg, "multi")) {
		mask |= GBX_MIO;
	} else if (!strcmp(arg, "all")) {
		mask |= GBX_SIO | GBX_MIO;
	} else {
		funex_die_illegal_arg(key, arg);
	}
	return mask;
}

static void funex_test_getopts(funex_getopts_t *opt)
{
	int c, mask = 0, verify = 1;
	const char *arg;

	while ((c = funex_getnextopt(opt)) != 0) {
		switch (c) {
			case 'p':
				mask |= GBX_POSIX;
				break;
			case 'x':
				mask |= funex_test_getxio_or_die(opt);
				break;
			case 'S':
				mask |= GBX_STRESS;
				break;
			case 'C':
				mask |= GBX_CUSTOM;
				break;
			case 'R':
				mask |= GBX_RANDOM;
				break;
			case 'k':
				mask |= GBX_KEEPGO;
				break;
			case 'B':
				verify = 0;
				break;
			case 'a':
				mask |= GBX_ALL;
				break;
			case 'N':
				funex_test_nitr = funex_test_getnitr_or_die(opt);
				break;
			case 'V':
				arg = funex_getoptarg(opt, "verbose");
				funex_globals_set_verbose(arg);
				break;
			case 'L':
				funex_show_license_and_goodbye();
				break;
			case 'v':
				funex_show_version_and_goodbye();
				break;
			case 'h':
			default:
				funex_show_cmdhelp_and_goodbye(opt);
				break;
		}
	}
	if (!mask) {
		mask |= GBX_POSIX | GBX_SIO;
	}
	if (!verify) {
		mask &= ~GBX_VERIFY;
	} else {
		mask |= GBX_VERIFY;
	}

	arg = funex_getnextarg(opt, "path", FUNEX_ARG_LAST);
	if (arg != NULL) {
		funex_globals_set_path(arg);
	} else {
		funex_globals_set_path(globals.proc.cwd);
	}

	funex_test_mask = mask;
}

static const funex_cmdent_t funex_test_cmdent = {
	.usage  = "funex-test [<testdir>]",
	.optdef = \
	"h,help v,version L,license a,all p,posix x,xio= "\
	"S,stress C,custom V,verbose= R,random N,loop= B,no-verify k,keep-go",
	.helps  = \
	"Run Funex file-system sanity test-suite. If no options are given, \n"\
	"runs POSIX and I/O tests. If no <testdir> is given, uses current \n"\
	"working directory \n\n" \
	"Options: \n" \
	"  -p, --posix            Run POSIX compatibility tests\n"\
	"  -x, --xio=<mode>       Run I/O tests (single|multi|all)\n"\
	"  -S, --stress           Run stress tests \n"\
	"  -C, --custom           Run custom tests (Funex specific)\n"\
	"  -a, --all              Run all tests \n"\
	"  -R, --random           Random-shuffle tests ordering \n"\
	"  -N, --loop=<count>     Repeat tests 'count' iterations \n"\
	"  -B, --no-verify        Do not try to verify fs between tests \n"\
	"  -k, --keep-go          Keep-going upon failures \n"\
	"  -V, --verbose=<level>  Verbose mode (0|1|2)\n"\
	"  -L, --license          Show license info \n"\
	"  -v, --version          Show version info \n"\
	"  -h, --help             Show this help \n"
};

static void funex_test_parse_args(int argc, char *argv[])
{
	funex_parse_cmdargs(argc, argv, &funex_test_cmdent, funex_test_getopts);
}
