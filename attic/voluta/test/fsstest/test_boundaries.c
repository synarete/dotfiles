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
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "fsstest.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects read-write data-consistency when I/O at block boundaries
 */
static void test_boundaries_(struct voluta_t_ctx *t_ctx, loff_t boff)
{
	int fd;
	uint8_t byte;
	uint64_t val1, val2;
	loff_t off_beg, off_end, off;
	const long vsz = (long)sizeof(val1);
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);

	off_beg = boff - vsz - 1;
	off_end = boff + vsz + 1;
	for (off = off_beg; off < off_end; ++off) {
		if (off < 1) {
			continue;
		}
		if ((off + vsz) > FILESIZE_MAX) {
			break;
		}
		byte = (uint8_t)off;
		val1 = (uint64_t)off;
		voluta_t_pwriten(fd, &val1, sizeof(val1), off);
		voluta_t_preadn(fd, &val2, sizeof(val2), off);
		voluta_t_expect_eq(val1, val2);
		voluta_t_preadn(fd, &byte, sizeof(byte), off - 1);
		voluta_t_expect_eq(byte, 0);
		voluta_t_ftruncate(fd, off);
	}
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_boundaries_arr_(struct voluta_t_ctx *t_ctx,
				 const loff_t *arr, size_t cnt)
{
	for (size_t i = 0; i < cnt; ++i) {
		test_boundaries_(t_ctx, arr[i]);
	}
}

static void test_boundaries_write_read(struct voluta_t_ctx *t_ctx)
{
	const loff_t offs[] = {
		0, KILO, BK_SIZE, MEGA, 2 * MEGA + 1, GIGA, 7 * GIGA - 7,
		TERA, TERA / 2 - 1, FILESIZE_MAX / 2, FILESIZE_MAX / 2 + 1,
		FILESIZE_MAX
	};

	test_boundaries_arr_(t_ctx, offs, ARRAY_SIZE(offs));
}

static void test_boundaries_tree_levels(struct voluta_t_ctx *t_ctx)
{
	const loff_t offs[] = {
		DS_SIZE,
		DS_SIZE * FILEMAP_NCHILD,
		DS_SIZE *FILEMAP_NCHILD * FILEMAP_NCHILD,
		DS_SIZE *FILEMAP_NCHILD *FILEMAP_NCHILD *FILEMAP_NCHILD
	};

	test_boundaries_arr_(t_ctx, offs, ARRAY_SIZE(offs));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_boundaries_write_read),
	VOLUTA_T_DEFTEST(test_boundaries_tree_levels),
};

const struct voluta_t_tests voluta_t_test_boundaries =
	VOLUTA_T_DEFTESTS(t_local_tests);
