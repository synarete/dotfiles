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


static const char *make_symname(struct ut_env *ut_env, size_t idx)
{
	return ut_make_name(ut_env, "symlink", idx);
}

static char *make_symval(struct ut_env *ut_env, char c, size_t len)
{
	char *val;
	const size_t vsz = VOLUTA_PATH_MAX;
	const size_t name_max = UT_NAME_MAX;

	ut_assert_lt(len, vsz);
	val = ut_zerobuf(ut_env, vsz);
	for (size_t i = 0; i < len; ++i) {
		if (i % name_max) {
			val[i] = c;
		} else {
			val[i] = '/';
		}
	}
	return val;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_symlink_simple(struct ut_env *ut_env)
{
	ino_t dino;
	ino_t tino;
	ino_t sino;
	const char *dname = UT_NAME;
	const char *tname = "target";
	const char *sname = "symlink";

	ut_mkdir_at_root(ut_env, dname, &dino);
	ut_mkdir_oki(ut_env, dino, tname, &tino);
	ut_symlink_ok(ut_env, dino, sname, tname, &sino);
	ut_lookup_exists(ut_env, dino, sname, sino, S_IFLNK);
	ut_readlink_expect(ut_env, sino, tname);
	ut_rmdir_ok(ut_env, dino, tname);
	ut_lookup_exists(ut_env, dino, sname, sino, S_IFLNK);
	ut_readlink_expect(ut_env, sino, tname);
	ut_rmdir_err(ut_env, UT_ROOT_INO, dname, -ENOTEMPTY);
	ut_unlink_ok(ut_env, dino, sname);
	ut_rmdir_at_root(ut_env, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_symlink_length(struct ut_env *ut_env)
{
	ino_t dino;
	ino_t sino;
	const ino_t root_ino = UT_ROOT_INO;
	const char *dname = UT_NAME;
	const char *tname;
	const char *sname;
	const size_t nlinks = VOLUTA_PATH_MAX - 1;

	ut_mkdir_oki(ut_env, root_ino, dname, &dino);
	for (size_t i = 1; i <= nlinks; ++i) {
		sname = make_symname(ut_env, i);
		tname = make_symval(ut_env, 'A', i);
		ut_symlink_ok(ut_env, dino, sname, tname, &sino);
	}
	ut_rmdir_err(ut_env, root_ino, dname, -ENOTEMPTY);
	for (size_t j = 1; j <= nlinks; ++j) {
		sname = make_symname(ut_env, j);
		ut_unlink_ok(ut_env, dino, sname);
	}
	ut_rmdir_ok(ut_env, root_ino, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_symlink_nested(struct ut_env *ut_env)
{
	ino_t sino;
	ino_t dino[128];
	const char *sname[128];
	const char *dname = UT_NAME;
	struct stat st;

	dino[0] = UT_ROOT_INO;
	for (size_t i = 1; i < UT_ARRAY_SIZE(dino); ++i) {
		ut_mkdir_oki(ut_env, dino[i - 1], dname, &dino[i]);
		sname[i] = make_symname(ut_env, 8 * i);
		ut_symlink_ok(ut_env, dino[i], sname[i],
			      make_symval(ut_env, 'z', i), &sino);
		ut_rmdir_err(ut_env, dino[i - 1], dname, -ENOTEMPTY);
	}
	for (size_t j = UT_ARRAY_SIZE(dino); j > 1; --j) {
		ut_unlink_ok(ut_env, dino[j - 1], sname[j - 1]);
		ut_rmdir_ok(ut_env, dino[j - 2], dname);
	}
	ut_getattr_ok(ut_env, dino[0], &st);
	ut_assert_eq(st.st_size, VOLUTA_DIR_EMPTY_SIZE);
	ut_assert_eq(st.st_nlink, 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_symlink_to_reg_(struct ut_env *ut_env, size_t cnt)
{
	ino_t dino;
	ino_t ino;
	ino_t sino;
	const char *dname = UT_NAME;
	const char *fname;
	const char *sname;

	ut_mkdir_at_root(ut_env, dname, &dino);
	for (size_t i = 0; i < cnt; ++i) {
		sname = make_symname(ut_env, i);
		fname = ut_make_name(ut_env, dname, i);
		ut_create_only(ut_env, dino, fname, &ino);
		ut_symlink_ok(ut_env, dino, sname, fname, &sino);
	}
	for (size_t i = 0; i < cnt; ++i) {
		sname = make_symname(ut_env, i);
		fname = ut_make_name(ut_env, dname, i);
		ut_lookup_ino(ut_env, dino, sname, &sino);
		ut_lookup_lnk(ut_env, dino, sname, sino);
		ut_readlink_expect(ut_env, sino, fname);
		ut_unlink_ok(ut_env, dino, sname);
	}
	for (size_t i = 0; i < cnt; ++i) {
		fname = ut_make_name(ut_env, dname, i);
		ut_unlink_ok(ut_env, dino, fname);
	}
	ut_rmdir_at_root(ut_env, dname);
}

static void ut_symlink_to_reg(struct ut_env *ut_env)
{
	ut_symlink_to_reg_(ut_env, 64);
	ut_symlink_to_reg_(ut_env, 1024);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static char *ut_make_asymval(struct ut_env *ut_env, size_t len)
{
	const char *abc = "abcdefghijklmnopqrstuvwxyz";

	return make_symval(ut_env, abc[strlen(abc) % len], len);
}

static void ut_symlink_and_io_(struct ut_env *ut_env, size_t cnt)
{
	loff_t off;
	ino_t dino;
	ino_t fino;
	ino_t sino;
	char *symval;
	const char *fname = NULL;
	const char *sname = NULL;
	const char *fp = "f";
	const char *sp = "s";
	const char *dname = UT_NAME;

	ut_mkdir_at_root(ut_env, dname, &dino);
	for (size_t i = 0; i < cnt; ++i) {
		sname = ut_make_name(ut_env, sp, i);
		fname = ut_make_name(ut_env, fp, i);
		symval = ut_make_asymval(ut_env, i + 1);
		ut_create_file(ut_env, dino, fname, &fino);
		ut_symlink_ok(ut_env, dino, sname, symval, &sino);

		off = (loff_t)(i * UT_UMEGA + i);
		ut_write_read_str(ut_env, fino, symval, off);
	}
	for (size_t i = 0; i < cnt; ++i) {
		sname = ut_make_name(ut_env, sp, i);
		fname = ut_make_name(ut_env, fp, i);
		symval = ut_make_asymval(ut_env, i + 1);
		ut_lookup_ino(ut_env, dino, sname, &sino);
		ut_readlink_expect(ut_env, sino, symval);

		ut_lookup_ino(ut_env, dino, fname, &fino);
		off = (loff_t)(i * UT_UMEGA + i);
		ut_read_verify_str(ut_env, fino, symval, off);
	}
	for (size_t i = 0; i < cnt; ++i) {
		sname = ut_make_name(ut_env, sp, i);
		fname = ut_make_name(ut_env, fp, i);

		ut_lookup_ino(ut_env, dino, fname, &fino);
		ut_release_file(ut_env, fino);
		ut_unlink_ok(ut_env, dino, sname);
		ut_unlink_ok(ut_env, dino, fname);
	}
	ut_rmdir_at_root(ut_env, dname);
}

static void ut_symlink_and_io(struct ut_env *ut_env)
{
	ut_symlink_and_io_(ut_env, 100);
	ut_symlink_and_io_(ut_env, 4000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_symlink_and_io2_(struct ut_env *ut_env, size_t cnt)
{
	loff_t off;
	ino_t dino;
	ino_t fino;
	ino_t sino;
	char *symval;
	const char *fname = NULL;
	const char *sname = NULL;
	const char *dname = UT_NAME;
	const char *ff = "ff";
	const char *s1 = "s1";
	const char *s2 = "s2";
	const ino_t root_ino = UT_ROOT_INO;

	ut_mkdir_oki(ut_env, root_ino, dname, &dino);
	for (size_t i = 0; i < cnt; ++i) {
		sname = ut_make_name(ut_env, s1, i);
		fname = ut_make_name(ut_env, ff, i);
		symval = ut_make_asymval(ut_env, cnt);
		ut_create_file(ut_env, dino, fname, &fino);
		ut_symlink_ok(ut_env, dino, sname, symval, &sino);

		off = (loff_t)(i * cnt);
		ut_write_read_str(ut_env, fino, symval, off);
		ut_release_file(ut_env, fino);
	}
	ut_drop_caches_fully(ut_env);
	for (size_t j = cnt; j > 0; --j) {
		sname = ut_make_name(ut_env, s1, j - 1);
		fname = ut_make_name(ut_env, ff, j - 1);
		symval = ut_make_asymval(ut_env, cnt);
		ut_lookup_ino(ut_env, dino, sname, &sino);
		ut_readlink_expect(ut_env, sino, symval);
		ut_lookup_ino(ut_env, dino, fname, &fino);

		ut_open_rdonly(ut_env, fino);
		off = (loff_t)((j - 1) * cnt);
		ut_read_verify_str(ut_env, fino, symval, off);
		ut_release_file(ut_env, fino);
		ut_unlink_ok(ut_env, dino, fname);

		sname = ut_make_name(ut_env, s2, j - 1);
		ut_symlink_ok(ut_env, dino, sname, symval, &sino);
	}
	ut_drop_caches_fully(ut_env);
	for (size_t i = 0; i < cnt; ++i) {
		fname = ut_make_name(ut_env, ff, i);
		ut_create_only(ut_env, dino, fname, &fino);
		sname = ut_make_name(ut_env, s1, i);
		ut_unlink_ok(ut_env, dino, sname);
		ut_lookup_file(ut_env, dino, fname, fino);
	}
	ut_drop_caches_fully(ut_env);
	for (size_t i = 0; i < cnt; ++i) {
		fname = ut_make_name(ut_env, ff, i);
		sname = ut_make_name(ut_env, s2, i);
		ut_unlink_ok(ut_env, dino, sname);
		ut_unlink_ok(ut_env, dino, fname);
	}
	ut_rmdir_ok(ut_env, root_ino, dname);
}

static void ut_symlink_and_io2(struct ut_env *ut_env)
{
	ut_symlink_and_io2_(ut_env, 1);
	ut_symlink_and_io2_(ut_env, 2);
	ut_symlink_and_io2_(ut_env, 11);

	ut_symlink_and_io2_(ut_env, 111);
	ut_symlink_and_io2_(ut_env, 1111);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_symlink_simple),
	UT_DEFTEST(ut_symlink_length),
	UT_DEFTEST(ut_symlink_nested),
	UT_DEFTEST(ut_symlink_to_reg),
	UT_DEFTEST(ut_symlink_and_io),
	UT_DEFTEST(ut_symlink_and_io2),
};

const struct ut_tests ut_test_symlink = UT_MKTESTS(ut_local_tests);
