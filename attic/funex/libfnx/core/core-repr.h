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
#ifndef FNX_CORE_REPR_H_
#define FNX_CORE_REPR_H_


void fnx_repr_intval(fnx_substr_t *, long);

void fnx_repr_fsize(fnx_substr_t *, double);

void fnx_repr_uuid(fnx_substr_t *, const fnx_uuid_t *);

void fnx_repr_mntf(fnx_substr_t *, const fnx_mntf_t, const char *);

void fnx_repr_times(fnx_substr_t *, const fnx_times_t *, const char *);

void fnx_repr_fsattr(fnx_substr_t *, const fnx_fsattr_t *, const char *);

void fnx_repr_fsstat(fnx_substr_t *, const fnx_fsstat_t *, const char *);

void fnx_repr_iostat(fnx_substr_t *, const fnx_iostat_t *, const char *);

void fnx_repr_opstat(fnx_substr_t *, const fnx_opstat_t *, const char *);

void fnx_repr_super(fnx_substr_t *, const fnx_super_t *, const char *);

void fnx_repr_cstats(fnx_substr_t *, const fnx_cstats_t *, const char *);

void fnx_repr_iattr(fnx_substr_t *, const fnx_iattr_t *, const char *);

void fnx_repr_inode(fnx_substr_t *, const fnx_inode_t *, const char *);

void fnx_repr_dir(fnx_substr_t *, const fnx_dir_t *, const char *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_parse_param(const fnx_substr_t *, int *);

void fnx_log_repr(const fnx_substr_t *);

#endif /* FNX_CORE_REPR_H_ */
