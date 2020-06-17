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
#ifndef VOLUTA_UTIL_H_
#define VOLUTA_UTIL_H_

#include <stddef.h>


/* stringify macros */
#define VOLUTA_STR(x_)          VOLUTA_MAKESTR_(x_)
#define VOLUTA_MAKESTR_(x_)     #x_
#define VOLUTA_CONCAT(x_, y_)   x_ ## y_

/* array number of elements */
#define VOLUTA_ARRAY_SIZE(x_)   ( (sizeof((x_))) / (sizeof((x_)[0])) )

/* utility macros */
#define VOLUTA_CONTAINER_OF(ptr, type, member) \
	(type *)((void *)((char *)ptr - offsetof(type, member)))

#define voluta_container_of(ptr, type, member) \
	VOLUTA_CONTAINER_OF(ptr, type, member)

#define voluta_unused(x_)       ((void)x_)

/* logging */
#define VOLUTA_LOG_DEBUG        (0x0001)
#define VOLUTA_LOG_INFO         (0x0002)
#define VOLUTA_LOG_WARN         (0x0004)
#define VOLUTA_LOG_ERROR        (0x0008)
#define VOLUTA_LOG_CRIT         (0x0010)
#define VOLUTA_LOG_STDOUT       (0x1000)
#define VOLUTA_LOG_SYSLOG       (0x2000)

#define voluta_log_debug(fmt, ...) \
	voluta_logf(VOLUTA_LOG_DEBUG, fmt, __VA_ARGS__)

#define voluta_log_info(fmt, ...) \
	voluta_logf(VOLUTA_LOG_INFO, fmt, __VA_ARGS__)

#define voluta_log_warn(fmt, ...) \
	voluta_logf(VOLUTA_LOG_WARN, fmt, __VA_ARGS__)

#define voluta_log_error(fmt, ...) \
	voluta_logf(VOLUTA_LOG_ERROR, fmt, __VA_ARGS__)

#define voluta_log_crit(fmt, ...) \
	voluta_logf(VOLUTA_LOG_CRIT, fmt, __VA_ARGS__)


extern int voluta_g_log_mask;

void voluta_logf(int log_mask, const char *fmt, ...);

#endif /* VOLUTA_UTIL_H_ */
