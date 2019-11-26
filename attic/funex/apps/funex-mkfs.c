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
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "funex-app.h"

#define globals (funex_globals)


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Locals */
static void funex_mkfs_parse_args(int argc, char *argv[]);
static void funex_mkfs_execute(void);



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                           *
 *                                                                           *
 *                         The Funex File-System                             *
 *                                                                           *
 *                  Mkfs-utility: foramt file-system layout                  *
 *                                                                           *
 *                                                                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
int main(int argc, char *argv[])
{
	/* Begin with defaults */
	funex_init_process(argc, argv);

	/* Parse command-line args */
	funex_mkfs_parse_args(argc, argv);

	/* Enable logging */
	funex_setup_plogger();

	/* Execute command */
	funex_mkfs_execute();

	/* Cleanups and Goodbye :) */
	funex_finalize_exit();

	return EXIT_SUCCESS;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_mkfs_prepare(void)
{
	int rc;
	off_t sz = 0;

	globals.fs.uid = getuid();
	globals.fs.gid = getgid();
	fnx_times_fill(&globals.fs.tms, FNX_BAMCTIME_NOW, NULL);
	if (fnx_uuid_isnull(&globals.fs.uuid)) {
		fnx_uuid_generate(&globals.fs.uuid);
	}
	if (globals.vol.bkcnt == 0) {
		rc = fnx_sys_statsz(globals.vol.path, NULL, &sz);
		if (rc != 0) {
			funex_dief("can-not-stat-size: path=%s", globals.vol.path);
		}
		globals.vol.bkcnt = (blkcnt_t)fnx_volsz_to_bkcnt(sz);
	}
}

static void funex_mkfs_chkconf(void)
{
	int rc;
	const fnx_bkcnt_t nbk_vol = (fnx_bkcnt_t)globals.vol.bkcnt;
	const fnx_bkcnt_t nbk_min = fnx_bytes_to_nbk(FNX_VOLSIZE_MIN);
	const fnx_bkcnt_t nbk_max = fnx_bytes_to_nbk(FNX_VOLSIZE_MAX);

	if (!nbk_vol && ((rc = fnx_sys_statblk(globals.vol.path, NULL)) != 0)) {
		funex_dief("missing vol-size: %s", globals.vol.path);
	}
	if ((nbk_vol < nbk_min) || (nbk_max < nbk_vol)) {
		funex_die_illegal_volsize(NULL, (loff_t)nbk_vol * FNX_BLKSIZE);
	}
}

/*
 * Protect user from overriding existing volume, unless explicitly required
 * to do so with '--force' (or no-op if volume does-not exist).
 */
static void funex_mkfs_protect(void)
{
	int rc, has_super;
	fnx_bdev_t *bdev;
	const fnx_dblk_t *dblk;
	const fnx_vinfo_t *vinfo;
	struct stat st;
	const char *path = globals.vol.path;

	if (globals.opt.force) {
		return;
	}
	if ((rc = fnx_sys_stat(path, &st)) != 0) {
		return;
	}
	if (S_ISREG(st.st_mode) && (st.st_size < (off_t)FNX_BCKTSIZE)) {
		return;
	}

	/* Ensure no super-block exists */
	bdev = funex_open_bdev(path, FNX_BDEV_RDONLY);
	dblk = funex_get_dblk(bdev, FNX_LBA_SUPER);
	vinfo = fnx_get_vinfo(FNX_VTYPE_SUPER);
	rc = vinfo->dcheck(&dblk->bk_hdr);
	has_super = (rc == 0);
	funex_close_bdev(bdev);
	if (has_super) {
		funex_dief("operation may destroy existing data: %s", path);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_mkfs_setup_super(fnx_super_t *super)
{
	fnx_fsinfo_t *fsinfo        = &super->su_fsinfo;
	const char *fsname          = globals.fs.name;
	const char *version         = funex_version();
	const fnx_bkcnt_t bkcnt     = (fnx_bkcnt_t)globals.vol.bkcnt;
	const fnx_size_t  volsz     = fnx_nbk_to_nbytes(bkcnt);

	fnx_super_setup(super, fsname ? fsname : FNX_FSNAME, version);
	fnx_super_settimes(super, &globals.fs.tms, FNX_BAMCTIME);
	fnx_super_devise_fs(super, volsz);
	fnx_fsattr_setup(&fsinfo->fs_attr, &globals.fs.uuid,
	                 globals.fs.uid, globals.fs.gid);
	fsinfo->fs_attr.f_gen   = 1;
}

static void funex_mkfs_setup_rootd(fnx_dir_t *rootd, const fnx_super_t *super)
{
	fnx_ino_t  dino;
	fnx_uctx_t uctx;
	fnx_iattr_t *iattr = &rootd->d_inode.i_iattr;

	fnx_uctx_init(&uctx);
	uctx.u_root = 1;
	uctx.u_capf = FNX_CAPF_ALL;
	uctx.u_cred.cr_uid   = globals.uid;
	uctx.u_cred.cr_gid   = globals.gid;
	uctx.u_cred.cr_umask = globals.umsk;
	uctx.u_cred.cr_ngids = 0;

	dino = super->su_fsinfo.fs_attr.f_rootino;
	fnx_inode_setino(&rootd->d_inode, dino);
	rootd->d_rootd = FNX_TRUE;
	fnx_dir_setup(rootd, &uctx, 0755);

	iattr->i_uid = globals.fs.uid;
	iattr->i_gid = globals.fs.gid;
	fnx_times_copy(&iattr->i_times, &globals.fs.tms);
}

static void funex_mkfs_reset_superbckt(fnx_bdev_t *bdev)
{
	fnx_lba_t lba;
	fnx_dblk_t *dblk;

	for (lba = 1; lba < FNX_BCKTNBK; ++lba) {
		dblk = funex_get_dblk(bdev, lba);
		fnx_bzero(dblk, sizeof(*dblk)); /* TODO: Reset all data? */
	}
}

static void funex_mkfs_place_spmaps(fnx_bdev_t *bdev, const fnx_super_t *super)
{
	fnx_size_t nbckts;
	fnx_dblk_t *dblk;
	fnx_vnode_t *vnode;
	fnx_spmap_t *spmap;

	nbckts = fnx_super_getnbckts(super);
	vnode  = funex_newvobj(FNX_VTYPE_SPMAP);
	spmap  = fnx_vnode_to_spmap(vnode);
	for (fnx_size_t i = 0; i < nbckts; ++i) {
		fnx_spmap_setup2(spmap, i + 1);
		dblk = funex_get_dblk(bdev, spmap->sm_vnode.v_baddr.lba);
		fnx_export_vobj(vnode, &dblk->bk_hdr);
		funex_put_dblk(bdev, dblk, 0);
	}
	funex_delvobj(vnode);
}

static void funex_mkfs_place_super(fnx_bdev_t *bdev, const fnx_super_t *super)
{
	fnx_dblk_t *dblk;

	dblk = funex_get_dblk(bdev, super->su_vnode.v_baddr.lba);
	fnx_export_vobj(&super->su_vnode, &dblk->bk_hdr);
	funex_put_dblk(bdev, dblk, 1);
}

static void funex_mkfs_place_rootd(fnx_bdev_t        *bdev,
                                   const fnx_super_t *super,
                                   const fnx_dir_t   *rootd)
{
	int rc;
	fnx_ino_t ino;
	fnx_vaddr_t vaddr, sm_vaddr;
	fnx_baddr_t baddr, sm_baddr;
	fnx_vnode_t *vnode;
	fnx_spmap_t *spmap;
	fnx_dblk_t  *dblk1, *dblk2;

	ino   = fnx_dir_getino(rootd);
	vnode = funex_newvobj(FNX_VTYPE_SPMAP);
	spmap = fnx_vnode_to_spmap(vnode);

	fnx_vaddr_for_by_ino(&vaddr, ino);
	fnx_super_resolve_sm(super, &vaddr, &sm_vaddr, &sm_baddr);
	dblk1 = funex_get_dblk(bdev, sm_baddr.lba);
	fnx_import_vobj(&spmap->sm_vnode, &dblk1->bk_hdr);
	fnx_spmap_setup(spmap, &sm_vaddr, &sm_baddr, NULL);

	fnx_vaddr_for_dir(&vaddr, ino);
	rc = fnx_spmap_insert(spmap, &vaddr, &baddr);
	if (rc != 0) {
		funex_dief("failed-placing-root-dir: lba=%#lx", baddr.lba);
	}

	dblk2 = funex_get_dblk(bdev, baddr.lba);
	fnx_export_vobj(&rootd->d_inode.i_vnode, &dblk2->bk_hdr);
	funex_put_dblk(bdev, dblk2, 1);
	fnx_export_vobj(&spmap->sm_vnode, &dblk1->bk_hdr);
	funex_put_dblk(bdev, dblk1, 1);

	funex_delvobj(vnode);
}

static void funex_mkfs_sync_bdev(fnx_bdev_t *bdev)
{
	fnx_bdev_mflush(bdev, 1);
}

static void funex_mkfs_reprsize(fnx_size_t sz, char *buf, size_t bsz)
{
	fnx_substr_t ss;

	fnx_bzero(buf, bsz);
	fnx_substr_init_rw(&ss, buf, 0, bsz);
	fnx_repr_fsize(&ss, (double)sz);
}

static void funex_mkfs_show(const fnx_super_t *super, const fnx_dir_t *rootd)
{
	char volumesz[80], availsz[80];
	const fnx_iattr_t  *iattr  = &rootd->d_inode.i_iattr;
	const fnx_fsstat_t *fsstat = &super->su_fsinfo.fs_stat;
	const fnx_bkcnt_t   bfree  = fsstat->f_bfrgs - fsstat->f_bused;
	const fnx_size_t spavail   = fnx_nfrg_to_nbytes(bfree);

	funex_mkfs_reprsize(super->su_volsize, volumesz, sizeof(volumesz));
	funex_mkfs_reprsize(spavail, availsz, sizeof(availsz));

	fnx_info("funex-version:    %s", funex_version());
	fnx_info("volume-path:      %s", globals.vol.path);
	fnx_info("filesystem-uuid:  %s", globals.fs.uuid.str);
	fnx_info("filesystem-name:  %s", super->su_name.str);
	fnx_info("block-size:       %ld", (long)FNX_BLKSIZE);
	fnx_info("bucket-size:      %ldK", (long)(FNX_BCKTSIZE / FNX_KILO));
	fnx_info("num-buckets:      %ld", (long)fnx_super_getnbckts(super));
	fnx_info("volume-size:      %s bytes", volumesz);
	fnx_info("avail-space:      %s bytes", availsz);
	fnx_info("total-blocks:     %lu", fnx_nfrg_to_nbk(fsstat->f_bfrgs));
	fnx_info("total-inodes:     %lu", fsstat->f_inodes);
	fnx_info("rootd-uid:        %ld", (long)iattr->i_uid);
	fnx_info("rootd-gid:        %ld", (long)iattr->i_gid);
}

static void funex_mkfs_apply(void)
{
	fnx_bdev_t  *bdev;
	fnx_super_t *super;
	fnx_dir_t   *rootd;
	fnx_bkcnt_t  bkcnt;

	bkcnt = (fnx_bkcnt_t)globals.vol.bkcnt;
	bdev  = funex_create_bdev(globals.vol.path, bkcnt);
	super = fnx_vnode_to_super(funex_newvobj(FNX_VTYPE_SUPER));
	rootd = fnx_vnode_to_dir(funex_newvobj(FNX_VTYPE_DIR));

	funex_mkfs_setup_super(super);
	funex_mkfs_setup_rootd(rootd, super);

	funex_mkfs_reset_superbckt(bdev);
	funex_mkfs_place_spmaps(bdev, super);
	funex_mkfs_place_super(bdev, super);
	funex_mkfs_place_rootd(bdev, super, rootd);
	funex_mkfs_sync_bdev(bdev);
	funex_mkfs_show(super, rootd);

	funex_delvobj(&rootd->d_inode.i_vnode);
	funex_delvobj(&super->su_vnode);
	funex_close_bdev(bdev);
}

static void funex_mkfs_execute(void)
{
	funex_mkfs_prepare();
	funex_mkfs_chkconf();
	funex_mkfs_protect();
	funex_mkfs_apply();
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_mkfs_getopts(funex_getopts_t *opt)
{
	int c;
	const char *arg = NULL;

	while ((c = funex_getnextopt(opt)) != 0) {
		switch (c) {
			case 'u':
				arg = funex_getoptarg(opt, "uuid");
				funex_globals_set_fsuuid(arg);
				break;
			case 'L':
				arg = funex_getoptarg(opt, "name");
				funex_globals_set_fsname(arg);
				break;
			case 's':
				arg = funex_getoptarg(opt, "size");
				funex_globals_set_volsize(arg);
				break;
			case 'z':
				globals.opt.zfill = FNX_TRUE;
				break;
			case 'F':
				globals.opt.force = FNX_TRUE;
				break;
			case 'd':
				funex_set_debug_level(opt);
				break;
			case 'v':
				funex_show_version_and_goodbye();
				break;
			case 'h':
			default:
				funex_show_cmdhelp_and_goodbye(opt);
				break;
		}
	}
	arg = funex_getnextarg(opt, "volume", FUNEX_ARG_REQLAST);
	funex_clone_str(&globals.vol.path, arg);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const funex_cmdent_t funex_mkfs_cmdent = {
	.usage  = "funex-mkfs [-u UUID] [-L <name>] [-s <size>] <volume>",
	.optdef = \
	"h,help v,version d,debug= u,uuid= L,name= s,size= z,zfill F,force",
	.helps = \
	"Format raw volume (block-device or regular-file) with Funex \n" \
	"file-system layout \n\n" \
	"Options: \n" \
	"  -u, --uuid=<UUID>      File-system's UUID \n" \
	"  -L, --name=<label>     FS label-name \n" \
	"  -s, --size=<bytes>     Volume's calacity in bytes \n" \
	"  -z, --zfill            Fill data-blocks with zeros \n" \
	"  -F, --force            Override existing data \n" \
	"  -d, --debug=<level>    Debug mode level \n"

};

static void funex_mkfs_parse_args(int argc, char *argv[])
{
	funex_parse_cmdargs(argc, argv, &funex_mkfs_cmdent, funex_mkfs_getopts);
}
