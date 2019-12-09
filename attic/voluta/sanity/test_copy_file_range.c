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
#include <stdlib.h>
#include <string.h>
#include "sanity.h"


struct voluta_t_copy_range_info {
	loff_t src_fsize;
	size_t src_datasz;
	loff_t src_doff;
	loff_t dst_fsize;
	size_t dst_datasz;
	loff_t dst_doff;
	size_t copysz;
};

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects copy_file_range(2) to successfully reflink-copy partial file range
 * between two files.
 */
static void test_copy_file_range_(struct voluta_t_ctx *t_ctx,
				  const struct voluta_t_copy_range_info *cri)

{
	int src_fd, dst_fd;
	size_t nb;
	loff_t src_off, dst_off, cnt;
	char *src_path, *dst_path;
	void *src_data, *dst_data;

	src_path = voluta_t_new_path_unique(t_ctx);
	voluta_t_open(src_path, O_CREAT | O_RDWR, 0600, &src_fd);
	voluta_t_ftruncate(src_fd, cri->src_fsize);

	dst_path = voluta_t_new_path_unique(t_ctx);
	voluta_t_open(dst_path, O_CREAT | O_RDWR, 0600, &dst_fd);
	voluta_t_ftruncate(dst_fd, cri->dst_fsize);

	if (cri->src_datasz > 0) {
		src_data = voluta_t_new_buf_rands(t_ctx, cri->src_datasz);
		voluta_t_pwrite(src_fd, src_data,
				cri->src_datasz, cri->src_doff, &nb);
		voluta_t_expect_eq(cri->src_datasz, nb);
	}

	if (cri->dst_datasz > 0) {
		dst_data = voluta_t_new_buf_rands(t_ctx, cri->dst_datasz);
		voluta_t_pwrite(dst_fd, dst_data,
				cri->dst_datasz, cri->dst_doff, &nb);
		voluta_t_expect_eq(cri->dst_datasz, nb);
	}

	src_off = cri->src_doff;
	dst_off = cri->dst_doff;
	voluta_t_copy_file_range(src_fd, &src_off, dst_fd, &dst_off,
				 cri->copysz, 0, &cnt);
	voluta_t_expect_eq(cnt, (loff_t)cri->copysz);

	src_data = voluta_t_new_buf_rands(t_ctx, cri->copysz);
	voluta_t_pread(src_fd, src_data, cri->copysz, cri->src_doff, &nb);
	voluta_t_expect_eq(cri->copysz, nb);

	dst_data = voluta_t_new_buf_rands(t_ctx, cri->copysz);
	voluta_t_pread(dst_fd, dst_data, cri->copysz, cri->dst_doff, &nb);
	voluta_t_expect_eq(cri->copysz, nb);
	voluta_t_expect_eqm(src_data, dst_data, cri->copysz);

	voluta_t_close(src_fd);
	voluta_t_close(dst_fd);
	voluta_t_unlink(src_path);
	voluta_t_unlink(dst_path);
}

static void test_copy_file_range_simple1(struct voluta_t_ctx *t_ctx)
{
	const struct voluta_t_copy_range_info cri = {
		.src_fsize = VOLUTA_MEGA,
		.src_datasz = VOLUTA_MEGA,
		.src_doff = 0,
		.dst_fsize = VOLUTA_MEGA,
		.dst_datasz = VOLUTA_BK_SIZE,
		.dst_doff = 0,
		.copysz = VOLUTA_BK_SIZE
	};
	test_copy_file_range_(t_ctx, &cri);
}

static void test_copy_file_range_simple2(struct voluta_t_ctx *t_ctx)
{
	const struct voluta_t_copy_range_info cri = {
		.src_fsize = VOLUTA_MEGA,
		.src_datasz = VOLUTA_MEGA,
		.src_doff = 0,
		.dst_fsize = VOLUTA_MEGA,
		.dst_datasz = VOLUTA_MEGA,
		.dst_doff = 0,
		.copysz = VOLUTA_MEGA
	};
	test_copy_file_range_(t_ctx, &cri);
}

static void test_copy_file_range_simple3(struct voluta_t_ctx *t_ctx)
{
	const struct voluta_t_copy_range_info cri = {
		.src_fsize = VOLUTA_MEGA,
		.src_datasz = VOLUTA_MEGA,
		.src_doff = 0,
		.dst_fsize = 2 * VOLUTA_MEGA,
		.dst_datasz = VOLUTA_MEGA,
		.dst_doff = VOLUTA_MEGA,
		.copysz = VOLUTA_MEGA
	};
	test_copy_file_range_(t_ctx, &cri);
}

static void test_copy_file_range_nosrcdata1(struct voluta_t_ctx *t_ctx)
{
	const struct voluta_t_copy_range_info cri = {
		.src_fsize = VOLUTA_MEGA,
		.src_datasz = 0,
		.src_doff = 0,
		.dst_fsize = VOLUTA_MEGA,
		.dst_datasz = VOLUTA_BK_SIZE,
		.dst_doff = 0,
		.copysz = 64 * VOLUTA_BK_SIZE
	};
	test_copy_file_range_(t_ctx, &cri);
}

static void test_copy_file_range_nosrcdata2(struct voluta_t_ctx *t_ctx)
{
	const struct voluta_t_copy_range_info cri = {
		.src_fsize = VOLUTA_MEGA,
		.src_datasz = 0,
		.src_doff = 8 * VOLUTA_BK_SIZE,
		.dst_fsize = VOLUTA_MEGA,
		.dst_datasz = VOLUTA_MEGA,
		.dst_doff = 7 * VOLUTA_BK_SIZE,
		.copysz = 11 * VOLUTA_BK_SIZE
	};
	test_copy_file_range_(t_ctx, &cri);
}

static void test_copy_file_range_nodstdata1(struct voluta_t_ctx *t_ctx)
{
	const struct voluta_t_copy_range_info cpri = {
		.src_fsize = VOLUTA_MEGA,
		.src_datasz = VOLUTA_MEGA,
		.src_doff = 0,
		.dst_fsize = VOLUTA_MEGA,
		.dst_datasz = 0,
		.dst_doff = 0,
		.copysz = 64 * VOLUTA_BK_SIZE
	};
	test_copy_file_range_(t_ctx, &cpri);
}

static void test_copy_file_range_nodstdata2(struct voluta_t_ctx *t_ctx)
{
	const struct voluta_t_copy_range_info cpri = {
		.src_fsize = VOLUTA_MEGA,
		.src_datasz = VOLUTA_MEGA,
		.src_doff = 17 * VOLUTA_BK_SIZE,
		.dst_fsize = 2 * VOLUTA_MEGA,
		.dst_datasz = 0,
		.dst_doff = VOLUTA_MEGA,
		.copysz = 64 * VOLUTA_BK_SIZE
	};
	test_copy_file_range_(t_ctx, &cpri);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTESTF(test_copy_file_range_simple1, VOLUTA_T_IO_CLONE),
	VOLUTA_T_DEFTESTF(test_copy_file_range_simple2, VOLUTA_T_IO_CLONE),
	VOLUTA_T_DEFTESTF(test_copy_file_range_simple3, VOLUTA_T_IO_CLONE),
	VOLUTA_T_DEFTESTF(test_copy_file_range_nosrcdata1, VOLUTA_T_IO_CLONE),
	VOLUTA_T_DEFTESTF(test_copy_file_range_nosrcdata2, VOLUTA_T_IO_CLONE),
	VOLUTA_T_DEFTESTF(test_copy_file_range_nodstdata1, VOLUTA_T_IO_CLONE),
	VOLUTA_T_DEFTESTF(test_copy_file_range_nodstdata2, VOLUTA_T_IO_CLONE),
};

const struct voluta_t_tests
voluta_t_test_copy_file_range = VOLUTA_T_DEFTESTS(t_local_tests);
