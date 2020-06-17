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
#include <linux/fs.h>
#include <linux/fiemap.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include "fsstest.h"


static struct fiemap *new_fiemap(struct voluta_t_ctx *t_ctx, size_t cnt)
{
	size_t sz;
	struct fiemap *fm = NULL;

	sz = sizeof(*fm) + (cnt * sizeof(fm->fm_extents[0]));
	fm = voluta_t_new_buf_zeros(t_ctx, sz);
	fm->fm_extent_count = (uint32_t)cnt;

	return fm;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_fiemap_simple_(struct voluta_t_ctx *t_ctx,
				loff_t off, size_t bsz)
{
	int fd;
	void *buf = voluta_t_new_buf_rands(t_ctx, bsz);
	const char *path = voluta_t_new_path_unique(t_ctx);
	struct fiemap *fm = NULL;

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_pwriten(fd, buf, bsz, off);

	fm = new_fiemap(t_ctx, 2);
	fm->fm_start = (uint64_t)off;
	fm->fm_length = bsz;
	fm->fm_flags = FIEMAP_FLAG_SYNC;
	voluta_t_fiemap(fd, fm);

	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_fiemap_simple(struct voluta_t_ctx *t_ctx)
{
	test_fiemap_simple_(t_ctx, 0, 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTESTF(test_fiemap_simple, VOLUTA_T_IGNORE),
};

const struct voluta_t_tests voluta_t_test_fiemap =
	VOLUTA_T_DEFTESTS(t_local_tests);
