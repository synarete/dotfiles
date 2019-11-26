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
#ifndef FNX_INFRA_TREE_H_
#define FNX_INFRA_TREE_H_


struct fnx_keyref;
struct fnx_tlink;
struct fnx_tree;


enum FNX_TREE_TYPE {
	FNX_TREE_AVL    = 1,
	FNX_TREE_RB     = 2,
	FNX_TREE_TREAP  = 4
};
typedef enum FNX_TREE_TYPE    fnx_treetype_e;


/* Get key-ref of tree-node */
typedef const void *(*fnx_getkey_fn)(const struct fnx_tlink *);

/* 3-way compare function-pointer */
typedef int (*fnx_keycmp_fn)(const void *, const void *);



/* Node operator */
typedef void (*fnx_tnode_fn)(struct fnx_tlink *, void *);

/* User-provided tree-node operations-hooks */
struct fnx_treehooks {
	fnx_getkey_fn   getkey_hook;
	fnx_keycmp_fn   keycmp_hook;
};
typedef struct fnx_treehooks    fnx_treehooks_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * "Base" for specific self-balancing binary-search-tree nodes.
 */
struct fnx_aligned fnx_tlink {
	struct fnx_tlink *parent;
	struct fnx_tlink *left;
	struct fnx_tlink *right;

	union {
		int avl_balance;
		int rb_color;
		unsigned long treap_priority;
		const void *alignment;
	} d;
};
typedef struct fnx_tlink fnx_tlink_t;


/*
 * "Iterators" range a-la STL pair.
 */
struct fnx_tree_range {
	fnx_tlink_t *first;
	fnx_tlink_t *second;
};
typedef struct fnx_tree_range fnx_tree_range_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Binary-search-tree (self-balancing). Holds reference to user-provided nodes
 * (intrusive container). Uses user-provided hooks for resolving node-to-key
 * and key-compare. Head stores leftmost, rightmost and root pointers.
 *
 * NB: It is up to the user to release any associated resources, including the
 * possibly still-linked nodes upon destruction.
 */
struct fnx_tree {
	size_t tr_nodecount;            /* Size (num nodes) */
	unsigned int    tr_pad_;        /* Alignment */
	fnx_treetype_e   tr_type;        /* Sub-type (AVL,RB,Treap) */
	fnx_tlink_t      tr_head;        /* Left, right, root */
	const fnx_treehooks_t *tr_uops; /* User provided oper-hooks */
};
typedef struct fnx_tree  fnx_tree_t;



/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/


/*
 * Set all links to NULL.
 */
FNX_ATTR_NONNULL
void fnx_tlink_init(fnx_tlink_t *x);

FNX_ATTR_NONNULL
void fnx_tlink_destroy(fnx_tlink_t *x);



/*
 * Construct empty tree (AVL|RB|Treap), using user-provided hooks.
 */
FNX_ATTR_NONNULL
void fnx_avl_init(fnx_tree_t *, const fnx_treehooks_t *);

FNX_ATTR_NONNULL
void fnx_rb_init(fnx_tree_t *, const fnx_treehooks_t *);

FNX_ATTR_NONNULL
void fnx_treap_init(fnx_tree_t *, const fnx_treehooks_t *);

FNX_ATTR_NONNULL
void fnx_tree_init(fnx_tree_t *, fnx_treetype_e, const fnx_treehooks_t *);


/*
 * Destructor: clear & zero.
 */
FNX_ATTR_NONNULL
void fnx_tree_destroy(fnx_tree_t *);

/*
 * Returns True is no elements in tree.
 */
FNX_ATTR_NONNULL
int fnx_tree_isempty(const fnx_tree_t *);

/*
 * Returns the number elements stored in tree.
 */
FNX_ATTR_NONNULL
size_t fnx_tree_size(const fnx_tree_t *);

/*
 * Returns the number of elements with key.
 */
FNX_ATTR_NONNULL
size_t fnx_tree_count(const fnx_tree_t *, const void *);

/*
 * Returns pointer with iterator-semantics to the first element.
 */
FNX_ATTR_NONNULL
fnx_tlink_t *fnx_tree_begin(const fnx_tree_t *);

/*
 * Returns pointer with iterator-semantics to one past the last element.
 */
FNX_ATTR_NONNULL
fnx_tlink_t *fnx_tree_end(const fnx_tree_t *);

/*
 * Returns TRUE is an iterator is referring to end-of-sequence (or NULL).
 */
int fnx_tree_iseoseq(const fnx_tree_t *, const fnx_tlink_t *);

/*
 * Enables iterator operator++
 * Returns fnx_tree_end at the end of sequence.
 */
fnx_tlink_t *fnx_tree_next(const fnx_tree_t *, const fnx_tlink_t *);

/*
 * Enables iterator operator--
 */
fnx_tlink_t *fnx_tree_prev(const fnx_tree_t *, const fnx_tlink_t *);

/*
 * Searches for an element by key. Returns NULL if not exist.
 */
FNX_ATTR_NONNULL
fnx_tlink_t *fnx_tree_find(const fnx_tree_t *, const void *);

/*
 * Searches for the first element by key. Returns NULL if not exist.
 */
FNX_ATTR_NONNULL
fnx_tlink_t *fnx_tree_find_first(const fnx_tree_t *, const void *);

/*
 * Finds the first element whose key is not less than. Returns NULL if not
 * exist.
 */
FNX_ATTR_NONNULL
fnx_tlink_t *fnx_tree_lower_bound(const fnx_tree_t *, const void *);

/*
 * Finds the first element whose key greater than. Returns NULL if not exist.
 */
FNX_ATTR_NONNULL
fnx_tlink_t *fnx_tree_upper_bound(const fnx_tree_t *, const void *);

/*
 * Finds a range containing all elements with key.
 */
FNX_ATTR_NONNULL
void fnx_tree_equal_range(const fnx_tree_t *,
                          const void *, fnx_tree_range_t *);

/*
 * Inserts new element. Possibly multiple elements with same key.
 */
FNX_ATTR_NONNULL
void fnx_tree_insert(fnx_tree_t *, fnx_tlink_t *);

/*
 * Inserts new unique element. Returns -1 if key already exists.
 */
FNX_ATTR_NONNULL
int fnx_tree_insert_unique(fnx_tree_t *, fnx_tlink_t *);

/*
 * Inserts new element or replace existing one. Returns prepsius element if key
 * already exists, NULL otherwise.
 */
FNX_ATTR_NONNULL
fnx_tlink_t *fnx_tree_insert_replace(fnx_tree_t *, fnx_tlink_t *);

/*
 * Erases an existing element.
 */
FNX_ATTR_NONNULL
void fnx_tree_remove(fnx_tree_t *, fnx_tlink_t *);

/*
 * Erases iterators range [first, last).
 */
FNX_ATTR_NONNULL
void fnx_tree_remove_range(fnx_tree_t *, fnx_tlink_t *, fnx_tlink_t *);

/*
 * Do tree-walk, remove each node and apply user's hook. User provided pointer
 * ptr is passed as hint to fn.
 */
FNX_ATTR_NONNULL1
void fnx_tree_clear(fnx_tree_t *, fnx_tnode_fn fn, void *ptr);


/*
 * Removes all the elements from the tree. Returns a linked list of prepsiusly
 * stored elements (linked by ->right).
 *
 * NB: It is up to the user to release any resources associated with the tree's
 *     nodes.
 */
FNX_ATTR_NONNULL
fnx_tlink_t *fnx_tree_reset(fnx_tree_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Common 3-way compare functions when using (unsigned) long-integers as keys.
 */
int fnx_compare3(long x, long y);

int fnx_ucompare3(unsigned long x, unsigned long y);


#endif /* FNX_INFRA_TREE_H_ */




