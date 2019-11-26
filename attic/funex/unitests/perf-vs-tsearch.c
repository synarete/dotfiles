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
#define _GNU_SOURCE 1
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <search.h>
#include <pthread.h>

#include <fnxinfra.h>
#include <fnxhash.h>


#define MAGIC    (0x2334AAB9)
#define ASSERT(x)   /*fnx_assert(x)*/


static size_t s_nelems = 100;
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;


struct keyval {
	int magic;
	fnx_tlink_t tlnk;
	int key, val;
};
typedef struct keyval   keyval_t;

static keyval_t *keyval_new(int k)
{
	keyval_t *kv = NULL;

	kv = (keyval_t *)fnx_xmalloc(sizeof(*kv), FNX_NOFAIL);
	kv->key = k;
	kv->val = k;
	kv->magic = MAGIC;
	return kv;
}

static void keyval_del(keyval_t *kv)
{
	ASSERT(kv->magic == MAGIC);
	kv->magic = kv->key = 0;
	fnx_xfree(kv, sizeof(*kv), 0);
}

static void keyvals_generate(keyval_t *kvs[], size_t nelems)
{
	size_t i, keys_sz;
	int *keys;

	keys_sz = nelems * sizeof(int);
	keys    = fnx_xmalloc(keys_sz, FNX_NOFAIL);

	pthread_mutex_lock(&s_mutex);
	blgn_prandom_tseq(keys, nelems, 1);
	pthread_mutex_unlock(&s_mutex);

	for (i = 0; i < nelems; ++i) {
		kvs[i] = keyval_new(keys[i]);
	}
	fnx_xfree(keys, keys_sz, 0);
}

static keyval_t **keyvals_new(size_t nelems)
{
	size_t sz;
	keyval_t **kvs;

	sz  = sizeof(keyval_t *) * nelems;
	kvs = (keyval_t **)fnx_xmalloc(sz, FNX_NOFAIL);
	keyvals_generate(kvs, nelems);

	return kvs;
}

static void keyvals_delarr(keyval_t *kvs[], size_t nelems)
{
	size_t sz;

	sz  = sizeof(keyval_t *) * nelems;
	fnx_xfree(kvs, sz, 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static keyval_t *keyval_from_tlnk(const fnx_tlink_t *tlnk)
{
	const keyval_t *kv;

	kv = fnx_container_of(tlnk, keyval_t, tlnk);
	return (keyval_t *)kv;
}

static fnx_tlink_t *keyval_to_tlnk(const keyval_t *kv)
{
	return (fnx_tlink_t *)(&kv->tlnk);
}

static const void *keyval_getkey(const fnx_tlink_t *tlnk)
{
	const keyval_t *kv;

	kv = keyval_from_tlnk(tlnk);
	return &(kv->key);
}

static int keyval_keycmp(const void *x, const void *y)
{
	int u, v;

	u = *(const int *)x;
	v = *(const int *)y;
	return (u - v);
}

static const fnx_treehooks_t s_keyval_treehooks = {
	.getkey_hook = keyval_getkey,
	.keycmp_hook = keyval_keycmp
};

static void keyval_tlink_del(fnx_tlink_t *tlnk, void *p)
{
	keyval_t *kv;

	kv = keyval_from_tlnk(tlnk);
	keyval_del(kv);
	fnx_unused(p);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
fnx_tree_insert_find_remove(keyval_t *kvs[], size_t nelems, fnx_treetype_e tt)
{
	int key, rc;
	size_t i;
	keyval_t *kv, *kv2;
	fnx_tlink_t *itr;
	fnx_tree_t tree_obj;
	fnx_tree_t *tree = &tree_obj;

	fnx_tree_init(tree, tt, &s_keyval_treehooks);
	for (i = 0; i < nelems; ++i) {
		kv  = kvs[i];
		itr = keyval_to_tlnk(kv);
		rc = fnx_tree_insert_unique(tree, itr);
		ASSERT(rc == 0);
	}
	for (i = 0; i < nelems; ++i) {
		kv  = kvs[i];
		key = kv->key;
		itr = fnx_tree_find(tree, &key);
		kv2 = keyval_from_tlnk(itr);
		ASSERT(kv2 == kv);
	}
	fnx_tree_clear(tree, keyval_tlink_del, NULL);
	fnx_tree_destroy(tree);

	fnx_unused(kv2);
	fnx_unused(rc);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int tsearch_compr(const void *x, const void *y)
{
	return keyval_keycmp(x, y);
}

static keyval_t *tsearch_get_keyval(const void *x)
{
	const keyval_t *kv;
	const int *p;

	p = (const int *)x;
	kv = fnx_container_of(p, keyval_t, key);

	return (keyval_t *)kv;
}

static keyval_t *tsearch_get_keyvalp(const void *x)
{
	const int **p;

	p = (const int **)x;
	return tsearch_get_keyval(*p);
}


static void tsearch_free_node(void *x)
{
	keyval_t *kv;

	kv = tsearch_get_keyval(x);
	keyval_del(kv);
}

static void
libc_tsearch_insert_find_remove(keyval_t *kvs[], size_t nelems)
{
	int key;
	size_t i;
	keyval_t *kv, *kv2;
	void *root, *p;

	root = NULL;
	for (i = 0; i < nelems; ++i) {
		kv = kvs[i];
		p  = tsearch(&kv->key, &root, tsearch_compr);
		ASSERT(p != NULL);
		ASSERT(tsearch_get_keyvalp(p) == kv);
	}

	for (i = 0; i < nelems; ++i) {
		kv  = kvs[i];
		key = kv->key;
		p   = tfind(&key, &root, tsearch_compr);
		ASSERT(p != NULL);
		kv2 = tsearch_get_keyvalp(p);
		ASSERT(kv2 == kv);
	}

	tdestroy(root, tsearch_free_node);

	fnx_unused(p);
	fnx_unused(kv2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const char *treetype_str(fnx_treetype_e tt)
{
	const char *s;

	switch (tt) {
		case FNX_TREE_AVL:
			s = "avl";
			break;
		case FNX_TREE_RB:
			s = "rb";
			break;
		case FNX_TREE_TREAP:
			s = "treap";
			break;
		default:
			s = "xxx";
			break;
	}
	return s;
}

static void
check_perf_vs_tsearch(size_t nelems, fnx_treetype_e tt)
{
	FILE *fp = stdout;
	keyval_t **kvs;
	fnx_timespec_t ts_start, ts_end;
	int64_t fnx_usec, libc_usec, diff;
	float fnx_sec, libc_sec, ratio;

	fnx_usec = libc_usec = 0;

	kvs = keyvals_new(nelems);
	fnx_timespec_gettimeofday(&ts_start);
	fnx_tree_insert_find_remove(kvs, nelems, tt);
	fnx_timespec_gettimeofday(&ts_end);
	keyvals_delarr(kvs, nelems);
	diff = fnx_timespec_usecdiff(&ts_start, &ts_end);
	ASSERT(diff >= 0);
	fnx_usec += diff;

	kvs = keyvals_new(nelems);
	fnx_timespec_gettimeofday(&ts_start);
	libc_tsearch_insert_find_remove(kvs, nelems);
	fnx_timespec_gettimeofday(&ts_end);
	keyvals_delarr(kvs, nelems);
	diff = fnx_timespec_usecdiff(&ts_start, &ts_end);
	ASSERT(diff >= 0);
	libc_usec += diff;


	fnx_sec = (float)fnx_usec / 1000000.0f;
	libc_sec  = (float)libc_usec / 1000000.0f;
	ratio     = fnx_sec / libc_sec;

	flockfile(fp);
	fprintf(fp, "libc-tsearch: %0.2f fnx-%s: %0.2f ratio=%0.2f\n",
	        libc_sec, treetype_str(tt), fnx_sec, ratio);
	funlockfile(fp);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void *start_check_perf(void *p)
{
	fnx_treetype_e tt = *(fnx_treetype_e *)p;
	check_perf_vs_tsearch(s_nelems, tt);
	return NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int main(int argc, char *argv[])
{
	int rc;
	pthread_t tr_avl, tr_rb, tr_treap;
	fnx_treetype_e tt_avl, tt_rb, tt_treap;
	void *retval = NULL;

	if (argc > 1) {
		sscanf(argv[1], "%zu", &s_nelems);
	}

	tt_treap = FNX_TREE_TREAP;
	rc = pthread_create(&tr_treap, NULL, start_check_perf, &tt_treap);
	fnx_assert(rc == 0);

	tt_avl = FNX_TREE_AVL;
	rc = pthread_create(&tr_avl, NULL, start_check_perf, &tt_avl);
	fnx_assert(rc == 0);

	tt_rb = FNX_TREE_RB;
	rc = pthread_create(&tr_rb, NULL, start_check_perf, &tt_rb);
	fnx_assert(rc == 0);

	rc = pthread_join(tr_avl, &retval);
	fnx_assert(rc == 0);
	rc = pthread_join(tr_rb, &retval);
	fnx_assert(rc == 0);
	rc = pthread_join(tr_treap, &retval);
	fnx_assert(rc == 0);

	return EXIT_SUCCESS;
}


