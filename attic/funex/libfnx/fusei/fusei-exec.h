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
#ifndef FNX_FUSEI_EXEC_H_
#define FNX_FUSEI_EXEC_H_

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

extern const struct fuse_lowlevel_ops fnx_fusei_ll_ops;

void fnx_fusei_init(struct fnx_fusei *);

void fnx_fusei_destroy(struct fnx_fusei *);

int fnx_fusei_setup(struct fnx_fusei *, fnx_mntf_t);

int fnx_fusei_open_mntpoint(struct fnx_fusei *, int, const char *);

int fnx_fusei_close(struct fnx_fusei *);

void fnx_fusei_term(struct fnx_fusei *);

void fnx_fusei_clean(struct fnx_fusei *);


void fnx_fusei_process_rx(struct fnx_fusei *);

void fnx_fusei_execute_tx(struct fnx_fusei *, struct fnx_task *);

void fnx_fusei_del_task(struct fnx_fusei *, struct fnx_task *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/


void fnx_user_update(fnx_user_t *, fnx_pid_t, fnx_fuse_req_t);

void fnx_user_setctx(const fnx_user_t *, fnx_uctx_t *);


void fnx_usersdb_init(fnx_usersdb_t *);

void fnx_usersdb_destroy(fnx_usersdb_t *);

fnx_user_t *fnx_usersdb_lookup(fnx_usersdb_t *, fnx_uid_t);

fnx_user_t *fnx_usersdb_insert(fnx_usersdb_t *, fnx_uid_t);


#endif /* FNX_FUSEI_EXEC_H_ */
