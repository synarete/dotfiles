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
#include <fnxconfig.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <fnxinfra.h>
#include "core-blobs.h"
#include "core-types.h"
#include "core-stats.h"
#include "core-addr.h"
#include "core-elems.h"
#include "core-inode.h"
#include "core-inodei.h"
#include "core-dir.h"
#include "core-file.h"
#include "core-space.h"
#include "core-super.h"


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void init_spmap(fnx_vnode_t *vnode)
{
	fnx_spmap_init(fnx_vnode_to_spmap(vnode));
}

static void destroy_spmap(fnx_vnode_t *vnode)
{
	fnx_spmap_destroy(fnx_vnode_to_spmap(vnode));
}

static void clone_spmap(const fnx_vnode_t *vnode, fnx_vnode_t *other)
{
	fnx_spmap_clone(fnx_vnode_to_spmap(vnode), fnx_vnode_to_spmap(other));
}

static void dexport_spmap(const fnx_vnode_t *vnode, fnx_header_t *hdr)
{
	fnx_spmap_htod(fnx_vnode_to_spmap(vnode), fnx_header_to_dspmap(hdr));
}

static void dimport_spmap(fnx_vnode_t *vnode, const fnx_header_t *hdr)
{
	fnx_spmap_dtoh(fnx_vnode_to_spmap(vnode), fnx_header_to_dspmap(hdr));
}

static int dcheck_spmap(const fnx_header_t *hdr)
{
	return fnx_spmap_dcheck(hdr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void init_super(fnx_vnode_t *vnode)
{
	fnx_super_init(fnx_vnode_to_super(vnode));
}

static void destroy_super(fnx_vnode_t *vnode)
{
	fnx_super_destroy(fnx_vnode_to_super(vnode));
}

static void clone_super(const fnx_vnode_t *vnode, fnx_vnode_t *other)
{
	fnx_super_clone(fnx_vnode_to_super(vnode), fnx_vnode_to_super(other));
}

static void dexport_super(const fnx_vnode_t *vnode, fnx_header_t *hdr)
{
	fnx_super_htod(fnx_vnode_to_super(vnode), fnx_header_to_dsuper(hdr));
}

static void dimport_super(fnx_vnode_t *vnode, const fnx_header_t *hdr)
{
	fnx_super_dtoh(fnx_vnode_to_super(vnode), fnx_header_to_dsuper(hdr));
}

static int dcheck_super(const fnx_header_t *hdr)
{
	return fnx_super_dcheck(hdr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void init_dir(fnx_vnode_t *vnode)
{
	fnx_dir_init(fnx_vnode_to_dir(vnode));
}

static void destroy_dir(fnx_vnode_t *vnode)
{
	fnx_dir_destroy(fnx_vnode_to_dir(vnode));
}

static void clone_dir(const fnx_vnode_t *vnode, fnx_vnode_t *other)
{
	fnx_dir_clone(fnx_vnode_to_dir(vnode), fnx_vnode_to_dir(other));
}

static void dexport_dir(const fnx_vnode_t *vnode, fnx_header_t *hdr)
{
	fnx_dir_htod(fnx_vnode_to_dir(vnode), fnx_header_to_ddir(hdr));
}

static void dimport_dir(fnx_vnode_t *vnode, const fnx_header_t *hdr)
{
	fnx_dir_dtoh(fnx_vnode_to_dir(vnode), fnx_header_to_ddir(hdr));
}

static int dcheck_dir(const fnx_header_t *hdr)
{
	return fnx_dir_dcheck(hdr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void init_dirseg(fnx_vnode_t *vnode)
{
	fnx_dirseg_init(fnx_vnode_to_dirseg(vnode));
}

static void destroy_dirseg(fnx_vnode_t *vnode)
{
	fnx_dirseg_destroy(fnx_vnode_to_dirseg(vnode));
}

static void clone_dirseg(const fnx_vnode_t *vnode, fnx_vnode_t *other)
{
	fnx_dirseg_clone(fnx_vnode_to_dirseg(vnode), fnx_vnode_to_dirseg(other));
}

static void dexport_dirseg(const fnx_vnode_t *vnode, fnx_header_t *hdr)
{
	fnx_dirseg_htod(fnx_vnode_to_dirseg(vnode), fnx_header_to_ddirseg(hdr));
}

static void dimport_dirseg(fnx_vnode_t *vnode, const fnx_header_t *hdr)
{
	fnx_dirseg_dtoh(fnx_vnode_to_dirseg(vnode), fnx_header_to_ddirseg(hdr));
}

static int dcheck_dirseg(const fnx_header_t *hdr)
{
	return fnx_dirseg_dcheck(hdr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void init_chrdev(fnx_vnode_t *vnode)
{
	fnx_inode_init(fnx_vnode_to_inode(vnode), FNX_VTYPE_CHRDEV);
}

static void destroy_chrdev(fnx_vnode_t *vnode)
{
	fnx_inode_destroy(fnx_vnode_to_inode(vnode));
}

static void clone_chrdev(const fnx_vnode_t *vnode, fnx_vnode_t *other)
{
	fnx_inode_clone(fnx_vnode_to_inode(vnode), fnx_vnode_to_inode(other));
}

static void dexport_chrdev(const fnx_vnode_t *vnode, fnx_header_t *hdr)
{
	fnx_inode_htod(fnx_vnode_to_inode(vnode), fnx_header_to_dinode(hdr));
}

static void dimport_chrdev(fnx_vnode_t *vnode, const fnx_header_t *hdr)
{
	fnx_inode_dtoh(fnx_vnode_to_inode(vnode), fnx_header_to_dinode(hdr));
}

static int dcheck_chrdev(const fnx_header_t *hdr)
{
	return fnx_inode_dcheck(hdr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void init_blkdev(fnx_vnode_t *vnode)
{
	fnx_inode_init(fnx_vnode_to_inode(vnode), FNX_VTYPE_BLKDEV);
}

static void destroy_blkdev(fnx_vnode_t *vnode)
{
	fnx_inode_destroy(fnx_vnode_to_inode(vnode));
}

static void clone_blkdev(const fnx_vnode_t *vnode, fnx_vnode_t *other)
{
	fnx_inode_clone(fnx_vnode_to_inode(vnode), fnx_vnode_to_inode(other));
}

static void dexport_blkdev(const fnx_vnode_t *vnode, fnx_header_t *hdr)
{
	fnx_inode_htod(fnx_vnode_to_inode(vnode), fnx_header_to_dinode(hdr));
}

static void dimport_blkdev(fnx_vnode_t *vnode, const fnx_header_t *hdr)
{
	fnx_inode_dtoh(fnx_vnode_to_inode(vnode), fnx_header_to_dinode(hdr));
}

static int dcheck_blkdev(const fnx_header_t *hdr)
{
	return fnx_inode_dcheck(hdr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void init_sock(fnx_vnode_t *vnode)
{
	fnx_inode_init(fnx_vnode_to_inode(vnode), FNX_VTYPE_SOCK);
}

static void destroy_sock(fnx_vnode_t *vnode)
{
	fnx_inode_destroy(fnx_vnode_to_inode(vnode));
}

static void clone_sock(const fnx_vnode_t *vnode, fnx_vnode_t *other)
{
	fnx_inode_clone(fnx_vnode_to_inode(vnode), fnx_vnode_to_inode(other));
}

static void dexport_sock(const fnx_vnode_t *vnode, fnx_header_t *hdr)
{
	fnx_inode_htod(fnx_vnode_to_inode(vnode), fnx_header_to_dinode(hdr));
}

static void dimport_sock(fnx_vnode_t *vnode, const fnx_header_t *hdr)
{
	fnx_inode_dtoh(fnx_vnode_to_inode(vnode), fnx_header_to_dinode(hdr));
}

static int dcheck_sock(const fnx_header_t *hdr)
{
	return fnx_inode_dcheck(hdr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void init_fifo(fnx_vnode_t *vnode)
{
	fnx_inode_init(fnx_vnode_to_inode(vnode), FNX_VTYPE_FIFO);
}

static void destroy_fifo(fnx_vnode_t *vnode)
{
	fnx_inode_destroy(fnx_vnode_to_inode(vnode));
}

static void clone_fifo(const fnx_vnode_t *vnode, fnx_vnode_t *other)
{
	fnx_inode_clone(fnx_vnode_to_inode(vnode), fnx_vnode_to_inode(other));
}

static void dexport_fifo(const fnx_vnode_t *vnode, fnx_header_t *hdr)
{
	fnx_inode_htod(fnx_vnode_to_inode(vnode), fnx_header_to_dinode(hdr));
}

static void dimport_fifo(fnx_vnode_t *vnode, const fnx_header_t *hdr)
{
	fnx_inode_dtoh(fnx_vnode_to_inode(vnode), fnx_header_to_dinode(hdr));
}

static int dcheck_fifo(const fnx_header_t *hdr)
{
	return fnx_inode_dcheck(hdr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void init_reflnk(fnx_vnode_t *vnode)
{
	fnx_inode_init(fnx_vnode_to_inode(vnode), FNX_VTYPE_REFLNK);
}

static void destroy_reflnk(fnx_vnode_t *vnode)
{
	fnx_inode_destroy(fnx_vnode_to_inode(vnode));
}

static void clone_reflnk(const fnx_vnode_t *vnode, fnx_vnode_t *other)
{
	fnx_inode_clone(fnx_vnode_to_inode(vnode), fnx_vnode_to_inode(other));
}

static void dexport_reflnk(const fnx_vnode_t *vnode, fnx_header_t *hdr)
{
	fnx_inode_htod(fnx_vnode_to_inode(vnode), fnx_header_to_dinode(hdr));
}

static void dimport_reflnk(fnx_vnode_t *vnode, const fnx_header_t *hdr)
{
	fnx_inode_dtoh(fnx_vnode_to_inode(vnode), fnx_header_to_dinode(hdr));
}

static int dcheck_reflnk(const fnx_header_t *hdr)
{
	return fnx_inode_dcheck(hdr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void init_symlnk1(fnx_vnode_t *vnode)
{
	fnx_symlnk_init(fnx_vnode_to_symlnk(vnode), FNX_VTYPE_SYMLNK1);
}

static void destroy_symlnk1(fnx_vnode_t *vnode)
{
	fnx_symlnk_destroy(fnx_vnode_to_symlnk(vnode));
}

static void clone_symlnk1(const fnx_vnode_t *vnode, fnx_vnode_t *other)
{
	fnx_symlnk_clone(fnx_vnode_to_symlnk(vnode), fnx_vnode_to_symlnk(other));
}

static void dexport_symlnk1(const fnx_vnode_t *vnode, fnx_header_t *hdr)
{
	fnx_symlnk_htod1(fnx_vnode_to_symlnk(vnode), fnx_header_to_dsymlnk1(hdr));
}

static void dimport_symlnk1(fnx_vnode_t *vnode, const fnx_header_t *hdr)
{
	fnx_symlnk_dtoh1(fnx_vnode_to_symlnk(vnode), fnx_header_to_dsymlnk1(hdr));
}

static int dcheck_symlnk1(const fnx_header_t *hdr)
{
	return fnx_symlnk_dcheck(hdr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void init_symlnk2(fnx_vnode_t *vnode)
{
	fnx_symlnk_init(fnx_vnode_to_symlnk(vnode), FNX_VTYPE_SYMLNK2);
}

static void destroy_symlnk2(fnx_vnode_t *vnode)
{
	fnx_symlnk_destroy(fnx_vnode_to_symlnk(vnode));
}

static void clone_symlnk2(const fnx_vnode_t *vnode, fnx_vnode_t *other)
{
	fnx_symlnk_clone(fnx_vnode_to_symlnk(vnode), fnx_vnode_to_symlnk(other));
}

static void dexport_symlnk2(const fnx_vnode_t *vnode, fnx_header_t *hdr)
{
	fnx_symlnk_htod2(fnx_vnode_to_symlnk(vnode), fnx_header_to_dsymlnk2(hdr));
}

static void dimport_symlnk2(fnx_vnode_t *vnode, const fnx_header_t *hdr)
{
	fnx_symlnk_dtoh2(fnx_vnode_to_symlnk(vnode), fnx_header_to_dsymlnk2(hdr));
}

static int dcheck_symlnk2(const fnx_header_t *hdr)
{
	return fnx_symlnk_dcheck(hdr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void init_symlnk3(fnx_vnode_t *vnode)
{
	fnx_symlnk_init(fnx_vnode_to_symlnk(vnode), FNX_VTYPE_SYMLNK3);
}

static void destroy_symlnk3(fnx_vnode_t *vnode)
{
	fnx_symlnk_destroy(fnx_vnode_to_symlnk(vnode));
}

static void clone_symlnk3(const fnx_vnode_t *vnode, fnx_vnode_t *other)
{
	fnx_symlnk_clone(fnx_vnode_to_symlnk(vnode), fnx_vnode_to_symlnk(other));
}

static void dexport_symlnk3(const fnx_vnode_t *vnode, fnx_header_t *hdr)
{
	fnx_symlnk_htod3(fnx_vnode_to_symlnk(vnode), fnx_header_to_dsymlnk3(hdr));
}

static void dimport_symlnk3(fnx_vnode_t *vnode, const fnx_header_t *hdr)
{
	fnx_symlnk_dtoh3(fnx_vnode_to_symlnk(vnode), fnx_header_to_dsymlnk3(hdr));
}

static int dcheck_symlnk3(const fnx_header_t *hdr)
{
	return fnx_symlnk_dcheck(hdr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void init_reg(fnx_vnode_t *vnode)
{
	fnx_reg_init(fnx_vnode_to_reg(vnode));
}

static void destroy_reg(fnx_vnode_t *vnode)
{
	fnx_reg_destroy(fnx_vnode_to_reg(vnode));
}

static void clone_reg(const fnx_vnode_t *vnode, fnx_vnode_t *other)
{
	fnx_reg_clone(fnx_vnode_to_reg(vnode), fnx_vnode_to_reg(other));
}

static void dexport_reg(const fnx_vnode_t *vnode, fnx_header_t *hdr)
{
	fnx_reg_htod(fnx_vnode_to_reg(vnode), fnx_header_to_dreg(hdr));
}

static void dimport_reg(fnx_vnode_t *vnode, const fnx_header_t *hdr)
{
	fnx_reg_dtoh(fnx_vnode_to_reg(vnode), fnx_header_to_dreg(hdr));
}

static int dcheck_reg(const fnx_header_t *hdr)
{
	return fnx_reg_dcheck(hdr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void init_regsec(fnx_vnode_t *vnode)
{
	fnx_regsec_init(fnx_vnode_to_regsec(vnode));
}

static void destroy_regsec(fnx_vnode_t *vnode)
{
	fnx_regsec_destroy(fnx_vnode_to_regsec(vnode));
}

static void clone_regsec(const fnx_vnode_t *vnode, fnx_vnode_t *other)
{
	fnx_regsec_clone(fnx_vnode_to_regsec(vnode), fnx_vnode_to_regsec(other));
}

static void dexport_regsec(const fnx_vnode_t *vnode, fnx_header_t *hdr)
{
	fnx_regsec_htod(fnx_vnode_to_regsec(vnode), fnx_header_to_dregsec(hdr));
}

static void dimport_regsec(fnx_vnode_t *vnode, const fnx_header_t *hdr)
{
	fnx_regsec_dtoh(fnx_vnode_to_regsec(vnode), fnx_header_to_dregsec(hdr));
}

static int dcheck_regsec(const fnx_header_t *hdr)
{
	return fnx_regsec_dcheck(hdr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void init_regseg(fnx_vnode_t *vnode)
{
	fnx_regseg_init(fnx_vnode_to_regseg(vnode));
}

static void destroy_regseg(fnx_vnode_t *vnode)
{
	fnx_regseg_destroy(fnx_vnode_to_regseg(vnode));
}

static void clone_regseg(const fnx_vnode_t *vnode, fnx_vnode_t *other)
{
	fnx_regseg_clone(fnx_vnode_to_regseg(vnode), fnx_vnode_to_regseg(other));
}

static void dexport_regseg(const fnx_vnode_t *vnode, fnx_header_t *hdr)
{
	fnx_regseg_htod(fnx_vnode_to_regseg(vnode), fnx_header_to_dregseg(hdr));
}

static void dimport_regseg(fnx_vnode_t *vnode, const fnx_header_t *hdr)
{
	fnx_regseg_dtoh(fnx_vnode_to_regseg(vnode), fnx_header_to_dregseg(hdr));
}

static int dcheck_regseg(const fnx_header_t *hdr)
{
	return fnx_regseg_dcheck(hdr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void init_vbk(fnx_vnode_t *vnode)
{
	fnx_vnode_init(vnode, FNX_VTYPE_VBK);
}

static void destroy_vbk(fnx_vnode_t *vnode)
{
	fnx_vnode_destroy(vnode);
}

static void clone_vbk(const fnx_vnode_t *vnode, fnx_vnode_t *other)
{
	fnx_unused2(vnode, other);
}

static void dexport_vbk(const fnx_vnode_t *vnode, fnx_header_t *hdr)
{
	fnx_unused2(vnode, hdr);
}

static void dimport_vbk(fnx_vnode_t *vnode, const fnx_header_t *hdr)
{
	fnx_unused2(vnode, hdr);
}

static int dcheck_vbk(const fnx_header_t *hdr)
{
	fnx_unused(hdr);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const fnx_vinfo_t s_vinfo_none = {
	.name       = "NONE",
};

static const fnx_vinfo_t s_vinfo_empty = {
	.name       = "EMPTY",
};

static const fnx_vinfo_t s_vinfo_any = {
	.name       = "ANY",
};

static const fnx_vinfo_t s_vinfo_spmap = {
	.name       = "SPMAP",
	.size       = sizeof(fnx_spmap_t),
	.dsize      = sizeof(fnx_dspmap_t),
	.init       = init_spmap,
	.destroy    = destroy_spmap,
	.clone      = clone_spmap,
	.dexport    = dexport_spmap,
	.dimport    = dimport_spmap,
	.dcheck     = dcheck_spmap,
};

static const fnx_vinfo_t s_vinfo_super = {
	.name       = "SUPER",
	.size       = sizeof(fnx_super_t),
	.dsize      = sizeof(fnx_dsuper_t),
	.init       = init_super,
	.destroy    = destroy_super,
	.clone      = clone_super,
	.dexport    = dexport_super,
	.dimport    = dimport_super,
	.dcheck     = dcheck_super,
};

static const fnx_vinfo_t s_vinfo_dir = {
	.name       = "DIR",
	.size       = sizeof(fnx_dir_t),
	.dsize      = sizeof(fnx_ddir_t),
	.init       = init_dir,
	.destroy    = destroy_dir,
	.clone      = clone_dir,
	.dexport    = dexport_dir,
	.dimport    = dimport_dir,
	.dcheck     = dcheck_dir
};

static const fnx_vinfo_t s_vinfo_dirseg = {
	.name       = "DIRSEG",
	.size       = sizeof(fnx_dirseg_t),
	.dsize      = sizeof(fnx_ddirseg_t),
	.init       = init_dirseg,
	.destroy    = destroy_dirseg,
	.clone      = clone_dirseg,
	.dexport    = dexport_dirseg,
	.dimport    = dimport_dirseg,
	.dcheck     = dcheck_dirseg
};

static const fnx_vinfo_t s_vinfo_chrdev = {
	.name       = "CHRDEV",
	.size       = sizeof(fnx_inode_t),
	.dsize      = sizeof(fnx_dinode_t),
	.init       = init_chrdev,
	.destroy    = destroy_chrdev,
	.clone      = clone_chrdev,
	.dexport    = dexport_chrdev,
	.dimport    = dimport_chrdev,
	.dcheck     = dcheck_chrdev
};

static const fnx_vinfo_t s_vinfo_blkdev = {
	.name       = "BLKDEV",
	.size       = sizeof(fnx_inode_t),
	.dsize      = sizeof(fnx_dinode_t),
	.init       = init_blkdev,
	.destroy    = destroy_blkdev,
	.clone      = clone_blkdev,
	.dexport    = dexport_blkdev,
	.dimport    = dimport_blkdev,
	.dcheck     = dcheck_blkdev
};

static const fnx_vinfo_t s_vinfo_sock = {
	.name       = "SOCK",
	.size       = sizeof(fnx_inode_t),
	.dsize      = sizeof(fnx_dinode_t),
	.init       = init_sock,
	.clone      = clone_sock,
	.destroy    = destroy_sock,
	.dexport    = dexport_sock,
	.dimport    = dimport_sock,
	.dcheck     = dcheck_sock,
};

static const fnx_vinfo_t s_vinfo_fifo = {
	.name       = "FIFO",
	.size       = sizeof(fnx_inode_t),
	.dsize      = sizeof(fnx_dinode_t),
	.init       = init_fifo,
	.destroy    = destroy_fifo,
	.clone      = clone_fifo,
	.dexport    = dexport_fifo,
	.dimport    = dimport_fifo,
	.dcheck     = dcheck_fifo
};

static const fnx_vinfo_t s_vinfo_reflnk = {
	.name       = "REFLNK",
	.size       = sizeof(fnx_inode_t),
	.dsize      = sizeof(fnx_dinode_t),
	.init       = init_reflnk,
	.destroy    = destroy_reflnk,
	.clone      = clone_reflnk,
	.dexport    = dexport_reflnk,
	.dimport    = dimport_reflnk,
	.dcheck     = dcheck_reflnk
};

static const fnx_vinfo_t s_vinfo_symlnk1 = {
	.name       = "SYMLNK1",
	.size       = sizeof(fnx_symlnk_t),
	.dsize      = sizeof(fnx_dsymlnk1_t),
	.init       = init_symlnk1,
	.destroy    = destroy_symlnk1,
	.clone      = clone_symlnk1,
	.dexport    = dexport_symlnk1,
	.dimport    = dimport_symlnk1,
	.dcheck     = dcheck_symlnk1
};

static const fnx_vinfo_t s_vinfo_symlnk2 = {
	.name       = "SYMLNK2",
	.size       = sizeof(fnx_symlnk_t),
	.dsize      = sizeof(fnx_dsymlnk2_t),
	.init       = init_symlnk2,
	.destroy    = destroy_symlnk2,
	.clone      = clone_symlnk2,
	.dexport    = dexport_symlnk2,
	.dimport    = dimport_symlnk2,
	.dcheck     = dcheck_symlnk2
};

static const fnx_vinfo_t s_vinfo_symlnk3 = {
	.name       = "SYMLNK3",
	.size       = sizeof(fnx_symlnk_t),
	.dsize      = sizeof(fnx_dsymlnk3_t),
	.init       = init_symlnk3,
	.destroy    = destroy_symlnk3,
	.clone      = clone_symlnk3,
	.dexport    = dexport_symlnk3,
	.dimport    = dimport_symlnk3,
	.dcheck     = dcheck_symlnk3
};

static const fnx_vinfo_t s_vinfo_reg = {
	.name       = "REG",
	.size       = sizeof(fnx_reg_t),
	.dsize      = sizeof(fnx_dreg_t),
	.init       = init_reg,
	.destroy    = destroy_reg,
	.clone      = clone_reg,
	.dexport    = dexport_reg,
	.dimport    = dimport_reg,
	.dcheck     = dcheck_reg
};

static const fnx_vinfo_t s_vinfo_regsec = {
	.name       = "REGSEC",
	.size       = sizeof(fnx_regsec_t),
	.dsize      = sizeof(fnx_dregsec_t),
	.init       = init_regsec,
	.destroy    = destroy_regsec,
	.clone      = clone_regsec,
	.dexport    = dexport_regsec,
	.dimport    = dimport_regsec,
	.dcheck     = dcheck_regsec
};

static const fnx_vinfo_t s_vinfo_regseg = {
	.name       = "REGSEG",
	.size       = sizeof(fnx_regseg_t),
	.dsize      = sizeof(fnx_dregseg_t),
	.init       = init_regseg,
	.destroy    = destroy_regseg,
	.clone      = clone_regseg,
	.dexport    = dexport_regseg,
	.dimport    = dimport_regseg,
	.dcheck     = dcheck_regseg
};

static const fnx_vinfo_t s_vinfo_vbk = {
	.name       = "VBK",
	.size       = sizeof(fnx_vnode_t),
	.dsize      = sizeof(fnx_dblk_t),
	.init       = init_vbk,
	.destroy    = destroy_vbk,
	.clone      = clone_vbk,
	.dexport    = dexport_vbk,
	.dimport    = dimport_vbk,
	.dcheck     = dcheck_vbk
};

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const fnx_vinfo_t *s_vinfo_tbl[] = {
	[FNX_VTYPE_NONE]     = &s_vinfo_none,
	[FNX_VTYPE_SUPER]    = &s_vinfo_super,
	[FNX_VTYPE_SPMAP]    = &s_vinfo_spmap,
	[FNX_VTYPE_DIR]      = &s_vinfo_dir,
	[FNX_VTYPE_DIRSEG]   = &s_vinfo_dirseg,
	[FNX_VTYPE_CHRDEV]   = &s_vinfo_chrdev,
	[FNX_VTYPE_BLKDEV]   = &s_vinfo_blkdev,
	[FNX_VTYPE_SOCK]     = &s_vinfo_sock,
	[FNX_VTYPE_FIFO]     = &s_vinfo_fifo,
	[FNX_VTYPE_SYMLNK1]  = &s_vinfo_symlnk1,
	[FNX_VTYPE_SYMLNK2]  = &s_vinfo_symlnk2,
	[FNX_VTYPE_SYMLNK3]  = &s_vinfo_symlnk3,
	[FNX_VTYPE_REFLNK]   = &s_vinfo_reflnk,
	[FNX_VTYPE_REG]      = &s_vinfo_reg,
	[FNX_VTYPE_REGSEC]   = &s_vinfo_regsec,
	[FNX_VTYPE_REGSEG]   = &s_vinfo_regseg,
	[FNX_VTYPE_VBK]      = &s_vinfo_vbk,
	[FNX_VTYPE_EMPTY]    = &s_vinfo_empty,
	[FNX_VTYPE_ANY]      = &s_vinfo_any
};


const fnx_vinfo_t *fnx_get_vinfo(fnx_vtype_e vtype)
{
	const size_t idx = (size_t)vtype;
	const fnx_vinfo_t *vinfo = NULL;

	if (idx < FNX_NELEMS(s_vinfo_tbl)) {
		vinfo = s_vinfo_tbl[idx];
	}
	return vinfo;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_init_vobj(fnx_vnode_t *vnode, fnx_vtype_e vtype)
{
	const fnx_vinfo_t *vinfo;

	vinfo = fnx_get_vinfo(vtype);
	vnode->v_vaddr.vtype = vtype;
	vinfo->init(vnode);
}

void fnx_destroy_vobj(fnx_vnode_t *vnode)
{
	const fnx_vinfo_t *vinfo;

	vinfo = fnx_get_vinfo(vnode->v_vaddr.vtype);
	vinfo->destroy(vnode);
}

void fnx_export_vobj(const fnx_vnode_t *vnode, fnx_header_t *hdr)
{
	const fnx_vinfo_t *vinfo;

	vinfo = fnx_get_vinfo(vnode->v_vaddr.vtype);
	vinfo->dexport(vnode, hdr);

	fnx_assert(vinfo->dcheck(hdr) == 0);
}

void fnx_import_vobj(fnx_vnode_t *vnode, const fnx_header_t *hdr)
{
	int rc;
	const fnx_vinfo_t *vinfo;

	vinfo = fnx_get_vinfo(vnode->v_vaddr.vtype);
	rc = vinfo->dcheck(hdr);
	if (rc != 0) {
		fnx_assert(rc == 0);
	}
	fnx_assert(rc == 0);
	vinfo->dimport(vnode, hdr);
}

int fnx_check_dobj(const fnx_header_t *hdr, fnx_vtype_e expected_vtype)
{
	int rc;
	fnx_vtype_e vtype;
	const fnx_vinfo_t *vinfo;

	vtype = fnx_dobj_vtype(hdr);
	if ((expected_vtype != FNX_VTYPE_ANY) && (vtype != expected_vtype)) {
		fnx_warn("unexpected-dobj-vtype: vtype=%d expected=%d",
		         (int)vtype, (int)expected_vtype);
		return -1;
	}
	vinfo = fnx_get_vinfo(vtype);
	if (vinfo == NULL) {
		fnx_error("unsupported-dobj: vtype=%d", (int)vtype);
		return -1;
	}
	rc = fnx_dobj_check(hdr, FNX_VTYPE_ANY);
	if (rc != 0) {
		return rc;
	}
	return vinfo->dcheck(hdr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* On-disk object sizes in bytes/fragments units */
fnx_size_t fnx_get_dobjsize(fnx_vtype_e vtype)
{
	const fnx_vinfo_t *vinfo;

	vinfo = fnx_get_vinfo(vtype);
	return vinfo ? vinfo->dsize : 0;
}

fnx_bkcnt_t fnx_get_dobjnfrgs(fnx_vtype_e vtype)
{
	const fnx_size_t nbytes = fnx_get_dobjsize(vtype);
	return fnx_bytes_to_nfrg(nbytes);
}
