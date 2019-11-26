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
#ifndef FNX_INFRA_UTILITY_H_
#define FNX_INFRA_UTILITY_H_

/*
 * Allocations flags:
 */
#define FNX_BZERO   0x01
#define FNX_NOFAIL  0x02



/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Allocation hooks */
typedef void *(*fnx_malloc_fn)(size_t, void *);
typedef void (*fnx_free_fn)(void *, size_t, void *);

/*
 * Memory-allocator interface
 */
struct fnx_alloc_if {
	fnx_malloc_fn malloc;
	fnx_free_fn   free;
	void *userp;
};
typedef struct fnx_alloc_if fnx_alloc_if_t;

/* Default allocator (std) */
extern fnx_alloc_if_t *fnx_mallocator;

/*
 * Memory allocation/deallocation via allocator object.
 */
void *fnx_allocate(const fnx_alloc_if_t *alloc, size_t sz);
void fnx_deallocate(const fnx_alloc_if_t *alloc, void *p, size_t n);

/*
 * Wrappers over malloc/free via Funex global allocator. By default, useS
 * standard malloc/free.
 */
void *fnx_malloc(size_t sz);
void fnx_free(void *p, size_t sz);
void *fnx_xmalloc(size_t sz, int flags);
void fnx_xfree(void *p, size_t sz, int flags);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Wrappers over memory operations
 */
#define fnx_bzero_obj(ptr) fnx_bzero((ptr), sizeof(*(ptr)))

FNX_ATTR_NONNULL
void fnx_bzero(void *p, size_t n);

FNX_ATTR_NONNULL
void fnx_bff(void *p, size_t n);

FNX_ATTR_NONNULL
void fnx_bcopy(void *t, const void *s, size_t n);

FNX_ATTR_NONNULL
int fnx_bcmp(void *t, const void *s, size_t n);


/*
 * Recursive stack coloring.
 */
void fnx_burnstack(int n);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Min/Max as functions for size_t
 */
FNX_ATTR_PURE
size_t fnx_min(size_t x, size_t y);

FNX_ATTR_PURE
size_t fnx_max(size_t x, size_t y);


/*
 * Chooses a "good" prime-value for hash-table of n-elements
 */
size_t fnx_good_hprime(size_t n);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Global pointer to program's execution name or path.
 */
extern const char *fnx_execname;

/*
 * Assign process's path-to-executable, or invocation-name into global pointer
 * fnx_execname (derived from Solaris' getexecname(3C)).
 */
void fnx_setup_execname(void);

/*
 * Converts errno value to human-readable string (strerror return description
 * message).
 */
const char *fnx_errno_to_string(int errnum);


#endif /* FNX_INFRA_UTILITY_H_ */


