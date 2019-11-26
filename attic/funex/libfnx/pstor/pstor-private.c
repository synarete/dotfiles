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

fnx_logger_t *fnx_pstor_get_logger(const fnx_pstor_t *pstor)
{
	fnx_unused(pstor);
	return fnx_default_logger;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

#define watch_rc(rc, lba, cnt) watch_rc_(rc, lba, cnt, FNX_FUNCTION)

static void watch_rc_(int rc, fnx_lba_t lba, fnx_bkcnt_t cnt, const char *fn)
{
	switch (rc) {
		case 0:
			break;
		case -EAGAIN:
		case -EINTR:
		case -ENOSPC:
			fnx_warn("%s: lba=%#lx cnt=%lu err=%d", fn, lba, cnt, abs(rc));
			fnx_usleep(10); /* XXX */
			break;
		default:
			fnx_panic("%s: lba=%#lx cnt=%lu err=%d", fn, lba, cnt, abs(rc));
			break;
	}
}

static void verify_lba(const fnx_pstor_t *pstor, fnx_lba_t lba)
{
	fnx_lba_t lba_max;

	fnx_assert(lba != FNX_LBA_NULL);
	if (pstor->super != NULL) { /* May be NULL in bootstrap case */
		lba_max = fnx_bytes_to_nbk(pstor->super->su_volsize);
		fnx_assert(lba < lba_max);
	}
}

static void verify_bk(const fnx_pstor_t *pstor, const fnx_bkref_t *bkref)
{
	int rc;
	fnx_lba_t lba;
	const fnx_header_t *hdr;

	fnx_assert(bkref != NULL);
	lba = bkref->bk_baddr.lba;
	verify_lba(pstor, lba);

	hdr = (const fnx_header_t *)bkref->bk_blk;
	if ((lba % FNX_BCKTNBK) == 0) {
		fnx_assert(hdr->h_vtype == FNX_VTYPE_SPMAP);
		rc = fnx_dobj_check(hdr, FNX_VTYPE_SPMAP);
		fnx_assert(rc == 0);
	}
}

int fnx_pstor_read_bk(const fnx_pstor_t *pstor, const fnx_bkref_t *bkref)
{
	int rc, nn = 100;
	fnx_lba_t lba;
	const fnx_bkcnt_t cnt = 1;
	const fnx_bdev_t *bdev = &pstor->bdev;

	lba = bkref->bk_baddr.lba;
	verify_lba(pstor, lba);
	while (nn-- > 0) {
		rc = fnx_bdev_read(bdev, bkref->bk_blk, lba, cnt);
		if (rc == 0) {
			break;
		}
		watch_rc(rc, lba, cnt);
	}
	if (rc != 0) {
		pstor_error(pstor, "failed-read-bk: lba=%#lx rc=%d", lba, rc);
	}
	fnx_assert(rc == 0);
	verify_bk(pstor, bkref);
	return rc;
}

int fnx_pstor_write_bk(const fnx_pstor_t *pstor, const fnx_bkref_t *bkref)
{
	int rc, nn = 100;
	fnx_lba_t lba;
	const fnx_bkcnt_t cnt = 1;
	const fnx_bdev_t *bdev = &pstor->bdev;

	verify_bk(pstor, bkref);

	lba = bkref->bk_baddr.lba;
	while (nn-- > 0) {
		rc = fnx_bdev_write(bdev, bkref->bk_blk, lba, cnt);
		if (rc == 0) {
			break;
		}
		watch_rc(rc, lba, cnt);
	}
	fnx_assert(rc == 0);
	return rc;
}

int fnx_pstor_sync_bk(const fnx_pstor_t *pstor, const fnx_bkref_t *bkref)
{
	int rc;

	verify_bk(pstor, bkref);
	rc = fnx_bdev_sync(&pstor->bdev);
	fnx_assert(rc == 0);
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_pstor_new_bk(const fnx_pstor_t *pstor,
                     const fnx_baddr_t *baddr,
                     fnx_bkref_t      **bkref)
{
	*bkref = fnx_alloc_new_bk(pstor->alloc, baddr);
	if (*bkref == NULL) {
		pstor_error(pstor, "alloc-new-bk-failed: lba=%#lx", baddr->lba);
		return -ENOMEM;
	}
	return 0;
}

int fnx_pstor_new_bk2(const fnx_pstor_t *pstor,
                      const fnx_baddr_t *baddr,
                      fnx_spmap_t       *spmap,
                      fnx_bkref_t      **bkref)
{
	int rc;

	rc = fnx_pstor_new_bk(pstor, baddr, bkref);
	if (rc == 0) {
		fnx_bkref_attach(*bkref, spmap);
	}
	return rc;
}


void fnx_pstor_del_bk(const fnx_pstor_t *pstor, fnx_bkref_t *bkref)
{
	fnx_alloc_del_bk(pstor->alloc, bkref);
}

void fnx_pstor_del_bk2(const fnx_pstor_t *pstor, fnx_bkref_t *bkref)
{
	fnx_bkref_detach(bkref);
	fnx_alloc_del_bk(pstor->alloc, bkref);
}


int fnx_pstor_new_vobj(const fnx_pstor_t *pstor,
                       const fnx_vaddr_t *vaddr,
                       fnx_vnode_t      **vnode)
{
	*vnode = fnx_alloc_new_vobj2(pstor->alloc, vaddr);
	if (*vnode == NULL) {
		pstor_error(pstor, "alloc-new-vobj-failed: vtype=%d", vaddr->vtype);
		return -ENOMEM;
	}
	return 0;
}

void fnx_pstor_del_vobj(const fnx_pstor_t *pstor, fnx_vnode_t *vnode)
{
	fnx_alloc_del_vobj(pstor->alloc, vnode);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_pstor_decode_vobj(const fnx_pstor_t *pstor, fnx_vnode_t *vnode,
                          const fnx_bkref_t *bkref, const fnx_baddr_t *baddr)
{
	int rc;
	fnx_lba_t lba_max;

	if (pstor->super != NULL) {
		lba_max = fnx_bytes_to_nbk(pstor->super->su_volsize);
		fnx_assert(baddr->lba < lba_max);
	}
	rc = fnx_decode_vobj(vnode, bkref, baddr);
	if (rc != 0) {
		pstor_error(pstor, "no-decode-vobj: vtype=%d lba=%#lx frg=%d rc=%d",
		            fnx_vnode_vtype(vnode), baddr->lba, baddr->frg, rc);
	}
	return rc;
}
