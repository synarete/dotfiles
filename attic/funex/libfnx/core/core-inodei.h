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
#ifndef FNX_CORE_INODEI_H_
#define FNX_CORE_INODEI_H_

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static inline fnx_vtype_e fnx_vnode_vtype(const fnx_vnode_t *vnode)
{
	return vnode->v_vaddr.vtype;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static inline fnx_vtype_e fnx_inode_vtype(const fnx_inode_t *inode)
{
	return fnx_vnode_vtype(&inode->i_vnode);
}

static inline fnx_ino_t fnx_inode_getino(const fnx_inode_t *inode)
{
	return inode->i_vnode.v_vaddr.ino;
}

static inline int fnx_inode_hasino(const fnx_inode_t *inode, fnx_ino_t ino)
{
	return (ino == fnx_inode_getino(inode));
}


static inline fnx_off_t fnx_inode_getsize(const fnx_inode_t *inode)
{
	return inode->i_iattr.i_size;
}

static inline void fnx_inode_setsize(fnx_inode_t *inode, fnx_off_t size)
{
	inode->i_iattr.i_size = size;
}

static inline fnx_bkcnt_t fnx_inode_getvcap(const fnx_inode_t *inode)
{
	return inode->i_iattr.i_vcap;
}

static inline void fnx_inode_setvcap(fnx_inode_t *inode, fnx_bkcnt_t vcap)
{
	inode->i_iattr.i_vcap = vcap;
}

static inline fnx_nlink_t fnx_inode_getnlink(const fnx_inode_t *inode)
{
	return inode->i_iattr.i_nlink;
}

static inline fnx_size_t fnx_inode_getrefcnt(const fnx_inode_t *inode)
{
	return inode->i_vnode.v_refcnt;
}

#endif /* FNX_CORE_INODEI_H_ */
