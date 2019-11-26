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
#ifndef FUNEX_RECORDQ_H_
#define FUNEX_RECORDQ_H_

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Reentrant wrapper over raw-memory buffer. Provides low-level thread-safe
 * FIFO operations on user-provided memory region.
 *
 * NB: most likely you want to use fnx_recordq (see below).
 */
struct fnx_queue {
	fnx_lock_t   q_lock;
	fnx_buffer_t q_buf;
};
typedef struct fnx_queue  fnx_queue_t;


/* Constructor */
void fnx_queue_init(fnx_queue_t *queue, void *mem, size_t sz);

/* Destructor */
void fnx_queue_destroy(fnx_queue_t *queue);

/* Push n-bytes to end-of-queue. Returns 0 upon success, -1 if no room */
int fnx_queue_push_back(fnx_queue_t *queue, const void *p, size_t n);

/*
 * Pops n-bytes to end-of-queue. Returns 0 upon success, -1 if timed-out. If
 * n_poped is non-NULL, sets it with the number of dequeued bytes.
 */
int fnx_queue_pop_front(fnx_queue_t *queue, void *p, size_t n,
                        size_t usec_timeout, size_t *n_poped);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Reentrant container which enables equally-sized records-queue FIFO
 * semantics over user-provided memory region. The record size remains fixed
 * throughout the entire life-cycle of the object.
 */
struct fnx_recordq {
	fnx_queue_t  rq_queue;
	size_t  rq_recsz;   /* In bytes */
	size_t  rq_maxsz;   /* In recs */
	size_t  rq_size;    /* Num elements */
};
typedef struct fnx_recordq      fnx_recordq_t;


/* Allocates and constructs records-queue object */
fnx_recordq_t *fnx_recordq_new(size_t rec_size, size_t max_recs);

/* Destroy records-queue object and free its memory */
void fnx_recordq_del(fnx_recordq_t *mq);

/* Constructor over user-provided memory region */
void fnx_recordq_init(fnx_recordq_t *, size_t rec_sz, void *mem, size_t mem_sz);

/* Destructor */
void fnx_recordq_destroy(fnx_recordq_t *);

/* Returns the current number of elements in queue */
size_t fnx_recordq_size(const fnx_recordq_t *);

/* Push single record to end-of-queue */
int fnx_recordq_push(fnx_recordq_t *, const void *);

/* Pops single record from front-of-queue */
int fnx_recordq_pop(fnx_recordq_t *, void *p, size_t usec_timeout);


#endif /* FUNEX_RECORDQ_H_ */

