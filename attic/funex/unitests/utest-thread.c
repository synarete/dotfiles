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
#include <stdint.h>

#define MAGIC (0xDEADBEEF)

struct fnx_tentry {
	fnx_thread_t *th;
	uint64_t magic;
};
typedef struct fnx_tentry   fnx_tentry_t;


static uint64_t magicof(const fnx_tentry_t *te)
{
	return ((uint64_t)MAGIC ^ (uint64_t)te);
}

static void tentry_exec(void *arg)
{
	const fnx_tentry_t *te = (fnx_tentry_t *)arg;
	fnx_assert(te->magic == magicof(te));
}

int main(void)
{
	size_t i, sz, nelems;
	fnx_thread_t *th = NULL;
	fnx_tentry_t *te = NULL;
	fnx_tentry_t *te_arr;

	sz     = sizeof(*te);
	nelems = 128;
	te_arr = (fnx_tentry_t *)(malloc(nelems * sz));

	for (i = 0; i < nelems; ++i) {
		te = &te_arr[i];
		th = (fnx_thread_t *)malloc(sizeof(*th));
		fnx_assert(th != NULL);
		te->th = th;
		te->magic = magicof(te);
		fnx_thread_start(th, tentry_exec, te);
	}
	for (i = 0; i < nelems; ++i) {
		te = &te_arr[i];
		fnx_thread_join(te->th, FNX_NOFAIL);
		free(te->th);
	}
	free(te_arr);

	return 0;
}


