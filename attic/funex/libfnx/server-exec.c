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
#include <stdarg.h>
#include <errno.h>
#include <limits.h>

#include <fnxinfra.h>
#include <fnxcore.h>
#include <fnxpstor.h>
#include <fnxvproc.h>
#include <fnxfusei.h>
#include "server-exec.h"


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void report_thread_started(const fnx_thread_t *thread)
{
	fnx_info("thread-started: %s tid=%d", thread->th_name, thread->th_tid);
}

static void report_thread_completed(const fnx_thread_t *thread)
{
	fnx_info("thread-completed: %s tid=%d", thread->th_name, thread->th_tid);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const char *mntpoint(const fnx_server_t *server)
{
	return server->fusei->fi_mntpoint;
}

static fnx_bool_t isemptyq(const fnx_jobsq_t *jobsq)
{
	return fnx_jobsq_isempty(jobsq);
}

static int hasdirty(const fnx_server_t *server)
{
	fnx_unused(server);
	return FNX_FALSE; /* XXX */
}

static int haspending(const fnx_server_t *server)
{
	return !fnx_pendq_isempty(&server->vproc.pendq);
}

static int haspendirty(const fnx_server_t *server)
{
	return haspending(server) || hasdirty(server);
}

static int hasqueued(const fnx_server_t *server)
{
	if (haspending(server)) {
		return FNX_TRUE;
	}
	if (!isemptyq(&server->fusei_jobsq)) {
		return FNX_TRUE;
	}
	if (!isemptyq(&server->vproc_jobsq)) {
		return FNX_TRUE;
	}
	for (size_t i = 0; i < fnx_nelems(server->pstor_sio); ++i) {
		if (!isemptyq(&server->pstor_sio[i].sio_jobsq)) {
			return FNX_TRUE;
		}
	}
	return FNX_FALSE;
}

static int hasopenf(const fnx_server_t *server)
{
	return fnx_vproc_hasopenf(&server->vproc);
}

static int isactive(const fnx_server_t *server)
{
	/* Normal case (run-time common) */
	if ((server->state == FNX_SERVSTATE_ACTIVE) ||
	    (server->state == FNX_SERVSTATE_DRAIN)) {
		return FNX_TRUE;
	}
	/* Tear-down case; make sure all queues are flushed */
	if (hasqueued(server)) {
		return FNX_TRUE;
	}
	/* No shutdown while has dirty */
	if (hasdirty(server)) {
		return FNX_TRUE;
	}
	/* No shutdown while has open files */
	if (hasopenf(server)) {
		return FNX_TRUE;
	}
	return FNX_FALSE;
}

static fnx_jelem_t *popjobs(fnx_jobsq_t *jobsq)
{
	return fnx_jobsq_dequeue(jobsq, 50000);
}

static fnx_jelem_t *getfirst(fnx_jelem_t **jlist)
{
	fnx_link_t *next;
	fnx_jelem_t *jelem;

	if ((jelem = *jlist) != NULL) {
		fnx_assert(jelem->qlink.prev == NULL);
		next = jelem->qlink.next;
		if (next != NULL) {
			next->prev = NULL;
			*jlist = fnx_qlink_to_jelem(next);
		} else {
			*jlist = NULL;
		}
		fnx_link_init(&jelem->qlink);
	}
	return jelem;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void server_sio_execute_pstorq(fnx_server_sio_t *pstor_sio)
{
	fnx_jelem_t *jelem, *jlist;
	fnx_server_t *server = pstor_sio->sio_server;

	jlist = popjobs(&pstor_sio->sio_jobsq);
	while ((jelem = getfirst(&jlist)) != NULL) {
		fnx_pstor_exec_job(&server->pstor, jelem);
	}
}

static void run_pstor_sio(void *p)
{
	fnx_server_sio_t *pstor_sio = (fnx_server_sio_t *)p;
	fnx_server_t *server = pstor_sio->sio_server;

	report_thread_started(&pstor_sio->sio_thread);
	while (isactive(server)) {
		server_sio_execute_pstorq(pstor_sio);
	}
	report_thread_completed(&pstor_sio->sio_thread);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void server_execute_vproc_jobsq(fnx_server_t *server)
{
	fnx_jelem_t *jelem, *jlist;

	server->nexec = 0;
	jlist = popjobs(&server->vproc_jobsq);
	while ((jelem = getfirst(&jlist)) != NULL) {
		server->nexec += 1;
		fnx_vproc_exec_job(&server->vproc, jelem);
	}
}

static void server_execute_vproc_pendq(fnx_server_t *server)
{
	fnx_vproc_exec_pendq(&server->vproc);
}

static void server_adjust_space(fnx_server_t *server)
{
	/* FIXME */
	fnx_unused(server);
}

static void server_adjust_cache(fnx_server_t *server)
{
	size_t cur, lim, cnt;
	const fnx_alloc_t *alloc = &server->alloc;

	cur = fnx_alloc_total_nbk(alloc);
	lim = alloc->limit;
	if (cur > lim) {
		/* Aggressive */
		cnt = cur / 2;
		fnx_vproc_squeeze(&server->vproc, cnt);
		fnx_pstor_squeeze(&server->pstor, cnt);
	} else if (cur > (lim / 8)) {
		/* Normal */
		cnt = 1UL << ((4 * lim) / cur);
		fnx_vproc_squeeze(&server->vproc, cnt);
		fnx_pstor_squeeze(&server->pstor, cnt);
	} else if ((cur > (lim / 32)) && !server->nexec) {
		/* Idle */
		cnt = 1UL;
		fnx_vproc_squeeze(&server->vproc, cnt);
		fnx_pstor_squeeze(&server->pstor, cnt);
	}

	/* Tighten caches upon shutdown */
	if (!server->vproc.super->su_active) {
		fnx_vproc_squeeze(&server->vproc, 8);
		fnx_pstor_squeeze(&server->pstor, 8);
	}
}


static void server_adjust_dirty(fnx_server_t *server)
{
	fnx_vproc_t *vproc = &server->vproc;

	if (server->state == FNX_SERVSTATE_DRAIN) {
		fnx_vproc_commit_dirty(vproc);
	}
}

/*
 * If super has been deactivated, force flush of all-dirty and then trigger
 * halt via fusei when no more requests and no open files left.
 * Flush-dirty threshold is determined by currently open files.
 */
static void server_probe_triggers(fnx_server_t *server)
{
	fnx_vproc_t *vproc = &server->vproc;
	fnx_fusei_t *fusei = server->fusei;

	if (!vproc->super->su_active) {
		if (!fnx_vproc_hasopenf(vproc)) {
			fnx_vproc_commit_dirty(vproc);
			if (!haspendirty(server)) {
				fusei->fi_active = FNX_FALSE;
			}
			server->state = FNX_SERVSTATE_DRAIN;
		}
	}
	if (fusei->fi_rx_done) {
		server->state = FNX_SERVSTATE_TERM;
	}
}

static void server_execute_core(fnx_server_t *server)
{
	/* Process next in-coming task/bk */
	server_execute_vproc_jobsq(server);

	/* Re-execute tasks revoked by reply-bk */
	server_execute_vproc_pendq(server);

	/* Do space adjustments */
	server_adjust_space(server);

	/* Do caching adjustments */
	server_adjust_cache(server);

	/* Force cleanups upon shutdown */
	server_adjust_dirty(server);

	/* Reorder execution state */
	server_probe_triggers(server);
}

static void run_vproc(void *p)
{
	fnx_server_t *server = (fnx_server_t *)p;

	report_thread_started(&server->vproc_thread);
	while (isactive(server)) {
		server_execute_core(server);
	}
	report_thread_completed(&server->vproc_thread);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void server_execute_fusei_task(fnx_server_t *server, fnx_task_t *task)
{
	fnx_fusei_t *fusei = server->fusei;

	/* Task finished life-cycle after read-cleanup purification */
	if (task->tsk_jelem.jtype == FNX_JOB_TASK_FINI_RES) {
		fnx_fusei_del_task(fusei, task);
		return;
	}
	/* Normal response execution */
	fnx_fusei_execute_tx(fusei, task);

	/* Read-cleanup: need to release blocks via re-send */
	if (task->tsk_opcode == FNX_VOP_READ) {
		task->tsk_jelem.jtype = FNX_JOB_TASK_FINI_REQ;
		fnx_server_dispatch_job(server, &task->tsk_jelem);
		return;
	}
	/* End of task's life-cycle */
	fnx_fusei_del_task(fusei, task);
}

static void server_execute_fusei_tx(fnx_server_t *server)
{
	fnx_task_t *task;
	fnx_jelem_t *jelem, *jlist;

	jlist = popjobs(&server->fusei_jobsq);
	while ((jelem = getfirst(&jlist)) != NULL) {
		fnx_assert((jelem->jtype == FNX_JOB_TASK_EXEC_RES) ||
		           (jelem->jtype == FNX_JOB_TASK_FINI_RES));
		task  = fnx_jelem_to_task(jelem);
		server_execute_fusei_task(server, task);
	}
}

static void run_fusei_tx(void *p)
{
	fnx_server_t *server = (fnx_server_t *)p;

	report_thread_started(&server->fusei_tx_thread);
	while (isactive(server)) {
		server_execute_fusei_tx(server);
	}
	report_thread_completed(&server->fusei_tx_thread);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void run_fusei_rx(void *p)
{
	fnx_server_t *server = (fnx_server_t *)p;

	report_thread_started(&server->fusei_rx_thread);
	fnx_fusei_process_rx(server->fusei);
	report_thread_completed(&server->fusei_rx_thread);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_server_start(fnx_server_t *server)
{
	fnx_thread_t *thread;

	fnx_info("start-sub-threads: mountpoint=%s", mntpoint(server));
	server->state = FNX_SERVSTATE_ACTIVE;
	for (size_t i = 0; i < fnx_nelems(server->pstor_sio); ++i) {
		thread = &server->pstor_sio[i].sio_thread;
		fnx_thread_setname(thread, "fnx-pstor-sio");
		fnx_thread_start(thread, run_pstor_sio, &server->pstor_sio[i]);
	}
	thread = &server->vproc_thread;
	fnx_thread_setname(thread, "fnx-vproc");
	fnx_thread_start(thread, run_vproc, server);
	thread = &server->fusei_tx_thread;
	fnx_thread_setname(thread, "fnx-fusei-tx");
	fnx_thread_start(thread, run_fusei_tx, server);
	thread = &server->fusei_rx_thread;
	fnx_thread_setname(thread, "fnx-fusei-rx");
	fnx_thread_start(thread, run_fusei_rx, server);
	return 0;
}

int fnx_server_stop(fnx_server_t *server)
{
	const int flags = FNX_NOFAIL;

	fnx_info("stop-sub-threads: mountpoint=%s", mntpoint(server));
	server->state = FNX_SERVSTATE_NONE;
	fnx_thread_join(&server->fusei_rx_thread, flags);
	fnx_thread_join(&server->fusei_tx_thread, flags);
	fnx_thread_join(&server->vproc_thread, flags);
	for (size_t i = 0; i < fnx_nelems(server->pstor_sio); ++i) {
		fnx_thread_join(&server->pstor_sio[i].sio_thread, flags);
	}
	fnx_fusei_term(server->fusei);
	return 0;
}

void fnx_server_getcstats(const fnx_server_t *server, fnx_cstats_t *cstats)
{
	const fnx_cache_t *cache = &server->cache;

	fnx_bzero(cstats, sizeof(*cstats));
	cstats->cs_vnodes = cache->vc.vhtbl.size;
	cstats->cs_spmaps = cache->uc.smhtbl.size;
	cstats->cs_blocks = cache->uc.bkhtbl.size;
}
