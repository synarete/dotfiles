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
#ifndef FNX_INFRA_FIFO_H_
#define FNX_INFRA_FIFO_H_


/*
 * Linked-queue over user-provided (intrusive) elements, with first-in-first-out
 * semantics. Thread safe.
 *
 * Supports two-levels priority. Underneath, the object maintains two linked
 * lists (queues): one for normal objects and one for high-priority objects.
 * Insertion with priority will cause more urgent elements will to be removed
 * first.
 */
struct fnx_fifo {
	size_t      ff_npend;
	fnx_lock_t  ff_lock;
	fnx_list_t  ff_list[2];
};
typedef struct fnx_fifo fnx_fifo_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Construct an an empty object.
 */
void fnx_fifo_init(fnx_fifo_t *);

/*
 * Destroy existing object. It is up to the user to release any user-provided
 * resources still referenced by the internal list-queue.
 */
void fnx_fifo_destroy(fnx_fifo_t *);


/*
 * Returns the total number of currently stored elements in queue.
 * NB: Not thread safe (no locking).
 */
size_t fnx_fifo_size(const fnx_fifo_t *);

/*
 * Return TRUE if size is zero.
 */
int fnx_fifo_isempty(const fnx_fifo_t *);

/*
 * Push single user-provided element to end-of-queue. For the second call, q may
 * be either 0 for high-priority elements, 1 for normal elements.
 */
void fnx_fifo_push(fnx_fifo_t *, fnx_link_t *);

void fnx_fifo_pushq(fnx_fifo_t *, fnx_link_t *, int q);

/*
 * Pops single element from front-of-queue without any wait on condition. If
 * queue is empty, returns NULL.
 */
fnx_link_t *fnx_fifo_trypop(fnx_fifo_t *fifo);

/*
 * Pops single element from front-of-queue. Returns the removed element, or
 * NULL pointer if timed-out.
 */
fnx_link_t *fnx_fifo_pop(fnx_fifo_t *fifo, size_t usec_timeout);

/*
 * Pops multiple elements from front-of-queue. Returns a chain of up to n
 * removed element, or NULL pointer if timed-out. Does not pend more then usec
 * timeout until the first element is available. Thus, it may return a list of
 * less then n elements.
 */
fnx_link_t *fnx_fifo_popn(fnx_fifo_t *fifo, size_t n, size_t usec_timeout);

/*
 * Pops multiple elements from front-of-queue. Returns a chain of removed
 * element, or NULL pointer if timed-out. Does not pend more then usec
 * timeout until the first element is available.
 */
fnx_link_t *fnx_fifo_popq(fnx_fifo_t *fifo, size_t usec_timeout);

#endif /* FNX_INFRA_FIFO_H_ */




