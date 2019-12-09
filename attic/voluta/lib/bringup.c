/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * This file is part of libvoluta
 *
 * Copyright (C) 2019 Shachar Sharon
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


/*
 * ISSUE-0000 (INFO): Sample issue
 *
 * An example of a way to document isses within the code. First line of issue's
 * comment must be in the format of: ISSUE-<NUM> (<TYPE>): <DESC>, where:
 *   <NUM> is a 4 digits unique issue number
 *   <TYPE> is the issue's type (BUG, FIXME, TODO, INFO)
 *   <DESC> is a short human-readable description
 */


/* Default mode for root inode */
#define VOLUTA_ROOTDIR_MODE (S_IFDIR | 0755)

/* Local variables */
static int g_libvoluta_init;

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_env_setmode(struct voluta_env *env, bool pedantic)
{
	env->qmal.pedantic_mode = pedantic;
}

void voluta_env_setcreds(struct voluta_env *env,
			 const struct voluta_ucred *ucred)
{
	struct voluta_ucred *dst_ucred = &env->ucred;

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

	memset(env, 0, sizeof(*env));
	voluta_env_setcreds(env, &self_ucred);
	env->tmpfs_mode = false;
	env->iconv_handle = (iconv_t)(-1);
}

static int env_init_memory_allocator(struct voluta_env *env)
{
	int err;
	size_t memsize = 0, memwant = 4 * VOLUTA_GIGA;

	err = voluta_resolve_memsize(memwant, &memsize);
	if (err) {
		return err;
	}
	err = voluta_qmalloc_init(&env->qmal, memsize);
	if (err) {
		return err;
	}
	return 0;
}

static int env_init_cache(struct voluta_env *env)
{
	return voluta_cache_init(&env->cache, &env->qmal);
}

static int env_open_iconv(struct voluta_env *env)
{
	int err = 0;

	/* Using UTF32LE to avoid BOM (byte-order-mark) character */
	env->iconv_handle = iconv_open("UTF32LE", "UTF8");
	if (env->iconv_handle == (iconv_t)(-1)) {
		err = errno ? -errno : -ENOTSUP;
	}
	return err;
}

static void env_close_iconv(struct voluta_env *env)
{
	if (env->iconv_handle == (iconv_t)(-1)) {
		iconv_close(env->iconv_handle);
		env->iconv_handle = (iconv_t)(-1);
	}
}

static int env_init_super(struct voluta_env *env)
{
	return voluta_sbi_init(&env->sbi, &env->cache, &env->vdi);
}

static int env_init_crypto(struct voluta_env *env)
{
	return voluta_crypto_init(&env->crypto);
}

static int env_init_volume(struct voluta_env *env)
{
	return voluta_vdi_init(&env->vdi, &env->qmal, &env->crypto);
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
	err = env_open_iconv(env);
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
	env_close_iconv(env);
	voluta_fusei_fini(&env->fusei);
	voluta_vdi_fini(&env->vdi);
	voluta_crypto_fini(&env->crypto);
	voluta_sbi_fini(&env->sbi);
	voluta_cache_fini(&env->cache);
	voluta_qmalloc_fini(&env->qmal);
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
	size_t passlen, saltlen;
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
	memset(&pass, 0, sizeof(pass));
	memcpy(pass.pass, passphrase, passlen);
	memcpy(pass.salt, salt, saltlen);

	return env_derive_key(env, &pass);
}

static int load_master_record(const struct voluta_sb_info *sbi)
{
	int err;
	struct voluta_master_record mr = {
		.marker = VOLUTA_MASTER_MARK,
		.version = VOLUTA_VERSION,
	};

	err = voluta_vdi_read(sbi->s_vdi, 0, sizeof(mr), &mr);
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

	err = voluta_load_super_block(sbi);
	if (err) {
		return err;
	}
	err = voluta_load_ag_space(sbi);
	if (err) {
		return err;
	}
	err = voluta_load_inode_table(sbi);
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
	err = voluta_vdi_open(&env->vdi, path);
	if (err) {
		return err;
	}
	err = load_master_record(sbi);
	if (err) {
		return err;
	}
	/* TODO: Wrong logic -- FIXME */
	err = voluta_prepare_space(sbi, path, env->vdi.size);
	if (err) {
		return err;
	}
	err = reload_meta_data(env);
	if (err) {
		return err;
	}
	return 0;
}

static void fill_random_pass(struct voluta_passphrase *pass)
{
	memset(pass, 0, sizeof(*pass));

	voluta_fill_random_ascii(pass->pass, sizeof(pass->pass) - 1);
	voluta_fill_random_ascii(pass->salt, sizeof(pass->salt) - 1);
}

static int load_as_tmpfs(struct voluta_env *env)
{
	int err;
	loff_t size;
	struct voluta_passphrase pass;

	fill_random_pass(&pass);
	err = voluta_env_setup_key(env, pass.pass, pass.salt);
	if (err) {
		return err;
	}
	size = (loff_t)env->volume_size;
	err = voluta_fs_format(env, NULL, size);
	if (err) {
		return err;
	}
	voluta_itable_init(&env->sbi.s_itable); /* revert state */
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
	voluta_vdi_close(&env->vdi);
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
	return env->qmal.st.nbytes_used;
}

void voluta_env_cache_stats(const struct voluta_env *env,
			    struct voluta_cache_stat *cst)
{
	const struct voluta_cache *cache = &env->cache;

	cst->nblocks = cache->bcmap.count;
	cst->ninodes = cache->icmap.count;
	cst->nvnodes = cache->vcmap.count;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int format_master_record(const struct voluta_sb_info *sbi)
{
	struct voluta_master_record mr = {
		.marker = VOLUTA_MASTER_MARK,
		.version = VOLUTA_VERSION,
	};

	return voluta_vdi_write(sbi->s_vdi, 0, sizeof(mr), &mr);
}

static void fixup_root_inode(struct voluta_inode_info *ii)
{
	struct voluta_inode *inode = ii->inode;

	inode->i_nlink = 2;
	inode->i_parent_ino = i_ino_of(ii);
	i_dirtify(ii);
}

static int format_root_inode(struct voluta_env *env,
			     struct voluta_inode_info **out_ii)
{
	int err;
	const mode_t mode = VOLUTA_ROOTDIR_MODE;

	err = voluta_new_inode(env, mode, VOLUTA_INO_NULL, out_ii);
	if (err) {
		return err;
	}
	fixup_root_inode(*out_ii);
	return 0;
}

int voluta_fs_format(struct voluta_env *env, const char *path, loff_t size)
{
	int err;
	struct voluta_inode_info *root_ii = NULL;
	struct voluta_sb_info *sbi = &env->sbi;

	err = voluta_prepare_space(sbi, env->volume_path, size);
	if (err) {
		return err;
	}
	err = voluta_vdi_create(&env->vdi, path, size);
	if (err) {
		return err;
	}
	err = format_master_record(sbi);
	if (err) {
		return err;
	}
	err = voluta_format_super_block(sbi);
	if (err) {
		return err;
	}
	err = voluta_format_uber_blocks(sbi);
	if (err) {
		return err;
	}
	err = voluta_format_itable(sbi);
	if (err) {
		return err;
	}
	err = format_root_inode(env, &root_ii);
	if (err) {
		return err;
	}
	voluta_bind_root_ino(sbi, i_ino_of(root_ii));

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
	size_t page_size, page_shift[] = { 12, 13, 14, 16 };

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
