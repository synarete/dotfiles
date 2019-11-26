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
#include "core-addr.h"
#include "core-elems.h"
#include "core-jobsq.h"



fnx_jelem_t *fnx_qlink_to_jelem(const fnx_link_t *qlink)
{
	const fnx_jelem_t *jelem;

	jelem = fnx_container_of(qlink, fnx_jelem_t, qlink);
	return (fnx_jelem_t *)jelem;
}

fnx_jelem_t *fnx_plink_to_jelem(const fnx_link_t *plink)
{
	const fnx_jelem_t *jelem;

	jelem = fnx_container_of(plink, fnx_jelem_t, plink);
	return (fnx_jelem_t *)jelem;
}

static fnx_jelem_t *fnx_slink_to_jelem(const fnx_link_t *slink)
{
	const fnx_jelem_t *jelem;

	jelem = fnx_container_of(slink, fnx_jelem_t, slink);
	return (fnx_jelem_t *)jelem;
}


fnx_link_t *fnx_bkref_to_qlink(const fnx_bkref_t *bkref)
{
	const fnx_link_t *qlink = &bkref->bk_jelem.qlink;
	return (fnx_link_t *)qlink;
}

fnx_bkref_t *fnx_qlink_to_bkref(const fnx_link_t *qlnk)
{
	const fnx_jelem_t *jelem;

	jelem = fnx_qlink_to_jelem(qlnk);
	return fnx_jelem_to_bkref(jelem);
}

fnx_bkref_t *fnx_jelem_to_bkref(const fnx_jelem_t *jelem)
{
	const fnx_bkref_t *bkref;

	bkref = fnx_container_of(jelem, fnx_bkref_t, bk_jelem);
	fnx_assert(bkref->bk_magic == FNX_BKREF_MAGIC);
	return (fnx_bkref_t *)bkref;
}

fnx_vnode_t *fnx_jelem_to_vnode(const fnx_jelem_t *jelem)
{
	const fnx_vnode_t *vnode;

	vnode = fnx_container_of(jelem, fnx_vnode_t, v_jelem);
	fnx_assert(vnode->v_magic == FNX_VNODE_MAGIC);
	return (fnx_vnode_t *)vnode;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_jelem_init(fnx_jelem_t *jelem)
{
	fnx_link_init(&jelem->qlink);
	fnx_link_init(&jelem->plink);
	fnx_link_init(&jelem->slink);
	jelem->jtype    = FNX_JOB_NONE;
	jelem->status   = 0;
}

void fnx_jelem_destroy(fnx_jelem_t *jelem)
{
	fnx_assert(fnx_link_isunlinked(&jelem->slink));
	fnx_assert(fnx_link_isunlinked(&jelem->plink));
	fnx_assert(fnx_link_isunlinked(&jelem->qlink));

	fnx_link_destroy(&jelem->plink);
	fnx_link_destroy(&jelem->qlink);
	fnx_link_destroy(&jelem->slink);
	jelem->status   = -40000;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_jobsq_init(fnx_jobsq_t *jobsq)
{
	fnx_fifo_init(&jobsq->fifo);
}

void fnx_jobsq_destroy(fnx_jobsq_t *jobsq)
{
	fnx_fifo_destroy(&jobsq->fifo);
}

int fnx_jobsq_isempty(const fnx_jobsq_t *jobsq)
{
	return fnx_fifo_isempty(&jobsq->fifo);
}

void fnx_jobsq_enqueue(fnx_jobsq_t *jobsq, fnx_jelem_t *jelem, int q)
{
	fnx_fifo_pushq(&jobsq->fifo, &jelem->qlink, q);
}

fnx_jelem_t *fnx_jobsq_dequeue(fnx_jobsq_t *jobsq, unsigned timeout)
{
	fnx_link_t  *qlink;
	fnx_jelem_t *jelem = NULL;

	qlink = fnx_fifo_popq(&jobsq->fifo, timeout);
	if (qlink != NULL) {
		jelem = fnx_qlink_to_jelem(qlink);
	}
	return jelem;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_pendq_init(fnx_pendq_t *pendq)
{
	fnx_list_init(&pendq->pq);
	fnx_list_init(&pendq->sq);
}

void fnx_pendq_destroy(fnx_pendq_t *pendq)
{
	fnx_list_destroy(&pendq->pq);
	fnx_list_destroy(&pendq->sq);
}

int fnx_pendq_isempty(const fnx_pendq_t *pendq)
{
	return fnx_list_isempty(&pendq->pq) && fnx_list_isempty(&pendq->sq);
}

void fnx_pendq_pend(fnx_pendq_t *pendq, fnx_jelem_t *jelem)
{
	fnx_link_t *plink = &jelem->plink;

	fnx_assert(fnx_link_isunlinked(plink));
	fnx_list_push_back(&pendq->pq, plink);
}

void fnx_pendq_unpend(fnx_pendq_t *pendq, fnx_jelem_t *jelem)
{
	fnx_link_t *plink = &jelem->plink;

	fnx_assert(fnx_link_islinked(plink));
	fnx_list_remove(&pendq->pq, plink);
}

fnx_jelem_t *fnx_pendq_pfront(const fnx_pendq_t *pendq)
{
	fnx_link_t *plink;
	fnx_jelem_t *jelem = NULL;

	plink = fnx_list_front(&pendq->pq);
	if (plink != NULL) {
		jelem = fnx_plink_to_jelem(plink);
	}
	return jelem;
}

void fnx_pendq_stage(fnx_pendq_t *pendq, fnx_jelem_t *jelem)
{
	fnx_link_t *slink = &jelem->slink;

	if (fnx_link_isunlinked(slink)) {
		fnx_list_push_back(&pendq->sq, slink);
	}
}

void fnx_pendq_unstage(fnx_pendq_t *pendq, fnx_jelem_t *jelem)
{
	fnx_link_t *slink = &jelem->slink;

	if (fnx_link_islinked(slink)) {
		fnx_list_remove(&pendq->sq, slink);
	}
}

fnx_jelem_t *fnx_pendq_sfront(const fnx_pendq_t *pendq)
{
	fnx_link_t *slink;
	fnx_jelem_t *jelem = NULL;

	slink = fnx_list_front(&pendq->sq);
	if (slink != NULL) {
		jelem = fnx_slink_to_jelem(slink);
	}
	return jelem;
}


