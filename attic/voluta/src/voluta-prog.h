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
#ifndef VOLUTA_PROG_H_
#define VOLUTA_PROG_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "config-site.h"
#endif

#include <voluta/voluta-syscall.h>
#include <voluta/voluta.h>

/* Mapping from file-system magic type to name & flags */
struct voluta_fsinfo {
	long vfstype;
	const char *name;
	int permitted_mount;
};


typedef void (*voluta_exec_fn)(void);

/* Global settings */
struct voluta_globals {
	/* Program's version string */
	const char *version;

	/* Program's arguments */
	int     argc;
	char  **argv;
	char   *name;
	char   *prog;
	char   *subcmd;

	/* Process ids */
	pid_t   pid;
	uid_t   uid;
	gid_t   gid;
	mode_t  umsk;

	/* Common process settings */
	bool    dont_daemonize;
	bool    allow_coredump;
	bool    disable_ptrace; /* XXX: TODO: allow set */

	/* Signals info */
	int     sig_halt;
	int     sig_fatal;

	/* Execution start-time */
	time_t  start_time;

	/* Capability */
	bool    cap_sys_admin;

	/* Options for 'mount' sub-command */
	char   *mount_passphrase;
	char   *mount_passphrase_file;
	char   *mount_salt;
	char   *mount_volume;
	char   *mount_point;
	char   *mount_point_real;
	char   *mount_subopts;
	char   *mount_tmpfs;
	loff_t  mount_tmpfs_size;

	/* Options for 'mkfs' sub-command */
	char   *mkfs_passphrase;
	char   *mkfs_passphrase_file;
	char   *mkfs_salt;
	char   *mkfs_volume;
	loff_t  mkfs_size;
	bool    mkfs_force;

	/* Sub-command execution hook */
	voluta_exec_fn exec_hook;
};

extern struct voluta_globals voluta_globals;


/* Execution hooks */
void voluta_mount_exec(void);

void voluta_mkfs_exec(void);

void voluta_mkfs_parse_args(int subcmd);

void voluta_mount_parse_args(int subcmd);

/* Common utilities */
voluta_decl_nonreturn_fn
void voluta_die(int errnum, const char *fmt, ...);

void voluta_register_sigactions(void);

long voluta_parse_size(const char *str);

void voluta_daemonize(void);

void voluta_open_syslog(void);

void voluta_close_syslog(void);

void voluta_flush_stdout(void);

void voluta_setrlimit_nocore(void);

void voluta_prctl_non_dumpable(void);

void voluta_pfree_string(char **pp);

void voluta_setup_globals(int argc, char *argv[]);

void voluta_init_process(void);

void voluta_show_help_and_exit(int exit_code, const char *help_string);

void voluta_show_version_and_exit(int exit_code);

/* Resolve file-system info by statfs.f_type */
const struct voluta_fsinfo *voluta_fsinfo_by_vfstype(long vfstype);

/* Singleton instance */
struct voluta_env *voluta_get_instance(void);

void voluta_require_instance(void);

void voluta_delete_instance(void);

/* Signal call-back hook */
typedef void (*voluta_signal_hook_fn)(int);

extern voluta_signal_hook_fn voluta_signal_callback_hook;

/* Passphrase input */
char *voluta_read_passphrase(const char *path, int repeat);
void voluta_free_passphrase(char **pass);

#endif /* VOLUTA_PROG_H_ */
