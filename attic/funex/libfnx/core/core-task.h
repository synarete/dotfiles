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
#ifndef FNX_CORE_TASK_H_
#define FNX_CORE_TASK_H_


/* FUSE types */
struct fnx_fusei;
struct fuse_req;
struct fuse_chan;
typedef struct fuse_req  *fnx_fuse_req_t;
typedef struct fuse_chan *fnx_fuse_chan_t;


/* Execution-context (task) */
struct fnx_aligned fnx_task {
	struct fnx_fusei   *tsk_fusei;      /* Parent FUSE-interface */
	fnx_fileref_t      *tsk_fref;       /* Optional file-reference */
	fnx_jelem_t         tsk_jelem;      /* Execution-queue job-object */
	fnx_uctx_t          tsk_uctx;       /* User's context */
	fnx_vopcode_e       tsk_opcode;     /* Sub-command opcode */
	fnx_magic_t         tsk_magic;      /* Debug checker */
	fnx_timespec_t      tsk_start;      /* Execution start-time */
	fnx_size_t          tsk_seqno;      /* Sequential task-id */
	fnx_size_t          tsk_runcnt;     /* Number of sub-executions */
	fnx_request_t       tsk_request;    /* Request params */
	fnx_response_t      tsk_response;   /* Response params */
	fnx_iobufs_t        tsk_iobufs;
	fnx_fuse_req_t      tsk_fuse_req;
	fnx_fuse_chan_t     tsk_fuse_chan;
};
typedef struct fnx_task fnx_task_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_task_init(fnx_task_t *, fnx_vopcode_e);

void fnx_task_destroy(fnx_task_t *);

void fnx_task_check(const fnx_task_t *);

int fnx_task_status(const fnx_task_t *);

void fnx_task_setup_nhash(fnx_task_t *);


void fnx_send_task(fnx_jobsq_t *, fnx_task_t *, fnx_bool_t);

fnx_task_t *fnx_link_to_task(const fnx_link_t *);

fnx_task_t *fnx_jelem_to_task(const fnx_jelem_t *);


#endif /* FNX_CORE_TASK_H_ */

