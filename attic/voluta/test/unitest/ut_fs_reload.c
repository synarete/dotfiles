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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include "unitest.h"


static void ut_fs_reload_nfiles_(struct ut_env *ut_env, size_t nfiles)
{
	ino_t ino;
	ino_t dino;
	const char *fname;
	const char *dname = UT_NAME;

	ut_mkdir_at_root(ut_env, dname, &dino);
	ut_reload_ok(ut_env, dino);
	for (size_t i = 0; i < nfiles; ++i) {
		fname = ut_make_name(ut_env, "f", i);
		ut_create_only(ut_env, dino, fname, &ino);
	}
	ut_reload_ok(ut_env, dino);
	for (size_t i = 0; i < nfiles; ++i) {
		fname = ut_make_name(ut_env, "f", i);
		ut_remove_link(ut_env, dino, fname);
	}
	ut_reload_ok(ut_env, dino);
	ut_rmdir_at_root(ut_env, dname);
}

static void ut_fs_reload_simple(struct ut_env *ut_env)
{
	ut_fs_reload_nfiles_(ut_env, 0);
}

static void ut_fs_reload_nfiles(struct ut_env *ut_env)
{
	ut_fs_reload_nfiles_(ut_env, 1);
	ut_fs_reload_nfiles_(ut_env, 11);
	ut_fs_reload_nfiles_(ut_env, 1111);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_fs_reload_mixed_(struct ut_env *ut_env, size_t nfiles)
{
	ino_t fino;
	ino_t sino;
	ino_t dino;
	ino_t tino;
	const char *name;
	const char *tname = UT_NAME;
	struct stat st;

	ut_mkdir_at_root(ut_env, tname, &tino);
	ut_reload_ok(ut_env, tino);
	for (size_t i = 0; i < nfiles; ++i) {
		name = ut_make_name(ut_env, "d", i);
		ut_mkdir_oki(ut_env, tino, name, &dino);
		name = ut_make_name(ut_env, "f", i);
		ut_create_only(ut_env, dino, name, &fino);
		name = ut_make_name(ut_env, "s", i);
		ut_symlink_ok(ut_env, dino, name, tname, &sino);
		ut_reload_ok(ut_env, dino);
		ut_getattr_file(ut_env, fino, &st);
		ut_lookup_lnk(ut_env, dino, name, sino);
	}
	for (size_t i = 0; i < nfiles; ++i) {
		name = ut_make_name(ut_env, "d", i);
		ut_lookup_ino(ut_env, tino, name, &dino);
		ut_getattr_dir(ut_env, dino, &st);
		name = ut_make_name(ut_env, "f", i);
		ut_lookup_ino(ut_env, dino, name, &fino);
		ut_getattr_file(ut_env, fino, &st);
		ut_reload_ok(ut_env, dino);
		ut_remove_link(ut_env, dino, name);
		name = ut_make_name(ut_env, "s", i);
		ut_lookup_ino(ut_env, dino, name, &sino);
		ut_getattr_lnk(ut_env, sino, &st);
		ut_remove_link(ut_env, dino, name);
		name = ut_make_name(ut_env, "d", i);
		ut_rmdir_ok(ut_env, tino, name);
	}
	ut_reload_ok(ut_env, tino);
	ut_rmdir_at_root(ut_env, tname);
}

static void ut_fs_reload_mixed(struct ut_env *ut_env)
{
	ut_fs_reload_mixed_(ut_env, 10);
	ut_fs_reload_mixed_(ut_env, 100);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static loff_t make_offset(size_t idx, size_t step)
{
	return (loff_t)((idx * step) + idx);
}

static void ut_fs_reload_io_(struct ut_env *ut_env, size_t nfiles, size_t step)
{
	ino_t fino;
	ino_t dino;
	loff_t off;
	size_t len;
	const char *fname;
	const char *dname = UT_NAME;
	struct stat st;

	ut_mkdir_at_root(ut_env, dname, &dino);
	for (size_t i = 0; i < nfiles; ++i) {
		fname = ut_make_name(ut_env, "f", i);
		ut_create_file(ut_env, dino, fname, &fino);
		len = strlen(fname);
		off = make_offset(i, step);
		ut_write_read(ut_env, fino, fname, len, off);
		ut_release_file(ut_env, fino);
	}
	ut_reload_ok(ut_env, dino);
	for (size_t i = 0; i < nfiles; ++i) {
		fname = ut_make_name(ut_env, "f", i);
		ut_lookup_ino(ut_env, dino, fname, &fino);
		ut_open_rdonly(ut_env, fino);
		len = strlen(fname);
		off = make_offset(i, step);
		ut_read_verify(ut_env, fino, fname, len, off);
		ut_trunacate_file(ut_env, fino, off);
		ut_release_file(ut_env, fino);
	}
	ut_reload_ok(ut_env, dino);
	for (size_t i = 0; i < nfiles; ++i) {
		fname = ut_make_name(ut_env, "f", i);
		ut_lookup_ino(ut_env, dino, fname, &fino);
		off = make_offset(i, step);
		ut_getattr_file(ut_env, fino, &st);
		ut_assert_eq(st.st_size, off);
		ut_unlink_ok(ut_env, dino, fname);
	}
	ut_rmdir_at_root(ut_env, dname);
}

static void ut_fs_reload_io(struct ut_env *ut_env)
{
	ut_fs_reload_io_(ut_env, 10, VOLUTA_GIGA);
	ut_fs_reload_io_(ut_env, 100, VOLUTA_MEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_fs_reload_unlinked_(struct ut_env *ut_env,
				   size_t nfiles, size_t step)
{
	ino_t fino;
	ino_t dino;
	loff_t off;
	size_t len;
	const char *fname;
	const char *dname = UT_NAME;
	ino_t *fino_arr = ut_zalloc(ut_env, nfiles * sizeof(ino_t));

	ut_mkdir_at_root(ut_env, dname, &dino);
	for (size_t i = 0; i < nfiles; ++i) {
		fname = ut_make_name(ut_env, "f", i);
		ut_create_file(ut_env, dino, fname, &fino);
		fino_arr[i] = fino;
		len = strlen(fname);
		off = make_offset(i, step);
		ut_write_read(ut_env, fino, fname, len, off);
		ut_unlink_file(ut_env, dino, fname);
	}
	for (size_t i = 0; i < nfiles; ++i) {
		fname = ut_make_name(ut_env, "f", i);
		fino = fino_arr[i];
		len = strlen(fname);
		off = make_offset(i, step);
		ut_read_verify(ut_env, fino, fname, len, off);
		ut_release_file(ut_env, fino);
	}
	ut_reload_ok(ut_env, dino);
	ut_rmdir_at_root(ut_env, dname);
}

static void ut_fs_reload_unlinked(struct ut_env *ut_env)
{
	ut_fs_reload_unlinked_(ut_env, 10, VOLUTA_GIGA - 1);
	ut_fs_reload_unlinked_(ut_env, 1000, VOLUTA_MEGA - 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_fs_reload_simple),
	UT_DEFTEST(ut_fs_reload_nfiles),
	UT_DEFTEST(ut_fs_reload_mixed),
	UT_DEFTEST(ut_fs_reload_io),
	UT_DEFTEST(ut_fs_reload_unlinked),
};

const struct ut_tests ut_test_fs_reload = UT_MKTESTS(ut_local_tests);
