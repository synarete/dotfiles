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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fnxserv.h>
#include "process.h"
#include "parse.h"
#include "cmdline.h"
#include "globals.h"

/* Globals singleton  instance */
funex_globals_t  funex_globals;

/* Shorter */
#define globals (funex_globals)


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void funex_clone_str(char **ptr, const char *str)
{
	char *new_str, *prev_str;

	new_str = NULL;
	if (str != NULL) {
		new_str = strdup(str);
		if (new_str == NULL) {
			funex_dief("no-strdup: %s", str);
		}
	}
	prev_str = *ptr;
	if (prev_str != NULL) {
		free(prev_str);
	}
	*ptr = new_str;
}

static void funex_release_str(char **ptr)
{
	if (*ptr != NULL) {
		free(*ptr);
		*ptr = NULL;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void funex_globals_setup(void)
{
	memset(&globals, 0, sizeof(globals));
	globals.pid             = getpid();
	globals.uid             = geteuid();
	globals.gid             = getegid();
	globals.umsk            = umask(0);
	globals.start           = time(NULL);
	globals.mntd.usock      = funex_mntusock();
	globals.proc.cwd        = get_current_dir_name();
	globals.sys.username    = funex_username();
	globals.sys.selfexe     = funex_selfexe();
	globals.sys.nodename    = funex_nodename();
	globals.sys.confdir     = funex_sysconfdir();
	globals.fs.mntf         = FNX_MNTF_DEFAULT;
	fnx_uuid_clear(&globals.fs.uuid);
}

void funex_globals_cleanup(void)
{
	char **strs[] = {
		&globals.conf,
		&globals.path,
		&globals.path2,
		&globals.head,
		&globals.mntd.usock,
		&globals.proc.cwd,
		&globals.sys.username,
		&globals.sys.selfexe,
		&globals.sys.nodename,
		&globals.sys.confdir,
		&globals.fs.mntpoint,
		&globals.fs.name,
		&globals.vol.path
	};

	for (size_t i = 0; i < FNX_NELEMS(strs); ++i) {
		funex_release_str(strs[i]);
	}
	memset(&globals, 0, sizeof(globals));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void funex_globals_set_debug(const char *arg)
{
	int rc;
	long n = -1;

	rc = funex_parse_long(arg, &n);
	if (rc != 0) {
		funex_dief("illegal-debug-value: %s", arg);
	}
	if ((n < 0) || (n > 3)) {
		funex_dief("debug-out-of-range: %s (valid: 0,1,2,3)", arg);
	}
	globals.proc.debug = (int)n;
}

void funex_globals_set_verbose(const char *arg)
{
	int rc;
	long n = -1;

	rc = funex_parse_long(arg, &n);
	if (rc != 0) {
		funex_dief("illegal-verbose-value: %s", arg);
	}
	if ((n < 0) || (n > 2)) {
		funex_dief("verbose-out-of-range: %s (valid: 0,1,2)", arg);
	}
	globals.log.trace = (int)n;
}

void funex_globals_set_core(const char *arg)
{
	int rc;
	long n = -1;

	rc = funex_parse_long(arg, &n);
	if (rc != 0) {
		funex_dief("illegal-core-value: %s", arg);
	}
	if ((n < 0) || (n > (long)FNX_GIGA)) {
		funex_dief("out-of-range: %s", arg);
	}
	globals.proc.core = n;
}

void funex_globals_set_path(const char *path)
{
	funex_clone_str(&globals.path, path);
}

void funex_globals_set_path2(const char *path)
{
	funex_clone_str(&globals.path2, path);
}

void funex_globals_set_head(const char *path)
{
	int rc;

	if (path == NULL) {
		funex_dief("missing-path");
	}
	rc = fnx_sys_fsboundary(path, &globals.head);
	if (rc != 0) {
		funex_dief("unknown-fsboundary: %s", path);
	}
}

void funex_globals_set_conf(const char *arg)
{
	int rc;

	if ((rc = fnx_sys_statreg(arg, NULL)) != 0) {
		funex_dief("no-reg: %s err=%d", arg, rc);
	}
	funex_clone_str(&globals.conf, arg);
}


void funex_globals_set_mntf(const char *arg)
{
	int rc;

	rc = funex_parse_mntops(arg, &globals.fs.mntf);
	if (rc != 0) {
		funex_dief("illegal-mount-options: %s", arg);
	}
}

void funex_globals_set_volpath(const char *arg)
{
	funex_clone_str(&globals.vol.path, arg);
}

void funex_globals_set_mntpoint(const char *arg)
{
	funex_clone_str(&globals.fs.mntpoint, arg);
}

void funex_globals_set_fsuuid(const char *arg)
{
	int rc;

	rc = funex_parse_uuid(arg, &globals.fs.uuid);
	if (rc != 0) {
		funex_dief("illegal-uuid: %s", arg);
	}
}

static int funex_name_isvalid(const char *name)
{
	return ((name != NULL) && (strlen(name) <= FNX_NAME_MAX));
}

void funex_globals_set_fsname(const char *arg)
{
	if (!funex_name_isvalid(arg)) {
		funex_dief("illegal-name: %s", arg);
	}
	funex_clone_str(&globals.fs.name, arg);
}

void funex_globals_set_volsize(const char *arg)
{
	int rc;
	loff_t sz = 0;
	blkcnt_t *bkcnt = &globals.vol.bkcnt;
	const loff_t minsz = (loff_t)FNX_VOLSIZE_MIN;
	const loff_t maxsz = (loff_t)FNX_VOLSIZE_MAX;

	rc = funex_parse_loff(arg, &sz);
	if ((rc != 0) || (sz < minsz) || (sz > maxsz)) {
		funex_die_illegal_volsize(arg, sz);
	}

	/* Chop to section boundaries */
	*bkcnt = (blkcnt_t)fnx_volsz_to_bkcnt(sz);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int isequal(const char *s1, const char *s2)
{
	return (strcmp(s1, s2) == 0);
}

static char parse_bool(const char *arg)
{
	int rc, b = FNX_FALSE;

	rc = funex_parse_bool(arg, &b);
	if (rc != 0) {
		funex_dief("illegal-bool-value: %s", arg);
	}
	return (char)b;
}

static void parse_conf_line(const char *path, const char *line, int num)
{
	int rc;
	char key[FNX_NAME_MAX + 1] = "";
	char val[FNX_PATH_MAX / 2] = "";

	rc = funex_parse_kv(line, key, sizeof(key), val, sizeof(val));
	if (rc != 0) {
		funex_dief("parse-line-error: %s:%d", path, num);
	}

	if (isequal(key, "")) {
		return;
	} else if (isequal(key, "debug")) {
		funex_globals_set_debug(val);
	} else if (isequal(key, "verbose")) {
		globals.log.verbose = parse_bool(val);
	} else if (isequal(key, "nodaemon")) {
		globals.proc.nodaemon  = parse_bool(val);
	} else if (isequal(key, "core")) {
		funex_globals_set_core(val);
	} else if (isequal(key, "usock")) {
		funex_clone_str(&globals.mntd.usock, val);
	} else {
		funex_dief("unknown-conf-key: %s", key);
	}
}

/* Load specific globals via conf file */
static void load_conf_file(const char *path)
{
	int rc = 0;
	FILE *fp;
	int num, lim;
	char *line = NULL;
	size_t n = 0;
	ssize_t len;

	fp = fopen(path, "r");
	if (fp == NULL) {
		funex_dief("failed-to-open-cfgfile: %s", path);
	}
	num = 0;
	lim = 10000; /* Max lines in cfg file */
	while (!feof(fp) && (num++ < lim)) {
		len = getline(&line, &n, fp);
		if (len < 0) {
			if (ferror(fp)) {
				rc = -1;
			}
			break;
		}
		if (len > 1) {
			parse_conf_line(path, line, num);
		}
	}
	if (line != NULL) {
		free(line);
	}
	fclose(fp);
	if (rc != 0) {
		funex_dief("read-cfgfile-error: %s", path);
	}
}

void funex_globals_byconf(void)
{
	if (globals.conf != NULL) {
		load_conf_file(globals.conf);
	}
}



