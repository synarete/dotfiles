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
#include <fnxcore.h>
#include <fnxvproc.h>
#include <fnxfusei.h>
#include "server-exec.h"


static fnx_server_t *server_of_inode(const fnx_inode_t *inode)
{
	const fnx_server_t *server;

	server = (const fnx_server_t *)inode->i_super->su_private;
	fnx_assert(server->magic == FNX_SERVER_MAGIC);
	return (fnx_server_t *)server;
}

static fnx_super_t *getsuper(const fnx_reg_t *reg)
{
	return reg->r_inode.i_super;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int show_nodata(fnx_reg_t *reg, void *p, size_t n, size_t *sz)
{
	fnx_unused(reg);
	fnx_bzero(p, n);
	*sz = 0;
	return 0;
}

static int save_notsup(fnx_reg_t *reg, const void *blk, size_t sz)
{
	fnx_unused3(reg, blk, sz);
	return -ENOTSUP;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int save_halt(fnx_reg_t *reg, const void *blk, size_t sz)
{
	int rc, nn;
	fnx_substr_t ss;
	fnx_super_t *super = getsuper(reg);

	fnx_substr_init_rd(&ss, (const char *)blk, sz);
	rc = fnx_parse_param(&ss, &nn);
	if ((rc == 0) && (nn == 0)) {
		super->su_active = FNX_FALSE;
	} else if ((rc == 0) && (nn == 1)) {
		super->su_active = FNX_TRUE;
	} else {
		rc = -EINVAL;
	}
	return rc;
}

static int show_halt(fnx_reg_t *reg, void *p, size_t n, size_t *sz)
{
	fnx_substr_t ss;
	const fnx_super_t *super = getsuper(reg);

	fnx_substr_init_rw(&ss, (char *)p, 0, n);
	fnx_repr_intval(&ss, super->su_active);
	*sz = ss.len;
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int show_uuid(fnx_reg_t *reg, void *p, size_t n, size_t *sz)
{
	fnx_substr_t ss;
	const fnx_super_t *super = getsuper(reg);

	fnx_substr_init_rw(&ss, (char *)p, 0, n);
	fnx_repr_uuid(&ss, &super->su_fsinfo.fs_attr.f_uuid);
	*sz = ss.len;
	return 0;
}

static int show_fsstat(fnx_reg_t *reg, void *p, size_t n, size_t *sz)
{
	fnx_substr_t ss;
	const fnx_super_t *super = getsuper(reg);

	fnx_substr_init_rw(&ss, (char *)p, 0, n);
	fnx_repr_fsstat(&ss, &super->su_fsinfo.fs_stat, NULL);
	*sz = ss.len;
	return 0;
}

static int show_opstat(fnx_reg_t *reg, void *p, size_t n, size_t *sz)
{
	fnx_substr_t ss;
	const fnx_super_t *super = getsuper(reg);

	fnx_substr_init_rw(&ss, (char *)p, 0, n);
	fnx_repr_opstat(&ss, &super->su_fsinfo.fs_oper, NULL);
	*sz = ss.len;
	return 0;
}

static int show_iostat(fnx_reg_t *reg, void *p, size_t n, size_t *sz)
{
	fnx_substr_t ss;
	const fnx_super_t *super = getsuper(reg);

	fnx_substr_init_rw(&ss, (char *)p, 0, n);
	fnx_repr_iostat(&ss, &super->su_fsinfo.fs_rdst, "rd");
	fnx_repr_iostat(&ss, &super->su_fsinfo.fs_wrst, "wr");
	*sz = ss.len;
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int show_cache_cstats(fnx_reg_t *reg, void *p, size_t n, size_t *sz)
{
	fnx_substr_t ss;
	fnx_cstats_t cstats;
	const fnx_server_t *server;

	server = server_of_inode(&reg->r_inode);
	fnx_server_getcstats(server, &cstats);

	fnx_substr_init_rw(&ss, (char *)p, 0, n);
	fnx_repr_cstats(&ss, &cstats, NULL);
	*sz = ss.len;
	return 0;
}

static int show_cache_alloc_nbk(fnx_reg_t *reg, void *p, size_t n, size_t *sz)
{
	size_t total;
	fnx_substr_t ss;
	const fnx_server_t *server;

	server = server_of_inode(&reg->r_inode);
	total = fnx_alloc_total_nbk(&server->alloc);

	fnx_substr_init_rw(&ss, (char *)p, 0, n);
	fnx_repr_intval(&ss, (long)total);
	*sz = ss.len;
	return 0;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int save_attr_timeout(fnx_reg_t *reg, const void *blk, size_t sz)
{
	int rc, nn;
	fnx_substr_t ss;
	fnx_server_t *server;

	server = server_of_inode(&reg->r_inode);
	fnx_substr_init_rd(&ss, (const char *)blk, sz);
	rc = fnx_parse_param(&ss, &nn);
	if ((rc == 0) && (nn >= 0) && (nn < 10)) {
		server->fusei->fi_info.attr_timeout = (unsigned)nn;
	} else {
		rc = -EINVAL;
	}
	return rc;
}

static int show_attr_timeout(fnx_reg_t *reg, void *p, size_t n, size_t *sz)
{
	fnx_substr_t ss;
	const fnx_server_t *server;

	server = server_of_inode(&reg->r_inode);
	fnx_substr_init_rw(&ss, (char *)p, 0, n);
	fnx_repr_intval(&ss, (long)server->fusei->fi_info.attr_timeout);
	*sz = ss.len;
	return 0;
}

static int save_entry_timeout(fnx_reg_t *reg, const void *blk, size_t sz)
{
	int rc, nn;
	fnx_substr_t ss;
	fnx_server_t *server;

	server = server_of_inode(&reg->r_inode);
	fnx_substr_init_rd(&ss, (const char *)blk, sz);
	rc = fnx_parse_param(&ss, &nn);
	if ((rc == 0) && (nn >= 0) && (nn < 10)) {
		server->fusei->fi_info.entry_timeout = (unsigned)nn;
	} else {
		rc = -EINVAL;
	}
	return rc;
}

static int show_entry_timeout(fnx_reg_t *reg, void *p, size_t n, size_t *sz)
{
	fnx_substr_t ss;
	const fnx_server_t *server;

	server = server_of_inode(&reg->r_inode);
	fnx_substr_init_rw(&ss, (char *)p, 0, n);
	fnx_repr_intval(&ss, (long)server->fusei->fi_info.entry_timeout);
	*sz = ss.len;
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int save_log_trace(fnx_reg_t *reg, const void *blk, size_t sz)
{
	int rc, nn = 0;
	fnx_substr_t ss;

	fnx_substr_init_rd(&ss, (const char *)blk, sz);
	rc = fnx_parse_param(&ss, &nn);
	nn = -abs(nn);
	if ((rc == 0) && (nn <= 0) && (nn >= FNX_LL_TRACE3)) {
		fnx_default_logger->log_trace = nn;
	} else {
		rc = -EINVAL;
	}
	fnx_unused(reg);
	return rc;
}

static int show_log_trace(fnx_reg_t *reg, void *p, size_t n, size_t *sz)
{
	fnx_substr_t ss;

	fnx_substr_init_rw(&ss, (char *)p, 0, n);
	fnx_repr_intval(&ss, labs(fnx_default_logger->log_trace));
	*sz = ss.len;
	fnx_unused(reg);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/


/* Meta information & ops associated with pseudo-inodes */
struct fnx_imeta {
	const char *name;
	fnx_mode_t  mode;
	int (*show)(fnx_reg_t *, void *, size_t, size_t *);
	int (*save)(fnx_reg_t *, const void *, size_t);
};
typedef struct fnx_imeta fnx_imeta_t;


static const fnx_imeta_t s_imeta_ioctl = {
	.name = FNX_PSIOCTLNAME,
	.mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
	.show = show_nodata,
	.save = save_notsup
};

static const fnx_imeta_t s_imeta_halt = {
	.name = FNX_PSHALTNAME,
	.mode = S_IRUSR | S_IWUSR | S_IRGRP,
	.show = show_halt,
	.save = save_halt
};

static const fnx_imeta_t s_imeta_uuid = {
	.name = "uuid",
	.mode = S_IRUSR | S_IRGRP,
	.show = show_uuid,
	.save = save_notsup
};

static const fnx_imeta_t s_imeta_fsstat = {
	.name = FNX_PSFSSTATNAME,
	.mode = S_IRUSR | S_IRGRP,
	.show = show_fsstat,
	.save = save_notsup
};

static const fnx_imeta_t s_imeta_opstat = {
	.name = FNX_PSOPSTATSNAME,
	.mode = S_IRUSR | S_IRGRP,
	.show = show_opstat,
	.save = save_notsup
};

static const fnx_imeta_t s_imeta_iostat = {
	.name = FNX_PSIOSTATSNAME,
	.mode = S_IRUSR | S_IRGRP,
	.show = show_iostat,
	.save = save_notsup
};

static const fnx_imeta_t s_imeta_cache_cstats = {
	.name = FNX_PSCSTATSNAME,
	.mode = S_IRUSR | S_IRGRP,
	.show = show_cache_cstats,
	.save = save_notsup
};

static const fnx_imeta_t s_imeta_cache_alloc_nbk = {
	.name = "alloc_nbk",
	.mode = S_IRUSR | S_IRGRP,
	.show = show_cache_alloc_nbk,
	.save = save_notsup
};

static const fnx_imeta_t s_imeta_attr_timeout = {
	.name = FNX_PSATTRTIMEOUTNAME,
	.mode = S_IRUSR | S_IWUSR | S_IRGRP,
	.save = save_attr_timeout,
	.show = show_attr_timeout
};

static const fnx_imeta_t s_imeta_entry_timeout = {
	.name = FNX_PSENTRYTIMEOUTNAME,
	.mode = S_IRUSR | S_IWUSR | S_IRGRP,
	.save = save_entry_timeout,
	.show = show_entry_timeout
};

static const fnx_imeta_t s_imeta_log_trace = {
	.name = "debug",
	.mode = S_IRUSR | S_IWUSR | S_IRGRP,
	.save = save_log_trace,
	.show = show_log_trace
};

/*: : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : :*/

static fnx_ino_t next_pseudo_ino(fnx_vproc_t *vproc, fnx_vtype_e vtype)
{
	return fnx_ino_mkpseudo(vproc->pino++, vtype);
}

static fnx_inode_t *new_pseudo_inode(fnx_vproc_t *vproc, fnx_vtype_e vtype)
{
	fnx_ino_t pino;
	fnx_vnode_t *vnode = NULL;
	fnx_inode_t *inode = NULL;

	vnode = fnx_alloc_new_vobj(vproc->alloc, vtype);
	if (vnode == NULL) {
		fnx_panic("no-new-pseudo-inode: vtype=%d", vtype);
	}
	pino  = next_pseudo_ino(vproc, vtype);
	inode = fnx_vnode_to_inode(vnode);
	fnx_inode_setino(inode, pino);
	inode->i_super = vproc->super;
	inode->i_vnode.v_pseudo = FNX_TRUE;
	inode->i_vnode.v_pinned = FNX_TRUE;
	fnx_vcache_store_vnode(&vproc->cache->vc, vnode);
	return inode;
}

static fnx_dir_t *new_dir(fnx_vproc_t *vproc)
{
	fnx_mode_t mode;
	fnx_dir_t   *dir;
	fnx_inode_t *inode;
	const fnx_vtype_e vtype = FNX_VTYPE_DIR;

	inode = new_pseudo_inode(vproc, vtype);
	dir   = fnx_inode_to_dir(inode);
	mode  = fnx_vtype_to_crmode(vtype, inode->i_iattr.i_mode, 0);
	fnx_dir_setup(dir, &inode->i_super->su_uctx, mode);
	inode->i_iattr.i_mode |= (S_IRGRP | S_IXGRP);

	return dir;
}

static fnx_inode_t *new_reg(fnx_vproc_t *vproc, const fnx_imeta_t *meta)
{
	fnx_mode_t mode;
	fnx_inode_t *inode;
	const fnx_vtype_e vtype = FNX_VTYPE_REG;

	inode = new_pseudo_inode(vproc, vtype);
	mode  = fnx_vtype_to_crmode(vtype, meta->mode, 0);
	fnx_inode_setup(inode, &inode->i_super->su_uctx, mode, 0);
	fnx_inode_setsize(inode, sizeof(vproc->pblk));
	inode->i_meta = meta;

	return inode;
}

static void linkp(fnx_vproc_t *vproc, fnx_dir_t *parentd,
                  const char *str, fnx_inode_t *inode)
{
	int rc;
	fnx_name_t name;

	fnx_name_setup(&name, str);
	if (parentd != NULL) {
		rc = fnx_vproc_link_child(vproc, parentd, inode, &name);
		if (rc != 0) {
			fnx_panic("link-pseudo-failed: name=%s", name.str);
		}
	} else {
		fnx_inode_associate(inode, FNX_INO_ROOT, &name);
	}
}

static void linkp2(fnx_vproc_t *vproc, fnx_dir_t *parentd, fnx_inode_t *inode)
{
	const fnx_imeta_t *imeta = (const fnx_imeta_t *)inode->i_meta;
	linkp(vproc, parentd, imeta->name, inode);
}

static fnx_dir_t *mkpdir(fnx_vproc_t *vproc, fnx_dir_t *parentd,
                         const char *name)
{
	fnx_dir_t *dir;

	dir = new_dir(vproc);
	linkp(vproc, parentd, name, &dir->d_inode);
	return dir;
}

static void mkpreg(fnx_vproc_t *vproc, fnx_dir_t *parentd,
                   const fnx_imeta_t *meta)
{
	fnx_inode_t *reg;

	reg = new_reg(vproc, meta);
	linkp2(vproc, parentd, reg);
}


static void genpns(fnx_vproc_t *vproc)
{
	fnx_dir_t *parentd, *rootd;

	vproc->proot = rootd = mkpdir(vproc, NULL, FNX_PSROOTNAME);
	mkpreg(vproc, rootd, &s_imeta_ioctl);
	mkpreg(vproc, rootd, &s_imeta_halt);
	mkpreg(vproc, rootd, &s_imeta_uuid);
	parentd = mkpdir(vproc, rootd, FNX_PSSUPERDNAME);
	mkpreg(vproc, parentd, &s_imeta_fsstat);
	mkpreg(vproc, parentd, &s_imeta_iostat);
	mkpreg(vproc, parentd, &s_imeta_opstat);
	parentd = mkpdir(vproc, rootd, FNX_PSCACHEDNAME);
	mkpreg(vproc, parentd, &s_imeta_cache_cstats);
	mkpreg(vproc, parentd, &s_imeta_cache_alloc_nbk);
	parentd = mkpdir(vproc, rootd, FNX_PSFUSEIDNAME);
	mkpreg(vproc, parentd, &s_imeta_attr_timeout);
	mkpreg(vproc, parentd, &s_imeta_entry_timeout);
	parentd = mkpdir(vproc, rootd, "logger");
	mkpreg(vproc, parentd, &s_imeta_log_trace);
}

static void tiepns(fnx_vproc_t *vproc)
{
	fnx_dir_t *proot = vproc->proot;
	fnx_dir_t *rootd = vproc->rootd;

	proot->d_dent[FNX_DOFF_PARENT].de_ino = fnx_dir_getino(rootd);
}

void fnx_server_bind_pseudo_ns(fnx_server_t *server)
{
	fnx_info("bind-pseudo-namespace: %s", ""); /* TODO: mount-point? */
	genpns(&server->vproc);
	tiepns(&server->vproc);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_vproc_read_pseudo(fnx_vproc_t *vproc, fnx_task_t *task,
                          fnx_off_t off, fnx_size_t len)
{
	int rc;
	fnx_size_t sz = 0;
	fnx_off_t  end;
	fnx_size_t cnt;
	fnx_blk_t *blk;
	fnx_reg_t *reg;
	const fnx_imeta_t *imeta;
	const fnx_fileref_t *fref = task->tsk_fref;
	fnx_iobufs_t *iobufs = &task->tsk_iobufs;

	reg  = fnx_inode_to_reg(fref->f_inode);
	imeta = reg->r_inode.i_meta;
	if (!imeta || !imeta->show) {
		return -ENOTSUP;
	}
	blk = &vproc->pblk;
	rc  = imeta->show(reg, blk, sizeof(*blk), &sz);
	if (rc != 0) {
		return rc;
	}
	end = fnx_off_min(fnx_off_end(off, len), fnx_off_end(0, sz));
	cnt = (end > off) ? fnx_off_len(off, end) : 0;
	rc = fnx_iobufs_assign(iobufs, off, cnt, blk + off);
	if (rc != 0) {
		return rc;
	}
	fnx_iobufs_increfs(iobufs);
	fnx_inode_setitime(&reg->r_inode, FNX_ATIME_NOW);
	return 0;
}

int fnx_vproc_write_pseudo(fnx_vproc_t *vproc, fnx_task_t *task,
                           fnx_off_t off, fnx_size_t len)
{
	int rc;
	fnx_size_t dsz, sz = 0;
	fnx_flags_t flags;
	fnx_iovec_t iov[1];
	fnx_reg_t *reg;
	const char *dat;
	const fnx_imeta_t *imeta;
	const fnx_fileref_t *fref = task->tsk_fref;

	reg  = fnx_inode_to_reg(fref->f_inode);
	imeta = reg->r_inode.i_meta;
	if (!imeta || !imeta->save) {
		return -ENOTSUP;
	}
	if ((off != 0) || (len > sizeof(vproc->pblk))) {
		return -EFBIG;
	}
	rc = fnx_iobufs_mkiovec(&task->tsk_iobufs, iov, 1, &sz);
	if ((rc != 0) || (sz != 1)) {
		return -EINVAL;
	}
	dat = (const char *)iov[0].iov_base;
	dsz = fnx_min(iov[0].iov_len, strlen(dat));
	rc  = imeta->save(reg, dat, dsz);
	if (rc != 0) {
		return rc;
	}
	flags = (len > 0) ? FNX_AMCTIME_NOW : FNX_ATIME_NOW;
	fnx_inode_setitime(&reg->r_inode, flags);
	return 0;
}

int fnx_vproc_trunc_pseudo(fnx_vproc_t *vproc, fnx_task_t *task, fnx_off_t off)
{
	fnx_reg_t *reg;
	fnx_flags_t flags;
	const fnx_fileref_t *fref = task->tsk_fref;

	reg = fnx_inode_to_reg(fref->f_inode);
	flags = FNX_AMCTIME_NOW;
	fnx_inode_setitime(&reg->r_inode, flags);
	fnx_unused2(vproc, off);
	return 0;
}

int fnx_vproc_punch_pseudo(fnx_vproc_t *vproc, fnx_task_t *task,
                           fnx_off_t off, fnx_size_t len)
{
	fnx_reg_t *reg;
	fnx_flags_t flags;
	const fnx_fileref_t *fref = task->tsk_fref;

	reg = fnx_inode_to_reg(fref->f_inode);
	flags = (len > 0) ? FNX_AMCTIME_NOW : FNX_ATIME_NOW;
	fnx_inode_setitime(&reg->r_inode, flags);
	fnx_unused2(vproc, off);
	return 0;
}


