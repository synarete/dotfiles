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

#include "infra-compiler.h"
#include "infra-macros.h"
#include "infra-panic.h"
#include "infra-list.h"


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Link:
 */
void fnx_link_init(fnx_link_t *lnk)
{
	lnk->next = lnk->prev = lnk;
}

void fnx_link_destroy(fnx_link_t *lnk)
{
	lnk->next = lnk->prev = NULL;
}

void fnx_link_set(fnx_link_t *lnk, fnx_link_t *prev, fnx_link_t *next)
{
	lnk->next = next;
	lnk->prev = prev;
}

void fnx_link_insert(fnx_link_t *lnk, fnx_link_t *prev, fnx_link_t *next)
{
	fnx_link_set(lnk, prev, next);

	next->prev = lnk;
	prev->next = lnk;
}

void fnx_link_remove(fnx_link_t *lnk)
{
	fnx_link_t *next, *prev;

	next = lnk->next;
	prev = lnk->prev;

	next->prev = prev;
	prev->next = next;

	fnx_link_set(lnk, lnk, lnk);
}

int fnx_link_isunlinked(const fnx_link_t *lnk)
{
	return (lnk->next == lnk) && (lnk->prev == lnk);
}

int fnx_link_islinked(const fnx_link_t *lnk)
{
	return ((lnk->next != NULL) && (lnk->prev != NULL) &&
	        !fnx_link_isunlinked(lnk));
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * List:
 */
static void list_inc_size(fnx_list_t *ls)
{
	ls->size += 1;
}

static void list_dec_size(fnx_list_t *ls)
{
	ls->size -= 1;
}

static void list_tie(fnx_list_t *ls, fnx_link_t *first, fnx_link_t *last)
{
	ls->head.next = first;
	ls->head.prev = last;
	first->prev = &ls->head;
	last->next  = &ls->head;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_list_init(fnx_list_t *ls)
{
	list_tie(ls, &ls->head, &ls->head);
	ls->size = 0;
}

void fnx_list_destroy(fnx_list_t *ls)
{
	fnx_link_set(&ls->head, NULL, NULL);
	ls->size = 0;
}

static size_t list_size(const fnx_list_t *ls)
{
	return ls->size;
}

size_t fnx_list_size(const fnx_list_t *ls)
{
	return list_size(ls);
}

int fnx_list_isempty(const fnx_list_t *ls)
{
	return (list_size(ls) == 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_link_t *fnx_list_clear(fnx_list_t *ls)
{
	fnx_link_t *lnk = NULL;

	if (list_size(ls) > 0) {
		/* Take elements */
		lnk = ls->head.next;

		/* Detach links from list */
		ls->head.next->prev = NULL;
		ls->head.prev->next = NULL;

		/* Re-link cyclic */
		fnx_link_init(&ls->head);
		ls->size = 0;
	}
	return lnk;
}

void fnx_list_push_front(fnx_list_t *ls, fnx_link_t *lnk)
{
	fnx_list_insert_after(ls, &ls->head, lnk);
}

void fnx_list_push_back(fnx_list_t *ls, fnx_link_t *lnk)
{
	fnx_list_insert_before(ls, lnk, &ls->head);
}

void fnx_list_insert_before(fnx_list_t *ls,
                            fnx_link_t *lnk, fnx_link_t *next_lnk)
{
	fnx_link_insert(lnk, next_lnk->prev, next_lnk);
	list_inc_size(ls);
}

void fnx_list_insert_after(fnx_list_t *ls,
                           fnx_link_t *prev_lnk, fnx_link_t *lnk)
{
	fnx_link_insert(lnk, prev_lnk, prev_lnk->next);
	list_inc_size(ls);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_list_remove(fnx_list_t *ls, fnx_link_t *lnk)
{
	if (list_size(ls) > 0) {
		fnx_link_remove(lnk);
		list_dec_size(ls);
	}
}

fnx_link_t *fnx_list_pop_front(fnx_list_t *ls)
{
	fnx_link_t *lnk = NULL;

	if (list_size(ls) > 0) {
		lnk = ls->head.next;
		fnx_list_remove(ls, lnk);
	}
	return lnk;
}

fnx_link_t *fnx_list_pop_back(fnx_list_t *ls)
{
	fnx_link_t *lnk = NULL;

	if (list_size(ls) > 0) {
		lnk = ls->head.prev;
		fnx_list_remove(ls, lnk);
	}
	return lnk;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_link_t *fnx_list_begin(const fnx_list_t *ls)
{
	const fnx_link_t *lnk;

	lnk = &ls->head;
	return (fnx_link_t *)lnk->next;
}

fnx_link_t *fnx_list_end(const fnx_list_t *ls)
{
	const fnx_link_t *lnk;

	lnk = &ls->head;
	return (fnx_link_t *)lnk;
}

fnx_link_t *fnx_list_next(const fnx_list_t *ls, const fnx_link_t *lnk)
{
	(void)ls;
	return lnk->next;
}

fnx_link_t *fnx_list_prev(const fnx_list_t *ls, const fnx_link_t *lnk)
{
	(void)ls;
	return lnk->prev;
}

fnx_link_t *fnx_list_front(const fnx_list_t *ls)
{
	const fnx_link_t *lnk = NULL;

	if (list_size(ls) > 0) {
		lnk = ls->head.next;
	}
	return (fnx_link_t *)lnk;
}

fnx_link_t *fnx_list_back(const fnx_list_t *ls)
{
	const fnx_link_t *lnk = NULL;

	if (list_size(ls) > 0) {
		lnk = ls->head.prev;
	}
	return (fnx_link_t *)lnk;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_link_t *fnx_list_find(const fnx_list_t *ls, fnx_link_unary_pred fn)
{
	const fnx_link_t *res;
	const fnx_link_t *itr;
	const fnx_link_t *end;

	res = NULL;
	itr = fnx_list_begin(ls);
	end = fnx_list_end(ls);
	while (itr != end) {
		if (fn(itr)) {
			res = itr;
			break;
		}
		itr = itr->next;
	}
	return (fnx_link_t *)res;
}

fnx_link_t *fnx_list_vfind(const fnx_list_t *ls,
                           fnx_link_vbinary_pred fn, const void *v)
{
	const fnx_link_t *res;
	const fnx_link_t *itr;
	const fnx_link_t *end;

	res = NULL;
	itr = fnx_list_begin(ls);
	end = fnx_list_end(ls);
	while (itr != end) {
		if (fn(itr, v)) {
			res = itr;
			break;
		}
		itr = itr->next;
	}
	return (fnx_link_t *)res;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_list_foreach(const fnx_list_t *ls, fnx_link_unary_fn fn)
{
	fnx_link_t *itr, *end;

	itr = fnx_list_begin(ls);
	end = fnx_list_end(ls);
	while (itr != end) {
		if (fn(itr)) {
			break;
		}
		itr = itr->next;
	}
}

void fnx_list_vforeach(const fnx_list_t *ls,
                       fnx_link_vbinary_fn fn, const void *v)
{
	fnx_link_t *itr, *end;

	itr = fnx_list_begin(ls);
	end = fnx_list_end(ls);
	while (itr != end) {
		if (fn(itr, v)) {
			break;
		}
		itr = itr->next;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_list_append(fnx_list_t *ls1, fnx_list_t *ls2)
{
	size_t sz;
	fnx_link_t *head, *lnk;

	sz = list_size(ls2);
	if (sz > 0) {
		/* Link content of list2 into list1 */
		head = &ls1->head;

		lnk = ls2->head.next;
		lnk->prev = head->prev;
		lnk->prev->next = lnk;

		lnk = ls2->head.prev;
		lnk->next  = head;
		lnk->next->prev = lnk;

		ls1->size += sz;

		/* Reset list2 to zero-size */
		fnx_list_init(ls2);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void list_swap_sizes(fnx_list_t *ls1, fnx_list_t *ls2)
{
	const size_t sz1 = ls1->size;

	ls1->size = ls2->size;
	ls2->size = sz1;
}

static void list_copy_head(const fnx_list_t *ls, fnx_link_t *head)
{
	head->next = ls->head.next;
	head->prev = ls->head.prev;
}

void fnx_list_swap(fnx_list_t *ls1, fnx_list_t *ls2)
{
	fnx_link_t  head1, head2;

	list_copy_head(ls1, &head1);
	list_copy_head(ls2, &head2);

	if (list_size(ls2) > 0) {
		list_tie(ls1, head2.next, head2.prev);
	} else {
		list_tie(ls1, &ls1->head, &ls1->head);
	}

	if (list_size(ls1) > 0) {
		list_tie(ls2, head1.next, head1.prev);
	} else {
		list_tie(ls2, &ls2->head, &ls2->head);
	}

	list_swap_sizes(ls1, ls2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_slist_init(fnx_slink_t *head)
{
	head->next = NULL;
}

void fnx_slist_destroy(fnx_slink_t *head)
{
	fnx_slink_t *itr;
	fnx_slink_t *cur;

	itr = head;
	while (itr != NULL) {
		cur = itr;
		itr = itr->next;
		cur->next = NULL;
	}
}

size_t fnx_slist_length(const fnx_slink_t *head)
{
	size_t cnt;
	const fnx_slink_t *itr;

	cnt = 0;
	itr = head->next;
	while (itr != NULL) {
		cnt++;
		itr = itr->next;
	}
	return cnt;
}

int fnx_slist_isempty(const fnx_slink_t *head)
{
	return (head->next == NULL);
}

void fnx_slist_push_front(fnx_slink_t *head, fnx_slink_t *lnk)
{
	fnx_slist_insert(head, head->next, lnk);
}

fnx_slink_t *fnx_slist_pop_front(fnx_slink_t *head)
{
	return fnx_slist_remove(head, head->next);
}

fnx_slink_t *fnx_slist_insert(fnx_slink_t *head, fnx_slink_t *pos,
                              fnx_slink_t *lnk)
{
	fnx_slink_t *prv;
	fnx_slink_t *itr;

	prv = head;
	itr = head->next;
	while ((itr != NULL) && (itr != pos)) {
		prv = itr;
		itr = itr->next;
	}

	if (itr == pos) {
		prv->next  = lnk;
		lnk->next = pos;
	}
	return itr;
}

fnx_slink_t *fnx_slist_remove(fnx_slink_t *head, fnx_slink_t *lnk)
{
	fnx_slink_t *prv;
	fnx_slink_t *itr;

	prv = head;
	itr = head->next;
	while ((itr != NULL) && (itr != lnk)) {
		prv = itr;
		itr = itr->next;
	}

	if (itr != NULL) {
		prv->next = itr->next;
		itr->next = NULL;
	}
	return itr;
}

fnx_slink_t *fnx_slist_find(const fnx_slink_t *head,
                            fnx_slink_test_fn fn, const void *v)
{
	fnx_slink_t *itr;

	itr = head->next;
	while (itr != NULL) {
		if (fn(itr, v)) {
			break;
		}
		itr = itr->next;
	}
	return (fnx_slink_t *)itr;
}

fnx_slink_t *fnx_slist_findremove(fnx_slink_t *head,
                                  fnx_slink_test_fn fn, const void *v)
{
	fnx_slink_t *prv;
	fnx_slink_t *itr;

	prv = head;
	itr = head->next;
	while (itr != NULL) {
		if (fn(itr, v)) {
			break;
		}
		prv = itr;
		itr = itr->next;
	}

	if (itr != NULL) {
		prv->next = itr->next;
		itr->next = NULL;
	}
	return (fnx_slink_t *)itr;
}

void fnx_slist_foreach(const fnx_slink_t *head,
                       fnx_slink_foreach_fn fn, const void *v)
{
	fnx_slink_t *itr;

	itr = head->next;
	while (itr != NULL) {
		fn(itr, v);
		itr = itr->next;
	}
}




