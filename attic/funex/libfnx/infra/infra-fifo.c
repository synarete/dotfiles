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
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

#include "infra-compiler.h"
#include "infra-macros.h"
#include "infra-panic.h"
#include "infra-timex.h"
#include "infra-utility.h"
#include "infra-atomic.h"
#include "infra-thread.h"
#include "infra-list.h"
#include "infra-fifo.h"

static void fifo_acquire_lock(fnx_fifo_t *fifo)
{
	fnx_lock_acquire(&fifo->ff_lock);
}

static void fifo_release_lock(fnx_fifo_t *fifo)
{
	fnx_lock_release(&fifo->ff_lock);
}

static void fifo_signal_lock(fnx_fifo_t *fifo)
{
	fnx_lock_signal(&fifo->ff_lock);
}

static int fifo_timedwait_lock(fnx_fifo_t *fifo, const fnx_timespec_t *ts)
{
	return fnx_lock_timedwait(&fifo->ff_lock, ts);
}

static size_t fifo_qsize(const fnx_fifo_t *fifo)
{
	size_t sz0, sz1;

	sz0 = fnx_list_size(&fifo->ff_list[0]);
	sz1 = fnx_list_size(&fifo->ff_list[1]);

	return (sz0 + sz1);
}

static void fifo_push(fnx_fifo_t *fifo, fnx_link_t *lnk, int q)
{
	fnx_list_t *list;

	list = q ? &fifo->ff_list[1] : &fifo->ff_list[0];
	fnx_list_push_back(list, lnk);
}

static fnx_link_t *fifo_pop(fnx_fifo_t *fifo)
{
	fnx_link_t *lnk;

	lnk = fnx_list_pop_front(&fifo->ff_list[0]);
	if (lnk == NULL) {
		lnk = fnx_list_pop_front(&fifo->ff_list[1]);
	}

	if (lnk != NULL) {
		lnk->next = lnk->prev = NULL;
	}
	return lnk;
}

static fnx_link_t *fifo_popq(fnx_fifo_t *fifo)
{
	fnx_link_t *lnk;

	lnk = fnx_list_clear(&fifo->ff_list[0]);
	if (lnk == NULL) {
		lnk = fnx_list_clear(&fifo->ff_list[1]);
	}
	return lnk;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_fifo_init(fnx_fifo_t *fifo)
{
	fnx_lock_init(&fifo->ff_lock);
	fnx_list_init(&fifo->ff_list[0]);
	fnx_list_init(&fifo->ff_list[1]);
	fifo->ff_npend = 0;
}

void fnx_fifo_destroy(fnx_fifo_t *fifo)
{
	fnx_lock_destroy(&fifo->ff_lock);
	fnx_list_destroy(&fifo->ff_list[0]);
	fnx_list_destroy(&fifo->ff_list[1]);
}

size_t fnx_fifo_size(const fnx_fifo_t *fifo)
{
	return fifo_qsize(fifo);
}

int fnx_fifo_isempty(const fnx_fifo_t *fifo)
{
	return (fifo_qsize(fifo) == 0);
}
void fnx_fifo_push(fnx_fifo_t *fifo, fnx_link_t *lnk)
{
	fnx_fifo_pushq(fifo, lnk, 0);
}

void fnx_fifo_pushq(fnx_fifo_t *fifo, fnx_link_t *lnk, int q)
{
	fifo_acquire_lock(fifo);
	fifo_push(fifo, lnk, q);
	if (fifo->ff_npend > 0) {
		fifo_signal_lock(fifo);
	}
	fifo_release_lock(fifo);
}

fnx_link_t *fnx_fifo_trypop(fnx_fifo_t *fifo)
{
	fnx_link_t *lnk;

	fifo_acquire_lock(fifo);
	lnk = fifo_pop(fifo);  /* NULL if empty */
	fifo_release_lock(fifo);

	return lnk;
}

fnx_link_t *fnx_fifo_pop(fnx_fifo_t *fifo, size_t usec_timeout)
{
	return fnx_fifo_popn(fifo, 1, usec_timeout);
}

fnx_link_t *fnx_fifo_popn(fnx_fifo_t *fifo, size_t n, size_t usec_timeout)
{
	int rc, err = 0;
	fnx_timespec_t ts;
	fnx_link_t lst;
	fnx_link_t *lnk;
	fnx_link_t **ppl;

	lst.next = lst.prev = NULL;
	ppl = &lst.next;

	if (fnx_unlikely(n == 0)) {
		return NULL;
	}

	fifo_acquire_lock(fifo);
	lnk = fifo_pop(fifo);
	if (lnk == NULL) {
		fnx_timespec_getmonotime(&ts);
		fnx_timespec_usecadd(&ts, (long)usec_timeout);

		fifo->ff_npend++;
		while (!fifo_qsize(fifo) && !err) {
			rc  = fifo_timedwait_lock(fifo, &ts);
			err = (rc != 0); /* ETIMEDOUT  | EINTR */
		}
		fifo->ff_npend--;
		if (!err) {
			lnk = fifo_pop(fifo);
		}
	}
	if (!err && lnk) {
		*ppl = lnk;
		ppl  = &lnk->next;
		while (--n && lnk) {
			lnk = fifo_pop(fifo);
			if (lnk != NULL) {
				*ppl = lnk;
				ppl  = &lnk->next;
			}
		}
	}
	fifo_release_lock(fifo);

	return lst.next;
}

fnx_link_t *fnx_fifo_popq(fnx_fifo_t *fifo, size_t usec_timeout)
{
	int rc, err = 0;
	fnx_timespec_t ts;
	fnx_link_t *lnk;

	fifo_acquire_lock(fifo);
	lnk = fifo_popq(fifo);
	if (lnk == NULL) {
		fnx_timespec_getmonotime(&ts);
		fnx_timespec_usecadd(&ts, (long)usec_timeout);

		fifo->ff_npend++;
		while (!fifo_qsize(fifo) && !err) {
			rc  = fifo_timedwait_lock(fifo, &ts);
			err = (rc != 0); /* ETIMEDOUT  | EINTR */
		}
		fifo->ff_npend--;
		if (!err) {
			lnk = fifo_popq(fifo);
		}
	}
	fifo_release_lock(fifo);

	return lnk;
}


