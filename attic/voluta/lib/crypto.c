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
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <gcrypt.h>
#include "voluta-lib.h"

#define VOLUTA_SECMEM_SIZE (65536)

#define ARRAY_SIZE(x) VOLUTA_ARRAY_SIZE(x)

#define voluta_trace_gcrypt_err(fn, err) \
	do { voluta_log_error("%s: %s", fn, gcry_strerror(err)); } while (0)


static int gcrypt_err(gcry_error_t gcry_err)
{
	const int err = (int)gcry_err;

	return (err > 0) ? -err : err;
}

int voluta_init_gcrypt(void)
{
	gcry_error_t err;
	enum gcry_ctl_cmds cmd;
	const char *version;
	const char *expected_version = GCRYPT_VERSION;

	version = gcry_check_version(expected_version);
	if (!version) {
		log_warn("libgcrypt version != %s", expected_version);
		return -1;
	}
	cmd = GCRYCTL_SUSPEND_SECMEM_WARN;
	err = gcry_control(cmd);
	if (err) {
		goto out_control_err;
	}
	cmd = GCRYCTL_INIT_SECMEM;
	err = gcry_control(cmd, VOLUTA_SECMEM_SIZE, 0);
	if (err) {
		goto out_control_err;
	}
	cmd = GCRYCTL_RESUME_SECMEM_WARN;
	err = gcry_control(cmd);
	if (err) {
		goto out_control_err;
	}
	cmd = GCRYCTL_INITIALIZATION_FINISHED;
	gcry_control(cmd, 0);
	if (err) {
		goto out_control_err;
	}
	return 0;

out_control_err:
	voluta_trace_gcrypt_err("gcry_control", err);
	return gcrypt_err(err);
}

static int setup_mdigest(struct voluta_crypto *crypto)
{
	int algo;
	gcry_error_t err;
	const int algos[] = {
		GCRY_MD_MD5,
		GCRY_MD_CRC32,
		GCRY_MD_CRC32_RFC1510,
		GCRY_MD_SHA256,
		GCRY_MD_SHA512
	};

	err = gcry_md_open(&crypto->md_hd, 0, GCRY_MD_FLAG_SECURE);
	if (err) {
		voluta_trace_gcrypt_err("gcry_md_open", err);
		return gcrypt_err(err);
	}
	for (size_t i = 0; i < ARRAY_SIZE(algos); ++i) {
		algo = algos[i];
		err = gcry_md_enable(crypto->md_hd, algo);
		if (err) {
			voluta_trace_gcrypt_err("gcry_md_enable", err);
			return gcrypt_err(err);
		}
	}
	return 0;
}

static void destroy_mdigest(struct voluta_crypto *crypto)
{
	if (crypto->md_hd != NULL) {
		gcry_md_close(crypto->md_hd);
	}
}
static void do_sha(const struct voluta_crypto *crypto, int algo,
		   const void *buf, size_t bsz, size_t len, void *out_hash_buf)
{
	const void *hval;
	gcry_md_hd_t md_hd = crypto->md_hd;

	gcry_md_reset(md_hd);
	gcry_md_write(md_hd, buf, bsz);
	gcry_md_final(md_hd);
	hval = gcry_md_read(md_hd, algo);
	memcpy(out_hash_buf, hval, len);
}

int voluta_sha256_of(const struct voluta_crypto *crypto,
		     const void *buf, size_t bsz,
		     struct voluta_hash256 *out_hash)
{
	size_t len;
	const int algo = GCRY_MD_SHA256;

	len = gcry_md_get_algo_dlen(algo);
	if (len != sizeof(out_hash->hash)) {
		return -EINVAL;
	}
	do_sha(crypto, algo, buf, bsz, len, out_hash->hash);
	return 0;
}

int voluta_sha512_of(const struct voluta_crypto *crypto,
		     const void *buf, size_t bsz,
		     struct voluta_hash512 *out_hash)
{
	size_t len;
	const int algo = GCRY_MD_SHA512;

	len = gcry_md_get_algo_dlen(algo);
	if (len != sizeof(out_hash->hash)) {
		return -EINVAL;
	}
	do_sha(crypto, algo, buf, bsz, len, out_hash->hash);
	return 0;
}

static uint32_t digest_to_uint32(const uint8_t *digest)
{
	const uint32_t d0 = digest[0];
	const uint32_t d1 = digest[1];
	const uint32_t d2 = digest[2];
	const uint32_t d3 = digest[3];

	return (d0 << 24) | (d1 << 16) | (d2 << 8) << d3;
}

int voluta_crc32_of(const struct voluta_crypto *crypto,
		    const void *buf, size_t bsz, uint32_t *out_crc32)
{
	size_t len;
	const void *ptr;
	gcry_md_hd_t md_hd = crypto->md_hd;
	const int algo = GCRY_MD_CRC32;

	len = gcry_md_get_algo_dlen(algo);
	if (len != sizeof(*out_crc32)) {
		return -EINVAL;
	}

	gcry_md_reset(md_hd);
	gcry_md_write(md_hd, buf, bsz);
	gcry_md_final(md_hd);
	ptr = gcry_md_read(md_hd, algo);

	*out_crc32 = digest_to_uint32(ptr);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int setup_cipher(struct voluta_crypto *crypto)
{
	gcry_error_t err;
	const int algo = GCRY_CIPHER_AES256;
	const int mode = GCRY_CIPHER_MODE_GCM;
	const unsigned int flags = GCRY_CIPHER_SECURE;

	err = gcry_cipher_open(&crypto->chiper_hd, algo, mode, flags);
	if (err) {
		voluta_trace_gcrypt_err("gcry_cipher_open", err);
		return gcrypt_err(err);
	}
	return 0;
}

static void destroy_cipher(struct voluta_crypto *crypto)
{
	if (crypto->chiper_hd != NULL) {
		gcry_cipher_close(crypto->chiper_hd);
		crypto->chiper_hd = NULL;
	}
}

static int prepare_cipher(const struct voluta_crypto *crypto,
			  const struct voluta_iv *iv,
			  const struct voluta_key *key)
{
	size_t blklen;
	gcry_error_t err;

	blklen = gcry_cipher_get_algo_blklen(GCRY_CIPHER_AES256);
	if (blklen > sizeof(iv->iv)) {
		log_warn("bad blklen: %lu", blklen);
		return -1;
	}
	err = gcry_cipher_reset(crypto->chiper_hd);
	if (err) {
		voluta_trace_gcrypt_err("gcry_cipher_reset", err);
		return gcrypt_err(err);
	}
	err = gcry_cipher_setkey(crypto->chiper_hd, key->key, sizeof(key->key));
	if (err) {
		voluta_trace_gcrypt_err("gcry_cipher_setkey", err);
		return gcrypt_err(err);
	}
	err = gcry_cipher_setiv(crypto->chiper_hd, iv->iv, blklen);
	if (err) {
		voluta_trace_gcrypt_err("gcry_cipher_setiv", err);
		return gcrypt_err(err);
	}
	return 0;
}

static int encrypt_data(const struct voluta_crypto *crypto,
			const void *in_dat, void *out_dat, size_t len)
{
	gcry_error_t err;

	err = gcry_cipher_encrypt(crypto->chiper_hd,
				  out_dat, len, in_dat, len);
	if (err) {
		voluta_trace_gcrypt_err("gcry_cipher_encrypt", err);
		return gcrypt_err(err);
	}
	err = gcry_cipher_final(crypto->chiper_hd);
	if (err) {
		voluta_trace_gcrypt_err("gcry_cipher_final", err);
		return gcrypt_err(err);
	}
	return 0;
}

int voluta_encrypt_bk(const struct voluta_crypto *crypto,
		      const struct voluta_iv *iv, const struct voluta_key *key,
		      const union voluta_bk *bk, union voluta_bk *enc_bk)
{
	int err;

	err = prepare_cipher(crypto, iv, key);
	if (err) {
		return err;
	}
	for (size_t i = 0; i < ARRAY_SIZE(bk->kbs); ++i) {
		err = encrypt_data(crypto, &bk->kbs[i], &enc_bk->kbs[i],
				   sizeof(enc_bk->kbs[i]));
		if (err) {
			return err;
		}
	}
	return 0;
}

static int decrypt_data(const struct voluta_crypto *crypto,
			const void *in_dat, void *out_dat, size_t len)
{
	gcry_error_t err;

	err = gcry_cipher_decrypt(crypto->chiper_hd,
				  out_dat, len, in_dat, len);
	if (err) {
		voluta_trace_gcrypt_err("gcry_cipher_decrypt", err);
		return gcrypt_err(err);
	}
	err = gcry_cipher_final(crypto->chiper_hd);
	if (err) {
		voluta_trace_gcrypt_err("gcry_cipher_final", err);
		return gcrypt_err(err);
	}
	return 0;
}

int voluta_decrypt_bk(const struct voluta_crypto *crypto,
		      const struct voluta_iv *iv, const struct voluta_key *key,
		      const union voluta_bk *bk, union voluta_bk *dec_bk)
{
	int err;

	err = prepare_cipher(crypto, iv, key);
	if (err) {
		return err;
	}
	for (size_t i = 0; i < ARRAY_SIZE(bk->kbs); ++i) {
		err = decrypt_data(crypto, &bk->kbs[i], &dec_bk->kbs[i],
				   sizeof(dec_bk->kbs[i]));
		if (err) {
			return err;
		}
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_derive_iv_key(const struct voluta_crypto *crypto,
			 const void *passphrase, size_t passphraselen,
			 const void *salt, size_t saltlen,
			 struct voluta_iv_key *iv_key)
{
	int err;
	gpg_error_t gcry_err;
	unsigned long iterations = 1024; /* XXX */
	struct voluta_iv *iv = &iv_key->iv;
	struct voluta_key *key = &iv_key->key;
	struct voluta_hash512 sha;

	voluta_memzero(&sha, sizeof(sha));
	err = voluta_sha512_of(crypto, salt, saltlen, &sha);
	if (err) {
		return err;
	}
	gcry_err = gcry_kdf_derive(passphrase, passphraselen, GCRY_KDF_PBKDF2,
				   GCRY_MD_SHA256, sha.hash, sizeof(sha.hash),
				   iterations, sizeof(iv->iv), iv->iv);
	if (gcry_err) {
		voluta_trace_gcrypt_err("gcry_kdf_derive", gcry_err);
		return gcrypt_err(gcry_err);
	}
	gcry_err = gcry_kdf_derive(passphrase, passphraselen, GCRY_KDF_SCRYPT,
				   8, sha.hash, sizeof(sha.hash),
				   iterations, sizeof(key->key), key->key);
	if (gcry_err) {
		voluta_trace_gcrypt_err("gcry_kdf_derive", gcry_err);
		return gcrypt_err(gcry_err);
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_fill_random(void *buf, size_t len, bool very_strong)
{
	const enum gcry_random_level random_level =
		very_strong ? GCRY_VERY_STRONG_RANDOM : GCRY_STRONG_RANDOM;

	gcry_randomize(buf, len, random_level);
}

void voluta_fill_random_ascii(char *str, size_t len)
{
	size_t pos;
	size_t nrands = 0;
	uint32_t rands[64];
	const char *chset = "abcdefghijklmnopqrstuvwxyz"
			    "ABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789";
	const size_t nchars = strlen(chset);

	for (size_t i = 0; i < len; ++i) {
		if (nrands == 0) {
			nrands = ARRAY_SIZE(rands);
			voluta_fill_random(rands, sizeof(rands), 0);
		}
		pos = rands[--nrands] % nchars;
		str[i] = chset[pos];
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int voluta_crypto_init(struct voluta_crypto *crypto)
{
	int err;

	voluta_memzero(crypto, sizeof(*crypto));
	err = setup_mdigest(crypto);
	if (err) {
		return err;
	}
	err = setup_cipher(crypto);
	if (err) {
		destroy_mdigest(crypto);
		return err;
	}
	return 0;
}

void voluta_crypto_fini(struct voluta_crypto *crypto)
{
	destroy_cipher(crypto);
	destroy_mdigest(crypto);
	memset(crypto, 0xEF, sizeof(*crypto));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void voluta_fill_random_key(struct voluta_key *key)
{
	voluta_getentropy(key->key, sizeof(key->key));
}

void voluta_fill_random_iv(struct voluta_iv *iv)
{
	voluta_getentropy(iv->iv, sizeof(iv->iv));
}

