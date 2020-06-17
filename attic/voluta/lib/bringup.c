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
#include "libvoluta.h"

struct voluta_env_core {
	struct voluta_cache     cache;
	struct voluta_cstore    cstore;
	char pad1[16];
	struct voluta_sb_info   sbi;
	struct voluta_fusei     fusei;

} voluta_aligned64;

struct voluta_env_obj {
	struct voluta_env_core  core;
	struct voluta_env       env;

} voluta_aligned64;

#define STATICASSERT_ALIGNED64(t_) \
	VOLUTA_STATICASSERT_EQ(sizeof(t_) % 64, 0)

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static struct voluta_env_obj *env_obj_of(struct voluta_env *env)
{
	STATICASSERT_ALIGNED64(struct voluta_env);
	STATICASSERT_ALIGNED64(struct voluta_env_core);
	STATICASSERT_ALIGNED64(struct voluta_env_obj);

	return container_of(env, struct voluta_env_obj, env);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static uint64_t zmr_marker(const struct voluta_zmeta_record *zmr)
{
	return le64_to_cpu(zmr->z_marker);
}

static void zmr_set_marker(struct voluta_zmeta_record *zmr, uint64_t mark)
{
	zmr->z_marker = cpu_to_le64(mark);
}

static uint64_t zmr_version(const struct voluta_zmeta_record *zmr)
{
	return le64_to_cpu(zmr->z_version);
}

static void zmr_set_version(struct voluta_zmeta_record *zmr, uint64_t version)
{
	zmr->z_version = cpu_to_le64(version);
}

static void zmr_stamp(struct voluta_zmeta_record *zmr)
{
	zmr_set_marker(zmr, VOLUTA_ZMARK);
	zmr_set_version(zmr, VOLUTA_VERSION);
}

static void zmr_init(struct voluta_zmeta_record *zmr)
{
	voluta_memzero(zmr, sizeof(*zmr));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void zsr_rfill_random(struct voluta_zsign_record *zsr)
{
	voluta_fill_random(zsr->z_rfill, sizeof(zsr->z_rfill), false);
}

static void zsr_calc_rhash(const struct voluta_zsign_record *zsr,
			   const struct voluta_crypto *crypto,
			   struct voluta_hash512 *out_hash)
{
	voluta_sha512_of(crypto, zsr->z_rfill, sizeof(zsr->z_rfill), out_hash);
}

static void zsr_set_rhash(struct voluta_zsign_record *zsr,
			  const struct voluta_hash512 *h)
{
	VOLUTA_STATICASSERT_EQ(sizeof(h->hash), sizeof(zsr->z_rhash));

	memcpy(zsr->z_rhash, h->hash, sizeof(zsr->z_rhash));
}

static bool zsr_has_rhash(const struct voluta_zsign_record *zsr,
			  const struct voluta_hash512 *h)
{
	return (memcmp(zsr->z_rhash, h->hash, sizeof(h->hash)) == 0);
}

static void zsr_copyto(const struct voluta_zsign_record *zsr,
		       struct voluta_zsign_record *other)
{
	memcpy(other, zsr, sizeof(*other));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void fill_zmr(struct voluta_zmeta_record *zmr)
{
	zmr_init(zmr);
	zmr_stamp(zmr);
}

static int write_zmr(const struct voluta_zmeta_record *zmr,
		     const struct voluta_pstore *pstore)
{
	return voluta_pstore_write(pstore, 0, sizeof(*zmr), zmr);
}

static int read_zmr(struct voluta_zmeta_record *zmr,
		    const struct voluta_pstore *pstore)
{
	return voluta_pstore_read(pstore, 0, sizeof(*zmr), zmr);
}

static int check_zmr(const struct voluta_zmeta_record *zmr)
{
	if (zmr_marker(zmr) != VOLUTA_ZMARK) {
		return -EFSCORRUPTED;
	}
	if (zmr_version(zmr) != VOLUTA_VERSION) {
		return -EFSCORRUPTED;
	}
	return 0;
}

static void fill_zsr(struct voluta_zsign_record *zsr,
		     const struct voluta_crypto *crypto)
{
	struct voluta_hash512 hash;

	zsr_rfill_random(zsr);
	zsr_calc_rhash(zsr, crypto, &hash);
	zsr_set_rhash(zsr, &hash);
}

static int encrypt_zsr(struct voluta_zsign_record *zsr,
		       const struct voluta_crypto *crypto,
		       const struct voluta_iv_key *iv_key)
{
	int err;
	union voluta_kb *kb;
	union voluta_kb kb_enc;

	kb = container_of(zsr, union voluta_kb, zsr);
	err = voluta_encrypt_nkbs(crypto, iv_key, kb, &kb_enc, 1);
	zsr_copyto(&kb_enc.zsr, zsr);
	return err;
}

static int decrypt_zsr(struct voluta_zsign_record *zsr,
		       const struct voluta_crypto *crypto,
		       const struct voluta_iv_key *iv_key)
{
	int err;
	union voluta_kb *kb;
	union voluta_kb kb_dec;

	kb = container_of(zsr, union voluta_kb, zsr);
	err = voluta_decrypt_kbs(crypto, iv_key, kb, &kb_dec, 1);
	zsr_copyto(&kb_dec.zsr, zsr);
	return err;
}

static int write_zsr(const struct voluta_zsign_record *zsr,
		     const struct voluta_pstore *pstore)
{
	return voluta_pstore_write(pstore, VOLUTA_KB_SIZE, sizeof(*zsr), zsr);
}

static int read_zsr(struct voluta_zsign_record *zsr,
		    const struct voluta_pstore *pstore)
{
	return voluta_pstore_read(pstore, VOLUTA_KB_SIZE, sizeof(*zsr), zsr);
}

static int check_zsr(const struct voluta_zsign_record *zsr,
		     const struct voluta_crypto *crypto)
{
	struct voluta_hash512 hash;

	zsr_calc_rhash(zsr, crypto, &hash);
	return zsr_has_rhash(zsr, &hash) ? 0 : -EKEYEXPIRED;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void env_set_pedantic(struct voluta_env *env, bool pedantic)
{
	env->qalloc.pedantic_mode = pedantic;
}

static void env_set_ucred(struct voluta_env *env,
			  const struct voluta_ucred *ucred)
{
	struct voluta_ucred *dst_ucred = &env->op_ctx.ucred;

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
	env_set_ucred(env, &self_ucred);
	env_set_pedantic(env, false);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int env_init_qalloc(struct voluta_env *env)
{
	int err;
	size_t memsize = 0;
	const size_t memwant = 4 * VOLUTA_UGIGA;

	err = voluta_resolve_memsize(memwant, &memsize);
	if (err) {
		return err;
	}
	err = voluta_qalloc_init(&env->qalloc, memsize);
	if (err) {
		return err;
	}
	return 0;
}

static void env_fini_qalloc(struct voluta_env *env)
{
	voluta_qalloc_fini(&env->qalloc);
}

static int env_init_mpool(struct voluta_env *env)
{
	voluta_mpool_init(&env->mpool, &env->qalloc);
	return 0;
}

static void env_fini_mpool(struct voluta_env *env)
{
	voluta_mpool_fini(&env->mpool);
}

static int env_init_cache(struct voluta_env *env)
{
	int err;
	struct voluta_cache *cache = &env_obj_of(env)->core.cache;

	err = voluta_cache_init(cache, &env->mpool);
	if (!err) {
		env->cache = cache;
	}
	return err;
}

static void env_fini_cache(struct voluta_env *env)
{
	if (env->cache != NULL) {
		voluta_cache_fini(env->cache);
		env->cache = NULL;
	}
}

static int env_init_super(struct voluta_env *env)
{
	int err;
	struct voluta_sb_info *sbi = &env_obj_of(env)->core.sbi;

	err = voluta_sbi_init(sbi, env->cache, env->cstore);
	if (!err) {
		env->sbi = sbi;
	}
	return err;
}

static void env_fini_super(struct voluta_env *env)
{
	if (env->sbi != NULL) {
		voluta_sbi_fini(env->sbi);
		env->sbi = NULL;
	}
}

static int env_reinit_super(struct voluta_env *env)
{
	return voluta_sbi_reinit(env->sbi);
}

static int env_init_cstore(struct voluta_env *env)
{
	int err;
	struct voluta_cstore *cstore = &env_obj_of(env)->core.cstore;

	err = voluta_cstore_init(cstore, &env->qalloc);
	if (!err) {
		env->cstore = cstore;
	}
	return err;
}

static void env_fini_cstore(struct voluta_env *env)
{
	if (env->cstore != NULL) {
		voluta_cstore_fini(env->cstore);
		env->cstore = NULL;
	}
}

static int env_init_fusei(struct voluta_env *env)
{
	int err;
	struct voluta_fusei *fusei = &env_obj_of(env)->core.fusei;

	err = voluta_fusei_init(fusei, env);
	if (!err) {
		env->fusei = fusei;
	}
	return err;
}

static void env_fini_fusei(struct voluta_env *env)
{
	if (env->fusei != NULL) {
		voluta_fusei_fini(env->fusei);
		env->fusei = NULL;
	}
}

static int env_init(struct voluta_env *env)
{
	int err;

	env_setup_defaults(env);

	err = env_init_qalloc(env);
	if (err) {
		return err;
	}
	err = env_init_mpool(env);
	if (err) {
		return err;
	}
	err = env_init_cache(env);
	if (err) {
		return err;
	}
	err = env_init_cstore(env);
	if (err) {
		return err;
	}
	err = env_init_super(env);
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

static void env_fini(struct voluta_env *env)
{
	env_fini_fusei(env);
	env_fini_super(env);
	env_fini_cstore(env);
	env_fini_cache(env);
	env_fini_mpool(env);
	env_fini_qalloc(env);
	voluta_burnstack();
}

int voluta_env_new(struct voluta_env **out_env)
{
	int err;
	size_t alignment;
	void *ptr = NULL;
	struct voluta_env *env = NULL;
	struct voluta_env_obj *env_obj = NULL;
	const size_t env_obj_size = sizeof(*env_obj);

	alignment = voluta_sc_page_size();
	err = posix_memalign(&ptr, alignment, env_obj_size);
	if (err) {
		return err;
	}

	env_obj = ptr;
	env = &env_obj->env;
	memset(env_obj, 0, env_obj_size);

	err = env_init(env);
	if (err) {
		env_fini(env);
		free(env_obj);
		return err;
	}

	*out_env = env;
	return 0;
}

void voluta_env_del(struct voluta_env *env)
{
	struct voluta_env_obj *env_obj;

	env_obj = env_obj_of(env);
	voluta_env_term(env);
	env_fini(env);

	memset(env_obj, 7, sizeof(*env_obj));
	free(env_obj);
}

static const struct voluta_crypto *crypto_of(const struct voluta_sb_info *sbi)
{
	return &sbi->s_cstore->crypto;
}

static int env_derive_key(struct voluta_env *env,
			  const struct voluta_passphrase *pass)
{
	struct voluta_sb_info *sbi = sbi_of(env);

	return voluta_derive_iv_key(crypto_of(sbi),
				    pass->pass, strlen(pass->pass),
				    pass->salt, strlen(pass->salt),
				    &sbi->s_iv_key);
}

static int env_setup_key(struct voluta_env *env,
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
	if (salt && saltlen) {
		memcpy(pass.salt, salt, saltlen);
	}
	return env_derive_key(env, &pass);
}

static int load_zrecords(const struct voluta_sb_info *sbi)
{
	int err;
	union voluta_kb kb;
	struct voluta_zmeta_record *zmr = &kb.zmr;
	struct voluta_zsign_record *zsr = &kb.zsr;

	err = read_zmr(zmr, sbi->s_pstore);
	if (err) {
		return err;
	}
	err = check_zmr(zmr);
	if (err) {
		return err;
	}
	err = read_zsr(zsr, sbi->s_pstore);
	if (err) {
		return err;
	}
	err = decrypt_zsr(zsr, crypto_of(sbi), &sbi->s_iv_key);
	if (err) {
		return err;
	}
	err = check_zsr(zsr, crypto_of(sbi));
	if (err) {
		return err;
	}
	return 0;
}

static int prepare_space_info(struct voluta_env *env)
{
	const char *path = env->params.volume_path;
	struct voluta_sb_info *sbi = sbi_of(env);

	return voluta_prepare_space(sbi, path, sbi->s_pstore->size);
}

static int load_meta_data(struct voluta_env *env)
{
	int err;
	struct voluta_sb_info *sbi = sbi_of(env);

	err = load_zrecords(sbi);
	if (err) {
		return err;
	}
	err = prepare_space_info(env);
	if (err) {
		return err;
	}
	err = voluta_load_super(sbi);
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
	return 0;
}

static int open_pstore(struct voluta_sb_info *sbi, const char *path)
{
	int err;

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
	return 0;
}

static int close_pstore(struct voluta_sb_info *sbi)
{
	int err;

	err = voluta_pstore_funlock(sbi->s_pstore);
	if (err) {
		return err;
	}
	voluta_pstore_close(sbi->s_pstore);
	return 0;
}

static int load_from_volume(struct voluta_env *env)
{
	int err;
	struct voluta_sb_info *sbi = sbi_of(env);

	err = open_pstore(sbi, env->params.volume_path);
	if (err) {
		return err;
	}
	err = load_meta_data(env);
	if (err) {
		return err;
	}
	return 0;
}

static int env_drop_all_caches(struct voluta_env *env)
{
	struct voluta_stats st;

	voluta_env_stats(env, &st);
	voluta_env_sync_drop(env);
	voluta_env_stats(env, &st);

	if (st.ncache_blocks || st.ncache_inodes || st.ncache_inodes) {
		return -EAGAIN;
	}
	return 0;
}

static int revert_to_default(struct voluta_env *env)
{
	int err;
	struct voluta_sb_info *sbi = env->sbi;

	err = voluta_env_sync_drop(env);
	if (err) {
		return err;
	}
	err = voluta_shut_super(sbi);
	if (err) {
		return err;
	}
	err = env_drop_all_caches(env);
	if (err) {
		return err;
	}
	err = env_reinit_super(env);
	if (err) {
		return err;
	}
	return 0;
}

static int format_as_tmpfs(struct voluta_env *env)
{
	const loff_t vol_size = env->params.volume_size;

	return voluta_env_format(env, NULL, vol_size, "voluta-tmpfs");
}

static int format_load_as_tmpfs(struct voluta_env *env)
{
	int err;

	err = format_as_tmpfs(env);
	if (err) {
		return err;
	}
	err = revert_to_default(env);
	if (err) {
		return err;
	}
	err = load_meta_data(env);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_env_load(struct voluta_env *env)
{
	int err;

	if (env->params.tmpfs) {
		return format_load_as_tmpfs(env);
	}
	err = load_from_volume(env);
	if (err) {
		return err;
	}
	voluta_burnstack();
	return 0;
}

int voluta_env_verify(struct voluta_env *env)
{
	int err;
	struct voluta_sb_info *sbi = sbi_of(env);

	if (env->params.tmpfs) {
		return 0;
	}
	err = open_pstore(sbi, env->params.volume_path);
	if (err) {
		return err;
	}
	err = load_zrecords(sbi);
	if (err) {
		close_pstore(sbi);
		return err;
	}
	err = close_pstore(sbi);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_env_shut(struct voluta_env *env)
{
	int err;
	struct voluta_sb_info *sbi = sbi_of(env);

	err = voluta_flush_dirty(sbi);
	if (err) {
		return err;
	}
	err = voluta_drop_caches(sbi);
	if (err) {
		return err;
	}
	err = voluta_shut_super(sbi);
	if (err) {
		return err;
	}
	err = voluta_drop_caches(sbi);
	if (err) {
		return err;
	}
	voluta_burnstack();
	return err;
}

int voluta_env_reload(struct voluta_env *env)
{
	int err;

	err = revert_to_default(env);
	if (err) {
		return err;
	}
	err = load_meta_data(env);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_env_exec(struct voluta_env *env)
{
	int err;
	struct voluta_fusei *fusei = env->fusei;

	err = voluta_fusei_mount(fusei, env->params.mount_point);
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
	voluta_env_sync_drop(env);
	return 0;
}

int voluta_env_term(struct voluta_env *env)
{
	int err;
	struct voluta_sb_info *sbi = sbi_of(env);

	err = voluta_shut_super(sbi);
	if (err) {
		return err;
	}
	err = close_pstore(sbi);
	if (err) {
		return err;
	}
	voluta_burnstack();
	return 0;
}

void voluta_env_halt(struct voluta_env *env, int signum)
{
	env->signum = signum;
	voluta_fusei_session_break(env->fusei);
}

int voluta_env_sync_drop(struct voluta_env *env)
{
	int err;
	struct voluta_sb_info *sbi = env->sbi;

	err = voluta_flush_dirty(sbi);
	if (err) {
		return err;
	}
	err = voluta_drop_caches(sbi);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_env_setparams(struct voluta_env *env,
			 const struct voluta_params *params)
{
	/* TODO: check params, strdup */
	memcpy(&env->params, params, sizeof(env->params));

	env_set_pedantic(env, params->pedantic);
	env_set_ucred(env, &params->ucred);
	return env_setup_key(env, params->passphrase, params->salt);
}

void voluta_env_stats(const struct voluta_env *env, struct voluta_stats *st)
{
	const struct voluta_cache *cache = env->cache;

	st->nalloc_bytes = cache->c_qalloc->st.nbytes_used;
	st->ncache_blocks = cache->c_blm.count;
	st->ncache_inodes = cache->c_ilm.count;
	st->ncache_vnodes = cache->c_vlm.count;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int format_zrecords(const struct voluta_sb_info *sbi)
{
	int err = -ENOMEM;
	union voluta_kb kb;
	struct voluta_zmeta_record *zmr = &kb.zmr;
	struct voluta_zsign_record *zsr = &kb.zsr;

	fill_zmr(zmr);
	err = write_zmr(zmr, sbi->s_pstore);
	if (err) {
		log_error("write zmeta-record failed: err=%d", err);
		return err;
	}
	fill_zsr(zsr, crypto_of(sbi));
	err = encrypt_zsr(zsr, crypto_of(sbi), &sbi->s_iv_key);
	if (err) {
		return err;
	}
	err = write_zsr(zsr, sbi->s_pstore);
	if (err) {
		log_error("write zmeta-record failed: err=%d", err);
		return err;
	}
	return 0;
}

static void fixup_root_inode(struct voluta_inode_info *ii)
{
	voluta_fixup_rootd(ii);
	i_dirtify(ii);
}

static int format_root_inode(struct voluta_sb_info *sbi,
			     const struct voluta_oper_ctx *op_ctx)
{
	int err;
	const mode_t mode = S_IFDIR | 0755;
	struct voluta_inode_info *root_ii = NULL;

	err = voluta_new_inode(sbi, op_ctx, mode, VOLUTA_INO_NULL, &root_ii);
	if (err) {
		return err;
	}
	fixup_root_inode(root_ii);
	voluta_bind_root_ino(sbi, i_ino_of(root_ii));
	return 0;
}

int voluta_env_format(struct voluta_env *env, const char *path,
		      loff_t size, const char *name)
{
	int err;
	struct voluta_sb_info *sbi = sbi_of(env);

	err = voluta_prepare_space(sbi, path, size);
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
	err = format_zrecords(sbi);
	if (err) {
		return err;
	}
	err = voluta_format_super(sbi, name);
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
	err = voluta_flush_dirty(sbi);
	if (err) {
		return err;
	}
	err = format_root_inode(sbi, &env->op_ctx);
	if (err) {
		return err;
	}
	err = voluta_flush_dirty(sbi);
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
	const long page_size_min = VOLUTA_PAGE_SIZE;
	const long cl_size_min = VOLUTA_CACHELINE_SIZE;

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

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int check_marker(const struct voluta_pstore *pstore)
{
	int err;
	struct voluta_zmeta_record zmr = {
		.z_marker = 0,
		.z_version = 0
	};

	err = voluta_pstore_read(pstore, 0, sizeof(zmr), &zmr);
	if (!err) {
		err = check_zmr(&zmr);
	}
	return err;
}

static int check_exclusive_volume(const struct voluta_pstore *pstore,
				  bool with_check_marker)
{
	int err;

	err = voluta_pstore_flock(pstore);
	if (err) {
		return err;
	}
	if (with_check_marker) {
		err = check_marker(pstore);
	}
	voluta_pstore_funlock(pstore);
	return err;
}


int voluta_require_exclusive_volume(const char *path, bool check_marker)
{
	int err;
	struct voluta_pstore pstore;

	voluta_pstore_init(&pstore);
	err = voluta_pstore_open(&pstore, path);
	if (err) {
		return err;
	}
	err = check_exclusive_volume(&pstore, check_marker);
	voluta_pstore_close(&pstore);
	return err;
}

int voluta_make_ascii_passphrase(char *pass, size_t plen)
{
	int err = 0;

	if (pass && plen && (plen <= VOLUTA_PASSPHRASE_MAX)) {
		voluta_fill_random_ascii(pass, plen);
	} else {
		err = -EINVAL;
	}
	return err;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int g_libvoluta_init;

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
