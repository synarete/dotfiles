/*
 * CaC: Cauta-to-C Compiler
 *
 * Copyright (C) 2016,2017,2018 Shachar Sharon
 *
 * CaC is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CaC is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CaC. If not, see <https://www.gnu.org/licenses/gpl>.
 */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "macros.h"
#include "fatal.h"
#include "memory.h"
#include "hashtbl.h"

/* Local functions */
static size_t select_good_hprime(size_t n);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/


static int
htbl_keycmp(const htbl_t *htbl, const void *key1, const void *key2)
{
	return htbl->hcmp(key1, key2);
}

static bool
htbl_isequal(const htbl_t *htbl, const void *key1, const void *key2)
{
	return (htbl_keycmp(htbl, key1, key2) == 0);
}

static hashkey_t htbl_key2hash(const htbl_t *htbl, const void *key)
{
	return htbl->hkey(key);
}

static size_t htbl_key2pos(const htbl_t *htbl, const void *key)
{
	return htbl_key2hash(htbl, key) % htbl->tsz;
}

static hentry_t **htbl_alloc(size_t nbins)
{
	hentry_t **tbl;

	tbl = (hentry_t **)gc_malloc(nbins * sizeof(tbl[0]));
	return tbl;
}

static void htbl_init(htbl_t *htbl, hashkey_fn hkey, hashcmp_fn hcmp,
                      size_t tsz)
{
	htbl->size = 0;
	htbl->tsz = select_good_hprime(tsz);
	htbl->tbl = htbl_alloc(htbl->tsz);
	htbl->pool = NULL;
	htbl->hcmp = hcmp;
	htbl->hkey = hkey;
}

htbl_t *htbl_new(const htblfns_t *hfns)
{
	return htbl_new2(hfns->hkey, hfns->hcmp);
}

htbl_t *htbl_new2(hashkey_fn hkey, hashcmp_fn hcmp)
{
	htbl_t *htbl;

	htbl = (htbl_t *)gc_malloc(sizeof(*htbl));
	htbl_init(htbl, hkey, hcmp, 32);
	return htbl;
}

size_t htbl_size(const htbl_t *htbl)
{
	return (htbl->size);
}


int htbl_isempty(const htbl_t *htbl)
{
	return (htbl->size == 0);
}

void *htbl_find(const htbl_t *htbl, const void *key)
{
	size_t pos;
	void *obj = NULL;
	const hentry_t *itr;

	pos = htbl_key2pos(htbl, key);
	itr = htbl->tbl[pos];
	while (itr != NULL) {
		if (htbl_isequal(htbl, key, itr->key)) {
			obj = itr->obj;
			break;
		}
		itr = itr->next;
	}
	return obj;
}

static void htbl_inc_pool(htbl_t *htbl)
{
	hentry_t *ent;
	const size_t nn = 64;

	ent = (hentry_t *)gc_malloc(nn * sizeof(*ent));
	for (size_t i = 0; i < nn; ++i) {
		ent->next = htbl->pool;
		htbl->pool = ent;
		ent++;
	}
}

static hentry_t *htbl_pop_free_entry(htbl_t *htbl)
{
	hentry_t *ent;

	if (htbl->pool == NULL) {
		htbl_inc_pool(htbl);
	}
	ent = htbl->pool;
	htbl->pool = htbl->pool->next;
	ent->next = NULL;
	return ent;
}

static void htbl_push_free_entry(htbl_t *htbl, hentry_t *ent)
{
	ent->next = htbl->pool;
	htbl->pool = ent;
}

void *htbl_erase(htbl_t *htbl, const void *key)
{
	size_t pos;
	void *obj = NULL;
	hentry_t *ent;
	hentry_t **itr;

	pos = htbl_key2pos(htbl, key);
	itr = &htbl->tbl[pos];
	while ((ent = *itr) != NULL) {
		if (htbl_isequal(htbl, key, ent->key)) {
			obj = ent->obj;
			*itr = ent->next;
			htbl_push_free_entry(htbl, ent);
			htbl->size--;
			break;
		}
		itr = &ent->next;
	}
	return obj;
}

static hentry_t *
htbl_new_entry(htbl_t *htbl, const void *key, void *obj)
{
	hentry_t *ent;

	ent = htbl_pop_free_entry(htbl);
	ent->key = key;
	ent->obj = obj;
	ent->next = NULL;
	return ent;
}

static void htbl_insert_ent(htbl_t *htbl, hentry_t *ent)
{
	size_t pos;
	hentry_t **itr;

	pos = htbl_key2pos(htbl, ent->key);
	itr = &htbl->tbl[pos];
	ent->next = *itr;
	*itr = ent;
	htbl->size++;
}

static void htbl_rehash(htbl_t *htbl, size_t new_tsz)
{
	hentry_t *itr, *ent;
	hentry_t **tbl = htbl->tbl;
	const size_t tsz = htbl->tsz;

	htbl_init(htbl, htbl->hkey, htbl->hcmp, new_tsz);
	for (size_t i = 0; i < tsz; ++i) {
		itr = tbl[i];
		while ((ent = itr) != NULL) {
			itr = itr->next;
			htbl_insert_ent(htbl, ent);
		}
	}
}

void htbl_insert(htbl_t *htbl, const void *key, void *obj)
{
	hentry_t *ent;
	const size_t lim = (2 * htbl->tsz);

	ent = htbl_new_entry(htbl, key, obj);
	htbl_insert_ent(htbl, ent);
	if (htbl->size > lim) {
		htbl_rehash(htbl, htbl->size + 1);
	}
}

void htbl_clear(htbl_t *htbl)
{
	htbl->size = 0;
	htbl->tsz = select_good_hprime(16);
	htbl->tbl = htbl_alloc(htbl->tsz);
	htbl->pool = NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Chooses a "good" prime-value for hash-table of n-elements.
 *
 * Prime numbers used by SGI STL C++ hashtable. See also:
 * http://planetmath.org/encyclopedia/GoodHashTablePrimes.html
 */
static const size_t s_good_hashtable_primes[] = {
	13UL,
	53UL,
	97UL,
	193UL,
	389UL,
	769UL,
	1543UL,
	3079UL,
	6151UL,
	12289UL,
	24593UL,
	49157UL,
	98317UL,
	196613UL,
	393241UL,
	786433UL,
	1572869UL,
	3145739UL,
	6291469UL,
	12582917UL,
	25165843UL,
	50331653UL,
	100663319UL,
	201326611UL,
	402653189UL,
	805306457UL,
	1610612741UL,
	3221225473UL,
	4294967291UL
};

static size_t select_good_hprime(size_t n)
{
	size_t v;
	const size_t nelems = ARRAYSIZE(s_good_hashtable_primes);

	for (size_t i = 0; i < nelems; ++i) {
		v = s_good_hashtable_primes[i];
		if (v >= n) {
			break;
		}
	}
	return v;
}

