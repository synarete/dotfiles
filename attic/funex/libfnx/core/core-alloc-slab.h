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
#ifndef FNX_CORE_ALLOC_SLAB_H_
#define FNX_CORE_ALLOC_SLAB_H_

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Low-level block's memory allocation elements, which fits into regions of
 * mmapped blocks. Should never be used directly by lib's user.
 */

typedef struct fnx_aligned fnx_slab {
	const fnx_sinfo_t *sinfo;
	void           *smem;
	fnx_size_t      avail;
	fnx_magic_t     magic;

} fnx_slab_t;


typedef struct fnx_aligned fnx_sobj {
	fnx_slab_t     *slab;
	fnx_blk_t      *blk;
	fnx_link_t      lnk;

} fnx_sobj_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

typedef struct fnx_aligned64 fnx_sobj_task {
	fnx_sobj_t      sobj;
	fnx_task_t      task;

} fnx_sobj_task_t;


typedef struct fnx_aligned64 fnx_sobj_bkref {
	fnx_sobj_t      sobj;
	fnx_bkref_t     bkref;

} fnx_sobj_bkref_t;


typedef struct fnx_aligned64 fnx_sobj_spmap {
	fnx_sobj_t      sobj;
	fnx_spmap_t     spmap;

} fnx_sobj_spmap_t;


typedef struct fnx_aligned64 fnx_sobj_dir {
	fnx_sobj_t      sobj;
	fnx_dir_t       dir;

} fnx_sobj_dir_t;


typedef struct fnx_aligned64 fnx_sobj_dirseg {
	fnx_sobj_t      sobj;
	fnx_dirseg_t    dirseg;

} fnx_sobj_dirseg_t;


typedef struct fnx_aligned64 fnx_sobj_inode {
	fnx_sobj_t      sobj;
	fnx_inode_t     inode;

} fnx_sobj_inode_t;


typedef struct fnx_aligned64 fnx_sobj_reg {
	fnx_sobj_t      sobj;
	fnx_reg_t       reg;

} fnx_sobj_reg_t;


typedef struct fnx_aligned64 fnx_sobj_regseg {
	fnx_sobj_t      sobj;
	fnx_regseg_t    regseg;

} fnx_sobj_regseg_t;


typedef struct fnx_aligned64 fnx_sobj_regsec {
	fnx_sobj_t      sobj;
	fnx_regsec_t    regsec;

} fnx_sobj_regsec_t;


typedef struct fnx_aligned64 fnx_sobj_symlnk {
	fnx_sobj_t      sobj;
	fnx_symlnk_t    symlnk;

} fnx_sobj_symlnk_t;


typedef struct fnx_aligned64 fnx_sobj_vbk {
	fnx_sobj_t      sobj;
	fnx_vnode_t     vbk;

} fnx_sobj_vbk_t;



#endif /* FNX_CORE_ALLOC_SLAB_H_ */

