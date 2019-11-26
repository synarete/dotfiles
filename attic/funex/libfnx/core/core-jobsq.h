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
#ifndef FNX_CORE_JOBSQ_H_
#define FNX_CORE_JOBSQ_H_

/* Threads queuing mechanism */
struct fnx_jobsq {
	fnx_fifo_t fifo;
};
typedef struct fnx_jobsq fnx_jobsq_t;


/* Pending (+ temporal-modified temporal) elements queuing */
struct fnx_pendq {
	fnx_list_t      pq;     /* Pending-objects queue */
	fnx_list_t      sq;     /* Staging-objects queue */
};
typedef struct fnx_pendq fnx_pendq_t;


/* Dispatching callback */
typedef int (*fnx_dispatch_fn)(void *, fnx_jelem_t *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_jelem_t *fnx_qlink_to_jelem(const fnx_link_t *);

fnx_jelem_t *fnx_plink_to_jelem(const fnx_link_t *);


fnx_bkref_t *fnx_qlink_to_bkref(const fnx_link_t *);

fnx_bkref_t *fnx_jelem_to_bkref(const fnx_jelem_t *);

fnx_link_t *fnx_bkref_to_qlink(const fnx_bkref_t *);

fnx_vnode_t *fnx_jelem_to_vnode(const fnx_jelem_t *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_jelem_init(fnx_jelem_t *);

void fnx_jelem_destroy(fnx_jelem_t *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_jobsq_init(fnx_jobsq_t *);

void fnx_jobsq_destroy(fnx_jobsq_t *);

int fnx_jobsq_isempty(const fnx_jobsq_t *);

void fnx_jobsq_enqueue(fnx_jobsq_t *, fnx_jelem_t *, int);

fnx_jelem_t *fnx_jobsq_dequeue(fnx_jobsq_t *jobsq, unsigned);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_pendq_init(fnx_pendq_t *);

void fnx_pendq_destroy(fnx_pendq_t *);

int fnx_pendq_isempty(const fnx_pendq_t *);

void fnx_pendq_pend(fnx_pendq_t *, fnx_jelem_t *);

void fnx_pendq_unpend(fnx_pendq_t *, fnx_jelem_t *);

fnx_jelem_t *fnx_pendq_pfront(const fnx_pendq_t *);

void fnx_pendq_stage(fnx_pendq_t *, fnx_jelem_t *);

void fnx_pendq_unstage(fnx_pendq_t *, fnx_jelem_t *);

fnx_jelem_t *fnx_pendq_sfront(const fnx_pendq_t *);



#endif /* FNX_CORE_JOBSQ_H_ */

