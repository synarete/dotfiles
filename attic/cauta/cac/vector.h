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
#ifndef CAC_VECTOR_H_
#define CAC_VECTOR_H_

/*
 * Single pointer-entry within a vector.
 */
typedef struct vector_slot {
	void *obj;
} vector_slot_t;

/*
 * A sequence-container that supports random access to elements, constant time
 * insertion and removal of elements at the beginning and end, and linear time
 * insertion and removal of elements at the middle. Holds objects via opaque
 * pointer.
 */
typedef struct vector {
	vector_slot_t *mem;     /* Memory-slots buffer */
	size_t  cap;            /* Total memory capacity */
	size_t  beg;            /* Begin offset */
	size_t  len;            /* Data length */
} vector_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/


/*
 * Initialize vector object with start-capacity.
 */
void vector_init(vector_t *, size_t cap);


/*
 * Allocates and construct new empty container with initial capacity.
 */
vector_t *vector_new(void);
vector_t *vector_nnew(size_t cap);


/*
 * Returns a duplicated vector, which reference the same objects.
 */
vector_t *vector_dup(const vector_t *);


/*
 * Returns a duplicated vector, which reference the trailing objects beginning
 * from position pos.
 */
vector_t *vector_tail(const vector_t *, size_t);


/*
 * Returns a duplicated vector, which reference to cnt objects beginning at
 * position pos.
 */
vector_t *vector_sub(const vector_t *, size_t, size_t);


/*
 * Returns the number of elements stored in the container.
 */
size_t vector_size(const vector_t *vector);


/*
 * Returns true is the container's size is zero.
 */
int vector_isempty(const vector_t *vector);


/*
 * Returns the element at position pos, or NULL if out-of-range.
 */
void *vector_at(const vector_t *vector, size_t pos);


/*
 * Returns the position of specific object-reference within the vector.
 */
size_t vector_find(const vector_t *vector, const void *obj);


/*
 * Returns the first element, or NULL if empty
 */
void *vector_front(const vector_t *vector);


/*
 * Returns the last element, or NULL if empty
 */
void *vector_back(const vector_t *vector);


/*
 * Inserts an element at the end-of-sequence, with possible resize-and-copy.
 */
void vector_push_back(vector_t *vector, void *obj);


/*
 * Inserts an element at the start-of-sequence, with possible resize-and-copy.
 */
void vector_push_front(vector_t *vector, void *obj);


/*
 * Removes the last element, if any.
 */
void *vector_pop_back(vector_t *vector);


/*
 * Removes the first element, if any.
 */
void *vector_pop_front(vector_t *vector);


/*
 * Synonym to push_back
 */
void vector_append(vector_t *vector, void *obj);


/*
 * Inserts element before position pos.
 */
void vector_insert(vector_t *vector, size_t pos, void *obj);


/*
 * Replaces the element at position pos. Returns the previous one.
 */
void *vector_replace(vector_t *vector, size_t pos, void *obj);


/*
 * Erases the element at position pos.
 */
void vector_erase(vector_t *vector, size_t pos);


/*
 * Erases n-elements, beginning at position pos.
 */
void vector_erasen(vector_t *vector, size_t pos, size_t cnt);


/*
 * Clear all data and reduce size to zero.
 */
void vector_clear(vector_t *vector);


/*
 * Returns a new vector-object with joined data of [vector1,vector2].
 */
vector_t *vector_join(const vector_t *vector1, const vector_t *vector2);



#endif /* CAC_VECTOR_H_ */

