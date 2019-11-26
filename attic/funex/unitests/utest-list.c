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
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <fnxinfra.h>


#define MAGIC           (0xFEA12345)
#define MAX_SIZE        (2048)


struct msg_ {
	fnx_link_t lnk; /* <-- MUST be first */
	unsigned int magic;
	int key;
};
typedef struct msg_     msg_t;

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static msg_t *new_msg(void)
{
	msg_t *msg;
	msg = (msg_t *)fnx_xmalloc(sizeof(*msg), FNX_BZERO | FNX_NOFAIL);
	return msg;
}

static void delete_msg(msg_t *msg)
{
	fnx_xfree(msg, sizeof(*msg), FNX_BZERO);
}

static fnx_link_t *msg_to_lnk(const msg_t *msg)
{
	const fnx_link_t *lnk = &msg->lnk;
	return (fnx_link_t *)lnk;
}

static msg_t *msg_from_lnk(const fnx_link_t *lnk)
{
	const msg_t *msg;

	msg = fnx_container_of(lnk, msg_t, lnk);
	return (msg_t *)msg;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void check_magic1(const fnx_link_t *lnk)
{
	const msg_t *msg;

	msg = msg_from_lnk(lnk);
	fnx_assert(msg->magic == MAGIC);
}

static int check_magic(fnx_link_t *lnk)
{
	check_magic1(lnk);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_range(const fnx_list_t *lst)
{
	size_t n, sz;
	const fnx_link_t *itr;

	sz = fnx_list_size(lst);
	n  = 0;

	itr = fnx_list_begin(lst);
	while (itr != fnx_list_end(lst)) {
		++n;
		itr = fnx_list_next(lst, itr);
	}
	fnx_assert(n == sz);

	fnx_list_foreach(lst, check_magic);
}

static void test_rrange(const fnx_list_t *lst)
{
	size_t n, sz;
	const fnx_link_t *itr;

	sz = fnx_list_size(lst);
	n  = 0;

	itr = fnx_list_end(lst);
	while (itr != fnx_list_begin(lst)) {
		itr = fnx_list_prev(lst, itr);
		++n;
	}
	fnx_assert(n == sz);

	fnx_list_foreach(lst, check_magic);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_pushpop1(fnx_list_t *lst)
{
	const size_t nelems = MAX_SIZE;
	size_t i, sz;
	msg_t *msg = NULL;
	fnx_link_t *lnk = NULL;

	for (i = 0; i < nelems; ++i) {
		sz = fnx_list_size(lst);
		fnx_assert(sz == i);

		msg = new_msg();
		msg->key    = (int) i;
		msg->magic  = MAGIC;

		lnk = msg_to_lnk(msg);
		fnx_list_push_back(lst, lnk);

		test_range(lst);
		test_rrange(lst);
	}
	sz = fnx_list_size(lst);
	fnx_assert(sz == nelems);

	for (i = 0; i < nelems; ++i) {
		lnk = fnx_list_pop_front(lst);
		msg = msg_from_lnk(lnk);
		fnx_assert(msg != NULL);
		fnx_assert(msg->magic == MAGIC);
		fnx_assert(msg->key == (int) i);

		delete_msg(msg);

		test_range(lst);
		test_rrange(lst);
	}
	sz = fnx_list_size(lst);
	fnx_assert(sz == 0);
}

static void test_pushpop2(fnx_list_t *lst)
{
	const size_t nelems = MAX_SIZE;
	size_t i, sz;
	msg_t *msg = NULL;
	fnx_link_t *lnk = NULL;

	for (i = 0; i < nelems; ++i) {
		sz = fnx_list_size(lst);
		fnx_assert(sz == i);

		msg = new_msg();
		msg->key    = (int) i;
		msg->magic  = MAGIC;

		lnk = msg_to_lnk(msg);
		fnx_list_push_front(lst, lnk);
	}
	sz = fnx_list_size(lst);
	fnx_assert(sz == nelems);

	for (i = 0; i < nelems; ++i) {
		lnk = fnx_list_pop_back(lst);
		msg = msg_from_lnk(lnk);
		fnx_assert(msg != NULL);
		fnx_assert(msg->magic == MAGIC);
		fnx_assert(msg->key == (int) i);

		delete_msg(msg);
	}
	sz = fnx_list_size(lst);
	fnx_assert(sz == 0);
}

static void test_pushclear(fnx_list_t *lst)
{
	const size_t nelems = MAX_SIZE;
	size_t i, sz, nn = 0;
	msg_t *msg = NULL;
	fnx_link_t *lnk = NULL;
	fnx_link_t *lnk2 = NULL;

	for (i = 0; i < nelems; ++i) {
		sz = fnx_list_size(lst);
		fnx_assert(sz == i);

		msg = new_msg();
		msg->key    = (int) i;
		msg->magic  = MAGIC;

		lnk = msg_to_lnk(msg);
		fnx_list_push_front(lst, lnk);
	}
	sz = fnx_list_size(lst);
	fnx_assert(sz == nelems);

	lnk = fnx_list_clear(lst);
	fnx_assert(lnk != NULL);

	sz = fnx_list_size(lst);
	fnx_assert(sz == 0);

	while ((lnk2 = lnk) != NULL) {
		lnk = lnk->next;
		msg = msg_from_lnk(lnk2);
		fnx_assert(msg != NULL);
		fnx_assert(msg->magic == MAGIC);
		delete_msg(msg);
		nn += 1;
	}
	fnx_assert(nn == nelems);
}

static void test_list1(void)
{
	fnx_list_t lst_obj;
	fnx_list_t *lst = &lst_obj;
	size_t sz;

	fnx_list_init(lst);
	sz = fnx_list_size(lst);
	fnx_assert(sz == 0);

	test_pushpop1(lst);
	test_pushpop2(lst);
	test_pushclear(lst);

	fnx_list_clear(lst);
	fnx_list_destroy(lst);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void fill_list(fnx_list_t *lst, size_t n)
{
	size_t i;
	msg_t *msg = NULL;
	fnx_link_t *lnk = NULL;

	for (i = 0; i < n; ++i) {
		msg = new_msg();
		msg->key    = (int)i;
		msg->magic  = MAGIC;

		lnk = msg_to_lnk(msg);
		fnx_list_push_back(lst, lnk);
	}
}

static int equal_to(const fnx_link_t *lnk, const void *p_n)
{
	size_t n;
	const msg_t *msg;

	check_magic1(lnk);

	n   = *((const size_t *)p_n);
	msg = msg_from_lnk(lnk);

	return (msg->key == (int)n);
}

static void clear_list(fnx_list_t *lst)
{
	size_t sz;
	msg_t *msg = NULL;
	fnx_link_t *lnk = NULL;

	sz = fnx_list_size(lst);
	while (sz > 0) {
		lnk = fnx_list_pop_front(lst);
		fnx_assert(lnk != NULL);
		msg = msg_from_lnk(lnk);
		fnx_assert(msg->magic == MAGIC);
		delete_msg(msg);

		sz = fnx_list_size(lst);
	}
}

static void test_find(fnx_list_t *lst)
{
	size_t i, n;
	const msg_t *msg = NULL;
	const fnx_link_t *lnk = NULL;

	n = 127;
	fill_list(lst, n);

	for (i = 0; i < n; ++i) {
		lnk = fnx_list_vfind(lst, equal_to, &i);
		fnx_assert(lnk != NULL);
		check_magic1(lnk);

		msg = msg_from_lnk(lnk);
		fnx_assert(msg->key == (int)i);
	}

	clear_list(lst);
}

static void test_swap(fnx_list_t *lst1, fnx_list_t *lst2)
{
	size_t i, n1, n2, sz1, sz2;
	const size_t sizes1[] = { 0, 1, 1, 0, 919, 919 };
	const size_t sizes2[] = { 0, 0, 1, 991, 0, 991 };

	for (i = 0; i < FNX_ARRAYSIZE(sizes1); ++i) {
		n1 = sizes1[i];
		n2 = sizes2[i];

		fill_list(lst1, n1);
		fill_list(lst2, n2);

		sz1 = fnx_list_size(lst1);
		fnx_assert(sz1 == n1);
		sz2 = fnx_list_size(lst2);
		fnx_assert(sz2 == n2);

		fnx_list_swap(lst1, lst2);

		sz1 = fnx_list_size(lst1);
		fnx_assert(sz1 == n2);
		sz2 = fnx_list_size(lst2);
		fnx_assert(sz2 == n1);

		clear_list(lst1);
		clear_list(lst2);
	}
}

static void test_append(fnx_list_t *lst1, fnx_list_t *lst2)
{
	size_t i, n1, n2, sz1, sz2;
	const size_t sizes1[] = { 0, 1, 1, 0, 331, 317 };
	const size_t sizes2[] = { 0, 0, 1, 317, 0, 331 };

	for (i = 0; i < FNX_ARRAYSIZE(sizes1); ++i) {
		n1 = sizes1[i];
		n2 = sizes2[i];

		fill_list(lst1, n1);
		fill_list(lst2, n2);

		sz1 = fnx_list_size(lst1);
		fnx_assert(sz1 == n1);
		sz2 = fnx_list_size(lst2);
		fnx_assert(sz2 == n2);

		fnx_list_append(lst1, lst2);

		sz1 = fnx_list_size(lst1);
		fnx_assert(sz1 == (n1 + n2));
		sz2 = fnx_list_size(lst2);
		fnx_assert(sz2 == 0);

		clear_list(lst1);
		clear_list(lst2);
	}
}


static void test_list2(void)
{
	fnx_list_t lst_obj1, lst_obj2;
	fnx_list_t *lst1 = &lst_obj1;
	fnx_list_t *lst2 = &lst_obj2;

	fnx_list_init(lst1);
	fnx_list_init(lst2);

	test_find(lst1);
	test_swap(lst1, lst2);
	test_append(lst1, lst2);

	fnx_list_destroy(lst1);
	fnx_list_destroy(lst2);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int main(void)
{
	test_list1();
	test_list2();

	return EXIT_SUCCESS;
}


