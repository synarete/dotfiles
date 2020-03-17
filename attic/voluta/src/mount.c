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
#include <sys/statvfs.h>
#include <sys/vfs.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <error.h>
#include <getopt.h>

#include "voluta-prog.h"


static void mount_seup_env_params(void)
{
	struct voluta_env *env = voluta_get_instance();

	if (voluta_globals.mount_volume) {
		voluta_env_setparams(env, voluta_globals.mount_point_real,
				     voluta_globals.mount_volume, 0);
	} else {
		voluta_env_setparams(env, voluta_globals.mount_point_real,
				     NULL, voluta_globals.mount_tmpfs);
	}
}

static void halt_by_signal(int signum)
{
	struct voluta_env *env = voluta_get_instance();

	if (env) {
		voluta_env_halt(env, signum);
	}
}

static void mount_enable_signals(void)
{
	voluta_signal_callback_hook = halt_by_signal;
	voluta_register_sigactions();
}

static int mount_execute_fs(void)
{
	int err;
	struct voluta_env *env = voluta_get_instance();

	err = voluta_env_load(env);
	if (err) {
		/* XXX no here -- do it in main */
		voluta_die(err, "load failure: %s",
			   voluta_globals.mount_volume);
		return err;
	}
	err = voluta_env_exec(env);
	if (err) {
		/* XXX no here -- do it in main */
		voluta_die(err, "mount failure: %s",
			   voluta_globals.mount_point);
		return err;
	}
	err = voluta_env_term(env);
	if (err) {
		/* XXX no here -- do it in main */
		voluta_die(err, "shutdown error: %s",
			   voluta_globals.mount_volume);
		return err;
	}
	return 0;
}

static void mount_cleanup_all(void)
{
	voluta_delete_instance();
	voluta_pfree_string(&voluta_globals.mount_point_real);
	voluta_pfree_string(&voluta_globals.mount_subopts);
	voluta_free_passphrase(&voluta_globals.mount_passphrase);
	voluta_close_syslog();
	voluta_flush_stdout();
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void mount_setup_params(void)
{
	int err;
	struct stat st;
	char *mount_point, *mount_point_real;

	mount_point = voluta_globals.mount_point;
	err = voluta_sys_stat(mount_point, &st);
	if (err) {
		voluta_die(err, "stat failure: %s", mount_point);
	}
	if (!S_ISDIR(st.st_mode)) {
		voluta_die(-ENOTDIR, "illegal mount-point: %s", mount_point);
	}
	mount_point_real = realpath(mount_point, NULL);
	if (!mount_point_real) {
		voluta_die(errno, "realpath failure: %s", mount_point);
	}
	err = voluta_sys_stat(mount_point_real, &st);
	if (err) {
		voluta_die(err, "stat failure: %s", mount_point_real);
	}
	err = voluta_sys_access(mount_point, R_OK | W_OK | X_OK);
	if (err) {
		voluta_die(err, "access failure: %s", mount_point);
	}
	voluta_globals.mount_point_real = mount_point_real;
}

static void mount_check_mntpoint(void)
{
	int err, dir_fd = -1;
	size_t ndirents = 0;
	const char *mntp;
	struct statfs stfs;
	struct dirent64 de[8];
	const size_t nde = VOLUTA_ARRAY_SIZE(de);
	const struct voluta_fsinfo *fsinfo = NULL;
	char buf[1024] = "";

	mntp = voluta_globals.mount_point_real;
	err = voluta_sys_statfs(mntp, &stfs);
	if (err) {
		voluta_die(err, "statfs failure: %s", mntp);
	}
	fsinfo = voluta_fsinfo_by_vfstype(stfs.f_type);
	if (!fsinfo) {
		voluta_die(0, "illegal vfstype at: %s", mntp);
	}
	if (!fsinfo->permitted_mount) {
		voluta_die(0, "can not mount on %s: %s",
			   fsinfo->name, mntp);
	}
	err = voluta_sys_open(mntp, O_DIRECTORY | O_RDONLY, 0, &dir_fd);
	if (err) {
		voluta_die(err, "open-dir error: %s", mntp);
	}
	err = voluta_sys_getdents(dir_fd, buf, sizeof(buf), de, nde, &ndirents);
	if (err) {
		voluta_die(err, "read dir failure: %s", mntp);
	}
	err = voluta_sys_close(dir_fd);
	if (err) {
		voluta_die(err, "close-dir error: %s", mntp);
	}
	if (ndirents > 2) {
		voluta_die(0, "mount point not empty: %s", mntp);
	}
}

static void check_tmpfs_backend(void)
{
	int err;
	loff_t size = voluta_globals.mount_tmpfs;
	const char *volume_path = voluta_globals.mount_volume;

	err = voluta_resolve_volume_size(volume_path, size,
					 &voluta_globals.mount_tmpfs);
	if (err) {
		voluta_die(0, "illegal tmpfs size: %ld", size);
	}
}

static void check_volume_backend(void)
{
	int err;
	const char *path = voluta_globals.mount_volume;

	err = voluta_require_volume_path(path);
	if (err) {
		voluta_die(err, "illegal volume: %s", path);
	}
	err = voluta_require_volume_exclusive(path);
	if (err) {
		voluta_die(err, "failed to access-lock volume: %s", path);
	}
}

static void mount_check_backend_storage(void)
{
	if (voluta_globals.mount_tmpfs) {
		check_tmpfs_backend();
	} else {
		check_volume_backend();
	}
}

static void setup_tmpfs_passphrase(void)
{
	int err;
	struct voluta_env *env = voluta_get_instance();

	err = voluta_env_setup_tmpkey(env);
	if (err) {
		voluta_die(err, "random passphrase error: %s",
			   voluta_globals.mount_volume);
	}
}

static void setup_volume_passphrase(void)
{
	int err;
	struct voluta_env *env = voluta_get_instance();

	voluta_globals.mount_passphrase =
		voluta_read_passphrase(voluta_globals.mount_passphrase_file, 0);

	err = voluta_env_setup_key(env, voluta_globals.mount_passphrase,
				   voluta_globals.mount_salt);
	if (err) {
		voluta_die(err, "passphrase error: %s",
			   voluta_globals.mount_volume);
	}
}

static void mount_setup_passphrase(void)
{
	if (voluta_globals.mount_tmpfs) {
		setup_tmpfs_passphrase();
	} else {
		setup_volume_passphrase();
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void mount_prepare_process(void)
{
	if (!voluta_globals.dont_daemonize) {
		voluta_daemonize();
		voluta_open_syslog();
	}
	if (!voluta_globals.allow_coredump) {
		voluta_setrlimit_nocore();
	}
	if (!voluta_globals.disable_ptrace) {
		voluta_prctl_non_dumpable();
	}
}

/*
 * Trace global setting to user. When running as daemon on systemd-based
 * environments, users should use the following command to view voluta's
 * traces:
 *
 *   $ journalctl -b -n 60 -f -t voluta
 */
static void mount_trace_start(void)
{
	voluta_tr_info("program: %s", voluta_globals.prog);
	voluta_tr_info("version: %s", voluta_globals.version);
	voluta_tr_info("mount-point: %s", voluta_globals.mount_point_real);
	if (voluta_globals.mount_volume) {
		voluta_tr_info("backend-storage: %s",
			       voluta_globals.mount_volume);
	}
}

static void mount_trace_finish(void)
{
	voluta_tr_info("mount done: %s", voluta_globals.mount_point_real);
	voluta_tr_info("execution time: %ld seconds",
		       time(NULL) - voluta_globals.start_time);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_mount_exec(void)
{
	/* Do all cleanups upon exits */
	atexit(mount_cleanup_all);

	/* Prepare and resolve mount settings */
	mount_setup_params();

	/* Require empty valid mount-point */
	mount_check_mntpoint();

	/* Require valid back-end storage device */
	mount_check_backend_storage();

	/* Create singleton instance */
	voluta_require_instance();

	/* Ask user to provide passphrase */
	mount_setup_passphrase();

	/* Become daemon process */
	mount_prepare_process();

	/* Report beginning-of-mount */
	mount_trace_start();

	/* Initialize */
	mount_seup_env_params();

	/* Allow halt by signal */
	mount_enable_signals();

	/* Execute as long as needed... */
	mount_execute_fs();

	/* Report end-of-mount */
	mount_trace_finish();

	/* Post execution cleanups */
	mount_cleanup_all();

	/* Return to main for global cleanups */
}

/*: : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : :*/

static const char *g_mount_usage =
	"[options] <storage-path> <mount-point> \n\n" \
	"options: \n" \
	"  -t, --tmpfs=N                Memory only file-system\n" \
	"  -D, --nodaemon               Do not run as daemon process\n" \
	"  -C, --coredump               Allow core-dumps upon fatal errors\n" \
	"  -P, --passphrase-file=PATH   Passphrase input file (unsafe)\n" \
	"  -S, --salt=SALT              Additional SALT input to passphrase\n" \
	"";

void voluta_mount_parse_args(int subcmd)
{
	char **argv;
	int c, opt_index, argc;
	const struct option long_options[] = {
		{ "options", required_argument, NULL, 'o' },
		{ "tmpfs", required_argument, NULL, 't' },
		{ "nodaemon", no_argument, NULL, 'D' },
		{ "coredump", no_argument, NULL, 'C' },
		{ "passphrase-file", required_argument, NULL, 'P' },
		{ "salt", required_argument, NULL, 'S' },
		{ "help", no_argument, NULL, 'h' },
		{ NULL, no_argument, NULL, 0 },
	};

	argc = voluta_globals.argc;
	argv = voluta_globals.argv;
	if (subcmd) {
		argc--;
		argv++;
	}

	while (true) {
		opt_index = 0;
		c = getopt_long(argc, argv, "o:t:P:S:DCh",
				long_options, &opt_index);
		if (c < 0) {
			break;
		} else if (c == 'o') {
			/* TODO: use xfstests' subopts: 'rw,context=' */
			voluta_globals.mount_subopts = strdup(optarg);
		} else if (c == 'P') {
			voluta_globals.mount_passphrase_file = optarg;
		} else if (c == 'S') {
			voluta_globals.mount_salt = optarg;
		} else if (c == 't') {
			voluta_globals.mount_tmpfs = voluta_parse_size(optarg);
		} else if (c == 'D') {
			voluta_globals.dont_daemonize = true;
		} else if (c == 'C') {
			voluta_globals.allow_coredump = true;
		} else if (c == 'h') {
			voluta_show_help_and_exit(0, g_mount_usage);
		} else {
			voluta_die(0, "unsupported code 0%o", c);
		}
	}
	if (voluta_globals.mount_tmpfs) {
		if (optind >= argc) {
			voluta_die(0, "missing mount-point");
		}
		voluta_globals.mount_point = argv[optind++];
	} else {
		if (optind >= argc) {
			voluta_die(0, "missing storage path");
		}
		voluta_globals.mount_volume = argv[optind++];
		if (optind >= argc) {
			voluta_die(0, "missing mount-point");
		}
		voluta_globals.mount_point = argv[optind++];
	}
	if (optind < argc) {
		voluta_die(0, "redundant argument: %s", argv[optind]);
	}
}

