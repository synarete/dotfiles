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
#include <stdint.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include <fnxinfra.h>
#include "core-blobs.h"
#include "core-types.h"
#include "core-stats.h"
#include "core-proto.h"
#include "core-htod.h"
#include "core-addr.h"
#include "core-addri.h"
#include "core-elems.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_iattr_init(fnx_iattr_t *iattr)
{
	fnx_bzero(iattr, sizeof(*iattr));
	fnx_times_init(&iattr->i_times);
	iattr->i_gen        = 1;
	iattr->i_nlink      = 0;
	iattr->i_uid        = (fnx_uid_t)FNX_UID_NULL;
	iattr->i_gid        = (fnx_gid_t)FNX_GID_NULL;
	iattr->i_mode       = 0;
	iattr->i_size       = 0;
	iattr->i_vcap       = 0;
}

void fnx_iattr_destroy(fnx_iattr_t *iattr)
{
	fnx_bff(iattr, sizeof(*iattr));
	iattr->i_uid        = (fnx_uid_t)(FNX_UID_NULL);
	iattr->i_gid        = (fnx_gid_t)(FNX_GID_NULL);
	iattr->i_mode       = 0;
}

void fnx_iattr_copy(fnx_iattr_t *iattr, const fnx_iattr_t *other)
{
	memcpy(iattr, other, sizeof(*iattr));
}

void fnx_iattr_htod(const fnx_iattr_t *iattr, fnx_diattr_t *diattr)
{
	/* R[0] */
	diattr->i_mode      = fnx_htod_mode(iattr->i_mode);
	diattr->i_nlink     = fnx_htod_nlink(iattr->i_nlink);
	diattr->i_uid       = fnx_htod_uid(iattr->i_uid);
	diattr->i_gid       = fnx_htod_gid(iattr->i_gid);
	diattr->i_rdev      = fnx_htod_dev(iattr->i_rdev);
	diattr->i_size      = fnx_htod_off(iattr->i_size);
	diattr->i_bcap      = fnx_htod_bkcnt(iattr->i_vcap);
	diattr->i_nblk      = fnx_htod_bkcnt(iattr->i_nblk);
	diattr->i_wops      = fnx_htod_size(iattr->i_wops);
	diattr->i_wcnt      = fnx_htod_size(iattr->i_wcnt);

	/* R[1] */
	fnx_times_htod(&iattr->i_times, &diattr->i_times);
}

void fnx_iattr_dtoh(fnx_iattr_t *iattr, const fnx_diattr_t *diattr)
{
	/* R[0] */
	iattr->i_mode       = fnx_dtoh_mode(diattr->i_mode);
	iattr->i_nlink      = fnx_dtoh_nlink(diattr->i_nlink);
	iattr->i_uid        = fnx_dtoh_uid(diattr->i_uid);
	iattr->i_gid        = fnx_dtoh_gid(diattr->i_gid);
	iattr->i_rdev       = fnx_dtoh_dev(diattr->i_rdev);
	iattr->i_size       = fnx_dtoh_off(diattr->i_size);
	iattr->i_vcap       = fnx_dtoh_bkcnt(diattr->i_bcap);
	iattr->i_nblk       = fnx_dtoh_bkcnt(diattr->i_nblk);
	iattr->i_wops       = fnx_dtoh_size(diattr->i_wops);
	iattr->i_wcnt       = fnx_dtoh_size(diattr->i_wcnt);

	/* R[1] */
	fnx_times_dtoh(&iattr->i_times, &diattr->i_times);
}


void fnx_iattr_to_stat(const fnx_iattr_t *iattr, struct stat *st)
{
	fnx_bzero(st, sizeof(*st));

	st->st_ino          = iattr->i_ino;
	st->st_dev          = iattr->i_dev;
	st->st_mode         = iattr->i_mode;
	st->st_nlink        = iattr->i_nlink;
	st->st_uid          = iattr->i_uid;
	st->st_gid          = iattr->i_gid;
	st->st_rdev         = iattr->i_rdev;
	st->st_size         = iattr->i_size;
	st->st_blksize      = FNX_BLKSIZE;
	st->st_blocks       = (blkcnt_t)(iattr->i_nblk * FNX_BLKNFRG);

	fnx_timespec_copy(&st->st_atim, &iattr->i_times.atime);
	fnx_timespec_copy(&st->st_ctim, &iattr->i_times.ctime);
	fnx_timespec_copy(&st->st_mtim, &iattr->i_times.mtime);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_fsattr_init(fnx_fsattr_t *fsattr)
{
	fnx_bzero(fsattr, sizeof(*fsattr));
	fsattr->f_type      = FNX_FSMAGIC;
	fsattr->f_vers      = FNX_FSVERSION;
	fsattr->f_mntf      = FNX_MNTF_DEFAULT;
	fsattr->f_gen       = 0;
	fsattr->f_uid       = (fnx_uid_t)(FNX_UID_NULL);
	fsattr->f_gid       = (fnx_gid_t)(FNX_GID_NULL);
	fsattr->f_rootino   = FNX_INO_ROOT;
	fsattr->f_nbckts    = 0;
}

void fnx_fsattr_setup(fnx_fsattr_t *fsattr,
                      const fnx_uuid_t *uuid, fnx_uid_t uid, fnx_gid_t gid)
{
	fnx_uuid_copy(&fsattr->f_uuid, uuid);
	fsattr->f_uid       = uid;
	fsattr->f_gid       = gid;
	fsattr->f_rootino   = FNX_INO_ROOT;
}

void fnx_fsattr_htod(const fnx_fsattr_t *fsattr, fnx_dfsattr_t *dfsattr)
{
	/* R[0] */
	fnx_htod_uuid(dfsattr->f_uuid, &fsattr->f_uuid);
	dfsattr->f_fstype   = fnx_htod_u32(fsattr->f_type);
	dfsattr->f_version  = fnx_htod_versnum(fsattr->f_vers);
	dfsattr->f_gen      = fnx_htod_u32((uint32_t)fsattr->f_gen);
	dfsattr->f_uid      = fnx_htod_uid(fsattr->f_uid);
	dfsattr->f_gid      = fnx_htod_gid(fsattr->f_gid);
	dfsattr->f_rootino  = fnx_htod_ino(fsattr->f_rootino);
	dfsattr->f_nbckts   = fnx_htod_u64(fsattr->f_nbckts);
}

void fnx_fsattr_dtoh(fnx_fsattr_t *fsattr, const fnx_dfsattr_t *dfsattr)
{
	/* R[0] */
	fnx_dtoh_uuid(&fsattr->f_uuid, dfsattr->f_uuid);
	fsattr->f_type      = fnx_dtoh_u32(dfsattr->f_fstype);
	fsattr->f_vers      = fnx_dtoh_versnum(dfsattr->f_version);
	fsattr->f_gen       = fnx_dtoh_u32(dfsattr->f_gen);
	fsattr->f_uid       = fnx_dtoh_uid(dfsattr->f_uid);
	fsattr->f_gid       = fnx_dtoh_gid(dfsattr->f_gid);
	fsattr->f_rootino   = fnx_dtoh_ino(dfsattr->f_rootino);
	fsattr->f_nbckts    = fnx_dtoh_u64(dfsattr->f_nbckts);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_fsstat_init(fnx_fsstat_t *fsstat)
{
	fsstat->f_apex_ino  = FNX_INO_MAX;
	fsstat->f_apex_vlba = FNX_LBA_VMAX;

	fsstat->f_inodes    = 0;
	fsstat->f_iused     = 0;
	fsstat->f_bfrgs     = 0;
	fsstat->f_bused     = 0;
	fsstat->f_super     = 0;
	fsstat->f_dir       = 0;
	fsstat->f_dirseg    = 0;
	fsstat->f_symlnk    = 0;
	fsstat->f_reflnk    = 0;
	fsstat->f_special   = 0;
	fsstat->f_reg       = 0;
	fsstat->f_regsec    = 0;
	fsstat->f_regseg    = 0;
	fsstat->f_vbk       = 0;
}

void fnx_fsstat_htod(const fnx_fsstat_t *fsstat, fnx_dfsstat_t *dfsstat)
{
	/* R[0] */
	dfsstat->f_apex_ino = fnx_htod_ino(fsstat->f_apex_ino);
	dfsstat->f_apex_vlba = fnx_htod_lba(fsstat->f_apex_vlba);

	/* R[1] */
	dfsstat->f_bfrgs    = fnx_htod_bkcnt(fsstat->f_bfrgs);
	dfsstat->f_bused    = fnx_htod_bkcnt(fsstat->f_bused);
	dfsstat->f_inodes   = fnx_htod_filcnt(fsstat->f_inodes);
	dfsstat->f_iused    = fnx_htod_filcnt(fsstat->f_iused);

	/* R[2] */
	dfsstat->f_super    = fnx_htod_size(fsstat->f_super);
	dfsstat->f_dir      = fnx_htod_size(fsstat->f_dir);
	dfsstat->f_dirseg   = fnx_htod_size(fsstat->f_dirseg);
	dfsstat->f_symlnk   = fnx_htod_size(fsstat->f_symlnk);
	dfsstat->f_reflnk   = fnx_htod_size(fsstat->f_reflnk);
	dfsstat->f_special  = fnx_htod_size(fsstat->f_special);

	/* R[3] */
	dfsstat->f_reg      = fnx_htod_size(fsstat->f_reg);
	dfsstat->f_regsec    = fnx_htod_size(fsstat->f_regsec);
	dfsstat->f_regseg   = fnx_htod_size(fsstat->f_regseg);
	dfsstat->f_vbk      = fnx_htod_size(fsstat->f_vbk);
}

void fnx_fsstat_dtoh(fnx_fsstat_t *fsstat, const fnx_dfsstat_t *dfsstat)
{
	/* R[0] */
	fsstat->f_apex_ino  = fnx_dtoh_ino(dfsstat->f_apex_ino);
	fsstat->f_apex_vlba = fnx_dtoh_lba(dfsstat->f_apex_vlba);

	/* R[1] */
	fsstat->f_bfrgs     = fnx_dtoh_bkcnt(dfsstat->f_bfrgs);
	fsstat->f_bused     = fnx_dtoh_bkcnt(dfsstat->f_bused);
	fsstat->f_inodes    = fnx_dtoh_filcnt(dfsstat->f_inodes);
	fsstat->f_iused     = fnx_dtoh_filcnt(dfsstat->f_iused);

	/* R[2] */
	fsstat->f_super     = fnx_dtoh_size(dfsstat->f_super);
	fsstat->f_dir       = fnx_dtoh_size(dfsstat->f_dir);
	fsstat->f_dirseg    = fnx_dtoh_size(dfsstat->f_dirseg);
	fsstat->f_symlnk    = fnx_dtoh_size(dfsstat->f_symlnk);
	fsstat->f_reflnk    = fnx_dtoh_size(dfsstat->f_reflnk);
	fsstat->f_special   = fnx_dtoh_size(dfsstat->f_special);

	/* R[3] */
	fsstat->f_reg       = fnx_dtoh_size(dfsstat->f_reg);
	fsstat->f_regsec     = fnx_dtoh_size(dfsstat->f_regsec);
	fsstat->f_regseg    = fnx_dtoh_size(dfsstat->f_regseg);
	fsstat->f_vbk       = fnx_dtoh_size(dfsstat->f_vbk);
}

#define FSSTAT_OFF(vtype) offsetof(fnx_fsstat_t, vtype)

static const size_t s_vtype_to_vstat_off[] = {
	[FNX_VTYPE_SUPER]   = FSSTAT_OFF(f_super),
	[FNX_VTYPE_CHRDEV]  = FSSTAT_OFF(f_special),
	[FNX_VTYPE_BLKDEV]  = FSSTAT_OFF(f_special),
	[FNX_VTYPE_SOCK]    = FSSTAT_OFF(f_special),
	[FNX_VTYPE_FIFO]    = FSSTAT_OFF(f_special),
	[FNX_VTYPE_SYMLNK1] = FSSTAT_OFF(f_symlnk),
	[FNX_VTYPE_SYMLNK2] = FSSTAT_OFF(f_symlnk),
	[FNX_VTYPE_SYMLNK3] = FSSTAT_OFF(f_symlnk),
	[FNX_VTYPE_REFLNK]  = FSSTAT_OFF(f_reflnk),
	[FNX_VTYPE_DIR]     = FSSTAT_OFF(f_dir),
	[FNX_VTYPE_DIRSEG]  = FSSTAT_OFF(f_dirseg),
	[FNX_VTYPE_REG]     = FSSTAT_OFF(f_reg),
	[FNX_VTYPE_REGSEC]  = FSSTAT_OFF(f_regsec),
	[FNX_VTYPE_REGSEG]  = FSSTAT_OFF(f_regseg),
	[FNX_VTYPE_VBK]     = FSSTAT_OFF(f_vbk)
};

static fnx_size_t *fsstat_getcnt(fnx_fsstat_t *fsstat, int vtype)
{
	size_t nelems, offset = 0;
	void *ptr = NULL;

	nelems = FNX_NELEMS(s_vtype_to_vstat_off);
	if ((vtype > 0) && (vtype < (int)nelems)) {
		offset = s_vtype_to_vstat_off[vtype];
		if ((offset > 0) && (offset < sizeof(*fsstat))) {
			ptr = (void *)((char *)fsstat + offset);
		}
	}
	return (fnx_size_t *)ptr;
}

static fnx_bkcnt_t getnfrgs(fnx_vtype_e vtype)
{
	return fnx_get_dobjnfrgs(vtype);
}

static fnx_bkcnt_t getnfrgs2(fnx_vtype_e vtype1, fnx_vtype_e vtype2)
{
	return fnx_get_dobjnfrgs(vtype1) + fnx_get_dobjnfrgs(vtype2);
}

void fnx_fsstat_account(fnx_fsstat_t *fsstat, fnx_vtype_e vtype, int n)
{
	fnx_bkcnt_t  nfrgs;
	fnx_size_t   *vtcnt;
	fnx_filcnt_t *incnt;
	fnx_filcnt_t none = 0;

	nfrgs = getnfrgs(vtype);
	vtcnt = fsstat_getcnt(fsstat, vtype);
	incnt = fnx_isitype(vtype) ? &fsstat->f_iused : &none;
	fnx_assert(vtcnt != NULL);
	if (n > 0) {
		fnx_assert(fsstat->f_bused + nfrgs <= fsstat->f_bfrgs);
		fnx_assert(*vtcnt < ((fnx_size_t)(-1) / 8));
		fsstat->f_bused += nfrgs;
		*vtcnt += (fnx_size_t)n;
		*incnt += (fnx_filcnt_t)n;
	} else {
		fnx_assert(*vtcnt >= (fnx_size_t)(-n));
		fnx_assert(fsstat->f_bused >= nfrgs);
		fsstat->f_bused -= nfrgs;
		*vtcnt -= (fnx_size_t)(-n);
		*incnt -= (fnx_filcnt_t)(-n);
	}
}

/*
 * Heuristic calculation of fs-stats for freshly-formatted fs within a volume.
 * Total number of available data-bks is the sum of all sections-bks minus:
 * 1. First meta section
 * 2. First spmap-block in each section
 * 3. Reserved 4 blocks for space collisions within section
 *
 * Assuming no more then single inode per block.
 */
void fnx_fsstat_devise_fs(fnx_fsstat_t *fsstat, fnx_size_t nsecs)
{
	const fnx_size_t secnbk  = FNX_BCKTNBK;
	const fnx_size_t totbks  = (nsecs - 1) * secnbk;
	const fnx_size_t totfrgs = fnx_nbk_to_nfrg(totbks);
	const fnx_size_t datbks  = (nsecs - 1) * (secnbk - 1 - 4);
	const fnx_size_t datfrgs = fnx_nbk_to_nfrg(datbks);
	const fnx_size_t usrfrgs = getnfrgs2(FNX_VTYPE_SUPER, FNX_VTYPE_DIR);

	fsstat->f_apex_ino  = FNX_INO_APEX0;
	fsstat->f_apex_vlba = FNX_LBA_APEX0;
	fsstat->f_inodes    = datbks - 2;
	fsstat->f_iused     = 2; /* Super + root-dir */
	fsstat->f_bfrgs     = totfrgs;
	fsstat->f_bused     = (totfrgs - datfrgs) + usrfrgs;
	fsstat->f_super     = 1;
	fsstat->f_dir       = 1;
}

int fnx_fsstat_next_ino(fnx_fsstat_t *fsstat, fnx_ino_t *ino)
{
	int rc = -1;

	if (fsstat->f_iused < fsstat->f_inodes) {
		*ino = (fsstat->f_apex_ino + 1);
		rc = 0;
	}
	return rc;
}

void fnx_fsstat_stamp_ino(fnx_fsstat_t *fsstat, fnx_ino_t ino)
{
	if (ino > fsstat->f_apex_ino) {
		fsstat->f_apex_ino = ino;
	}
}

int fnx_fsstat_next_vlba(const fnx_fsstat_t *fsstat, fnx_lba_t *vlba)
{
	int rc = -1;
	fnx_bkcnt_t nfrg = FNX_BLKNFRG;

	*vlba = FNX_LBA_VNULL;
	if ((fsstat->f_bused + nfrg) < fsstat->f_bfrgs) {
		*vlba = (fsstat->f_apex_vlba + 1);
		rc = 0;
	}
	return rc;
}

void fnx_fsstat_stamp_vlba(fnx_fsstat_t *fsstat, fnx_lba_t vlba)
{
	if (vlba > fsstat->f_apex_vlba) {
		fsstat->f_apex_vlba = vlba;
	}
}

static fnx_size_t reduced(fnx_size_t sz)
{
	return (sz > 3) ? (sz - 3) : 0;
}

int fnx_fsstat_hasnexti(const fnx_fsstat_t *fsstat, int prvlgd)
{
	int res = FNX_FALSE;

	if (fsstat->f_iused < fsstat->f_inodes) {
		res = prvlgd || (fsstat->f_iused < reduced(fsstat->f_inodes));
	}
	return res;
}

int fnx_fsstat_hasfreeb(const fnx_fsstat_t *fsstat,
                        fnx_bkcnt_t nfrgs, int prvlgd)
{
	int res = FNX_FALSE;
	const fnx_bkcnt_t bsum = fsstat->f_bused + nfrgs;

	if (bsum < fsstat->f_bfrgs) {
		res = prvlgd || (bsum < reduced(fsstat->f_bfrgs));
	}
	return res;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

#define OPSTAT_OFF(opc) offsetof(fnx_opstat_t, opc)

static const size_t s_opc_to_opstat_off[] = {
	[FNX_VOP_LOOKUP]      = OPSTAT_OFF(op_lookup),
	[FNX_VOP_FORGET]      = OPSTAT_OFF(op_forget),
	[FNX_VOP_GETATTR]     = OPSTAT_OFF(op_getattr),
	[FNX_VOP_SETATTR]     = OPSTAT_OFF(op_setattr),
	[FNX_VOP_READLINK]    = OPSTAT_OFF(op_readlink),
	[FNX_VOP_SYMLINK]     = OPSTAT_OFF(op_symlink),
	[FNX_VOP_MKNOD]       = OPSTAT_OFF(op_mknod),
	[FNX_VOP_MKDIR]       = OPSTAT_OFF(op_mkdir),
	[FNX_VOP_UNLINK]      = OPSTAT_OFF(op_unlink),
	[FNX_VOP_RMDIR]       = OPSTAT_OFF(op_rmdir),
	[FNX_VOP_RENAME]      = OPSTAT_OFF(op_rename),
	[FNX_VOP_LINK]        = OPSTAT_OFF(op_link),
	[FNX_VOP_OPEN]        = OPSTAT_OFF(op_open),
	[FNX_VOP_READ]        = OPSTAT_OFF(op_read),
	[FNX_VOP_WRITE]       = OPSTAT_OFF(op_write),
	[FNX_VOP_STATFS]      = OPSTAT_OFF(op_statfs),
	[FNX_VOP_RELEASE]     = OPSTAT_OFF(op_release),
	[FNX_VOP_FSYNC]       = OPSTAT_OFF(op_fsync),
	[FNX_VOP_FLUSH]       = OPSTAT_OFF(op_flush),
	[FNX_VOP_OPENDIR]     = OPSTAT_OFF(op_opendir),
	[FNX_VOP_READDIR]     = OPSTAT_OFF(op_readdir),
	[FNX_VOP_RELEASEDIR]  = OPSTAT_OFF(op_releasedir),
	[FNX_VOP_FSYNCDIR]    = OPSTAT_OFF(op_fsyncdir),
	[FNX_VOP_ACCESS]      = OPSTAT_OFF(op_access),
	[FNX_VOP_CREATE]      = OPSTAT_OFF(op_create),
	[FNX_VOP_INTERRUPT]   = OPSTAT_OFF(op_interrupt),
	[FNX_VOP_BMAP]        = OPSTAT_OFF(op_bmap),
	[FNX_VOP_POLL]        = OPSTAT_OFF(op_poll),
	[FNX_VOP_TRUNCATE]    = OPSTAT_OFF(op_truncate),
	[FNX_VOP_FALLOCATE]   = OPSTAT_OFF(op_fallocate),
	[FNX_VOP_PUNCH]       = OPSTAT_OFF(op_punch),
	[FNX_VOP_FQUERY]      = OPSTAT_OFF(op_fquery),
	[FNX_VOP_FSINFO]      = OPSTAT_OFF(op_fsinfo),
	[FNX_VOP_LAST]        = 0
};

/* Convert opcode-enum to counter */
static fnx_size_t *opstat_getcnt(fnx_opstat_t *opstat, int opc)
{
	size_t nelems, offset = 0;
	void *ptr = NULL;

	nelems = FNX_NELEMS(s_opc_to_opstat_off);
	if ((opc > 0) && (opc < (int)nelems)) {
		offset = s_opc_to_opstat_off[opc];
		if ((offset > 0) && (offset < sizeof(fnx_opstat_t))) {
			ptr = (void *)((char *)opstat + offset);
		}
	}
	return (fnx_size_t *)ptr;
}

void fnx_opstat_count(fnx_opstat_t *opstat, int opc)
{
	fnx_size_t *ptr;

	ptr = opstat_getcnt(opstat, opc);
	if (ptr != NULL) {
		*ptr += 1;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Converts from internal stats repr to system 'statvfs' repr. Let privileged
 * user have ~10% more inodes/blocks then normal-user.
 */
void fnx_fsinfo_to_statvfs(const fnx_fsinfo_t *fsinfo, struct statvfs *stvfs)
{
	const fnx_fsattr_t *fsattr = &fsinfo->fs_attr;
	const fnx_fsstat_t *fsstat = &fsinfo->fs_stat;

	fnx_bzero(stvfs, sizeof(*stvfs));
	stvfs->f_bsize   = (long unsigned int)(FNX_BLKSIZE);
	stvfs->f_frsize  = (long unsigned int)(FNX_FRGSIZE);
	stvfs->f_namemax = FNX_NAME_MAX;
	stvfs->f_flag    = fsattr->f_mntf;
	stvfs->f_blocks  = fsstat->f_bfrgs;
	stvfs->f_bfree   = fsstat->f_bfrgs - fsstat->f_bused;
	stvfs->f_bavail  = reduced(stvfs->f_bfree);
	stvfs->f_files   = fsstat->f_inodes;
	stvfs->f_ffree   = fsstat->f_inodes - fsstat->f_iused;
	stvfs->f_favail  = reduced(stvfs->f_ffree);

	/*
	 * In 'man fstatfs(2)' we find the statment that "Nobody knows what f_fsid
	 * is supposed to contain (but see below)", yet that "the general idea is
	 * that f_fsid contains some random stuff such that the pair (f_fsid,ino)
	 * uniquely determines a file".
	 *
	 * NB: Trying to do our best; multiple funex mounts will have same fsid.
	 */
	stvfs->f_fsid = FNX_FSMAGIC;
}

void fnx_fsinfo_copy(fnx_fsinfo_t *tgt, const fnx_fsinfo_t *src)
{
	fnx_bcopy(tgt, src, sizeof(*tgt));
}
