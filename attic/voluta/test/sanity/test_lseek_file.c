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
#include <stdio.h>
#include "sanity.h"


/*
 * Tests lseek(2) with SEEK_DATA
 */
static void test_lseek_data(struct voluta_t_ctx *t_ctx)
{
	int fd;
	loff_t off, data_off, pos, ssize;
	size_t i, nwr, step, size, nsteps = 256;
	char *path, *buf1;

	size = BK_SIZE;
	ssize = (loff_t)size;
	step = UMEGA;
	path = voluta_t_new_path_unique(t_ctx);
	buf1 = voluta_t_new_buf_rands(t_ctx, size);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	for (i = 0; i < nsteps; ++i) {
		off = (loff_t)(step * (i + 1));
		data_off = off - ssize;
		voluta_t_ftruncate(fd, off);
		voluta_t_pwrite(fd, buf1, size, data_off, &nwr);
		voluta_t_expect_eq(nwr, size);
	}

	voluta_t_llseek(fd, 0, SEEK_SET, &pos);
	voluta_t_expect_eq(pos, 0);
	for (i = 0; i < nsteps; ++i) {
		off = (loff_t)(step * i);
		data_off = (loff_t)(step * (i + 1)) - ssize;
		voluta_t_llseek(fd, off, SEEK_DATA, &pos);
		voluta_t_expect_eq(pos, data_off);
	}
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*
 * Tests lseek(2) with SEEK_HOLE
 */
static void test_lseek_hole(struct voluta_t_ctx *t_ctx)
{
	int fd;
	loff_t off, hole_off, pos, ssize;
	size_t i, nwr, step, size, nsteps = 256;
	char *path, *buf1;

	size = BK_SIZE;
	ssize = (loff_t)size;
	step = UMEGA;
	path = voluta_t_new_path_unique(t_ctx);
	buf1 = voluta_t_new_buf_rands(t_ctx, size);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	for (i = 0; i < nsteps; ++i) {
		off = (loff_t)(step * i);
		voluta_t_pwrite(fd, buf1, size, off, &nwr);
		voluta_t_expect_eq(nwr, size);
	}

	voluta_t_llseek(fd, 0, SEEK_SET, &pos);
	voluta_t_expect_eq(pos, 0);
	for (i = 0; i < nsteps - 1; ++i) {
		off = (loff_t)(step * i);
		hole_off = off + ssize;
		voluta_t_llseek(fd, off, SEEK_HOLE, &pos);
		voluta_t_expect_eq(pos, hole_off);
	}

	voluta_t_llseek(fd, 0, SEEK_SET, &pos);
	voluta_t_expect_eq(pos, 0);
	for (i = 0; i < nsteps - 1; ++i) {
		off = (loff_t)(step * i) + ssize + 1;
		hole_off = off;
		voluta_t_llseek(fd, off, SEEK_HOLE, &pos);
		voluta_t_expect_eq(pos, hole_off);
	}

	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTESTF(test_lseek_data, VOLUTA_T_IO_EXTRA),
	VOLUTA_T_DEFTESTF(test_lseek_hole, VOLUTA_T_IO_EXTRA),
};

const struct voluta_t_tests
voluta_t_test_seek = VOLUTA_T_DEFTESTS(t_local_tests);
