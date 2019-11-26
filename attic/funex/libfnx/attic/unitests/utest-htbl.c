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
#include <time.h>

#include <fnx/infra.h>
#include "randutil.h"

struct hash_elem {
	fnx_hlink_t hlink;
	int key, value;
};
typedef struct hash_elem        hash_elem_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int key_to_value(int key)
{
	return (key * key);
}

static hash_elem_t *make_elem(int key)
{
	hash_elem_t *elem = NULL;

	elem = (hash_elem_t *)fnx_malloc(sizeof(*elem));
	fnx_assert(elem != NULL);

	elem->key   = key;
	elem->value = key_to_value(key);
	return elem;
}

static void check_elem(const hash_elem_t *elem)
{
	fnx_assert(elem != NULL);
	fnx_assert(elem->value == key_to_value(elem->key));
}

static void free_elem(hash_elem_t *elem)
{
	check_elem(elem);
	fnx_free(elem, sizeof(*elem));
}

static fnx_hlink_t *elem_to_hlink(const hash_elem_t *elem)
{
	const fnx_hlink_t *lnk = &elem->hlink;
	return (fnx_hlink_t *)lnk;
}

static hash_elem_t *hlink_to_elem(const fnx_hlink_t *lnk)
{
	const hash_elem_t *elem = fnx_container_of(lnk, hash_elem_t, hlink);
	return (hash_elem_t *)elem;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int elem_hcompare(const fnx_hlink_t *hlnk, const void *ref, size_t len)
{
	int key;
	const hash_elem_t *elem;

	elem = hlink_to_elem(hlnk);
	key  = *(const int *)ref;

	return ((elem->key == key) && (len == sizeof(key))) ? 0 : -1;
}

static void
test_hashtable_insert(fnx_htbl_t *htbl, const int *keys, size_t nelems)
{
	size_t i, sz, len;
	int key;
	const void *ref;
	hash_elem_t *elem  = NULL;
	fnx_hlink_t *hlnk  = NULL;
	hash_elem_t *elem2 = NULL;

	for (i = 0; i < nelems; ++i) {
		key = keys[i];
		ref = &key;
		len = sizeof(key);

		elem = make_elem(key);
		check_elem(elem);

		hlnk = elem_to_hlink(elem);
		fnx_htbl_insert(htbl, hlnk, &elem->key, sizeof(elem->key));

		hlnk = fnx_htbl_lookup(htbl, ref, len);
		fnx_assert(hlnk != NULL);
		elem2 = hlink_to_elem(hlnk);
		check_elem(elem2);
		fnx_assert(elem == elem2);

		sz = fnx_htbl_size(htbl);
		fnx_assert(sz == (i + 1));
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
test_hashtable_lookup(const fnx_htbl_t *htbl, const int *keys, size_t nelems)
{
	size_t i;
	int key;
	const hash_elem_t *elem = NULL;
	const fnx_hlink_t *hlnk  = NULL;

	for (i = 0; i < nelems; ++i) {
		key  = keys[i];

		hlnk  = fnx_htbl_lookup(htbl, &key, sizeof(key));
		fnx_assert(hlnk != NULL);
		elem = hlink_to_elem(hlnk);
		check_elem(elem);
		fnx_assert(elem->key == key);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
test_hashtable_remove(fnx_htbl_t *ht, const int *keys, size_t nelems)
{
	size_t i, sz;
	int key;
	hash_elem_t *elem  = NULL;
	fnx_hlink_t *hlnk  = NULL;

	for (i = 0; i < nelems; ++i) {
		key = keys[i];

		hlnk = fnx_htbl_lookup(ht, &key, sizeof(key));
		fnx_assert(hlnk != NULL);

		elem  = hlink_to_elem(hlnk);
		check_elem(elem);
		fnx_assert(elem->key == key);

		fnx_htbl_remove(ht, hlnk);
		free_elem(elem);

		sz = fnx_htbl_size(ht);
		fnx_assert(sz == (nelems - i - 1));
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void
test_hashtable(int *keys, size_t nelems, struct random_data *rnd_dat)
{
	int rc;
	fnx_htbl_t htbl_obj;
	fnx_htbl_t *ht = &htbl_obj;

	rc = fnx_htbl_init(ht, nelems, elem_hcompare);
	fnx_assert(rc == 0);

	test_hashtable_insert(ht, keys, nelems);
	test_hashtable_lookup(ht, keys, nelems);
	random_shuffle(keys, nelems, rnd_dat);
	test_hashtable_lookup(ht, keys, nelems);
	random_shuffle(keys, nelems, rnd_dat);
	test_hashtable_remove(ht, keys, nelems);

	fnx_htbl_destroy(ht);
}

int main(int argc, char *argv[])
{
	int rc;
	size_t size, nkeys, nn;
	int *keys;
	struct random_data *rnd_dat;

	rnd_dat = create_randomizer();

	nkeys = 601;
	if (argc > 1) {
		rc = sscanf(argv[1], "%zu", &nn);
		if ((rc == 1) && (nn > 0)) {
			nkeys = nn;
		}
	}

	size  = nkeys * sizeof(keys[0]);
	keys  = (int *)malloc(size);
	fnx_assert(keys != NULL);

	generate_keys(keys, nkeys, 1); /* Unique keys */
	random_shuffle(keys, nkeys, rnd_dat);
	test_hashtable(keys, nkeys, rnd_dat);

	generate_keys(keys, nkeys, 7); /* Non-unique */
	random_shuffle(keys, nkeys, rnd_dat);
	test_hashtable(keys, nkeys, rnd_dat);

	free(keys);

	return EXIT_SUCCESS;
}


