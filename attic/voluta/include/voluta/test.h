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
#ifndef VOLUTA_TEST_H_
#define VOLUTA_TEST_H_

#include "voluta.h"

#ifdef VOLUTA_TEST

#define T_NAME          __func__

#define ROOT_INO        VOLUTA_INO_ROOT

#define KILO            VOLUTA_KILO
#define MEGA            VOLUTA_MEGA
#define GIGA            VOLUTA_GIGA
#define TERA            VOLUTA_TERA
#define PETA            VOLUTA_PETA

#define UKILO           VOLUTA_UKILO
#define UMEGA           VOLUTA_UMEGA
#define UGIGA           VOLUTA_UGIGA
#define UTERA           VOLUTA_UTERA
#define UPETA           VOLUTA_UPETA

#define BK_SIZE         VOLUTA_BK_SIZE
#define DS_SIZE         VOLUTA_DS_SIZE
#define FILEMAP_NCHILD  VOLUTA_FILE_MAP_NCHILD
#define FILESIZE_MAX    VOLUTA_FILE_SIZE_MAX
#define IOSIZE_MAX      VOLUTA_IO_SIZE_MAX

#define ARRAY_SIZE(x_)  VOLUTA_ARRAY_SIZE(x_)


struct voluta_t_ioparams {
	loff_t offset;
	size_t length;
	size_t nskip;
	size_t count;
};

#endif /* VOLUTA_TEST */

#endif /* VOLUTA_TEST_H_ */
