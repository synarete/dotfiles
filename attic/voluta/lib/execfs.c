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

#define DIV_ROUND_UP(n, d)      ((n + d - 1) / d)
#define ROUND_TO_K(n)           (DIV_ROUND_UP(n, VOLUTA_KILO) * VOLUTA_KILO)


struct voluta_fs_core {
	struct voluta_sb_info   sbi;
	struct voluta_fusei     fusei;
	struct voluta_fuseq     fuseq;
	struct voluta_cache     cache;
	struct voluta_crypto    crypto;
	struct voluta_pstore    pstore;
};


union voluta_fs_core_u {
	struct voluta_fs_core c;
	uint8_t dat[ROUND_TO_K(sizeof(struct voluta_fs_core))];

} voluta_aligned64;

struct voluta_fs_env_obj {
	union voluta_fs_core_u  fs_core;
	struct voluta_fs_env    fs_env;
	uint8_t pad[32];
};

#define STATICASSERT_ALIGNED64(t_) \
	VOLUTA_STATICASSERT_EQ(sizeof(t_) % 64, 0)

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t namelen_of(const char *name)
{
	return (name != NULL) ? strnlen(name, VOLUTA_NAME_MAX) : 0;
}

static struct voluta_fs_env_obj *fs_env_obj_of(struct voluta_fs_env *fs_env)
{
	STATICASSERT_ALIGNED64(union voluta_fs_core_u);
	STATICASSERT_ALIGNED64(struct voluta_fs_env_obj);

	return container_of(fs_env, struct voluta_fs_env_obj, fs_env);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static uint64_t vbr_marker(const struct voluta_vboot_record *vbr)
{
	return le64_to_cpu(vbr->vb_pub.v_marker);
}

static void vbr_set_marker(struct voluta_vboot_record *vbr, uint64_t mark)
{
	vbr->vb_pub.v_marker = cpu_to_le64(mark);
}

static uint64_t vbr_version(const struct voluta_vboot_record *vbr)
{
	return le64_to_cpu(vbr->vb_pub.v_version);
}

static void vbr_set_version(struct voluta_vboot_record *vbr, uint64_t version)
{
	vbr->vb_pub.v_version = cpu_to_le64(version);
}

static void vbr_set_sw_version(struct voluta_vboot_record *vbr,
			       const char *sw_version)
{
	const size_t len = strlen(sw_version);
	const size_t len_max = ARRAY_SIZE(vbr->vb_pub.v_sw_version) - 1;

	memcpy(vbr->vb_pub.v_sw_version, sw_version, min(len, len_max));
}

static void vbr_set_label(struct voluta_vboot_record *vbr, const char *label)
{
	memcpy(vbr->vb_pub.v_label, label, namelen_of(label));
}

static void vbr_set_name(struct voluta_vboot_record *vbr, const char *name)
{
	memcpy(vbr->vb_pri.v_name, name, namelen_of(name));
}

static void vbr_stamp(struct voluta_vboot_record *vbr)
{
	vbr_set_marker(vbr, VOLUTA_VBR_MARK);
	vbr_set_version(vbr, VOLUTA_VERSION);
}

static void vbr_init(struct voluta_vboot_record *vbr)
{
	voluta_memzero(vbr, sizeof(*vbr));
	vbr_stamp(vbr);
}

static void vbr_rfill_random(struct voluta_vboot_record *vbr)
{
	voluta_fill_random(vbr->vb_pri.v_rfill,
			   sizeof(vbr->vb_pri.v_rfill), false);
}

static void vbr_calc_rhash(const struct voluta_vboot_record *vbr,
			   const struct voluta_crypto *crypto,
			   struct voluta_hash512 *out_hash)
{
	voluta_sha512_of(crypto, vbr->vb_pri.v_rfill,
			 sizeof(vbr->vb_pri.v_rfill), out_hash);
}

static void vbr_set_rhash(struct voluta_vboot_record *vbr,
			  const struct voluta_hash512 *h)
{
	VOLUTA_STATICASSERT_EQ(sizeof(h->hash), sizeof(vbr->vb_pri.v_rhash));

	memcpy(vbr->vb_pri.v_rhash, h->hash, sizeof(vbr->vb_pri.v_rhash));
}

static bool vbr_has_rhash(const struct voluta_vboot_record *vbr,
			  const struct voluta_hash512 *h)
{
	return (memcmp(vbr->vb_pri.v_rhash, h->hash, sizeof(h->hash)) == 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void fill_vboot_record(struct voluta_vboot_record *vbr,
			      const struct voluta_crypto *crypto,
			      const char *label, const char *name)
{
	struct voluta_hash512 hash;

	vbr_init(vbr);
	vbr_set_sw_version(vbr, voluta_g_config.version);
	vbr_set_label(vbr, label);
	vbr_set_name(vbr, name);
	vbr_rfill_random(vbr);
	vbr_calc_rhash(vbr, crypto, &hash);
	vbr_set_rhash(vbr, &hash);
}

static int encrypt_vboot_record(struct voluta_vboot_record *vbr,
				const struct voluta_crypto *crypto,
				const struct voluta_iv_key *iv_key)
{
	struct voluta_vboot_pri *vbr_pri = &vbr->vb_pri;

	return voluta_encrypt_data(crypto, iv_key, vbr_pri,
				   vbr_pri, sizeof(*vbr_pri));
}

static int write_vboot_record(const struct voluta_vboot_record *vbr,
			      const struct voluta_pstore *pstore)
{
	return voluta_pstore_write(pstore, 0, sizeof(*vbr), vbr);
}

static int read_vboot_record(struct voluta_vboot_record *vbr,
			     const struct voluta_pstore *pstore)
{
	return voluta_pstore_read(pstore, 0, sizeof(*vbr), vbr);
}

static int decrypt_vboot_record(struct voluta_vboot_record *vbr,
				const struct voluta_crypto *crypto,
				const struct voluta_iv_key *iv_key)
{
	struct voluta_vboot_pri *vbr_pri = &vbr->vb_pri;

	return voluta_decrypt_data(crypto, iv_key, vbr_pri,
				   vbr_pri, sizeof(*vbr_pri));
}

static int check_vboot_record_pub(const struct voluta_vboot_record *vbr)
{
	if (vbr_marker(vbr) != VOLUTA_VBR_MARK) {
		return -EFSCORRUPTED;
	}
	if (vbr_version(vbr) != VOLUTA_VERSION) {
		return -EFSCORRUPTED;
	}
	return 0;
}

static int check_vboot_record_pri(const struct voluta_vboot_record *vbr,
				  const struct voluta_crypto *crypto)
{
	struct voluta_hash512 hash;

	vbr_calc_rhash(vbr, crypto, &hash);
	return vbr_has_rhash(vbr, &hash) ? 0 : -EKEYEXPIRED;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void fs_env_set_pedantic(struct voluta_fs_env *fs_env, bool pedantic)
{
	fs_env->qalloc.pedantic_mode = pedantic;
}

static void fs_env_setup_defaults(struct voluta_fs_env *fs_env)
{
	voluta_memzero(fs_env, sizeof(*fs_env));
	fs_env_set_pedantic(fs_env, false);

	fs_env->params.ucred.uid = getuid();
	fs_env->params.ucred.gid = getgid();
	fs_env->params.ucred.pid = getpid();
	fs_env->params.ucred.umask = 0002;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int fs_env_init_qalloc(struct voluta_fs_env *fs_env)
{
	int err;
	size_t memsize = 0;
	const size_t memwant = 4 * VOLUTA_UGIGA;

	err = voluta_resolve_memsize(memwant, &memsize);
	if (err) {
		return err;
	}
	err = voluta_qalloc_init(&fs_env->qalloc, memsize);
	if (err) {
		return err;
	}
	return 0;
}

static void fs_env_fini_qalloc(struct voluta_fs_env *fs_env)
{
	voluta_qalloc_fini(&fs_env->qalloc);
}

static int fs_env_init_mpool(struct voluta_fs_env *fs_env)
{
	voluta_mpool_init(&fs_env->mpool, &fs_env->qalloc);
	return 0;
}

static void fs_env_fini_mpool(struct voluta_fs_env *fs_env)
{
	voluta_mpool_fini(&fs_env->mpool);
}

static int fs_env_init_cache(struct voluta_fs_env *fs_env)
{
	int err;
	struct voluta_cache *cache = &fs_env_obj_of(fs_env)->fs_core.c.cache;

	err = voluta_cache_init(cache, &fs_env->mpool);
	if (!err) {
		fs_env->cache = cache;
	}
	return err;
}

static void fs_env_fini_cache(struct voluta_fs_env *fs_env)
{
	if (fs_env->cache != NULL) {
		voluta_cache_fini(fs_env->cache);
		fs_env->cache = NULL;
	}
}

static int fs_env_init_super(struct voluta_fs_env *fs_env)
{
	int err;
	struct voluta_sb_info *sbi = &fs_env_obj_of(fs_env)->fs_core.c.sbi;

	err = voluta_sbi_init(sbi, fs_env->cache,
			      fs_env->crypto, fs_env->pstore);
	if (!err) {
		fs_env->sbi = sbi;
	}
	return err;
}

static void fs_env_fini_super(struct voluta_fs_env *fs_env)
{
	if (fs_env->sbi != NULL) {
		voluta_sbi_fini(fs_env->sbi);
		fs_env->sbi = NULL;
	}
}

static int fs_env_reinit_super(struct voluta_fs_env *fs_env)
{
	return voluta_sbi_reinit(fs_env->sbi);
}

static int fs_env_init_crypto(struct voluta_fs_env *fs_env)
{
	int err;
	struct voluta_crypto *crypto =
		&fs_env_obj_of(fs_env)->fs_core.c.crypto;

	err = voluta_crypto_init(crypto);
	if (!err) {
		fs_env->crypto = crypto;
	}
	return err;
}

static void fs_env_fini_crypto(struct voluta_fs_env *fs_env)
{
	if (fs_env->crypto != NULL) {
		voluta_crypto_fini(fs_env->crypto);
	}
}

static int fs_env_init_pstore(struct voluta_fs_env *fs_env)
{
	int err;
	struct voluta_pstore *pstore =
		&fs_env_obj_of(fs_env)->fs_core.c.pstore;

	err = voluta_pstore_init(pstore);
	if (!err) {
		fs_env->pstore = pstore;
	}
	return err;
}

static void fs_env_fini_pstore(struct voluta_fs_env *fs_env)
{
	if (fs_env->pstore != NULL) {
		voluta_pstore_fini(fs_env->pstore);
		fs_env->pstore = NULL;
	}
}

static int fs_env_init_fusei(struct voluta_fs_env *fs_env)
{
	int err;
	struct voluta_fusei *fusei = &fs_env_obj_of(fs_env)->fs_core.c.fusei;

	err = voluta_fusei_init(fusei, fs_env);
	if (!err) {
		fs_env->fusei = fusei;
	}
	return err;
}

static void fs_env_fini_fusei(struct voluta_fs_env *fs_env)
{
	if (fs_env->fusei != NULL) {
		voluta_fusei_fini(fs_env->fusei);
		fs_env->fusei = NULL;
	}
}

static int fs_env_init_fuseq(struct voluta_fs_env *fs_env)
{
	int err;
	struct voluta_fuseq *fuseq = &fs_env_obj_of(fs_env)->fs_core.c.fuseq;

	err = voluta_fuseq_init(fuseq, fs_env->sbi);
	if (!err) {
		fs_env->fuseq = fuseq;
	}
	return err;
}

static void fs_env_fini_fuseq(struct voluta_fs_env *fs_env)
{
	if (fs_env->fuseq != NULL) {
		voluta_fuseq_fini(fs_env->fuseq);
		fs_env->fuseq = NULL;
	}
}

static int fs_env_init(struct voluta_fs_env *fs_env)
{
	int err;

	fs_env_setup_defaults(fs_env);

	err = fs_env_init_qalloc(fs_env);
	if (err) {
		return err;
	}
	err = fs_env_init_mpool(fs_env);
	if (err) {
		return err;
	}
	err = fs_env_init_cache(fs_env);
	if (err) {
		return err;
	}
	err = fs_env_init_crypto(fs_env);
	if (err) {
		return err;
	}
	err = fs_env_init_pstore(fs_env);
	if (err) {
		return err;
	}
	err = fs_env_init_super(fs_env);
	if (err) {
		return err;
	}
	err = fs_env_init_fusei(fs_env);
	if (err) {
		return err;
	}
	err = fs_env_init_fuseq(fs_env);
	if (err) {
		return err;
	}
	return 0;
}

static void fs_env_fini(struct voluta_fs_env *fs_env)
{
	fs_env_fini_fuseq(fs_env);
	fs_env_fini_fusei(fs_env);
	fs_env_fini_super(fs_env);
	fs_env_fini_pstore(fs_env);
	fs_env_fini_crypto(fs_env);
	fs_env_fini_cache(fs_env);
	fs_env_fini_mpool(fs_env);
	fs_env_fini_qalloc(fs_env);
}

int voluta_fs_env_new(struct voluta_fs_env **out_fs_env)
{
	int err;
	size_t alignment;
	void *mem = NULL;
	struct voluta_fs_env *fs_env = NULL;
	struct voluta_fs_env_obj *fs_env_obj = NULL;
	const size_t fs_env_obj_size = sizeof(*fs_env_obj);

	alignment = voluta_sc_page_size();
	err = posix_memalign(&mem, alignment, fs_env_obj_size);
	if (err) {
		return err;
	}

	fs_env_obj = mem;
	fs_env = &fs_env_obj->fs_env;
	memset(fs_env_obj, 0, fs_env_obj_size);

	err = fs_env_init(fs_env);
	if (err) {
		fs_env_fini(fs_env);
		free(mem);
		return err;
	}
	voluta_burnstack();
	*out_fs_env = fs_env;
	return 0;
}

void voluta_fs_env_del(struct voluta_fs_env *fs_env)
{
	struct voluta_fs_env_obj *fs_env_obj;

	fs_env_obj = fs_env_obj_of(fs_env);
	fs_env_fini(fs_env);

	memset(fs_env_obj, 7, sizeof(*fs_env_obj));
	free(fs_env_obj);
	voluta_burnstack();
}

static int fs_env_setup_key(struct voluta_fs_env *fs_env,
			    const char *pass, const char *salt)
{
	struct voluta_sb_info *sbi = sbi_of(fs_env);
	struct voluta_iv_key *iv_key = &sbi->s_iv_key;

	return voluta_derive_iv_key(sbi->s_crypto, pass, salt, iv_key);
}

static int check_vboot_record(struct voluta_vboot_record *vbr,
			      const struct voluta_crypto *crypto,
			      const struct voluta_iv_key *iv_key)
{
	int err;

	err = check_vboot_record_pub(vbr);
	if (err) {
		return err;
	}
	if (iv_key == NULL) {
		return 0;
	}
	err = decrypt_vboot_record(vbr, crypto, iv_key);
	if (err) {
		return err;
	}
	err = check_vboot_record_pri(vbr, crypto);
	if (err) {
		return err;
	}
	return 0;
}

static int load_vboot_record(const struct voluta_sb_info *sbi)
{
	int err;
	struct voluta_vboot_record vbr_buf;
	struct voluta_vboot_record *vbr = &vbr_buf;

	vbr_init(vbr);
	err = read_vboot_record(vbr, sbi->s_pstore);
	if (err) {
		return err;
	}
	err = check_vboot_record(vbr, sbi->s_crypto, &sbi->s_iv_key);
	if (err) {
		return err;
	}
	return 0;
}

static int prepare_space_info(struct voluta_fs_env *fs_env)
{
	const char *path = fs_env->params.volume_path;
	struct voluta_sb_info *sbi = sbi_of(fs_env);

	return voluta_prepare_space(sbi, path, sbi->s_pstore->ps_size);
}

static int load_meta_data(struct voluta_fs_env *fs_env)
{
	int err;
	struct voluta_sb_info *sbi = sbi_of(fs_env);

	err = load_vboot_record(sbi);
	if (err) {
		return err;
	}
	err = prepare_space_info(fs_env);
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

static int load_from_volume(struct voluta_fs_env *fs_env)
{
	int err;
	struct voluta_sb_info *sbi = sbi_of(fs_env);

	err = open_pstore(sbi, fs_env->params.volume_path);
	if (err) {
		return err;
	}
	err = load_meta_data(fs_env);
	if (err) {
		return err;
	}
	return 0;
}

static int fs_env_drop_all_caches(struct voluta_fs_env *fs_env)
{
	struct voluta_stats st;

	voluta_fs_env_stats(fs_env, &st);
	voluta_fs_env_sync_drop(fs_env);
	voluta_fs_env_stats(fs_env, &st);

	if (st.ncache_segs || st.ncache_inodes || st.ncache_inodes) {
		return -EAGAIN;
	}
	return 0;
}

static int revert_to_default(struct voluta_fs_env *fs_env)
{
	int err;
	struct voluta_sb_info *sbi = fs_env->sbi;

	err = voluta_fs_env_sync_drop(fs_env);
	if (err) {
		return err;
	}
	err = voluta_shut_super(sbi);
	if (err) {
		return err;
	}
	err = fs_env_drop_all_caches(fs_env);
	if (err) {
		return err;
	}
	err = fs_env_reinit_super(fs_env);
	if (err) {
		return err;
	}
	return 0;
}

static int format_as_tmpfs(struct voluta_fs_env *fs_env)
{
	return voluta_fs_env_format(fs_env);
}

static int format_load_as_tmpfs(struct voluta_fs_env *fs_env)
{
	int err;

	err = format_as_tmpfs(fs_env);
	if (err) {
		return err;
	}
	err = revert_to_default(fs_env);
	if (err) {
		return err;
	}
	err = load_meta_data(fs_env);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_fs_env_load(struct voluta_fs_env *fs_env)
{
	int err;

	if (fs_env->params.tmpfs) {
		return format_load_as_tmpfs(fs_env);
	}
	err = load_from_volume(fs_env);
	if (err) {
		return err;
	}
	voluta_burnstack();
	return 0;
}

int voluta_fs_env_verify(struct voluta_fs_env *fs_env)
{
	int err;
	struct voluta_sb_info *sbi = sbi_of(fs_env);

	if (fs_env->params.tmpfs) {
		return 0;
	}
	err = open_pstore(sbi, fs_env->params.volume_path);
	if (err) {
		return err;
	}
	err = load_vboot_record(sbi);
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

static int voluta_fs_env_shut(struct voluta_fs_env *fs_env)
{
	int err;
	struct voluta_sb_info *sbi = sbi_of(fs_env);

	err = voluta_commit_dirty_now(sbi);
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

int voluta_fs_env_reload(struct voluta_fs_env *fs_env)
{
	int err;

	err = revert_to_default(fs_env);
	if (err) {
		return err;
	}
	err = load_meta_data(fs_env);
	if (err) {
		return err;
	}
	return 0;
}

static int voluta_fs_env_exec1(struct voluta_fs_env *fs_env)
{
	int err;
	struct voluta_fusei *fusei = fs_env->fusei;

	err = voluta_fusei_mount(fusei, fs_env->params.mount_point);
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
	voluta_fs_env_sync_drop(fs_env);
	return 0;
}

static int voluta_fs_env_exec2(struct voluta_fs_env *fs_env)
{
	int err;
	struct voluta_fuseq *fuseq = fs_env->fuseq;

	err = voluta_fuseq_mount(fuseq, fs_env->params.mount_point);
	if (err) {
		return err;
	}
	err = voluta_fuseq_exec(fuseq);
	if (err) {
		return err;
	}


	/* XXX crap */
	exit(0);

	return 0;
}

static int voluta_fs_env_exec(struct voluta_fs_env *fs_env)
{
	bool with_fuseq = false;

	return with_fuseq ?
	       voluta_fs_env_exec2(fs_env) :
	       voluta_fs_env_exec1(fs_env);
}

int voluta_fs_env_term(struct voluta_fs_env *fs_env)
{
	int err;
	struct voluta_sb_info *sbi = sbi_of(fs_env);

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

void voluta_fs_env_halt(struct voluta_fs_env *fs_env, int signum)
{
	fs_env->signum = signum;
	voluta_fusei_session_break(fs_env->fusei);
}

int voluta_fs_env_sync_drop(struct voluta_fs_env *fs_env)
{
	int err;
	struct voluta_sb_info *sbi = fs_env->sbi;

	err = voluta_commit_dirty_now(sbi);
	if (err) {
		return err;
	}
	err = voluta_drop_caches(sbi);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_fs_env_setparams(struct voluta_fs_env *fs_env,
			    const struct voluta_params *params)
{
	/* TODO: check params, strdup */
	memcpy(&fs_env->params, params, sizeof(fs_env->params));

	fs_env_set_pedantic(fs_env, params->pedantic);
	voluta_sbi_setowner(fs_env->sbi, &params->ucred);
	return fs_env_setup_key(fs_env, params->passphrase, params->salt);
}

void voluta_fs_env_stats(const struct voluta_fs_env *fs_env,
			 struct voluta_stats *st)
{
	const struct voluta_cache *cache = fs_env->cache;

	st->nalloc_bytes = cache->c_qalloc->st.nbytes_used;
	st->ncache_segs = cache->c_slm.count;
	st->ncache_inodes = cache->c_ilm.count;
	st->ncache_vnodes = cache->c_vlm.count;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int format_vboot_record(const struct voluta_sb_info *sbi,
			       const char *label, const char *name)
{
	int err;
	struct voluta_vboot_record vbr_buf;
	struct voluta_vboot_record *vbr = &vbr_buf;
	const struct voluta_iv_key *super_iv_key = &sbi->s_iv_key;

	fill_vboot_record(vbr, sbi->s_crypto, label, name);
	err = encrypt_vboot_record(vbr, sbi->s_crypto, super_iv_key);
	if (err) {
		log_err("encrypt zmeta-record failed: err=%d", err);
		return err;
	}
	err = write_vboot_record(vbr, sbi->s_pstore);
	if (err) {
		log_err("write zmeta-record failed: err=%d", err);
		return err;
	}
	return 0;
}

static int format_root_inode(struct voluta_sb_info *sbi,
			     const struct voluta_oper *op)
{
	int err;
	const mode_t mode = S_IFDIR | 0755;
	struct voluta_inode_info *root_ii = NULL;

	err = voluta_new_inode(sbi, op, mode, VOLUTA_INO_NULL, &root_ii);
	if (err) {
		return err;
	}
	voluta_fixup_rootd(root_ii);
	voluta_bind_root_ino(sbi, i_ino_of(root_ii));
	return 0;
}

static int fs_format_now(struct voluta_sb_info *sbi,
			 const struct voluta_oper *op,
			 const struct voluta_params *params)
{
	int err;
	loff_t size;

	size = params->volume_size;
	err = voluta_prepare_space(sbi, params->volume_path, size);
	if (err) {
		return err;
	}
	err = voluta_pstore_create(sbi->s_pstore, params->volume_path, size);
	if (err) {
		return err;
	}
	err = voluta_pstore_flock(sbi->s_pstore);
	if (err) {
		return err;
	}
	err = format_vboot_record(sbi, params->volume_label,
				  params->volume_name);
	if (err) {
		return err;
	}
	err = voluta_format_super(sbi, params->volume_name);
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
	err = voluta_commit_dirty_now(sbi);
	if (err) {
		return err;
	}
	err = format_root_inode(sbi, op);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_fs_env_format(struct voluta_fs_env *fs_env)
{
	int err;
	struct voluta_sb_info *sbi = sbi_of(fs_env);
	const struct voluta_params *params = &fs_env->params;
	struct voluta_oper op = {
		.ucred.uid = params->ucred.uid,
		.ucred.gid = params->ucred.gid,
		.ucred.pid = params->ucred.pid,
		.ucred.umask = params->ucred.umask,
		.unique = 1,
	};

	err = voluta_ts_gettime(&op.xtime, true);
	if (err) {
		return err;
	}
	err = fs_format_now(sbi, &op, params);
	if (err) {
		return err;
	}
	err = voluta_commit_dirty_now(sbi);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_fs_env_serve(struct voluta_fs_env *fs_env)
{
	int err;
	const char *volume = fs_env->params.volume_path;
	const char *mntpath = fs_env->params.mount_point;

	err = voluta_fs_env_load(fs_env);
	if (err) {
		log_err("load-fs error: %s %d", volume, err);
		return err;
	}
	err = voluta_fs_env_exec(fs_env);
	if (err) {
		log_err("exec-fs error: %s %d", mntpath, err);
		return err;
	}
	err = voluta_fs_env_shut(fs_env);
	if (err) {
		log_err("shut-fs error: %s %d", volume, err);
		return err;
	}
	err = voluta_fs_env_term(fs_env);
	if (err) {
		log_err("term-fs error: %s %d", mntpath, err);
		return err;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int xload_vboot_record(const struct voluta_pstore *pstore,
			      struct voluta_vboot_record *vbr)
{
	int err;
	int status;

	err = voluta_pstore_flock(pstore);
	if (err) {
		return err;
	}
	status = voluta_pstore_read(pstore, 0, sizeof(*vbr), vbr);
	err = voluta_pstore_funlock(pstore);
	if (err) {
		return err;
	}
	return status;
}

int voluta_read_vboot_record(const char *path, struct voluta_vboot_record *vbr)
{
	int err;
	struct voluta_pstore pstore;

	vbr_init(vbr);

	voluta_pstore_init(&pstore);
	err = voluta_pstore_open(&pstore, path);
	if (!err) {
		err = xload_vboot_record(&pstore, vbr);
		voluta_pstore_close(&pstore);
		voluta_pstore_fini(&pstore);
	}
	return err;
}

int voluta_check_vboot_record(struct voluta_vboot_record *vbr,
			      const struct voluta_iv_key *iv_key)
{
	int err;
	struct voluta_crypto crypto;

	err = voluta_crypto_init(&crypto);
	if (!err) {
		err = check_vboot_record(vbr, &crypto, iv_key);
		voluta_crypto_fini(&crypto);
	}
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

