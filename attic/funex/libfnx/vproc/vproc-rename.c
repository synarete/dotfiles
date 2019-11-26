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

#include <fnxinfra.h>
#include <fnxcore.h>
#include <fnxpstor.h>
#include "vproc-elems.h"
#include "vproc-let.h"
#include "vproc-exec.h"


/*
 * Rename existing entry within same parent-directory.
 */
static int vproc_exec_rename_inplace(fnx_vproc_t      *vproc,
                                     fnx_dir_t        *parentd,
                                     fnx_inode_t      *inode,
                                     const fnx_name_t *newname)
{
	int rc;

	rc = fnx_vproc_prep_rename(vproc, parentd, inode, newname);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_rename_inplace(vproc, parentd, inode, newname);
	if (rc != 0) {
		return rc;
	}
	return 0;
}

/*
 * Rename-move existing entry between directories as a two-steps sequence of
 * unlink and link.
 */
static int vproc_exec_rename_move(fnx_vproc_t      *vproc,
                                  fnx_dir_t        *parentd,
                                  fnx_inode_t      *inode,
                                  fnx_dir_t        *newparentd,
                                  const fnx_name_t *newname)
{
	int rc;

	rc = fnx_vproc_prep_unlink(vproc, parentd, inode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_prep_link(vproc, newparentd, newname);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_unlink_child(vproc, parentd, inode);
	fnx_assert(rc == 0); /* XXX */
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_link_child(vproc, newparentd, inode, newname);
	fnx_assert(rc == 0); /* XXX */
	if (rc != 0) {
		return rc;
	}
	return 0;
}

/*
 * Rename existing entry within same parent-directory with implicit unlink
 * existing current-child.
 */
static int vproc_exec_rename_replace(fnx_vproc_t      *vproc,
                                     fnx_dir_t        *parentd,
                                     fnx_inode_t      *inode,
                                     fnx_inode_t      *curinode)
{
	int rc;

	rc = fnx_vproc_prep_xunlink(vproc, parentd, curinode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_prep_unlink(vproc, parentd, inode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_rename_replace(vproc, parentd, inode, curinode);
	if (rc != 0) {
		return rc;
	}
	return 0;
}

/*
 * Rename-move existing entry between directories with implicit unlink
 * existing current-child in destination directory. Using a a sequence of
 * unlinks-link
 */
static int vproc_exec_rename_override(fnx_vproc_t      *vproc,
                                      fnx_dir_t        *parentd,
                                      fnx_inode_t      *inode,
                                      fnx_dir_t        *newparentd,
                                      fnx_inode_t      *curinode)
{
	int rc;

	rc = fnx_vproc_prep_xunlink(vproc, newparentd, curinode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_prep_unlink(vproc, parentd, inode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_rename_override(vproc, parentd, inode, newparentd, curinode);
	if (rc != 0) {
		return rc;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_vproc_exec_rename(fnx_vproc_t *vproc, fnx_task_t *task)
{
	int rc = 0;
	fnx_ino_t parent_ino, newparent_ino;
	fnx_inode_t *inode, *curinode = NULL;
	fnx_dir_t *parentd, *newparentd;
	const fnx_name_t *name;
	const fnx_name_t *newname;
	const fnx_request_t *rqst = &task->tsk_request;

	parent_ino      = rqst->req_rename.parent;
	name            = &rqst->req_rename.name;
	newparent_ino   = rqst->req_rename.newparent;
	newname         = &rqst->req_rename.newname;

	rc = fnx_vproc_fetch_dir(vproc, parent_ino, &parentd);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_fetch_dir(vproc, newparent_ino, &newparentd);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_namespace(vproc, task, parentd, name);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_namespace(vproc, task, newparentd, newname);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_lookup_iinode(vproc, parentd, name, &inode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_lookup_ientry(vproc, newparentd, newname, &curinode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_rename_src(vproc, task, parentd, newparentd, inode);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_vproc_let_rename_tgt(vproc, task, newparentd, curinode);
	if (rc != 0) {
		return rc;
	}

	if ((parentd == newparentd) && !curinode) {
		rc = vproc_exec_rename_inplace(vproc, parentd, inode, newname);
	} else if ((parentd == newparentd) && curinode) {
		rc = vproc_exec_rename_replace(vproc, parentd, inode, curinode);
	} else if ((parentd != newparentd) && !curinode) {
		rc = vproc_exec_rename_move(vproc, parentd, inode,
		                            newparentd, newname);
	} else {
		rc = vproc_exec_rename_override(vproc, parentd, inode,
		                                newparentd, curinode);
	}
	if (rc != 0) {
		return rc;
	}

	rc = fnx_vproc_fix_unlinked(vproc, curinode);
	fnx_assert(rc == 0); /* XXX */
	if (rc != 0) {
		return rc;
	}
	return 0;
}
