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
#include "pstor-exec.h"
#include "pstor-private.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_pstor_init(fnx_pstor_t *pstor)
{
	fnx_bdev_init(&pstor->bdev);
	fnx_list_init(&pstor->sbkq);
	fnx_pendq_init(&pstor->pendq);
	pstor->cache    = NULL;
	pstor->super    = NULL;
	pstor->rootd    = NULL;
	pstor->alloc    = NULL;
	pstor->ops      = fnx_pstor_default_ops;
	pstor->server   = NULL;
	pstor->dispatch = NULL;
}

void fnx_pstor_destory(fnx_pstor_t *pstor)
{
	fnx_bdev_destroy(&pstor->bdev);
	fnx_list_destroy(&pstor->sbkq);
	fnx_pendq_destroy(&pstor->pendq);
	pstor->cache    = NULL;
	pstor->super    = NULL;
	pstor->rootd    = NULL;
	pstor->alloc    = NULL;
	pstor->ops      = NULL;
	pstor->server   = NULL;
	pstor->dispatch = NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Open underlying block-device storage-volume */
static int pstor_open_bdev(fnx_pstor_t *pstor, const char *path)
{
	int rc, flags;
	fnx_off_t volsz;
	fnx_bkcnt_t nbk;
	fnx_bdev_t *bdev = &pstor->bdev;

	pstor_info(pstor, "open-bdev: %s", path);

	flags = FNX_BDEV_RDWR | FNX_BDEV_DIRECT |
	        FNX_BDEV_LOCK | FNX_BDEV_SAVEPATH;
	rc = fnx_bdev_open(bdev, path, 0, 0, flags);
	if (rc != 0) {
		pstor_error(pstor, "no-open: %s flags=%#x err=%d", path, flags, -rc);
		return rc;
	}
	rc = fnx_bdev_getcap(bdev, &nbk);
	if (rc != 0) {
		fnx_bdev_close(bdev);
		pstor_error(pstor, "no-getcap: %s err=%d", path, -rc);
		return rc;
	}
	volsz = (fnx_off_t)fnx_nbk_to_nbytes(nbk);
	if (!fnx_isvalid_volsz(volsz)) {
		fnx_bdev_close(bdev);
		pstor_error(pstor, "illegal-volume-size: path=%s nbk=%lu", path, nbk);
		return rc;
	}
	rc = fnx_bdev_tryflock(bdev);
	if (rc != 0) {
		fnx_bdev_close(bdev);
		pstor_error(pstor, "no-flock: path=%s err=%d", path, -rc);
		return rc;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Load volume's super-block entry from well-known addressing (non-sync) */
static int pstor_open_super(fnx_pstor_t *pstor)
{
	int rc;
	fnx_baddr_t baddr;
	fnx_vaddr_t vaddr;
	fnx_bkref_t *bkref = NULL;
	fnx_vnode_t *vnode = NULL;

	fnx_baddr_for_super(&baddr);
	pstor_info(pstor, "open-super: lba=%#lx", baddr.lba);

	rc = fnx_pstor_new_bk(pstor, &baddr, &bkref);
	if (rc != 0) {
		goto out_err;
	}
	rc = fnx_pstor_read_bk(pstor, bkref);
	if (rc != 0) {
		goto out_err;
	}
	fnx_vaddr_for_super(&vaddr);
	rc = fnx_pstor_new_vobj(pstor, &vaddr, &vnode);
	if (rc != 0) {
		goto out_err;
	}
	rc = fnx_pstor_decode_vobj(pstor, vnode, bkref, &bkref->bk_baddr);
	if (rc != 0) {
		goto out_err;
	}
	/* Habemus super! */
	fnx_vnode_attach(vnode, &bkref->bk_baddr, bkref);
	vnode->v_placed = FNX_TRUE;
	vnode->v_pinned = FNX_TRUE;

	fnx_ucache_store_bk(&pstor->cache->uc, bkref);
	pstor->super = fnx_vnode_to_super(vnode);
	pstor->super->su_private = pstor->server;
	return 0;

out_err:
	if (bkref != NULL) {
		fnx_pstor_del_bk(pstor, bkref);
	}
	if (vnode != NULL) {
		fnx_pstor_del_vobj(pstor, vnode);
	}
	return rc;
}

static void pstor_report_super(const fnx_pstor_t *pstor)
{
	const fnx_super_t  *super  = pstor->super;
	const fnx_fsinfo_t *fsinfo = &super->su_fsinfo;

	pstor_info(pstor, "super.name=%s", super->su_name.str);
	pstor_info(pstor, "super.uuid=%s", fsinfo->fs_attr.f_uuid.str);
	pstor_info(pstor, "super.version=%s", super->su_version.str);
	pstor_info(pstor, "super.fsvers=%u", fsinfo->fs_attr.f_vers);
	pstor_info(pstor, "super.volsize=%lu", super->su_volsize);
	pstor_info(pstor, "super.nbckts=%lu", fsinfo->fs_attr.f_nbckts);
	pstor_info(pstor, "super.rootino=%#lx", fsinfo->fs_attr.f_rootino);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Verify super's info and block-device consistency */
static int pstor_verify_super(const fnx_pstor_t *pstor)
{
	int rc;
	fnx_ino_t ino;
	fnx_size_t  nbckts, nbckts_size, cap_bytes;
	fnx_bkcnt_t cap_min, cap_max, cap = 0;
	const fnx_super_t  *super  = pstor->super;
	const fnx_fsinfo_t *fsinfo = &super->su_fsinfo;
	const char *path = pstor->bdev.path;

	cap_min = fnx_bytes_to_nbk(FNX_VOLSIZE_MIN);
	cap_max = fnx_bytes_to_nbk(FNX_VOLSIZE_MAX);
	rc = fnx_bdev_getcap(&pstor->bdev, &cap);
	if (rc != 0) {
		pstor_error(pstor, "no-getcap: %s cap=%lu", path, cap);
		return rc;
	}
	if ((cap < cap_min) || (cap > cap_max)) {
		pstor_error(pstor, "invalid-capacity: %s cap=%lu", path, cap);
		return -EINVAL;
	}
	cap_bytes = fnx_nbk_to_nbytes(cap);
	if (cap_bytes < super->su_volsize) {
		pstor_error(pstor, "volume-size-mismatch: %s capacity=%lu volsize=%lu",
		            path, cap_bytes, super->su_volsize);
		return -EINVAL;
	}
	nbckts = super->su_fsinfo.fs_attr.f_nbckts;
	nbckts_size = (nbckts + 1) * FNX_BCKTSIZE;
	if (nbckts_size != super->su_volsize) {
		pstor_error(pstor, "volsize-buckets-inconsistency: %s "\
		            "volsize=%lu nbckts=%lu", path, super->su_volsize, nbckts);
		return -EINVAL;
	}
	ino = fsinfo->fs_attr.f_rootino;
	if (ino != FNX_INO_ROOT) {
		pstor_error(pstor, "illegal-root-ino: %s rootino=%#jx", path, ino);
		return -EINVAL;
	}
	ino = fsinfo->fs_stat.f_apex_ino;
	if (ino < FNX_INO_APEX0) {
		pstor_error(pstor, "illegal-apex-ino: %s apexino=%#jx", path, ino);
		return -EINVAL;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Load root-directory & associated space-mapping node into cache */
static int pstor_open_rootd(fnx_pstor_t *pstor)
{
	int rc;
	fnx_ino_t dino;
	fnx_dir_t *rootd = NULL;
	fnx_baddr_t baddr, sm_baddr;
	fnx_vaddr_t vaddr, sm_vaddr;
	fnx_bkref_t *bkref = NULL;
	fnx_vnode_t *vnode = NULL;
	fnx_bkref_t *sm_bkref = NULL;
	fnx_vnode_t *sm_vnode = NULL;
	fnx_spmap_t *spmap = NULL;

	dino = pstor->super->su_fsinfo.fs_attr.f_rootino;
	fnx_vaddr_for_by_ino(&vaddr, dino);
	fnx_super_resolve_sm(pstor->super, &vaddr, &sm_vaddr, &sm_baddr);
	pstor_info(pstor, "open-rootd: spmap.lba=%#lx", sm_baddr.lba);

	rc = fnx_pstor_new_bk(pstor, &sm_baddr, &sm_bkref);
	if (rc != 0) {
		goto out_err;
	}
	rc = fnx_pstor_read_bk(pstor, sm_bkref);
	if (rc != 0) {
		goto out_err;
	}
	rc = fnx_pstor_new_vobj(pstor, &sm_vaddr, &sm_vnode);
	if (rc != 0) {
		goto out_err;
	}
	rc = fnx_pstor_decode_vobj(pstor, sm_vnode, sm_bkref, &sm_baddr);
	if (rc != 0) {
		goto out_err;
	}
	spmap = fnx_vnode_to_spmap(sm_vnode);
	fnx_spmap_setup(spmap, &sm_vaddr, &sm_baddr, sm_bkref);
	rc = fnx_spmap_lookup(spmap, &vaddr, &baddr);
	if (rc != 0) {
		pstor_error(pstor, "no-rootd: spmap.lba=%#lx", sm_baddr.lba);
		goto out_err;
	}
	pstor_info(pstor, "open-rootd: lba=%#lx", baddr.lba);

	rc = fnx_pstor_new_bk(pstor, &baddr, &bkref);
	if (rc != 0) {
		goto out_err;
	}
	rc = fnx_pstor_read_bk(pstor, bkref);
	if (rc != 0) {
		goto out_err;
	}
	rc = fnx_pstor_new_vobj(pstor, &vaddr, &vnode);
	if (rc != 0) {
		goto out_err;
	}
	rc = fnx_pstor_decode_vobj(pstor, vnode, bkref, &baddr);
	if (rc != 0) {
		goto out_err;
	}
	rootd = fnx_vnode_to_dir(vnode);
	if (!rootd->d_rootd) {
		pstor_error(pstor, "illegal-rootd: lba=%#lx", baddr.lba);
		rc = -EINVAL;
		goto out_err;
	}
	/* Habemus root-dir! */
	fnx_vnode_attach(vnode, &baddr, bkref);
	vnode->v_placed = FNX_TRUE;
	vnode->v_pinned = FNX_TRUE;

	fnx_ucache_store_bk(&pstor->cache->uc, sm_bkref);
	fnx_ucache_store_bk(&pstor->cache->uc, bkref);
	fnx_ucache_store_spmap(&pstor->cache->uc, spmap);
	pstor->rootd = rootd;
	return 0;

out_err:
	if (sm_vnode != NULL) {
		fnx_vnode_detach(sm_vnode);
		fnx_pstor_del_vobj(pstor, sm_vnode);
	}
	if (sm_bkref != NULL) {
		fnx_pstor_del_bk(pstor, sm_bkref);
	}
	if (vnode != NULL) {
		fnx_pstor_del_vobj(pstor, vnode);
	}
	if (bkref != NULL) {
		fnx_pstor_del_bk(pstor, bkref);
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Verify super's apex ino and vlba are consistent with space-maps */
static int pstor_verify_ispace(const fnx_pstor_t *pstor)
{
	int rc;
	fnx_ino_t ino, apex_ino;
	fnx_vtype_e vtype;
	fnx_baddr_t baddr, sm_baddr;
	fnx_vaddr_t vaddr, sm_vaddr;
	fnx_bkref_t *sm_bkref   = NULL;
	fnx_vnode_t *sm_vnode   = NULL;
	fnx_spmap_t *spmap      = NULL;
	const fnx_super_t  *super  = pstor->super;
	const fnx_fsinfo_t *fsinfo = &super->su_fsinfo;

	apex_ino = fsinfo->fs_stat.f_apex_ino;
	for (size_t i = 0; i < 1024; ++i) {
		vtype = (i & 1) ? FNX_VTYPE_REG : FNX_VTYPE_DIR;
		ino = fnx_ino_create(apex_ino + i + 1, vtype);
		fnx_vaddr_for_by_ino(&vaddr, ino);

		fnx_super_resolve_sm(super, &vaddr, &sm_vaddr, &sm_baddr);
		rc = fnx_pstor_new_bk(pstor, &sm_baddr, &sm_bkref);
		if (rc != 0) {
			goto out_err;
		}
		rc = fnx_pstor_read_bk(pstor, sm_bkref);
		if (rc != 0) {
			goto out_err;
		}
		rc = fnx_pstor_new_vobj(pstor, &sm_vaddr, &sm_vnode);
		if (rc != 0) {
			goto out_err;
		}
		rc = fnx_pstor_decode_vobj(pstor, sm_vnode, sm_bkref, &sm_baddr);
		if (rc != 0) {
			goto out_err;
		}
		spmap = fnx_vnode_to_spmap(sm_vnode);
		rc = fnx_spmap_lookup(spmap, &vaddr, &baddr);
		if (rc == 0) {
			pstor_error(pstor, "unexpected-ino: ino=%#jx apex_ino=%#jx",
			            ino, apex_ino);
			goto out_err;
		}
		fnx_pstor_del_vobj(pstor, sm_vnode);
		fnx_pstor_del_bk(pstor, sm_bkref);
		sm_vnode = NULL;
		sm_bkref = NULL;
	}
	return 0;

out_err:
	if (sm_vnode != NULL) {
		fnx_pstor_del_vobj(pstor, sm_vnode);
	}
	if (sm_bkref != NULL) {
		fnx_pstor_del_bk(pstor, sm_bkref);
	}
	return rc;
}

static int pstor_verify_vspace(const fnx_pstor_t *pstor)
{
	int rc;
	fnx_lba_t vlba, apex_vlba;
	fnx_baddr_t baddr, sm_baddr;
	fnx_vaddr_t vaddr, sm_vaddr;
	fnx_bkref_t *sm_bkref   = NULL;
	fnx_vnode_t *sm_vnode   = NULL;
	fnx_spmap_t *spmap      = NULL;
	const fnx_super_t  *super  = pstor->super;
	const fnx_fsinfo_t *fsinfo = &super->su_fsinfo;

	apex_vlba = fsinfo->fs_stat.f_apex_vlba;
	for (size_t i = 0; i < 128; ++i) {
		vlba = apex_vlba + i + 1;
		fnx_vaddr_for_vbk(&vaddr, vlba);

		fnx_super_resolve_sm(super, &vaddr, &sm_vaddr, &sm_baddr);
		rc = fnx_pstor_new_bk(pstor, &sm_baddr, &sm_bkref);
		if (rc != 0) {
			goto out_err;
		}
		rc = fnx_pstor_read_bk(pstor, sm_bkref);
		if (rc != 0) {
			goto out_err;
		}
		rc = fnx_pstor_new_vobj(pstor, &sm_vaddr, &sm_vnode);
		if (rc != 0) {
			goto out_err;
		}
		rc = fnx_pstor_decode_vobj(pstor, sm_vnode, sm_bkref, &sm_baddr);
		if (rc != 0) {
			goto out_err;
		}
		spmap = fnx_vnode_to_spmap(sm_vnode);
		rc = fnx_spmap_lookup(spmap, &vaddr, &baddr);
		if (rc == 0) {
			pstor_error(pstor, "unexpected-vlba: vlba=%#jx apex_vlba=%#jx",
			            vlba, apex_vlba);
			goto out_err;
		}
		fnx_pstor_del_vobj(pstor, sm_vnode);
		fnx_pstor_del_bk(pstor, sm_bkref);
		sm_vnode = NULL;
		sm_bkref = NULL;
	}
	return 0;

out_err:
	if (sm_vnode != NULL) {
		fnx_pstor_del_vobj(pstor, sm_vnode);
	}
	if (sm_bkref != NULL) {
		fnx_pstor_del_bk(pstor, sm_bkref);
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_pstor_open_volume(fnx_pstor_t *pstor, const char *volume)
{
	int rc;

	pstor_info(pstor, "open-volume: %s", volume);
	rc = pstor_open_bdev(pstor, volume);
	if (rc != 0) {
		pstor_error(pstor, "open-pstor-bdev-failed: %s", volume);
		return rc;
	}
	rc = pstor_open_super(pstor);
	if (rc != 0) {
		pstor_error(pstor, "open-super-failed: %s", volume);
		fnx_pstor_close_volume(pstor);
		return rc;
	}
	pstor_report_super(pstor);
	rc = pstor_verify_super(pstor);
	if (rc != 0) {
		pstor_error(pstor, "illegal-pstor-super: %s", volume);
		fnx_pstor_close_volume(pstor);
		return rc;
	}
	rc = pstor_open_rootd(pstor);
	if (rc != 0) {
		pstor_error(pstor, "open-rootd-failed: %s", volume);
		fnx_pstor_close_volume(pstor);
		return rc;
	}
	rc = pstor_verify_ispace(pstor);
	if (rc != 0) {
		pstor_error(pstor, "verify-ispace-failed: %s", volume);
		fnx_pstor_close_volume(pstor);
		return rc;
	}
	rc = pstor_verify_vspace(pstor);
	if (rc != 0) {
		pstor_error(pstor, "verify-vspace-failed: %s", volume);
		fnx_pstor_close_volume(pstor);
		return rc;
	}
	pstor_info(pstor, "volume-opened: %s", volume);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void pstor_fini_rootd(fnx_pstor_t *pstor)
{
	fnx_ino_t dino;
	fnx_dir_t *rootd;

	if ((rootd = pstor->rootd) != NULL) {
		dino = rootd->d_inode.i_iattr.i_ino;
		pstor_info(pstor, "fini-rootd: dino=%ld", dino);
		fnx_vnode_detach(&rootd->d_inode.i_vnode);
		fnx_pstor_del_vobj(pstor, &rootd->d_inode.i_vnode);
		pstor->rootd = NULL;
	}
}

static void pstor_fini_super(fnx_pstor_t *pstor)
{
	fnx_super_t *super;
	fnx_bkref_t *bkref;

	if ((super = pstor->super) != NULL) {
		pstor_info(pstor, "fini-super: name=%s", super->su_name.str);
		bkref = fnx_vnode_detach(&super->su_vnode);
		fnx_pstor_del_bk(pstor, bkref);
		fnx_pstor_del_vobj(pstor, &super->su_vnode);
		pstor->super = NULL;
	}
}

static void pstor_close_bdev(fnx_pstor_t *pstor)
{
	int rc;
	fnx_bdev_t *bdev = &pstor->bdev;

	if (fnx_bdev_isopen(bdev)) {
		rc = fnx_bdev_sync(bdev);
		if (rc != 0) {
			pstor_error(pstor, "no-sync: %s err=%d", bdev->path, -rc);
		}
		fnx_bdev_close(bdev);
	}
}

void fnx_pstor_close_volume(fnx_pstor_t *pstor)
{
	pstor_info(pstor, "close-volume: %s", pstor->bdev.path);
	pstor_fini_rootd(pstor);
	pstor_fini_super(pstor);
	pstor_close_bdev(pstor);
}
