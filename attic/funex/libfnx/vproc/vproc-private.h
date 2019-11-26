/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                             *
 *                            Funex File-System                                *
 *                                                                             *
 *  Copyright (C) 2014,2015 Synarete                                           *
 *                                                                             *
 *  Funex is a free software; you can redistribute it and/or modify it under   *
 *  the terms of the GNU General Public License as published by the Free       *
 *  Software Foundation, either version 3 of the License, or (at your option)  *
 *  any later version.                                                         *
 *                                                                             *
 *  Funex is distributed in the hope that it will be useful, but WITHOUT ANY   *
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS  *
 *  FOR A PARTICULAR PURPOSE. See the GNU General Public License for more      *
 *  details.                                                                   *
 *                                                                             *
 *  You should have received a copy of the GNU General Public License along    *
 *  with this program. If not, see <http://www.gnu.org/licenses/>              *
 *                                                                             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
#ifndef FNX_VPROC_PRIVATE_H_
#define FNX_VPROC_PRIVATE_H_

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

#define vproc_debug(vproc, fmt, ...) \
	fnx_log_trace(fnx_vproc_get_logger(vproc), "vproc: " fmt, __VA_ARGS__)

#define vproc_info(vproc, fmt, ...) \
	fnx_log_info(fnx_vproc_get_logger(vproc), "vproc: " fmt, __VA_ARGS__)

#define vproc_warn(vproc, fmt, ...) \
	fnx_log_warn(fnx_vproc_get_logger(vproc), "vproc: " fmt, __VA_ARGS__)

#define vproc_error(vproc, fmt, ...) \
	fnx_log_error(fnx_vproc_get_logger(vproc), "vproc: " fmt, __VA_ARGS__)

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_logger_t *fnx_vproc_get_logger(const fnx_vproc_t *);


void fnx_frpool_init(fnx_frpool_t *);

void fnx_frpool_destroy(fnx_frpool_t *);

size_t fnx_frpool_getnused(const fnx_frpool_t *);


int fnx_vproc_hasfree_fileref(const fnx_vproc_t *, int);

fnx_fileref_t *fnx_vproc_tie_fileref(fnx_vproc_t *, fnx_inode_t *, fnx_flags_t);

fnx_inode_t *fnx_vproc_untie_fileref(fnx_vproc_t *, fnx_fileref_t *);

void fnx_vproc_clear_filerefs(fnx_vproc_t *);

#endif /* FNX_VPROC_PRIVATE_H_ */


