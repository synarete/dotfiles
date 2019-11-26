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
#ifndef FNX_CORE_SUPER_H_
#define FNX_CORE_SUPER_H_


fnx_dsuper_t *fnx_header_to_dsuper(const fnx_header_t *);

fnx_super_t *fnx_vnode_to_super(const fnx_vnode_t *);

void fnx_super_init(fnx_super_t *);

void fnx_super_destroy(fnx_super_t *);

void fnx_super_clone(const fnx_super_t *, fnx_super_t *);

void fnx_super_setup(fnx_super_t *, const char *, const char *);

void fnx_super_settimes(fnx_super_t *, const fnx_times_t *, fnx_flags_t);

void fnx_super_htod(const fnx_super_t *, fnx_dsuper_t *);

void fnx_super_dtoh(fnx_super_t *, const fnx_dsuper_t *);

int fnx_super_dcheck(const fnx_header_t *);

void fnx_super_getfsinfo(const fnx_super_t *, fnx_fsinfo_t *);

void fnx_super_fillnewi(const fnx_super_t *, fnx_inode_t *);

int fnx_super_hasnexti(const fnx_super_t *, int);

int fnx_super_hasfreeb(const fnx_super_t *, fnx_bkcnt_t, int);

fnx_ino_t fnx_super_getino(const fnx_super_t *);

fnx_size_t fnx_super_getnbckts(const fnx_super_t *);

void fnx_super_devise_fs(fnx_super_t *, fnx_size_t);

void fnx_super_resolve_sm(const fnx_super_t *, const fnx_vaddr_t *,
                          fnx_vaddr_t *, fnx_baddr_t *);

#endif /* FNX_CORE_SUPER_H_ */
