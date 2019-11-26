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
#ifndef FNX_CORE_CACHE_H_
#define FNX_CORE_CACHE_H_


/* I/V-nodes hash-table */
struct fnx_vhtbl {
	fnx_size_t      size;       /* Number of stored elements */
	fnx_size_t      tcap;       /* Table's capacity */
	fnx_vnode_t   **htbl;       /* Element's lookup table */
};
typedef struct fnx_vhtbl fnx_vhtbl_t;


/* Blocks hash-table */
struct fnx_bkhtbl {
	fnx_size_t      size;       /* Number of stored elements */
	fnx_size_t      tcap;       /* Table's capacity */
	fnx_bkref_t   **htbl;
};
typedef struct fnx_bkhtbl fnx_bkhtbl_t;


/* Dir-entries hash */
struct fnx_dentry {
	fnx_ino_t       dino;
	fnx_hash_t      hash;
	fnx_size_t      nlen;
	fnx_ino_t       ino;
};
typedef struct fnx_dentry fnx_dentry_t;

struct fnx_dehtbl {
	fnx_size_t      tcap;       /* Table's capacity */
	fnx_dentry_t   *htbl;
};
typedef struct fnx_dehtbl fnx_dehtbl_t;


/* Space-maps and raw-blocks caching */
struct fnx_ucache {
	fnx_vhtbl_t     smhtbl;     /* Space-maps hash-table */
	fnx_list_t      smlru;      /* Space-maps LRU queue */
	fnx_bkhtbl_t    bkhtbl;     /* Blocks hash-table */
	fnx_list_t      bklru;      /* Blocks LRU */
};
typedef struct fnx_ucache fnx_ucache_t;


/* V-nodes and dir-entries cache */
struct fnx_vcache {
	fnx_dehtbl_t    dehtbl;     /* Dir-entries hash-table */
	fnx_vhtbl_t     vhtbl;      /* V-nodes' hash-table */
	fnx_list_t      vlru;       /* V/I-nodes LRU queue */
};
typedef struct fnx_vcache fnx_vcache_t;


/* Unified cache */
struct fnx_cache {
	fnx_ucache_t    uc;         /* Space & blocks cache */
	fnx_vcache_t    vc;         /* V-nodes cache */
};
typedef struct fnx_cache fnx_cache_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_ucache_init(fnx_ucache_t *);

void fnx_ucache_destroy(fnx_ucache_t *);

int fnx_ucache_setup(fnx_ucache_t *);


void fnx_ucache_store_bk(fnx_ucache_t *, fnx_bkref_t *);

void fnx_ucache_evict_bk(fnx_ucache_t *, fnx_bkref_t *);

fnx_bkref_t *fnx_ucache_search_bk(fnx_ucache_t *, const fnx_baddr_t *);

fnx_bkref_t *fnx_ucache_poplru_bk(fnx_ucache_t *);


void fnx_ucache_store_spmap(fnx_ucache_t *, fnx_spmap_t *);

fnx_spmap_t *fnx_ucache_search_spmap(fnx_ucache_t *, const fnx_vaddr_t *);

void fnx_ucache_evict_spmap(fnx_ucache_t *, fnx_spmap_t *);

fnx_spmap_t *fnx_ucache_poplru_spmap(fnx_ucache_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_vcache_init(fnx_vcache_t *);

void fnx_vcache_destroy(fnx_vcache_t *);

int fnx_vcache_setup(fnx_vcache_t *);


void fnx_vcache_store_vnode(fnx_vcache_t *, fnx_vnode_t *);

fnx_vnode_t *fnx_vcache_search_vnode(fnx_vcache_t *, const fnx_vaddr_t *);

void fnx_vcache_evict_vnode(fnx_vcache_t *, fnx_vnode_t *);

fnx_vnode_t *fnx_vcache_poplru_vnode(fnx_vcache_t *);


void fnx_vcache_clear_des(fnx_vcache_t *);

void fnx_vcache_remap_de(fnx_vcache_t *,
                         fnx_ino_t, fnx_hash_t, fnx_size_t, fnx_ino_t);

fnx_ino_t fnx_vcache_lookup_de(const fnx_vcache_t *,
                               fnx_ino_t, fnx_hash_t, fnx_size_t);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_cache_init(fnx_cache_t *);

void fnx_cache_destroy(fnx_cache_t *);

int fnx_cache_setup(fnx_cache_t *);


#endif /* FNX_CORE_CACHE_H_ */
