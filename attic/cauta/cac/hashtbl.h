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
#ifndef CAC_HASHTBL_H_
#define CAC_HASHTBL_H_

typedef unsigned long hashkey_t;
typedef hashkey_t (*hashkey_fn)(const void *);
typedef int (*hashcmp_fn)(const void *, const void *);


/* Hash-table hooks functions */
struct htblfns {
	hashkey_fn hkey;   /* Key-to-hash hook */
	hashcmp_fn hcmp;   /* 3-way key-compare hook */
};
typedef struct htblfns htblfns_t;


/* Single entry within hash-table */
typedef struct hentry {
	const void *key;
	void       *obj;
	struct hentry *next;
} hentry_t;


/*
 * Hash table with separate chaining.
 */
typedef struct htbl {
	size_t     tsz;    /* Table's capacity */
	size_t     size;   /* Number of stored elements */
	hentry_t **tbl;    /* Hash-table entries */
	hentry_t  *pool;   /* Pool of free entries */
	hashkey_fn hkey;   /* Key-to-hash hook */
	hashcmp_fn hcmp;   /* 3-way key-compare hook */

} htbl_t;

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Allocates and construct new empty hash-table.
 */
htbl_t *htbl_new(const htblfns_t *hfns);
htbl_t *htbl_new2(hashkey_fn hkey, hashcmp_fn hcmp);


/*
 * Returns the current number of elements stored in container.
 */
size_t htbl_size(const htbl_t *htbl);


/*
 * Returns true is the container's size is zero.
 */
int htbl_isempty(const htbl_t *htbl);


/*
 * Inserts a new elements. May trigger implicit resize and rehash in case of
 * high collision ratio.
 */
void htbl_insert(htbl_t *htbl, const void *key, void *obj);


/*
 * Searches for hashed element by key.
 */
void *htbl_find(const htbl_t *htbl, const void *key);


/*
 * Remove an element by key. Returns the removed object.
 */
void *htbl_erase(htbl_t *htbl, const void *key);


/*
 * Clear all data and reduce size to zero.
 */
void htbl_clear(htbl_t *htbl);


#endif /* CAC_HASHTBL_H_ */
