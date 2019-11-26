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
#ifndef FUNEX_PARSE_H_
#define FUNEX_PARSE_H_

int funex_parse_bool(const char *, int *);

int funex_parse_loff(const char *, loff_t *);

int funex_parse_int(const char *, int *);

int funex_parse_long(const char *, long *);

int funex_parse_uid(const char *, fnx_uid_t *);

int funex_parse_gid(const char *, fnx_gid_t *);

int funex_parse_uuid(const char *, fnx_uuid_t *);

int funex_parse_mntops(const char *, fnx_mntf_t *);


int funex_parse_kv(const char *, char *, size_t, char *, size_t);


#endif /* FUNEX_PARSE_H_ */


