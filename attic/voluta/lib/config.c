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

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "config-site.h"
#endif
#include <voluta/voluta.h>

#ifdef HAVE_CONFIG_H
#if !defined(VERSION)
#error "missing VERSION in config.h"
#endif
#if !defined(RELEASE)
#error "missing RELEASE in config.h"
#endif
#if !defined(REVISION)
#error "missing REVISION in config.h"
#endif
#else
#define VERSION  "0"
#define RELEASE  "0"
#define REVISION "xxxxxxxx"
#endif

#define VOLUTA_VERSION_STRING    VERSION "-" RELEASE "." REVISION

#ifndef VOLUTA_PREFIX
#define VOLUTA_PREFIX "/"
#endif

#ifndef VOLUTA_BINDIR
#define VOLUTA_BINDIR "/bin"
#endif

#ifndef VOLUTA_LIBDIR
#define VOLUTA_LIBDIR "/lib"
#endif

#ifndef VOLUTA_SYSCONFDIR
#define VOLUTA_SYSCONFDIR "/etc"
#endif

#ifndef VOLUTA_DATAROOTDIR
#define VOLUTA_DATAROOTDIR "/usr/share"
#endif

#ifndef VOLUTA_LOCALSTATEDIR
#define VOLUTA_LOCALSTATEDIR "/var"
#endif


const struct voluta_config voluta_g_config = {
	.version = VOLUTA_VERSION_STRING,
	.prefix = VOLUTA_PREFIX,
	.bindir = VOLUTA_BINDIR,
	.libdir = VOLUTA_LIBDIR,
	.sysconfdir = VOLUTA_SYSCONFDIR,
	.datadir = VOLUTA_DATAROOTDIR,
	.statedir = VOLUTA_LOCALSTATEDIR
};



