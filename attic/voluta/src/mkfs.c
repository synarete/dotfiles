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
	voluta_fini_fs_env();
	voluta_free_passphrase(&voluta_globals.mkfs_passphrase);
	voluta_flush_stdout();
}

static void mkfs_setup_params(void)
{
	int err;
	int exists;
	struct stat st;
	bool force = voluta_globals.mkfs_force;
	loff_t size = voluta_globals.mkfs_volume_size;
	const char *volume_path = voluta_globals.mkfs_volume;
	struct voluta_vboot_record vbr;

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
		err = voluta_read_vboot_record(volume_path, &vbr);
		if (err) {
			voluta_die(err, "volume in use: %s", volume_path);
		}
	}

	err = voluta_resolve_volume_size(volume_path, size,
					 &voluta_globals.mkfs_volume_size);
	if (err) {
		voluta_die(0, "unsupported size: %s", voluta_globals.mkfs_size);
	}
	voluta_globals.mkfs_passphrase =
		voluta_read_passphrase(voluta_globals.mkfs_passphrase_file, 1);
}

static void mkfs_create_setup_fs_env(void)
{
	int err;
	struct voluta_fs_env *fs_env;
	const struct voluta_params params = {
		.ucred = {
			.uid = getuid(),
			.gid = getgid(),
			.pid = getpid(),
			.umask = 0002
		},
		.passphrase = voluta_globals.mkfs_passphrase,
		.salt = voluta_globals.mkfs_salt,
		.volume_label = voluta_globals.mkfs_label,
		.volume_name = voluta_globals.mkfs_name,
		.volume_path = voluta_globals.mkfs_volume,
		.volume_size = voluta_globals.mkfs_volume_size
	};

	voluta_init_fs_env();

	fs_env = voluta_fs_env_inst();
	err = voluta_fs_env_setparams(fs_env, &params);
	if (err) {
		voluta_die(err, "illegal mkfs params");
	}
}

static void mkfs_format_volume(void)
{
	int err;
	struct voluta_fs_env *fs_env;
	const char *volume_path = voluta_globals.mkfs_volume;

	fs_env = voluta_fs_env_inst();
	err = voluta_fs_env_format(fs_env);
	if (err) {
		voluta_die(err, "format error: %s", volume_path);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_execute_mkfs(void)
{
	/* Do all cleanups upon exits */
	atexit(mkfs_finalize);

	/* Verify user's arguments */
	mkfs_setup_params();

	/* Prepare environment */
	mkfs_create_setup_fs_env();

	/* Do actual mkfs */
	mkfs_format_volume();

	/* Post execution cleanups */
	mkfs_finalize();
}


/*: : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : :*/

static const char *g_mkfs_usage =
	"[options] <volume-path> \n\n" \
	"options: \n" \
	"  -s, --size=NBYTES            Volume size\n" \
	"  -l, --label=LABEL            Public label\n" \
	"  -n, --name=NAME              Private name\n" \
	"  -F, --force                  Force overwrite if volume exists\n" \
	"  -P, --passphrase-file=PATH   Passphrase input file (unsafe)\n" \
	"  -S, --salt=SALT              Additional SALT input to passphrase\n" \
	"  -v, --version                Show version and exit\n" \
	"";


void voluta_getopt_mkfs(void)
{
	int c = 1;
	int opt_index;
	int argc;
	char **argv;
	const struct option opts[] = {
		{ "size", required_argument, NULL, 's' },
		{ "label", required_argument, NULL, 'l' },
		{ "name", required_argument, NULL, 'n' },
		{ "force", no_argument, NULL, 'F' },
		{ "passphrase-file", required_argument, NULL, 'P' },
		{ "salt", required_argument, NULL, 'S' },
		{ "version", no_argument, NULL, 'v' },
		{ "help", no_argument, NULL, 'h' },
		{ NULL, no_argument, NULL, 0 },
	};

	argc = voluta_globals.cmd_argc;
	argv = voluta_globals.cmd_argv;
	while (c > 0) {
		opt_index = 0;
		c = getopt_long(argc, argv, "s:l:n:P:S:Fvh", opts, &opt_index);
		if (c == -1) {
			break;
		}
		if (c == 's') {
			voluta_globals.mkfs_size = optarg;
			voluta_globals.mkfs_volume_size =
				voluta_parse_size(optarg);
		} else if (c == 'l') {
			voluta_globals.mkfs_label = optarg;
		} else if (c == 'n') {
			voluta_globals.mkfs_name = optarg;
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
		} else {
			voluta_die_unsupported_opt();
		}
	}
	if (optind >= argc) {
		voluta_die_no_volume_path();
	}
	voluta_globals.mkfs_volume = argv[optind++];
	voluta_check_no_redundant_arg();
}

