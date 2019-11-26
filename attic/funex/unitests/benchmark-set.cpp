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
#include <utility>
#include <algorithm>
#include <vector>
#include <set>
#include <errno.h>
#include <error.h>
#include <limits.h>
#include <signal.h>
#include <getopt.h>

#include <fnxinfra.h>

// Defs:
#define NO_ARGUMENT                 (0)
#define REQUIRED_ARGUMENT           (1)

typedef std::vector<int>            Vector;
typedef std::set<int>               Set;
typedef std::multiset<int>          MultiSet;

struct set_node {
	fnx_tlink_t link;
	int key;
};
typedef struct set_node     set_node_t;


// Local vars:
static const char *s_progname  = 0;
static unsigned int s_count     = 256;
static unsigned int s_erase     = 0;
static volatile bool s_random   = false;
static volatile bool s_stl      = false;
static volatile bool s_fnx      = false;
static volatile bool s_avl      = false;
static volatile bool s_rb       = false;
static volatile bool s_treap    = false;


static void make_keys(Vector &vec)
{
	for (int i = 0; i < int (s_count); ++i) {
		vec.push_back(i);
	}
	if (s_random) {
		std::random_shuffle(vec.begin(), vec.end());
	}
}

static const void *getkey(const fnx_tlink_t *x)
{
	const set_node_t *node = reinterpret_cast<const set_node_t *>(x);
	return &node->key;
}

static int keycmp(const void *x_key, const void *y_key)
{
	const int *x_k = static_cast<const int *>(x_key);
	const int *y_k = static_cast<const int *>(y_key);

	return ((*y_k) - (*x_k));
}

static fnx_tlink_t *new_node(int key)
{
	set_node_t *p_node = new set_node_t;
	p_node->key = key;

	return reinterpret_cast<fnx_tlink_t *>(p_node);
}

static void delete_node(fnx_tlink_t *x, void *p)
{
	delete x;
	fnx_unused(p);
}


static int getkeyi(const Vector &keys, int index)
{
	const size_t i = static_cast<size_t>(index);

	fnx_assert(i < keys.size());
	return keys[i];
}

static void benchmark_fnx_set(fnx_treetype_e tree_type)
{
	int key;
	fnx_tree_t set;
	fnx_tlink_t *itr = 0;
	fnx_treehooks_t uops;

	uops.getkey_hook = getkey;
	uops.keycmp_hook = keycmp;
	fnx_tree_init(&set, tree_type, &uops);

	Vector keys;
	make_keys(keys);

	const int n_insert(static_cast<int>(keys.size()));
	for (int i = 0; i < n_insert; ++i) {
		key = getkeyi(keys, i);
		fnx_tree_insert_unique(&set, new_node(key));
	}
	const int n_erase(std::min(int (keys.size()), int (s_erase)));
	for (int j = 0; j < n_erase; ++j) {
		key = getkeyi(keys, j);

		itr = fnx_tree_find(&set, &key);
		fnx_assert(itr != NULL);

		fnx_tree_remove(&set, itr);
		delete_node(itr, NULL);
	}

	fnx_tree_clear(&set, delete_node, NULL);
	fnx_tree_destroy(&set);
}


static void benchmark_stl_set(void)
{
	int key;
	Set set;
	Set::iterator itr;

	Vector keys;
	make_keys(keys);

	const int n_insert(int (keys.size()));
	for (int i = 0; i < n_insert; ++i) {
		key = getkeyi(keys, i);
		set.insert(key);
	}

	const int n_erase(std::min(int (keys.size()), int (s_erase)));
	for (int j = 0; j < n_erase; ++j) {
		key = getkeyi(keys, j);
		itr = set.find(key);
		set.erase(itr);
	}
}


// Do the actual benchmark, based on input-params settings.
static void benchmark(void)
{
	if (s_fnx) {
		if (s_avl) {
			benchmark_fnx_set(FNX_TREE_AVL);
		} else if (s_rb) {
			benchmark_fnx_set(FNX_TREE_RB);
		} else if (s_treap) {
			benchmark_fnx_set(FNX_TREE_TREAP);
		}

	} else if (s_stl) {
		benchmark_stl_set();
	}
}


// Parses command line arguments, exit if failure.
static void parse_opt(int argc, char *argv[])
{
	int c;
	opterr = 0;

	/* Options. */
	static const struct option s_longopts[] = {
		{ "help",           NO_ARGUMENT,        0,  'h' },
		{ "count",          REQUIRED_ARGUMENT,  0,  'c' },
		{ "random",         NO_ARGUMENT,        0,  'r' },
		{ "erase",          REQUIRED_ARGUMENT,  0,  'e' },
		{ "lib",            REQUIRED_ARGUMENT,  0,  's' },
		{ "tree",           REQUIRED_ARGUMENT,  0,  't' },
		{ 0, 0, 0, 0}
	};

	/* Usage */
	static const char s_usage_fmt[] =
	    "                                                               \n" \
	    "Main Options:                                                  \n" \
	    "   -h, --help                  Display this help message       \n" \
	    "   -c, --count=N               Number of elements              \n" \
	    "   -r, --random                Use random sequence             \n" \
	    "   -e, --erase=N               Number of elements to erase     \n" \
	    "   -s, --lib                   Use Funex|STL                   \n" \
	    "   -t, --tree=AVL,RB,TREAP     Underlying tree type            \n";

	/*  Parse options */
	static const char s_opts[] = "hvc:re:s:t:";
	while ((c = getopt_long(argc, argv, s_opts, s_longopts, 0)) != EOF) {
		switch (c) {
			case 'h':
				fprintf(stdout, "%s\n%s\n", s_progname, s_usage_fmt);
				exit(EXIT_SUCCESS);
				break;

			case 'c':
				sscanf(optarg, "%u", &s_count);
				break;

			case 'r':
				s_random = true;
				break;

			case 'e':
				sscanf(optarg, "%u", &s_erase);
				break;

			case 's':
				if (!strcasecmp(optarg, "funex") ||
				    !strcasecmp(optarg, "fnx")) {
					s_fnx = true;
				} else if (!strcasecmp(optarg, "stl")) {
					s_stl = true;
				} else {
					error(EXIT_FAILURE, 0, "Illegal lib-type %s\n", optarg);
				}
				break;

			case 't':
				if (!strcasecmp(optarg, "AVL")) {
					s_avl   = true;
				} else if (!strcasecmp(optarg, "RB")) {
					s_rb    = true;
				} else if (!strcasecmp(optarg, "TREAP")) {
					s_treap = true;
				} else {
					error(EXIT_FAILURE, 0, "Illegal tree-type %s\n", optarg);
				}
				break;

			case ':':
			case '?':  /* getopt returns '?' if an illegal option is supplied */
				fprintf(stderr, "*** Unknown or missing option character");
				if (optarg) {
					fprintf(stderr, ": %s", optarg);
				}
				printf("\n");

			default:
				error(EXIT_FAILURE, 0, "Try `--help' for more information.\n");
				break;
		}
	}
}


// Compare Funex TREE performance against C++STL SET.
int main(int argc, char *argv[])
{
	// Begin with defaults.
	s_progname  = program_invocation_short_name;

	// Parse args.
	parse_opt(argc, argv);

	// Actual benchmark.
	benchmark();

	return 0;
}

