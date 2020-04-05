/* SPDX-License-Identifier: LGPL-3.0-or-later */
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
 */
#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <uuid/uuid.h>
#include "voluta-lib.h"


/* Default mode for root inode */
#define VOLUTA_ROOTDIR_MODE (S_IFDIR | 0755)

/* Local variables */
static int g_libvoluta_init;

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_env_setmode(struct voluta_env *env, bool pedantic)
{
	env->qal.pedantic_mode = pedantic;
}

void voluta_env_setcreds(struct voluta_env *env,
			 const struct voluta_ucred *ucred)
{
	struct voluta_ucred *dst_ucred = &env->opi.ucred;

	memcpy(dst_ucred, ucred, sizeof(*dst_ucred));
}

static void env_setup_defaults(struct voluta_env *env)
{
	const struct voluta_ucred self_ucred = {
		.uid = getuid(),
		.gid = getgid(),
		.pid = getpid(),
		.umask = 0002
	};

	voluta_memzero(env, sizeof(*env));
	voluta_env_setcreds(env, &self_ucred);
	env->tmpfs_mode = false;
}

static int env_init_memory_allocator(struct voluta_env *env)
{
	int err;
	size_t memsize = 0;
	const size_t memwant = 4 * VOLUTA_UGIGA;

	err = voluta_resolve_memsize(memwant, &memsize);
	if (err) {
		return err;
	}
	err = voluta_qalloc_init(&env->qal, memsize);
	if (err) {
		return err;
	}
	return 0;
}

static int env_init_cache(struct voluta_env *env)
{
	return voluta_cache_init(&env->cache, &env->qal);
}

static int env_init_super(struct voluta_env *env)
{
	return voluta_sbi_init(&env->sbi, &env->cache, &env->cstore);
}

static int env_init_crypto(struct voluta_env *env)
{
	return voluta_crypto_init(&env->crypto);
}

static int env_init_volume(struct voluta_env *env)
{
	return voluta_cstore_init(&env->cstore, &env->qal, &env->crypto);
}

static int env_init_fusei(struct voluta_env *env)
{
	return voluta_fusei_init(&env->fusei, env);
}

static int env_init(struct voluta_env *env)
{
	int err;

	env_setup_defaults(env);

	err = env_init_memory_allocator(env);
	if (err) {
		return err;
	}
	err = env_init_cache(env);
	if (err) {
		return err;
	}
	err = env_init_super(env);
	if (err) {
		return err;
	}
	err = env_init_crypto(env);
	if (err) {
		return err;
	}
	err = env_init_volume(env);
	if (err) {
		return err;
	}
	err = env_init_fusei(env);
	if (err) {
		return err;
	}
	voluta_burnstack();
	return 0;
}

int voluta_env_new(struct voluta_env **out_env)
{
	int err;
	struct voluta_env *env;

	env = calloc(1, sizeof(*env));
	if (env == NULL) {
		return errno ? -errno : -ENOMEM;
	}
	err = env_init(env);
	if (err) {
		free(env);
		return err;
	}
	*out_env = env;
	return err;
}

static void env_fini(struct voluta_env *env)
{
	voluta_fusei_fini(&env->fusei);
	voluta_cstore_fini(&env->cstore);
	voluta_crypto_fini(&env->crypto);
	voluta_sbi_fini(&env->sbi);
	voluta_cache_fini(&env->cache);
	voluta_qalloc_fini(&env->qal);
	voluta_burnstack();
}

void voluta_env_del(struct voluta_env *env)
{
	voluta_env_term(env);
	env_fini(env);
	memset(env, 0xC8, sizeof(*env));
	free(env);
}

static int env_derive_key(struct voluta_env *env,
			  const struct voluta_passphrase *pass)
{
	struct voluta_sb_info *sbi = sbi_of(env);
	struct voluta_iv_key *iv_key = &sbi->s_iv_key;
	const struct voluta_crypto *crypto = &env->crypto;

	return voluta_derive_iv_key(crypto, pass->pass, strlen(pass->pass),
				    pass->salt, strlen(pass->salt), iv_key);
}

int voluta_env_setup_key(struct voluta_env *env,
			 const char *passphrase, const char *salt)
{
	size_t passlen;
	size_t saltlen;
	struct voluta_passphrase pass;

	if (passphrase == NULL) {
		return -EINVAL;
	}
	passlen = strlen(passphrase);
	if (!passlen || (passlen >= sizeof(pass.pass))) {
		return -EINVAL;
	}
	saltlen = salt ? strlen(salt) : 0;
	if (saltlen >= sizeof(pass.salt)) {
		return -EINVAL;
	}
	voluta_memzero(&pass, sizeof(pass));
	memcpy(pass.pass, passphrase, passlen);
	memcpy(pass.salt, salt, saltlen);

	return env_derive_key(env, &pass);
}

static void generate_random_passphrase(struct voluta_passphrase *pass)
{
	voluta_memzero(pass, sizeof(*pass));

	voluta_fill_random_ascii(pass->pass, sizeof(pass->pass) - 1);
	voluta_fill_random_ascii(pass->salt, sizeof(pass->salt) - 1);
}

int voluta_env_setup_tmpkey(struct voluta_env *env)
{
	struct voluta_passphrase pass;

	env->tmpfs_mode = true;
	generate_random_passphrase(&pass);
	return voluta_env_setup_key(env, pass.pass, pass.salt);
}

static int load_master_record(const struct voluta_sb_info *sbi)
{
	int err;
	struct voluta_master_record mr = {
		.marker = VOLUTA_MASTER_MARK,
		.version = VOLUTA_VERSION,
	};

	err = voluta_pstore_read(sbi->s_pstore, 0, sizeof(mr), &mr);
	if (err) {
		return err;
	}
	if (mr.marker != VOLUTA_MASTER_MARK) {
		return -EFSCORRUPTED;
	}
	if (mr.version != VOLUTA_VERSION) {
		return -EFSCORRUPTED;
	}
	return 0;
}

static int reload_meta_data(struct voluta_env *env)
{
	int err;
	struct voluta_iaddr iaddr;
	struct voluta_inode_info *root_ii = NULL;
	struct voluta_sb_info *sbi = sbi_of(env);

	err = voluta_load_sb(sbi);
	if (err) {
		return err;
	}
	err = voluta_load_usmaps(sbi);
	if (err) {
		return err;
	}
	err = voluta_load_agmaps(sbi);
	if (err) {
		return err;
	}
	err = voluta_load_itable(sbi);
	if (err) {
		return err;
	}
	err = voluta_find_root_ino(sbi, &iaddr);
	if (err) {
		return err;
	}
	err = voluta_stage_inode(sbi, iaddr.ino, &root_ii);
	if (err) {
		return err;
	}
	voluta_bind_root_ino(sbi, i_ino_of(root_ii));
	return 0;
}

static int load_from_volume(struct voluta_env *env)
{
	int err;
	struct voluta_sb_info *sbi = sbi_of(env);
	const char *path = env->volume_path;

	err = voluta_require_volume_path(path);
	if (err) {
		return err;
	}
	err = voluta_pstore_open(sbi->s_pstore, path);
	if (err) {
		return err;
	}
	err = voluta_pstore_flock(sbi->s_pstore);
	if (err) {
		return err;
	}
	err = load_master_record(sbi);
	if (err) {
		return err;
	}
	/* TODO: Wrong logic -- FIXME */
	err = voluta_prepare_space(sbi, path, sbi->s_pstore->size);
	if (err) {
		return err;
	}
	err = reload_meta_data(env);
	if (err) {
		return err;
	}
	return 0;
}

static int load_as_tmpfs(struct voluta_env *env)
{
	int err;

	err = voluta_env_setup_tmpkey(env);
	if (err) {
		return err;
	}
	err = voluta_fs_format(env, NULL, env->volume_size);
	if (err) {
		return err;
	}
	err = voluta_init_itable(&env->sbi); /* revert state */
	if (err) {
		return err;
	}
	err = reload_meta_data(env);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_env_load(struct voluta_env *env)
{
	int err;

	if (env->tmpfs_mode) {
		err = load_as_tmpfs(env);
	} else {
		err = load_from_volume(env);
	}
	voluta_burnstack();
	return err;
}

int voluta_env_exec(struct voluta_env *env)
{
	int err;
	struct voluta_fusei *fusei = &env->fusei;

	err = voluta_fusei_mount(fusei, env->mount_point);
	if (err) {
		return err;
	}
	err = voluta_fusei_session_loop(fusei);
	if (err) {
		return err;
	}
	err = voluta_fusei_umount(fusei);
	if (err) {
		return err;
	}
	voluta_env_drop_caches(env);
	return 0;
}

int voluta_env_term(struct voluta_env *env)
{
	int err;
	struct voluta_sb_info *sbi = sbi_of(env);

	err = voluta_pstore_funlock(sbi->s_pstore);
	if (err) {
		return err;
	}
	voluta_pstore_close(sbi->s_pstore);
	voluta_burnstack();
	return 0;
}

void voluta_env_halt(struct voluta_env *env, int signum)
{
	env->signum = signum;
	voluta_fusei_session_break(&env->fusei);
}

void voluta_env_drop_caches(struct voluta_env *env)
{
	struct voluta_sb_info *sbi = &env->sbi;
	const int flags = VOLUTA_F_SYNC | VOLUTA_F_NOW;

	voluta_commit_dirtyq(sbi, flags);
	voluta_cache_step_cycles(sbi->s_cache, 8);
	voluta_cache_drop(sbi->s_cache);
}

void voluta_env_setparams(struct voluta_env *env, const char *mntpoint,
			  const char *volpath, loff_t volsize)
{
	env->mount_point = mntpoint;
	env->volume_path = volpath;
	env->volume_size = volsize;
	if (env->volume_size && !volpath) {
		env->tmpfs_mode = true;
	}
}

size_t voluta_env_allocated_mem(const struct voluta_env *env)
{
	return env->qal.st.nbytes_used;
}

void voluta_env_cache_stats(const struct voluta_env *env,
			    struct voluta_cache_stat *cst)
{
	const struct voluta_cache *cache = &env->cache;

	cst->nblocks = cache->bcq.count;
	cst->ninodes = cache->icq.count;
	cst->nvnodes = cache->vcq.count;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int format_master_record(const struct voluta_sb_info *sbi)
{
	struct voluta_master_record mr = {
		.marker = VOLUTA_MASTER_MARK,
		.version = VOLUTA_VERSION,
	};

	return voluta_pstore_write(sbi->s_pstore, 0, sizeof(mr), &mr);
}

static void fixup_root_inode(struct voluta_inode_info *ii)
{
	voluta_fixup_rootd(ii);
	i_dirtify(ii);
}

static int format_root_inode(struct voluta_sb_info *sbi,
			     const struct voluta_oper_info *opi)
{
	int err;
	const mode_t mode = VOLUTA_ROOTDIR_MODE;
	struct voluta_inode_info *root_ii = NULL;

	err = voluta_new_inode(sbi, opi, mode, VOLUTA_INO_NULL, &root_ii);
	if (err) {
		return err;
	}
	fixup_root_inode(root_ii);
	voluta_bind_root_ino(sbi, i_ino_of(root_ii));
	return 0;
}

int voluta_fs_format(struct voluta_env *env, const char *path, loff_t size)
{
	int err;
	struct voluta_sb_info *sbi = sbi_of(env);

	err = voluta_prepare_space(sbi, env->volume_path, size);
	if (err) {
		return err;
	}
	err = voluta_pstore_create(sbi->s_pstore, path, size);
	if (err) {
		return err;
	}
	err = voluta_pstore_flock(sbi->s_pstore);
	if (err) {
		return err;
	}
	err = format_master_record(sbi);
	if (err) {
		return err;
	}
	err = voluta_format_sb(sbi);
	if (err) {
		return err;
	}
	err = voluta_format_usmaps(sbi);
	if (err) {
		return err;
	}
	err = voluta_format_agmaps(sbi);
	if (err) {
		return err;
	}
	err = voluta_format_itable(sbi);
	if (err) {
		return err;
	}
	err = format_root_inode(sbi, opi_of(env));
	if (err) {
		return err;
	}
	err = voluta_commit_dirtyq(sbi, VOLUTA_F_SYNC);
	if (err) {
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int errno_or_errnum(int errnum)
{
	return errno ? -errno : -abs(errnum);
}

static int check_sysconf(void)
{
	long val;
	const long page_size_min = VOLUTA_PAGE_SIZE_MIN;
	const long cl_size_min = VOLUTA_CACHELINE_SIZE_MIN;

	errno = 0;
	val = (long)voluta_sc_l1_dcache_linesize();
	if ((val != cl_size_min) || (val % cl_size_min)) {
		return errno_or_errnum(ENOTSUP);
	}
	val = (long)voluta_sc_page_size();
	if ((val < page_size_min) || (val % page_size_min)) {
		return errno_or_errnum(ENOTSUP);
	}
	val = (long)voluta_sc_phys_pages();
	if (val <= 0) {
		return errno_or_errnum(ENOMEM);
	}
	val = (long)voluta_sc_avphys_pages();
	if (val <= 0) {
		return errno_or_errnum(ENOMEM);
	}
	return 0;
}

static int check_system_page_size(void)
{
	size_t page_size;
	const size_t page_shift[] = { 12, 13, 14, 16 };

	page_size = voluta_sc_page_size();
	if (page_size <= VOLUTA_BK_SIZE) {
		for (size_t i = 0; i < ARRAY_SIZE(page_shift); ++i) {
			if (page_size == (1UL << page_shift[i])) {
				return 0;
			}
		}
	}
	return -ENOTSUP;
}

int voluta_lib_init(void)
{
	int err;

	voluta_verify_persistent_format();
	if (g_libvoluta_init) {
		return 0;
	}
	err = check_sysconf();
	if (err) {
		return err;
	}
	err = check_system_page_size();
	if (err) {
		return err;
	}
	err = voluta_init_gcrypt();
	if (err) {
		return err;
	}
	g_libvoluta_init = 1;
	voluta_burnstack();
	return 0;
}
