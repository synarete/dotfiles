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
#define VOLUTA_TEST 1
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include "unitest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_reload_nfiles_(struct voluta_ut_ctx *ut_ctx, size_t nfiles)
{
	ino_t ino;
	ino_t dino;
	const char *fname;
	const char *dname = T_NAME;

	voluta_ut_mkdir_at_root(ut_ctx, dname, &dino);
	voluta_ut_reload_ok(ut_ctx, dino);
	for (size_t i = 0; i < nfiles; ++i) {
		fname = ut_make_name(ut_ctx, "f", i);
		voluta_ut_create_only(ut_ctx, dino, fname, &ino);
	}
	voluta_ut_reload_ok(ut_ctx, dino);
	for (size_t i = 0; i < nfiles; ++i) {
		fname = ut_make_name(ut_ctx, "f", i);
		voluta_ut_remove_link(ut_ctx, dino, fname);
	}
	voluta_ut_reload_ok(ut_ctx, dino);
	voluta_ut_rmdir_at_root(ut_ctx, dname);
}

static void ut_file_reload_simple(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_reload_nfiles_(ut_ctx, 0);
}

static void ut_file_reload_nfiles(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_reload_nfiles_(ut_ctx, 1);
	ut_file_reload_nfiles_(ut_ctx, 10);
	ut_file_reload_nfiles_(ut_ctx, 1000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_reload_mixed_(struct voluta_ut_ctx *ut_ctx, size_t nfiles)
{
	ino_t fino;
	ino_t sino;
	ino_t dino;
	ino_t tino;
	const char *name;
	const char *tname = T_NAME;
	struct stat st;

	voluta_ut_mkdir_at_root(ut_ctx, tname, &tino);
	voluta_ut_reload_ok(ut_ctx, tino);
	for (size_t i = 0; i < nfiles; ++i) {
		name = ut_make_name(ut_ctx, "d", i);
		voluta_ut_mkdir_ok(ut_ctx, tino, name, &dino);
		name = ut_make_name(ut_ctx, "f", i);
		voluta_ut_create_only(ut_ctx, dino, name, &fino);
		name = ut_make_name(ut_ctx, "s", i);
		voluta_ut_create_symlink(ut_ctx, dino, name, tname, &sino);
		voluta_ut_reload_ok(ut_ctx, dino);
		voluta_ut_getattr_file(ut_ctx, fino, &st);
		voluta_ut_lookup_lnk(ut_ctx, dino, name, sino);
	}
	for (size_t i = 0; i < nfiles; ++i) {
		name = ut_make_name(ut_ctx, "d", i);
		voluta_ut_lookup_ok(ut_ctx, tino, name, &dino);
		voluta_ut_getattr_dir(ut_ctx, dino, &st);
		name = ut_make_name(ut_ctx, "f", i);
		voluta_ut_lookup_ok(ut_ctx, dino, name, &fino);
		voluta_ut_getattr_file(ut_ctx, fino, &st);
		voluta_ut_reload_ok(ut_ctx, dino);
		voluta_ut_remove_link(ut_ctx, dino, name);
		name = ut_make_name(ut_ctx, "s", i);
		voluta_ut_lookup_ok(ut_ctx, dino, name, &sino);
		voluta_ut_getattr_lnk(ut_ctx, sino, &st);
		voluta_ut_remove_link(ut_ctx, dino, name);
		name = ut_make_name(ut_ctx, "d", i);
		voluta_ut_rmdir_ok(ut_ctx, tino, name);
	}
	voluta_ut_reload_ok(ut_ctx, tino);
	voluta_ut_rmdir_at_root(ut_ctx, tname);
}

static void ut_file_reload_mixed(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_reload_mixed_(ut_ctx, 10);
	ut_file_reload_mixed_(ut_ctx, 100);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static loff_t make_offset(size_t idx, size_t step)
{
	return (loff_t)((idx * step) + idx);
}

static void ut_file_reload_io_(struct voluta_ut_ctx *ut_ctx,
			       size_t nfiles, size_t step)
{
	ino_t fino;
	ino_t dino;
	loff_t off;
	size_t len;
	const char *fname;
	const char *dname = T_NAME;
	struct stat st;

	voluta_ut_mkdir_at_root(ut_ctx, dname, &dino);
	for (size_t i = 0; i < nfiles; ++i) {
		fname = ut_make_name(ut_ctx, "f", i);
		voluta_ut_create_file(ut_ctx, dino, fname, &fino);
		len = strlen(fname);
		off = make_offset(i, step);
		voluta_ut_write_read(ut_ctx, fino, fname, len, off);
		voluta_ut_release_file(ut_ctx, fino);
	}
	voluta_ut_reload_ok(ut_ctx, dino);
	for (size_t i = 0; i < nfiles; ++i) {
		fname = ut_make_name(ut_ctx, "f", i);
		voluta_ut_lookup_ok(ut_ctx, dino, fname, &fino);
		voluta_ut_open_rdonly(ut_ctx, fino);
		len = strlen(fname);
		off = make_offset(i, step);
		voluta_ut_read_verify(ut_ctx, fino, fname, len, off);
		voluta_ut_trunacate_file(ut_ctx, fino, off);
		voluta_ut_release_file(ut_ctx, fino);
	}
	voluta_ut_reload_ok(ut_ctx, dino);
	for (size_t i = 0; i < nfiles; ++i) {
		fname = ut_make_name(ut_ctx, "f", i);
		voluta_ut_lookup_ok(ut_ctx, dino, fname, &fino);
		off = make_offset(i, step);
		voluta_ut_getattr_file(ut_ctx, fino, &st);
		ut_assert_eq(st.st_size, off);
		voluta_ut_unlink_exists(ut_ctx, dino, fname);
	}
	voluta_ut_rmdir_at_root(ut_ctx, dname);
}

static void ut_file_reload_io(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_reload_io_(ut_ctx, 10, VOLUTA_GIGA);
	ut_file_reload_io_(ut_ctx, 100, VOLUTA_MEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_reload_unlinked_(struct voluta_ut_ctx *ut_ctx,
				     size_t nfiles, size_t step)
{
	ino_t fino;
	ino_t dino;
	loff_t off;
	size_t len;
	const char *fname;
	const char *dname = T_NAME;
	ino_t *fino_arr = voluta_ut_zalloc(ut_ctx, nfiles * sizeof(ino_t));

	voluta_ut_mkdir_at_root(ut_ctx, dname, &dino);
	for (size_t i = 0; i < nfiles; ++i) {
		fname = ut_make_name(ut_ctx, "f", i);
		voluta_ut_create_file(ut_ctx, dino, fname, &fino);
		fino_arr[i] = fino;
		len = strlen(fname);
		off = make_offset(i, step);
		voluta_ut_write_read(ut_ctx, fino, fname, len, off);
		voluta_ut_unlink_file(ut_ctx, dino, fname);
	}
	for (size_t i = 0; i < nfiles; ++i) {
		fname = ut_make_name(ut_ctx, "f", i);
		fino = fino_arr[i];
		len = strlen(fname);
		off = make_offset(i, step);
		voluta_ut_read_verify(ut_ctx, fino, fname, len, off);
		voluta_ut_release_file(ut_ctx, fino);
	}
	voluta_ut_reload_ok(ut_ctx, dino);
	voluta_ut_rmdir_at_root(ut_ctx, dname);
}

static void ut_file_reload_unlinked(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_reload_unlinked_(ut_ctx, 10, VOLUTA_GIGA - 1);
	ut_file_reload_unlinked_(ut_ctx, 1000, VOLUTA_MEGA - 1);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_file_reload_simple),
	UT_DEFTEST(ut_file_reload_nfiles),
	UT_DEFTEST(ut_file_reload_mixed),
	UT_DEFTEST(ut_file_reload_io),
	UT_DEFTEST(ut_file_reload_unlinked),
};

const struct voluta_ut_tests voluta_ut_test_reload =
	UT_MKTESTS(ut_local_tests);
