/*
 * This file is part of libvoluta
 *
 * Copyright (C) 2020 Shachar Sharon
 *
 * Libvoluta is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Libvoluta is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 */
#ifndef VOLUTA_API_H_
#define VOLUTA_API_H_

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "defs.h"


struct voluta_fs_env;
struct voluta_ms_env;


struct voluta_config {
	const char *version;
	const char *prefix;
	const char *bindir;
	const char *libdir;
	const char *sysconfdir;
	const char *datadir;
	const char *statedir;
};


struct voluta_stats {
	size_t nalloc_bytes;
	size_t ncache_segs;
	size_t ncache_inodes;
	size_t ncache_vnodes;
};


struct voluta_ucred {
	uid_t  uid;
	gid_t  gid;
	pid_t  pid;
	mode_t umask;
};

struct voluta_params {
	struct voluta_ucred ucred;
	const char *passphrase;
	const char *salt;
	const char *mount_point;
	const char *volume_label;
	const char *volume_name;
	const char *volume_path;
	loff_t volume_size;
	bool tmpfs;
	bool pedantic;
	char pad[6];
};

struct voluta_query {
	uint32_t version;
};


#define VOLUTA_FS_IOC_QUERY   _IOR('F', 1, struct voluta_query)


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

extern const struct voluta_config voluta_g_config;

int voluta_lib_init(void); /* TODO: have fini_lib */

int voluta_make_ascii_passphrase(char *pass, size_t plen);

int voluta_resolve_volume_size(const char *path,
			       loff_t size_want, loff_t *out_size);

int voluta_require_volume_path(const char *path);

int voluta_derive_iv_key_of(const void *pass, const void *salt,
			    struct voluta_iv_key *iv_key);

int voluta_read_vboot_record(const char *path,
			     struct voluta_vboot_record *vbr);

int voluta_check_vboot_record(struct voluta_vboot_record *vbr,
			      const struct voluta_iv_key *iv_key);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* mount-service */
int voluta_ms_env_new(struct voluta_ms_env **out_ms_env);

void voluta_ms_env_del(struct voluta_ms_env *ms_env);

int voluta_ms_env_serve(struct voluta_ms_env *ms_env);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* file-system */
int voluta_fs_env_new(struct voluta_fs_env **out_fs_env);

void voluta_fs_env_del(struct voluta_fs_env *fs_env);

int voluta_fs_env_load(struct voluta_fs_env *fs_env);

int voluta_fs_env_format(struct voluta_fs_env *fs_env);

int voluta_fs_env_serve(struct voluta_fs_env *fs_env);

int voluta_fs_env_verify(struct voluta_fs_env *fs_env);

int voluta_fs_env_reload(struct voluta_fs_env *fs_env);

int voluta_fs_env_term(struct voluta_fs_env *fs_env);

void voluta_fs_env_halt(struct voluta_fs_env *fs_env, int signum);

int voluta_fs_env_sync_drop(struct voluta_fs_env *fs_env);

int voluta_fs_env_setparams(struct voluta_fs_env *fs_env,
			    const struct voluta_params *params);

void voluta_fs_env_stats(const struct voluta_fs_env *fs_env,
			 struct voluta_stats *st);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/


/* commons */
void *voluta_unconst(const void *p);

size_t voluta_clamp(size_t v, size_t lo, size_t hi);

int voluta_dump_backtrace(void);

/* monotonic clock */
void voluta_mclock_now(struct timespec *);

void voluta_mclock_dur(const struct timespec *, struct timespec *);

/* random generator */
void voluta_getentropy(void *, size_t);

/* sysconf wrappers */
size_t voluta_sc_page_size(void);

size_t voluta_sc_phys_pages(void);

size_t voluta_sc_avphys_pages(void);

size_t voluta_sc_l1_dcache_linesize(void);

/* logging */
void voluta_log_mask_set(int mask);

void voluta_log_mask_add(int mask);

void voluta_log_mask_clear(int mask);

int voluta_log_mask_test(int mask);

void voluta_logf(int flags, const char *fmt, ...);


#endif /* VOLUTA_API_H_ */


