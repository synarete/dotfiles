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
#ifndef FNX_CORE_DIRI_H_
#define FNX_CORE_DIRI_H_


static inline fnx_ino_t fnx_dir_getino(const fnx_dir_t *dir)
{
	return fnx_inode_getino(&dir->d_inode);
}

static inline int fnx_dir_isempty(const fnx_dir_t *dir)
{
	return (dir->d_nchilds == 0);
}

static inline int fnx_dir_isroot(const fnx_dir_t *dir)
{
	const fnx_ino_t dino = fnx_dir_getino(dir);
	return ((dino == FNX_INO_ROOT) && dir->d_rootd);
}


#endif /* FNX_CORE_DIRI_H_ */



