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
#include "infra-utility.h"
#include "infra-panic.h"
#include "infra-tree.h"


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
    See: Corman, Leiserson, Rivest, Stein, "INTRODUCTION TO ALGORITHMS",
    2nd ed., The MIT Press, Ch. 12 "Binary Search Trees".

    Rotate-Left-Right:
                |                               |
                a                               c
               / \                             / \
              /   \        ==>                /   \
             b    [g]                        b     a
            / \                             / \   / \
          [d]  c                          [d] e  f  [g]
              / \
             e   f


    Rotate-Right-Left:
                |                               |
                a                               c
               / \                             / \
              /   \                           /   \
            [d]   b        ==>               a     b
                 / \                        / \   / \
                c  [g]                    [d] e  f  [g]
               / \
              e  f

 */
static void tlink_init(fnx_tlink_t *x, fnx_tlink_t *p)
{
	x->left = x->right = NULL;
	x->parent = p;
}

static void tlink_reset(fnx_tlink_t *x)
{
	x->left = x->parent = x->right = NULL;
}

static void tlink_destroy(fnx_tlink_t *x)
{
	x->left = x->right = x->parent = NULL;
}

static const fnx_tlink_t *tlink_minimum(const fnx_tlink_t *x)
{
	while (x->left) {
		x = x->left;
	}
	return x;
}

static const fnx_tlink_t *tlink_maximum(const fnx_tlink_t *x)
{
	while (x->right) {
		x = x->right;
	}
	return x;
}

static fnx_tlink_t *tlink_successor(const fnx_tlink_t *x)
{
	const fnx_tlink_t *y;

	if (x->right) {
		y = tlink_minimum(x->right);
	} else {
		y = x->parent;
		while (y && (x == y->right)) {
			x = y;
			y = y->parent;
		}
	}
	return (fnx_tlink_t *) y;
}

static fnx_tlink_t *tlink_predecessor(const fnx_tlink_t *x)
{
	const fnx_tlink_t *y;

	if (x->left) {
		y = tlink_maximum(x->left);
	} else {
		y = x->parent;
		while (y && (x == y->left)) {
			x = y;
			y = y->parent;
		}
	}
	return (fnx_tlink_t *) y;
}

static void tlink_rotate_left(fnx_tlink_t *x, fnx_tlink_t **root)
{
	fnx_tlink_t *y;

	y = x->right;
	x->right = y->left;
	if (y->left != NULL) {
		y->left->parent = x;
	}
	y->parent = x->parent;
	if (x == *root) {
		*root = y;
	} else if (x == x->parent->left) {
		x->parent->left = y;
	} else {
		x->parent->right = y;
	}
	y->left = x;
	x->parent = y;
}

static void tlink_rotate_right(fnx_tlink_t *x, fnx_tlink_t **root)
{
	fnx_tlink_t *y;

	y = x->left;
	x->left = y->right;
	if (y->right != NULL) {
		y->right->parent = x;
	}
	y->parent = x->parent;
	if (x == *root) {
		*root = y;
	} else if (x == x->parent->right) {
		x->parent->right = y;
	} else {
		x->parent->left = y;
	}
	y->right = x;
	x->parent = y;
}

static void tlink_rotate_left_right(fnx_tlink_t *a, fnx_tlink_t **root)
{
	fnx_tlink_t *b, *c;

	b = a->left;
	c = b->right;

	a->left = c->right;
	b->right = c->left;

	c->right = a;
	c->left = b;

	c->parent = a->parent;
	a->parent = b->parent = c;

	if (a->left) {
		a->left->parent = a;
	}
	if (b->right) {
		b->right->parent = b;
	}

	if (a == *root) {
		*root = c;
	} else {
		if (a == c->parent->left) {
			c->parent->left = c;
		} else {
			c->parent->right = c;
		}
	}
}

static void tlink_rotate_right_left(fnx_tlink_t *a, fnx_tlink_t **root)
{
	fnx_tlink_t *b, *c;

	b = a->right;
	c = b->left;

	a->right = c->left;
	b->left = c->right;

	c->left = a;
	c->right = b;

	c->parent = a->parent;
	a->parent = b->parent = c;

	if (a->right) {
		a->right->parent = a;
	}
	if (b->left) {
		b->left->parent = b;
	}
	if (a == *root) {
		*root = c;
	} else {
		if (a == c->parent->left) {
			c->parent->left = c;
		} else {
			c->parent->right = c;
		}
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* AVL */
static void avl_swap_balance(fnx_tlink_t *x, fnx_tlink_t *y)
{
	int balance;

	balance = x->d.avl_balance;
	x->d.avl_balance  = y->d.avl_balance;
	y->d.avl_balance  = balance;
}

static void avl_init_node(fnx_tlink_t *x)
{
	tlink_init(x, NULL);
	x->d.avl_balance = 0;
}

static void avl_rotate_left(fnx_tlink_t *x, fnx_tlink_t **root)
{
	fnx_tlink_t *y;

	y = x->right;
	tlink_rotate_left(x, root);

	if (y->d.avl_balance == 1) {
		x->d.avl_balance = 0;
		y->d.avl_balance = 0;
	} else {
		x->d.avl_balance = 1;
		y->d.avl_balance = -1;
	}
}

static void avl_rotate_right(fnx_tlink_t *x, fnx_tlink_t **root)
{
	fnx_tlink_t *y   = x->left;

	tlink_rotate_right(x, root);

	if (y->d.avl_balance == -1) {
		x->d.avl_balance = 0;
		y->d.avl_balance = 0;
	} else {
		x->d.avl_balance = -1;
		y->d.avl_balance = 1;
	}
}

static void avl_rotate_left_right(fnx_tlink_t *a, fnx_tlink_t **root)
{
	fnx_tlink_t *b = a->left;
	fnx_tlink_t *c = b->right;

	tlink_rotate_left_right(a, root);

#if 1
	switch (c->d.avl_balance) {
		case -1:
			a->d.avl_balance = 1;
			b->d.avl_balance = 0;
			break;
		case 0:
			a->d.avl_balance = 0;
			b->d.avl_balance = 0;
			break;
		case 1:
			a->d.avl_balance = 0;
			b->d.avl_balance = -1;
			break;
		default:
			fnx_panic("AVL-error '%s'", FNX_FUNCTION);
			break;
	}

#else
	a->d.avl_balance = (c->d.avl_balance == -1) ? 1 : 0;
	b->d.avl_balance = (c->d.avl_balance == 1) ? -1 : 0;
#endif

	c->d.avl_balance = 0;
}

/*
                |                               |
                a(1)                            c
               / \                             / \
              /   \                           /   \
            [d]   b(-1)          ==>         a     b
                 / \                        / \   / \
                c  [g]                    [d] e  f  [g]
               / \
              e  f
*/
static void avl_rotate_right_left(fnx_tlink_t *a, fnx_tlink_t **root)
{
	fnx_tlink_t *b = a->right;
	fnx_tlink_t *c = b->left;

	tlink_rotate_right_left(a, root);

#if 0
	switch (c->d.avl_balance) {
		case -1:
			a->d.avl_balance = 0;
			b->d.avl_balance = 1;
			break;
		case 0:
			a->d.avl_balance = 0;
			b->d.avl_balance = 0;
			break;
		case 1:
			a->d.avl_balance = -1;
			b->d.avl_balance = 0;
			break;

		default:
			fnx_panic("AVL-error '%s'", FNX_FUNCTION);
	}
#else
	a->d.avl_balance = (c->d.avl_balance == 1) ? -1 : 0;
	b->d.avl_balance = (c->d.avl_balance == -1) ? 1 : 0;
#endif

	c->d.avl_balance = 0;
}

/*
 * Re-balance AVL-tree after insertion of node x.
 */
static void avl_insert_fixup(fnx_tlink_t *x, fnx_tlink_t **root)
{
	while (x != *root) {
		switch (x->parent->d.avl_balance) {
			case 0:
				/* If x is left, parent will have parent->balance = -1
				    else, parent->balance = 1 */
				x->parent->d.avl_balance = (x == x->parent->left) ? -1 : 1;
				x = x->parent;
				break;

			case 1:
				/* If x is a left child, parent->balance = 0 */

				if (x == x->parent->left) {
					x->parent->d.avl_balance = 0;
				} else {
					/* x is a right child, needs rebalancing */
					if (x->d.avl_balance == -1) {
						avl_rotate_right_left(x->parent, root);
					} else {
						avl_rotate_left(x->parent, root);
					}
				}
				return;

			case -1:
				/* If x is a left child, needs re-balancing */

				if (x == x->parent->left) {
					if (x->d.avl_balance == 1) {
						avl_rotate_left_right(x->parent, root);
					} else {
						avl_rotate_right(x->parent, root);
					}
				} else {
					x->parent->d.avl_balance = 0;
				}
				return;

			default:
				fnx_panic("AVL-error '%s'", FNX_FUNCTION);
				break;
		}
	}
}
/*
 * Re-balanced AVL-tree after erased node.
 */
static void avl_delete_fixup(fnx_tlink_t *x,
                             fnx_tlink_t *x_parent, fnx_tlink_t **root)
{
	fnx_tlink_t *a = NULL;

	while (x != *root) {
		switch (x_parent->d.avl_balance) {
			case 0:
				x_parent->d.avl_balance = (x == x_parent->right) ? -1 : 1;
				return;       /* the height didn't change, let's stop here */

			case -1:
				if (x == x_parent->left) {
					x_parent->d.avl_balance = 0; /* balanced */
					x = x_parent;
					x_parent = x_parent->parent;
				} else {
					/* x is right child
					   a is left child */
					a = x_parent->left;
					if (a->d.avl_balance == 1) {
						/* a MUST have a right child */
						avl_rotate_left_right(x_parent, root);
						x = x_parent->parent;
						x_parent = x_parent->parent->parent;
					} else {
						avl_rotate_right(x_parent, root);
						x = x_parent->parent;
						x_parent = x_parent->parent->parent;
					}

					/* if changed from -1 to 1, no need to check above */
					if (x->d.avl_balance == 1) {
						return;
					}
				}
				break;

			case 1:
				if (x == x_parent->right) {
					x_parent->d.avl_balance = 0; /* balanced */
					x = x_parent;
					x_parent = x_parent->parent;
				} else {
					/* x is left child
					   a is right child */
					a = x_parent->right;
					if (a->d.avl_balance == -1) {
						/* a MUST have then a left child */
						avl_rotate_right_left(x_parent, root);
						x = x_parent->parent;
						x_parent = x_parent->parent->parent;
					} else {
						avl_rotate_left(x_parent, root);
						x = x_parent->parent;
						x_parent = x_parent->parent->parent;
					}
					/* if changed from 1 to -1, no need to check above */
					if (x->d.avl_balance == -1) {
						return;
					}
				}
				break;

			default:
				fnx_panic("avl_balance=%d", (int)x_parent->d.avl_balance);
				break;
		}
	}
}

/* Remove node from AVL-tree */
static void avl_delete(fnx_tlink_t *z, fnx_tlink_t **root)
{
	fnx_tlink_t *y, *x, *xp = NULL;

	if (z->left == NULL) {
		y = z;
		x = z->right;
	} else if (z->right == NULL) {
		y = z;
		x = z->left;
	} else {
		/* z has two non-null children. */
		y = tlink_successor(z);
		x = y->right;
	}
	if (y != z) { /* z has two non-null childrens and y is z's successor */
		/* Relink y in place of z. */
		z->left->parent = y;
		y->left = z->left;

		if (y != z->right) {
			xp = y->parent;
			if (x != NULL) {
				x->parent = y->parent;
			}
			y->parent->left = x;   /* y must be a child of left */
			y->right = z->right;
			z->right->parent = y;
		} else {
			xp = y;
		}
		if (*root == z) {
			/* If we are deleting the root the new root is y */
			*root = y;
		} else if (z->parent->left == z) {   /* else, fix parent's child */
			z->parent->left = y;
		} else {
			z->parent->right = y;
		}
		y->parent = z->parent;
		avl_swap_balance(y, z);
		/*y = z;*/ /* y now points to node to be actually deleted */
	} else { /* y == z    --> z has only one child, or none */
		xp = y->parent;
		if (x != NULL) {
			/* if z has at least one child, new parent is now y */
			x->parent = y->parent;
		}
		if (*root == z) {    /* if we deleted the root */
			/* if we deleted the root, new root is x */
			*root = x;
		} else {
			/* else, fix the parent's child */
			if (z->parent->left == z) {
				z->parent->left = x;
			} else {
				z->parent->right = x;
			}
		}
	}
	avl_delete_fixup(x, xp, root);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Red-Black Tree */
#define RED         (0)
#define BLACK       (1)

static void rb_swap_color(fnx_tlink_t *x, fnx_tlink_t *y)
{
	int color;

	color = x->d.rb_color;
	x->d.rb_color = y->d.rb_color;
	y->d.rb_color = color;
}

static void rb_init_node(fnx_tlink_t *x)
{
	tlink_init(x, NULL);
	x->d.rb_color = BLACK;
}

static void rb_rotate_left(fnx_tlink_t *x, fnx_tlink_t **root)
{
	fnx_tlink_t *y;

	y = x->right;
	x->right = y->left;
	if (y->left != NULL) {
		y->left->parent = x;
	}
	y->parent = x->parent;
	if (x == *root) {
		*root = y;
	} else if (x == x->parent->left) {
		x->parent->left = y;
	} else {
		x->parent->right = y;
	}
	y->left     = x;
	x->parent   = y;
}

static void rb_rotate_right(fnx_tlink_t *x, fnx_tlink_t **root)
{
	fnx_tlink_t *y;

	y = x->left;
	x->left = y->right;
	if (y->right != NULL) {
		y->right->parent = x;
	}
	y->parent = x->parent;
	if (x == *root) {
		*root = y;
	} else if (x == x->parent->right) {
		x->parent->right = y;
	} else {
		x->parent->left = y;
	}
	y->right  = x;
	x->parent = y;
}

static void rb_insert_fixup(fnx_tlink_t *x, fnx_tlink_t **root)
{
	fnx_tlink_t *y, *xpp = NULL;

	x->d.rb_color = RED;
	while ((x != *root) && (x->parent->d.rb_color == RED)) {
		xpp = x->parent->parent;

		if (x->parent == xpp->left) {
			y = xpp->right;
			if (y && (y->d.rb_color == RED)) {
				x->parent->d.rb_color = BLACK;
				y->d.rb_color = BLACK;
				xpp->d.rb_color = RED;
				x = xpp;
			} else {
				if (x == x->parent->right) {
					x = x->parent;
					rb_rotate_left(x, root);
				}

				x->parent->d.rb_color = BLACK;
				xpp->d.rb_color = RED;
				rb_rotate_right(xpp, root);
			}
		} else {
			y = xpp->left;
			if (y && (y->d.rb_color == RED)) {
				x->parent->d.rb_color = BLACK;
				y->d.rb_color = BLACK;
				xpp->d.rb_color = RED;
				x = xpp;
			} else {
				if (x == x->parent->left) {
					x = x->parent;
					rb_rotate_right(x, root);
				}

				x->parent->d.rb_color = BLACK;
				xpp->d.rb_color = RED;
				rb_rotate_left(xpp, root);
			}
		}
	}
	if (*root) {
		(*root)->d.rb_color = BLACK;
	}
}

static void
rb_delete_fixup(fnx_tlink_t *x, fnx_tlink_t *x_parent, fnx_tlink_t **root)
{
	fnx_tlink_t *w = NULL;

	while ((x != *root) && (!x || (x->d.rb_color == BLACK))) {
		if (x == x_parent->left) {
			w = x_parent->right;
			if (w->d.rb_color == RED) {
				w->d.rb_color        = BLACK;
				x_parent->d.rb_color = RED;
				rb_rotate_left(x_parent, root);
				w = x_parent->right;
			}
			if ((!w->left || (w->left->d.rb_color == BLACK)) &&
			    (!w->right || (w->right->d.rb_color == BLACK))) {
				w->d.rb_color = RED;
				x = x_parent;
				x_parent = x_parent->parent;
			} else {
				if (!w->right || (w->right->d.rb_color == BLACK)) {
					if (w->left == NULL) {
						fnx_panic("corrupted-tree root=%p link=%p",
						          (const void *)(*root), (const void *)w);
					}
					w->left->d.rb_color = BLACK;
					w->d.rb_color = RED;
					rb_rotate_right(w, root);
					w = x_parent->right;
				}
				w->d.rb_color = x_parent->d.rb_color;
				x_parent->d.rb_color = BLACK;
				if (w->right) {
					w->right->d.rb_color = BLACK;
				}
				rb_rotate_left(x_parent, root);
				break;
			}
		} else {
			/* same as above, with right <-> left. */
			w = x_parent->left;
			if (w->d.rb_color == RED) {
				w->d.rb_color = BLACK;
				x_parent->d.rb_color = RED;
				rb_rotate_right(x_parent, root);
				w = x_parent->left;
			}

			if ((!w->right || (w->right->d.rb_color == BLACK)) &&
			    (!w->left  || (w->left->d.rb_color  == BLACK))) {
				w->d.rb_color = RED;
				x = x_parent;
				x_parent = x_parent->parent;
			} else {
				if (!w->left || (w->left->d.rb_color == BLACK)) {
					if (w->right == NULL) {
						fnx_panic("corrupted-tree root=%p link=%p",
						          (const void *)(*root), (const void *)w);
					}
					w->right->d.rb_color = BLACK;
					w->d.rb_color = RED;
					rb_rotate_left(w, root);
					w = x_parent->left;
				}
				w->d.rb_color = x_parent->d.rb_color;
				x_parent->d.rb_color = BLACK;
				if (w->left) {
					w->left->d.rb_color = BLACK;
				}
				rb_rotate_right(x_parent, root);
				break;
			}
		}
	}
	if (x != NULL) {
		x->d.rb_color = BLACK;
	}
}
static void rb_delete(fnx_tlink_t *z, fnx_tlink_t **root)
{
	fnx_tlink_t *y, *x, *xp = NULL;

	y = z;
	if (y->left == NULL) {
		x = y->right;
	} else if (y->right == NULL) {
		x = y->left;
	} else {
		/* z has two non-null children. */
		y = y->right;

		while (y->left != NULL) {
			y = y->left;
		}
		x = y->right;
	}
	if (y != z) {
		/* relink y in place of z.  y is z's successor */
		z->left->parent = y;
		y->left = z->left;
		if (y != z->right) {
			xp = y->parent;
			if (x != NULL) {
				x->parent = y->parent;
			}
			y->parent->left = x;   /* y must be a child of left */
			y->right = z->right;
			z->right->parent = y;
		} else {
			xp = y;
		}
		if (*root == z) {
			*root = y;
		} else if (z->parent->left == z) {
			z->parent->left = y;
		} else {
			z->parent->right = y;
		}
		y->parent = z->parent;
		rb_swap_color(y, z);
		y = z;
		/* y now points to node to be actually deleted */
	} else {
		/* y == z */
		xp = y->parent;
		if (x != NULL) {
			x->parent = y->parent;
		}
		if (*root == z) {
			*root = x;
		} else {
			if (z->parent->left == z) {
				z->parent->left = x;
			} else {
				z->parent->right = x;
			}
		}
	}
	if (y->d.rb_color != RED) {
		rb_delete_fixup(x, xp, root);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Treap */
/*
 * Generate pseudo-random priority-value for x.
 * Don't care about thread-safety here -- its random...
 *
 * See also: www0.cs.ucl.ac.uk/staff/d.jones/GoodPracticeRNG.pdf
 * Does is it make random? Apparently good enough...
 */
static unsigned long marsaglia_prng(void)
{
	static uint64_t x = 123456789ULL;
	static uint64_t y = 362436000ULL;
	static uint64_t z = 521288629ULL;
	static uint64_t c = 7654321ULL;     /* Seed variables */

	uint64_t t, a = 698769069ULL;

	x = 69069 * x + 12345;
	y ^= (y << 13);
	y ^= (y >> 17);
	y ^= (y << 5); /* y must never be set to zero! */
	t = a * z + c;
	c = (t >> 32); /* Also avoid setting z=c=0! */
	return (unsigned long)(x + y + (z = t));
}

static unsigned long make_random(const fnx_tlink_t *x)
{
	unsigned long v, r;
	static unsigned long s = 1610612741UL;

#if (FNX_WORDSIZE == 64)
	v = (unsigned long)(((uint64_t)(x)) - 1);
#else
	v = (unsigned long)(((uint32_t)(x)) - 1);
#endif
	r = marsaglia_prng();
	v = v % (34211UL * 34213UL); /* Keep primes */

	s += 17;
	s ^= r;
	return (s ^ v);
}

static void treap_init_node(fnx_tlink_t *x)
{
	tlink_init(x, NULL);
	x->d.treap_priority = make_random(x);
}

static void treap_rotate_left(fnx_tlink_t *x, fnx_tlink_t **root)
{
	tlink_rotate_left(x, root);
}

static void treap_rotate_right(fnx_tlink_t *x, fnx_tlink_t **root)
{
	tlink_rotate_right(x, root);
}

static void treap_insert_fixup(fnx_tlink_t *x, fnx_tlink_t **root)
{
	/*
	 * Re-balance & preserve HEAP-order of TREAP after insertion of node x.
	 * Keeps tree in "Heap-order": for any node x with parent z the relation
	 * x.priority <= z.priority. Reestablishes heap-order by rotating x up as
	 * long as it has a parent with smaller priority.
	 */
	fnx_tlink_t *z;
	unsigned long x_priority, z_priority;

	x_priority = x->d.treap_priority;
	for (z = x->parent; z != NULL; z = x->parent) {
		z_priority = z->d.treap_priority;
		if (x_priority <= z_priority) {
			break;
		}
		if (z->left == x) {
			treap_rotate_right(z, root);
		} else {
			treap_rotate_left(z, root);
		}
	}
}

static void treap_clip_vine_node(fnx_tlink_t *x, fnx_tlink_t **root)
{
	fnx_tlink_t *y, *z;

	z = x->parent;
	y = x->left ? x->left : x->right;
	if (y != NULL) {
		y->parent = z;
	}
	if (z != NULL) {
		if (z->right == x) {
			z->right = y;
		} else {
			z->left = y;
		}
	}
	if (*root == x) {
		*root = y;
	}
}

static void treap_delete(fnx_tlink_t *x, fnx_tlink_t **root)
{
	/*
	 * "Inverting" the insertion operation: rotates x down until it becomes
	 * a leaf (or internal node with single child), where the decision to
	 * rotate left or right is dictated by the relative order of the
	 * priorites of the children of x, and finally clip away the leaf (or
	 * single-child-internal-node).
	 */
	while (x->left && x->right) {
		if (x->left->d.treap_priority > x->right->d.treap_priority) {
			treap_rotate_right(x, root);
		} else {
			treap_rotate_left(x, root);
		}
	}
	treap_clip_vine_node(x, root);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Common Tree Operations */

struct tree_position {
	fnx_tlink_t *parent;
	fnx_tlink_t **link;
};
typedef struct tree_position   tree_position_t;

static fnx_tlink_t **tree_root(const fnx_tree_t *tree)
{
	fnx_tree_t *p_tree = (fnx_tree_t *)tree;
	return &p_tree->tr_head.parent;
}

static fnx_tlink_t **tree_leftmost(const fnx_tree_t *tree)
{
	fnx_tree_t *p_tree = (fnx_tree_t *)tree;
	return &p_tree->tr_head.left; /* Minimal element */
}

static fnx_tlink_t **tree_rightmost(const fnx_tree_t *tree)
{
	fnx_tree_t *p_tree = (fnx_tree_t *)tree;
	return &p_tree->tr_head.right; /* Maximal element */
}

static fnx_tlink_t *tree_header(const fnx_tree_t *tree)
{
	fnx_tree_t *p_tree = (fnx_tree_t *)tree;
	return &p_tree->tr_head; /* Header-cell as pseudo-node */
}

static void
tree_init(fnx_tree_t *tree, fnx_treetype_e type, const fnx_treehooks_t *uops)
{
	tree->tr_nodecount = 0;
	tree->tr_type = type;
	tree->tr_uops = uops;
	tlink_reset(&tree->tr_head);
}

static void tree_init_node(const fnx_tree_t *tree, fnx_tlink_t *x)
{
	switch (tree->tr_type) {
		case FNX_TREE_AVL:
			avl_init_node(x);
			break;
		case FNX_TREE_RB:
			rb_init_node(x);
			break;
		case FNX_TREE_TREAP:
			treap_init_node(x);
			break;
		default:
			fnx_panic("t_type=%d", (int)tree->tr_type);
			break;
	}
}

static void tree_insert_fixup_node(fnx_tree_t *tree, fnx_tlink_t *x)
{
	fnx_tlink_t **root;

	root = tree_root(tree);
	switch (tree->tr_type) {
		case FNX_TREE_AVL:
			avl_insert_fixup(x, root);
			break;
		case FNX_TREE_RB:
			rb_insert_fixup(x, root);
			break;
		case FNX_TREE_TREAP:
			treap_insert_fixup(x, root);
			break;
		default:
			fnx_panic("t_type=%d", (int)tree->tr_type);
			break;
	}
}

static void tree_remove_node(fnx_tree_t *tree, fnx_tlink_t *x)
{
	fnx_tlink_t **root;

	root = tree_root(tree);
	switch (tree->tr_type) {
		case FNX_TREE_AVL:
			avl_delete(x, root);
			break;
		case FNX_TREE_RB:
			rb_delete(x, root);
			break;
		case FNX_TREE_TREAP:
			treap_delete(x, root);
			break;
		default:
			fnx_panic("t_type=%d", (int)tree->tr_type);
			break;
	}
}

static size_t tree_size(const fnx_tree_t *tree)
{
	return tree->tr_nodecount;
}

size_t fnx_tree_size(const fnx_tree_t *tree)
{
	return tree_size(tree);
}

int fnx_tree_isempty(const fnx_tree_t *tree)
{
	return (tree_size(tree) == 0);
}

static fnx_tlink_t *tree_end(const fnx_tree_t *tree)
{
	fnx_tree_t *p_tree = (fnx_tree_t *)tree;
	return &p_tree->tr_head;
}

fnx_tlink_t *fnx_tree_end(const fnx_tree_t *tree)
{
	return tree_end(tree);
}

int fnx_tree_iseoseq(const fnx_tree_t *tree, const fnx_tlink_t *itr)
{
	return ((itr == NULL) || (itr == tree_end(tree)));
}

static fnx_tlink_t *tree_begin(const fnx_tree_t *tree)
{
	fnx_tlink_t **leftmost;

	leftmost = tree_leftmost(tree);
	return *leftmost ? *leftmost : tree_end(tree);
}

fnx_tlink_t *fnx_tree_begin(const fnx_tree_t *tree)
{
	return tree_begin(tree);
}

static fnx_tlink_t *
tree_next(const fnx_tree_t *tree, const fnx_tlink_t *x)
{
	fnx_tlink_t const *h, *y;

	h = tree_header(tree);
	if (x != h) {
		y = tlink_successor(x);
		if (y == NULL) {
			y = h;
		}
	} else {
		y = h->left;
	}
	return (fnx_tlink_t *)y;
}

fnx_tlink_t *fnx_tree_next(const fnx_tree_t *tree, const fnx_tlink_t *x)
{
	return tree_next(tree, x);
}


fnx_tlink_t *fnx_tree_prev(const fnx_tree_t *tree, const fnx_tlink_t *x)
{
	const fnx_tlink_t *h;
	const fnx_tlink_t *y;

	h = tree_header(tree);
	if (x != h) {
		y = tlink_predecessor(x);
		if (y == NULL) {
			y = h;
		}
	} else {
		y = h->right;
	}
	return (fnx_tlink_t *)y;
}

static fnx_tlink_t *tree_insert_root(fnx_tree_t *tree, fnx_tlink_t *x)
{
	tree_init_node(tree, x);

	*tree_leftmost(tree)  = x;
	*tree_rightmost(tree) = x;
	*tree_root(tree)      = x;
	return x;
}

static const void *
tree_keyof(const fnx_tree_t *tree, const fnx_tlink_t *x)
{
	return tree->tr_uops->getkey_hook(x);
}

static int
tree_compare(const fnx_tree_t *t, const fnx_tlink_t *x, const fnx_tlink_t *y)
{
	return t->tr_uops->keycmp_hook(tree_keyof(t, x), tree_keyof(t, y));
}


static void tree_setpos(tree_position_t *pos, fnx_tlink_t *p, int isleft)
{
	if (p != NULL) {
		if (isleft) {
			pos->parent = p;
			pos->link   = &p->left;
		} else {
			pos->parent = p;
			pos->link   = &p->right;
		}
	} else {
		pos->parent = NULL;
		pos->link   = NULL;
	}
}

/* Search for insert position of new leaf (unique). */
static int
tree_find_insert_unique_pos(fnx_tree_t *tree,
                            const fnx_tlink_t *z,
                            tree_position_t *pos)
{
	int cmp, left_child;
	fnx_tlink_t *p, *x;

	left_child = 0;
	p = NULL;
	x = *tree_root(tree);
	while (x != NULL) {
		cmp = tree_compare(tree, x, z);
		if (cmp > 0) {
			p = x;
			x = x->right;
			left_child = 0;
		} else if (cmp < 0) {
			p = x;
			x = x->left;
			left_child = 1;
		} else {
			return -1; /* Not unique */
		}
	}

	tree_setpos(pos, p, left_child);
	return 0;
}

/* Search for insert position of new leaf (non-unique). */
static void
tree_find_insert_non_unique_pos(fnx_tree_t *tree,
                                const fnx_tlink_t *z,
                                tree_position_t *pos)
{
	int cmp, left_child;
	fnx_tlink_t *p, *x;

	left_child = 0;
	p = NULL;
	x = *tree_root(tree);
	while (x != NULL) {
		cmp = tree_compare(tree, x, z);
		if (cmp < 0) {
			p = x;
			x = x->left;
			left_child = 1;
		} else {
			p = x;
			x = x->right;
			left_child = 0;
		}
	}
	tree_setpos(pos, p, left_child);
}

static int
tree_find_insert_pos(fnx_tree_t *tree, const fnx_tlink_t *x,
                     int unique, tree_position_t *pos)
{
	int rc = 0;

	if (unique) {
		rc = tree_find_insert_unique_pos(tree, x, pos);
	} else {
		tree_find_insert_non_unique_pos(tree, x, pos);
	}
	return rc;
}

/* Insert leaf node. */
static fnx_tlink_t *tree_insert_leaf(fnx_tree_t *tree,
                                     fnx_tlink_t *x, int unique)
{
	fnx_tlink_t *t_min, *t_max, **link;
	tree_position_t pos;

	/* Where to insert new leaf */
	if (tree_find_insert_pos(tree, x, unique, &pos) != 0) {
		return NULL; /* No room for you (not unique). */
	}

	/* Link node to tree. */
	tree_init_node(tree, x);

	x->parent = pos.parent;
	*pos.link = x;

	/* Fixup tree to keep balanced (AVL, RB or TREAP). */
	tree_insert_fixup_node(tree, x);

	/* Update leftmost/rightmost pointers (if needed). */
	link  = tree_leftmost(tree);
	t_min = tlink_predecessor(*link);
	if (t_min != NULL) {
		*link = t_min;
	}

	link  = tree_rightmost(tree);
	t_max = tlink_successor(*link);
	if (t_max != NULL) {
		*link = t_max;
	}
	return x;
}

/* Inserts node */
static fnx_tlink_t *
tree_insert_node(fnx_tree_t *tree, fnx_tlink_t *z, int unique)
{
	fnx_tlink_t *x;

	if (tree_size(tree) > 0) {
		x = tree_insert_leaf(tree, z, unique);
	} else {
		x = tree_insert_root(tree, z);
	}
	if (x != NULL) {
		tree->tr_nodecount += 1;
	}
	return x;
}

void fnx_tree_insert(fnx_tree_t *tree, fnx_tlink_t *z)
{
	tree_insert_node(tree, z, 0);
}

int fnx_tree_insert_unique(fnx_tree_t *tree, fnx_tlink_t *z)
{
	const fnx_tlink_t *y;

	y = tree_insert_node(tree, z, 1);
	return (y != NULL) ? 0 : -1;
}

/* Erases the element pointed to by x. */
static void tree_remove(fnx_tree_t *tree, fnx_tlink_t *x)
{
	fnx_tlink_t **leftmost, **rightmost;

	leftmost  = tree_leftmost(tree);
	rightmost = tree_rightmost(tree);

	if (tree->tr_nodecount > 1) {
		if (*leftmost == x) {
			*leftmost = tlink_successor(x);
		}

		if (*rightmost == x) {
			*rightmost = tlink_predecessor(x);
		}
	}

	tree_remove_node(tree, x);
	tree->tr_nodecount -= 1;

	if (tree->tr_nodecount == 0) {
		tlink_reset(&tree->tr_head);
	}
}

void fnx_tree_remove(fnx_tree_t *tree, fnx_tlink_t *x)
{
	tree_remove(tree, x);
	tlink_reset(x);
}


static fnx_tlink_t *tree_unlinkall(fnx_tree_t *tree)
{
	fnx_tlink_t *x, *y;
	fnx_tlink_t *list = NULL;

	x = *tree_root(tree);
	while (x != NULL) {
		y = x->parent;

		if (x->left) {
			x = x->left;
		} else if (x->right) {
			x = x->right;
		} else { /* Leaf */
			if (y != NULL) {
				if (y->left == x) {
					y->left  = NULL;
				} else if (y->right == x) {
					y->right = NULL;
				}
			}

			/* Link removed node in list */
			tlink_reset(x);
			x->right = list;
			list = x;

			x = y;
		}
	}
	return list;
}

static void tree_reset(fnx_tree_t *tree)
{
	tree->tr_nodecount = 0;
	tlink_reset(tree_header(tree));
}

fnx_tlink_t *fnx_tree_reset(fnx_tree_t *tree)
{
	fnx_tlink_t *list;

	list = tree_unlinkall(tree);
	tree_reset(tree);
	return list;
}

void fnx_tree_clear(fnx_tree_t *tree, fnx_tnode_fn fn, void *ptr)
{
	fnx_tlink_t *x, *list;

	list = tree_unlinkall(tree);
	tree_reset(tree);

	while ((x = list) != NULL) {
		list = list->right;
		fn(x, ptr);
	}
}

void fnx_tree_remove_range(fnx_tree_t *tree,
                           fnx_tlink_t *first, fnx_tlink_t *last)
{
	fnx_tlink_t *itr, *next;

	if ((first == tree_begin(tree)) && (last == tree_end(tree))) {
		tree_reset(tree);
	} else {
		itr = first;
		while (itr != last) {
			next = tree_next(tree, first);

			tree_remove(tree, itr);
			tlink_reset(itr);
			itr = next;
		}
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Tree Search */

static int
tree_keycmp(const fnx_tree_t *t, const fnx_tlink_t *x, const void *k)
{
	return t->tr_uops->keycmp_hook(tree_keyof(t, x), k);
}

static void tree_range_init(const fnx_tlink_t *x,
                            const fnx_tlink_t *y,
                            fnx_tree_range_t *range)
{
	range->first  = (fnx_tlink_t *) x;
	range->second = (fnx_tlink_t *) y;
}

static size_t
tree_distance(const fnx_tree_t *tree, const fnx_tree_range_t *range)
{
	size_t cnt;
	const fnx_tlink_t *itr;

	cnt = 0;
	itr = range->first;
	while (itr != range->second) {
		++cnt;
		itr = tree_next(tree, itr);
	}
	return cnt;
}

/* Finds the first element whose value is greater than v */
static fnx_tlink_t *
tree_find_upper_bound(const fnx_tree_t *tree,
                      const fnx_tlink_t *x, const fnx_tlink_t *y, const void *k)
{
	int cmp;

	while (x != NULL) {
		cmp = tree_keycmp(tree, x, k);
		if (cmp < 0) {
			y = x;
			x = x->left;
		} else {
			x = x->right;
		}
	}
	return (fnx_tlink_t *) y;
}

/* Finds the first element whose value is not less than k */
static fnx_tlink_t *
tree_find_lower_bound(const fnx_tree_t *tree,
                      const fnx_tlink_t *x, const fnx_tlink_t *y, const void *k)
{
	int cmp;

	while (x != NULL) {
		cmp = tree_keycmp(tree, x, k);
		if (cmp <= 0) {
			y = x;
			x = x->left;
		} else {
			x = x->right;
		}
	}
	return (fnx_tlink_t *)y;
}

static void
tree_find_equal_range(const fnx_tree_t *tree,
                      const fnx_tlink_t *x, const fnx_tlink_t *y,
                      const void *k, fnx_tree_range_t *range)
{
	int cmp;
	fnx_tlink_t const *xu, *yu, *w, *z;

	w = z = y;
	while (x != NULL) {
		cmp = tree_keycmp(tree, x, k);
		if (cmp > 0) {
			x = x->right;
		} else if (cmp < 0) {
			y = x;
			x = x->left;
		} else {
			xu = x->right;
			yu = y;

			y = x;
			x = x->left;

			w = tree_find_lower_bound(tree, x, y, k);
			z = tree_find_upper_bound(tree, xu, yu, k);
			break;
		}
	}

	tree_range_init(w, z, range);
}

static fnx_tlink_t *tree_vfind_first(const fnx_tree_t *tree,
                                     const void *k)
{
	fnx_tlink_t *x = NULL;

	if (tree_size(tree) > 0) {
		x = tree_find_lower_bound(tree, *tree_root(tree), x, k);
	}
	return x;
}

static fnx_tlink_t *tree_vfind_top(const fnx_tree_t *tree,
                                   const void *k)
{
	int cmp;
	fnx_tlink_t *x;

	x = *tree_root(tree);
	while (x != NULL) {
		cmp = tree_keycmp(tree, x, k);
		if (cmp < 0) {
			x = x->left;
		} else if (cmp > 0) {
			x = x->right;
		} else {
			/* Bingo! We have a match
			    (possibly first out of some if multiset) */
			break;
		}
	}
	return x;
}

fnx_tlink_t *fnx_tree_find(const fnx_tree_t *tree, const void *k)
{
	fnx_tlink_t *x;

	x = tree_vfind_top(tree, k);
	return x;
}

fnx_tlink_t *fnx_tree_find_first(const fnx_tree_t *tree,
                                 const void *k)
{
	fnx_tlink_t *x;

	x = tree_vfind_first(tree, k);
	if ((x != NULL) && (tree_keycmp(tree, x, k) > 0)) {
		x = NULL;
	}

	return x;
}

size_t fnx_tree_count(const fnx_tree_t *tree, const void *k)
{
	size_t cnt;
	fnx_tree_range_t range = { NULL, NULL };

	cnt = 0;
	if (tree_size(tree) > 0) {
		fnx_tree_equal_range(tree, k, &range);
		if (range.first != NULL) {
			if (range.second == NULL) {
				range.second = tree_end(tree);
			}
			cnt = tree_distance(tree, &range);
		}
	}
	return cnt;
}

fnx_tlink_t *fnx_tree_lower_bound(const fnx_tree_t *tree,
                                  const void *k)
{
	const fnx_tlink_t *x = NULL;

	if (tree_size(tree) > 0) {
		x = tree_find_lower_bound(tree, *tree_root(tree), NULL, k);
	}
	return (fnx_tlink_t *)x;
}

fnx_tlink_t *fnx_tree_upper_bound(const fnx_tree_t *tree,
                                  const void *k)
{
	const fnx_tlink_t *x = NULL;

	if (tree_size(tree) > 0) {
		x = tree_find_upper_bound(tree, *tree_root(tree), x, k);
	}

	return (fnx_tlink_t *)x;
}

void fnx_tree_equal_range(const fnx_tree_t *tree,
                          const void *k, fnx_tree_range_t *range)
{
	tree_range_init(NULL, NULL, range);

	if (tree_size(tree) > 0) {
		tree_find_equal_range(tree, *tree_root(tree), NULL, k, range);
	}
}

static void tlink_replace(fnx_tlink_t *y, fnx_tlink_t *z, fnx_tlink_t **root)
{
	z->parent = y->parent;
	z->left = y->left;
	z->right = y->right;
	z->d.alignment = y->d.alignment;

	if (y->parent != NULL) {
		if (y->parent->left == y) {
			y->parent->left = z;
		} else {
			y->parent->right = z;
		}
	}
	if (y->left != NULL) {
		y->left->parent = z;
	}
	if (y->right != NULL) {
		y->right->parent = z;
	}

	if (*root == y) {
		*root = z;
	}
}

static void tree_replace(fnx_tree_t *tree, fnx_tlink_t *y, fnx_tlink_t *z)
{
	fnx_tlink_t **leftmost, **rightmost, **root;

	root = tree_root(tree);
	leftmost = tree_leftmost(tree);
	rightmost = tree_rightmost(tree);

	tlink_replace(y, z, root);
	if (*leftmost == y) {
		*leftmost = z;
	}
	if (*rightmost == y) {
		*rightmost = z;
	}
	tlink_destroy(y);
}

fnx_tlink_t *fnx_tree_insert_replace(fnx_tree_t *tree, fnx_tlink_t *z)
{
	const void *k;
	fnx_tlink_t *y;

	k = tree_keyof(tree, z);
	y = fnx_tree_find(tree, k);
	if (y == NULL) {
		fnx_tree_insert(tree, z);
	} else if (y != z) {
		tree_replace(tree, y, z);
	} else {
		y = NULL;
	}
	return y;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Init / Destroy */
void fnx_avl_init(fnx_tree_t *avl, const fnx_treehooks_t *uops)
{
	tree_init(avl, FNX_TREE_AVL, uops);
}

void fnx_rb_init(fnx_tree_t *rb, const fnx_treehooks_t *uops)
{
	tree_init(rb, FNX_TREE_RB, uops);
}

void fnx_treap_init(fnx_tree_t *treap, const fnx_treehooks_t *uops)
{
	tree_init(treap, FNX_TREE_TREAP, uops);
}

void fnx_tree_init(fnx_tree_t *tree,
                   fnx_treetype_e tree_type,
                   const fnx_treehooks_t *uops)
{
	switch (tree_type) {
		case FNX_TREE_AVL:
			fnx_avl_init(tree, uops);
			break;
		case FNX_TREE_RB:
			fnx_rb_init(tree, uops);
			break;
		case FNX_TREE_TREAP:
			fnx_treap_init(tree, uops);
			break;
		default:
			tree->tr_type = FNX_TREE_AVL;
			fnx_avl_init(tree, uops);
			break;
	}
}

void fnx_tree_destroy(fnx_tree_t *tree)
{
	tree_reset(tree);
	tlink_destroy(&tree->tr_head);
}

void fnx_tlink_init(fnx_tlink_t *x)
{
	tlink_init(x, NULL);
	x->d.treap_priority = 0;
}

void fnx_tlink_destroy(fnx_tlink_t *x)
{
	tlink_destroy(x);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_ucompare3(unsigned long x, unsigned long y)
{
	int res = 0;

	if (x < y) {
		res = -1;
	} else if (x > y) {
		res = 1;
	}
	return res;
}

int fnx_compare3(long x, long y)
{
	int res = 0;

	if (x < y) {
		res = -1;
	} else if (x > y) {
		res = 1;
	}
	return res;
}

