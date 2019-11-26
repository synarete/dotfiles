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
#include <string.h>
#include <time.h>

#include <fnxinfra.h>
#include <fnxhash.h>

#define MAGIC           (0xCC4466AA)
#define MAX_SIZE        (511)

struct element {
	int num;
	unsigned magic;
	fnx_slink_t lnk;
};
typedef struct element element_t;

static void elem_init(element_t *elem)
{
	elem->num      = 0;
	elem->magic    = MAGIC;
	elem->lnk.next = NULL;
}

static void elem_destroy(element_t *elem)
{
	elem->num      = -1;
	elem->magic    = 0;
	elem->lnk.next = NULL;
}

static void elem_check(const element_t *elem)
{
	fnx_assert(elem->magic == MAGIC);
}

static element_t *elem_new(size_t n)
{
	element_t *elem = NULL;

	elem = (element_t *)fnx_malloc(sizeof(*elem));
	fnx_assert(elem != 0);
	elem_init(elem);
	elem->num = (int)n;
	return elem;
}

static void elem_delete(element_t *elem)
{
	elem_destroy(elem);
	fnx_free(elem, sizeof(*elem));
}

static element_t *slink_to_elem(fnx_slink_t *lnk)
{
	element_t *elem = fnx_container_of(lnk, element_t, lnk);
	elem_check(elem);
	return elem;
}

static fnx_slink_t *elem_to_slink(element_t *elem)
{
	elem_check(elem);
	return &elem->lnk;
}

static void check_nomembers(const fnx_slink_t *head)
{
	int isempty;

	isempty = fnx_slist_isempty(head);
	fnx_assert(isempty);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int vequal(const fnx_slink_t *lnk, const void *v)
{
	int rc, num;
	const element_t *elem;

	elem = slink_to_elem((fnx_slink_t *)lnk);
	num  = *((const int *)v);

	rc = (elem->num == num) ? 1 : 0;
	return rc;
}

static void test_vfind(const fnx_slink_t *head, int num)
{
	const fnx_slink_t *lnk;
	const element_t *elem;

	lnk = fnx_slist_find(head, vequal, &num);
	fnx_assert(lnk != NULL);

	elem = slink_to_elem((fnx_slink_t *)lnk);
	fnx_assert(elem->num == num);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void utest_pushpop(fnx_slink_t *head)
{
	size_t i, n, len;
	element_t *elem;
	fnx_slink_t *slnk;

	n = MAX_SIZE;
	for (i = 0; i < n; ++i) {
		len = fnx_slist_length(head);
		fnx_assert(len == i);

		elem = elem_new(len);
		slnk = elem_to_slink(elem);

		fnx_slist_push_front(head, slnk);

		test_vfind(head, (int)len);
	}

	for (i = 0; i < n; ++i) {
		len = fnx_slist_length(head);
		test_vfind(head, (int)(len - 1));

		slnk = fnx_slist_pop_front(head);
		elem = slink_to_elem(slnk);
		elem_check(elem);

		len = fnx_slist_length(head);
		fnx_assert(elem->num == (int)len);

		elem_delete(elem);
	}

	check_nomembers(head);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void utest_findremove(fnx_slink_t *head)
{
	int num;
	size_t i, n, len;
	element_t *elem;
	fnx_slink_t *slnk;

	n = MAX_SIZE;
	for (i = 0; i < n; ++i) {
		len = fnx_slist_length(head);
		fnx_assert(len == i);

		elem = elem_new(len);
		slnk = elem_to_slink(elem);
		fnx_slist_push_front(head, slnk);
	}

	for (i = n; i > 0; --i) {
		num = (int)(i - 1);
		slnk = fnx_slist_findremove(head, vequal, &num);
		fnx_assert(slnk != NULL);

		elem = slink_to_elem(slnk);
		elem_delete(elem);
	}

	check_nomembers(head);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void utest_random(fnx_slink_t *head)
{
	int rnd, num, *nums;
	size_t i, j, n, k, sz;
	element_t *elem;
	fnx_slink_t *slnk;

	n  = MAX_SIZE;
	sz = sizeof(int) * n;
	nums = (int *)fnx_xmalloc(sz, FNX_NOFAIL);
	blgn_prandom_tseq(nums, n, 0);

	for (i = 0; i < n; ++i) {
		num = nums[i];
		elem = elem_new((size_t)num);
		slnk = elem_to_slink(elem);
		fnx_slist_push_front(head, slnk);
	}

	rnd = nums[0];
	k = ((size_t)rnd) % n;
	for (i = 0; i < k; ++i) {
		j = n - i - 1;
		num = nums[i];
		nums[i] = nums[j];
		nums[j] = num;
	}

	for (i = 0; i < n; ++i) {
		num = nums[i];
		slnk = fnx_slist_findremove(head, vequal, &num);
		fnx_assert(slnk != NULL);

		elem = slink_to_elem(slnk);
		elem_delete(elem);
	}

	check_nomembers(head);

	fnx_free(nums, sz);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void accum(fnx_slink_t *lnk, const void *p)
{
	size_t *p_sum = (size_t *)p;
	const element_t *elem;

	elem = slink_to_elem(lnk);
	*p_sum += (size_t)(elem->num);
}

static void utest_foreach(fnx_slink_t *head)
{
	size_t i, n, total;
	element_t *elem;
	fnx_slink_t *slnk;

	n = MAX_SIZE;
	for (i = 0; i < n; ++i) {
		elem = elem_new(1);
		slnk = elem_to_slink(elem);

		fnx_slist_push_front(head, slnk);

		total = 0;
		fnx_slist_foreach(head, accum, &total);
		fnx_assert(total == (i + 1));
	}

	for (i = 0; i < n; ++i) {
		slnk = fnx_slist_pop_front(head);
		fnx_assert(slnk != NULL);
		elem = slink_to_elem(slnk);
		elem_delete(elem);
	}

	total = 0;
	fnx_slist_foreach(head, accum, &total);
	fnx_assert(total == 0);

	check_nomembers(head);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void utest_slist(void)
{
	fnx_slink_t sl_obj;
	fnx_slink_t *head = &sl_obj;

	fnx_slist_init(head);

	utest_pushpop(head);
	utest_findremove(head);
	utest_random(head);
	utest_foreach(head);

	fnx_slist_destroy(head);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int main(void)
{
	utest_slist();

	return EXIT_SUCCESS;
}


