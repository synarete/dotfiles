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
#ifndef FUNEX_VECTOR_H_
#define FUNEX_VECTOR_H_

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * A sequence-container that supports random access to elements, constant time
 * insertion and removal of elements at the end, and linear time insertion and
 * removal of elements at the beginning or in the middle. Holds fixed-size
 * elements, which are a copy of user-inserted objects, stored in dynamic
 * allocated array.
 */
struct fnx_vector {
	fnx_alloc_if_t vec_alloc;   /* Memory allocator           */
	size_t vec_elemsize;     /* Size of single user object */
	size_t vec_size;         /* Current number of elements */
	size_t vec_capacity;     /* Max number of elements     */
	void *vec_data;          /* Elements-array             */
};
typedef struct fnx_vector        fnx_vector_t;


/*: : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : :*/
/*
 * Creates empty vector object, associated with user's allocator. If alloc is
 * NULL, using default allocator.
 */
void fnx_vector_init(fnx_vector_t *vec, size_t elem_sz,
                     const fnx_alloc_if_t *alloc);

/*
 * Releases any resources associated with vector object.
 */
void fnx_vector_destroy(fnx_vector_t *vec);


/*
 * Returns the number of elements stored in vec.
 */
size_t fnx_vector_size(const fnx_vector_t *vec);


/*
 * Number of elements for which memory has been allocated (always greater
 * than or equal to size).
 */
size_t fnx_vector_capacity(const fnx_vector_t *vec);


/*
 * Returns TRUE is vec size is zero.
 */
int fnx_vector_isempty(const fnx_vector_t *vec);


/*
 * Returns the i-th element, or NULL if out of range.
 */
void *fnx_vector_at(const fnx_vector_t *vec, size_t i);


/*
 * Returns the underlying memory region associated with vec.
 */
void *fnx_vector_data(const fnx_vector_t *vec);


/*
 * Set size to zero (but without deallocation).
 */
void fnx_vector_clear(fnx_vector_t *vec);


/*
 * Copy the content of vec to mem. Copies no more then len bytes.
 *
 * Returns the number of bytes copied to mem.
 */
size_t fnx_vector_copyto(const fnx_vector_t *vec,
                         void *mem, size_t len);

/**
 * Forces capacity of at-least n elements (manual allocation).
 *
 * Returns pointer to beginning of data, or NULL pointer in case of
 * allocation failure.
 */
void *fnx_vector_reserve(fnx_vector_t *vec, size_t n);


/*
 * Inserts a copy of element at the end.
 *
 * Returns pointer to the newly inserted element, or NULL if full (in which
 * user need to do explicit allocation with reserve).
 */
void *fnx_vector_push_back(fnx_vector_t *vec, const void *element);


/*
 * Inserts a copy of element at the end, do allocation if full.
 *
 * Returns pointer to the newly inserted element, or NULL in case of
 * allocation failure.
 */
void *fnx_vector_xpush_back(fnx_vector_t *vec, const void *element);

/*
 * Removes the last element, if any.
 */
void fnx_vector_pop_back(fnx_vector_t *vec);


/*
 * Inserts element before position pos.
 *
 * Returns pointer to the newly inserted element, or NULL if full.
 */
void *fnx_vector_insert(fnx_vector_t *vec,
                        size_t pos, const void *element);

/*
 * Inserts element before position pos.
 *
 * Returns pointer to the newly inserted element, or NULL in case of
 * allocation failure.
 */
void *fnx_vector_xinsert(fnx_vector_t *vec,
                         size_t pos, const void *element);

/*
 * Erases the element at position pos.
 */
void fnx_vector_erase(fnx_vector_t *vec, size_t pos);


#endif /* FUNEX_VECTOR_H_ */
