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
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "compiler.h"
#include "macros.h"
#include "panic.h"
#include "utility.h"
#include "htbl.h"

#define FNX_HTBL_MAGIC         (0x4854424C)    /* H T B L */

#define htbl_debug_check(ht)                                    \
	do { if (fnx_unlikely(ht->ht_magic != FNX_HTBL_MAGIC))  \
			fnx_panic("ht_magic=%#x", ht->ht_magic);      \
	} while (0)

typedef size_t fnx_hslot_t;

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Common hash functions. Resources:
 *
 * http://www.burtleburtle.net/bob/hash/doobs.html
 * http://www.cse.yorku.ca/~oz/hash.html
 */
static uint32_t jenkins_one_at_a_time_hash(const uint8_t *key, size_t len)
{
	uint32_t hash, i;
	hash = 0;

	for (i = 0; i < len; ++i) {
		hash += key[i];
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return hash;
}

static uint32_t fnx_jenkins32(const void *key, size_t len)
{
	return jenkins_one_at_a_time_hash((const uint8_t *)key, len);
}

static fnx_hkey_t gethkey_jenkins(const void *ref, size_t len)
{
	return fnx_jenkins32(ref, len);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t aligned_size(size_t sz)
{
	const size_t align = sizeof(void *);

	return ((sz + align - 1) / align) * align;
}

/* Allocation sizes for full object + slots in single memory chunk */
static void
htbl_memalloc_size(const fnx_htbl_t *htbl, int with_hdr,
                   size_t nelems, size_t *p_memsz, size_t *p_tblsz)
{
	size_t size, hdr_sz, elem_sz, tbl_sz;

	hdr_sz  = with_hdr ? sizeof(fnx_htbl_t) : 0;
	tbl_sz  = fnx_good_hprime(nelems);
	elem_sz = sizeof(htbl->ht_slots_arr[0]);
	size    = aligned_size(hdr_sz + (elem_sz * tbl_sz));

	*p_memsz = size;
	*p_tblsz = tbl_sz;
}

static void
htbl_init(fnx_htbl_t *htbl, fnx_hlink_t **slots, size_t nslots,
          fnx_gethkey_fn getkey, fnx_hcompare_fn hcompare)
{
	size_t i;

	htbl->ht_nelems     = 0;
	htbl->ht_memsize    = 0;
	htbl->ht_tblsize    = nslots;
	htbl->ht_gethkey_fn = getkey;
	htbl->ht_compare_fn = hcompare;
	htbl->ht_magic   = FNX_HTBL_MAGIC;
	htbl->ht_slots      = slots;

	for (i = 0; i < nslots; ++i) {
		slots[i] = NULL;
	}

	htbl_debug_check(htbl);
}

static void
htbl_setmem(fnx_htbl_t *htbl, void *mem, size_t memsz)
{
	htbl->ht_memsize = memsz;
	htbl->ht_memory  = mem;
}

int fnx_htbl_init(fnx_htbl_t *htbl, size_t nelems, fnx_hcompare_fn hcompare)
{
	size_t memsz, tblsz;
	fnx_hlink_t **slots;
	void *mem;

	htbl_memalloc_size(htbl, 0, nelems, &memsz, &tblsz);
	mem = fnx_malloc(memsz);
	if (mem == NULL) {
		return -1;
	}

	slots = (fnx_hlink_t **)mem;
	htbl_init(htbl, slots, tblsz, gethkey_jenkins, hcompare);
	htbl_setmem(htbl, mem, memsz);
	return 0;
}

fnx_htbl_t *fnx_htbl_new(size_t nelems, fnx_hcompare_fn hcompare)
{
	size_t memsz, tblsz;
	fnx_htbl_t  *htbl;
	fnx_hlink_t **slots;
	void *mem;

	htbl_memalloc_size(NULL, 1, nelems, &memsz, &tblsz);
	mem = fnx_malloc(memsz);
	if (mem == NULL) {
		return NULL;
	}

	htbl = (fnx_htbl_t *)mem;
	slots = htbl->ht_slots_arr;

	htbl_init(htbl, slots, tblsz, gethkey_jenkins, hcompare);
	htbl_setmem(htbl, mem, memsz);
	return htbl;
}


static void htbl_destroy(fnx_htbl_t *htbl)
{
	htbl_debug_check(htbl);

	htbl->ht_nelems      = 0;
	htbl->ht_tblsize     = 0;
	htbl->ht_gethkey_fn  = NULL;
	htbl->ht_compare_fn  = NULL;
	htbl->ht_magic    = 0;
}

void fnx_htbl_destroy(fnx_htbl_t *htbl)
{
	void *mem;
	size_t memsz;

	htbl_debug_check(htbl);
	memsz = htbl->ht_memsize;
	mem   = htbl->ht_memory;

	htbl_destroy(htbl);
	fnx_free(mem, memsz);
}

void fnx_htbl_del(fnx_htbl_t *htbl)
{
	fnx_htbl_destroy(htbl);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Double-hash for cases where user provided poor hash function */
static fnx_hkey_t hashkey_rehash(fnx_hkey_t n)
{
	n = ~n + (n >> 23);
	n ^= (n << 12);
	n ^= (n >> 7);
	n ^= (n << 2);
	n ^= (n >> 20);
	return (n | 1); /* Avoid zero hash-key */
}

static fnx_hkey_t
htbl_gethkey(const fnx_htbl_t *htbl, const void *ref, size_t len)
{
	fnx_hkey_t hkey;

	hkey = htbl->ht_gethkey_fn(ref, len);
	return hashkey_rehash(hkey);
}

static fnx_hslot_t
htbl_gethslot(const fnx_htbl_t *htbl, fnx_hkey_t hkey)
{
	return (hkey % htbl->ht_tblsize);
}

static int
htbl_isequal(const fnx_htbl_t *htbl, fnx_hkey_t hkey,
             const fnx_hlink_t *hlnk, const void *ref, size_t len)
{
	int res, cmp;

	res = 0;
	if (hkey == hlnk->h_key) {
		cmp =  htbl->ht_compare_fn(hlnk, ref, len);
		res = (cmp == 0);
	}
	return res;
}



/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_htbl_size(const fnx_htbl_t *htbl)
{
	htbl_debug_check(htbl);
	return htbl->ht_nelems;
}

int fnx_htbl_isempty(const fnx_htbl_t *htbl)
{
	htbl_debug_check(htbl);
	return (htbl->ht_nelems == 0);
}

void fnx_htbl_insert(fnx_htbl_t *htbl, fnx_hlink_t *x,
                     const void *ref, size_t len)
{
	fnx_hkey_t  hkey;
	fnx_hslot_t slot;
	fnx_hlink_t **pp = NULL;

	htbl_debug_check(htbl);

	hkey = htbl_gethkey(htbl, ref, len);
	slot = htbl_gethslot(htbl, hkey);

	pp = &htbl->ht_slots[slot];
	x->h_next = (*pp);
	x->h_key  = hkey;
	(*pp) = x;

	htbl->ht_nelems += 1;
}

fnx_hlink_t *
fnx_htbl_lookup(const fnx_htbl_t *htbl, const void *ref, size_t len)
{
	fnx_hkey_t  hkey;
	fnx_hslot_t slot;
	const fnx_hlink_t *hlnk = NULL;
	fnx_hlink_t *const *pp  = NULL;

	htbl_debug_check(htbl);

	hkey = htbl_gethkey(htbl, ref, len);
	slot = htbl_gethslot(htbl, hkey);

	pp = &htbl->ht_slots[slot];
	while ((hlnk = *pp) != NULL) {
		if (htbl_isequal(htbl, hkey, hlnk, ref, len)) {
			break;
		}
		pp = &((*pp)->h_next);
	}
	return (fnx_hlink_t *)hlnk;
}

void fnx_htbl_remove(fnx_htbl_t *htbl, fnx_hlink_t *x)
{
	fnx_hslot_t slot;
	fnx_hlink_t **pp = NULL;

	htbl_debug_check(htbl);

	if (htbl->ht_nelems > 0) {
		slot = htbl_gethslot(htbl, x->h_key);
		pp = &htbl->ht_slots[slot];
		while (*pp != NULL) {
			if (*pp == x) {
				*pp = x->h_next;
				htbl->ht_nelems -= 1;
				break;
			}
			pp = &((*pp)->h_next);
		}
	}
}

