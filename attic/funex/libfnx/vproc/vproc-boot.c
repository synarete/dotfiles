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

#include <fnxinfra.h>
#include <fnxcore.h>
#include <fnxpstor.h>
#include "vproc-elems.h"
#include "vproc-exec.h"
#include "vproc-private.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_vproc_init(fnx_vproc_t *vproc)
{
	fnx_uctx_init(&vproc->uctx);
	fnx_pendq_init(&vproc->pendq);
	fnx_frpool_init(&vproc->frpool);
	fnx_bzero(vproc->pblk, sizeof(vproc->pblk));
	vproc->cache        = NULL;
	vproc->alloc        = NULL;
	vproc->pstor        = NULL;
	vproc->super        = NULL;
	vproc->rootd        = NULL;
	vproc->proot        = NULL;
	vproc->pino         = FNX_INO_ROOT;
	vproc->mntf         = FNX_MNTF_DEFAULT;
	vproc->dispatch     = NULL;
	vproc->verbose      = FNX_FALSE;
	vproc->server       = NULL;
}

void fnx_vproc_destroy(fnx_vproc_t *vproc)
{
	fnx_uctx_destroy(&vproc->uctx);
	fnx_pendq_destroy(&vproc->pendq);
	fnx_frpool_destroy(&vproc->frpool);
	fnx_bff(vproc->pblk, sizeof(vproc->pblk));
	vproc->cache        = NULL;
	vproc->alloc        = NULL;
	vproc->pstor        = NULL;
	vproc->super        = NULL;
	vproc->rootd        = NULL;
	vproc->proot        = NULL;
	vproc->pino         = FNX_INO_NULL;
	vproc->mntf         = 0;
	vproc->dispatch     = NULL;
	vproc->server       = NULL;
}

int fnx_vproc_hasopenf(const fnx_vproc_t *vporc)
{
	return fnx_list_size(&vporc->frpool.usedf) > 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_vproc_open_namespace(fnx_vproc_t *vproc, const char *volume)
{
	fnx_pstor_t *pstor      = vproc->pstor;
	fnx_dir_t   *rootd      = pstor->rootd;
	fnx_vnode_t *vnode      = &rootd->d_inode.i_vnode;
	fnx_super_t *super      = pstor->super;
	fnx_fsinfo_t *fsinfo    = &super->su_fsinfo;
	const fnx_uctx_t *uctx  = &vproc->uctx;

	vproc_info(vproc, "open-namespace: %s", volume);

	/* Require super's uid to match current run */
	if (fsinfo->fs_attr.f_uid != uctx->u_cred.cr_uid) {
		vproc_error(vproc, "uid-mismatch: %s fs-uid=%u uid=%u",
		            volume, fsinfo->fs_attr.f_uid, uctx->u_cred.cr_uid);
		return -EINVAL;
	}
	/* Expects gid to match current run */
	if (fsinfo->fs_attr.f_gid != uctx->u_cred.cr_gid) {
		vproc_warn(vproc, "gid-mismatch: %s fs-gid=%u gid=%u",
		           volume, fsinfo->fs_attr.f_gid, uctx->u_cred.cr_gid);
	}
	/* Refresh mount-bits & super's uctx */
	fsinfo->fs_attr.f_mntf = vproc->mntf;
	fnx_uctx_copy(&super->su_uctx, uctx);

	/* Bind pstor et.al. */
	vproc->super = pstor->super;
	vproc->rootd = pstor->rootd;

	/* Cache updated root-dir */
	vnode->v_pinned = FNX_TRUE;
	fnx_inode_setitime(&rootd->d_inode, FNX_AMCTIME_NOW);
	fnx_vcache_store_vnode(&vproc->cache->vc, vnode);

	return 0;
}
