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
#ifndef VOLUTA_LOGGING_H_
#define VOLUTA_LOGGING_H_


enum VOLUTA_LOG_LEVEL {
	VOLUTA_LOG_DEBUG  = 0x0001,
	VOLUTA_LOG_INFO   = 0x0002,
	VOLUTA_LOG_WARN   = 0x0004,
	VOLUTA_LOG_ERROR  = 0x0008,
	VOLUTA_LOG_CRIT   = 0x0010,
	VOLUTA_LOG_STDOUT = 0x1000,
	VOLUTA_LOG_SYSLOG = 0x2000,
};

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


#endif /* VOLUTA_LOGGING_H_ */
