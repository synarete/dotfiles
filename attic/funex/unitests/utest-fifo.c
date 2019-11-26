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
#include <pthread.h>
#include <unistd.h>

#include <fnxinfra.h>

#define MAGIC       0xDEF1
#define NITEMS      7531
#define NTHREADS    7


struct item {
	fnx_link_t lnk;
	int index;
	int magic;
};
typedef struct item     item_t;

static fnx_fifo_t s_fifo;
static size_t s_in  = 0;
static size_t s_out = 0;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void item_check(const item_t *itm)
{
	fnx_assert(itm != NULL);
	fnx_assert(itm->magic == MAGIC);
	fnx_assert((itm->index >= 0) && (itm->index < NITEMS));
}

static item_t *item_new(int index_j)
{
	item_t *itm;

	itm = (item_t *)malloc(sizeof(item_t));
	fnx_assert(itm != NULL);
	memset(itm, 0, sizeof(*itm));
	itm->index  = index_j;
	itm->magic  = MAGIC;

	item_check(itm);
	return itm;
}

static void item_del(item_t *itm)
{
	item_check(itm);

	memset(itm, 0xff, sizeof(*itm));
	free(itm);
}

static item_t *item_from_lnk(const fnx_link_t *lnk)
{
	const item_t *itm;

	itm = fnx_container_of(lnk, item_t, lnk);
	item_check(itm);

	return (item_t *)itm;
}

static fnx_link_t *item_to_lnk(item_t *itm)
{
	return &itm->lnk;
}

static void *producer(void *p)
{
	int i, nitems = NITEMS;
	fnx_fifo_t *fifo = (fnx_fifo_t *)p;
	item_t *itm = NULL;

	for (i = 0; i < nitems; ++i) {
		itm = item_new(i);
		fnx_fifo_pushq(fifo, item_to_lnk(itm), (i % 2));

		fnx_atomic_size_inc(&s_in);

		if ((i % 19) == 0) {
			fnx_usleep(1);
		}
	}

	return NULL;
}

static void *consumer(void *p)
{
	int i, nitems = NITEMS;
	fnx_fifo_t *fifo = (fnx_fifo_t *)p;
	item_t *itm        = NULL;
	fnx_link_t *lnk    = NULL;

	i = 0;
	while (i < nitems) {
		lnk = NULL;
		if (i % 2) {
			lnk = fnx_fifo_trypop(fifo);
		}

		if (lnk == NULL) {
			lnk = fnx_fifo_pop(fifo, 1);
		}

		if (lnk != NULL) {
			itm = item_from_lnk(lnk);
			item_del(itm);
			++i;
			fnx_atomic_size_inc(&s_out);

			if ((i % 13) == 0) {
				fnx_usleep(1);
			}
		}
		lnk = NULL;
		itm = NULL;
	}
	return NULL;
}


static void test_fifo_threads(void)
{
	size_t sz, i;
	pthread_t *pth;
	pthread_t tid_prod[NTHREADS];
	pthread_t tid_cons[NTHREADS];
	fnx_fifo_t *fifo = &s_fifo;

	fnx_fifo_init(fifo);

	sz = fnx_fifo_size(fifo);
	fnx_assert(sz == 0);

	for (i = 0; i < NTHREADS; ++i) {
		pth = &tid_prod[i];
		pthread_create(pth, NULL, producer, fifo);
		pth = &tid_cons[i];
		pthread_create(pth, NULL, consumer, fifo);
	}

	for (i = 0; i < NTHREADS; ++i) {
		pth = &tid_prod[i];
		pthread_join(*pth, NULL);
		pth = &tid_cons[i];
		pthread_join(*pth, NULL);
	}

	fnx_assert(s_in  == NTHREADS * NITEMS);
	fnx_assert(s_out == NTHREADS * NITEMS);
	fnx_fifo_destroy(fifo);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_fifo_basic(void)
{
	size_t i, sz;
	item_t    *itm  = NULL;
	fnx_link_t *lnk  = NULL;
	fnx_link_t *tmp  = NULL;
	fnx_fifo_t *fifo = &s_fifo;

	fnx_fifo_init(fifo);

	sz = fnx_fifo_size(fifo);
	fnx_assert(sz == 0);

	for (i = 0; i < NITEMS; ++i) {
		itm = item_new((int)i);
		fnx_fifo_push(fifo, item_to_lnk(itm));
	}

	for (i = 0; i < NITEMS; ++i) {
		lnk = fnx_fifo_pop(fifo, 100000);
		fnx_assert(lnk != NULL);
		itm = item_from_lnk(lnk);
		fnx_assert(itm->index == (int)i);
		item_del(itm);
	}
	sz = fnx_fifo_size(fifo);
	fnx_assert(sz == 0);

	for (i = 0; i < NITEMS; ++i) {
		itm = item_new((int)i);
		fnx_fifo_push(fifo, item_to_lnk(itm));
	}
	lnk = fnx_fifo_popn(fifo, NITEMS, 100000);
	fnx_assert(lnk != NULL);
	sz = fnx_fifo_size(fifo);
	fnx_assert(sz == 0);

	while (lnk != NULL) {
		tmp = lnk;
		lnk = lnk->next;

		itm = item_from_lnk(tmp);
		item_del(itm);
	}

	fnx_fifo_destroy(fifo);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_fifo_queue(void)
{
	size_t i, sz, nn = 0;
	item_t    *itm  = NULL;
	fnx_link_t *lnk  = NULL;
	fnx_link_t *tmp  = NULL;
	fnx_fifo_t *fifo = &s_fifo;

	fnx_fifo_init(fifo);

	sz = fnx_fifo_size(fifo);
	fnx_assert(sz == 0);

	for (i = 0; i < NITEMS; ++i) {
		itm = item_new((int)i);
		fnx_fifo_push(fifo, item_to_lnk(itm));
	}

	lnk = fnx_fifo_popq(fifo, 100000);
	fnx_assert(lnk != NULL);
	sz = fnx_fifo_size(fifo);
	fnx_assert(sz == 0);

	while (lnk != NULL) {
		tmp = lnk;
		lnk = lnk->next;

		itm = item_from_lnk(tmp);
		item_del(itm);
		nn++;
	}
	fnx_assert(nn == NITEMS);

	fnx_fifo_destroy(fifo);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int main(void)
{
	test_fifo_basic();
	test_fifo_queue();
	test_fifo_threads();

	return EXIT_SUCCESS;
}


