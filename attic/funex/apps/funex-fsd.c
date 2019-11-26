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
#include <unistd.h>

#include "funex-app.h"

#define globals (funex_globals)

/* Local funections */
static void funex_fsd_parse_args(int, char *[]);
static void funex_fsd_set_panic_hook(void);
static void funex_fsd_chkconfig(void);
static void funex_fsd_execute(void);
static void funex_fsd_bootstrap(void);
static void funex_fsd_shutdown(void);
static void funex_fsd_enable_sighalt(void);
static void funex_fsd_new_server(void);
static void funex_fsd_del_server(void);
static void funex_fsd_setup_server(void);
static void funex_fsd_setup_logger(void);

/* Local variables */
static int funex_fsd_started = 0;
static int funex_fsd_halted  = 0;
static int funex_fsd_signum  = 0;
static int funex_fsd_sigcnt  = 0;
static siginfo_t funex_fsd_siginfo;

/* Server's singleton objects */
static fnx_server_t *funex_fsd_server   = NULL;
static fnx_fusei_t  *funex_fsd_fusei    = NULL;


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                           *
 *                                                                           *
 *                          The Funex File-System                            *
 *                                                                           *
 *                       File-System's Daemon-Server                         *
 *                                                                           *
 *                                                                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int main(int argc, char *argv[])
{
	/* Begin with defaults */
	funex_init_process(argc, argv);

	/* Bind panic callback hook */
	funex_fsd_set_panic_hook();

	/* Parse command-line args */
	funex_fsd_parse_args(argc, argv);

	/* Override settings by conf-file */
	funex_globals_byconf();

	/* Check validity of config parameters */
	funex_fsd_chkconfig();

	/* Become a daemon process */
	funex_daemonize();

	/* Enable logging facility for service */
	funex_fsd_setup_logger();

	/* Set process limits */
	funex_setup_process();

	/* Allow signals */
	funex_fsd_enable_sighalt();

	/* Create singleton instance */
	funex_fsd_new_server();

	/* Setup server's instance */
	funex_fsd_setup_server();

	/* Bootsrap and mount */
	funex_fsd_bootstrap();

	/* Run as long as active */
	funex_fsd_execute();

	/* Do orderly shutdown */
	funex_fsd_shutdown();

	/* Delete singleton instance */
	funex_fsd_del_server();

	/* Global cleanups */
	funex_finalize();

	/* Enough is enough; be happy ;-) */
	return EXIT_SUCCESS;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_fsd_new_server(void)
{
	fnx_server_t **server = &funex_fsd_server;
	fnx_fusei_t  **fusei  = &funex_fsd_fusei;

	*server = funex_new_sobject(sizeof(**server));
	fnx_server_init(*server);

	*fusei = funex_new_sobject(sizeof(**fusei));
	fnx_fusei_init(*fusei);

	fnx_info("new-server: instance=%p", (void *)(*server));
}

static void funex_fsd_del_server(void)
{
	fnx_server_t **server = &funex_fsd_server;
	fnx_fusei_t  **fusei  = &funex_fsd_fusei;

	if (*server != NULL) {
		fnx_server_destroy(*server);
		funex_del_sobject(*server, sizeof(**server));
		*server = NULL;
	}
	if (*fusei != NULL) {
		fnx_fusei_destroy(*fusei);
		funex_del_sobject(*fusei, sizeof(**fusei));
		*fusei = NULL;
	}
}

static void funex_fsd_setup_server(void)
{
	int rc;
	fnx_uctx_t uctx;
	fnx_server_t *server    = funex_fsd_server;
	fnx_fusei_t  *fusei     = funex_fsd_fusei;
	const fnx_mntf_t mntf   = globals.fs.mntf;

	/* File-systems master user-context */
	uctx.u_root             = 1;
	uctx.u_capf             = FNX_CAPF_ALL;
	uctx.u_cred.cr_pid      = globals.pid;
	uctx.u_cred.cr_uid      = globals.uid;
	uctx.u_cred.cr_gid      = globals.gid;
	uctx.u_cred.cr_umask    = globals.umsk;
	uctx.u_cred.cr_ngids    = 0;

	rc = fnx_server_setup(server, &uctx);
	if (rc != 0) {
		funex_dief("server-setup-failure errno=%d", errno);
	}
	rc = fnx_fusei_setup(fusei, mntf);
	if (rc != 0) {
		funex_dief("fusei-setup-failure mntf=%#lx", mntf);
	}
	fnx_server_assemble(server, fusei, mntf);

	fnx_info("setup-server: uid=%d gid=%d mntf=%#lx",
	         uctx.u_cred.cr_uid, uctx.u_cred.cr_gid, mntf);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_fsd_set_panic_hook(void)
{
	fnx_set_panic_callback(funex_fatal_error);
}

static void funex_fsd_chkconfig(void)
{
	int rc;
	char *real = NULL;
	const char *path;
	const mode_t mask = R_OK | W_OK;

	path = globals.fs.mntpoint;
	rc = fnx_sys_checkmnt(path);
	if (rc != 0) {
		if (rc == -ENOTDIR) {
			funex_dief("not-a-directory: %s", path);
		} else if (rc == -ENOTEMPTY) {
			fnx_error("not-empty: %s", path);
		} else if (rc == -EACCES) {
			fnx_error("no-rwx-access: %s", path);
		} else {
			fnx_error("non-valid-mount-point: %s err=%d", path, -rc);
		}
	}
	rc = fnx_sys_realpath(path, &real);
	if (rc != 0) {
		funex_dief("no-realpath: %s err=%d", path, -rc);
	}
	funex_globals_set_mntpoint(real);
	fnx_sys_freeppath(&real);

	path = globals.vol.path;
	if ((rc = fnx_sys_access(path, R_OK)) != 0) {
		funex_dief("no-read-access: %s err=%d", path, -rc);
	}
	if ((rc = fnx_sys_statvol(path, NULL, mask)) != 0) {
		funex_dief("illegal-vol: %s err=%d", path, -rc);
	}
	rc = fnx_sys_realpath(path, &real);
	if (rc != 0) {
		funex_dief("no-realpath: %s err=%d", path, -rc);
	}
	funex_globals_set_volpath(real);
	fnx_sys_freeppath(&real);

	if (globals.fs.mntf == 0) {
		funex_dief("illegal-mntf: %#lx", globals.fs.mntf);
	}
}

static void funex_fsd_setup_logger(void)
{
	globals.proc.utility = 0;
	globals.proc.service = 1;
	funex_setup_dlogger();
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Run as long as active. Do not block signals for main-thread; it is the only
 * thread which may execute signal-handlers.
 */
static void funex_fsd_start(void)
{
	int rc;
	const char *mntpoint = globals.fs.mntpoint;
	fnx_server_t *server = funex_fsd_server;

	rc = fnx_server_start(server);
	if (rc != 0) {
		fnx_panic("server-start-failed: mntpoint=%s", mntpoint);
	}
	fnx_info("server-started mntpoint=%s", mntpoint);
	funex_fsd_started = 1;
}

static int funex_fsd_isterm(void)
{
	const fnx_server_t *server  = funex_fsd_server;

	if (funex_fsd_signum != 0) {
		fnx_info("last-signum: %d", funex_fsd_signum);
		return FNX_TRUE;
	}
	if (server->state == FNX_SERVSTATE_TERM) {
		fnx_info("server-state: %s", "TERM");
		return FNX_TRUE;
	}
	return FNX_FALSE;
}

static void funex_fsd_waitactive(void)
{
	char uptime[80];
	time_t step = 7200, t1, t0 = 0;
	const char *mntpoint = globals.fs.mntpoint;

	while (!funex_fsd_isterm()) {
		t1 = time(NULL);
		if ((t1 - t0) >= step) {
			funex_uptime_str(uptime, sizeof(uptime));
			fnx_info("fsd-active: uptime=%s mount-point=%s",
			         uptime, mntpoint);
			t0 = t1;
		}
		fnx_msleep(500);
	}
}

static void funex_fsd_execute(void)
{
	funex_fsd_start();
	funex_sigunblock();
	funex_fsd_waitactive();
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_fsd_report_mntf(void)
{
	fnx_substr_t ss;
	char buf[1024] = "";

	fnx_substr_init_rw(&ss, buf, 0, sizeof(buf));
	fnx_repr_mntf(&ss, funex_fsd_server->vproc.mntf, "mount.");
	fnx_log_repr(&ss);
}

static void funex_fsd_report_super(void)
{
	fnx_substr_t ss;
	char buf[1024] = "";

	fnx_substr_init_rw(&ss, buf, 0, sizeof(buf));
	fnx_repr_super(&ss, funex_fsd_server->vproc.super, "super.");
	fnx_log_repr(&ss);
}

static void funex_fsd_report_rootd(void)
{
	fnx_substr_t ss;
	char buf[1024] = "";

	fnx_substr_init_rw(&ss, buf, 0, sizeof(buf));
	fnx_repr_dir(&ss, funex_fsd_server->vproc.rootd, "rootd.");
	fnx_log_repr(&ss);
}

static void funex_fsd_report_fsinfo(void)
{
	funex_fsd_report_mntf();
	funex_fsd_report_super();
	funex_fsd_report_rootd();
}

static void funex_fsd_bootstrap(void)
{
	int rc, have_async;
	fnx_server_t *server = funex_fsd_server;
	char const *mntpoint = globals.fs.mntpoint;
	const char *volume   = globals.vol.path;
	const char *usock    = globals.mntd.usock;

	have_async = !globals.opt.safe;
	rc = fnx_server_open(server, volume, have_async);
	if (rc != 0) {
		fnx_panic("no-server-open: volume=%s", volume);
	}
	funex_fsd_report_fsinfo();
	rc = fnx_server_mount(server, usock, mntpoint);
	if (rc != 0) {
		fnx_panic("no-mount: usock=%s mntpoint=%s", usock, mntpoint);
	}
	fnx_info("server-opened: volume=%s mntpoint=%s", volume, mntpoint);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_fsd_shutdown(void)
{
	const char *mntpoint = globals.fs.mntpoint;

	funex_fsd_halted = 1;
	funex_handle_lastsig(funex_fsd_signum, &funex_fsd_siginfo);

	fnx_server_stop(funex_fsd_server);
	fnx_info("server-stoped: mntpoint=%s", mntpoint);
	fnx_server_close(funex_fsd_server);
	fnx_info("server-closed: mntpoint=%s", mntpoint);
	fnx_server_cleanup(funex_fsd_server);
	fnx_info("server-shutdown: mntpoint=%s", mntpoint);

	fnx_info("fsd-done: version=%s", funex_version());
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int
funex_fsd_sighalt_callback(int signum, const siginfo_t *si, void *p)
{
	siginfo_t *fsi = &funex_fsd_siginfo;

	/* Only the first callback is relevant */
	funex_fsd_sigcnt += 1;
	if (funex_fsd_signum != 0) {
		return (funex_fsd_sigcnt < 100) ? 0 : 1; /* 1 = reraise */
	}

	/* Copy signal info into local variables */
	memcpy(fsi, si, sizeof(*fsi));
	funex_fsd_signum = signum;

	/* Try to wait for init-completion */
	if (!funex_fsd_started) {
		fnx_msleep(1);
	}

	/* Try to let main-thread wake-up and halt */
	if (!funex_fsd_halted) {
		fnx_msleep(1);
	}

	/* In all cases, don't got into 100% CPU if multi-signals */
	fnx_msleep(10);
	fnx_unused(p);
	return 0; /* 0 = No-reraise */
}

static void funex_fsd_enable_sighalt(void)
{
	funex_register_sigactions();
	funex_register_sighalt(funex_fsd_sighalt_callback);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_fsd_getopts(funex_getopts_t *opt)
{
	int c;
	const char *arg;

	while ((c = funex_getnextopt(opt)) != 0) {
		switch (c) {
			case 'o':
				arg = funex_getoptarg(opt, "o");
				funex_globals_set_mntf(arg);
				break;
			case 'R':
				globals.fs.mntf |= FNX_MNTF_RDONLY;
				break;
			case 'S':
				globals.fs.mntf |= FNX_MNTF_STRICT;
				break;
			case 's':
				globals.opt.safe = FNX_TRUE;
				break;
			case 'f':
				arg = funex_getoptarg(opt, "conf");
				funex_globals_set_conf(arg);
				break;
			case 'Z':
				globals.proc.nodaemon = FNX_TRUE;
				break;
			case 'c':
				arg = funex_getoptarg(opt, "core");
				funex_globals_set_core(arg);
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
	arg = funex_getnextarg(opt, "volume", FUNEX_ARG_REQ);
	funex_globals_set_volpath(arg);
	arg  = funex_getnextarg(opt, "mntpoint", FUNEX_ARG_REQ | FUNEX_ARG_LAST);
	funex_globals_set_mntpoint(arg);
}

static const funex_cmdent_t funex_fsd_cmdent = {
	.optdef = \
	"h,help d,debug= v,version L,license f,conf= R,rdonly Z,nodaemon " \
	"o,options= S,strict c,core, s,safe",
	.usage  = \
	"funex-fsd [--rdonly] [-o opts] <volume> <mntpoint> ",
	.helps  = \
	"Mount file-system daemon \n\n" \
	"Options: \n" \
	"  -f, --conf=<file>      Use configuration-file \n"\
	"  -R, --rdonly           Read-only mode \n"\
	"  -o, --option=<mntf>    Comma separated mount options \n"\
	"  -Z, --nodaemon         Do not ran as daemon process \n"\
	"  -S, --strict           Non-permissive mode for inode attributes \n"\
	"  -s, --safe             Run in safe slow-motion mode \n"\
	"  -c, --core=<size>      Core-dump file-size limit (0=no-core) \n" \
	"  -d, --debug=<level>    Debug mode level \n"\
	"  -v, --version          Show version info \n"\
	"  -h, --help             Show this help \n"
};

static void funex_fsd_parse_args(int argc, char *argv[])
{
	funex_parse_cmdargs(argc, argv, &funex_fsd_cmdent, funex_fsd_getopts);
}

