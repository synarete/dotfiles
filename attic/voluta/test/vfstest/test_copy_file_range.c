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
#include <stdlib.h>
#include <string.h>
#include "vfstest.h"


struct vt_copy_range_info {
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
static void test_copy_file_range_(struct vt_env *vt_env,
				  const struct vt_copy_range_info *cri)

{
	int src_fd, dst_fd;
	size_t nb;
	loff_t src_off, dst_off, cnt;
	char *src_path, *dst_path;
	void *src_data, *dst_data;

	src_path = vt_new_path_unique(vt_env);
	vt_open(src_path, O_CREAT | O_RDWR, 0600, &src_fd);
	vt_ftruncate(src_fd, cri->src_fsize);

	dst_path = vt_new_path_unique(vt_env);
	vt_open(dst_path, O_CREAT | O_RDWR, 0600, &dst_fd);
	vt_ftruncate(dst_fd, cri->dst_fsize);

	if (cri->src_datasz > 0) {
		src_data = vt_new_buf_rands(vt_env, cri->src_datasz);
		vt_pwrite(src_fd, src_data,
			  cri->src_datasz, cri->src_doff, &nb);
		vt_expect_eq(cri->src_datasz, nb);
	}

	if (cri->dst_datasz > 0) {
		dst_data = vt_new_buf_rands(vt_env, cri->dst_datasz);
		vt_pwrite(dst_fd, dst_data,
			  cri->dst_datasz, cri->dst_doff, &nb);
		vt_expect_eq(cri->dst_datasz, nb);
	}

	src_off = cri->src_doff;
	dst_off = cri->dst_doff;
	vt_copy_file_range(src_fd, &src_off, dst_fd, &dst_off,
			   cri->copysz, 0, &cnt);
	vt_expect_eq(cnt, (loff_t)cri->copysz);

	src_data = vt_new_buf_rands(vt_env, cri->copysz);
	vt_pread(src_fd, src_data, cri->copysz, cri->src_doff, &nb);
	vt_expect_eq(cri->copysz, nb);

	dst_data = vt_new_buf_rands(vt_env, cri->copysz);
	vt_pread(dst_fd, dst_data, cri->copysz, cri->dst_doff, &nb);
	vt_expect_eq(cri->copysz, nb);
	vt_expect_eqm(src_data, dst_data, cri->copysz);

	vt_close(src_fd);
	vt_close(dst_fd);
	vt_unlink(src_path);
	vt_unlink(dst_path);
}

static void test_copy_file_range_simple1(struct vt_env *vt_env)
{
	const struct vt_copy_range_info cri = {
		.src_fsize = VT_UMEGA,
		.src_datasz = VT_UMEGA,
		.src_doff = 0,
		.dst_fsize = VT_UMEGA,
		.dst_datasz = VT_BK_SIZE,
		.dst_doff = 0,
		.copysz = VT_BK_SIZE
	};
	test_copy_file_range_(vt_env, &cri);
}

static void test_copy_file_range_simple2(struct vt_env *vt_env)
{
	const struct vt_copy_range_info cri = {
		.src_fsize = VT_UMEGA,
		.src_datasz = VT_UMEGA,
		.src_doff = 0,
		.dst_fsize = VT_UMEGA,
		.dst_datasz = VT_UMEGA,
		.dst_doff = 0,
		.copysz = VT_UMEGA
	};
	test_copy_file_range_(vt_env, &cri);
}

static void test_copy_file_range_simple3(struct vt_env *vt_env)
{
	const struct vt_copy_range_info cri = {
		.src_fsize = VT_UMEGA,
		.src_datasz = VT_UMEGA,
		.src_doff = 0,
		.dst_fsize = 2 * VT_UMEGA,
		.dst_datasz = VT_UMEGA,
		.dst_doff = VT_UMEGA,
		.copysz = VT_UMEGA
	};
	test_copy_file_range_(vt_env, &cri);
}

static void test_copy_file_range_nosrcdata1(struct vt_env *vt_env)
{
	const struct vt_copy_range_info cri = {
		.src_fsize = VT_UMEGA,
		.src_datasz = 0,
		.src_doff = 0,
		.dst_fsize = VT_UMEGA,
		.dst_datasz = VT_BK_SIZE,
		.dst_doff = 0,
		.copysz = 64 * VT_BK_SIZE
	};
	test_copy_file_range_(vt_env, &cri);
}

static void test_copy_file_range_nosrcdata2(struct vt_env *vt_env)
{
	const struct vt_copy_range_info cri = {
		.src_fsize = VT_UMEGA,
		.src_datasz = 0,
		.src_doff = 8 * VT_BK_SIZE,
		.dst_fsize = VT_UMEGA,
		.dst_datasz = VT_UMEGA,
		.dst_doff = 7 * VT_BK_SIZE,
		.copysz = 11 * VT_BK_SIZE
	};
	test_copy_file_range_(vt_env, &cri);
}

static void test_copy_file_range_nodstdata1(struct vt_env *vt_env)
{
	const struct vt_copy_range_info cpri = {
		.src_fsize = VT_UMEGA,
		.src_datasz = VT_UMEGA,
		.src_doff = 0,
		.dst_fsize = VT_UMEGA,
		.dst_datasz = 0,
		.dst_doff = 0,
		.copysz = 64 * VT_BK_SIZE
	};
	test_copy_file_range_(vt_env, &cpri);
}

static void test_copy_file_range_nodstdata2(struct vt_env *vt_env)
{
	const struct vt_copy_range_info cpri = {
		.src_fsize = VT_UMEGA,
		.src_datasz = VT_UMEGA,
		.src_doff = 17 * VT_BK_SIZE,
		.dst_fsize = 2 * VT_UMEGA,
		.dst_datasz = 0,
		.dst_doff = VT_UMEGA,
		.copysz = 64 * VT_BK_SIZE
	};
	test_copy_file_range_(vt_env, &cpri);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct vt_tdef vt_local_tests[] = {
	VT_DEFTESTF(test_copy_file_range_simple1, VT_IGNORE),
	VT_DEFTESTF(test_copy_file_range_simple2, VT_IGNORE),
	VT_DEFTESTF(test_copy_file_range_simple3, VT_IGNORE),
	VT_DEFTESTF(test_copy_file_range_nosrcdata1, VT_IGNORE),
	VT_DEFTESTF(test_copy_file_range_nosrcdata2, VT_IGNORE),
	VT_DEFTESTF(test_copy_file_range_nodstdata1, VT_IGNORE),
	VT_DEFTESTF(test_copy_file_range_nodstdata2, VT_IGNORE),
};

const struct vt_tests
vt_test_copy_file_range = VT_DEFTESTS(vt_local_tests);
