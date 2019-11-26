/*
 * This file is part of CaC
 *
 * Copyright (C) 2016,2017 Shachar Sharon
 *
 * CaC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CaC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CaC.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef CAC_LIST_H_
#define CAC_LIST_H_



/*
 * List-link element within bidirectional linked-list. Typically, part (at the
 * beginning) of user's object.
 */
struct cac_llink {
	struct cac_llink *next;
	struct cac_llink *prev;
};
typedef struct cac_llink     llink_t;


/*
 * Bidirectional cyclic linked-list. The actual type of objects is defined
 * by the user.
 *
 * NB: It is up to the user to ensure that the linked elements remain valid
 * throughout the time they are members in the list.
 *
 */
struct cac_list {
	llink_t head;    /* "Pseudo" node; Always be cyclic   */
	size_t  size;    /* Number of elements (without head) */
};
typedef struct cac_list         list_t;


/*
 * Unidirectional linked-list: each element is linked only to the next element.
 * That is, a sequence-container that supports forward traversal only, and
 * (amortized) constant time insertion of elements. The actual type of objects
 * is defined by the user.
 *
 * NB: It is up to the user to ensure that the linked elements remain valid
 * throughout the time they are members os a list.
 */
struct cac_slink {
	struct cac_slink *next;
};
typedef struct cac_slink slink_t;


typedef int (*slink_test_fn)(const slink_t *, const void *);
typedef void (*slink_foreach_fn)(slink_t *, const void *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Compare/Apply functions:
 */
typedef int (*llink_unary_pred)(const llink_t *);

typedef int (*llink_binary_pred)(const llink_t *,
                                 const llink_t *);

typedef int (*llink_vbinary_pred)(const llink_t *, const void *);

typedef int (*llink_unary_fn)(llink_t *);

typedef int (*llink_binary_fn)(llink_t *, llink_t *);

typedef int (*llink_vbinary_fn)(llink_t *, const void *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Create cyclic (unlinked, selfish) link */
CAC_ATTR_NONNULL
void llink_init(llink_t *);

/* Have NULL links */
CAC_ATTR_NONNULL
void llink_destroy(llink_t *);


/* Set link within linked-list */
void llink_set(llink_t *, llink_t *, llink_t *);

/* Link into existing list, keep it in order */
void llink_insert(llink_t *, llink_t *, llink_t *);

/* Remove link from cyclic-linked list, keep it in order */
CAC_ATTR_NONNULL
void llink_remove(llink_t *);


/* Returns TRUE if link is not associated with any list (selfish links) */
CAC_ATTR_NONNULL
int llink_isunlinked(const llink_t *);

/* Returns TRUE if link is associated with a list (non-selfish no-null links) */
CAC_ATTR_NONNULL
int llink_islinked(const llink_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Constructor: initializes an empty, cyclic list.
 */
CAC_ATTR_NONNULL
void list_init(list_t *ls);

/*
 * Destroys list object. NB: It is up to the user to release any linked
 * resources associated with the list.
 */
CAC_ATTR_NONNULL
void list_destroy(list_t *ls);


/*
 * Returns the number of elements currently linked in list.
 */
CAC_ATTR_NONNULL
size_t list_size(const list_t *ls);


/*
 * Returns TRUE if no elements are associated with the list.
 */
CAC_ATTR_NONNULL
int list_isempty(const list_t *ls);



/*
 * Link user-provided node into the beginning of list (intrusive).
 */
CAC_ATTR_NONNULL
void list_push_front(list_t *ls, llink_t *lnk);


/*
 * Link node to end of list
 */
CAC_ATTR_NONNULL
void list_push_back(list_t *ls, llink_t *lnk);


/*
 * Insert node before existing node in list
 */
CAC_ATTR_NONNULL
void list_insert_before(list_t *ls, llink_t *lnk, llink_t *next_lnk);

/*
 * Insert node after existing node in list
 */
CAC_ATTR_NONNULL
void list_insert_after(list_t *ls, llink_t *prev_lnk, llink_t *lnk);

/*
 * Unlink existing node.
 */
CAC_ATTR_NONNULL
void list_remove(list_t *ls, llink_t *lnk);


/*
 * Removes the first node.
 */
CAC_ATTR_NONNULL
llink_t *list_pop_front(list_t *ls);

/*
 * Removes the last node.
 */
CAC_ATTR_NONNULL
llink_t *list_pop_back(list_t *ls);



/*
 * Returns the first node, or end() if empty.
 */
CAC_ATTR_NONNULL
llink_t *list_begin(const list_t *ls);

/*
 * Returns one "past" last valid node.
 */
CAC_ATTR_NONNULL
llink_t *list_end(const list_t *ls);


/*
 * Iterators semantics (bidirectional).
 */
CAC_ATTR_NONNULL
llink_t *list_next(const list_t *ls, const llink_t *lnk);

CAC_ATTR_NONNULL
llink_t *list_prev(const list_t *ls, const llink_t *lnk);


/*
 * Returns the first/last element, or NULL if empty.
 */
CAC_ATTR_NONNULL
llink_t *list_front(const list_t *ls);

CAC_ATTR_NONNULL
llink_t *list_back(const list_t *ls);


/*
 * Removes all elements (reduces size to zero). Returns a list of the linked
 * elements prior to the clear operation.
 *
 * NB: It is up to the user to release any resource associate with the list's
 * element.
 */
CAC_ATTR_NONNULL
llink_t *list_clear(list_t *ls);


/*
 * Searches for the first element for which fn returns TRUE. If none exists in
 * the list, returns NULL.
 */
CAC_ATTR_NONNULL
llink_t *list_find(const list_t *ls, llink_unary_pred fn);

/*
 * Searches for the first element for which fn with v returns TRUE. If none
 * exists in the list, returns NULL.
 */
CAC_ATTR_NONNULL
llink_t *list_vfind(const list_t *ls,
                    llink_vbinary_pred fn, const void *v);


/*
 * Apply unary function fn on every element in the list.
 */
CAC_ATTR_NONNULL
void list_foreach(const list_t *ls, llink_unary_fn fn);


/*
 * Apply unary function fn on every element in the list with v.
 */
CAC_ATTR_NONNULL
void list_vforeach(const list_t *ls, llink_vbinary_fn fn, const void *v);



/*
 * Swaps the content of two lists.
 */
CAC_ATTR_NONNULL
void list_swap(list_t *ls1, list_t *ls2);

/*
 * Appends the content of list2 to list1. Post-op, the size of list2 is reduced
 * to zero.
 */
CAC_ATTR_NONNULL
void list_append(list_t *ls1, list_t *ls2);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Constructor: initializes an empty, null-terminated, single-linked list.
 */
CAC_ATTR_NONNULL
void slist_init(slink_t *head);

/*
 * Destroys list object: iterates on list's elements and break their links.
 *
 * NB: It is up to the user to release any linked resources associated with
 * the list.
 */
CAC_ATTR_NONNULL
void slist_destroy(slink_t *head);

/*
 * Iterates and counts the number of linked-elements in list.
 */
CAC_ATTR_NONNULL
size_t slist_length(const slink_t *head);

/*
 * Returns TRUE if no elements are associated with the list.
 */
CAC_ATTR_NONNULL
int slist_isempty(const slink_t *head);


/*
 * Link user-provided element into the beginning of list (intrusive).
 */
CAC_ATTR_NONNULL
void slist_push_front(slink_t *head, slink_t *lnk);

/*
 * Removes the first element.
 */
CAC_ATTR_NONNULL
slink_t *slist_pop_front(slink_t *head);


/*
 * Inserts an element prepsius to existing element in the list. Returns NULL if
 * pos element was not found in list.
 */
CAC_ATTR_NONNULL
slink_t *slist_insert(slink_t *head,
                      slink_t *pos, slink_t *lnk);

/*
 * Removes an element which is linked on list. Returns NULL if the element was
 * not found in list.
 */
CAC_ATTR_NONNULL
slink_t *slist_remove(slink_t *head, slink_t *lnk);


/*
 * Searches for the first element for which fn with v returns TRUE. If none
 * exists in the list, returns NULL.
 */
slink_t *slist_find(const slink_t *head,
                    slink_test_fn fn, const void *v);

/*
 * Searches for the first element for which fn with v returns TRUE and remove it
 * from the list. If exists returns the removed element; otherwise returns NULL.
 */
slink_t *slist_findremove(slink_t *head,
                          slink_test_fn fn, const void *v);

/*
 * Apply user-provided hook fn with v on every element in the list.
 */
void slist_foreach(const slink_t *head,
                   slink_foreach_fn fn, const void *v);


#endif /* CAC_LIST_H_ */


