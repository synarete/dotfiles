/*
 * CaC: Cauta-to-C Compiler
 *
 * Copyright (C) 2016,2017,2018 Shachar Sharon
 *
 * CaC is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * CaC is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "cacdefs.h"
#include "panic.h"
#include "list.h"


/*
 * Link:
 */
void llink_init(llink_t *lnk)
{
	lnk->next = lnk->prev = lnk;
}

void llink_destroy(llink_t *lnk)
{
	lnk->next = lnk->prev = NULL;
}

void llink_set(llink_t *lnk, llink_t *prev, llink_t *next)
{
	lnk->next = next;
	lnk->prev = prev;
}

void llink_insert(llink_t *lnk, llink_t *prev, llink_t *next)
{
	llink_set(lnk, prev, next);

	next->prev = lnk;
	prev->next = lnk;
}

void llink_remove(llink_t *lnk)
{
	llink_t *next, *prev;

	next = lnk->next;
	prev = lnk->prev;

	next->prev = prev;
	prev->next = next;

	llink_set(lnk, lnk, lnk);
}

int llink_isunlinked(const llink_t *lnk)
{
	return (lnk->next == lnk) && (lnk->prev == lnk);
}

int llink_islinked(const llink_t *lnk)
{
	return ((lnk->next != NULL) && (lnk->prev != NULL) &&
	        !llink_isunlinked(lnk));
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * List:
 */
static void list_inc_size(list_t *ls)
{
	ls->size += 1;
}

static void list_dec_size(list_t *ls)
{
	ls->size -= 1;
}

static void list_tie(list_t *ls, llink_t *first, llink_t *last)
{
	ls->head.next = first;
	ls->head.prev = last;
	first->prev = &ls->head;
	last->next  = &ls->head;
}


void list_init(list_t *ls)
{
	list_tie(ls, &ls->head, &ls->head);
	ls->size = 0;
}

void list_destroy(list_t *ls)
{
	llink_set(&ls->head, NULL, NULL);
	ls->size = 0;
}

size_t list_size(const list_t *ls)
{
	return ls->size;
}

int list_isempty(const list_t *ls)
{
	return (list_size(ls) == 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

llink_t *list_clear(list_t *ls)
{
	llink_t *lnk = NULL;

	if (list_size(ls) > 0) {
		/* Take elements */
		lnk = ls->head.next;

		/* Detach links from list */
		ls->head.next->prev = NULL;
		ls->head.prev->next = NULL;

		/* Re-link cyclic */
		llink_init(&ls->head);
		ls->size = 0;
	}
	return lnk;
}

void list_push_front(list_t *ls, llink_t *lnk)
{
	list_insert_after(ls, &ls->head, lnk);
}

void list_push_back(list_t *ls, llink_t *lnk)
{
	list_insert_before(ls, lnk, &ls->head);
}

void list_insert_before(list_t *ls,
                        llink_t *lnk, llink_t *next_lnk)
{
	llink_insert(lnk, next_lnk->prev, next_lnk);
	list_inc_size(ls);
}

void list_insert_after(list_t *ls,
                       llink_t *prev_lnk, llink_t *lnk)
{
	llink_insert(lnk, prev_lnk, prev_lnk->next);
	list_inc_size(ls);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void list_remove(list_t *ls, llink_t *lnk)
{
	if (list_size(ls) > 0) {
		llink_remove(lnk);
		list_dec_size(ls);
	}
}

llink_t *list_pop_front(list_t *ls)
{
	llink_t *lnk = NULL;

	if (list_size(ls) > 0) {
		lnk = ls->head.next;
		list_remove(ls, lnk);
	}
	return lnk;
}

llink_t *list_pop_back(list_t *ls)
{
	llink_t *lnk = NULL;

	if (list_size(ls) > 0) {
		lnk = ls->head.prev;
		list_remove(ls, lnk);
	}
	return lnk;
}

llink_t *list_begin(const list_t *ls)
{
	const llink_t *lnk;

	lnk = &ls->head;
	return (llink_t *)lnk->next;
}

llink_t *list_end(const list_t *ls)
{
	const llink_t *lnk;

	lnk = &ls->head;
	return (llink_t *)lnk;
}

llink_t *list_next(const list_t *ls, const llink_t *lnk)
{
	(void)ls;
	return lnk->next;
}

llink_t *list_prev(const list_t *ls, const llink_t *lnk)
{
	(void)ls;
	return lnk->prev;
}

llink_t *list_front(const list_t *ls)
{
	const llink_t *lnk = NULL;

	if (list_size(ls) > 0) {
		lnk = ls->head.next;
	}
	return (llink_t *)lnk;
}

llink_t *list_back(const list_t *ls)
{
	const llink_t *lnk = NULL;

	if (list_size(ls) > 0) {
		lnk = ls->head.prev;
	}
	return (llink_t *)lnk;
}

llink_t *list_find(const list_t *ls, llink_unary_pred fn)
{
	const llink_t *res;
	const llink_t *itr;
	const llink_t *end;

	res = NULL;
	itr = list_begin(ls);
	end = list_end(ls);
	while (itr != end) {
		if (fn(itr)) {
			res = itr;
			break;
		}
		itr = itr->next;
	}
	return (llink_t *)res;
}

llink_t *list_vfind(const list_t *ls,
                    llink_vbinary_pred fn, const void *v)
{
	const llink_t *res;
	const llink_t *itr;
	const llink_t *end;

	res = NULL;
	itr = list_begin(ls);
	end = list_end(ls);
	while (itr != end) {
		if (fn(itr, v)) {
			res = itr;
			break;
		}
		itr = itr->next;
	}
	return (llink_t *)res;
}

void list_foreach(const list_t *ls, llink_unary_fn fn)
{
	llink_t *itr, *end;

	itr = list_begin(ls);
	end = list_end(ls);
	while (itr != end) {
		if (fn(itr)) {
			break;
		}
		itr = itr->next;
	}
}

void list_vforeach(const list_t *ls,
                   llink_vbinary_fn fn, const void *v)
{
	llink_t *itr, *end;

	itr = list_begin(ls);
	end = list_end(ls);
	while (itr != end) {
		if (fn(itr, v)) {
			break;
		}
		itr = itr->next;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void list_append(list_t *ls1, list_t *ls2)
{
	size_t sz;
	llink_t *head, *lnk;

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
		list_init(ls2);
	}
}

static void list_swap_sizes(list_t *ls1, list_t *ls2)
{
	const size_t sz1 = ls1->size;

	ls1->size = ls2->size;
	ls2->size = sz1;
}

static void list_copy_head(const list_t *ls, llink_t *head)
{
	head->next = ls->head.next;
	head->prev = ls->head.prev;
}

void list_swap(list_t *ls1, list_t *ls2)
{
	llink_t  head1, head2;

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

void slist_init(slink_t *head)
{
	head->next = NULL;
}

void slist_destroy(slink_t *head)
{
	slink_t *itr;
	slink_t *cur;

	itr = head;
	while (itr != NULL) {
		cur = itr;
		itr = itr->next;
		cur->next = NULL;
	}
}

size_t slist_length(const slink_t *head)
{
	size_t cnt;
	const slink_t *itr;

	cnt = 0;
	itr = head->next;
	while (itr != NULL) {
		cnt++;
		itr = itr->next;
	}
	return cnt;
}

int slist_isempty(const slink_t *head)
{
	return (head->next == NULL);
}

void slist_push_front(slink_t *head, slink_t *lnk)
{
	slist_insert(head, head->next, lnk);
}

slink_t *slist_pop_front(slink_t *head)
{
	return slist_remove(head, head->next);
}

slink_t *slist_insert(slink_t *head, slink_t *pos,
                      slink_t *lnk)
{
	slink_t *prv;
	slink_t *itr;

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

slink_t *slist_remove(slink_t *head, slink_t *lnk)
{
	slink_t *prv;
	slink_t *itr;

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

slink_t *slist_find(const slink_t *head,
                    slink_test_fn fn, const void *v)
{
	slink_t *itr;

	itr = head->next;
	while (itr != NULL) {
		if (fn(itr, v)) {
			break;
		}
		itr = itr->next;
	}
	return (slink_t *)itr;
}

slink_t *slist_findremove(slink_t *head,
                          slink_test_fn fn, const void *v)
{
	slink_t *prv;
	slink_t *itr;

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
	return (slink_t *)itr;
}

void slist_foreach(const slink_t *head,
                   slink_foreach_fn fn, const void *v)
{
	slink_t *itr;

	itr = head->next;
	while (itr != NULL) {
		fn(itr, v);
		itr = itr->next;
	}
}




