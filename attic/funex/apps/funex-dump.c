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
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "funex-app.h"

#define globals (funex_globals)


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Locals forward declarations: */
static void funex_dump_parse_args(int argc, char *argv[]);
static void funex_dump_execute(void);

static int s_dump_defs = 0;
static int s_dump_sizes = 0;
static const char *s_dump_volume = NULL;


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                           *
 *                                                                           *
 *                         The Funex File-System                             *
 *                                                                           *
 *             Dump file-system's meta-data (developer's utility)            *
 *                                                                           *
 *                                                                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
int main(int argc, char *argv[])
{
	/* Begin with defaults */
	funex_init_process(argc, argv);

	/* Parse command-line args */
	funex_dump_parse_args(argc, argv);

	/* Enable logging */
	funex_setup_plogger();

	/* Execute sub-command */
	funex_dump_execute();

	/* Cleanups and Goodbye :) */
	funex_finalize_exit();

	return EXIT_SUCCESS;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

#define print_sizeof(t)     fnx_info("sizeof(%s) = %zu", #t, sizeof(t))
#define print_strdef(s)     fnx_info("%s = %s", #s, s)
#define print_constdef(d)   fnx_info("%s = %ld", #d, (long)(d))
#define print_delim(s)      fnx_info("%s", s)

#define print_globalstr(k)  \
	if (globals.k != NULL) \
	{ fnx_info("%s = %s", FNX_MAKESTR(k), globals.k); }


static void funex_dump_globals(void)
{
	print_globalstr(mntd.usock);
	print_globalstr(proc.cwd);
	print_globalstr(sys.username);
	print_globalstr(sys.selfexe);
	print_globalstr(sys.confdir);
	print_globalstr(sys.nodename);
	print_delim("");
}

static void funex_dump_defs(void)
{
	/* Global defs */
	print_strdef(FNX_FSNAME);
	print_constdef(FNX_FSMAGIC);
	print_constdef(FNX_FSVERSION);
	print_constdef(FNX_FRGSIZE);
	print_constdef(FNX_BLKSIZE);
	print_constdef(FNX_BCKTSIZE);
	print_delim("");

	/* Directory defs */
	print_constdef(FNX_DIRHNDENT);
	print_constdef(FNX_DIRNSEGS);
	print_constdef(FNX_DSEGNDENT);
	print_constdef(FNX_DOFF_NONE);
	print_constdef(FNX_DOFF_SELF);
	print_constdef(FNX_DOFF_PARENT);
	print_constdef(FNX_DOFF_END);
	print_constdef(FNX_DOFF_PROOT);
	print_delim("");

	/* Regular-file defs */
	print_constdef(FNX_RSEGSIZE);
	print_constdef(FNX_RSEGNBK);
	print_constdef(FNX_RSEGNFRG);
	print_constdef(FNX_RSECSIZE);
	print_constdef(FNX_RSECNSEG);
	print_constdef(FNX_RSECNBK);
	print_constdef(FNX_REGNSEC);
	print_constdef(FNX_REGNSEG);
	print_constdef(FNX_ROFF_BEGIN);
	print_constdef(FNX_ROFF_END);
	print_delim("");

	/* Limits */
	print_constdef(FNX_NAME_MAX);
	print_constdef(FNX_PATH_MAX);
	print_constdef(FNX_LINK_MAX);
	print_constdef(FNX_USERS_MAX);
	print_constdef(FNX_GROUPS_MAX);
	print_constdef(FNX_OPENF_MAX);
	print_constdef(FNX_SYMLNK1_MAX);
	print_constdef(FNX_SYMLNK2_MAX);
	print_constdef(FNX_SYMLNK3_MAX);
	print_constdef(FNX_REGSIZE_MAX);
	print_constdef(FNX_DIRCHILD_MAX);
	print_constdef(FNX_VOLSIZE_MIN);
	print_constdef(FNX_VOLSIZE_MAX);
	print_delim("");
}

static void funex_dump_sizeofs(void)
{
	/* Objects sizeof */
	print_sizeof(fnx_fileref_t);
	print_sizeof(fnx_jelem_t);
	print_sizeof(fnx_bkref_t);
	print_sizeof(fnx_iattr_t);
	print_sizeof(fnx_spmap_t);
	print_sizeof(fnx_fsattr_t);
	print_sizeof(fnx_fsstat_t);
	print_sizeof(fnx_opstat_t);
	print_sizeof(fnx_iostat_t);
	print_sizeof(fnx_fsinfo_t);
	print_sizeof(fnx_super_t);
	print_sizeof(fnx_vnode_t);
	print_sizeof(fnx_inode_t);
	print_sizeof(fnx_dir_t);
	print_sizeof(fnx_dirseg_t);
	print_sizeof(fnx_symlnk_t);
	print_sizeof(fnx_reg_t);
	print_sizeof(fnx_regsec_t);
	print_sizeof(fnx_regseg_t);
	print_sizeof(fnx_segmap_t);
	print_sizeof(fnx_task_t);
	print_sizeof(fnx_alloc_t);
	print_sizeof(fnx_ucache_t);
	print_sizeof(fnx_vcache_t);
	print_sizeof(fnx_cache_t);
	print_sizeof(fnx_frpool_t);
	print_sizeof(fnx_pstor_t);
	print_sizeof(fnx_vproc_t);
	print_sizeof(fnx_fusei_t);
	print_sizeof(fnx_server_t);
	print_sizeof(funex_globals_t);
	print_delim("");
}

static void funex_dump_dsizeofs(void)
{
	/* Blobs sizeofs */
	print_sizeof(fnx_header_t);
	print_sizeof(fnx_dtimes_t);
	print_sizeof(fnx_dfsattr_t);
	print_sizeof(fnx_dfsstat_t);
	print_sizeof(fnx_diattr_t);
	print_sizeof(fnx_dvref_t);
	print_sizeof(fnx_dspmap_t);
	print_sizeof(fnx_dsuper_t);
	print_sizeof(fnx_dinode_t);
	print_sizeof(fnx_ddir_t);
	print_sizeof(fnx_ddirseg_t);
	print_sizeof(fnx_dsymlnk1_t);
	print_sizeof(fnx_dsymlnk2_t);
	print_sizeof(fnx_dsymlnk3_t);
	print_sizeof(fnx_dregsec_t);
	print_sizeof(fnx_dregseg_t);
	print_delim("");
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_dump_prepare(void)
{
	int rc;
	const char *path = s_dump_volume;

	rc = fnx_sys_statvol(path, NULL, R_OK);
	if (rc != 0) {
		if (rc == -ENOTBLK) {
			funex_dief("not-reg-or-blk: %s", path);
		} else if (rc == -EACCES) {
			funex_dief("no-access: %s", path);
		} else {
			funex_dief("illegal: %s err=%d", path, rc);
		}
	}
}

static void funex_dump_load_super(fnx_bdev_t *bdev, fnx_super_t *super)
{
	fnx_baddr_t baddr;
	const fnx_dblk_t *dblk;

	fnx_baddr_for_super(&baddr);
	dblk = funex_get_dblk(bdev, baddr.lba);
	fnx_import_vobj(&super->su_vnode, &dblk->bk_hdr);
}

static void funex_dump_metadata(void)
{
	fnx_bdev_t  *bdev;
	fnx_super_t *super;

	bdev  = funex_open_bdev(s_dump_volume, FNX_BDEV_RDONLY);
	super = fnx_vnode_to_super(funex_newvobj(FNX_VTYPE_SUPER));

	funex_dump_load_super(bdev, super);
	funex_print_super(super, "super");

	funex_delvobj(&super->su_vnode);
	funex_close_bdev(bdev);
}

static void funex_dump_volume(void)
{
	funex_dump_prepare();
	funex_dump_metadata();
}
/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_dump_execute(void)
{
	if (s_dump_defs) {
		funex_dump_globals();
		funex_dump_defs();
	}
	if (s_dump_sizes) {
		funex_dump_sizeofs();
		funex_dump_dsizeofs();
	}
	if (s_dump_volume != NULL) {
		funex_dump_volume();
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void funex_dump_getopts(funex_getopts_t *opt)
{
	int c;
	const char *arg;

	while ((c = funex_getnextopt(opt)) != 0) {
		switch (c) {
			case 'd':
				s_dump_defs = 1;
				break;
			case 's':
				s_dump_sizes = 1;
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
	arg = funex_getnextarg(opt, NULL, FUNEX_ARG_OPT | FUNEX_ARG_LAST);
	s_dump_volume = arg;
}

static const funex_cmdent_t funex_dump_cmdent = {
	.usage  = "funex-dump [<volume>]",
	.optdef = "h,help v,version d,defs s,sizes", \
	.helps  = \
	"Dump volume's file-system meta-data info \n\n" \
	"Options: \n" \
	"  -d, --defs             Show common defs values \n" \
	"  -s, --sizes            Show internals sizes \n" \
	"  -v, --version          Show version info \n" \
	"  -h, --help             Show this help \n"
};

static void funex_dump_parse_args(int argc, char *argv[])
{
	funex_parse_cmdargs(argc, argv, &funex_dump_cmdent, funex_dump_getopts);
}


