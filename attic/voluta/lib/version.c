/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * This file is part of libvoluta
 *
 * Copyright (C) 2019 Shachar Sharon
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

/* Exported version string */
const char voluta_version_string[] = VOLUTA_VERSION_STRING;
