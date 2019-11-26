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
#ifndef FUNEX_HTBL_H_
#define FUNEX_HTBL_H_       (1)

#include <stdlib.h>

struct fnx_hlink;
struct fnx_htbl;

typedef size_t fnx_hkey_t;
typedef struct fnx_hlink fnx_hlink_t;

/* Hash-function hook: converts user's key-object to unsigned hash value */
typedef fnx_hkey_t (*fnx_gethkey_fn)(const void *, size_t);

/* Compare hash-link with hash-obj reference */
typedef int (*fnx_hcompare_fn)(const fnx_hlink_t *, const void *, size_t);

/*
 * User-provided hashing element. It is up to the user to ensure this object
 * remains valid through-out the entire life-cycle of the hash-table.
 */
struct fnx_hlink {
	fnx_hlink_t *h_next;
	fnx_hkey_t   h_key;
};


/*
 * An open addressing hash-table of user-provided linkable elements, where the
 * hash-key is long unsigned integer type.
 */
struct fnx_htbl {
	size_t ht_nelems;                   /* Stored elements count */
	size_t ht_tblsize;                  /* Actual table size     */
	size_t ht_memsize;                  /* Allocated memory size */
	void  *ht_memory;                   /* Allocated memory      */
	fnx_hlink_t   **ht_slots;           /* Buckets' array        */
	fnx_gethkey_fn  ht_gethkey_fn;      /* Calc hash-key hook    */
	fnx_hcompare_fn ht_compare_fn;      /* Compare-to hook       */
	unsigned int    ht_magic;        /* Debug checker         */
	fnx_hlink_t    *ht_slots_arr[1];
};
typedef struct fnx_htbl   fnx_htbl_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Initialize hash-table object, using private allocated memory.
 * Return 0 in case of success, -1 in case of memory allocation failure.
 */
int fnx_htbl_init(fnx_htbl_t *htbl, size_t nelems, fnx_hcompare_fn hcompare);

/*
 * Destroy hash-table object and release all prive allocated memory.
 * NB: It is up to the user to release elements still linked in hash-table.
 */
void fnx_htbl_destroy(fnx_htbl_t *htbl);


/*
 * Allocates and constructs new hash-table.
 */
fnx_htbl_t *fnx_htbl_new(size_t nelems, fnx_hcompare_fn);

/*
 * Destruct and release resources associated with hash-table.
 * NB: It is up to the user to release elements still linked in hash-table.
 */
void fnx_htbl_del(fnx_htbl_t *);


/*
 * Returns the number of elements currently stored in hash-table.
 */
size_t fnx_htbl_size(const fnx_htbl_t *);

/*
 * Returns TRUE if no elemets in hash-table.
 */
int fnx_htbl_isempty(const fnx_htbl_t *);

/*
 * Insert the element x to hash-table. Hash-key for x is calculated by applying
 * hash-function on user-provided data-ref.
 */
void fnx_htbl_insert(fnx_htbl_t *, fnx_hlink_t *, const void *, size_t);

/*
 * Search by refernce. Returns the first element whose hash-key equals given
 * data-ref. If no element found, returns NULL.
 */
fnx_hlink_t *fnx_htbl_lookup(const fnx_htbl_t *ht, const void *, size_t);

/*
 * Removes a prepsiusly looked-up element, which is still a member of the
 * hash-table.
 */
void fnx_htbl_remove(fnx_htbl_t *ht, fnx_hlink_t *x);


#endif /* FUNEX_HTBL_H_ */


