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
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>

#include <fnxinfra.h>
#include <fnxcore.h>
#include <fnxpstor.h>
#include "vproc-elems.h"
#include "vproc-exec.h"
#include "vproc-let.h"
#include "vproc-private.h"


/* Local functions forward declarations */
static int inode_isowner(const fnx_inode_t *, const fnx_uctx_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int uctx_isowner(const fnx_uctx_t *uctx, const fnx_inode_t *inode)
{
	return inode_isowner(inode, uctx);
}

static int uctx_isingroup(const fnx_uctx_t *uctx, fnx_gid_t gid)
{
	return fnx_isgroupmember(uctx, gid);
}

static int uctx_hascap(const fnx_uctx_t *uctx, fnx_capf_t mask)
{
	return uctx->u_root || fnx_uctx_iscap(uctx, mask);
}

static int inode_issize(const fnx_inode_t *inode, fnx_off_t size)
{
	return (fnx_inode_getsize(inode) == size);
}

static int inode_ispseudo(const fnx_inode_t *inode)
{
	return inode->i_vnode.v_pseudo;
}

static int inode_isreflnk(const fnx_inode_t *inode)
{
	return fnx_inode_isreflnk(inode);
}

static fnx_mode_t inode_getmode(const fnx_inode_t *inode)
{
	return inode->i_iattr.i_mode;
}

static int inode_isuid(const fnx_inode_t *inode, fnx_uid_t uid)
{
	return (inode->i_iattr.i_uid == uid);
}

static int inode_isowner(const fnx_inode_t *inode, const fnx_uctx_t *uctx)
{
	return inode_isuid(inode, uctx->u_cred.cr_uid);
}

static const fnx_dir_t *inode_to_dir(const fnx_inode_t *inode)
{
	const fnx_dir_t *dir = NULL;
	if (fnx_inode_isdir(inode)) {
		dir = fnx_inode_to_dir(inode);
	}
	return dir;
}

static int dir_hasspace(const fnx_dir_t *dir)
{
	return (dir->d_nchilds < FNX_DIRCHILD_MAX);
}

static int dir_ispseudo(const fnx_dir_t *dir)
{
	return inode_ispseudo(&dir->d_inode);
}

static int dir_isvalidname(const fnx_dir_t *dir, const fnx_name_t *name)
{
	/* TODO: Use dir-specific semantics */
	const size_t name_max = sizeof(dir->d_inode.i_name.str) - 1;
	return (name->len <= name_max);
}

static int dir_isvtx(const fnx_dir_t *dir)
{
	const fnx_mode_t mode = inode_getmode(&dir->d_inode);
	return ((mode & S_ISVTX) != 0);
}

static fnx_ino_t reg_getino(const fnx_reg_t *reg)
{
	return fnx_inode_getino(&reg->r_inode);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const fnx_fileref_t *task_get_fileref(const fnx_task_t *task)
{
	return task->tsk_fref;
}

static const fnx_uctx_t *task_get_uctx(const fnx_task_t *task)
{
	return &task->tsk_uctx;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int fileref_let_ino(const fnx_fileref_t *fref, fnx_ino_t ino)
{
	fnx_ino_t fref_ino;
	fnx_vtype_e vtype;

	if ((fref == NULL) || (fref->f_inode == NULL)) {
		return -EBADF;
	}
	if (!fref->f_inuse) {
		fnx_warn("fileref-not-cached: ino=%#jx", ino);
		return -EBADF;
	}
	fref_ino = fnx_inode_getino(fref->f_inode);
	if (fref_ino != ino) {
		vtype = fnx_inode_vtype(fref->f_inode);
		fnx_error("fileref-inconsistency: "\
		          "ino=%#jx fref-ino=%#jx vtype=%d", ino, fref_ino, vtype);
		return -EBADF;
	}
	return 0;
}

static int fileref_let_reg(const fnx_fileref_t *fref, const fnx_reg_t *reg)
{
	int rc;

	rc = fileref_let_ino(fref, reg_getino(reg));
	if (rc != 0) {
		return rc;
	}
	if (fref->f_inode != &reg->r_inode) {
		return -EBADF;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int vproc_let_modify(const fnx_vproc_t *vproc)
{
	if (vproc->mntf & FNX_MNTF_RDONLY) {
		return -EROFS;
	}
	if (vproc->pstor->sbkq.size > 100000) { /* XXX */
		return FNX_EPEND;
	}
	return 0;
}

static int vproc_let_fsaccess(const fnx_vproc_t *vproc, fnx_mode_t mask)
{
	return (mask & W_OK) ? vproc_let_modify(vproc) : 0;
}

static int vproc_let_modify_vnode(const fnx_vproc_t *vproc,
                                  const fnx_vnode_t *vnode)
{
	int rc;

	rc = vproc_let_modify(vproc);
	if (rc != 0) {
		return rc;
	}
	if (!fnx_vnode_ismutable(vnode)) {
		return FNX_EPEND;
	}
	return 0;
}


static int vproc_let_modify_vinode(const fnx_vproc_t *vproc,
                                   const fnx_inode_t *inode)
{
	return vproc_let_modify_vnode(vproc, &inode->i_vnode);
}

static int vproc_let_modify_inode(fnx_vproc_t *vproc, const fnx_inode_t *inode)
{
	int rc;
	fnx_inode_t *iref = NULL;

	if (inode_ispseudo(inode)) {
		return -EPERM;
	}
	rc = vproc_let_modify_vinode(vproc, inode);
	if (rc != 0) {
		return rc;
	}
	if (inode_isreflnk(inode)) {
		rc = fnx_vproc_fetch_inode(vproc, inode->i_refino, &iref);
		if (rc != 0) {
			return rc;
		}
		rc = vproc_let_modify_vinode(vproc, iref);
		if (rc != 0) {
			return rc;
		}
	}
	return 0;
}

static int vproc_let_modify_dir(fnx_vproc_t *vproc, const fnx_dir_t *dir)
{
	return dir ? vproc_let_modify_inode(vproc, &dir->d_inode) : 0;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/


int fnx_vproc_let_lookup(fnx_vproc_t      *vproc,
                         const fnx_task_t *task,
                         const fnx_dir_t  *dir)
{
	return fnx_vproc_let_access(vproc, task, &dir->d_inode, X_OK);
}

int fnx_vproc_let_namespace(fnx_vproc_t      *vproc,
                            const fnx_task_t *task,
                            const fnx_dir_t  *dir,
                            const fnx_name_t *name)
{
	if (!dir_isvalidname(dir, name)) {
		return -ENAMETOOLONG; /* XXX -EINVAL */
	}
	fnx_unused2(vproc, task);
	return 0;
}

int fnx_vproc_let_access(fnx_vproc_t       *vproc,
                         const fnx_task_t  *task,
                         const fnx_inode_t *inode,
                         fnx_mode_t         mask)
{
	int rc;
	const fnx_uctx_t *uctx;

	uctx = task_get_uctx(task);
	rc = fnx_inode_access(inode, uctx, mask);
	if (rc != 0) {
		return -EACCES;
	}
	rc = vproc_let_fsaccess(vproc, mask);
	if (rc != 0) {
		return rc;
	}
	return 0;
}

int fnx_vproc_let_getattr(fnx_vproc_t       *vproc,
                          const fnx_task_t  *task,
                          const fnx_inode_t *inode)
{
	/* TODO: Checks needed? */
	fnx_unused(vproc);
	fnx_unused(task);
	fnx_unused(inode);
	return 0;
}


int fnx_vproc_let_readdir(fnx_vproc_t      *vproc,
                          const fnx_task_t *task,
                          fnx_ino_t         ino,
                          fnx_off_t         off)
{
	int rc;
	const fnx_fileref_t *fref;

	fref = task_get_fileref(task);
	rc = fileref_let_ino(fref, ino);
	if (rc != 0) {
		return rc;
	}
	if (!fnx_inode_isdir(fref->f_inode)) {
		return -ENOTDIR;
	}
	if (!fnx_doff_isvalid(off)) {
		return -EINVAL;
	}
	fnx_unused(vproc);
	return 0;
}

int fnx_vproc_let_releasedir(fnx_vproc_t      *vproc,
                             const fnx_task_t *task,
                             fnx_ino_t         ino)
{
	int rc;
	const fnx_inode_t *inode;
	const fnx_fileref_t *fref;

	fref = task_get_fileref(task);
	rc = fileref_let_ino(fref, ino);
	if (rc != 0) {
		return rc;
	}
	inode = fref->f_inode;
	if (!fnx_inode_isdir(inode)) {
		return -ENOTDIR;
	}
	if (inode->i_vnode.v_expired) {
		rc = vproc_let_modify_vinode(vproc, inode);
		if (rc != 0) {
			return rc;
		}
	}
	return 0;
}

int fnx_vproc_let_readlink(fnx_vproc_t        *vproc,
                           const fnx_task_t   *task,
                           const fnx_symlnk_t *symlnk)
{
	return fnx_vproc_let_access(vproc, task, &symlnk->sl_inode, R_OK);
}

int fnx_vproc_let_flush(fnx_vproc_t *vproc,
                        const fnx_task_t *task, fnx_ino_t ino)
{
	int rc;
	const fnx_fileref_t *fref;
	const fnx_inode_t *inode;

	fref = task_get_fileref(task);
	rc = fileref_let_ino(fref, ino);
	if (rc != 0) {
		return rc;
	}
	inode = fref->f_inode;
	if (!fnx_inode_isreg(inode)) {
		return -EINVAL;
	}
	if (inode_ispseudo(inode)) {
		return 0; /* OK; no-op for pseudo */
	}
	rc = vproc_let_modify_inode(vproc, inode);
	if (rc != 0) {
		return rc;
	}
	return 0;
}

int fnx_vproc_let_forget(fnx_vproc_t      *vproc,
                         const fnx_task_t *task,
                         fnx_ino_t         ino)
{
	if (!fnx_ino_isvalid(ino)) {
		return -EINVAL;
	}
	fnx_unused2(vproc, task);
	return 0;
}

int fnx_vproc_let_release(fnx_vproc_t      *vproc,
                          const fnx_task_t *task,
                          fnx_ino_t         ino)
{
	int rc;
	const fnx_fileref_t *fref;

	fref = task_get_fileref(task);
	rc = fileref_let_ino(fref, ino);
	if (rc != 0) {
		return rc;
	}
	if (!fnx_inode_isreg(fref->f_inode)) {
		return -EINVAL;
	}
	rc = vproc_let_modify_vinode(vproc, fref->f_inode);
	if (rc != 0) {
		return rc;
	}
	return 0;
}

int fnx_vproc_let_fsyncdir(fnx_vproc_t      *vproc,
                           const fnx_task_t *task,
                           const fnx_dir_t  *dir)
{
	int rc;
	fnx_ino_t ino;
	const fnx_fileref_t *fref;

	ino  = fnx_dir_getino(dir);
	fref = task_get_fileref(task);
	rc = fileref_let_ino(fref, ino);
	if (rc != 0) {
		return rc;
	}
	if (!fnx_inode_isdir(fref->f_inode)) {
		return -ENOTDIR;
	}
	rc = vproc_let_modify_dir(vproc, dir);
	if (rc != 0) {
		return rc;
	}
	return 0;
}

int fnx_vproc_let_fsync(fnx_vproc_t      *vproc,
                        const fnx_task_t *task,
                        fnx_ino_t         ino)
{
	int rc;
	const fnx_fileref_t *fref;

	fref = task_get_fileref(task);
	rc = fileref_let_ino(fref, ino);
	if (rc != 0) {
		return rc;
	}
	if (!fnx_inode_isreg(fref->f_inode)) {
		return -EINVAL;
	}
	rc = vproc_let_modify_vinode(vproc, fref->f_inode);
	if (rc != 0) {
		return rc;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int vproc_let_chmod(fnx_vproc_t       *vproc,
                           const fnx_task_t  *task,
                           const fnx_inode_t *inode,
                           fnx_mode_t         mode)
{
	int rc;
	fnx_vtype_e vtype;
	const fnx_uctx_t *uctx = task_get_uctx(task);

	rc = vproc_let_modify_inode(vproc, inode);
	if (rc != 0) {
		return rc;
	}
	vtype = fnx_mode_to_vtype(mode);
	if (fnx_inode_vtype(inode) != vtype) {
		/* Can not change vtype upon chmod */
		return -EPERM;
	}
	if (!uctx_isowner(uctx, inode) &&
	    !uctx_hascap(uctx, FNX_CAPF_FOWNER)) {
		return -EPERM;
	}
	return 0;
}

static int vproc_let_chown_gid(fnx_vproc_t       *vproc,
                               const fnx_task_t  *task,
                               const fnx_inode_t *inode,
                               fnx_gid_t          gid)
{
	int rc;
	const fnx_uctx_t *uctx = task_get_uctx(task);

	rc = vproc_let_modify_inode(vproc, inode);
	if (rc != 0) {
		return rc;
	}
	if (!uctx_hascap(uctx, FNX_CAPF_CHOWN)) {
		if (!uctx_isowner(uctx, inode)) {
			return -EPERM;
		}
		if (!uctx_isingroup(uctx, gid)) {
			return -EPERM;
		}
	}
	return 0;
}

static int vproc_let_chown_uid(fnx_vproc_t       *vproc,
                               const fnx_task_t  *task,
                               const fnx_inode_t *inode,
                               const fnx_uid_t    uid)
{
	int rc;
	const fnx_uctx_t *uctx  = task_get_uctx(task);

	rc = vproc_let_modify_inode(vproc, inode);
	if (rc != 0) {
		return rc;
	}
	if (!fnx_uid_isvalid(uid)) {
		return -EINVAL;
	}
	if (!uctx_hascap(uctx, FNX_CAPF_CHOWN)) {
		if (!uctx_isowner(uctx, inode)) {
			return -EPERM;
		}
		if (!inode_isuid(inode, uid)) {
			return -EPERM;
		}
	}
	return 0;
}

static int vproc_let_utimes(fnx_vproc_t       *vproc,
                            const fnx_task_t  *task,
                            const fnx_inode_t *inode)
{
	int rc;
	const fnx_uctx_t *uctx = task_get_uctx(task);

	rc = vproc_let_modify_inode(vproc, inode);
	if (rc != 0) {
		return rc;
	}
	if (!uctx_isowner(uctx, inode) &&
	    !uctx_hascap(uctx, FNX_CAPF_FOWNER)) {
		return -EPERM;
	}
	return 0;
}

int fnx_vproc_let_setattr(fnx_vproc_t       *vproc,
                          const fnx_task_t  *task,
                          const fnx_inode_t *inode,
                          fnx_flags_t        flags,
                          fnx_mode_t         mode,
                          fnx_uid_t          uid,
                          fnx_gid_t          gid,
                          fnx_off_t          size)
{
	int rc = 0;

	if (flags & FNX_SETATTR_MODE) {
		rc = vproc_let_chmod(vproc, task, inode, mode);
		if (rc != 0) {
			return rc;
		}
	}
	if ((flags & FNX_SETATTR_GID) && !fnx_gid_isnull(gid)) {
		rc = vproc_let_chown_gid(vproc, task, inode, gid);
		if (rc != 0) {
			return rc;
		}
	}
	if ((flags & FNX_SETATTR_UID) && !fnx_uid_isnull(uid)) {
		rc = vproc_let_chown_uid(vproc, task, inode, uid);
		if (rc != 0) {
			return rc;
		}
	}
	if ((flags & FNX_SETATTR_SIZE) && !inode_issize(inode, size)) {
		rc = fnx_vproc_let_setsize(vproc, task, inode, size);
		if (rc != 0) {
			return rc;
		}
	}
	if (flags & FNX_SETATTR_AMTIME_ANY) {
		rc = vproc_let_utimes(vproc, task, inode);
		if (rc != 0) {
			return rc;
		}
	}
	return 0;
}

int fnx_vproc_let_setsize(fnx_vproc_t       *vproc,
                          const fnx_task_t  *task,
                          const fnx_inode_t *inode,
                          fnx_off_t          size)
{
	int rc;
	const fnx_uctx_t *uctx = task_get_uctx(task);

	if (size > (fnx_off_t)FNX_REGSIZE_MAX) {
		return -EFBIG;
	} else if (size < 0) {
		return -EINVAL;
	}
	if (fnx_inode_isdir(inode)) {
		return -EISDIR;
	}
	if (!fnx_inode_isreg(inode)) { /* TODO: Is it? */
		return -EINVAL;
	}
	rc = vproc_let_modify_inode(vproc, inode);
	if (rc != 0) {
		return rc;
	}
	if (!inode_isowner(inode, uctx) &&
	    !uctx_hascap(uctx, FNX_CAPF_ADMIN)) {
		return -EPERM;
	}
	return 0;
}

int fnx_vproc_let_truncate(fnx_vproc_t      *vproc,
                           const fnx_task_t *task,
                           const fnx_reg_t  *reg)
{
	int rc;
	fnx_ino_t ino;
	const fnx_fileref_t *fref;

	ino  = reg_getino(reg);
	fref = task_get_fileref(task);
	if (fref != NULL) {
		if (fref->f_inode == NULL) {
			return -EINVAL;
		}
		if (fnx_inode_getino(fref->f_inode) != ino) {
			return -EINVAL;
		}
		if (!fref->f_writeable) {
			return -EACCES;
		}
		rc = vproc_let_modify_inode(vproc, fref->f_inode);
		if (rc != 0) {
			return rc;
		}
	} else {
		rc = fnx_vproc_let_access(vproc, task, &reg->r_inode, W_OK);
		if (rc != 0) {
			return rc;
		}
		rc = vproc_let_modify_inode(vproc, &reg->r_inode);
		if (rc != 0) {
			return rc;
		}
	}
	return 0;
}

int fnx_vproc_let_statfs(fnx_vproc_t       *vproc,
                         const fnx_task_t  *task,
                         const fnx_inode_t *inode)
{
	int rc;
	fnx_ino_t ino;
	const fnx_fileref_t *fref;

	ino  = fnx_inode_getino(inode);
	fref = task_get_fileref(task);
	if (fref != NULL) {
		if (fref->f_inode == NULL) {
			return -EINVAL;
		}
		if (fnx_inode_getino(fref->f_inode) != ino) {
			return -EINVAL;
		}
		if (!fref->f_readable) {
			return -EACCES;
		}
	} else {
		rc = fnx_vproc_let_access(vproc, task, inode, R_OK);
		if (rc != 0) {
			return rc;
		}
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int vproc_let_openf(const fnx_vproc_t *vproc, const fnx_task_t  *task)
{
	int priv, rc = 0;
	const fnx_uctx_t *uctx;

	uctx = task_get_uctx(task);
	priv = fnx_isprivileged(uctx);
	if (!fnx_vproc_hasfree_fileref(vproc, priv)) {
		/*
		 * TODO: Check more then just priv? Unfortunately, this is not good
		 * enough. User may create/open zillion files which in turn will cause
		 * starvation to other processes.
		 *
		 * NB: Return FNX_EPEND may be risky! Consider with care.
		 */
		rc = -ENFILE;
	}
	return rc;
}

static int vproc_verify_sticky(const fnx_vproc_t *vproc,
                               const fnx_task_t  *task,
                               const fnx_dir_t   *dir,
                               const fnx_inode_t *inode)
{
	int rc;
	const fnx_uctx_t *uctx = task_get_uctx(task);

	if (!dir_isvtx(dir)) {
		rc = 0; /* No sticky-bit, we're fine */
	} else if (inode_isowner(&dir->d_inode, uctx)) {
		rc = 0;
	} else if (inode_isowner(inode, uctx)) {
		rc = 0;
	} else if (uctx_hascap(uctx, FNX_CAPF_FOWNER)) {
		rc = 0;
	} else {
		rc = -EPERM;
	}
	fnx_unused(vproc);
	return rc;
}

static int vproc_let_dir_access(fnx_vproc_t      *vproc,
                                const fnx_task_t *task,
                                const fnx_dir_t  *dir,
                                fnx_mode_t        mask)
{
	return fnx_vproc_let_access(vproc, task, &dir->d_inode, mask);
}

static int vproc_let_dir_waccess(fnx_vproc_t      *vproc,
                                 const fnx_task_t *task,
                                 const fnx_dir_t  *dir)
{
	return vproc_let_dir_access(vproc, task, dir, W_OK);
}

static int vproc_let_iacquire(const fnx_vproc_t *vproc,
                              const fnx_task_t  *task)
{
	int priv;
	const fnx_super_t *super = vproc->super;
	const fnx_uctx_t *uctx   = task_get_uctx(task);

	priv = fnx_isprivileged(uctx);
	return fnx_super_hasnexti(super, priv) ? 0 : -ENOSPC;
}

int fnx_vproc_let_opendir(fnx_vproc_t      *vproc,
                          const fnx_task_t *task,
                          const fnx_dir_t  *dir)
{
	int rc;

	rc = vproc_let_dir_access(vproc, task, dir, R_OK);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_openf(vproc, task);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_iacquire(vproc, task);
	if (rc != 0) {
		return rc;
	}
	return 0;
}

int fnx_vproc_let_rmdir(fnx_vproc_t      *vproc,
                        const fnx_task_t *task,
                        const fnx_dir_t  *parentd,
                        const fnx_dir_t  *dir)
{
	int rc;

	rc = vproc_let_modify_dir(vproc, parentd);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_modify_dir(vproc, dir);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_dir_waccess(vproc, task, parentd);
	if (rc != 0) {
		return rc;
	}
	if (!fnx_dir_isempty(dir)) {
		return -ENOTEMPTY;
	}
	if (fnx_dir_isroot(dir)) {
		return -EBUSY;
	}
	if (dir->d_nsegs > 0) {
		return -EBUSY; /* XXX Should never happen! */
	}
	rc = vproc_verify_sticky(vproc, task, parentd, &dir->d_inode);
	if (rc != 0) {
		return rc;
	}
	return 0;
}

int fnx_vproc_let_unlink(fnx_vproc_t       *vproc,
                         const fnx_task_t  *task,
                         const fnx_dir_t   *parentd,
                         const fnx_inode_t *inode)
{
	int rc;

	if (fnx_inode_isdir(inode)) {
		return -EISDIR;
	}
	rc = vproc_let_dir_waccess(vproc, task, parentd);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_verify_sticky(vproc, task, parentd, inode);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_modify_inode(vproc, inode);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_modify_dir(vproc, parentd);
	if (rc != 0) {
		return rc;
	}
	return 0;
}

int fnx_vproc_let_link(fnx_vproc_t       *vproc,
                       const fnx_task_t  *task,
                       const fnx_dir_t   *dir,
                       const fnx_inode_t *inode,
                       const fnx_name_t  *name)
{
	int rc;

	if (fnx_inode_isdir(inode)) {
		return -EISDIR;
	}
	if (dir_ispseudo(dir)) {
		return -EPERM;
	}
	if (!dir_hasspace(dir)) {
		return -EMLINK;
	}
	if (fnx_inode_getnlink(inode) >= FNX_LINK_MAX) {
		return -EMLINK;
	}
	rc = vproc_let_dir_waccess(vproc, task, dir);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_iacquire(vproc, task); /* XXX */
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_nolink(vproc, task, dir, name);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_modify_dir(vproc, dir);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_modify_inode(vproc, inode);
	if (rc != 0) {
		return rc;
	}
	return 0;
}

static int vproc_let_iorange(const fnx_vproc_t *vproc,
                             const fnx_task_t  *task,
                             fnx_ino_t          ino,
                             fnx_off_t          off,
                             fnx_size_t         len)
{
	int rc;
	fnx_off_t max, end;
	const fnx_fileref_t *fref;

	fref = task_get_fileref(task);
	rc = fileref_let_ino(fref, ino);
	if (rc != 0) {
		return rc;
	}
	if (off < 0) {
		return -EINVAL;
	}
	if (!fnx_inode_isreg(fref->f_inode)) {
		return -EPERM;
	}
	end = fnx_off_end(off, len);
	max = fnx_off_end(0, FNX_REGSIZE_MAX);
	if ((off >= max) || (end > max)) {
		return -EFBIG;
	}
	fnx_unused(vproc);
	return 0;
}

static int vproc_let_space(const fnx_vproc_t *vproc,
                           const fnx_task_t  *task,
                           const fnx_reg_t   *reg,
                           fnx_off_t          off,
                           fnx_size_t         len)
{
	int priv, has_free = 1;
	fnx_off_t beg, end;
	fnx_size_t xln;
	fnx_bkcnt_t nfrgs, extra;
	const fnx_super_t *super = vproc->super;
	const fnx_uctx_t  *uctx  = task_get_uctx(task);

	if (!inode_ispseudo(&reg->r_inode)) {
		beg   = fnx_off_floor_blk(off);
		end   = fnx_off_end(off, len);
		xln   = fnx_off_len(beg, end);
		nfrgs = fnx_bytes_to_nfrg(xln);
		extra = 2 * FNX_RSEGNFRG;
		priv  = fnx_isprivileged(uctx);
		has_free = fnx_super_hasfreeb(super, nfrgs + extra, priv);
	}
	return has_free ? 0 : -ENOSPC;
}

int fnx_vproc_let_fallocate(fnx_vproc_t      *vproc,
                            const fnx_task_t *task,
                            const fnx_reg_t  *reg,
                            fnx_off_t         off,
                            fnx_size_t        len)
{
	int rc;
	const fnx_fileref_t *fref;

	fref = task_get_fileref(task);
	rc = fileref_let_reg(fref, reg);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_iorange(vproc, task, reg_getino(reg), off, len);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_space(vproc, task, reg, off, len);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_modify_inode(vproc, fref->f_inode);
	if (rc != 0) {
		return rc;
	}
	if (!fnx_inode_isreg(fref->f_inode)) {
		return -ENODEV;
	}
	if (!fref->f_writeable) {
		return -EPERM;
	}
	return 0;
}

int fnx_vproc_let_write(fnx_vproc_t      *vproc,
                        const fnx_task_t *task,
                        const fnx_reg_t  *reg,
                        fnx_off_t         off,
                        fnx_size_t        len)
{
	int rc;
	const fnx_fileref_t *fref;

	fref = task_get_fileref(task);
	rc = fileref_let_reg(fref, reg);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_modify_vnode(vproc, &reg->r_inode.i_vnode);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_iorange(vproc, task, reg_getino(reg), off, len);
	if (rc != 0) {
		return rc;
	}
	if (!fref->f_writeable) {
		return -EPERM;
	}
	rc = vproc_let_space(vproc, task, reg, off, len);
	if (rc != 0) {
		return rc;
	}
	return rc;
}

int fnx_vproc_let_read(fnx_vproc_t      *vproc,
                       const fnx_task_t *task,
                       const fnx_reg_t  *reg,
                       fnx_off_t         off,
                       fnx_size_t        len)
{
	int rc;
	const fnx_fileref_t *fref;

	fref = task_get_fileref(task);
	rc = fileref_let_reg(fref, reg);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_iorange(vproc, task, reg_getino(reg), off, len);
	if (rc != 0) {
		return rc;
	}
	if (!fref->f_readable) {
		return -EBADF;
	}
	return 0;
}

int fnx_vproc_let_punch(fnx_vproc_t       *vproc,
                        const fnx_task_t  *task,
                        const fnx_reg_t   *reg,
                        fnx_off_t          off,
                        fnx_size_t         len)
{
	int rc;
	const fnx_fileref_t *fref;

	fref = task_get_fileref(task);
	rc = fileref_let_reg(fref, reg);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_iorange(vproc, task, reg_getino(reg), off, len);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_modify_inode(vproc, fref->f_inode);
	if (rc != 0) {
		return rc;
	}
	if (!fref->f_writeable) {
		return -EPERM;
	}
	return 0;
}

int fnx_vproc_let_fquery(fnx_vproc_t       *vproc,
                         const fnx_task_t  *task,
                         const fnx_reg_t   *reg)
{
	int rc;
	const fnx_fileref_t *fref;

	fref = task_get_fileref(task);
	rc = fileref_let_reg(fref, reg);
	if (rc != 0) {
		return rc;
	}
	if (!fref->f_readable) {
		return -EPERM;
	}
	fnx_unused(vproc);
	return 0;
}

static int vproc_verify_oaccess(fnx_vproc_t       *vproc,
                                const fnx_task_t  *task,
                                const fnx_inode_t *inode,
                                fnx_flags_t        flags)
{
	fnx_mode_t mask = 0;

	if ((flags & O_RDWR) == O_RDWR) {
		mask = R_OK | W_OK;
	} else if ((flags & O_WRONLY) == O_WRONLY) {
		mask = W_OK;
	} else if ((flags & O_RDONLY) == O_RDONLY) {
		mask = R_OK;
	}

	if ((flags & O_TRUNC) == O_TRUNC) {
		mask |= W_OK;
	}
	if ((flags & O_APPEND) == O_APPEND) {
		mask |= W_OK;
	}
	return fnx_vproc_let_access(vproc, task, inode, mask);
}

static int vproc_verify_opseudo(fnx_vproc_t       *vproc,
                                const fnx_task_t  *task,
                                const fnx_inode_t *inode,
                                fnx_flags_t        flags)
{
	const fnx_flags_t mask = O_WRONLY | O_RDWR;

	if (!inode_ispseudo(inode)) {
		return 0;
	}
	if (!fnx_inode_isreg(inode)) {
		return -EPERM;
	}
	if ((flags & mask) && !inode->i_meta) {
		return -ENOTSUP;
	}
	fnx_unused2(vproc, task);
	return 0;
}

int fnx_vproc_let_open(fnx_vproc_t       *vproc,
                       const fnx_task_t  *task,
                       const fnx_inode_t *inode,
                       fnx_flags_t        flags)
{
	int rc;
	const fnx_flags_t mask = O_CREAT | O_EXCL;

	if (fnx_inode_isdir(inode)) {
		return -EISDIR;
	}
	if (flags & O_DIRECTORY) {
		return -EISDIR;
	}
	if (flags & mask) {
		return -EEXIST; /* XXX ? */
	}
	if (!fnx_inode_isreg(inode)) {
		return -EINVAL;
	}
	rc = vproc_let_openf(vproc, task);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_verify_oaccess(vproc, task, inode, flags);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_verify_opseudo(vproc, task, inode, flags);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_modify_vinode(vproc, inode);
	if (rc != 0) {
		return rc;
	}
	return 0;
}

int fnx_vproc_let_nolink(fnx_vproc_t       *vproc,
                         const fnx_task_t  *task,
                         const fnx_dir_t   *dir,
                         const fnx_name_t  *name)
{
	int rc;
	fnx_inode_t *inode;

	rc = fnx_vproc_let_namespace(vproc, task, dir, name);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_lookup_iinode(vproc, dir, name, &inode);
	if (rc == 0) {
		return -EEXIST;
	}
	if (rc != -ENOENT) {
		return rc;
	}
	return 0;
}

int fnx_vproc_let_symlink(fnx_vproc_t      *vproc,
                          const fnx_task_t *task,
                          const fnx_dir_t  *dir,
                          const fnx_name_t *name,
                          const fnx_path_t *path)
{
	int rc;
	fnx_vtype_e vtype;

	rc = vproc_let_modify_dir(vproc, dir);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_dir_waccess(vproc, task, dir);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_iacquire(vproc, task);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_nolink(vproc, task, dir, name);
	if (rc != 0) {
		return rc;
	}
	if (!dir_hasspace(dir)) {
		return -EMLINK;
	}
	vtype = fnx_vtype_by_symlnk_value(path);
	if (vtype == FNX_VTYPE_NONE) {
		return -EINVAL;
	}
	return 0;
}

int fnx_vproc_let_mknod(fnx_vproc_t      *vproc,
                        const fnx_task_t *task,
                        const fnx_dir_t  *dir,
                        const fnx_name_t *name,
                        fnx_mode_t        mode)
{
	int rc;

	rc = vproc_let_modify_dir(vproc, dir);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_dir_waccess(vproc, task, dir);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_iacquire(vproc, task);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_nolink(vproc, task, dir, name);
	if (rc != 0) {
		return rc;
	}
	if (!dir_hasspace(dir)) {
		return -EMLINK;
	}
	if (!fnx_mode_isspecial(mode)) {
		return -EINVAL;
	}
	return 0;
}

int fnx_vproc_let_mkdir(fnx_vproc_t      *vproc,
                        const fnx_task_t *task,
                        const fnx_dir_t  *dir,
                        const fnx_name_t *name,
                        fnx_mode_t        mode)
{
	int rc;

	if (fnx_isnotdir(mode)) {
		/* Negative check here due to FUSE mode without dir mode bits on */
		return -EINVAL;
	}
	rc = vproc_let_modify_dir(vproc, dir);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_dir_waccess(vproc, task, dir);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_iacquire(vproc, task);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_nolink(vproc, task, dir, name);
	if (rc != 0) {
		return rc;
	}
	if (!dir_hasspace(dir)) {
		return -EMLINK;
	}
	return 0;
}

int fnx_vproc_let_create(fnx_vproc_t       *vproc,
                         const fnx_task_t  *task,
                         const fnx_dir_t   *dir,
                         const fnx_name_t  *name,
                         fnx_mode_t         mode)
{
	int rc;

	rc = vproc_let_modify_dir(vproc, dir);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_dir_waccess(vproc, task, dir);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_iacquire(vproc, task);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_openf(vproc, task);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_nolink(vproc, task, dir, name);
	if (rc != 0) {
		return rc;
	}
	if (!fnx_mode_isreg(mode)) {
		return -EINVAL;
	}
	if (!dir_hasspace(dir)) {
		return -EMLINK;
	}
	return 0;
}

static int vproc_verify_nocycles(fnx_vproc_t     *vproc,
                                 const fnx_dir_t *dir,
                                 const fnx_dir_t *childd)
{
	int rc = 0;
	fnx_ino_t parent_ino;
	fnx_dir_t *parentd;
	const fnx_dir_t *ditr;

	ditr = childd;
	while (ditr && !fnx_dir_isroot(ditr)) {
		if (ditr == dir) {
			rc = -EINVAL;
			break;
		}
		parent_ino = ditr->d_inode.i_parentd;
		if (parent_ino == FNX_INO_NULL) {
			break;
		}
		rc = fnx_vproc_fetch_dir(vproc, parent_ino, &parentd);
		if (rc != 0) {
			break;
		}
		if (ditr == parentd) {
			break;
		}
		ditr = parentd;
	}
	return rc;
}

int fnx_vproc_let_rename_src(fnx_vproc_t       *vproc,
                             const fnx_task_t  *task,
                             const fnx_dir_t   *parentd,
                             const fnx_dir_t   *newparentd,
                             const fnx_inode_t *inode)
{
	int rc;
	const fnx_dir_t *dir;

	rc = vproc_let_dir_waccess(vproc, task, parentd);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_verify_sticky(vproc, task, parentd, inode);
	if (rc != 0) {
		return rc;
	}
	if ((dir = inode_to_dir(inode)) != NULL) {
		if (fnx_dir_isroot(dir)) {
			return -EBUSY;
		}
		rc = vproc_verify_nocycles(vproc, dir, newparentd);
		if (rc != 0) {
			return rc;
		}
	}
	rc = vproc_let_modify_dir(vproc, parentd);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_modify_dir(vproc, newparentd);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_modify_inode(vproc, inode);
	if (rc != 0) {
		return rc;
	}
	return 0;
}

int fnx_vproc_let_rename_tgt(fnx_vproc_t       *vproc,
                             const fnx_task_t  *task,
                             const fnx_dir_t   *newparentd,
                             const fnx_inode_t *curinode)
{
	int rc;
	const fnx_dir_t *dir;

	rc = vproc_let_dir_waccess(vproc, task, newparentd);
	if (rc != 0) {
		return rc;
	}
	if (!dir_hasspace(newparentd)) {
		return -EMLINK;
	}
	if (curinode == NULL) {
		return 0; /* OK, no target */
	}
	rc = vproc_verify_sticky(vproc, task, newparentd, curinode);
	if (rc != 0) {
		return rc;
	}
	if ((dir = inode_to_dir(curinode)) != NULL) {
		rc  = fnx_vproc_let_rmdir(vproc, task, newparentd, dir);
		if (rc != 0) {
			return rc;
		}
	}
	rc = vproc_let_modify_dir(vproc, newparentd);
	if (rc != 0) {
		return rc;
	}
	rc = vproc_let_modify_inode(vproc, curinode);
	if (rc != 0) {
		return rc;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Special case for I/O operations on reg/dir files which are referenced.
 */
static int vproc_grab_inode(fnx_vproc_t      *vproc,
                            const fnx_task_t *task,
                            fnx_ino_t         ino,
                            fnx_inode_t     **inode)
{
	const fnx_fileref_t *fref = task_get_fileref(task);

	if (!fref || !fref->f_inode) {
		return -EBADF;
	}
	if (!fnx_inode_hasino(fref->f_inode, ino)) {
		return -EBADF;
	}
	*inode = fref->f_inode;
	fnx_unused(vproc);
	return 0;
}

int fnx_vproc_grab_reg(fnx_vproc_t *vproc, const fnx_task_t *task,
                       fnx_ino_t ino, fnx_reg_t **reg)
{
	int rc;
	fnx_inode_t *inode = NULL;

	rc = vproc_grab_inode(vproc, task, ino, &inode);
	if (rc != 0) {
		return rc;
	}
	if (!fnx_inode_isreg(inode)) {
		return -EBADF;
	}
	*reg = fnx_inode_to_reg(inode);
	return 0;
}

int fnx_vproc_grab_dir(fnx_vproc_t *vproc, const fnx_task_t *task,
                       fnx_ino_t ino, fnx_dir_t **dir)
{
	int rc;
	fnx_inode_t *inode = NULL;

	rc = vproc_grab_inode(vproc, task, ino, &inode);
	if (rc != 0) {
		return rc;
	}
	if (!fnx_inode_isdir(inode)) {
		return -ENOTDIR;
	}
	*dir = fnx_inode_to_dir(inode);
	return 0;
}


