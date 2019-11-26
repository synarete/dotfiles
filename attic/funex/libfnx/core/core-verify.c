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
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/mount.h>
#include <sys/capability.h>

#include <fnxinfra.h>
#include "core-blobs.h"
#include "core-types.h"
#include "core-stats.h"
#include "core-htod.h"
#include "core-addr.h"
#include "core-elems.h"
#include "core-iobuf.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* On-disk meta-data objects bytes-sizes, repr in fragment (512) units */
#define FNX_NRECS(n)        (FNX_RECORDSIZE * n)
#define FNX_NFGRS(n)        (FNX_FRGSIZE * n)

#define N2U(n, u)           ((n + u - 1) / u)
#define SIZE_IN_FRG(sz)     N2U(sz, FNX_FRGSIZE)
#define FRG_PER_BLK         SIZE_IN_FRG(FNX_BLKSIZE)

#define staticassert(a) \
	FNX_STATICASSERT(a)
#define staticassert_eq(a, b) \
	FNX_STATICASSERT_EQ(a, b)
#define staticassert_sizeof(type, size) \
	staticassert_eq(sizeof(type), size)
#define staticassert_eqmemsz(type, field, size) \
	staticassert_eq(FNX_FIELD_SIZEOF(type, field), size)
#define staticassert_eqarrsz(type, field, size) \
	staticassert_eq(FNX_ARRFIELD_NELEMS(type, field), size)
#define staticassert_eqarrsz2(type1, field1, type2, field2) \
	staticassert_eq(FNX_ARRFIELD_NELEMS(type1, field1), \
	                FNX_ARRFIELD_NELEMS(type2, field2))


#define check_capf(f, c)    staticassert_eq(f, (1 << c))

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void verify_defs(void)
{
	staticassert_eq(FNX_PATH_MAX, PATH_MAX);
	staticassert_eq(FNX_NAME_MAX, NAME_MAX);

	staticassert_eq(FNX_BLKSIZE, FNX_BLKNFRG * FNX_FRGSIZE);
	staticassert_eq(FNX_BCKTSIZE, FNX_BCKTNFRG * FNX_FRGSIZE);
	staticassert_eq(FNX_BCKTSIZE, FNX_BCKTNBK * FNX_BLKSIZE);
	staticassert_eq(FNX_RSEGSIZE, FNX_RSEGNBK * FNX_BLKSIZE);
	staticassert_eq(FNX_RSEGSIZE, FNX_RSEGNFRG * FNX_FRGSIZE);
	staticassert_eq(FNX_RSECSIZE, FNX_RSEGSIZE * FNX_RSECNSEG);
	staticassert_eq(FNX_BCKTSIZE, FNX_BCKTNBK * FNX_BLKSIZE);
	staticassert_eq(FNX_RSECNBK, FNX_RSECNSEG * FNX_RSEGNBK);
	staticassert_eq(FNX_REGNSEG, FNX_REGNSEC * FNX_RSECNSEG);
	staticassert_eq(FNX_REGNSEG * FNX_RSEGSIZE, FNX_REGNSEC * FNX_RSECSIZE);
	staticassert_eq(FNX_SYMLNK3_MAX, FNX_PATH_MAX);
	staticassert(FNX_VOLSIZE_MIN >= (2 * FNX_BCKTSIZE));
	staticassert(FNX_DIRCHILD_MAX < (FNX_DOFF_END / 2));

	staticassert_eq(FNX_REGSIZE_MAX, FNX_REGNSEG * FNX_RSEGSIZE);
	staticassert_eq(FNX_REGSIZE_MAX, FNX_REGNSEC * FNX_RSECSIZE);
}

static void verify_types(void)
{
	/* Funex requires 64-bits platform */
	staticassert_eq(sizeof(size_t), sizeof(uint64_t));
	staticassert_eq(sizeof(off_t), sizeof(int64_t));
	staticassert_eq(sizeof(ino_t), sizeof(uint64_t));

	staticassert_eq(sizeof(fnx_blk_t), FNX_BLKSIZE);

	/* I-types are used in ino low 4 bits */
	staticassert(FNX_VTYPE_DIR < 0xF);
	staticassert(FNX_VTYPE_CHRDEV < 0xF);
	staticassert(FNX_VTYPE_BLKDEV < 0xF);
	staticassert(FNX_VTYPE_SOCK < 0xF);
	staticassert(FNX_VTYPE_FIFO < 0xF);
	staticassert(FNX_VTYPE_SYMLNK1 < 0xF);
	staticassert(FNX_VTYPE_SYMLNK2 < 0xF);
	staticassert(FNX_VTYPE_SYMLNK3 < 0xF);
	staticassert(FNX_VTYPE_REFLNK < 0xF);
	staticassert(FNX_VTYPE_REG  < 0xF);

	/* Special case: root-ino */
	staticassert_eq(FNX_INO_ROOT, FNX_VTYPE_DIR);
}

static void verify_dobjs(void)
{
	staticassert_sizeof(fnx_header_t,   FNX_HDRSIZE);
	staticassert_sizeof(fnx_dfrg_t,     FNX_FRGSIZE);
	staticassert_sizeof(fnx_dblk_t,     FNX_BLKSIZE);

	staticassert_sizeof(fnx_dvref_t,    FNX_VREFSIZE);
	staticassert_sizeof(fnx_dsuper_t,   FNX_BLKSIZE);
	staticassert_sizeof(fnx_dspmap_t,   FNX_BLKSIZE);

	staticassert_sizeof(fnx_diattr_t,   FNX_NRECS(2));
	staticassert_sizeof(fnx_dfselems_t, FNX_NRECS(2));
	staticassert_sizeof(fnx_dfsattr_t,  FNX_NRECS(1));
	staticassert_sizeof(fnx_dfsstat_t,  FNX_NRECS(4));

	staticassert_sizeof(fnx_dinode_t,   FNX_NFGRS(1));
	staticassert_sizeof(fnx_ddir_t,     FNX_NFGRS(16));
	staticassert_sizeof(fnx_ddirseg_t,  FNX_NFGRS(1));
	staticassert_sizeof(fnx_dsymlnk1_t, FNX_NFGRS(2));
	staticassert_sizeof(fnx_dsymlnk2_t, FNX_NFGRS(4));
	staticassert_sizeof(fnx_dsymlnk3_t, FNX_NFGRS(9));
	staticassert_sizeof(fnx_dreg_t,     FNX_NFGRS(4));
	staticassert_sizeof(fnx_dregsec_t,  FNX_NFGRS(1));
	staticassert_sizeof(fnx_dregseg_t,  FNX_NFGRS(2));

	staticassert_sizeof(fnx_dbucket_t,  FNX_BCKTSIZE);
	staticassert_sizeof(fnx_dbucket_t,  FNX_NFGRS(FNX_BCKTNFRG));
}

static void verify_dmembers(void)
{
	staticassert_eqmemsz(fnx_dfsattr_t, f_uuid, FNX_UUIDSIZE);
	staticassert_eqarrsz(fnx_dblk_t, bk_frg, FRG_PER_BLK);
	staticassert_eqmemsz(fnx_dsymlnk1_t, sl_lnk, FNX_SYMLNK1_MAX);
	staticassert_eqmemsz(fnx_dsymlnk2_t, sl_lnk, FNX_SYMLNK2_MAX);
	staticassert_eqmemsz(fnx_dsymlnk3_t, sl_lnk, FNX_SYMLNK3_MAX);
	staticassert_eqarrsz(fnx_dregseg_t, rs_vlba, FNX_RSEGNBK);
	staticassert_eqarrsz(fnx_dregsec_t, rc_rsegs, FNX_RSECNSEG / 32);
	staticassert_eqarrsz(fnx_ddir_t, d_dent, FNX_DIRHNDENT);
	staticassert_eqarrsz(fnx_ddir_t, d_segs, FNX_DIRNSEGS / 32);
	staticassert_eqarrsz(fnx_ddirseg_t, ds_dent, FNX_DSEGNDENT);
	staticassert_eqarrsz(fnx_dspmap_t, sm_vref, FNX_BCKTNVREF);
	staticassert_eqarrsz(fnx_dspmap_t, sm_space, FNX_BCKTNBK);
	staticassert_eqmemsz(fnx_dspmap_t, sm_space[0], FNX_BLKNFRG / 8);
	staticassert_eqarrsz(fnx_dbucket_t, bt_blks, FNX_BCKTNBK);
	staticassert_eqarrsz(fnx_dbucket_t, bt_frgs, FNX_BCKTNFRG);
	staticassert_eqarrsz(fnx_dbucket_t, bt_frgs, FNX_BCKTNFRG);
	staticassert_eqarrsz(fnx_dreg_t, r_rsegs, FNX_RSECNSEG / 32);
	staticassert_eqarrsz(fnx_dreg_t, r_rsecs, FNX_REGNSEC / 32);
	staticassert_eqarrsz2(fnx_dreg_t, r_rsegs, fnx_dregsec_t, rc_rsegs);
}

static void verify_elems(void)
{
	const fnx_spmap_t *spmap = NULL;

	staticassert_eqarrsz2(fnx_iobuf_t, bks, fnx_segmap_t, vba);
	staticassert_eqarrsz2(fnx_reg_t, r_segmap0.vba, fnx_dreg_t, r_vlba_rseg0);
	staticassert_eqarrsz2(fnx_reg_t, r_rsegs, fnx_dreg_t, r_rsegs);
	staticassert_eqarrsz2(fnx_reg_t, r_rsecs, fnx_dreg_t, r_rsecs);
	staticassert_eqarrsz2(fnx_regsec_t, rc_rsegs, fnx_dregsec_t, rc_rsegs);
	staticassert_eqarrsz2(fnx_dir_t, d_segs, fnx_ddir_t, d_segs);
	staticassert_eqarrsz2(fnx_dir_t, d_dent, fnx_ddir_t, d_dent);
	staticassert_eqarrsz2(fnx_dirseg_t, ds_dent, fnx_ddirseg_t, ds_dent);

	/* Space-mapping verifications */
	staticassert_eq(8 * sizeof(spmap->sm_space[0]), FNX_BLKNFRG);
	staticassert_eq(FNX_BLKNFRG, 16);
}

static void verify_mntf(void)
{
	staticassert_eq(FNX_MNTF_RDONLY, MS_RDONLY);
	staticassert_eq(FNX_MNTF_NOSUID, MS_NOSUID);
	staticassert_eq(FNX_MNTF_NODEV, MS_NODEV);
	staticassert_eq(FNX_MNTF_NOEXEC, MS_NOEXEC);
	staticassert_eq(FNX_MNTF_MANDLOCK, MS_MANDLOCK);
	staticassert_eq(FNX_MNTF_DIRSYNC, MS_DIRSYNC);
	staticassert_eq(FNX_MNTF_NOATIME, MS_NOATIME);
	staticassert_eq(FNX_MNTF_NODIRATIME, MS_NODIRATIME);
	staticassert_eq(FNX_MNTF_RELATIME, MS_RELATIME);
}

static void verify_capf(void)
{
	check_capf(FNX_CAPF_CHOWN, CAP_CHOWN);
	check_capf(FNX_CAPF_FOWNER, CAP_FOWNER);
	check_capf(FNX_CAPF_FSETID, CAP_FSETID);
	check_capf(FNX_CAPF_ADMIN, CAP_SYS_ADMIN);
	check_capf(FNX_CAPF_MKNOD, CAP_MKNOD);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int verify_sys_defs(void)
{
	long page_size, blk_size = FNX_BLKSIZE;

	page_size = sysconf(_SC_PAGE_SIZE);
	if (page_size <= 0) {
		fnx_critical("sysconf(_SC_PAGE_SIZE)=%ld", page_size);
		return -1;
	}
	if ((blk_size <= page_size) || ((blk_size % page_size) != 0)) {
		fnx_critical("page_size=%ld blk_size=%ld", page_size, blk_size);
		return -1;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_verify_core(void)
{
	/* Static-asserts verifications */
	verify_defs();
	verify_types();
	verify_dobjs();
	verify_dmembers();
	verify_elems();
	verify_mntf();
	verify_capf();

	/* Dynamic-sizes verifications */
	return verify_sys_defs();
}
