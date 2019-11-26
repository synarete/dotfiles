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

#include "compiler.h"
#include "macros.h"
#include "utility.h"
#include "vector.h"


/* Allocate/Deallocate helpers */
static size_t vec_sizeof_nelems(const fnx_vector_t *vec, size_t nelems)
{
	return (nelems * vec->vec_elemsize);
}

static void *vec_alloc_nelems(const fnx_vector_t *vec, size_t n)
{
	size_t bytes_sz;
	bytes_sz = vec_sizeof_nelems(vec, n);
	return fnx_allocate(&vec->vec_alloc, bytes_sz);
}

static void vec_dealloc_nelems(const fnx_vector_t *vec, void *mem, size_t n)
{
	size_t sz;

	sz = vec_sizeof_nelems(vec, n);
	fnx_deallocate(&vec->vec_alloc, mem, sz);
}


static void vec_release_memory(fnx_vector_t *vec)
{
	if (vec->vec_data != NULL) {
		vec_dealloc_nelems(vec, vec->vec_data, vec->vec_capacity);
		vec->vec_data       = NULL;
		vec->vec_capacity   = 0;
		vec->vec_size       = 0;
	}
}

/* Returns pointer to the i-th element, no checking. */
static void *vec_element_at_mem(const fnx_vector_t *vec,
                                void *mem, size_t pos)
{
	uint8_t *dat = (uint8_t *)mem;
	return (void *)(dat + vec_sizeof_nelems(vec, pos));
}

static void *vec_element_at(const fnx_vector_t *vec, size_t pos)
{
	return vec_element_at_mem(vec, vec->vec_data, pos);
}

void fnx_vector_init(fnx_vector_t *vec, size_t elem_sz,
                     const fnx_alloc_if_t *alloc)
{
	fnx_alloc_if_t *vec_alloc = NULL;

	if (alloc == NULL) {
		alloc = fnx_mallocator;
	}

	vec->vec_elemsize = elem_sz;
	vec->vec_size     = 0;
	vec->vec_capacity = 0;
	vec->vec_data     = NULL;

	vec_alloc = &vec->vec_alloc;
	fnx_bcopy(vec_alloc, alloc, sizeof(*vec_alloc));
}

void fnx_vector_destroy(fnx_vector_t *vec)
{
	vec_release_memory(vec);
	fnx_bzero(vec, sizeof(*vec));
}

size_t fnx_vector_size(const fnx_vector_t *vec)
{
	return vec->vec_size;
}

size_t fnx_vector_capacity(const fnx_vector_t *vec)
{
	return vec->vec_capacity;
}

int fnx_vector_isempty(const fnx_vector_t *vec)
{
	return (vec->vec_size == 0);
}

void *fnx_vector_at(const fnx_vector_t *vec, size_t i)
{
	uint8_t *p = NULL;

	if (i < vec->vec_size) {
		p = vec_element_at(vec, i);
	}

	return (void *) p;
}

void *fnx_vector_data(const fnx_vector_t *vec)
{
	return vec->vec_data;
}

void fnx_vector_clear(fnx_vector_t *vec)
{
	vec->vec_size = 0;
}

/* Copy helpers: */
static size_t vec_copyto(const fnx_vector_t *vec, void *dst, size_t len)
{
	size_t sz;
	const void *dat;

	dat = fnx_vector_data(vec);
	sz  = fnx_min(len, vec_sizeof_nelems(vec, vec->vec_size));
	memcpy(dst, dat, sz);
	return sz;
}

static void vec_move_nelems(const fnx_vector_t *vec,
                            const void *src, void *dst, size_t nelems)
{
	size_t sz;

	sz = vec_sizeof_nelems(vec, nelems);
	memmove(dst, src, sz);
}

static void vec_copy_nelems(const fnx_vector_t *vec,
                            const void *src, void *dst, size_t nelems)
{
	size_t sz;

	sz = vec_sizeof_nelems(vec, nelems);
	memcpy(dst, src, sz);
}


static void vec_copy_elem(const fnx_vector_t *vec,
                          const void *src, void *dst)
{
	vec_copy_nelems(vec, src, dst, 1);
}

/* Forces allocation to hold up to n elements. */
static void *vec_renew_memory(fnx_vector_t *vec, size_t n)
{
	size_t sz, cap;
	void *mem;

	sz  = vec->vec_size;
	cap = vec->vec_capacity;
	mem = vec_alloc_nelems(vec, n);
	if (mem == NULL) {
		return NULL; /* Alloc failure! */
	}

	/* Use new memory region */
	if (cap > 0) {
		vec_copy_nelems(vec, vec->vec_data, mem, n);
	}

	/* Release old data */
	vec_release_memory(vec);

	/* Update members to new settings */
	vec->vec_data       = mem;
	vec->vec_capacity   = n;
	vec->vec_size       = fnx_min(n, sz);

	return mem;
}

size_t fnx_vector_copyto(const fnx_vector_t *vec, void *mem, size_t len)
{
	return vec_copyto(vec, mem, len);
}

void *fnx_vector_reserve(fnx_vector_t *vec, size_t n)
{
	void *p = NULL;

	if (n == 0) {
		vec_release_memory(vec);
	} else {
		p = vec_renew_memory(vec, n);
	}
	return p;
}

void *fnx_vector_push_back(fnx_vector_t *vec, const void *element)
{
	return fnx_vector_insert(vec, vec->vec_size, element);
}

void *fnx_vector_xpush_back(fnx_vector_t *vec, const void *element)
{
	return fnx_vector_xinsert(vec, vec->vec_size, element);
}

void fnx_vector_pop_back(fnx_vector_t *vec)
{
	size_t sz;

	sz = vec->vec_size;
	if (sz > 0) {
		fnx_vector_erase(vec, sz - 1);
	}
}

void *fnx_vector_insert(fnx_vector_t *vec,
                        size_t pos, const void *element)
{
	uint8_t *p = NULL;
	uint8_t *q = NULL;
	size_t sz, cap, n;

	sz  = vec->vec_size;
	cap = vec->vec_capacity;

	if ((sz < cap) && (pos <= sz)) {
		p = vec_element_at(vec, pos);
		q = vec_element_at(vec, pos + 1);
		n = sz - pos;
		vec_move_nelems(vec, p, q, n);
		vec_copy_elem(vec, element, p);

		vec->vec_size += 1;
	}

	return (void *) p;
}

void *fnx_vector_xinsert(fnx_vector_t *vec,
                         size_t pos, const void *element)
{
	void *p    = NULL;
	void *mem  = NULL;
	void *src  = NULL;
	void *dst  = NULL;
	size_t sz, cap, n;

	sz  = vec->vec_size;
	cap = vec->vec_capacity;

	/* Case 1: First time. */

	if ((cap == 0) && (pos == 0)) {
		fnx_vector_reserve(vec, 1);
		p = fnx_vector_insert(vec, pos, element);
	}

	/* Case 2: Not enough room, need alloc (doubling) */
	else if ((sz == cap)  && (pos <= sz)) {
		n   = fnx_max(2 * cap, 1);

		if ((mem = vec_alloc_nelems(vec, n)) == NULL) {
			return NULL; /* Alloc failure! */
		}

		/* Set new data: part1,element,part2 */
		src = vec_element_at(vec, 0);
		dst = mem;
		vec_copy_nelems(vec, src, dst, pos);
		dst = vec_element_at_mem(vec, mem, pos);
		vec_copy_elem(vec, element, dst);
		src = vec_element_at(vec, pos);
		dst = vec_element_at_mem(vec, mem, pos + 1);
		vec_copy_nelems(vec, src, dst, sz - pos);

		/* Release old data */
		vec_release_memory(vec);

		/* Update members to new settings */
		vec->vec_data       = mem;
		vec->vec_capacity   = n;
		vec->vec_size       = fnx_min(n, sz + 1);
		p = vec_element_at(vec, pos);
	}

	/* Case 3: Have enough free space, simple insertion. */
	else {
		p = fnx_vector_insert(vec, pos, element);
	}

	return p;
}

void fnx_vector_erase(fnx_vector_t *vec, size_t pos)
{
	uint8_t *p = NULL;
	uint8_t *q = NULL;
	size_t sz, n;

	sz = vec->vec_size;

	if (pos < sz) {
		p = vec_element_at(vec, pos);
		q = vec_element_at(vec, pos + 1);
		n = (sz - pos) - 1;
		vec_move_nelems(vec, q, p, n);
		vec->vec_size = sz - 1;
	}
}


