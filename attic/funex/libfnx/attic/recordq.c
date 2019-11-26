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
#include <time.h>
#include <pthread.h>

#include "compiler.h"
#include "utility.h"
#include "buffer.h"
#include "timex.h"
#include "thread.h"
#include "recordq.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_queue_init(fnx_queue_t *q, void *mem, size_t sz)
{
	fnx_bzero(q, sizeof(*q));

	fnx_lock_init(&q->q_lock);
	fnx_buffer_init(&q->q_buf, mem, sz);
}

void fnx_queue_destroy(fnx_queue_t *q)
{
	fnx_buffer_destroy(&q->q_buf);
	fnx_lock_destroy(&q->q_lock);
	fnx_bff(q, sizeof(*q));
}

int fnx_queue_push_back(fnx_queue_t *q, const void *p, size_t n)
{
	size_t sz, maxsz;
	fnx_lock_t *lock;

	lock = &q->q_lock;
	fnx_lock_acquire(lock);

	sz      = fnx_buffer_size(&q->q_buf);
	maxsz   = fnx_buffer_maxsize(&q->q_buf);

	if (n > maxsz) {
		fnx_lock_release(lock);
		return -1;
	}

	if ((sz + n) > maxsz) {
		fnx_lock_release(lock);
		return -1;
	}

	fnx_buffer_push_back(&q->q_buf, p, n);

	fnx_lock_signal(lock);
	fnx_lock_release(lock);

	return 0;
}

int fnx_queue_pop_front(fnx_queue_t *q, void *p, size_t nbytes,
                        size_t usec_timeout, size_t *n_poped)
{
	int rc, n = 0;
	size_t sz;
	fnx_timespec_t ts;
	fnx_lock_t *lock;

	lock = &q->q_lock;
	fnx_lock_acquire(lock);

	fnx_timespec_getmonotime(&ts);
	fnx_timespec_usecadd(&ts, (long)usec_timeout);

	sz = fnx_buffer_size(&q->q_buf);
	while (sz == 0) {
		rc = fnx_lock_timedwait(lock, &ts);
		if (rc != 0) {
			fnx_lock_release(lock);
			return -1;
		}
		sz = fnx_buffer_size(&q->q_buf);
	}

	sz = fnx_buffer_size(&q->q_buf);
	if (sz > 0) {
		n = fnx_buffer_pop_front(&q->q_buf, p, nbytes);
	}
	fnx_lock_release(lock);

	if (n_poped != NULL) {
		*n_poped = (size_t) n;
	}

	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_recordq_init(fnx_recordq_t *rq,
                      size_t rec_size, void *mem, size_t mem_sz)
{
	fnx_queue_init(&rq->rq_queue, mem, mem_sz);

	rq->rq_recsz = rec_size;
	rq->rq_maxsz = mem_sz / rec_size;
	rq->rq_size  = 0;
}


void fnx_recordq_destroy(fnx_recordq_t *rq)
{
	const size_t zero = 0;

	rq->rq_recsz  = ~zero;
	rq->rq_maxsz  = ~zero;
	rq->rq_size   = ~zero;

	fnx_queue_destroy(&rq->rq_queue);
}


fnx_recordq_t *fnx_recordq_new(size_t rec_size, size_t max_recs)
{
	size_t sz1, sz2, align;
	fnx_recordq_t *rq;
	void *p;

	align = sizeof(void *);
	sz1   = ((sizeof(*rq) + align - 1) / align) * align;
	sz2   = rec_size * max_recs;

	rq = NULL;
	p  = fnx_malloc(sz1 + sz2);
	if (p != NULL) {
		rq = (fnx_recordq_t *)p;
		fnx_recordq_init(rq, rec_size, ((char *)p) + sz1, sz2);
	}

	return rq;
}


void fnx_recordq_del(fnx_recordq_t *rq)
{
	size_t sz1, sz2, align;
	uint8_t *p;

	align = sizeof(void *);
	sz1   = ((sizeof(*rq) + align - 1) / align) * align;
	sz2   = rq->rq_recsz * rq->rq_maxsz;

	p = (uint8_t *)fnx_buffer_umemory(&rq->rq_queue.q_buf);
	p -= sz1;

	fnx_recordq_destroy(rq);
	fnx_free(p, sz1 + sz2);
}


size_t fnx_recordq_size(const fnx_recordq_t *rq)
{
	return rq->rq_size;
}


int fnx_recordq_push(fnx_recordq_t *rq, const void *p_msg)
{
	int rc;
	fnx_queue_t *queue;

	queue = &rq->rq_queue;
	rc = fnx_queue_push_back(queue, p_msg, rq->rq_recsz);
	if (rc == 0) {
		rq->rq_size += 1;
	}

	return rc;
}

int fnx_recordq_pop(fnx_recordq_t *rq, void *p, size_t usec_timeout)
{
	int rc;
	size_t n = 0;
	fnx_queue_t *queue;

	queue = &rq->rq_queue;
	rc = fnx_queue_pop_front(queue, p, rq->rq_recsz, usec_timeout, &n);
	if (rc == 0) {
		rq->rq_size -= 1;
	}

	return rc;
}


