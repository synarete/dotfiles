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

#define MAGIC1          (0xAA2191C)
#define MAGIC2          (~(MAGIC1))

/* Local Functions: */
static void test_buffer_raw(void);
static void test_buffer_rraw(void);
static void test_buffer_wrap(void);
static void test_buffer_rwrap(void);
static void test_buffer_stack(void);
static void test_buffer_rstack(void);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
int main(void)
{
	test_buffer_raw();
	test_buffer_rraw();
	test_buffer_wrap();
	test_buffer_rwrap();
	test_buffer_stack();
	test_buffer_rstack();

	return EXIT_SUCCESS;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_buffer_raw(void)
{
	enum { nelems = 137 };
	int i, j, n, rc, cond;
	int arr[nelems];
	int past_arr;
	fnx_buffer_t buffer;

	past_arr = MAGIC1;

	fnx_buffer_init(&buffer, arr, sizeof(arr));

	for (j = 0; j < 7; ++ j) {
		for (i = 0; i < nelems; ++i) {
			n  = i + 1;
			rc = fnx_buffer_push_back(&buffer, &n, sizeof(n));
			fnx_assert(rc == (int) sizeof(n));
		}

		cond = fnx_buffer_isempty(&buffer);
		fnx_assert(!cond);

		cond = fnx_buffer_isfull(&buffer);
		fnx_assert(cond);

		for (i = 0; i < nelems; ++i) {
			n = ~0;
			rc = fnx_buffer_pop_front(&buffer, &n, sizeof(n));
			fnx_assert(rc == (int) sizeof(n));
			fnx_assert(n == (i + 1));
		}

		cond = fnx_buffer_isempty(&buffer);
		fnx_assert(cond);
	}

	fnx_buffer_destroy(&buffer);

	fnx_assert(past_arr == MAGIC1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_buffer_rraw(void)
{
	enum  { nelems = 127 } ;
	int i, j, n, rc, cond;
	int arr[nelems];
	int past_arr;
	fnx_buffer_t buffer;

	past_arr = MAGIC2;

	fnx_buffer_init(&buffer, arr, sizeof(arr));

	for (j = 0; j < 17; ++ j) {
		for (i = 0; i < nelems; ++i) {
			n  = i + 1;
			rc = fnx_buffer_push_front(&buffer, &n, sizeof(n));
			fnx_assert(rc == (int) sizeof(n));
		}

		cond = fnx_buffer_isempty(&buffer);
		fnx_assert(!cond);

		cond = fnx_buffer_isfull(&buffer);
		fnx_assert(cond);

		for (i = 0; i < nelems; ++i) {
			n = ~0;
			rc = fnx_buffer_pop_back(&buffer, &n, sizeof(n));
			fnx_assert(rc == (int) sizeof(n));
			fnx_assert(n == (i + 1));
		}

		cond = fnx_buffer_isempty(&buffer);
		fnx_assert(cond);
	}

	fnx_buffer_destroy(&buffer);

	fnx_assert(past_arr == MAGIC2);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct msg_ {
	int  magic1;
	char zzz;
	int  magic2;
};
typedef struct msg_ msg_t;

static void test_buffer_wrap1(void *mem, size_t sz)
{
	int i, n, rc;
	msg_t msg;
	fnx_buffer_t buffer;

	fnx_buffer_init(&buffer, mem, sz);

	for (i = 0; i < 1000; ++i) {
		msg.magic1  = MAGIC1;
		msg.zzz     = (char) i;
		msg.magic2  = MAGIC2;

		n = sizeof(msg);

		rc = fnx_buffer_push_back(&buffer, &msg, (size_t) n);
		fnx_assert(rc == (int) sizeof(msg));

		memset(&msg, 0xFF, (size_t) n);

		rc = fnx_buffer_pop_front(&buffer, &msg, (size_t) n);
		fnx_assert(rc == (int) sizeof(msg));
		fnx_assert(msg.magic1 == MAGIC1);
		fnx_assert(msg.magic2 == MAGIC2);
	}

	fnx_buffer_destroy(&buffer);
}

static void test_buffer_wrap(void)
{
	size_t sz;
	void *mem;

	sz  = (sizeof(msg_t) * 8) / 7;
	mem = malloc(sz);
	fnx_assert(mem != NULL);
	test_buffer_wrap1(mem, sz);
	free(mem);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_buffer_rwrap1(void *mem, size_t sz)
{
	int i, n, rc;
	msg_t msg;
	fnx_buffer_t buffer;

	fnx_buffer_init(&buffer, mem, sz);

	for (i = 0; i < 1000; ++i) {
		msg.magic1  = MAGIC1;
		msg.zzz     = (char) i;
		msg.magic2  = MAGIC2;

		n = sizeof(msg);

		rc = fnx_buffer_push_front(&buffer, &msg, (size_t) n);
		fnx_assert(rc == (int) sizeof(msg));

		memset(&msg, 0xFF, (size_t) n);

		rc = fnx_buffer_pop_back(&buffer, &msg, (size_t) n);
		fnx_assert(rc == (int) sizeof(msg));
		fnx_assert(msg.magic1 == MAGIC1);
		fnx_assert(msg.magic2 == MAGIC2);
	}

	fnx_buffer_destroy(&buffer);
}

static void test_buffer_rwrap(void)
{
	size_t sz;
	void *mem;

	sz  = (sizeof(msg_t) * 19) / 17;
	mem = malloc(sz);
	test_buffer_rwrap1(mem, sz);
	free(mem);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_buffer_stack1(void *mem, size_t sz)
{
	int i, n, rc;
	msg_t msg;
	fnx_buffer_t buffer;

	fnx_buffer_init(&buffer, mem, sz);

	i = 0;
	while (!fnx_buffer_isfull(&buffer)) {
		msg.magic1  = MAGIC1;
		msg.zzz     = (char) i;
		msg.magic2  = MAGIC2;

		n = sizeof(msg);

		rc = fnx_buffer_push_front(&buffer, &msg, (size_t) n);
		fnx_assert(rc == n);
		++i;
	}

	while (!fnx_buffer_isempty(&buffer)) {
		--i;
		n = sizeof(msg);

		rc = fnx_buffer_pop_front(&buffer, &msg, (size_t) n);
		fnx_assert(rc == n);

		fnx_assert(msg.magic1 == MAGIC1);
		fnx_assert(msg.zzz    == i);
		fnx_assert(msg.magic2 == MAGIC2);
	}

	fnx_buffer_destroy(&buffer);
}

static void test_buffer_stack(void)
{
	size_t sz;
	void *mem;

	sz  = (sizeof(msg_t) * 100);
	mem = malloc(sz);
	fnx_assert(mem != NULL);
	test_buffer_stack1(mem, sz);
	free(mem);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_buffer_rstack1(void *mem, size_t sz)
{
	int i, n, rc;
	msg_t msg;
	fnx_buffer_t buffer;

	fnx_buffer_init(&buffer, mem, sz);

	i = 0;
	while (!fnx_buffer_isfull(&buffer)) {
		msg.magic1  = MAGIC1;
		msg.zzz     = (char) i;
		msg.magic2  = MAGIC2 + i;

		n = sizeof(msg);

		rc = fnx_buffer_push_back(&buffer, &msg, (size_t) n);
		fnx_assert(rc == n);
		++i;
	}

	while (!fnx_buffer_isempty(&buffer)) {
		--i;
		n = sizeof(msg);

		rc = fnx_buffer_pop_back(&buffer, &msg, (size_t) n);
		fnx_assert(rc == n);

		fnx_assert(msg.magic1 == MAGIC1);
		fnx_assert(msg.zzz    == i);
		fnx_assert(msg.magic2 == MAGIC2 + i);
	}

	fnx_buffer_destroy(&buffer);
}

static void test_buffer_rstack(void)
{
	size_t sz;
	void *mem;

	sz  = (sizeof(msg_t) * 100);
	mem = malloc(sz);
	fnx_assert(mem != NULL);
	test_buffer_rstack1(mem, sz);
	free(mem);
}


