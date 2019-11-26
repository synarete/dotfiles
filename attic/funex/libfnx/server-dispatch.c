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
#include <fnxinfra.h>
#include <fnxcore.h>
#include <fnxpstor.h>
#include <fnxvproc.h>
#include <fnxfusei.h>
#include "server-exec.h"


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void server_dispatch_job(fnx_server_t *server, fnx_jelem_t *jelem)
{
	fnx_jobsq_t *jobsq;
	fnx_size_t idx;

	switch (jelem->jtype) {
		case FNX_JOB_TASK_EXEC_REQ:
			jobsq = &server->vproc_jobsq;
			fnx_jobsq_enqueue(jobsq, jelem, 1);
			break;
		case FNX_JOB_TASK_EXEC_RES:
			jobsq = &server->fusei_jobsq;
			fnx_jobsq_enqueue(jobsq, jelem, 0);
			break;
		case FNX_JOB_TASK_FINI_REQ:
			jobsq = &server->vproc_jobsq;
			fnx_jobsq_enqueue(jobsq, jelem, 1);
			break;
		case FNX_JOB_TASK_FINI_RES:
			jobsq = &server->fusei_jobsq;
			fnx_jobsq_enqueue(jobsq, jelem, 0);
			break;
		case FNX_JOB_BK_READ_REQ:
		case FNX_JOB_BK_WRITE_REQ:
		case FNX_JOB_BK_SYNC_REQ:
			idx = server->nsio++ % fnx_nelems(server->pstor_sio);
			jobsq = &server->pstor_sio[idx].sio_jobsq;
			fnx_jobsq_enqueue(jobsq, jelem, 1);
			break;
		case FNX_JOB_BK_READ_RES:
		case FNX_JOB_BK_WRITE_RES:
		case FNX_JOB_BK_SYNC_RES:
			jobsq = &server->vproc_jobsq;
			fnx_jobsq_enqueue(jobsq, jelem, 0);
			break;
		case FNX_JOB_NONE:
		default:
			fnx_panic("illegal-job: jtype=%d", (int)jelem->jtype);
			break;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_server_dispatch_job(void *ptr, fnx_jelem_t *jelem)
{
	fnx_server_t *server = (fnx_server_t *)ptr;

	fnx_assert(server->magic == FNX_SERVER_MAGIC);
	fnx_assert((server->state == FNX_SERVSTATE_ACTIVE) ||
	           (server->state == FNX_SERVSTATE_DRAIN));
	server_dispatch_job(server, jelem);
	return FNX_EDELAY;
}
