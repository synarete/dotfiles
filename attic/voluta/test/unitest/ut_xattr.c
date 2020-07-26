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
#include "unitest.h"


struct voluta_kv_sizes {
	size_t name_len;
	size_t value_size;
};


static struct ut_keyval *
kv_new(struct ut_env *ut_env, size_t nlen, size_t size)
{
	struct ut_keyval *kv;

	kv = ut_malloc(ut_env, sizeof(*kv));
	kv->name = ut_randstr(ut_env, nlen);
	kv->value = ut_randstr(ut_env, size);
	kv->size = size;

	return kv;
}

static struct ut_kvl *kvl_new(struct ut_env *ut_env,
			      size_t limit)
{
	struct ut_kvl *kvl;
	const size_t list_sz = limit * sizeof(struct ut_keyval *);

	kvl = ut_malloc(ut_env, sizeof(*kvl));
	kvl->ut_env = ut_env;
	kvl->list = ut_zalloc(ut_env, list_sz);
	kvl->limit = limit;
	kvl->count = 0;
	return kvl;
}

static void kvl_append(struct ut_kvl *kvl, size_t nlen, size_t value_sz)
{
	ut_assert_lt(kvl->count, kvl->limit);

	kvl->list[kvl->count++] = kv_new(kvl->ut_env, nlen, value_sz);
}

static void kvl_appendn(struct ut_kvl *kvl,
			const struct voluta_kv_sizes *arr, size_t arr_len)
{
	for (size_t i = 0; i < arr_len; ++i) {
		kvl_append(kvl, arr[i].name_len, arr[i].value_size);
	}
}

static void kvl_populate(struct ut_kvl *kvl,
			 size_t name_len, size_t value_sz)
{
	for (size_t i = kvl->count; i < kvl->limit; ++i) {
		kvl_append(kvl, name_len, value_sz);
	}
}

static void kvl_populate_max(struct ut_kvl *kvl)
{
	const size_t name_len = UT_NAME_MAX;
	const size_t value_sz = VOLUTA_XATTR_VALUE_MAX;

	for (size_t i = kvl->count; i < kvl->limit; ++i) {
		kvl_append(kvl, name_len, value_sz);
	}
}

static void kvl_swap(struct ut_kvl *kvl, size_t i, size_t j)
{
	struct ut_keyval *kv;

	kv = kvl->list[i];
	kvl->list[i] = kvl->list[j];
	kvl->list[j] = kv;
}

static void kvl_reverse(struct ut_kvl *kvl)
{
	const size_t cnt = kvl->count / 2;

	for (size_t i = 0, j = kvl->count - 1; i < cnt; ++i, --j) {
		kvl_swap(kvl, i, j);
	}
}

static void kvl_random_shuffle(struct ut_kvl *kvl)
{
	size_t pos[32];
	const size_t npos = UT_ARRAY_SIZE(pos);

	for (size_t i = 0, j = npos; i < kvl->count / 2; ++i, ++j) {
		if (j >= npos) {
			voluta_getentropy(pos, sizeof(pos));
			j = 0;
		}
		kvl_swap(kvl, i, pos[j] % kvl->count);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_xattr_simple(struct ut_env *ut_env)
{
	ino_t ino;
	ino_t dino;
	const char *name = UT_NAME;
	struct ut_kvl *kvl;

	kvl = kvl_new(ut_env, 16);
	kvl_populate(kvl, 4, 32);

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_create_file(ut_env, dino, name, &ino);
	for (size_t i = 0; i < kvl->count; ++i) {
		ut_setxattr_create(ut_env, ino, kvl->list[i]);
	}
	for (size_t i = 0; i < kvl->count; ++i) {
		ut_getxattr_value(ut_env, ino, kvl->list[i]);
	}
	ut_listxattr_ok(ut_env, ino, kvl);
	for (size_t i = 0; i < kvl->count; ++i) {
		ut_removexattr_ok(ut_env, ino, kvl->list[i]);
	}
	for (size_t i = 0; i < kvl->count; ++i) {
		ut_getxattr_nodata(ut_env, ino, kvl->list[i]);
	}
	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_at_root(ut_env, name);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_xattr_long_names(struct ut_env *ut_env)
{
	ino_t ino;
	const ino_t root_ino = UT_ROOT_INO;
	const char *name = UT_NAME;
	struct ut_kvl *kvl;

	kvl = kvl_new(ut_env, 4);
	kvl_populate(kvl, UT_NAME_MAX, VOLUTA_XATTR_VALUE_MAX);

	ut_create_file(ut_env, root_ino, name, &ino);
	ut_setxattr_all(ut_env, ino, kvl);
	ut_listxattr_ok(ut_env, ino, kvl);
	kvl_reverse(kvl);
	ut_listxattr_ok(ut_env, ino, kvl);
	ut_removexattr_all(ut_env, ino, kvl);
	ut_remove_file(ut_env, root_ino, name, ino);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void fill_short_kv(struct ut_env *ut_env,
			  struct ut_keyval *kv, size_t idx)
{
	const char *str;

	str = ut_strfmt(ut_env, (idx & 1) ? "%lx" : "%032lx", idx);
	kv->name = str;
	kv->value = str;
	kv->size = strlen(str);
}

static void ut_xattr_shorts_(struct ut_env *ut_env, size_t cnt)
{
	ino_t ino;
	const char *dname = UT_NAME;
	struct ut_keyval kv;

	ut_mkdir_at_root(ut_env, dname, &ino);
	for (size_t i = 0; i < cnt; ++i) {
		fill_short_kv(ut_env, &kv, i);
		ut_setxattr_create(ut_env, ino, &kv);
	}
	for (size_t i = 0; i < cnt; i += 2) {
		fill_short_kv(ut_env, &kv, i);
		ut_removexattr_ok(ut_env, ino, &kv);
	}
	for (size_t i = 1; i < cnt; i += 2) {
		fill_short_kv(ut_env, &kv, i);
		ut_removexattr_ok(ut_env, ino, &kv);
		fill_short_kv(ut_env, &kv, ~i);
		ut_setxattr_create(ut_env, ino, &kv);
		fill_short_kv(ut_env, &kv, ~(i - 1));
		ut_setxattr_create(ut_env, ino, &kv);
	}
	for (size_t i = 0; i < cnt; ++i) {
		fill_short_kv(ut_env, &kv, ~i);
		ut_removexattr_ok(ut_env, ino, &kv);
	}
	ut_rmdir_at_root(ut_env, dname);
}

static void ut_xattr_shorts(struct ut_env *ut_env)
{
	ut_xattr_shorts_(ut_env, 10);
	ut_xattr_shorts_(ut_env, 100);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void fill_novalue_kv(struct ut_env *ut_env,
			    struct ut_keyval *kv, size_t idx)
{
	size_t len;
	char *str;

	str = ut_strfmt(ut_env, "%lx-%0255lx", idx, idx);
	len = strlen(str);
	if ((idx > 7) && (idx < len)) {
		str[idx] = '\0';
	} else if (len > UT_NAME_MAX) {
		str[UT_NAME_MAX] = '\0';
	}
	kv->name = str;
	kv->value = NULL;
	kv->size = 0;
}

static void ut_xattr_no_value_(struct ut_env *ut_env, size_t cnt)
{
	ino_t ino;
	const char *dname = UT_NAME;
	struct ut_keyval kv;

	ut_mkdir_at_root(ut_env, dname, &ino);
	for (size_t i = 0; i < cnt; ++i) {
		fill_novalue_kv(ut_env, &kv, i);
		ut_setxattr_create(ut_env, ino, &kv);
	}
	for (size_t i = 0; i < cnt; i += 2) {
		fill_novalue_kv(ut_env, &kv, i);
		ut_removexattr_ok(ut_env, ino, &kv);
	}
	for (size_t i = 1; i < cnt; i += 2) {
		fill_novalue_kv(ut_env, &kv, i);
		ut_removexattr_ok(ut_env, ino, &kv);
		fill_novalue_kv(ut_env, &kv, ~i);
		ut_setxattr_create(ut_env, ino, &kv);
		fill_novalue_kv(ut_env, &kv, ~(i - 1));
		ut_setxattr_create(ut_env, ino, &kv);
	}
	for (size_t i = 0; i < cnt; ++i) {
		fill_novalue_kv(ut_env, &kv, ~i);
		ut_removexattr_ok(ut_env, ino, &kv);
	}
	ut_rmdir_at_root(ut_env, dname);
}

static void ut_xattr_no_value(struct ut_env *ut_env)
{
	ut_xattr_no_value_(ut_env, 40);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_xattr_multi(struct ut_env *ut_env)
{
	ino_t ino;
	ino_t dino;
	const char *dname = UT_NAME;
	const char *fname = UT_NAME;
	struct ut_kvl *kvl;
	const struct voluta_kv_sizes kv_sizes_arr[] = {
		{ 1, 1 },
		{ UT_NAME_MAX / 2, 2 },
		{ 2, VOLUTA_XATTR_VALUE_MAX / 2 },
		{ UT_NAME_MAX / 16, 16 },
		{ 32, VOLUTA_XATTR_VALUE_MAX / 32 },
		{ UT_NAME_MAX, 128 },
		{ 64, VOLUTA_XATTR_VALUE_MAX },
	};
	const size_t nkv_sizes = UT_ARRAY_SIZE(kv_sizes_arr);

	ut_mkdir_at_root(ut_env, dname, &dino);
	ut_create_only(ut_env, dino, fname, &ino);

	for (size_t i = 0; i < 4; ++i) {
		kvl = kvl_new(ut_env, nkv_sizes);
		kvl_appendn(kvl, kv_sizes_arr, nkv_sizes);

		ut_setxattr_all(ut_env, dino, kvl);
		ut_listxattr_ok(ut_env, dino, kvl);
		ut_setxattr_all(ut_env, ino, kvl);
		ut_listxattr_ok(ut_env, ino, kvl);
		kvl_reverse(kvl);
		ut_listxattr_ok(ut_env, dino, kvl);
		ut_listxattr_ok(ut_env, ino, kvl);
		ut_removexattr_all(ut_env, ino, kvl);
		ut_drop_caches_fully(ut_env);
		kvl_random_shuffle(kvl);
		ut_setxattr_all(ut_env, ino, kvl);
		ut_listxattr_ok(ut_env, ino, kvl);
		ut_listxattr_ok(ut_env, dino, kvl);
		kvl_random_shuffle(kvl);
		ut_listxattr_ok(ut_env, ino, kvl);
		ut_removexattr_all(ut_env, ino, kvl);
		ut_removexattr_all(ut_env, dino, kvl);
	}
	ut_unlink_ok(ut_env, dino, fname);
	ut_rmdir_at_root(ut_env, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_xattr_lookup_random(struct ut_env *ut_env)
{
	ino_t ino;
	ino_t dino;
	const ino_t root_ino = UT_ROOT_INO;
	const char *dname = UT_NAME;
	const char *xname;
	struct ut_kvl *kvl = kvl_new(ut_env, 4);

	kvl_populate_max(kvl);
	ut_mkdir_oki(ut_env, root_ino, dname, &dino);
	for (size_t i = 0; i < kvl->count; ++i) {
		xname = kvl->list[i]->name;
		ut_create_file(ut_env, dino, xname, &ino);
		ut_setxattr_all(ut_env, ino, kvl);
		ut_listxattr_ok(ut_env, ino, kvl);
	}
	for (size_t i = 0; i < kvl->count; ++i) {
		xname = kvl->list[i]->name;
		ut_lookup_ino(ut_env, dino, xname, &ino);
		ut_listxattr_ok(ut_env, ino, kvl);
	}
	kvl_random_shuffle(kvl);
	for (size_t i = 0; i < kvl->count; i += 3) {
		xname = kvl->list[i]->name;
		ut_lookup_ino(ut_env, dino, xname, &ino);
		ut_removexattr_all(ut_env, ino, kvl);
	}
	kvl_random_shuffle(kvl);
	for (size_t i = 0; i < kvl->count; ++i) {
		xname = kvl->list[i]->name;
		ut_lookup_ino(ut_env, dino, xname, &ino);
		ut_remove_file(ut_env, dino, xname, ino);
	}
	ut_rmdir_ok(ut_env, root_ino, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_xattr_replace(struct ut_env *ut_env)
{
	ino_t ino;
	ino_t dino;
	const ino_t root_ino = UT_ROOT_INO;
	const char *dname = UT_NAME;
	const char *fname = UT_NAME;
	struct ut_kvl *kvl = kvl_new(ut_env, 5);

	kvl_populate(kvl, UT_NAME_MAX / 2, VOLUTA_XATTR_VALUE_MAX / 2);

	ut_mkdir_oki(ut_env, root_ino, dname, &dino);
	ut_create_file(ut_env, dino, fname, &ino);
	for (size_t i = 0; i < kvl->count; ++i) {
		ut_setxattr_create(ut_env, ino, kvl->list[i]);
	}
	for (size_t i = 0; i < kvl->count; ++i) {
		kvl->list[i]->size = VOLUTA_XATTR_VALUE_MAX / 3;
		ut_setxattr_replace(ut_env, ino, kvl->list[i]);
	}
	for (size_t i = 0; i < kvl->count; ++i) {
		kvl->list[i]->size = VOLUTA_XATTR_VALUE_MAX / 4;
		ut_setxattr_rereplace(ut_env, ino, kvl->list[i]);
	}
	for (size_t i = 0; i < kvl->count; ++i) {
		ut_removexattr_ok(ut_env, ino, kvl->list[i]);
	}
	ut_remove_file(ut_env, dino, fname, ino);
	ut_rmdir_ok(ut_env, root_ino, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_xattr_replace_multi(struct ut_env *ut_env)
{
	ino_t ino;
	ino_t dino;
	size_t value_size;
	const ino_t root_ino = UT_ROOT_INO;
	const char *dname = UT_NAME;
	const char *fname = UT_NAME;
	struct ut_kvl *kvl = kvl_new(ut_env, 3);

	value_size = VOLUTA_XATTR_VALUE_MAX;
	kvl_populate(kvl, UT_NAME_MAX / 2, value_size);

	ut_mkdir_oki(ut_env, root_ino, dname, &dino);
	ut_create_file(ut_env, dino, fname, &ino);
	for (size_t i = 0; i < kvl->count; ++i) {
		ut_setxattr_create(ut_env, ino, kvl->list[i]);
	}
	for (size_t i = 0; i < kvl->count; ++i) {
		for (size_t j = value_size; j > 2; --j) {
			kvl->list[i]->size = j - 1;
			ut_setxattr_replace(ut_env, ino, kvl->list[i]);
			kvl->list[i]->size = j - 2;
			ut_setxattr_rereplace(ut_env, ino,
					      kvl->list[i]);
		}
	}
	for (size_t i = 0; i < kvl->count; ++i) {
		ut_removexattr_ok(ut_env, ino, kvl->list[i]);
	}
	ut_remove_file(ut_env, dino, fname, ino);
	ut_rmdir_ok(ut_env, root_ino, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_xattr_simple),
	UT_DEFTEST(ut_xattr_long_names),
	UT_DEFTEST(ut_xattr_shorts),
	UT_DEFTEST(ut_xattr_no_value),
	UT_DEFTEST(ut_xattr_multi),
	UT_DEFTEST(ut_xattr_lookup_random),
	UT_DEFTEST(ut_xattr_replace),
	UT_DEFTEST(ut_xattr_replace_multi),
};

const struct ut_tests ut_test_xattr = UT_MKTESTS(ut_local_tests);
