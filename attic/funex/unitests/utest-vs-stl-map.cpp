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
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>

#include <utility>
#include <algorithm>
#include <map>
#include <set>
#include <list>
#include <vector>
#include <pthread.h>

#include <fnxinfra.h>


// Defs:
#define MAGIC           (0xDA337FC)
#define COUNT           3011 /* 301111 */


typedef std::map<int, int>      Map;

struct map_node {
	fnx_tlink_t link;
	int key;
	int value;
	int magic;
};
typedef struct map_node     map_node_t;

static size_t s_count_max = COUNT;
static int    s_full_test = 0;



/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static const void *getkey(const fnx_tlink_t *x)
{
	const map_node_t *x_kv = reinterpret_cast<const map_node_t *>(x);
	return &x_kv->key;
}

static int keycmp(const void *x_k, const void *y_k)
{
	const int *x_key = static_cast<const int *>(x_k);
	const int *y_key = static_cast<const int *>(y_k);

	return ((*y_key) - (*x_key));
}

static int mkvalue(int key)
{
	return abs((key * (key + 19)) ^ MAGIC);
}

static void check_node(const map_node_t *p_node)
{
	fnx_assert(p_node != 0);
	fnx_assert(p_node->magic == MAGIC);
}

static map_node_t *node_from_tlink(const fnx_tlink_t *x)
{
	return fnx_container_of(x, map_node_t, link);
}

static fnx_tlink_t *new_node(int key, int value)
{
	map_node_t *p_kv = new map_node_t;
	fnx_bzero(p_kv, sizeof(*p_kv));

	p_kv->key       = key;
	p_kv->value     = value;
	p_kv->magic     = MAGIC;

	return &(p_kv->link);
}

static void delete_node(fnx_tlink_t *x, void *ptr)
{
	map_node_t *p_kv = node_from_tlink(x);
	check_node(p_kv);
	p_kv->magic = 1;
	delete p_kv;
	fnx_unused(ptr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
template <typename Iterator>
void check_equal(const Iterator &stl_itr, const fnx_tlink_t *fnx_itr)
{
	const map_node_t *p_node;

	p_node = fnx_container_of(fnx_itr, map_node_t, link);
	check_node(p_node);

	const int key1 = stl_itr->first;
	const int key2 = p_node->key;
	fnx_assert(key1 == key2);

	const int value1 = stl_itr->second;
	const int value2 = p_node->value;
	fnx_assert(value1 == value2);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

template <typename MapT>
void check_equal(const MapT &stl_map,
                 const fnx_tree_t *fnx_map, size_t cnt = (2 * COUNT))
{
	typedef typename MapT::const_iterator   const_iterator;

	int key;
	size_t n, stl_map_sz, fnx_map_sz;

	n = cnt;
	stl_map_sz = stl_map.size();
	fnx_map_sz = fnx_tree_size(fnx_map);
	fnx_assert(stl_map_sz == fnx_map_sz);

	const_iterator stl_iterator = stl_map.begin();
	const fnx_tlink_t *fnx_iterator = fnx_tree_begin(fnx_map);
	while ((stl_iterator != stl_map.end()) && (n-- > 0)) {
		check_equal(stl_iterator, fnx_iterator);

		key = stl_iterator->first;

		const_iterator stl_iterator2 = stl_map.find(key);
		const fnx_tlink_t *fnx_iterator2  = fnx_tree_find(fnx_map, &key);
		check_equal(stl_iterator2, fnx_iterator2);

		++stl_iterator;
		fnx_iterator = fnx_tree_next(fnx_map, fnx_iterator);
	}

	if (cnt >= fnx_map_sz) {
		const fnx_tlink_t *fnx_iterator_end = fnx_tree_end(fnx_map);
		fnx_assert(fnx_iterator == fnx_iterator_end);
	}
}

template <typename MapT>
void check_equal_light(const MapT &stl_map, const fnx_tree_t *fnx_map)
{
	size_t sz;

	sz = stl_map.size();
	if (sz > 0) {
		sz = std::min(sz, size_t(17));
		check_equal(stl_map, fnx_map, sz);
	}
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static int select_existing_elem(const Map &stl_map, int *p_key)
{
	typedef Map::const_iterator stl_iterator;

	size_t sz, n;
	stl_iterator stl_itr;

	sz = stl_map.size();
	if (sz == 0) {
		return -1;
	}

	stl_itr = stl_map.begin();
	n = sz % 17;
	while (n-- > 0) {
		++stl_itr;
	}
	*p_key = stl_itr->second;
	return 0;

}

static int select_random_key(struct random_data *rnd)
{
	int rc, r;

	rc  = random_r(rnd, &r);
	fnx_assert(rc == 0);

	return r;
}

static void
test_map_oparations(Map &stl_map, fnx_tree_t *fnx_map, struct random_data *rnd)
{
	typedef Map::iterator   stl_iterator;
	typedef fnx_tlink_t    *fnx_iterator;

	int rc, key, val;
	stl_iterator stl_itr;
	fnx_iterator fnx_itr;
	fnx_tlink_t *tlnk;

	for (size_t i = 0; i < s_count_max; ++i) {
		rc = -1;
		if (i % 13) {
			rc = select_existing_elem(stl_map, &key);
		}
		if (rc != 0) {
			key = select_random_key(rnd);
		}

		val = mkvalue(key);

		stl_itr = stl_map.find(key);
		fnx_itr = fnx_tree_find(fnx_map, &key);

		if (stl_itr == stl_map.end()) {
			fnx_assert(fnx_itr == NULL);
			stl_map.insert(std::make_pair(key, val));
			fnx_tree_insert(fnx_map, new_node(key, val));
			tlnk = fnx_tree_insert_replace(fnx_map, new_node(key, val));
			delete_node(tlnk, NULL);
		} else if (key & 0x01) {
			stl_map.erase(stl_itr);
			fnx_tree_remove(fnx_map, fnx_itr);
			delete_node(fnx_itr, NULL);
		}

		if (s_full_test) {
			check_equal(stl_map, fnx_map);
		} else {
			check_equal_light(stl_map, fnx_map);
		}
	}
	check_equal(stl_map, fnx_map);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_vs_stl_map(fnx_treetype_e tree_type)
{
	int rc;
	size_t i, n;
	unsigned int seed;
	char statebuf[17];
	struct random_data rnd;
	Map stl_map;
	fnx_tree_t fnx_map;
	fnx_treehooks_t ops;

	fnx_bzero(statebuf, sizeof(statebuf));
	fnx_bzero(&rnd, sizeof(rnd));
	fnx_bzero(&fnx_map, sizeof(fnx_map));

	seed = static_cast<unsigned>(time(0));
	n    = FNX_NELEMS(statebuf);
	for (i = 0; i < n; ++i) {
		statebuf[i] = static_cast<char>(seed + i);
	}
	rc   = initstate_r(seed, statebuf, sizeof(statebuf), &rnd);
	fnx_assert(rc == 0);

	ops.getkey_hook = getkey;
	ops.keycmp_hook = keycmp;
	fnx_tree_init(&fnx_map, tree_type, &ops);
	test_map_oparations(stl_map, &fnx_map, &rnd);
	fnx_tree_clear(&fnx_map, delete_node, NULL);
	fnx_tree_destroy(&fnx_map);
}


static void *test_avl_map(void *p)
{
	test_vs_stl_map(FNX_TREE_AVL);
	return p;
}

static void *test_rb_map(void *p)
{
	test_vs_stl_map(FNX_TREE_RB);
	return p;
}

static void *test_treap_map(void *p)
{
	test_vs_stl_map(FNX_TREE_TREAP);
	return p;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int main(int argc, char *argv[])
{
	int rc;
	pthread_t th_avl, th_rb, th_treap;

	if ((argc > 0) && (argv[1])) {
		s_full_test = 1;
	}

	rc = pthread_create(&th_avl, NULL, test_avl_map, NULL);
	fnx_assert(rc == 0);

	rc = pthread_create(&th_rb, NULL, test_rb_map, NULL);
	fnx_assert(rc == 0);

	rc = pthread_create(&th_treap, NULL, test_treap_map, NULL);
	fnx_assert(rc == 0);

	rc = pthread_join(th_avl, NULL);
	fnx_assert(rc == 0);

	rc = pthread_join(th_rb, NULL);
	fnx_assert(rc == 0);

	rc = pthread_join(th_treap, NULL);
	fnx_assert(rc == 0);

	return 0;
}
