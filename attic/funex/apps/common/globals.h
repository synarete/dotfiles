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
#ifndef FUNEX_GLOBALS_H_
#define FUNEX_GLOBALS_H_

#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>



/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Process' global settings; discovered and set upon process initialization via
 * command line, or override via conf file.
 */

typedef struct funex_args_ {
	int         argc;
	char      **argv;
	const char *name;

} funex_args_t;


typedef struct funex_globals_ {
	/* [global] */
	funex_args_t    prog;           /* Program's arguments */
	pid_t           pid;            /* Process' system uid */
	uid_t           uid;            /* Process' system uid */
	gid_t           gid;            /* Process' system gid */
	mode_t          umsk;           /* Process' system umask */
	time_t          start;          /* Start time */
	char           *conf;           /* Config-file pathname */
	char           *path;           /* Primary path to operate on */
	char           *path2;          /* Secondary path to operate on */
	char           *head;           /* File-system's root path */

	/* [proc] */
	struct {
		char       *cwd;            /* Current working directory */
		int         debug;          /* Debug-level */
		fnx_bool_t  utility;        /* Run as command-line utility */
		fnx_bool_t  service;        /* Run as a service process */
		fnx_bool_t  nodaemon;       /* Do not daemonize */
		long        core;           /* Core-file limit upon panic */
	} proc;

	/* [sys] */
	struct {
		char       *username;       /* Environemnt for "USER" */
		char       *selfexe;        /* Linux: /proc/self/exe */
		char       *confdir;        /* %{sysconfdir} */
		char       *nodename;       /* utsname.nodename */
	} sys;

	/* [mntd] */
	struct {
		char       *usock;          /* UNIX-domain socket*/
		int         oper;           /* Controls */
	} mntd;

	/* [fs] */
	struct {
		char       *mntpoint;       /* Mount-point */
		char       *name;           /* File-system's name */
		fnx_uuid_t  uuid;           /* File-system's UUID */
		fnx_uid_t   uid;            /* FS-owner UID */
		fnx_gid_t   gid;            /* FS-owner GID */
		fnx_mntf_t  mntf;           /* Mount flags */
		fnx_times_t tms;            /* Timestamps */
	} fs;

	/* [vol] */
	struct {
		char       *path;           /* Volume's path (dev) */
		blkcnt_t    bkcnt;          /* Capacity in blocks */
	} vol;

	/* [log] */
	struct {
		int         trace;          /* Log-trace level */
		fnx_bool_t  verbose;        /* Have verbose logging */
		fnx_bool_t  quiet;          /* Suppress logging */
		fnx_bool_t  stdouts;        /* Use stdout/stderr (!daemon) */
		fnx_bool_t  syslog;         /* Use syslog */
		fnx_bool_t  showprog;       /* Show progname prefix */
		fnx_bool_t  showfunc;       /* Show function name */
		fnx_bool_t  showloc;        /* Show file:line */
		fnx_bool_t  showtimes;      /* Show time-stamps */
	} log;

	/* [options] */
	struct {
		fnx_bool_t  force;          /* Override */
		fnx_bool_t  zfill;          /* Fill with zeros */
		fnx_bool_t  all;            /* Any (--all) */
		fnx_bool_t  safe;           /* Run fsd in safe mode */
		fnx_bool_t  repair;         /* Fsck check+repair */
	} opt;

} funex_globals_t;


/* Globals singleton instance */
extern funex_globals_t   funex_globals;

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Actions on global config settings */
void funex_globals_setup(void);

void funex_globals_cleanup(void);

void funex_globals_byconf(void);


/* Specific global-config setters */
void funex_globals_set_debug(const char *);

void funex_globals_set_verbose(const char *);

void funex_globals_set_core(const char *);

void funex_globals_set_path(const char *);

void funex_globals_set_path2(const char *);

void funex_globals_set_head(const char *);

void funex_globals_set_conf(const char *);

void funex_globals_set_mntf(const char *);

void funex_globals_set_volpath(const char *);

void funex_globals_set_mntpoint(const char *);

void funex_globals_set_fsuuid(const char *);

void funex_globals_set_fsname(const char *);

void funex_globals_set_volsize(const char *);


void funex_clone_str(char **, const char *);

#endif /* FUNEX_GLOBALS_H_ */

