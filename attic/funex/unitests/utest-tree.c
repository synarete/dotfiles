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

#include <fnxinfra.h>
#include <fnxhash.h>

#define MAGIC1          (0x1ef459)
#define MAGIC2          (0x778edf)
#define NITER           1213


struct key_value_node {
	fnx_tlink_t link;
	int key;
	int magic1;
	char pad;
	int value;
	int magic2;
};
typedef struct key_value_node    key_value_node_t;

static int *s_rand_keys;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int value_for_key(int key)
{
	return key * key;
}

static key_value_node_t *tlnk_to_node(const fnx_tlink_t *tlnk)
{
	const key_value_node_t *node;

	node = fnx_container_of(tlnk, key_value_node_t, link);
	return (key_value_node_t *)node;
}

static const void *getkey(const fnx_tlink_t *x)
{
	const key_value_node_t *x_kv;

	x_kv   = tlnk_to_node(x);
	return &x_kv->key;
}

static int keycmp(const void *x_k, const void *y_k)
{
	const int *x_key;
	const int *y_key;

	x_key = (const int *)(x_k);
	y_key = (const int *)(y_k);

	return (*y_key) - (*x_key);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void delete_node(fnx_tlink_t *x, void *ptr)
{
	fnx_tlink_destroy(x);
	fnx_free(x, sizeof(key_value_node_t));
	fnx_unused(ptr);
}

static fnx_tlink_t *new_node(int key)
{
	size_t sz;
	key_value_node_t *p_kv;

	sz   = sizeof(key_value_node_t);
	p_kv = (key_value_node_t *)fnx_malloc(sz);
	fnx_assert(p_kv != NULL);

	fnx_bzero(p_kv, sizeof(*p_kv));

	p_kv->key     = key;
	p_kv->magic1  = MAGIC1;
	p_kv->magic2  = MAGIC2;
	p_kv->value   = value_for_key(key);

	fnx_tlink_init(&p_kv->link);

	return (fnx_tlink_t *)(p_kv);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void check_sequence(const fnx_tree_t *tree, size_t nelems)
{
	size_t sz;
	int i, n, k;
	const key_value_node_t *p_kv;
	const fnx_tlink_t *itr;
	const fnx_tlink_t *itr2;

	sz = fnx_tree_size(tree);
	fnx_assert(sz == nelems);

	i = 0;
	itr = fnx_tree_begin(tree);
	while (!fnx_tree_iseoseq(tree, itr)) {
		n = i + 1;

		itr2 = fnx_tree_find(tree, &n);
		fnx_assert(itr2 != fnx_tree_end(tree));

		p_kv = tlnk_to_node(itr2);
		fnx_assert(p_kv->magic1  == MAGIC1);
		fnx_assert(p_kv->magic2  == MAGIC2);
		fnx_assert(p_kv->key     == n);
		fnx_assert(p_kv->value   == value_for_key(n));

		k = (int)fnx_tree_count(tree, &n);
		fnx_assert(k == 1);

		++i;
		itr = fnx_tree_next(tree, itr);
	}
	fnx_assert(i == (int)nelems);
}


static const fnx_treehooks_t s_tree_hooks = {
	.getkey_hook    = getkey,
	.keycmp_hook    = keycmp
};

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_tree_basic(fnx_tree_t *tree)
{
	int rc, key;
	size_t i, niter;
	const fnx_tlink_t *itr;
	const fnx_tlink_t *end;
	fnx_tlink_t *node = NULL;

	niter = NITER;
	for (i = 0; i < niter; ++i) {
		key = (int)(i + 1);

		node = new_node(key);
		rc   = fnx_tree_insert_unique(tree, node);
		fnx_assert(rc == 0);

		check_sequence(tree, i + 1);
	}

	i = 0;
	itr = fnx_tree_begin(tree);
	end = fnx_tree_end(tree);
	while (itr != end) {
		node = (fnx_tlink_t *)itr;
		fnx_tree_remove(tree, node);

		delete_node(node, NULL);

		itr = fnx_tree_begin(tree);
		end = fnx_tree_end(tree);
		++i;
	}
	fnx_assert(fnx_tree_isempty(tree));
	fnx_assert(i == niter);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_tree_insert(fnx_tree_t *tree)
{
	int rc, key, cond;
	size_t i, niter;
	const fnx_tlink_t *itr;
	const fnx_tlink_t *end;
	fnx_tlink_t *node;

	niter = NITER;
	for (i = 0; i < niter; ++i) {
		key = (int)(i + 1);

		node = new_node(key);
		itr  = node;
		rc = fnx_tree_insert_unique(tree, node);
		fnx_assert(rc == 0);

		end  = fnx_tree_end(tree);
		fnx_assert(itr != end);
		fnx_assert(itr == node);

		cond = fnx_tree_iseoseq(tree, itr);
		fnx_assert(!cond);
		cond = fnx_tree_iseoseq(tree, end);
		fnx_assert(cond);

		check_sequence(tree, i + 1);

		rc = fnx_tree_insert_unique(tree, node);
		fnx_assert(rc == -1);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_tree_insert_replace(fnx_tree_t *tree)
{
	int rc, key, cond;
	size_t i, niter;
	const fnx_tlink_t *itr;
	const fnx_tlink_t *end;
	fnx_tlink_t *node, *node2, *node3;

	niter = NITER;
	for (i = 0; i < niter; ++i) {
		key = (int)(i + 1);

		node = new_node(key);
		itr  = node;
		rc = fnx_tree_insert_unique(tree, node);
		fnx_assert(rc == 0);

		end  = fnx_tree_end(tree);
		fnx_assert(itr != end);
		fnx_assert(itr == node);

		cond = fnx_tree_iseoseq(tree, itr);
		fnx_assert(!cond);
		cond = fnx_tree_iseoseq(tree, end);
		fnx_assert(cond);

		check_sequence(tree, i + 1);

		node2 = new_node(key);
		node3 = fnx_tree_insert_replace(tree, node2);
		fnx_assert(node3 == node);
		delete_node(node3, NULL);

		/* re-insert same node, already linked into tree! */
		node3 = fnx_tree_insert_replace(tree, node2);
		fnx_assert(node3 == NULL);
	}

	fnx_tree_clear(tree, delete_node, NULL);
	node = new_node(1);
	node2 = fnx_tree_insert_replace(tree, node);
	fnx_assert(node2 == NULL);
	fnx_tree_clear(tree, delete_node, NULL);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_tree_reset(fnx_tree_t *tree)
{
	int rc, key;
	size_t i, niter, size;
	fnx_tlink_t *list;
	fnx_tlink_t *node;

	niter = NITER;
	for (i = 0; i < niter; ++i) {
		key = (int)(i + 1);
		node = new_node(key);
		rc = fnx_tree_insert_unique(tree, node);
		fnx_assert(rc == 0);
	}

	list = fnx_tree_reset(tree);
	size = fnx_tree_size(tree);
	fnx_assert(list != NULL);
	fnx_assert(size == 0);

	i = 0;
	while ((node = list) != NULL) {
		list = list->right;
		delete_node(node, NULL);
		++i;
	}
	fnx_assert(i == niter);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_tree_insert_rand(fnx_tree_t *tree)
{
	int key;
	size_t i, niter;
	const fnx_tlink_t *itr;
	const key_value_node_t *p_kv;
	fnx_tlink_t *node;

	niter = NITER;
	for (i = 0; i < niter; ++i) {
		key = s_rand_keys[i];

		node = new_node(key);
		itr  = node;
		fnx_tree_insert(tree, node);
	}

	for (i = 0; i < niter; ++i) {
		key = (int)(i + 1);
		itr = fnx_tree_find(tree, &key);
		fnx_assert(itr != NULL);
		p_kv = tlnk_to_node(itr);
		fnx_assert(p_kv->key == key);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

typedef void (*tree_test_fn)(fnx_tree_t *);

static void
test_tree(fnx_treetype_e tree_type, tree_test_fn test_fn)
{
	fnx_tree_t *tree;

	tree = (fnx_tree_t *)fnx_malloc(sizeof(*tree));
	fnx_tree_init(tree, tree_type, &s_tree_hooks);
	test_fn(tree);
	fnx_tree_clear(tree, delete_node, NULL);
	fnx_tree_destroy(tree);
	fnx_free(tree, sizeof(*tree));
}

static void fnx_test_trees(void)
{
	size_t i, j;
	fnx_treetype_e tree_type;
	tree_test_fn test_fn;
	fnx_treetype_e types[] = {
		FNX_TREE_AVL,
		FNX_TREE_RB,
		FNX_TREE_TREAP
	};
	tree_test_fn tests[] = {
		test_tree_basic,
		test_tree_insert,
		test_tree_insert_replace,
		test_tree_reset,
		test_tree_insert_rand
	};

	for (i = 0; i < FNX_ARRAYSIZE(types); ++i) {
		tree_type = types[i];
		for (j = 0; j < FNX_ARRAYSIZE(tests); ++j) {
			test_fn = tests[j];
			test_tree(tree_type, test_fn);
		}
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void set_random_keys(void)
{
	int *keys_arr;
	size_t nelems, sz;

	nelems = NITER;
	sz = sizeof(int) * nelems;
	keys_arr = (int *)fnx_xmalloc(sz, FNX_NOFAIL);

	blgn_prandom_tseq(keys_arr, nelems, 1);
	s_rand_keys = keys_arr;
}

static void free_random_keys(void)
{
	size_t nelems, sz;

	nelems = NITER;
	sz = sizeof(int) * nelems;
	fnx_free(s_rand_keys, sz);
}

int main(void)
{
	set_random_keys();
	fnx_test_trees();
	free_random_keys();

	return EXIT_SUCCESS;
}


