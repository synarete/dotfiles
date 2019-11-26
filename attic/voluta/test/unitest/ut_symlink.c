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


static char *make_name(struct voluta_ut_ctx *ut_ctx, const char *prefix,
		       size_t idx)
{
	return ut_strfmt(ut_ctx, "%s-%ld", prefix, (long)idx);
}

static const char *make_symname(struct voluta_ut_ctx *ut_ctx, size_t idx)
{
	return make_name(ut_ctx, "symlink", idx);
}

static char *make_symval(struct voluta_ut_ctx *ut_ctx, char c, size_t len)
{
	char *val;
	const size_t vsz = VOLUTA_PATH_MAX;
	const size_t name_max = VOLUTA_NAME_MAX;

	ut_assert_lt(len, vsz);
	val = voluta_ut_zerobuf(ut_ctx, vsz);
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

static void ut_symlink_simple(struct voluta_ut_ctx *ut_ctx)
{
	ino_t dino, tino, sino, root_ino = VOLUTA_INO_ROOT;
	const char *dname = UT_NAME;
	const char *tname = "target";
	const char *sname = "symlink";

	voluta_ut_make_dir(ut_ctx, root_ino, dname, &dino);
	voluta_ut_make_dir(ut_ctx, dino, tname, &tino);
	voluta_ut_create_symlink(ut_ctx, dino, sname, tname, &sino);
	voluta_ut_lookup_exists(ut_ctx, dino, sname, sino, S_IFLNK);
	voluta_ut_readlink_expect(ut_ctx, sino, tname);
	voluta_ut_remove_dir(ut_ctx, dino, tname);
	voluta_ut_lookup_exists(ut_ctx, dino, sname, sino, S_IFLNK);
	voluta_ut_readlink_expect(ut_ctx, sino, tname);
	voluta_ut_noremove_dir(ut_ctx, root_ino, dname);
	voluta_ut_unlink_exists(ut_ctx, dino, sname);
	voluta_ut_remove_dir(ut_ctx, root_ino, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_symlink_length(struct voluta_ut_ctx *ut_ctx)
{
	ino_t dino, sino, root_ino = VOLUTA_INO_ROOT;
	const char *dname = UT_NAME;
	const char *tname;
	const char *sname;
	const size_t nlinks = VOLUTA_PATH_MAX - 1;

	voluta_ut_make_dir(ut_ctx, root_ino, dname, &dino);
	for (size_t i = 1; i <= nlinks; ++i) {
		sname = make_symname(ut_ctx, i);
		tname = make_symval(ut_ctx, 'A', i);
		voluta_ut_create_symlink(ut_ctx, dino, sname, tname, &sino);
	}
	voluta_ut_noremove_dir(ut_ctx, root_ino, dname);
	for (size_t j = 1; j <= nlinks; ++j) {
		sname = make_symname(ut_ctx, j);
		voluta_ut_unlink_exists(ut_ctx, dino, sname);
	}
	voluta_ut_remove_dir(ut_ctx, root_ino, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_symlink_nested(struct voluta_ut_ctx *ut_ctx)
{
	ino_t sino, dino[128];
	const char *sname[128];
	const char *dname = UT_NAME;
	struct stat st;

	dino[0] = VOLUTA_INO_ROOT;
	for (size_t i = 1; i < VOLUTA_ARRAY_SIZE(dino); ++i) {
		voluta_ut_make_dir(ut_ctx, dino[i - 1], dname, &dino[i]);
		sname[i] = make_symname(ut_ctx, 8 * i);
		voluta_ut_create_symlink(ut_ctx, dino[i], sname[i],
					 make_symval(ut_ctx, 'z', i), &sino);
		voluta_ut_noremove_dir(ut_ctx, dino[i - 1], dname);
	}
	for (size_t j = VOLUTA_ARRAY_SIZE(dino); j > 1; --j) {
		voluta_ut_unlink_exists(ut_ctx, dino[j - 1], sname[j - 1]);
		voluta_ut_remove_dir(ut_ctx, dino[j - 2], dname);
	}
	voluta_ut_getattr_exists(ut_ctx, dino[0], &st);
	voluta_ut_assert_eq(st.st_size, VOLUTA_DIREMPTY_SIZE);
	voluta_ut_assert_eq(st.st_nlink, 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const char *make_regname(struct voluta_ut_ctx *ut_ctx, size_t idx)
{
	size_t len;
	char prefix[VOLUTA_NAME_MAX + 1] = "reg";

	for (size_t i = 0; i < idx; ++i) {
		len = strlen(prefix);
		if (len < (sizeof(prefix) - 1)) {
			prefix[len] = '-';
			prefix[len + 1] = '\0';
		}
	}
	return make_name(ut_ctx, prefix, idx);
}

static void ut_symlink_to_reg(struct voluta_ut_ctx *ut_ctx)
{
	size_t i, cnt = 128;
	ino_t dino, ino, sino, root_ino = VOLUTA_INO_ROOT;
	const char *dname = UT_NAME;
	const char *fname;
	const char *sname;

	voluta_ut_make_dir(ut_ctx, root_ino, dname, &dino);
	for (i = 0; i < cnt; ++i) {
		sname = make_symname(ut_ctx, i);
		fname = make_regname(ut_ctx, i);
		voluta_ut_create_only(ut_ctx, dino, fname, &ino);
		voluta_ut_create_symlink(ut_ctx, dino, sname, fname, &sino);
	}
	for (i = 0; i < cnt; ++i) {
		sname = make_symname(ut_ctx, i);
		fname = make_regname(ut_ctx, i);
		voluta_ut_lookup_resolve(ut_ctx, dino, sname, &sino);
		voluta_ut_lookup_lnk(ut_ctx, dino, sname, sino);
		voluta_ut_readlink_expect(ut_ctx, sino, fname);
		voluta_ut_unlink_exists(ut_ctx, dino, sname);
	}
	for (i = 0; i < cnt; ++i) {
		fname = make_regname(ut_ctx, i);
		voluta_ut_unlink_exists(ut_ctx, dino, fname);
	}
	voluta_ut_remove_dir(ut_ctx, root_ino, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static char *make_asymval(struct voluta_ut_ctx *ut_ctx, size_t len)
{
	const char *abc = "abcdefghijklmnopqrstuvwxyz";

	return make_symval(ut_ctx, abc[strlen(abc) % len], len);
}

static void ut_symlink_and_io(struct voluta_ut_ctx *ut_ctx)
{
	loff_t off;
	size_t i, cnt = 2048;
	ino_t dino, fino, sino, root_ino = VOLUTA_INO_ROOT;
	char fp[] = "f", sp[] = "s";
	char *fname, *sname, *symval;
	const char *dname = UT_NAME;

	voluta_ut_make_dir(ut_ctx, root_ino, dname, &dino);
	for (i = 0; i < cnt; ++i) {
		sname = make_name(ut_ctx, sp, i);
		fname = make_name(ut_ctx, fp, i);
		symval = make_asymval(ut_ctx, i + 1);
		voluta_ut_create_file(ut_ctx, dino, fname, &fino);
		voluta_ut_create_symlink(ut_ctx, dino, sname, symval, &sino);

		off = (loff_t)(i * VOLUTA_MEGA + i);
		voluta_ut_write_read_str(ut_ctx, fino, symval, off);
	}
	for (i = 0; i < cnt; ++i) {
		sname = make_name(ut_ctx, sp, i);
		fname = make_name(ut_ctx, fp, i);
		symval = make_asymval(ut_ctx, i + 1);
		voluta_ut_lookup_resolve(ut_ctx, dino, sname, &sino);
		voluta_ut_readlink_expect(ut_ctx, sino, symval);

		voluta_ut_lookup_resolve(ut_ctx, dino, fname, &fino);
		off = (loff_t)(i * VOLUTA_MEGA + i);
		voluta_ut_read_verify_str(ut_ctx, fino, symval, off);
	}
	for (i = 0; i < cnt; ++i) {
		sname = make_name(ut_ctx, sp, i);
		fname = make_name(ut_ctx, fp, i);

		voluta_ut_lookup_resolve(ut_ctx, dino, fname, &fino);
		voluta_ut_release_file(ut_ctx, fino);
		voluta_ut_unlink_exists(ut_ctx, dino, sname);
		voluta_ut_unlink_exists(ut_ctx, dino, fname);
	}
	voluta_ut_remove_dir(ut_ctx, root_ino, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_symlink_and_io2(struct voluta_ut_ctx *ut_ctx)
{
	loff_t off;
	size_t i, j, cnt = 1111;
	ino_t dino, fino, sino, root_ino = VOLUTA_INO_ROOT;
	char fp[] = "f", sp1[] = "s1", sp2[] = "s2";
	char *fname, *sname, *symval;
	const char *dname = UT_NAME;

	voluta_ut_make_dir(ut_ctx, root_ino, dname, &dino);
	for (i = 0; i < cnt; ++i) {
		sname = make_name(ut_ctx, sp1, i);
		fname = make_name(ut_ctx, fp, i);
		symval = make_asymval(ut_ctx, cnt);
		voluta_ut_create_file(ut_ctx, dino, fname, &fino);
		voluta_ut_create_symlink(ut_ctx, dino, sname, symval, &sino);

		off = (loff_t)(i * cnt);
		voluta_ut_write_read_str(ut_ctx, fino, symval, off);
		voluta_ut_release_file(ut_ctx, fino);
	}
	voluta_ut_drop_caches(ut_ctx);
	for (j = cnt; j > 0; --j) {
		i = j - 1;
		sname = make_name(ut_ctx, sp1, i);
		fname = make_name(ut_ctx, fp, i);
		symval = make_asymval(ut_ctx, cnt);
		voluta_ut_lookup_resolve(ut_ctx, dino, sname, &sino);
		voluta_ut_readlink_expect(ut_ctx, sino, symval);
		voluta_ut_lookup_resolve(ut_ctx, dino, fname, &fino);

		voluta_ut_open_rdonly(ut_ctx, fino);
		off = (loff_t)(i * cnt);
		voluta_ut_read_verify_str(ut_ctx, fino, symval, off);
		voluta_ut_release_file(ut_ctx, fino);
		voluta_ut_unlink_exists(ut_ctx, dino, fname);

		sname = make_name(ut_ctx, sp2, i);
		voluta_ut_create_symlink(ut_ctx, dino, sname, symval, &sino);
	}
	voluta_ut_drop_caches(ut_ctx);
	for (i = 0; i < cnt; ++i) {
		fname = make_name(ut_ctx, fp, i);
		voluta_ut_create_only(ut_ctx, dino, fname, &fino);
		sname = make_name(ut_ctx, sp1, i);
		voluta_ut_unlink_exists(ut_ctx, dino, sname);
		voluta_ut_lookup_reg(ut_ctx, dino, fname, fino);
	}
	voluta_ut_drop_caches(ut_ctx);
	for (i = 0; i < cnt; ++i) {
		fname = make_name(ut_ctx, fp, i);
		sname = make_name(ut_ctx, sp2, i);
		voluta_ut_unlink_exists(ut_ctx, dino, sname);
		voluta_ut_unlink_exists(ut_ctx, dino, fname);
	}
	voluta_ut_remove_dir(ut_ctx, root_ino, dname);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_symlink_simple),
	UT_DEFTEST(ut_symlink_length),
	UT_DEFTEST(ut_symlink_nested),
	UT_DEFTEST(ut_symlink_to_reg),
	UT_DEFTEST(ut_symlink_and_io),
	UT_DEFTEST(ut_symlink_and_io2),
};

const struct voluta_ut_tests voluta_ut_symlink_tests = UT_MKTESTS(
			ut_local_tests);
