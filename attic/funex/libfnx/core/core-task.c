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

#include <fnxinfra.h>
#include "core-blobs.h"
#include "core-types.h"
#include "core-stats.h"
#include "core-proto.h"
#include "core-addr.h"
#include "core-addri.h"
#include "core-elems.h"
#include "core-iobuf.h"
#include "core-jobsq.h"
#include "core-task.h"
#include "core-inode.h"

#define TASK_MAGIC   (0xABCD)


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_task_t *fnx_jelem_to_task(const fnx_jelem_t *jelem)
{
	return fnx_container_of(jelem, fnx_task_t, tsk_jelem);
}

fnx_task_t *fnx_link_to_task(const fnx_link_t *qlnk)
{
	const fnx_jelem_t *jelem;

	jelem = fnx_qlink_to_jelem(qlnk);
	return fnx_jelem_to_task(jelem);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_task_init(fnx_task_t *task, fnx_vopcode_e opc)
{
	/* Make sure we did not get wild */
	fnx_staticassert(sizeof(*task) < (2 * FNX_BLKSIZE));

	fnx_bzero(task, sizeof(*task));
	fnx_jelem_init(&task->tsk_jelem);
	fnx_uctx_init(&task->tsk_uctx);
	fnx_iobufs_init(&task->tsk_iobufs);
	task->tsk_jelem.jtype   = FNX_JOB_TASK_EXEC_REQ;
	task->tsk_opcode        = opc;
	task->tsk_seqno         = 0;
	task->tsk_runcnt        = 0;
	task->tsk_fuse_req      = NULL;
	task->tsk_fuse_chan     = NULL;
	task->tsk_fref          = NULL;
	task->tsk_fusei         = NULL;
	task->tsk_magic         = TASK_MAGIC;
}

void fnx_task_destroy(fnx_task_t *task)
{
	fnx_iobufs_destroy(&task->tsk_iobufs);
	fnx_uctx_destroy(&task->tsk_uctx);
	fnx_jelem_destroy(&task->tsk_jelem);
	task->tsk_opcode        = FNX_VOP_NONE;
	task->tsk_seqno         = 0;
	task->tsk_fuse_req      = NULL;
	task->tsk_fuse_chan     = NULL;
	task->tsk_fref          = NULL;
	task->tsk_fusei         = NULL;
	task->tsk_magic         = 111;
}


int fnx_task_status(const fnx_task_t *task)
{
	return task->tsk_jelem.status;
}

void fnx_task_check(const fnx_task_t *task)
{
	fnx_assert(task != NULL);
	fnx_assert(task->tsk_magic == TASK_MAGIC);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_send_task(fnx_jobsq_t *taskq, fnx_task_t *task, fnx_bool_t hi)
{
	const int qid = hi ? 1 : 0;
	fnx_jobsq_enqueue(taskq, &task->tsk_jelem, qid);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void assign_nhash(fnx_name_t *name, fnx_ino_t parent_ino)
{
	name->hash = fnx_calc_inamehash(name, parent_ino);
}

void fnx_task_setup_nhash(fnx_task_t *task)
{
	switch ((int)task->tsk_opcode) {
		case FNX_VOP_LOOKUP:
			assign_nhash(&task->tsk_request.req_lookup.name,
			             task->tsk_request.req_lookup.parent);
			break;
		case FNX_VOP_MKNOD:
			assign_nhash(&task->tsk_request.req_mknod.name,
			             task->tsk_request.req_mknod.parent);
			break;
		case FNX_VOP_MKDIR:
			assign_nhash(&task->tsk_request.req_mkdir.name,
			             task->tsk_request.req_mkdir.parent);
			break;
		case FNX_VOP_UNLINK:
			assign_nhash(&task->tsk_request.req_unlink.name,
			             task->tsk_request.req_unlink.parent);
			break;
		case FNX_VOP_RMDIR:
			assign_nhash(&task->tsk_request.req_rmdir.name,
			             task->tsk_request.req_rmdir.parent);
			break;
		case FNX_VOP_SYMLINK:
			assign_nhash(&task->tsk_request.req_symlink.name,
			             task->tsk_request.req_symlink.parent);
			break;
		case FNX_VOP_LINK:
			assign_nhash(&task->tsk_request.req_link.newname,
			             task->tsk_request.req_link.newparent);
			break;
		case FNX_VOP_CREATE:
			assign_nhash(&task->tsk_request.req_create.name,
			             task->tsk_request.req_create.parent);
			break;
		case FNX_VOP_RENAME:
			assign_nhash(&task->tsk_request.req_rename.name,
			             task->tsk_request.req_rename.parent);
			assign_nhash(&task->tsk_request.req_rename.newname,
			             task->tsk_request.req_rename.newparent);
			break;
		default:
			break;
	}
}



