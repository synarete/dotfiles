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
#ifndef FNX_PSTOR_PRIVATE_H_
#define FNX_PSTOR_PRIVATE_H_


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

#define pstor_debug(pstor, fmt, ...) \
	fnx_log_trace(fnx_pstor_get_logger(pstor), "pstor: " fmt, __VA_ARGS__)

#define pstor_info(pstor, fmt, ...) \
	fnx_log_info(fnx_pstor_get_logger(pstor), "pstor: " fmt, __VA_ARGS__)

#define pstor_warn(pstor, fmt, ...) \
	fnx_log_warn(fnx_pstor_get_logger(pstor), "pstor: " fmt, __VA_ARGS__)

#define pstor_error(pstor, fmt, ...) \
	fnx_log_error(fnx_pstor_get_logger(pstor), "pstor: " fmt, __VA_ARGS__)

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_logger_t *fnx_pstor_get_logger(const fnx_pstor_t *);

int fnx_pstor_read_bk(const fnx_pstor_t *, const fnx_bkref_t *);

int fnx_pstor_write_bk(const fnx_pstor_t *, const fnx_bkref_t *);

int fnx_pstor_sync_bk(const fnx_pstor_t *, const fnx_bkref_t *);



int fnx_pstor_new_bk(const fnx_pstor_t *,
                     const fnx_baddr_t *, fnx_bkref_t **);

int fnx_pstor_new_bk2(const fnx_pstor_t *,
                      const fnx_baddr_t *, fnx_spmap_t *, fnx_bkref_t **);

void fnx_pstor_del_bk(const fnx_pstor_t *, fnx_bkref_t *);

void fnx_pstor_del_bk2(const fnx_pstor_t *, fnx_bkref_t *);


int fnx_pstor_new_vobj(const fnx_pstor_t *,
                       const fnx_vaddr_t *, fnx_vnode_t **);

void fnx_pstor_del_vobj(const fnx_pstor_t *, fnx_vnode_t *);


int fnx_pstor_decode_vobj(const fnx_pstor_t *, fnx_vnode_t *,
                          const fnx_bkref_t *, const fnx_baddr_t *);


#endif /* FNX_PSTOR_PRIVATE_H_ */


