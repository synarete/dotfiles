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
#include <search.h>

#include <fnxinfra.h>
#include <fnxhash.h>


#define MAGIC       (0x6A1C13B)
#define MAX_SIZE    503


struct keyval {
	fnx_tlink_t link;
	int magic;
	int key;
};
typedef struct keyval   keyval_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static keyval_t *tlnk_to_keyval(const fnx_tlink_t *tlnk)
{
	const keyval_t *kv;

	kv = fnx_container_of(tlnk, keyval_t, link);
	return (keyval_t *)kv;
}

static const void *keyval_getkey(const fnx_tlink_t *x)
{
	const keyval_t *kv;

	kv = tlnk_to_keyval(x);
	return &kv->key;
}

static int keyval_keycmp(const void *x_k, const void *y_k)
{
	const int *x_key;
	const int *y_key;

	x_key = (const int *)(x_k);
	y_key = (const int *)(y_k);

	return (*y_key) - (*x_key);
}

static void keyval_check(const keyval_t *kv)
{
	fnx_assert(kv->magic == MAGIC);
}

static void keyval_init(keyval_t *kv, int key)
{
	fnx_tlink_init(&kv->link);
	kv->magic   = MAGIC;
	kv->key     = key;
}

static void keyval_destroy(keyval_t *kv)
{
	keyval_check(kv);
	kv->magic = kv->key = 0;
}

static keyval_t *keyval_new(int key)
{
	int flags = FNX_BZERO | FNX_NOFAIL;
	keyval_t *kv;

	kv = fnx_xmalloc(sizeof(*kv), flags);
	keyval_init(kv, key);

	return kv;
}

static void keyval_delete(keyval_t *kv)
{
	keyval_destroy(kv);
	fnx_free(kv, sizeof(*kv));
}

static const fnx_treehooks_t s_tree_hooks = {
	.getkey_hook    = keyval_getkey,
	.keycmp_hook    = keyval_keycmp
};

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int tsearch_compare(const void *x, const void *y)
{
	int k_x, k_y;

	k_x = *(const int *)x;
	k_y = *(const int *)y;

	return (k_y - k_x);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
check_equal_trees(fnx_tree_t *tree, void **rootp, int *keys, size_t nkeys)
{
	size_t i;
	int key;
	fnx_tlink_t *itr = NULL;
	keyval_t *kv     = NULL;
	void *p;
	const int **z;

	for (i = 0; i < nkeys; ++i) {
		key = keys[i];
		itr = fnx_tree_find(tree, &key);
		fnx_assert(itr != NULL);
		kv  = tlnk_to_keyval(itr);
		keyval_check(kv);

		p = tfind(&key, rootp, tsearch_compare);
		z = (const int **)p;
		fnx_assert(p != NULL);
		fnx_assert(**z == kv->key);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
utest_tree_insert_remove(fnx_tree_t *tree, void **rootp, int *keys,
                         size_t nkeys)
{
	int rc, key;
	size_t i;
	fnx_tlink_t *itr = NULL;
	keyval_t *kv     = NULL;
	void *p;
	int **z;

	for (i = 0; i < nkeys; ++i) {
		key = keys[i];
		kv  = keyval_new(key);

		itr = &kv->link;
		rc  = fnx_tree_insert_unique(tree, itr);
		fnx_assert(rc == 0);

		p   = tsearch(&keys[i], rootp, tsearch_compare);
		fnx_assert(p != NULL);
		z   = (int **)p;
		fnx_assert(**z == key);

		check_equal_trees(tree, rootp, keys, i + 1);
	}

	for (i = 0; i < nkeys; ++i) {
		key = keys[i];
		itr = fnx_tree_find(tree, &key);
		kv  = tlnk_to_keyval(itr);
		keyval_check(kv);

		fnx_tree_remove(tree, itr);
		keyval_delete(kv);

		p = tdelete(&keys[i], rootp, tsearch_compare);
		fnx_assert(p != NULL);

		check_equal_trees(tree, rootp, keys + i + 1, nkeys - i - 1);
	}
}

static void utest_trees(size_t nkeys)
{
	size_t i, nkeys_sz;
	fnx_treetype_e types[] = { FNX_TREE_AVL, FNX_TREE_RB, FNX_TREE_TREAP };
	fnx_treetype_e tree_type;
	fnx_tree_t tree;
	void *rootp = NULL;
	int *keys;

	nkeys_sz = sizeof(int) * nkeys;
	keys = fnx_xmalloc(nkeys_sz, FNX_NOFAIL);
	for (i = 0; i < FNX_ARRAYSIZE(types); ++i) {
		blgn_prandom_tseq(keys, nkeys, 1);

		tree_type = types[i];
		fnx_tree_init(&tree, tree_type, &s_tree_hooks);
		utest_tree_insert_remove(&tree, &rootp, keys, nkeys);
		fnx_tree_destroy(&tree);
		rootp = NULL;
	}
	fnx_free(keys, nkeys_sz);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
int main(void)
{
	utest_trees(MAX_SIZE);

	return EXIT_SUCCESS;
}


