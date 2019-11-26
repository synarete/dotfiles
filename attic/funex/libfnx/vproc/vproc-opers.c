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

#include <fnxinfra.h>
#include <fnxcore.h>
#include <fnxpstor.h>
#include "vproc-elems.h"
#include "vproc-exec.h"
#include "vproc-data.h"
#include "vproc-let.h"
#include "vproc-private.h"


#define trace_vop(vproc, fmt, ...) \
	if (vproc->verbose) { \
		fnx_trace1("%s: " fmt, FNX_FUNCTION, __VA_ARGS__); }

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const fnx_uctx_t *task_get_uctx(fnx_task_t *task)
{
	return &task->tsk_uctx;
}

static const fnx_request_t *task_get_request(fnx_task_t *task)
{
	return &task->tsk_request;
}

static fnx_response_t *task_get_response(fnx_task_t *task)
{
	return &task->tsk_response;
}

static fnx_iobufs_t *task_get_iobufs(fnx_task_t *task)
{
	return &task->tsk_iobufs;
}

static fnx_size_t task_get_datalen(const fnx_task_t *task)
{
	return fnx_iobufs_len(&task->tsk_iobufs);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void inc_counter(fnx_size_t *cnt, fnx_size_t n)
{
	*cnt += n;
}

static void vproc_count_read(fnx_vproc_t *vproc, fnx_size_t nbytes)
{
	fnx_fsinfo_t *fsinfo = &vproc->super->su_fsinfo;

	inc_counter(&fsinfo->fs_rdst.io_nbytes, nbytes);
	inc_counter(&fsinfo->fs_rdst.io_nopers, 1);
	/* TODO: update nfrgs, latency & timestp */
}

static void vproc_count_write(fnx_vproc_t *vproc, fnx_size_t nbytes)
{
	fnx_fsinfo_t *fsinfo = &vproc->super->su_fsinfo;

	inc_counter(&fsinfo->fs_wrst.io_nbytes, nbytes);
	inc_counter(&fsinfo->fs_wrst.io_nopers, 1);
	/* TODO: update nfrgs, latency & timestp */
}

static int vproc_attach_fileref(fnx_vproc_t *vproc, fnx_task_t *task,
                                fnx_inode_t *inode, fnx_flags_t flags)
{
	size_t curr;
	fnx_fileref_t *fref;

	curr = fnx_list_size(&vproc->frpool.usedf);
	fref = fnx_vproc_tie_fileref(vproc, inode, flags);
	if (fref == NULL) {
		fnx_warn("no-new-fileref: curr-used=%lu", curr);
		return -ENOMEM;
	}
	task->tsk_fref = fref;
	return 0;
}

static int vproc_detach_fileref(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_inode_t *inode;
	fnx_fileref_t *fref;

	fref = task->tsk_fref;
	task->tsk_fref = NULL;

	inode = fnx_vproc_untie_fileref(vproc, fref);
	if (inode->i_vnode.v_expired) {
		rc = fnx_vproc_fix_unlinked(vproc, inode);
		fnx_assert(rc == 0);
	}
	return rc;
}

/* Bind to internal fileref for case of by-path truncate (vs ftruncate) */
static void vproc_reassure_task_fref(fnx_vproc_t *vproc,
                                     fnx_task_t  *task,
                                     fnx_reg_t   *reg)
{
	if (task->tsk_fref == NULL) {
		task->tsk_fref = &vproc->frpool.ghost;
		task->tsk_fref->f_inode = &reg->r_inode;
	}
}

static void vproc_fadeaway_task_fref(fnx_vproc_t *vproc, fnx_task_t *task)
{
	if (task->tsk_fref == &vproc->frpool.ghost) {
		task->tsk_fref = NULL;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* LOOKUP */
static int vop_lookup(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t parent_ino;
	fnx_dir_t *dir;
	fnx_inode_t *inode;
	fnx_response_t *resp;
	const fnx_request_t *rqst;
	const fnx_name_t *name;

	rqst = task_get_request(task);
	resp = task_get_response(task);
	parent_ino  = rqst->req_lookup.parent;
	name        = &rqst->req_lookup.name;

	rc = fnx_vproc_fetch_dir(vproc, parent_ino, &dir);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_lookup(vproc, task, dir);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_lookup_inode(vproc, dir, name, &inode);
	if (rc != 0) {
		return rc;
	}
	fnx_inode_getiattr(inode, &resp->res_lookup.iattr);
	trace_vop(vproc, "parent_ino=%#jx name=%s", parent_ino, name->str);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* FORGET */
static int vop_forget(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc;
	fnx_ino_t ino;
	unsigned long nlookup;
	const fnx_request_t *rqst;

	rqst    = task_get_request(task);
	ino     = rqst->req_forget.ino;
	nlookup = rqst->req_forget.nlookup;

	rc = fnx_vproc_let_forget(vproc, task, ino);
	if (rc != 0) {
		return rc;
	}
	rc = nlookup ? fnx_vproc_exec_forget(vproc, task, ino) : 0;
	if (rc != 0) {
		return rc;
	}
	/* Ignored operation */
	trace_vop(vproc, "ino=%#jx nlookup=%lu", ino, nlookup);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* GETATTR */
static int vop_getattr(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc;
	fnx_ino_t ino;
	fnx_inode_t *inode = NULL;
	fnx_response_t *resp;
	const fnx_request_t *rqst;

	rqst = task_get_request(task);
	resp = task_get_response(task);
	ino  = rqst->req_getattr.ino;
	rc = fnx_vproc_fetch_inode(vproc, ino, &inode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_getattr(vproc, task, inode);
	if (rc != 0) {
		return rc;
	}
	fnx_inode_getiattr(inode, &resp->res_getattr.iattr);
	trace_vop(vproc, "ino=%#jx", ino);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* SETATTR */
static int vop_setattr(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_flags_t flags;
	fnx_ino_t ino;
	fnx_off_t size;
	fnx_uid_t uid;
	fnx_gid_t gid;
	fnx_mode_t mode;
	fnx_inode_t *inode;
	fnx_response_t *resp;
	const fnx_request_t *rqst;
	const fnx_times_t *itms;
	const fnx_uctx_t  *uctx;

	uctx    = task_get_uctx(task);
	rqst    = task_get_request(task);
	resp    = task_get_response(task);
	flags   = rqst->req_setattr.flags;
	ino     = rqst->req_setattr.iattr.i_ino;
	mode    = rqst->req_setattr.iattr.i_mode;
	size    = rqst->req_setattr.iattr.i_size;
	uid     = rqst->req_setattr.iattr.i_uid;
	gid     = rqst->req_setattr.iattr.i_gid;
	itms    = &rqst->req_setattr.iattr.i_times;

	rc = fnx_vproc_fetch_inode(vproc, ino, &inode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_setattr(vproc, task, inode,
	                           flags, mode, uid, gid, size);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_setiattr(vproc, inode, uctx,
	                        flags, mode, uid, gid, size, itms);
	if (rc != 0) {
		return rc;
	}

	fnx_inode_getiattr(inode, &resp->res_setattr.iattr);
	trace_vop(vproc, "ino=%#jx flags=%#x mode=%o", ino, flags, mode);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* TRUNCATE */
static int vop_truncate(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t ino;
	fnx_off_t size;
	fnx_reg_t *reg;
	fnx_response_t *resp;
	const fnx_request_t *rqst;

	rqst = task_get_request(task);
	resp = task_get_response(task);
	ino  = rqst->req_truncate.ino;
	size = rqst->req_truncate.size;

	/* NB: Can not use grab_reg; truncate may not have fref */
	rc = fnx_vproc_fetch_reg(vproc, ino, &reg);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_truncate(vproc, task, reg);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_setsize(vproc, task, &reg->r_inode, size);
	if (rc != 0) {
		return rc;
	}
	if (size == fnx_inode_getsize(&reg->r_inode)) {
		goto out; /* OK, ignored case */
	}
	vproc_reassure_task_fref(vproc, task, reg);
	rc = fnx_vproc_exec_trunc(vproc, task, size);
	vproc_fadeaway_task_fref(vproc, task);
	if (rc != 0) {
		return rc;
	}

out:
	fnx_inode_getiattr(&reg->r_inode, &resp->res_truncate.iattr);
	trace_vop(vproc, "ino=%#jx size=%ju", ino, size);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* READLINK */
static int vop_readlink(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t ino;
	fnx_symlnk_t *symlnk;
	fnx_response_t *resp;
	const fnx_request_t *rqst;

	rqst = task_get_request(task);
	resp = task_get_response(task);
	ino  = rqst->req_readlink.ino;

	rc = fnx_vproc_fetch_symlnk(vproc, ino, &symlnk);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_readlink(vproc, task, symlnk);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_read_symlnk(vproc, symlnk, &resp->res_readlink.slnk);
	if (rc != 0) {
		return rc;
	}

	trace_vop(vproc, "ino=%#jx", ino);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* SYMLINK */
static int vop_symlink(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t parent_ino;
	fnx_dir_t    *parentd;
	fnx_inode_t  *inode;
	fnx_symlnk_t *symlnk;
	fnx_response_t *resp;
	const fnx_request_t *rqst;
	const fnx_name_t *name;
	const fnx_path_t *path;
	const fnx_uctx_t *uctx;

	uctx = task_get_uctx(task);
	rqst = task_get_request(task);
	resp = task_get_response(task);

	parent_ino  = rqst->req_symlink.parent;
	name  = &rqst->req_symlink.name;
	path  = &rqst->req_symlink.slnk;

	rc = fnx_vproc_fetch_dir(vproc, parent_ino, &parentd);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_symlink(vproc, task, parentd, name, path);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_prep_link(vproc, parentd, name);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_acquire_symlnk(vproc, uctx, path, &symlnk);
	if (rc != 0) {
		return rc;
	}
	inode = &symlnk->sl_inode;
	rc = fnx_vproc_link_child(vproc, parentd, inode, name);
	if (rc != 0) {
		fnx_vproc_retire_inode(vproc, inode);
		return rc;
	}

	fnx_inode_getiattr(inode, &resp->res_symlink.iattr);
	trace_vop(vproc, "parent_ino=%#jx name=%s slnk=%s",
	          parent_ino, name->str, path->str);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* MKNOD */
static int vop_mknod(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t   parent_ino;
	fnx_mode_t  mode;
	fnx_dev_t   rdev;
	fnx_inode_t *inode;
	fnx_dir_t *parentd;
	fnx_response_t *resp;
	const fnx_request_t *rqst;
	const fnx_name_t *name;
	const fnx_uctx_t *uctx;

	uctx = task_get_uctx(task);
	rqst = task_get_request(task);
	resp = task_get_response(task);
	parent_ino  = rqst->req_mknod.parent;
	name  = &rqst->req_mknod.name;
	mode  = rqst->req_mknod.mode;
	rdev  = rqst->req_mknod.rdev;

	rc = fnx_vproc_fetch_dir(vproc, parent_ino, &parentd);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_mknod(vproc, task, parentd, name, mode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_prep_link(vproc, parentd, name);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_acquire_special(vproc, uctx, mode, &inode);
	if (rc != 0) {
		return rc;
	}
	inode->i_iattr.i_rdev = rdev; /* FIXME */
	rc = fnx_vproc_link_child(vproc, parentd, inode, name);
	if (rc != 0) {
		fnx_vproc_retire_inode(vproc, inode);
		return rc;
	}

	fnx_inode_getiattr(inode, &resp->res_mknod.iattr);
	trace_vop(vproc, "parent_ino=%#jx name=%s mode=%o rdev=%#jx",
	          parent_ino, name->str, mode, rdev);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* MKDIR */
static int vop_mkdir(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_mode_t mode;
	fnx_ino_t parent_ino;
	fnx_dir_t   *parentd;
	fnx_dir_t   *dir;
	fnx_inode_t *inode = NULL;
	fnx_response_t *resp;
	const fnx_request_t *rqst;
	const fnx_name_t *name;
	const fnx_uctx_t *uctx;

	uctx = task_get_uctx(task);
	rqst = task_get_request(task);
	resp = task_get_response(task);
	parent_ino  = rqst->req_mkdir.parent;
	name    = &rqst->req_mkdir.name;
	mode    = rqst->req_mkdir.mode;

	rc = fnx_vproc_fetch_dir(vproc, parent_ino, &parentd);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_mkdir(vproc, task, parentd, name, mode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_prep_link(vproc, parentd, name);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_acquire_dir(vproc, uctx, mode, &dir);
	if (rc != 0) {
		return rc;
	}
	inode = &dir->d_inode;
	rc = fnx_vproc_link_child(vproc, parentd, inode, name);
	if (rc != 0) {
		fnx_vproc_retire_inode(vproc, inode);
		return rc;
	}

	fnx_inode_getiattr(inode, &resp->res_mkdir.iattr);
	trace_vop(vproc, "parent_ino=%#jx name=%s mode=%o",
	          parent_ino, name->str, mode);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* UNLINK */
static int vop_unlink(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t parent_ino, ino = FNX_INO_NULL;
	fnx_inode_t *inode = NULL;
	fnx_dir_t   *parentd;
	fnx_response_t *resp;
	const fnx_request_t *rqst;
	const fnx_name_t *name;

	rqst = task_get_request(task);
	resp = task_get_response(task);
	parent_ino  = rqst->req_unlink.parent;
	name    = &rqst->req_unlink.name;

	rc = fnx_vproc_fetch_dir(vproc, parent_ino, &parentd);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_lookup_link(vproc, parentd, name, &inode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_unlink(vproc, task, parentd, inode);
	if (rc != 0) {
		return rc;
	}
	ino = fnx_inode_getrefino(inode);
	rc = fnx_vproc_prep_xunlink(vproc, parentd, inode);
	if (rc != 0) {
		return rc;
	}
	rc  = fnx_vproc_unlink_child(vproc, parentd, inode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_fix_unlinked(vproc, inode);
	if (rc != 0) {
		return rc;
	}

	resp->res_unlink.ino = ino;
	trace_vop(vproc, "parent_ino=%#jx name=%s ino=%#jx",
	          parent_ino, name->str, ino);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* RMDIR */
static int vop_rmdir(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t parent_ino;
	fnx_dir_t *parentd;
	fnx_dir_t *dir;
	const fnx_request_t *rqst;
	const fnx_name_t *name;

	rqst  = task_get_request(task);
	parent_ino  = rqst->req_rmdir.parent;
	name  = &rqst->req_rmdir.name;

	rc = fnx_vproc_fetch_dir(vproc, parent_ino, &parentd);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_lookup_dir(vproc, parentd, name, &dir);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_rmdir(vproc, task, parentd, dir);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_prep_unlink(vproc, parentd, &dir->d_inode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_unlink_child(vproc, parentd, &dir->d_inode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_fix_unlinked(vproc, &dir->d_inode);
	if (rc != 0) {
		return rc;
	}

	trace_vop(vproc, "parent_ino=%#jx name=%s", parent_ino, name->str);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* RENAME */
static int vop_rename(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t parent_ino, newparent_ino;
	const fnx_request_t *rqst;
	const fnx_name_t *name;
	const fnx_name_t *newname;

	rqst = task_get_request(task);
	parent_ino = rqst->req_rename.parent;
	name = &rqst->req_rename.name;
	newparent_ino = rqst->req_rename.newparent;
	newname = &rqst->req_rename.newname;

	rc = fnx_vproc_exec_rename(vproc, task);
	if (rc != 0) {
		return rc;
	}
	trace_vop(vproc, "parent_ino=%#jx name=%s newparent_ino=%#jx newname=%s",
	          parent_ino, name->str, newparent_ino, newname->str);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* LINK */
static int vop_link(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t ino, newparent_ino;
	fnx_inode_t *inode, *rlnk = NULL;
	fnx_dir_t *newparentd = NULL;
	fnx_response_t *resp;
	const fnx_request_t *rqst;
	const fnx_name_t *newname;
	const fnx_uctx_t *uctx;

	uctx = task_get_uctx(task);
	rqst = task_get_request(task);
	resp = task_get_response(task);
	ino  = rqst->req_link.ino;
	newparent_ino = rqst->req_link.newparent;
	newname  = &rqst->req_link.newname;

	rc = fnx_vproc_fetch_dir(vproc, newparent_ino, &newparentd);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_fetch_inode(vproc, ino, &inode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_namespace(vproc, task, newparentd, newname);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_link(vproc, task, newparentd, inode, newname);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_prep_link(vproc, newparentd, newname);
	if (rc != 0) {
		fnx_assert(rc == FNX_EPEND);
		return rc;
	}
	rc = fnx_vproc_acquire_reflnk(vproc, uctx, ino, &rlnk);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_link_child(vproc, newparentd, rlnk, newname);
	if (rc != 0) {
		fnx_assert(rc == FNX_EPEND);
		fnx_vproc_retire_inode(vproc, rlnk);
		return rc;
	}

	fnx_inode_getiattr(inode, &resp->res_link.iattr);
	trace_vop(vproc, "ino=%#jx newparent_ino=%#jx newname=%s",
	          ino, newparent_ino, newname->str);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* OPEN */
static int vop_open(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t ino;
	fnx_flags_t flags;
	fnx_inode_t *inode;
	const fnx_request_t *rqst;

	rqst = task_get_request(task);
	ino  = rqst->req_open.ino;
	flags = rqst->req_open.flags;

	rc = fnx_vproc_fetch_inode(vproc, ino, &inode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_open(vproc, task, inode, flags);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_attach_fileref(vproc, task, inode, flags);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_exec_open(vproc, task, flags);
	if (rc != 0) {
		vproc_detach_fileref(vproc, task);
		return rc;
	}
	trace_vop(vproc, "ino=%#jx flags=%#x", ino, flags);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* READ */
static int vop_read(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t  ino;
	fnx_off_t  off;
	fnx_size_t size, rsize = 0;
	fnx_reg_t *reg;
	fnx_response_t *resp;
	const fnx_request_t *rqst;

	rqst = task_get_request(task);
	resp = task_get_response(task);
	ino  = rqst->req_read.ino;
	off  = rqst->req_read.off;
	size = rqst->req_read.size;

	rc = fnx_vproc_grab_reg(vproc, task, ino, &reg);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_read(vproc, task, reg, off, size);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_exec_read(vproc, task, off, size);
	if (rc != 0) {
		return rc;
	}
	rsize = task_get_datalen(task);
	resp->res_read.size = rsize;

	vproc_count_read(vproc, rsize);
	trace_vop(vproc, "ino=%#jx off=%ju rsize=%zu", ino, off, rsize);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* WRITE */
static int vop_write(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t  ino;
	fnx_size_t size, wsize;
	fnx_off_t off;
	fnx_reg_t *reg;
	fnx_iobufs_t *iobufs;
	const fnx_request_t *rqst;
	fnx_response_t *resp;

	rqst = task_get_request(task);
	resp = task_get_response(task);
	ino  = rqst->req_write.ino;
	off  = rqst->req_write.off;
	size = rqst->req_write.size;
	iobufs  = task_get_iobufs(task);

	rc = fnx_vproc_grab_reg(vproc, task, ino, &reg);
	if (rc != 0) {
		if (rc != FNX_EPEND) {
			fnx_iobufs_release(iobufs); /* TODO elsewhere */
		}
		return rc;
	}
	rc = fnx_vproc_let_write(vproc, task, reg, off, size);
	if (rc != 0) {
		if (rc != FNX_EPEND) {
			fnx_iobufs_release(iobufs); /* TODO elsewhere */
		}
		return rc;
	}
	rc = fnx_vproc_exec_write(vproc, task, off, size);
	if (rc != 0) {
		return rc;
	}

	wsize = task_get_datalen(task);
	resp->res_write.size = wsize;
	vproc_count_write(vproc, wsize);
	trace_vop(vproc, "ino=%#jx off=%ju size=%zu", ino, off, wsize);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* STATFS */
static int vop_statfs(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t ino;
	fnx_inode_t  *inode;
	fnx_super_t  *super;
	fnx_response_t *resp;
	const fnx_request_t *rqst;

	rqst = task_get_request(task);
	resp = task_get_response(task);
	ino  = rqst->req_statfs.ino;
	super = vproc->super;

	rc = fnx_vproc_fetch_inode(vproc, ino, &inode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_statfs(vproc, task, inode);
	if (rc != 0) {
		return rc;
	}
	fnx_super_getfsinfo(super, &resp->res_statfs.fsinfo);
	trace_vop(vproc, "ino=%#jx", ino);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* RELEASE */
static int vop_release(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t ino;
	fnx_flags_t flags;
	const fnx_request_t *rqst;

	rqst  = task_get_request(task);
	ino   = rqst->req_release.ino;
	flags = rqst->req_release.flags;

	rc = fnx_vproc_let_release(vproc, task, ino);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_exec_release(vproc, task);
	if (rc != 0) {
		return rc;
	}
	trace_vop(vproc, "ino=%#jx flags=%#x", ino, flags);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* FSYNC */
static int vop_fsync(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t ino;
	fnx_bool_t datasync;
	const fnx_request_t *rqst;

	rqst = task_get_request(task);
	ino  = rqst->req_fsync.ino;
	datasync = rqst->req_fsync.datasync;

	rc = fnx_vproc_let_fsync(vproc, task, ino);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_exec_fsync(vproc, task);
	if ((rc != 0) && (rc != FNX_EDELAY)) {
		return rc;
	}
	trace_vop(vproc, "ino=%#jx datasync=%d", ino, datasync);
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* FLUSH */
static int vop_flush(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc;
	fnx_ino_t ino;
	const fnx_request_t *rqst;

	rqst = task_get_request(task);
	ino  = rqst->req_flush.ino;

	rc = fnx_vproc_let_flush(vproc, task, ino);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_exec_fsync(vproc, task);
	if ((rc != 0) && (rc != FNX_EDELAY)) {
		return rc;
	}
	trace_vop(vproc, "ino=%#jx", ino);
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* OPENDIR */
static int vop_opendir(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t ino;
	fnx_dir_t *dir;
	const fnx_request_t *rqst;

	rqst = task_get_request(task);
	ino  = rqst->req_opendir.ino;

	rc = fnx_vproc_fetch_dir(vproc, ino, &dir);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_opendir(vproc, task, dir);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_attach_fileref(vproc, task, &dir->d_inode, 0);
	if (rc != 0) {
		return rc;
	}
	trace_vop(vproc, "ino=%#jx", ino);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* READDIR */
static int vop_readdir(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_off_t off, off_next  = FNX_DOFF_NONE; /* By default no next entry */
	fnx_ino_t ino, child_ino = FNX_INO_NULL;
	fnx_name_t *name     = NULL;
	fnx_dir_t  *parentd  = NULL;
	fnx_inode_t *child   = NULL;
	const fnx_request_t *rqst;
	fnx_response_t *resp;

	rqst = task_get_request(task);
	resp = task_get_response(task);
	ino  = rqst->req_readdir.ino;
	off  = rqst->req_readdir.off;

	/* Fill response so that even if no next entry, output is valid */
	resp->res_readdir.name.len  = 0;
	resp->res_readdir.child     = child_ino;
	resp->res_readdir.mode      = 0;
	resp->res_readdir.off_next  = off_next;

	/* May get -1 offset at end-of-stream. Still OK. */
	if (!fnx_doff_isvalid(off)) {
		goto out; /* OK */
	}
	rc = fnx_vproc_let_readdir(vproc, task, ino, off);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_fetch_dir(vproc, ino, &parentd);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_search_dent(vproc, parentd, off,
	                           &name, &child, &off_next);
	if (rc == 0) {
		fnx_name_copy(&resp->res_readdir.name, name);
		resp->res_readdir.child = child_ino = fnx_inode_getino(child);
		resp->res_readdir.mode      = child->i_iattr.i_mode;
		resp->res_readdir.off_next  = off_next;
	} else if (rc == FNX_EEOS) {
		resp->res_readdir.child = child_ino = FNX_INO_NULL;
		resp->res_readdir.off_next  = off_next  = FNX_DOFF_NONE;
	} else {
		return rc;
	}

out:
	trace_vop(vproc, "ino=%#jx off=%jd child_ino=%#jx off_next=%jd",
	          ino, off, child_ino, off_next);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* RELEASEDIR */
static int vop_releasedir(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t ino;
	const fnx_request_t *rqst;

	rqst = task_get_request(task);
	ino  = rqst->req_releasedir.ino;

	rc = fnx_vproc_let_releasedir(vproc, task, ino);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_detach_fileref(vproc, task);
	if (rc != 0) {
		return rc;
	}

	trace_vop(vproc, "ino=%#jx", ino);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* FSYNCDIR */
static int vop_fsyncdir(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t ino;
	int datasync;
	fnx_dir_t *dir;
	const fnx_request_t *rqst;

	rqst = task_get_request(task);
	ino  = rqst->req_fsyncdir.ino;
	datasync = rqst->req_fsyncdir.datasync;

	rc = fnx_vproc_grab_dir(vproc, task, ino, &dir);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_fsyncdir(vproc, task, dir);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_exec_fsync(vproc, task);
	if ((rc != 0) && (rc != FNX_EDELAY)) {
		return rc;
	}
	trace_vop(vproc, "ino=%#jx datasync=%d", ino, datasync);
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* ACCESS */
static int vop_access(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t  ino;
	fnx_mode_t mask;
	fnx_inode_t *inode;
	const fnx_request_t *rqst;

	rqst = task_get_request(task);
	ino  = rqst->req_access.ino;
	mask = rqst->req_access.mask;
	rc = fnx_vproc_fetch_inode(vproc, ino, &inode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_access(vproc, task, inode, mask);
	if (rc != 0) {
		return rc;
	}

	trace_vop(vproc, "ino=%#jx mask=%o", ino, mask);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* CREATE */
static int vop_create(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_mode_t  mode;
	fnx_flags_t flags;
	fnx_ino_t   parent_ino;
	fnx_reg_t   *reg;
	fnx_inode_t *inode;
	fnx_dir_t   *parentd;
	fnx_response_t *resp;
	const fnx_request_t *rqst;
	const fnx_name_t *name;
	const fnx_uctx_t *uctx;

	uctx = task_get_uctx(task);
	rqst = task_get_request(task);
	resp = task_get_response(task);
	parent_ino  = rqst->req_create.parent;
	name  = &rqst->req_create.name;
	mode  = rqst->req_create.mode;
	flags = rqst->req_create.flags;

	rc = fnx_vproc_fetch_dir(vproc, parent_ino, &parentd);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_create(vproc, task, parentd, name, mode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_prep_link(vproc, parentd, name);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_acquire_reg(vproc, uctx, mode, &reg);
	if (rc != 0) {
		return rc;
	}
	inode = &reg->r_inode;
	rc = fnx_vproc_link_child(vproc, parentd, inode, name);
	fnx_assert(rc == 0); /* XXX */
	if (rc != 0) {
		return rc;
	}
	rc = vproc_attach_fileref(vproc, task, inode, flags);
	if (rc != 0) {
		fnx_vproc_retire_inode(vproc, inode);
		return rc;
	}

	fnx_inode_getiattr(inode, &resp->res_create.iattr);
	trace_vop(vproc, "parent_ino=%#jx name=%s mode=%o flags=%#x",
	          parent_ino, name->str, mode, flags);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* FALLOCATE */
static int vop_fallocate(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t  ino;
	fnx_off_t  off;
	fnx_size_t len;
	fnx_bool_t keepsz;
	fnx_reg_t *reg;
	const fnx_request_t *rqst;

	rqst = task_get_request(task);
	ino  = rqst->req_fallocate.ino;
	off  = rqst->req_fallocate.off;
	len  = rqst->req_fallocate.len;
	keepsz  = rqst->req_fallocate.keep_size;

	rc = fnx_vproc_grab_reg(vproc, task, ino, &reg);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_fallocate(vproc, task, reg, off, len);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_exec_falloc(vproc, task, off, len, keepsz);
	if (rc != 0) {
		return rc;
	}

	trace_vop(vproc, "ino=%#jx off=%ju len=%zu keep_size=%i",
	          ino, off, len, keepsz);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* PUNCH */
static int vop_punch(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t  ino;
	fnx_off_t  off;
	fnx_size_t len;
	fnx_reg_t *reg;
	const fnx_request_t *rqst;

	rqst = task_get_request(task);
	ino  = rqst->req_punch.ino;
	off  = rqst->req_punch.off;
	len  = rqst->req_punch.len;

	rc = fnx_vproc_grab_reg(vproc, task, ino, &reg);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_punch(vproc, task, reg, off, len);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_exec_punch(vproc, task, off, len);
	if (rc != 0) {
		return rc;
	}
	trace_vop(vproc, "ino=%#jx off=%ju len=%zu", ino, off, len);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* FQUERY */
static int vop_fquery(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc;
	fnx_ino_t ino;
	fnx_reg_t *reg;
	const fnx_request_t *rqst;
	fnx_response_t *resp;

	rqst    = task_get_request(task);
	resp    = task_get_response(task);
	ino     = rqst->req_fquery.ino;

	rc = fnx_vproc_grab_reg(vproc, task, ino, &reg);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_fquery(vproc, task, reg);
	if (rc != 0) {
		return rc;
	}
	fnx_inode_getiattr(&reg->r_inode, &resp->res_fquery.iattr);
	trace_vop(vproc, "ino=%#jx", ino);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* FSINFO */
static int vop_fsinfo(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t ino;
	fnx_inode_t  *inode;
	const fnx_super_t *super;
	fnx_response_t *resp;
	const fnx_request_t *rqst;

	rqst = task_get_request(task);
	resp = task_get_response(task);
	ino  = rqst->req_fsinfo.ino;
	super = vproc->super;

	rc = fnx_vproc_fetch_inode(vproc, ino, &inode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_statfs(vproc, task, inode);
	if (rc != 0) {
		return rc;
	}
	fnx_super_getfsinfo(super, &resp->res_fsinfo.fsinfo);
	trace_vop(vproc, "ino=%#jx", ino);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_vproc_fn fnx_vop_hooks[] = {
	[FNX_VOP_NONE]       = NULL,
	[FNX_VOP_MKDIR]      = vop_mkdir,
	[FNX_VOP_RMDIR]      = vop_rmdir,
	[FNX_VOP_OPENDIR]    = vop_opendir,
	[FNX_VOP_READDIR]    = vop_readdir,
	[FNX_VOP_RELEASEDIR] = vop_releasedir,
	[FNX_VOP_FSYNCDIR]   = vop_fsyncdir,
	[FNX_VOP_STATFS]     = vop_statfs,
	[FNX_VOP_FORGET]     = vop_forget,
	[FNX_VOP_GETATTR]    = vop_getattr,
	[FNX_VOP_SETATTR]    = vop_setattr,
	[FNX_VOP_ACCESS]     = vop_access,
	[FNX_VOP_OPEN]       = vop_open,
	[FNX_VOP_READ]       = vop_read,
	[FNX_VOP_WRITE]      = vop_write,
	[FNX_VOP_RELEASE]    = vop_release,
	[FNX_VOP_FSYNC]      = vop_fsync,
	[FNX_VOP_FLUSH]      = vop_flush,
	[FNX_VOP_INTERRUPT]  = NULL,
	[FNX_VOP_BMAP]       = NULL,
	[FNX_VOP_POLL]       = NULL,
	[FNX_VOP_TRUNCATE]   = vop_truncate,
	[FNX_VOP_FALLOCATE]  = vop_fallocate,
	[FNX_VOP_PUNCH]      = vop_punch,
	[FNX_VOP_LOOKUP]     = vop_lookup,
	[FNX_VOP_LINK]       = vop_link,
	[FNX_VOP_UNLINK]     = vop_unlink,
	[FNX_VOP_RENAME]     = vop_rename,
	[FNX_VOP_READLINK]   = vop_readlink,
	[FNX_VOP_SYMLINK]    = vop_symlink,
	[FNX_VOP_CREATE]     = vop_create,
	[FNX_VOP_MKNOD]      = vop_mknod,
	[FNX_VOP_FQUERY]     = vop_fquery,
	[FNX_VOP_FSINFO]     = vop_fsinfo,
	[FNX_VOP_LAST]       = NULL,
};


fnx_vproc_fn fnx_get_vop(fnx_vopcode_e opc)
{
	unsigned indx, nops = FNX_VOP_LAST;
	fnx_vproc_fn hook = NULL;

	indx = (unsigned)opc;
	if (indx < nops) {
		hook = fnx_vop_hooks[indx];
	}
	return hook;
}
