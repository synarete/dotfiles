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
#include <fnx/infra.h>


#define MAGIC           (2011UL)
#define MAX_SIZE        (1024)

struct msg_ {
	size_t key;
	size_t magic;
	char value[51];
};
typedef struct msg_ msg_t;


static void msg_init(msg_t *msg, size_t key)
{
	fnx_bzero(msg, sizeof(*msg));

	msg->key    = key;
	msg->magic  = MAGIC + key;
	snprintf(msg->value, sizeof(msg->value), "%ld", (long)key);
}

static void msg_check(const msg_t *msg, size_t key)
{
	int cmp;
	char buf[100];

	snprintf(buf, sizeof(buf), "%ld", (long)key);

	fnx_assert(msg->key == key);
	fnx_assert(msg->magic == (MAGIC + key));

	cmp = strcmp(buf, msg->value);
	fnx_assert(cmp == 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_deque_ops1(fnx_deque_t *deque)
{
	int rc;
	size_t i, key, nelems, sz;
	msg_t msg;
	const msg_t *p_msg = NULL;

	nelems = fnx_deque_maxsize(deque);
	fnx_assert((nelems % 2) == 0);

	for (i = 0; i < nelems; ++i) {
		key = i;

		rc = fnx_deque_isfull(deque);
		fnx_assert(!rc);

		msg_init(&msg, key);
		fnx_deque_push_back(deque, &msg);
		p_msg = (const msg_t *)fnx_deque_back(deque);
		fnx_assert(p_msg != NULL);

		msg_check(p_msg, key);
	}

	for (i = 0; i < nelems; ++i) {
		key = i;
		p_msg = (const msg_t *)fnx_deque_at(deque, i);
		msg_check(p_msg, key);
	}

	sz = fnx_deque_size(deque);
	fnx_assert(sz == nelems);

	key = 0;
	while (sz >= 2) {
		p_msg = (const msg_t *)fnx_deque_front(deque);
		fnx_assert(p_msg != NULL);
		msg_check(p_msg, key);

		fnx_deque_pop_back(deque);
		fnx_deque_pop_front(deque);
		sz = fnx_deque_size(deque);

		++key;
	}

	fnx_assert(fnx_deque_isempty(deque));
}



static void test_deque_ops2(fnx_deque_t *deque)
{
	int rc;
	size_t i, key, nelems;
	msg_t msg;
	const msg_t *p_msg = NULL;

	nelems = fnx_deque_maxsize(deque);
	fnx_assert((nelems % 2) == 0);

	for (i = 0; i < nelems / 2; ++i) {
		key = i;

		rc = fnx_deque_isfull(deque);
		fnx_assert(!rc);

		msg_init(&msg, key);
		fnx_deque_push_back(deque, &msg);
		p_msg = (const msg_t *)fnx_deque_back(deque);
		msg_check(p_msg, key);

		msg_init(&msg, key);
		fnx_deque_push_back(deque, &msg);
		p_msg = (const msg_t *)fnx_deque_back(deque);
		msg_check(p_msg, key);

		fnx_deque_pop_front(deque);

		key = i + 1;
		msg_init(&msg, key);
		fnx_deque_push_front(deque, &msg);
		p_msg = (const msg_t *)fnx_deque_front(deque);
		msg_check(p_msg, key);
	}

	while (!fnx_deque_isempty(deque)) {
		fnx_deque_pop_back(deque);
	}
}

static void test_deque_ops(fnx_deque_t *deque)
{
	fnx_assert(fnx_deque_isempty(deque));
	test_deque_ops1(deque);

	fnx_assert(fnx_deque_isempty(deque));
	test_deque_ops1(deque);

	fnx_assert(fnx_deque_isempty(deque));
	test_deque_ops1(deque);

	fnx_assert(fnx_deque_isempty(deque));
	test_deque_ops2(deque);
}


static void test_deque(void)
{
	size_t nelems, max_sz;
	fnx_deque_t *deque;

	nelems = MAX_SIZE;
	deque = fnx_deque_new(sizeof(msg_t), nelems);

	max_sz = fnx_deque_maxsize(deque);
	fnx_assert(max_sz == nelems);

	test_deque_ops(deque);

	fnx_deque_delete(deque);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int main(void)
{
	test_deque();

	return EXIT_SUCCESS;
}



