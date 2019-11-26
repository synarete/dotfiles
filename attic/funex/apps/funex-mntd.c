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
#include <locale.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "funex-app.h"

#define globals (funex_globals)

/* Local funections */
static void funex_mntd_parse_args(int argc, char *argv[]);
static void funex_mntd_set_panic_hook(void);
static void funex_mntd_chkconfig(void);
static void funex_mntd_preinit(void);
static void funex_mntd_bootstrap(void);
static void funex_mntd_execute(void);
static void funex_mntd_shutdown(void);
static void funex_mntd_enable_sighalt(void);
static void funex_mntd_new_mounter(void);
static void funex_mntd_del_mounter(void);
static void funex_mntd_setup_logger(void);


/* Local variables */
static int funex_mntd_started = 0;
static int funex_mntd_halted  = 0;
static int funex_mntd_signum  = 0;
static siginfo_t funex_mntd_siginfo;


/* Singleton instance */
static fnx_mounter_t *funex_mntd_mounter = NULL;


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                           *
 *                                                                           *
 *                         The Funex File-System                             *
 *                                                                           *
 *                       Mounting auxiliary daemon                           *
 *                                                                           *
 *                                                                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
int main(int argc, char *argv[])
{
	/* Begin with defaults */
	funex_init_process(argc, argv);

	/* Bind panic callback hook */
	funex_mntd_set_panic_hook();

	/* Parse command-line args */
	funex_mntd_parse_args(argc, argv);

	/* Override settings by conf-file */
	funex_globals_byconf();

	/* Check settings validity */
	funex_mntd_chkconfig();

	/* Become a daemon process */
	funex_daemonize();

	/* Enable logging facility for daemon */
	funex_mntd_setup_logger();

	/* Set process limits */
	funex_setup_process();

	/* Allow signals */
	funex_mntd_enable_sighalt();

	/* Create singleton instance */
	funex_mntd_new_mounter();

	/* Few extras before actual init */
	funex_mntd_preinit();

	/* Initialize mounting-daemon */
	funex_mntd_bootstrap();

	/* Run as long as active */
	funex_mntd_execute();

	/* Do orderly shutdown */
	funex_mntd_shutdown();

	/* Delete singleton instance */
	funex_mntd_del_mounter();

	/* Global cleanups */
	funex_finalize();

	/* Done here... */
	return EXIT_SUCCESS;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_mntd_new_mounter(void)
{
	fnx_mounter_t **mounter = &funex_mntd_mounter;

	*mounter = funex_new_sobject(sizeof(**mounter));
	fnx_mounter_init(*mounter);
}

static void funex_mntd_del_mounter(void)
{
	fnx_mounter_t **mounter = &funex_mntd_mounter;

	if (*mounter != NULL) {
		fnx_mounter_destroy(*mounter);
		funex_del_sobject(*mounter, sizeof(**mounter));
		*mounter = NULL;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_mntd_set_panic_hook(void)
{
	fnx_set_panic_callback(funex_fatal_error);
}

static void funex_mntd_chkconfig(void)
{
	int cap_mount;
	const char *usock = globals.mntd.usock;

	if (!fnx_isvalid_usock(usock)) {
		funex_dief("illegal-usock: %s", usock);
	}
	cap_mount = funex_cap_is_sys_admin();
	if (!cap_mount) {
		fnx_warn("not CAP_SYS_ADMIN: uid=%d", geteuid());
	}
}

static void funex_mntd_setup_logger(void)
{
	globals.proc.utility = 0;
	globals.proc.service = 1;
	funex_setup_dlogger();
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_mntd_preinit(void)
{
	int rc, fd;
	const char *usock = globals.mntd.usock;

	/* fuse/lib/helper.c requires that we "make sure file descriptors 0, 1 and
	 * 2 are open, otherwise chaos would ensue" */
	do {
		fd = open("/dev/null", O_RDWR);
		if (fd > 2) {
			close(fd);
		}
	} while (fd >= 0 && fd <= 2);

	/* Require unique unix-socket path */
	rc = fnx_sys_stat(usock, NULL);
	if (rc == 0) {
		fnx_info("unlink-prev-sock: %s", usock);
		rc = fnx_sys_unlink(usock); /* TODO: unlink only if peer if dead? */
		if (rc != 0) {
			funex_dief("exists-and-unlinkable: %s", usock);
		}
	}
}

static void funex_mntd_bootstrap(void)
{
	int rc;
	fnx_mounter_t *mounter;
	char const  *usock = globals.mntd.usock;

	fnx_info("mntd-start: version=%s", funex_version());

	mounter = funex_mntd_mounter;
	fnx_info("new-mounter: instance=%p", (void *)mounter);
	rc = fnx_mounter_open(mounter, usock);
	if (rc != 0) {
		fnx_panic("no-mounter-open: %s", usock);
	}
	mounter->owner = globals.uid;
	funex_mntd_started = 1;
	fnx_info("mntd-initialized: usock=%s owner=%d", usock, mounter->owner);
}

static void funex_mntd_shutdown(void)
{
	const char *usock = globals.mntd.usock;

	funex_mntd_halted = 1;
	funex_handle_lastsig(funex_mntd_signum, &funex_mntd_siginfo);

	fnx_mounter_close(funex_mntd_mounter);
	fnx_info("mounter-closed: %s", usock);

	fnx_info("mntd-done: usock=%s version=%s", usock, funex_version());
}

/* Run as long as active. Stay single thread and do not block signals */
static void funex_mntd_execute(void)
{
	size_t cnt = 0;
	fnx_mounter_t *mounter = funex_mntd_mounter;

	funex_sigunblock();
	while (mounter->active && !funex_mntd_signum) {
		fnx_mounter_serv(mounter);
		if ((cnt++ % 10000) == 0) {
			fnx_monitor_mnts(0);
		}
		fnx_msleep(100);
	}
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int funex_mntd_sighalt_callback(int signum,
                                       const siginfo_t *si, void *p)
{
	siginfo_t *fsi = &funex_mntd_siginfo;

	/* Copy signal info into local variables */
	memcpy(fsi, si, sizeof(*fsi));
	funex_mntd_signum = signum;

	/* In all cases, don't got into 100% CPU if multi-signals */
	fnx_msleep(10);
	fnx_unused(p);
	return 0;
}

static void funex_mntd_enable_sighalt(void)
{
	funex_register_sigactions();
	funex_register_sighalt(funex_mntd_sighalt_callback);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_mntd_getopts(funex_getopts_t *opt)
{
	int c;
	const char *arg;

	while ((c = funex_getnextopt(opt)) != 0) {
		switch (c) {
			case 's':
				arg = funex_getoptarg(opt, "usock");
				funex_clone_str(&globals.mntd.usock, arg);
				break;
			case 'Z':
				globals.proc.nodaemon = FNX_TRUE;
				break;
			case 'L':
				funex_show_license_and_goodbye();
				break;
			case 'v':
				funex_show_version_and_goodbye();
				break;
			case 'd':
				arg = funex_getoptarg(opt, "debug");
				funex_globals_set_debug(arg);
				break;
			case 'h':
			default:
				funex_show_cmdhelp_and_goodbye(opt);
				break;
		}
	}
}

static const funex_cmdent_t funex_mntd_cmdent = {
	.usage  = "funex-mntd [-s|--usock=<path>] ",
	.optdef = "h,help d,debug= v,version L,license Z,nodaemon s,usock= ",
	.helps  = \
	"Execute mount-server daemon\n\n" \
	"Options: \n" \
	"  -s, --usock=<path>     UNIX-domain socket for IPC \n"\
	"  -Z, --nodaemon         Do not ran as daemon process \n"\
	"  -v, --version          Show version info \n" \
	"  -d, --debug=<level>    Debug mode level \n"\
	"  -h, --help             Show this help \n"
};

static void funex_mntd_parse_args(int argc, char *argv[])
{
	funex_parse_cmdargs(argc, argv, &funex_mntd_cmdent, funex_mntd_getopts);
}

