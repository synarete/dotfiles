/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * This file is part of voluta.
 *
 * Copyright (C) 2019 Shachar Sharon
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
#include "unitest.h"


struct kv_sizes {
	size_t name_len;
	size_t value_size;
};


static struct voluta_ut_keyval *
kv_new(struct voluta_ut_ctx *ut_ctx, size_t nlen, size_t size)
{
	struct voluta_ut_keyval *kv;

	kv = voluta_ut_malloc(ut_ctx, sizeof(*kv));
	kv->name = voluta_ut_randstr(ut_ctx, nlen);
	kv->value = voluta_ut_randstr(ut_ctx, size);
	kv->size = size;

	return kv;
}

static struct voluta_ut_kvl *kvl_new(struct voluta_ut_ctx *ut_ctx, size_t limit)
{
	struct voluta_ut_kvl *kvl;

	kvl = voluta_ut_malloc(ut_ctx, sizeof(*kvl));
	kvl->ut_ctx = ut_ctx;
	kvl->list = voluta_ut_zalloc(ut_ctx, limit * sizeof(kvl->list[0]));
	kvl->limit = limit;
	kvl->count = 0;
	return kvl;
}

static void kvl_append(struct voluta_ut_kvl *kvl, size_t name_len,
		       size_t value_sz)
{
	ut_assert_lt(kvl->count, kvl->limit);

	kvl->list[kvl->count++] = kv_new(kvl->ut_ctx, name_len, value_sz);
}

static void kvl_appendn(struct voluta_ut_kvl *kvl,
			const struct kv_sizes *kv_sizes_arr, size_t arr_len)
{
	for (size_t i = 0; i < arr_len; ++i) {
		kvl_append(kvl, kv_sizes_arr[i].name_len,
			   kv_sizes_arr[i].value_size);
	}
}

static void kvl_populate(struct voluta_ut_kvl *kvl, size_t name_len,
			 size_t value_sz)
{
	for (size_t i = kvl->count; i < kvl->limit; ++i) {
		kvl_append(kvl, name_len, value_sz);
	}
}

static void kvl_populate_max(struct voluta_ut_kvl *kvl)
{
	const size_t name_len = VOLUTA_NAME_MAX;
	const size_t value_sz = VOLUTA_XATTR_VALUE_MAX;

	for (size_t i = kvl->count; i < kvl->limit; ++i) {
		kvl_append(kvl, name_len, value_sz);
	}
}

static void kvl_swap(struct voluta_ut_kvl *kvl, size_t i, size_t j)
{
	struct voluta_ut_keyval *kv;

	kv = kvl->list[i];
	kvl->list[i] = kvl->list[j];
	kvl->list[j] = kv;
}

static void kvl_reverse(struct voluta_ut_kvl *kvl)
{
	size_t i, j;
	const size_t cnt = kvl->count / 2;

	for (i = 0, j = kvl->count - 1; i < cnt; ++i, --j) {
		kvl_swap(kvl, i, j);
	}
}

static void kvl_random_shuffle(struct voluta_ut_kvl *kvl)
{
	size_t i, j;
	size_t pos[32];
	const size_t npos = VOLUTA_ARRAY_SIZE(pos);

	for (i = 0, j = npos; i < kvl->count / 2; ++i, ++j) {
		if (j >= npos) {
			voluta_getentropy(pos, sizeof(pos));
			j = 0;
		}
		kvl_swap(kvl, i, pos[j] % kvl->count);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/


static void ut_xattr_simple(struct voluta_ut_ctx *ut_ctx)
{
	size_t i, nkvs = 16;
	ino_t ino, root_ino = VOLUTA_INO_ROOT;
	const char *name = UT_NAME;
	struct voluta_ut_kvl *kvl;

	kvl = kvl_new(ut_ctx, nkvs);
	kvl_populate(kvl, 4, 32);

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	for (i = 0; i < nkvs; ++i) {
		voluta_ut_setxattr_create(ut_ctx, ino, kvl->list[i]);
	}
	for (i = 0; i < nkvs; ++i) {
		voluta_ut_getxattr_value(ut_ctx, ino, kvl->list[i]);
	}
	voluta_ut_listxattr_exists(ut_ctx, ino, kvl);
	for (i = 0; i < nkvs; ++i) {
		voluta_ut_removexattr_exists(ut_ctx, ino, kvl->list[i]);
	}
	for (i = 0; i < nkvs; ++i) {
		voluta_ut_getxattr_nodata(ut_ctx, ino, kvl->list[i]);
	}
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_xattr_long_names(struct voluta_ut_ctx *ut_ctx)
{
	ino_t ino, root_ino = VOLUTA_INO_ROOT;
	const char *name = UT_NAME;
	struct voluta_ut_kvl *kvl;

	kvl = kvl_new(ut_ctx, 8);
	kvl_populate(kvl, VOLUTA_NAME_MAX, VOLUTA_XATTR_VALUE_MAX);

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	voluta_ut_setxattr_all(ut_ctx, ino, kvl);
	voluta_ut_listxattr_exists(ut_ctx, ino, kvl);
	kvl_reverse(kvl);
	voluta_ut_listxattr_exists(ut_ctx, ino, kvl);
	voluta_ut_removexattr_all(ut_ctx, ino, kvl);
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_xattr_multi(struct voluta_ut_ctx *ut_ctx)
{
	ino_t ino, dino, root_ino = VOLUTA_INO_ROOT;
	const char *dname = UT_NAME;
	const char *fname = UT_NAME;
	struct voluta_ut_kvl *kvl;
	const struct kv_sizes kv_sizes_arr[] = {
		{ 1, 1 },
		{ VOLUTA_NAME_MAX / 2, 2 },
		{ 2, VOLUTA_XATTR_VALUE_MAX / 2 },
		{ VOLUTA_NAME_MAX / 16, 16 },
		{ 32, VOLUTA_XATTR_VALUE_MAX / 32 },
		{ VOLUTA_NAME_MAX, 128 },
		{ 64, VOLUTA_XATTR_VALUE_MAX },
	};
	const size_t nkv_sizes = VOLUTA_ARRAY_SIZE(kv_sizes_arr);

	voluta_ut_make_dir(ut_ctx, root_ino, dname, &dino);
	voluta_ut_create_only(ut_ctx, dino, fname, &ino);

	for (size_t i = 0; i < 4; ++i) {
		kvl = kvl_new(ut_ctx, nkv_sizes);
		kvl_appendn(kvl, kv_sizes_arr, nkv_sizes);

		voluta_ut_setxattr_all(ut_ctx, dino, kvl);
		voluta_ut_listxattr_exists(ut_ctx, dino, kvl);
		voluta_ut_setxattr_all(ut_ctx, ino, kvl);
		voluta_ut_listxattr_exists(ut_ctx, ino, kvl);
		kvl_reverse(kvl);
		voluta_ut_listxattr_exists(ut_ctx, dino, kvl);
		voluta_ut_listxattr_exists(ut_ctx, ino, kvl);
		voluta_ut_removexattr_all(ut_ctx, ino, kvl);
		voluta_ut_drop_caches(ut_ctx);
		kvl_random_shuffle(kvl);
		voluta_ut_setxattr_all(ut_ctx, ino, kvl);
		voluta_ut_listxattr_exists(ut_ctx, ino, kvl);
		voluta_ut_listxattr_exists(ut_ctx, dino, kvl);
		kvl_random_shuffle(kvl);
		voluta_ut_listxattr_exists(ut_ctx, ino, kvl);
		voluta_ut_removexattr_all(ut_ctx, ino, kvl);
		voluta_ut_removexattr_all(ut_ctx, dino, kvl);
	}
	voluta_ut_unlink_exists(ut_ctx, dino, fname);
	voluta_ut_remove_dir(ut_ctx, root_ino, dname);
}

/* TODO: Test mixed XATTR_CREATE & XATTR_REPLACE */

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_xattr_maxlen(struct voluta_ut_ctx *ut_ctx)
{
	size_t count = 11;
	ino_t ino, dino, root_ino = VOLUTA_INO_ROOT;
	const char *dname = UT_NAME;
	const char *fname;
	struct voluta_ut_kvl *kvl = kvl_new(ut_ctx, count);

	kvl_populate_max(kvl);
	voluta_ut_make_dir(ut_ctx, root_ino, dname, &dino);
	for (size_t i = 0; i < count; ++i) {
		fname = kvl->list[i]->name;
		voluta_ut_create_file(ut_ctx, dino, fname, &ino);
		voluta_ut_setxattr_all(ut_ctx, ino, kvl);
		voluta_ut_listxattr_exists(ut_ctx, ino, kvl);
	}
	for (size_t i = 0; i < count; ++i) {
		fname = kvl->list[i]->name;
		voluta_ut_lookup_resolve(ut_ctx, dino, fname, &ino);
		voluta_ut_listxattr_exists(ut_ctx, ino, kvl);
	}
	kvl_random_shuffle(kvl);
	for (size_t i = 0; i < count; i += 3) {
		fname = kvl->list[i]->name;
		voluta_ut_lookup_resolve(ut_ctx, dino, fname, &ino);
		voluta_ut_removexattr_all(ut_ctx, ino, kvl);
	}
	kvl_random_shuffle(kvl);
	for (size_t i = 0; i < count; ++i) {
		fname = kvl->list[i]->name;
		voluta_ut_lookup_resolve(ut_ctx, dino, fname, &ino);
		voluta_ut_remove_file(ut_ctx, dino, fname, ino);
	}
	voluta_ut_remove_dir(ut_ctx, root_ino, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_xattr_simple),
	UT_DEFTEST(ut_xattr_long_names),
	UT_DEFTEST(ut_xattr_multi),
	UT_DEFTEST(ut_xattr_maxlen),
};

const struct voluta_ut_tests voluta_ut_xattr_tests = UT_MKTESTS(ut_local_tests);
