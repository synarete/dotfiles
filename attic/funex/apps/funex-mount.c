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
#include <libgen.h>
#include <unistd.h>

#include "funex-app.h"

#define globals (funex_globals)

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Locals */
static void funex_mount_parse_args(int argc, char *argv[]);
static void funex_mount_execute(void);

static int s_mount_list = 0;
static int s_mount_umount = 0;
static int s_mount_all = 0;
static const char *s_mount_volume = NULL;
static const char *s_mount_mntpoint = NULL;


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                           *
 *                                                                           *
 *                         The Funex File-System                             *
 *                                                                           *
 *              Attach of detach to/from file-system hierarchy               *
 *                                                                           *
 *                                                                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
int main(int argc, char *argv[])
{
	/* Begin with defaults */
	funex_init_process(argc, argv);

	/* Parse command-line args */
	funex_mount_parse_args(argc, argv);

	/* Enable logging */
	funex_setup_plogger();

	/* Execute sub-command */
	funex_mount_execute();

	/* Cleanups and Goodbye :) */
	funex_finalize_exit();

	return EXIT_SUCCESS;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const char *err2str(int err)
{
	const char *str;

	err = abs(err);
	if (err == ENOTCONN) {
		str = "!";
	} else if (err == EACCES) {
		str = "?";
	} else if (err != 0) {
		str = "&";
	} else {
		str = " ";
	}
	return str;
}

static void funex_mount_list(void)
{
	int rc, all = s_mount_all;
	dev_t dev;
	size_t i, nent = 0;
	fnx_sys_mntent_t mnts[128]; /* TODO: Define me */

	rc = fnx_sys_listmnts(mnts, fnx_nelems(mnts), FNX_MNTFSTYPE, &nent);
	if (rc != 0) {
		funex_dief("no-list-mounts: err=%d", -rc);
	}
	for (i = 0; i < nent; ++i) {
		if (all) {
			rc = fnx_sys_statdev(mnts[i].target, NULL, &dev);
			fnx_info("%s %u.%u %s %s", err2str(rc), major(dev), minor(dev),
			         mnts[i].target, mnts[i].options);
		} else {
			fnx_info("%s", mnts[i].target);
		}
	}
	fnx_sys_freemnts(mnts, nent);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void do_umount(const char *mntp)
{
	int rc, fd = -1;
	size_t nwr = 0;
	char *path = NULL;

	rc = fnx_sys_stat(mntp, NULL);
	if (rc != 0) {
		fnx_error("no-stat: %s %d", mntp, -rc);
		goto out;
	}
	path = fnx_sys_makepath(mntp, FNX_PSROOTNAME, FNX_PSHALTNAME, NULL);
	if (path == NULL) {
		fnx_error("no-makepath: %s", mntp);
		goto out;
	}
	rc = fnx_sys_access(path, R_OK | W_OK);
	if (rc != 0) {
		fnx_error("no-access: %s %d", path, -rc);
		goto out;
	}
	rc = fnx_sys_open(path, O_RDWR, 0, &fd);
	if (rc != 0) {
		fnx_error("no-open: %s %d", path, -rc);
		goto out;
	}
	rc = fnx_sys_write(fd, "0", 1, &nwr);
	if (rc != 0) {
		fnx_error("no-write: %s %d", path, -rc);
		goto out;
	}
	fnx_sys_fsync(fd);
	fnx_sys_close(fd);
	fd = -1;

	/* Triggers final ops */
	for (size_t i = 0; (i < 100) && (rc == 0); i++) {
		rc = fnx_sys_access(path, F_OK);
		fnx_msleep(100);
	}
	/* Expect fsd to be off */
	rc = fnx_sys_access(path, F_OK);
	if (rc == 0) {
		fnx_error("still-active: %s", mntp);
	}
out:
	if (fd > 0) {
		fnx_sys_close(fd);
	}
	if (path != NULL) {
		fnx_sys_freepath(path);
	}
}

static void funex_mount_umount(void)
{
	size_t i, nent = 0;
	fnx_sys_mntent_t mnts[128];

	if (s_mount_all) {
		fnx_sys_listmnts(mnts, fnx_nelems(mnts), FNX_MNTFSTYPE, &nent);
		for (i = 0; i < nent; ++i) {
			do_umount(mnts[i].target);
		}
		fnx_sys_freemnts(mnts, nent);
	} else {
		do_umount(s_mount_mntpoint);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_mount_execv(void)
{
	int rc;
	char *dirp, *path;
	char *argv[8];

	dirp = dirname(globals.prog.argv[0]);
	path = strcat(dirp, "/funex-fsd");
	argv[0] = path;
	argv[1] = strdup(s_mount_volume);
	argv[2] = strdup(s_mount_mntpoint);
	argv[3] = NULL;

	rc = execv(path, argv);
	if (rc != 0) {
		funex_dief("no-execv: %s", path);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_mount_execute(void)
{
	if (s_mount_list) {
		funex_mount_list();
	} else if (s_mount_umount) {
		funex_mount_umount();
	} else {
		funex_mount_execv();
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_mount_getopts(funex_getopts_t *opt)
{
	int c;
	const char *arg = NULL;

	while ((c = funex_getnextopt(opt)) != 0) {
		switch (c) {
			case 'a':
				s_mount_all = 1;
				break;
			case 'u':
				s_mount_umount = 1;
				break;
			case 'l':
				s_mount_list = 1;
				break;
			case 'd':
				funex_set_debug_level(opt);
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

	if (s_mount_umount) {
		arg = funex_getnextarg(opt, "mntpoint", FUNEX_ARG_LAST);
		s_mount_mntpoint = arg;
	} else if (s_mount_list) {
		arg = funex_getnextarg(opt, "", FUNEX_ARG_NONE);
	} else {
		arg = funex_getnextarg(opt, "volume", FUNEX_ARG_REQ);
		s_mount_volume = arg;
		arg = funex_getnextarg(opt, "mntpoint", FUNEX_ARG_REQ | FUNEX_ARG_LAST);
		s_mount_mntpoint = arg;
	}
}


const funex_cmdent_t funex_mount_cmdent = {
	.optdef = "h,help v,version a,all u,umount l,list",
	.usage  = \
	"funex-mount <volume> <mntpoint> \n" \
	"funex-mount -u [-a] [<mntpoint>] \n" \
	"funex-mount --list \n",
	.helps  = \
	"Attach/detach to/from file-system hierarchy, or show list of currently \n" \
	"mounted Funex file-systems. \n\n" \
	"Options: \n" \
	"  -u, --umount           Detach a mounted file-system \n" \
	"  -a, --all              Detach all Funex file-systems \n" \
	"  -l, --list             List currently mounted file-systems \n" \
	"  -h, --help             Show this help \n" \
};


static void funex_mount_parse_args(int argc, char *argv[])
{
	funex_parse_cmdargs(argc, argv, &funex_mount_cmdent, funex_mount_getopts);
}
