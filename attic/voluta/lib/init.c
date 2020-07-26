/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * This file is part of libvoluta
 *
 * Copyright (C) 2020 Shachar Sharon
 *
 * Libvoluta is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Libvoluta is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 */
#define _GNU_SOURCE 1
#include <stdlib.h>
#include <string.h>
#include "libvoluta.h"

static int g_libvoluta_init;

static int errno_or_errnum(int errnum)
{
	return (errno > 0) ? -errno : -abs(errnum);
}

static int check_sysconf(void)
{
	long val;
	const long page_size_min = VOLUTA_PAGE_SIZE;
	const long cl_size_min = VOLUTA_CACHELINE_SIZE;

	errno = 0;
	val = (long)voluta_sc_l1_dcache_linesize();
	if ((val != cl_size_min) || (val % cl_size_min)) {
		return errno_or_errnum(ENOTSUP);
	}
	val = (long)voluta_sc_page_size();
	if ((val < page_size_min) || (val % page_size_min)) {
		return errno_or_errnum(ENOTSUP);
	}
	val = (long)voluta_sc_phys_pages();
	if (val <= 0) {
		return errno_or_errnum(ENOMEM);
	}
	val = (long)voluta_sc_avphys_pages();
	if (val <= 0) {
		return errno_or_errnum(ENOMEM);
	}
	return 0;
}

static int check_system_page_size(void)
{
	size_t page_size;
	const size_t page_shift[] = { 12, 13, 14, 16 };

	page_size = voluta_sc_page_size();
	if (page_size > VOLUTA_SEG_SIZE) {
		return -ENOTSUP;
	}
	for (size_t i = 0; i < ARRAY_SIZE(page_shift); ++i) {
		if (page_size == (1UL << page_shift[i])) {
			return 0;
		}
	}
	return -ENOTSUP;
}

int voluta_lib_init(void)
{
	int err;

	voluta_verify_persistent_format();

	if (g_libvoluta_init) {
		return 0;
	}
	err = check_sysconf();
	if (err) {
		return err;
	}
	err = check_system_page_size();
	if (err) {
		return err;
	}
	err = voluta_init_gcrypt();
	if (err) {
		return err;
	}
	voluta_burnstack();
	g_libvoluta_init = 1;

	return 0;
}



