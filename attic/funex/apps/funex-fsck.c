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
#include <unistd.h>

#include "funex-app.h"

#define globals (funex_globals)


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Locals: */
static void funex_fsck_parse_args(int argc, char *argv[]);
static void funex_fsck_execute(void);

static const char *s_fsck_volume = NULL;


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                           *
 *                                                                           *
 *                         The Funex File-System                             *
 *                                                                           *
 *                   File-system's check & repair utility                    *
 *                                                                           *
 *                                                                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
int main(int argc, char *argv[])
{
	/* Begin with defaults */
	funex_init_process(argc, argv);

	/* Parse command-line args */
	funex_fsck_parse_args(argc, argv);

	/* Enable logging */
	funex_setup_logger();

	/* Execute check & repair utility */
	funex_fsck_execute();

	/* Cleanups and Goodbye :) */
	funex_finalize_exit();

	return EXIT_SUCCESS;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_fsck_execute(void)
{
	funex_die_unimplemented("fsck");
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_fsck_getopts(funex_getopts_t *opt)
{
	int c;

	while ((c = funex_getnextopt(opt)) != 0) {
		switch (c) {
			case 'd':
				funex_set_debug_level(opt);
				break;
			case 'r':
				globals.opt.repair = FNX_TRUE;
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
	s_fsck_volume = funex_getnextarg(opt, "volume", FUNEX_ARG_REQ);
}

static const funex_cmdent_t funex_fsck_cmdent = {
	.usage  = "funex-fsck <volume>",
	.optdef = "h,help v,version d,debug= r,repair",
	.helps  = \
	"Run file-system check & repair utility \n\n" \
	"Options: \n" \
	"  -r, --repair           Try to repair defects  \n" \
	"  -d, --debug=<level>    Debug mode level \n" \
	"  -h, --help             Show this help \n"
};


static void funex_fsck_parse_args(int argc, char *argv[])
{
	funex_parse_cmdargs(argc, argv, &funex_fsck_cmdent, funex_fsck_getopts);
}


