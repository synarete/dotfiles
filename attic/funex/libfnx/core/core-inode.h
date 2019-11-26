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
#ifndef FNX_CORE_INODE_H_
#define FNX_CORE_INODE_H_

/* Inodes init nlink defs */
#define FNX_INODE_INIT_NLINK     (0)
#define FNX_DIR_INIT_NLINK       (1)


/* V-type */
int fnx_mode_isspecial(fnx_mode_t);

int fnx_mode_isreg(fnx_mode_t);

fnx_vtype_e fnx_mode_to_vtype(fnx_mode_t);

fnx_mode_t fnx_vtype_to_mode(fnx_vtype_e);

fnx_mode_t fnx_vtype_to_crmode(fnx_vtype_e, fnx_mode_t, fnx_mode_t);

fnx_vtype_e fnx_vtype_by_symlnk_value(const fnx_path_t *);

fnx_hash_t fnx_calc_inamehash(const fnx_name_t *, fnx_ino_t);


/* V-node */
void fnx_vnode_init(fnx_vnode_t *, fnx_vtype_e);

void fnx_vnode_destroy(fnx_vnode_t *);

void fnx_vnode_clone(const fnx_vnode_t *, fnx_vnode_t *);

void fnx_vnode_htod(const fnx_vnode_t *, fnx_header_t *);

void fnx_vnode_dtoh(fnx_vnode_t *, const fnx_header_t *);

void fnx_vnode_setvaddr(fnx_vnode_t *, const fnx_vaddr_t *);

void fnx_vnode_attach(fnx_vnode_t *, const fnx_baddr_t *, fnx_bkref_t *);

fnx_bkref_t *fnx_vnode_detach(fnx_vnode_t *);

int fnx_vnode_ismutable(const fnx_vnode_t *);


/* I-node */
fnx_dinode_t *fnx_header_to_dinode(const fnx_header_t *);

fnx_inode_t *fnx_vnode_to_inode(const fnx_vnode_t *);

void fnx_inode_init(fnx_inode_t *, fnx_vtype_e);

void fnx_inode_destroy(fnx_inode_t *);

void fnx_inode_clone(const fnx_inode_t *, fnx_inode_t *);

void fnx_inode_setino(fnx_inode_t *, fnx_ino_t);

void fnx_inode_setsuper(fnx_inode_t *, fnx_super_t *);

void fnx_inode_setup(fnx_inode_t *, const fnx_uctx_t *,
                     fnx_mode_t, fnx_mode_t);

void fnx_inode_associate(fnx_inode_t *, fnx_ino_t, const fnx_name_t *);

void fnx_inode_clearsuid(fnx_inode_t *);

void fnx_inode_clearsgid(fnx_inode_t *);


void fnx_inode_htod(const fnx_inode_t *, fnx_dinode_t *);

void fnx_inode_dtoh(fnx_inode_t *, const fnx_dinode_t *);

int fnx_inode_dcheck(const fnx_header_t *);


void fnx_inode_setitimes(fnx_inode_t *, fnx_flags_t, const fnx_times_t *);

void fnx_inode_setitime(fnx_inode_t *, fnx_flags_t);

void fnx_inode_getiattr(const fnx_inode_t *, fnx_iattr_t *);

void fnx_inode_getname(const fnx_inode_t *, fnx_name_t *);

int fnx_inode_hasname(const fnx_inode_t *, const fnx_name_t *);

int fnx_inode_access(const fnx_inode_t *, const fnx_uctx_t *, fnx_mode_t);

int fnx_inode_isexec(const fnx_inode_t *);

int fnx_inode_isdir(const fnx_inode_t *);

int fnx_inode_isreg(const fnx_inode_t *);

int fnx_inode_isfifo(const fnx_inode_t *);

int fnx_inode_issock(const fnx_inode_t *);

int fnx_inode_issymlnk(const fnx_inode_t *);

int fnx_inode_isreflnk(const fnx_inode_t *);

fnx_ino_t fnx_inode_getrefino(const fnx_inode_t *);


int fnx_inode_testf(const fnx_inode_t *, fnx_flags_t);

void fnx_inode_setf(fnx_inode_t *, fnx_flags_t);

void fnx_inode_unsetf(fnx_inode_t *, fnx_flags_t);


#endif /* FNX_CORE_INODE_H_ */




