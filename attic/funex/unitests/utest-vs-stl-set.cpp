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

#include <fnxinfra.h>

// Defs:
#define MAGIC               (0x2010ad)
#define MAX_SIZE            503 /* Default max-size */


typedef std::vector<int>        Vector;
typedef std::set<int>           Set;
typedef std::map<int, int>      Map;
typedef std::multimap<int, int> MultiMap;

struct dict_node {
	fnx_tlink_t link;
	int key;
	int value;
	int magic;
};
typedef struct dict_node    dict_node_t;


// Local vars:
static int s_max_size = MAX_SIZE;



/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void make_random_range(Vector &vec, int begin, int end)
{
	for (int i = begin; i < end; ++i) {
		vec.push_back(i);
	}
	std::random_shuffle(vec.begin(), vec.end());
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static const void *getkey(const fnx_tlink_t *x)
{
	const dict_node_t *x_kv = reinterpret_cast<const dict_node_t *>(x);
	return &x_kv->key;
}

static int keycmp(const void *x_k, const void *y_k)
{
	const int *x_key = reinterpret_cast<const int *>(x_k);
	const int *y_key = reinterpret_cast<const int *>(y_k);

	return ((*y_key) - (*x_key));
}

static int value_for_key(int key)
{
	return abs((key * (key + 17)) ^ MAGIC);
}

static fnx_tlink_t *new_node(int key, int value)
{
	dict_node_t *p_kv = new dict_node_t;
	memset(p_kv, 0, sizeof(*p_kv));

	p_kv->key       = key;
	p_kv->value     = value;
	p_kv->magic     = MAGIC;

	return reinterpret_cast<fnx_tlink_t *>(p_kv);
}

static void delete_node(fnx_tlink_t *x, void *ptr)
{
	fnx_assert(x != 0);
	delete x;
	fnx_unused(ptr);
}

static void check_node(const void *p_node)
{
	const dict_node_t *p_dnode = static_cast<const dict_node_t *>(p_node);
	const int key   = p_dnode->key;
	const int value = p_dnode->value;

	fnx_assert(p_dnode != 0);
	fnx_assert(key >= 0);
	fnx_assert(value == value_for_key(key));
	fnx_assert(p_dnode->magic == MAGIC);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
template <typename Iterator>
void check_equal(const Iterator &stl_iterator,
                 const fnx_tlink_t *fnx_iterator)
{
	const dict_node_t *p_node;

	p_node = reinterpret_cast<const dict_node_t *>(fnx_iterator);
	check_node(p_node);

	const int key1 = stl_iterator->first;
	const int key2 = p_node->key;
	fnx_assert(key1 == key2);

	const int value1 = stl_iterator->second;
	const int value2 = p_node->value;
	fnx_assert(value1 == value2);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
template <typename MapT>
void check_equal(const MapT &stl_map, const fnx_tree_t *fnx_map)
{
	typedef typename MapT::const_iterator   const_iterator;

	int key;
	size_t stl_map_sz, fnx_map_sz;

	stl_map_sz = stl_map.size();
	fnx_map_sz = fnx_tree_size(fnx_map);
	fnx_assert(stl_map_sz == fnx_map_sz);

	const_iterator stl_iterator = stl_map.begin();
	const fnx_tlink_t *fnx_iterator = fnx_tree_begin(fnx_map);
	while (stl_iterator != stl_map.end()) {
		check_equal(stl_iterator, fnx_iterator);

		key = stl_iterator->first;
		const_iterator stl_iterator2 = stl_map.find(key);
		const fnx_tlink_t *fnx_iterator2 = fnx_tree_find(fnx_map, &key);
		check_equal(stl_iterator2, fnx_iterator2);

		++stl_iterator;
		fnx_iterator = fnx_tree_next(fnx_map, fnx_iterator);
	}

	const fnx_tlink_t *fnx_iterator_end = fnx_tree_end(fnx_map);
	fnx_assert(fnx_iterator == fnx_iterator_end);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_map(fnx_treetype_e tree_type)
{
	Vector keys;
	Map stl_map;
	fnx_tree_t fnx_map;
	fnx_treehooks_t hooks;
	fnx_tlink_t *tlnk;

	hooks.getkey_hook = getkey;
	hooks.keycmp_hook = keycmp;
	fnx_tree_init(&fnx_map, tree_type, &hooks);

	make_random_range(keys, 1, s_max_size);
	for (size_t i = 0; i < keys.size(); ++i) {
		const int key   = keys[i];
		const int value = value_for_key(key);

		stl_map.insert(std::make_pair(key, value));
		fnx_tree_insert_unique(&fnx_map, new_node(key, value));
		check_equal(stl_map, &fnx_map);

		tlnk = fnx_tree_insert_replace(&fnx_map, new_node(key, value));
		delete_node(tlnk, NULL);
		check_equal(stl_map, &fnx_map);
	}

	fnx_tree_clear(&fnx_map, delete_node, NULL);
	fnx_tree_destroy(&fnx_map);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_multimap(fnx_treetype_e tree_type)
{
	typedef MultiMap::iterator   iterator;

	Vector keys;
	MultiMap stl_multimap;
	fnx_tree_t fnx_multimap;
	iterator stl_iterator;
	fnx_tlink_t *fnx_iterator;
	fnx_treehooks_t hooks;

	hooks.getkey_hook = getkey;
	hooks.keycmp_hook = keycmp;
	fnx_tree_init(&fnx_multimap, tree_type, &hooks);

	make_random_range(keys, 1, s_max_size);
	for (size_t j = 0; j < 7; ++j) {
		for (size_t i = 0; i < keys.size(); ++i) {
			const int key   = keys[i];
			const int value = value_for_key(key);

			stl_multimap.insert(std::make_pair(key, value));
			fnx_tree_insert(&fnx_multimap, new_node(key, value));

			stl_iterator = stl_multimap.find(key);
			fnx_iterator = fnx_tree_find(&fnx_multimap, &key);
			check_equal(stl_iterator, fnx_iterator);

			fnx_iterator  =
			    fnx_tree_find_first(&fnx_multimap, &key);
			check_equal(stl_iterator, fnx_iterator);

			if (((j % 3) == 0) || ((j % 5) == 0)) {
				stl_multimap.erase(stl_iterator);
				fnx_tree_remove(&fnx_multimap, fnx_iterator);
				delete_node(fnx_iterator, NULL);
			}
			check_equal(stl_multimap, &fnx_multimap);
		}
	}

	fnx_tree_clear(&fnx_multimap, delete_node, NULL);
	fnx_tree_destroy(&fnx_multimap);
}


static void exectest(void (*test_fn)(fnx_treetype_e))
{
	test_fn(FNX_TREE_AVL);
	test_fn(FNX_TREE_RB);
	test_fn(FNX_TREE_TREAP);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
// Compare Funex associative containers agains C++STL containers.
int main(int argc, char *argv[])
{
	const unsigned int t(static_cast<unsigned int>(time(0)));
	srand(t);

	if (argc > 1) {
		sscanf(argv[1], "%d", &s_max_size);
	}

	exectest(test_map);
	exectest(test_multimap);

	return 0;
}
