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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "funex-app.h"

#define globals (funex_globals)


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Locals forward declarations: */
static void funex_show_parse_args(int argc, char *argv[]);
static void funex_show_execute(void);

static const char *s_show_subcmd = NULL;
static const char *s_show_path = NULL;


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                           *
 *                                                                           *
 *                         The Funex File-System                             *
 *                                                                           *
 *                   Show-settings command-line utility                      *
 *                                                                           *
 *                                                                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
int main(int argc, char *argv[])
{
	/* Begin with defaults */
	funex_init_process(argc, argv);

	/* Parse command-line args */
	funex_show_parse_args(argc, argv);

	/* Enable logging (default-mode) */
	funex_setup_plogger();

	/* Execute sub-command */
	funex_show_execute();

	/* Cleanups and Goodbye :) */
	funex_finalize_exit();

	return EXIT_SUCCESS;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_show_stats(void)
{
	int rc;
	char *psroot;

	funex_globals_set_head(s_show_path ? s_show_path : globals.proc.cwd);
	if ((rc = fnx_sys_access(globals.head, R_OK)) != 0) {
		funex_dief("no-access: %s %d", globals.head, -rc);
	}
	psroot = fnx_sys_joinpath(globals.head, FNX_PSROOTNAME);
	if ((rc = fnx_sys_access(psroot, R_OK)) != 0) {
		funex_dief("no-access: %s %d", psroot, -rc);
	}
	funex_show_pseudofs(globals.head);
	fnx_sys_freepath(psroot);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_show_attr(void)
{
	int rc, fd = -1;
	fnx_iocargs_t args;
	const fnx_iocdef_t *ioc;
	const char *path = s_show_path;

	if (path == NULL) {
		funex_die_missing_arg("pathname");
	}
	ioc = fnx_iocdef_by_opc(FNX_VOP_FQUERY);
	if (!ioc || (ioc->size != sizeof(args))) {
		funex_dief("unsupported");
	}
	rc = fnx_sys_open(path, O_RDONLY | O_NOATIME, 0, &fd);
	if (rc != 0) {
		funex_dief("no-open: %s %d", path, -rc);
	}
	fnx_bzero(&args, sizeof(args));
	rc = ioctl(fd, ioc->cmd, &args);
	if (rc != 0) {
		funex_dief("ioctl-error: %s %d", path, errno);
	}
	fnx_sys_close(fd);
	funex_print_iattr(&args.fquery_res.iattr, NULL);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_show_execute(void)
{
	if (s_show_subcmd != NULL) {
		if (!strcmp(s_show_subcmd, "stats")) {
			funex_show_stats();
		} else if (!strcmp(s_show_subcmd, "attr")) {
			funex_show_attr();
		} else {
			funex_die_illegal_arg(s_show_subcmd, NULL);
		}
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_show_getopts(funex_getopts_t *opt)
{
	int c;
	const char *arg;

	while ((c = funex_getnextopt(opt)) != 0) {
		switch (c) {
			case 'X':
				printf("-v\n");
				printf("--version\n");

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
	arg = funex_getnextarg(opt, NULL, FUNEX_ARG_OPT);
	s_show_subcmd = arg;
	arg = funex_getnextarg(opt, NULL, FUNEX_ARG_OPT | FUNEX_ARG_LAST);
	s_show_path = arg;
}

static const funex_cmdent_t funex_show_cmdent = {
	.usage  = "funex-show [stats|attr] [<pathname>]",
	.optdef = "h,help v,version L,license", \
	.helps  = \
	"Show various parameters \n\n" \
	"Options: \n" \
	"  -L, --license          Show license info \n"\
	"  -v, --version          Show version info \n"\
	"  -h, --help             Show this help \n"
};

static void funex_show_parse_args(int argc, char *argv[])
{
	funex_parse_cmdargs(argc, argv, &funex_show_cmdent, funex_show_getopts);
}


