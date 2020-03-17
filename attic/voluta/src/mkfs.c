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
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <error.h>
#include <getopt.h>
#include "voluta-prog.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void mkfs_finalize(void)
{
	voluta_delete_instance();
	voluta_free_passphrase(&voluta_globals.mkfs_passphrase);
	voluta_flush_stdout();
}

static void mkfs_prepare(void)
{
	struct voluta_env *env;
	const struct voluta_ucred ucred = {
		.uid = getuid(),
		.gid = getgid(),
		.pid = getpid(),
		.umask = 0002
	};

	env = voluta_get_instance();
	voluta_env_setcreds(env, &ucred);
}

static void mkfs_setup_params(void)
{
	int err, exists;
	struct stat st;
	bool force = voluta_globals.mkfs_force;
	loff_t size = voluta_globals.mkfs_size;
	const char *volume_path = voluta_globals.mkfs_volume;

	err = voluta_sys_stat(volume_path, &st);
	exists = !err;

	if (exists && S_ISDIR(st.st_mode)) {
		voluta_die(-EISDIR, "illegal volume path: %s", volume_path);
	}
	if (exists && !force && !S_ISBLK(st.st_mode)) {
		voluta_die(err, "file exists: %s", volume_path);
	}
	if (err && (err != -ENOENT)) {
		voluta_die(err, "stat failure: %s", volume_path);
	}
	if (exists) {
		err = voluta_require_volume_exclusive(volume_path);
		if (err) {
			voluta_die(err, "volume in use: %s", volume_path);
		}
	}

	err = voluta_resolve_volume_size(volume_path, size,
					 &voluta_globals.mkfs_size);
	if (err) {
		voluta_die(err, "unsupported size: %ld", size);
	}
	voluta_globals.mkfs_passphrase =
		voluta_read_passphrase(voluta_globals.mkfs_passphrase_file, 1);
}

static void mkfs_format_volume(void)
{
	int err;
	const char *volumepath = voluta_globals.mkfs_volume;
	const char *passphrase = voluta_globals.mkfs_passphrase;
	const char *salt = voluta_globals.mkfs_salt;
	const loff_t size = voluta_globals.mkfs_size;
	struct voluta_env *env = voluta_get_instance();

	err = voluta_env_setup_key(env, passphrase, salt);
	if (err) {
		voluta_die(err, "master-key error: %s", volumepath);
	}
	err = voluta_fs_format(env, volumepath, size);
	if (err) {
		voluta_die(err, "format error: %s", volumepath);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_mkfs_exec(void)
{
	/* Do all cleanups upon exits */
	atexit(mkfs_finalize);

	/* Create singleton instance */
	voluta_require_instance();

	/* Prepare globals */
	mkfs_prepare();

	/* Verify user's arguments */
	mkfs_setup_params();

	/* Do actual mkfs */
	mkfs_format_volume();

	/* Post execution cleanups */
	mkfs_finalize();
}


/*: : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : :*/

static const char *g_mkfs_usage =
	"[options] <volume-path> \n\n" \
	"options: \n" \
	"  -s, --size=N                 Volume size in bytes\n" \
	"  -F, --force                  Force overwrite if volume exists\n" \
	"  -P, --passphrase-file=PATH   Passphrase input file (unsafe)\n" \
	"  -S, --salt=SALT              Additional SALT input to passphrase\n" \
	"  -v, --version                Show version and exit\n" \
	"";


void voluta_mkfs_parse_args(int subcmd)
{
	char **argv;
	int c = 1, opt_index, argc;
	const struct option long_options[] = {
		{ "size", required_argument, NULL, 's' },
		{ "force", no_argument, NULL, 'F' },
		{ "passphrase-file", required_argument, NULL, 'P' },
		{ "salt", required_argument, NULL, 'S' },
		{ "version", no_argument, NULL, 'v' },
		{ "help", no_argument, NULL, 'h' },
		{ NULL, no_argument, NULL, 0 },
	};

	argc = voluta_globals.argc;
	argv = voluta_globals.argv;
	if (subcmd) {
		argc--;
		argv++;
	}

	while (c > 0) {
		opt_index = 0;
		c = getopt_long(argc, argv, "s:P:S:Fvh",
				long_options, &opt_index);
		if (c == 's') {
			voluta_globals.mkfs_size = voluta_parse_size(optarg);
		} else if (c == 'P') {
			voluta_globals.mkfs_passphrase_file = optarg;
		} else if (c == 'S') {
			voluta_globals.mkfs_salt = optarg;
		} else if (c == 'F') {
			voluta_globals.mkfs_force = true;
		} else if (c == 'v') {
			voluta_show_version_and_exit(0);
		} else if (c == 'h') {
			voluta_show_help_and_exit(0, g_mkfs_usage);
		} else if (c > 0) {
			voluta_die(0, "unsupported code 0%o", c);
		}
	}
	if (optind >= argc) {
		voluta_die(0, "missing volume path");
	}
	voluta_globals.mkfs_volume = argv[optind++];
	if (optind < argc) {
		voluta_die(0, "redundant argument: %s", argv[optind]);
	}
}

