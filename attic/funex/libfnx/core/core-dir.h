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
#ifndef FNX_CORE_DIR_H_
#define FNX_CORE_DIR_H_


fnx_ddir_t *fnx_header_to_ddir(const fnx_header_t *);

fnx_dir_t *fnx_vnode_to_dir(const fnx_vnode_t *);

fnx_dir_t *fnx_inode_to_dir(const fnx_inode_t *);

void fnx_dir_init(fnx_dir_t *);

void fnx_dir_destroy(fnx_dir_t *);

void fnx_dir_clone(const fnx_dir_t *, fnx_dir_t *);

void fnx_dir_setup(fnx_dir_t *, const fnx_uctx_t *, fnx_mode_t);

void fnx_dir_htod(const fnx_dir_t *, fnx_ddir_t *);

void fnx_dir_dtoh(fnx_dir_t *, const fnx_ddir_t *);

int fnx_dir_dcheck(const fnx_header_t *);


void fnx_dir_iref_parentd(fnx_dir_t *, const fnx_dir_t *);

void fnx_dir_unref_parentd(fnx_dir_t *);


const fnx_dirent_t *fnx_dir_meta(const fnx_dir_t *, const fnx_name_t *);

fnx_dirent_t *fnx_dir_lookup(const fnx_dir_t *, fnx_hash_t, size_t);

fnx_dirent_t *fnx_dir_ilookup(const fnx_dir_t *, const fnx_inode_t *);

const fnx_dirent_t *fnx_dir_search(const fnx_dir_t *, fnx_off_t);

fnx_dirent_t *fnx_dir_link(fnx_dir_t *, const fnx_inode_t *);

fnx_dirent_t *fnx_dir_unlink(fnx_dir_t *, const fnx_inode_t *);

fnx_dirent_t *fnx_dir_predict(const fnx_dir_t *, fnx_hash_t);


void fnx_dir_associate_child(const fnx_dir_t *,
                             const fnx_name_t *, fnx_inode_t *);

int fnx_dir_hasseg(const fnx_dir_t *, fnx_off_t);

void fnx_dir_setseg(fnx_dir_t *, fnx_off_t);

void fnx_dir_unsetseg(fnx_dir_t *, fnx_off_t);

fnx_off_t fnx_dir_nextseg(const fnx_dir_t *, fnx_off_t);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_ddirseg_t *fnx_header_to_ddirseg(const fnx_header_t *);

fnx_dirseg_t *fnx_vnode_to_dirseg(const fnx_vnode_t *);

void fnx_dirseg_init(fnx_dirseg_t *);

void fnx_dirseg_destroy(fnx_dirseg_t *);

void fnx_dirseg_clone(const fnx_dirseg_t *, fnx_dirseg_t *);

void fnx_dirseg_setup(fnx_dirseg_t *, fnx_off_t);

int fnx_dirseg_isempty(const fnx_dirseg_t *);

void fnx_dirseg_htod(const fnx_dirseg_t *, fnx_ddirseg_t *);

void fnx_dirseg_dtoh(fnx_dirseg_t *, const fnx_ddirseg_t *);

int fnx_dirseg_dcheck(const fnx_header_t *);


fnx_dirent_t *fnx_dirseg_lookup(const fnx_dirseg_t *, fnx_hash_t, fnx_size_t);

fnx_dirent_t *fnx_dirseg_ilookup(const fnx_dirseg_t *, const fnx_inode_t *);

fnx_dirent_t *fnx_dirseg_search(const fnx_dirseg_t *, fnx_off_t);

fnx_dirent_t *fnx_dirseg_link(fnx_dirseg_t *, const fnx_inode_t *);

fnx_dirent_t *fnx_dirseg_unlink(fnx_dirseg_t *, const fnx_inode_t *);

fnx_dirent_t *fnx_dirseg_predict(const fnx_dirseg_t *, fnx_hash_t);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_hash_t fnx_inamehash(const fnx_name_t *, const fnx_dir_t *);

#endif /* FNX_CORE_DIR_H_ */
