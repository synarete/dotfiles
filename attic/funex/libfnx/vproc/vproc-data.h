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
#ifndef FNX_SERV_VPROC_DATA_H_
#define FNX_SERV_VPROC_DATA_H_


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Data-path execution */

int fnx_vproc_exec_forget(fnx_vproc_t *, fnx_task_t *, fnx_ino_t);

int fnx_vproc_exec_release(fnx_vproc_t *, fnx_task_t *);

int fnx_vproc_exec_open(fnx_vproc_t *, fnx_task_t *, fnx_flags_t);

int fnx_vproc_exec_read(fnx_vproc_t *, fnx_task_t *, fnx_off_t, fnx_size_t);

int fnx_vproc_exec_write(fnx_vproc_t *, fnx_task_t *, fnx_off_t, fnx_size_t);

int fnx_vproc_exec_punch(fnx_vproc_t *, fnx_task_t *, fnx_off_t, fnx_size_t);

int fnx_vproc_exec_trunc(fnx_vproc_t *, fnx_task_t *, fnx_off_t);

int fnx_vproc_exec_fsync(fnx_vproc_t *, fnx_task_t *);

int fnx_vproc_exec_fsyncdir(fnx_vproc_t *, fnx_task_t *);

int fnx_vproc_exec_falloc(fnx_vproc_t *, fnx_task_t *,
                          fnx_off_t, fnx_size_t, fnx_bool_t);


void fnx_vproc_relax_iobufs(fnx_vproc_t *, fnx_task_t *);

int fnx_vproc_trunc_data(fnx_vproc_t *, fnx_reg_t *, fnx_off_t);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Pseudo-Namespace */

int fnx_vproc_write_pseudo(fnx_vproc_t *, fnx_task_t *, fnx_off_t, fnx_size_t);

int fnx_vproc_read_pseudo(fnx_vproc_t *, fnx_task_t *, fnx_off_t, fnx_size_t);

int fnx_vproc_trunc_pseudo(fnx_vproc_t *, fnx_task_t *, fnx_off_t);

int fnx_vproc_punch_pseudo(fnx_vproc_t *, fnx_task_t *, fnx_off_t, fnx_size_t);



#endif /* FNX_SERV_VPROC_DATA_H_ */
