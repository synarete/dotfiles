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
#ifndef FNX_INFRA_LIST_H_
#define FNX_INFRA_LIST_H_


/*
 * Forward declarations:
 */
struct fnx_link;
struct fnx_list;


/*
 * List-link element within bidirectional linked-list. Typically, part (at the
 * beginning) of user's object.
 */
struct fnx_link {
	struct fnx_link *next;
	struct fnx_link *prev;

} FNX_ATTR_ALIGNED;
typedef struct fnx_link     fnx_link_t;


/*
 * Bidirectional cyclic linked-list. The actual type of objects is defined
 * by the user.
 *
 * NB: It is up to the user to ensure that the linked elements remain valid
 * throughout the time they are members in the list.
 *
 */
struct fnx_list {
	fnx_link_t head;    /* "Pseudo" node; Always be cyclic   */
	size_t     size;    /* Number of elements (without head) */
};
typedef struct fnx_list         fnx_list_t;


/*
 * Unidirectional linked-list: each element is linked only to the next element.
 * That is, a sequence-container that supports forward traversal only, and
 * (amortized) constant time insertion of elements. The actual type of objects
 * is defined by the user.
 *
 * NB: It is up to the user to ensure that the linked elements remain valid
 * throughout the time they are members os a list.
 */
struct fnx_slink {
	struct fnx_slink *next;
};
typedef struct fnx_slink    fnx_slink_t;


typedef int (*fnx_slink_test_fn)(const fnx_slink_t *, const void *);
typedef void (*fnx_slink_foreach_fn)(fnx_slink_t *, const void *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Compare/Apply functions:
 */
typedef int (*fnx_link_unary_pred)(const fnx_link_t *);

typedef int (*fnx_link_binary_pred)(const fnx_link_t *,
                                    const fnx_link_t *);

typedef int (*fnx_link_vbinary_pred)(const fnx_link_t *, const void *);

typedef int (*fnx_link_unary_fn)(fnx_link_t *);

typedef int (*fnx_link_binary_fn)(fnx_link_t *, fnx_link_t *);

typedef int (*fnx_link_vbinary_fn)(fnx_link_t *, const void *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Create cyclic (unlinked, selfish) link */
FNX_ATTR_NONNULL
void fnx_link_init(fnx_link_t *);

/* Have NULL links */
FNX_ATTR_NONNULL
void fnx_link_destroy(fnx_link_t *);


/* Set link within linked-list */
void fnx_link_set(fnx_link_t *, fnx_link_t *, fnx_link_t *);

/* Link into existing list, keep it in order */
void fnx_link_insert(fnx_link_t *, fnx_link_t *, fnx_link_t *);

/* Remove link from cyclic-linked list, keep it in order */
FNX_ATTR_NONNULL
void fnx_link_remove(fnx_link_t *);


/* Returns TRUE if link is not associated with any list (selfish links) */
FNX_ATTR_NONNULL
int fnx_link_isunlinked(const fnx_link_t *);

/* Returns TRUE if link is associated with a list (non-selfish no-null links) */
FNX_ATTR_NONNULL
int fnx_link_islinked(const fnx_link_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Constructor: initializes an empty, cyclic list.
 */
FNX_ATTR_NONNULL
void fnx_list_init(fnx_list_t *ls);

/*
 * Destroys list object. NB: It is up to the user to release any linked
 * resources associated with the list.
 */
FNX_ATTR_NONNULL
void fnx_list_destroy(fnx_list_t *ls);


/*
 * Returns the number of elements currently linked in list.
 */
FNX_ATTR_NONNULL
size_t fnx_list_size(const fnx_list_t *ls);


/*
 * Returns TRUE if no elements are associated with the list.
 */
FNX_ATTR_NONNULL
int fnx_list_isempty(const fnx_list_t *ls);



/*
 * Link user-provided node into the beginning of list (intrusive).
 */
FNX_ATTR_NONNULL
void fnx_list_push_front(fnx_list_t *ls, fnx_link_t *lnk);


/*
 * Link node to end of list
 */
FNX_ATTR_NONNULL
void fnx_list_push_back(fnx_list_t *ls, fnx_link_t *lnk);


/*
 * Insert node before existing node in list
 */
FNX_ATTR_NONNULL
void fnx_list_insert_before(fnx_list_t *ls,
                            fnx_link_t *lnk, fnx_link_t *next_lnk);

/*
 * Insert node after existing node in list
 */
FNX_ATTR_NONNULL
void fnx_list_insert_after(fnx_list_t *ls,
                           fnx_link_t *prev_lnk, fnx_link_t *lnk);

/*
 * Unlink existing node.
 */
FNX_ATTR_NONNULL
void fnx_list_remove(fnx_list_t *ls, fnx_link_t *lnk);


/*
 * Removes the first node.
 */
FNX_ATTR_NONNULL
fnx_link_t *fnx_list_pop_front(fnx_list_t *ls);

/*
 * Removes the last node.
 */
FNX_ATTR_NONNULL
fnx_link_t *fnx_list_pop_back(fnx_list_t *ls);



/*
 * Returns the first node, or end() if empty.
 */
FNX_ATTR_NONNULL
fnx_link_t *fnx_list_begin(const fnx_list_t *ls);

/*
 * Returns one "past" last valid node.
 */
FNX_ATTR_NONNULL
fnx_link_t *fnx_list_end(const fnx_list_t *ls);


/*
 * Iterators semantics (bidirectional).
 */
FNX_ATTR_NONNULL
fnx_link_t *fnx_list_next(const fnx_list_t *ls, const fnx_link_t *lnk);

FNX_ATTR_NONNULL
fnx_link_t *fnx_list_prev(const fnx_list_t *ls, const fnx_link_t *lnk);


/*
 * Returns the first/last element, or NULL if empty.
 */
FNX_ATTR_NONNULL
fnx_link_t *fnx_list_front(const fnx_list_t *ls);

FNX_ATTR_NONNULL
fnx_link_t *fnx_list_back(const fnx_list_t *ls);


/*
 * Removes all elements (reduces size to zero). Returns a list of the linked
 * elements prior to the clear operation.
 *
 * NB: It is up to the user to release any resource associate with the list's
 * element.
 */
FNX_ATTR_NONNULL
fnx_link_t *fnx_list_clear(fnx_list_t *ls);


/*
 * Searches for the first element for which fn returns TRUE. If none exists in
 * the list, returns NULL.
 */
FNX_ATTR_NONNULL
fnx_link_t *fnx_list_find(const fnx_list_t *ls, fnx_link_unary_pred fn);

/*
 * Searches for the first element for which fn with v returns TRUE. If none
 * exists in the list, returns NULL.
 */
FNX_ATTR_NONNULL
fnx_link_t *fnx_list_vfind(const fnx_list_t *ls,
                           fnx_link_vbinary_pred fn, const void *v);


/*
 * Apply unary function fn on every element in the list.
 */
FNX_ATTR_NONNULL
void fnx_list_foreach(const fnx_list_t *ls, fnx_link_unary_fn fn);


/*
 * Apply unary function fn on every element in the list with v.
 */
FNX_ATTR_NONNULL
void fnx_list_vforeach(const fnx_list_t *ls,
                       fnx_link_vbinary_fn fn, const void *v);



/*
 * Swaps the content of two lists.
 */
FNX_ATTR_NONNULL
void fnx_list_swap(fnx_list_t *ls1, fnx_list_t *ls2);

/*
 * Appends the content of list2 to list1. Post-op, the size of list2 is reduced
 * to zero.
 */
FNX_ATTR_NONNULL
void fnx_list_append(fnx_list_t *ls1, fnx_list_t *ls2);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Constructor: initializes an empty, null-terminated, single-linked list.
 */
FNX_ATTR_NONNULL
void fnx_slist_init(fnx_slink_t *head);

/*
 * Destroys list object: iterates on list's elements and break their links.
 *
 * NB: It is up to the user to release any linked resources associated with
 * the list.
 */
FNX_ATTR_NONNULL
void fnx_slist_destroy(fnx_slink_t *head);

/*
 * Iterates and counts the number of linked-elements in list.
 */
FNX_ATTR_NONNULL
size_t fnx_slist_length(const fnx_slink_t *head);

/*
 * Returns TRUE if no elements are associated with the list.
 */
FNX_ATTR_NONNULL
int fnx_slist_isempty(const fnx_slink_t *head);


/*
 * Link user-provided element into the beginning of list (intrusive).
 */
FNX_ATTR_NONNULL
void fnx_slist_push_front(fnx_slink_t *head, fnx_slink_t *lnk);

/*
 * Removes the first element.
 */
FNX_ATTR_NONNULL
fnx_slink_t *fnx_slist_pop_front(fnx_slink_t *head);


/*
 * Inserts an element prepsius to existing element in the list. Returns NULL if
 * pos element was not found in list.
 */
FNX_ATTR_NONNULL
fnx_slink_t *fnx_slist_insert(fnx_slink_t *head,
                              fnx_slink_t *pos, fnx_slink_t *lnk);

/*
 * Removes an element which is linked on list. Returns NULL if the element was
 * not found in list.
 */
FNX_ATTR_NONNULL
fnx_slink_t *fnx_slist_remove(fnx_slink_t *head, fnx_slink_t *lnk);


/*
 * Searches for the first element for which fn with v returns TRUE. If none
 * exists in the list, returns NULL.
 */
fnx_slink_t *fnx_slist_find(const fnx_slink_t *head,
                            fnx_slink_test_fn fn, const void *v);

/*
 * Searches for the first element for which fn with v returns TRUE and remove it
 * from the list. If exists returns the removed element; otherwise returns NULL.
 */
fnx_slink_t *fnx_slist_findremove(fnx_slink_t *head,
                                  fnx_slink_test_fn fn, const void *v);

/*
 * Apply user-provided hook fn with v on every element in the list.
 */
void fnx_slist_foreach(const fnx_slink_t *head,
                       fnx_slink_foreach_fn fn, const void *v);


#endif /* FNX_INFRA_LIST_H_ */


